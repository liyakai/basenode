#include "service_discovery/zookeeper/zk_client_impl.h"
#include "utils/basenode_def_internal.h"
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>

namespace BaseNode::ServiceDiscovery::Zookeeper
{

ZkClientImpl::ZkClientImpl()
    : zh_(nullptr)
    , connected_(false)
{
}

ZkClientImpl::~ZkClientImpl()
{
    Disconnect();
}

bool ZkClientImpl::Connect(const std::string &hosts, int timeout_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (zh_ != nullptr)
    {
        BaseNodeLogWarn("[ZkClientImpl] Already connected, disconnecting first");
        zookeeper_close(zh_);
        zh_ = nullptr;
        connected_ = false;
    }

    // 设置日志级别（可选，根据需要调整）
    zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);

    // 创建 Zookeeper 句柄（使用多线程版本）
    zh_ = zookeeper_init(hosts.c_str(), GlobalWatcher, timeout_ms, nullptr, this, 0);
    if (zh_ == nullptr)
    {
        BaseNodeLogError("[ZkClientImpl] Failed to create zookeeper handle for %s", hosts.c_str());
        return false;
    }

    // 等待连接建立
    if (!WaitForConnected(timeout_ms))
    {
        BaseNodeLogError("[ZkClientImpl] Failed to connect to %s within %d ms", hosts.c_str(), timeout_ms);
        zookeeper_close(zh_);
        zh_ = nullptr;
        return false;
    }

    BaseNodeLogInfo("[ZkClientImpl] Connected to Zookeeper: %s", hosts.c_str());
    return true;
}

bool ZkClientImpl::AddAuth(const std::string &username, const std::string &password)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (zh_ == nullptr || !connected_)
    {
        BaseNodeLogError("[ZkClientImpl] Cannot add auth: not connected");
        return false;
    }

    // 构造 digest 认证字符串：username:password
    std::string auth_string = username + ":" + password;

    // 调用 zoo_add_auth 添加 digest 认证
    // scheme: "digest"
    // cert: "username:password" (明文，Zookeeper 会自动计算 digest)
    int rc = zoo_add_auth(zh_, "digest", auth_string.c_str(), 
                          static_cast<int>(auth_string.length()), nullptr, nullptr);
    
    if (rc != ZOK)
    {
        CheckZkError(rc, "AddAuth", "");
        return false;
    }

    BaseNodeLogInfo("[ZkClientImpl] Added digest auth for user: %s", username.c_str());
    return true;
}

bool ZkClientImpl::WaitForConnected(int timeout_ms)
{
    int elapsed = 0;
    const int check_interval = 100; // 每 100ms 检查一次

    while (elapsed < timeout_ms)
    {
        if (connected_)
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval));
        elapsed += check_interval;
    }

    return connected_;
}

void ZkClientImpl::GlobalWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{
    ZkClientImpl *client = static_cast<ZkClientImpl *>(watcherCtx);
    if (client == nullptr)
    {
        return;
    }

    if (type == ZOO_SESSION_EVENT)
    {
        bool is_connected = false;
        if (state == ZOO_CONNECTED_STATE)
        {
            client->connected_ = true;
            is_connected = true;
            BaseNodeLogInfo("[ZkClientImpl] Zookeeper session connected");
        }
        else if (state == ZOO_EXPIRED_SESSION_STATE)
        {
            client->connected_ = false;
            is_connected = false;
            BaseNodeLogError("[ZkClientImpl] Zookeeper session expired");
        }
        else if (state == ZOO_AUTH_FAILED_STATE)
        {
            client->connected_ = false;
            is_connected = false;
            BaseNodeLogError("[ZkClientImpl] Zookeeper authentication failed");
        }
        else if (state == ZOO_CONNECTING_STATE)
        {
            client->connected_ = false;
            is_connected = false;
            BaseNodeLogInfo("[ZkClientImpl] Zookeeper connecting...");
        }
        else if (state == ZOO_ASSOCIATING_STATE)
        {
            client->connected_ = false;
            is_connected = false;
            BaseNodeLogInfo("[ZkClientImpl] Zookeeper associating...");
        }

        // 调用会话状态回调
        std::lock_guard<std::mutex> lock(client->mutex_);
        if (client->session_state_callback_)
        {
            // 在单独线程中执行回调，避免阻塞 Zookeeper 事件处理
            std::thread([callback = client->session_state_callback_, is_connected]() {
                callback(is_connected);
            }).detach();
        }
    }
}

void ZkClientImpl::ChildrenWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{
    ZkClientImpl *client = static_cast<ZkClientImpl *>(watcherCtx);
    if (client == nullptr || path == nullptr)
    {
        return;
    }

    if (type == ZOO_CHILD_EVENT && state == ZOO_CONNECTED_STATE)
    {
        client->HandleChildrenChanged(path);
        
        // 重新设置 watch，以便继续监听变化
        std::lock_guard<std::mutex> lock(client->mutex_);
        auto it = client->watch_callbacks_.find(path);
        if (it != client->watch_callbacks_.end())
        {
            // 重新获取子节点以设置新的 watch
            struct String_vector strings;
            int rc = zoo_get_children(zh, path, 1, &strings);
            if (rc == ZOK)
            {
                deallocate_String_vector(&strings);
            }
        }
    }
}

void ZkClientImpl::HandleChildrenChanged(const std::string &path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = watch_callbacks_.find(path);
    if (it != watch_callbacks_.end() && it->second)
    {
        // 在单独的线程中执行回调，避免阻塞 Zookeeper 事件处理
        std::thread([callback = it->second, path]() {
            callback(path);
        }).detach();
    }
}

bool ZkClientImpl::CheckZkError(int rc, const std::string &operation, const std::string &path)
{
    if (rc == ZOK)
    {
        return true;
    }

    const char *error_msg = zerror(rc);
    if (path.empty())
    {
        BaseNodeLogError("[ZkClientImpl] %s failed: %s (code: %d)", operation.c_str(), error_msg, rc);
    }
    else
    {
        BaseNodeLogError("[ZkClientImpl] %s failed for path %s: %s (code: %d)", 
                        operation.c_str(), path.c_str(), error_msg, rc);
    }

    // 对于某些错误，不记录为错误（如节点已存在、节点不存在等）
    // 这些错误在某些场景下是正常的，返回 false 但不记录错误日志
    if (rc == ZNODEEXISTS || rc == ZNONODE)
    {
        return false;
    }

    return false;
}

bool ZkClientImpl::EnsurePath(const std::string &path)
{
    if (!IsConnected())
    {
        BaseNodeLogError("[ZkClientImpl] Not connected");
        return false;
    }

    if (path.empty() || path == "/")
    {
        return true;
    }

    // 递归创建路径
    std::string current_path;
    std::vector<std::string> parts;
    
    // 分割路径
    size_t start = 0;
    if (path[0] == '/')
    {
        start = 1;
    }

    size_t pos = path.find('/', start);
    while (pos != std::string::npos)
    {
        if (pos > start)
        {
            parts.push_back(path.substr(start, pos - start));
        }
        start = pos + 1;
        pos = path.find('/', start);
    }
    if (start < path.length())
    {
        parts.push_back(path.substr(start));
    }

    // 逐级创建路径
    current_path = "/";
    for (const auto &part : parts)
    {
        if (part.empty())
        {
            continue;
        }
        if (current_path != "/")
        {
            current_path += "/";
        }
        current_path += part;

        int rc = zoo_create(zh_, current_path.c_str(), nullptr, 0, &ZOO_OPEN_ACL_UNSAFE, 0, nullptr, 0);
        if (rc != ZOK && rc != ZNODEEXISTS)
        {
            CheckZkError(rc, "EnsurePath", current_path);
            return false;
        }
        BaseNodeLogInfo("[zookeeper] EnsurePath, path:%s, current_path:%s.", path.c_str(), current_path.c_str());
    }

    return true;
}

bool ZkClientImpl::CreateEphemeral(const std::string &path, const std::string &data)
{
    if (!IsConnected())
    {
        BaseNodeLogError("[ZkClientImpl] Not connected");
        return false;
    }

    // 确保父路径存在
    size_t last_slash = path.find_last_of('/');
    if (last_slash != 0 && last_slash != std::string::npos)
    {
        std::string parent_path = path.substr(0, last_slash);
        if (!EnsurePath(parent_path))
        {
            return false;
        }
    }

    // 创建临时节点
    int rc = zoo_create(zh_, path.c_str(), data.c_str(), static_cast<int>(data.length()),
                       &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, nullptr, 0);
    
    if (rc == ZNODEEXISTS)
    {
        // 节点已存在，尝试删除后重新创建
        zoo_delete(zh_, path.c_str(), -1);
        rc = zoo_create(zh_, path.c_str(), data.c_str(), static_cast<int>(data.length()),
                       &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, nullptr, 0);
    }
    BaseNodeLogInfo("[zookeeper] CreateEphemeral, path:%s, data:%s, rc:%d", path.c_str(), data.c_str(), rc);
    return CheckZkError(rc, "CreateEphemeral", path);
}

bool ZkClientImpl::Delete(const std::string &path)
{
    if (!IsConnected())
    {
        BaseNodeLogError("[ZkClientImpl] Not connected");
        return false;
    }

    int rc = zoo_delete(zh_, path.c_str(), -1);
    if (rc == ZNONODE)
    {
        // 节点不存在，忽略错误
        return true;
    }
    BaseNodeLogInfo("[zookeeper] Delete, path:%s", path.c_str());
    return CheckZkError(rc, "Delete", path);
}

bool ZkClientImpl::SetData(const std::string &path, const std::string &data)
{
    if (!IsConnected())
    {
        BaseNodeLogError("[ZkClientImpl] Not connected");
        return false;
    }

    int rc = zoo_set(zh_, path.c_str(), data.c_str(), static_cast<int>(data.length()), -1);
    return CheckZkError(rc, "SetData", path);
}

bool ZkClientImpl::GetData(const std::string &path, std::string &out_data)
{
    if (!IsConnected())
    {
        BaseNodeLogError("[ZkClientImpl] Not connected");
        return false;
    }

    char buffer[1024 * 64]; // 64KB 缓冲区
    int buffer_len = sizeof(buffer);
    struct Stat stat;

    int rc = zoo_get(zh_, path.c_str(), 0, buffer, &buffer_len, &stat);
    if (rc != ZOK)
    {
        CheckZkError(rc, "GetData", path);
        return false;
    }

    out_data.assign(buffer, buffer_len);
    return true;
}

std::vector<std::string> ZkClientImpl::GetChildren(const std::string &path)
{
    std::vector<std::string> children;

    if (!IsConnected())
    {
        BaseNodeLogError("[ZkClientImpl] Not connected");
        return children;
    }

    struct String_vector strings;
    int rc = zoo_get_children(zh_, path.c_str(), 0, &strings);
    if (rc != ZOK)
    {
        CheckZkError(rc, "GetChildren", path);
        return children;
    }

    for (int i = 0; i < strings.count; ++i)
    {
        if (strings.data[i] != nullptr)
        {
            children.push_back(strings.data[i]);
        }
    }

    deallocate_String_vector(&strings);
    return children;
}

bool ZkClientImpl::WatchChildren(const std::string &path, ChildrenChangedCallback cb)
{
    if (!IsConnected())
    {
        BaseNodeLogError("[ZkClientImpl] Not connected");
        return false;
    }

    if (!cb)
    {
        BaseNodeLogError("[ZkClientImpl] Invalid callback for WatchChildren");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    watch_callbacks_[path] = cb;

    // 设置 watch，通过获取子节点来触发
    struct String_vector strings;
    int rc = zoo_get_children(zh_, path.c_str(), 1, &strings); // 最后一个参数 1 表示设置 watch
    if (rc != ZOK)
    {
        CheckZkError(rc, "WatchChildren", path);
        watch_callbacks_.erase(path);
        return false;
    }

    // 释放字符串向量
    deallocate_String_vector(&strings);
    return true;
}

bool ZkClientImpl::WatchSessionState(SessionStateCallback cb)
{
    if (!cb)
    {
        BaseNodeLogError("[ZkClientImpl] Invalid callback for WatchSessionState");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    session_state_callback_ = cb;
    
    // 如果当前已连接，立即通知一次
    if (connected_)
    {
        std::thread([callback = cb]() {
            callback(true);
        }).detach();
    }
    
    return true;
}

void ZkClientImpl::Disconnect()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (zh_ != nullptr)
    {
        zookeeper_close(zh_);
        zh_ = nullptr;
    }

    connected_ = false;
    watch_callbacks_.clear();
    session_state_callback_ = nullptr;

    BaseNodeLogInfo("[ZkClientImpl] Disconnected from Zookeeper");
}

} // namespace BaseNode::ServiceDiscovery::Zookeeper

