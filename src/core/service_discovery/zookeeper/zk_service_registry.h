#pragma once

#include "service_discovery/service_discovery_core.h"
#include "service_discovery/zookeeper/zk_client.h"
#include "service_discovery/zookeeper/zk_paths.h"
#include "module/module_interface.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace BaseNode::ServiceDiscovery::Zookeeper
{

/**
 * @brief 基于 Zookeeper 的服务注册实现
 *
 * 同时负责三层信息的注册：
 *  - 进程（process）
 *  - 模块（IModule）
 *  - 模块提供的 RPC 函数（通过 IModule::GetAllServiceHandlerKeys）
 *  - 服务实例（继承自 IServiceRegistry 接口）
 */
class ZkServiceRegistry final : public BaseNode::ServiceDiscovery::IServiceRegistry
{
public:
    ZkServiceRegistry(IZkClientPtr zk_client,
                      ZkPaths      paths)
        : zk_client_(std::move(zk_client))
        , paths_(std::move(paths))
    {
    }

    /**
     * @brief 初始化注册器，创建进程层级节点
     */
    bool Init();

    // ------------ IServiceRegistry 接口：服务实例注册 ------------ //

    bool RegistService(const BaseNode::ServiceDiscovery::ServiceInstance &instance) override;

    bool DeRegisterService(const BaseNode::ServiceDiscovery::ServiceInstance &instance) override;

    bool RenewService(const BaseNode::ServiceDiscovery::ServiceInstance &instance) override;

    /**
     * @brief 清理孤儿节点（没有子临时节点的IP/模块节点）
     * @param base_path 要清理的根路径，默认为 BaseNodeRoot
     */
    void CleanupOrphanNodes(const std::string &base_path = "");

    /**
     * @brief 清理当前会话创建的所有节点
     */
    void CleanupSessionNodes();

private:
    /**
     * @brief 递归清理空节点（没有子节点的节点）
     * @param path 要清理的路径
     * @return 如果节点被删除返回 true
     */
    bool RecursiveCleanupEmptyNode(const std::string &path);

private:
    IZkClientPtr zk_client_;
    ZkPaths      paths_;
    
    // 跟踪当前会话创建的节点路径
    std::mutex tracked_nodes_mutex_;
    std::unordered_set<std::string> tracked_host_port_nodes_;  // 跟踪的IP:Port节点
    std::unordered_set<std::string> tracked_module_nodes_;     // 跟踪的模块节点
};

using ZkServiceRegistryPtr = std::shared_ptr<ZkServiceRegistry>;

} // namespace BaseNode::ServiceDiscovery::Zookeeper


