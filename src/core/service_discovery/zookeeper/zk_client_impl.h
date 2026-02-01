#pragma once

#include "service_discovery/zookeeper/zk_client.h"
#include <zookeeper.h>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>

namespace BaseNode::ServiceDiscovery::Zookeeper
{

/**
 * @brief 基于 zookeeper-c 的 IZkClient 实现
 *
 * 使用 zookeeper_mt（多线程版本）库实现 Zookeeper 客户端功能。
 * 支持异步操作和 watch 回调。
 */
class ZkClientImpl : public IZkClient
{
public:
    ZkClientImpl();
    ~ZkClientImpl() override;

    // 禁止拷贝和赋值
    ZkClientImpl(const ZkClientImpl &) = delete;
    ZkClientImpl &operator=(const ZkClientImpl &) = delete;

    /// 建立连接（host:port 列表）
    bool Connect(const std::string &hosts, int timeout_ms) override;

    /// 创建持久节点（如果已存在则忽略错误）
    bool EnsurePath(const std::string &path) override;

    /// 创建临时节点（ephemeral），data 可选
    bool CreateEphemeral(const std::string &path, const std::string &data = "") override;

    /// 删除节点（如果不存在则忽略错误）
    bool Delete(const std::string &path) override;

    /// 设置节点数据（如果节点不存在则返回 false）
    bool SetData(const std::string &path, const std::string &data) override;

    /// 读取节点数据（如果不存在返回 false）
    bool GetData(const std::string &path, std::string &out_data) override;

    /// 获取子节点列表（不含路径前缀，仅子名），不存在则返回空列表
    std::vector<std::string> GetChildren(const std::string &path) override;

    /// 监听子节点变化
    bool WatchChildren(const std::string &path, ChildrenChangedCallback cb) override;

    /// 断开连接
    void Disconnect();

    /// 检查是否已连接
    bool IsConnected() const { return connected_ && zh_ != nullptr; }

    /// 添加认证信息（digest 方式）
    /// @param username 用户名
    /// @param password 密码
    /// @return 成功返回 true，失败返回 false
    bool AddAuth(const std::string &username, const std::string &password);

private:
    /// 等待连接建立
    bool WaitForConnected(int timeout_ms);

    /// 检查 Zookeeper 错误码并记录日志
    bool CheckZkError(int rc, const std::string &operation, const std::string &path = "");

    /// Watch 回调函数（用于子节点变化）
    static void GlobalWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);
    static void ChildrenWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);

    /// 处理子节点变化的 watch 回调
    void HandleChildrenChanged(const std::string &path);

    /// 监听会话状态变化
    bool WatchSessionState(SessionStateCallback cb) override;

private:
    zhandle_t *zh_;                                    // Zookeeper 句柄
    std::atomic<bool> connected_;                      // 连接状态
    std::mutex mutex_;                                 // 保护共享数据
    std::unordered_map<std::string, ChildrenChangedCallback> watch_callbacks_; // path -> callback 映射
    SessionStateCallback session_state_callback_;      // 会话状态回调
};

} // namespace BaseNode::ServiceDiscovery::Zookeeper

