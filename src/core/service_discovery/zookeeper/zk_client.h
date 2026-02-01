#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace BaseNode::ServiceDiscovery::Zookeeper
{

/**
 * @brief 轻量级 Zookeeper 客户端接口抽象
 *
 * 实际可以用 zookeeper-c / zookeeper-mt / Curator 风格 C++ 封装来实现该接口，
 * 这里不依赖任何具体第三方库，只约定最小操作集合。
 */
class IZkClient
{
public:
    virtual ~IZkClient() = default;

    /// 建立连接（host:port 列表）
    virtual bool Connect(const std::string &hosts, int timeout_ms) = 0;

    /// 创建持久节点（如果已存在则忽略错误）
    virtual bool EnsurePath(const std::string &path) = 0;

    /// 创建临时节点（ephemeral），data 可选
    virtual bool CreateEphemeral(const std::string &path, const std::string &data = "") = 0;

    /// 删除节点（如果不存在则忽略错误）
    virtual bool Delete(const std::string &path) = 0;

    /// 设置节点数据（如果节点不存在则返回 false）
    virtual bool SetData(const std::string &path, const std::string &data) = 0;

    /// 读取节点数据（如果不存在返回 false）
    virtual bool GetData(const std::string &path, std::string &out_data) = 0;

    /// 获取子节点列表（不含路径前缀，仅子名），不存在则返回空列表
    virtual std::vector<std::string> GetChildren(const std::string &path) = 0;

    /// 监听子节点变化，具体底层可通过 watch/回调线程等实现
    using ChildrenChangedCallback = std::function<void(const std::string &path)>;
    virtual bool WatchChildren(const std::string &path, ChildrenChangedCallback cb) = 0;

    /// 监听会话状态变化（连接、断开、过期等）
    /// state: true 表示连接，false 表示断开/过期
    using SessionStateCallback = std::function<void(bool connected)>;
    virtual bool WatchSessionState(SessionStateCallback cb) = 0;
};

using IZkClientPtr = std::shared_ptr<IZkClient>;

} // namespace BaseNode::ServiceDiscovery::Zookeeper




