#pragma once

#include "service_discovery/service_discovery_core.h"
#include "service_discovery/zookeeper/zk_client.h"
#include "service_discovery/zookeeper/zk_paths.h"
#include "module/module_interface.h"

#include <memory>
#include <string>
#include <unordered_map>

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
                      ZkPaths      paths,
                      std::string  service_hosts)
        : zk_client_(std::move(zk_client))
        , paths_(std::move(paths))
        , service_hosts_(std::move(service_hosts))
    {
    }

    /**
     * @brief 初始化注册器，创建进程层级节点
     */
    bool Init()
    {
        if (!zk_client_)
        {
            BaseNodeLogError("Invalid zk_client_");
            return false;
        }
        BaseNodeLogInfo("Ready to EnsurePath in zookeeper. root:%s, ProcessesRoot:%s, ServicesRoot:%s."
                        , paths_.root.c_str()
                        , paths_.ProcessesRoot().c_str()
                        , paths_.ServicesRoot().c_str()
                    );
        // // 确保基础路径存在（这些路径应该是持久节点，因为需要在它们下面创建子节点）
        // zk_client_->EnsurePath(paths_.root);
        // zk_client_->EnsurePath(paths_.ProcessesRoot());
        // zk_client_->EnsurePath(paths_.ServicesRoot());

        // // 注意：持久节点不会自动清理，需要在进程退出时手动清理（在 Uninit 中处理）
        // const auto process_path = paths_.ProcessPath(service_hosts_);
        // BaseNodeLogInfo("Ready to create process path in zookeeper. process_path:%s (persistent, not ephemeral).", process_path.c_str());
        
        // // 创建持久进程节点（如果已存在则忽略）
        // if (!zk_client_->EnsurePath(process_path))
        // {
        //     BaseNodeLogError("Failed to create process path: %s", process_path.c_str());
        //     return false;
        // }
        
        // // 创建 /modules 持久节点路径
        // const std::string modules_path = paths_.ProcessPath(service_hosts_) + "/modules";
        // if (!zk_client_->EnsurePath(modules_path))
        // {
        //     BaseNodeLogError("Failed to create modules path: %s", modules_path.c_str());
        //     return false;
        // }
        
        return true;
    }

    // ------------ IServiceRegistry 接口：服务实例注册 ------------ //

    bool RegistService(const BaseNode::ServiceDiscovery::ServiceInstance &instance) override;

    bool DeRegisterService(const BaseNode::ServiceDiscovery::ServiceInstance &instance) override;

    bool RenewService(const BaseNode::ServiceDiscovery::ServiceInstance &instance) override;

private:
    IZkClientPtr zk_client_;
    ZkPaths      paths_;
    std::string  service_hosts_;
};

using ZkServiceRegistryPtr = std::shared_ptr<ZkServiceRegistry>;

} // namespace BaseNode::ServiceDiscovery::Zookeeper


