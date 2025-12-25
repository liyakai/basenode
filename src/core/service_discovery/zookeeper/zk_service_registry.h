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
                      std::string  process_id)
        : zk_client_(std::move(zk_client))
        , paths_(std::move(paths))
        , process_id_(std::move(process_id))
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
        // 确保基础路径存在（这些路径应该是持久节点，因为需要在它们下面创建子节点）
        zk_client_->EnsurePath(paths_.root);
        zk_client_->EnsurePath(paths_.ProcessesRoot());
        zk_client_->EnsurePath(paths_.ServicesRoot());

        // 注册进程节点（ephemeral）
        const auto process_path = paths_.ProcessPath(process_id_);
        BaseNodeLogInfo("Ready to EnsurCreateEphemeralePath in zookeeper. process_path:%s.", process_path.c_str());
        return zk_client_->CreateEphemeral(process_path, /*data=*/"");
    }

    /**
     * @brief 注册模块及其 RPC 函数
     */
    bool RegisterModuleInServiceDiscovery(BaseNode::IModule *module)
    {
        if (!module || !zk_client_)
        {
            return false;
        }
        const auto module_id_str = std::to_string(module->GetModuleId());
        const auto module_path   = paths_.ModulePath(process_id_, module_id_str);

        zk_client_->EnsurePath(paths_.ProcessPath(process_id_) + "/modules");
        if (!zk_client_->CreateEphemeral(module_path, module->GetModuleClassName()))
        {
            // 已存在也可以视为成功
        }

        // 注册该模块下所有 RPC 函数 HandlerKey
        auto handler_keys = module->GetAllServiceHandlerKeys();
        if (!handler_keys.empty())
        {
            zk_client_->EnsurePath(module_path + "/rpcs");
        }
        for (auto key : handler_keys)
        {
            const auto rpc_key  = std::to_string(key);
            const auto rpc_path = paths_.RpcFuncPath(process_id_, module_id_str, rpc_key);
            zk_client_->CreateEphemeral(rpc_path, /*data=*/"");
        }
        return true;
    }

    bool DeregisterModuleInServiceDiscovery(BaseNode::IModule *module)
    {
        if (!module || !zk_client_)
        {
            return false;
        }
        const auto module_id_str = std::to_string(module->GetModuleId());
        const auto module_path   = paths_.ModulePath(process_id_, module_id_str);
        // 简单删除模块路径整棵子树（具体实现时可递归删除）
        zk_client_->Delete(module_path);
        return true;
    }

    // ------------ IServiceRegistry 接口：服务实例注册 ------------ //

    bool Register(const BaseNode::ServiceDiscovery::ServiceInstance &instance) override;

    bool Deregister(const BaseNode::ServiceDiscovery::ServiceInstance &instance) override;

    bool Renew(const BaseNode::ServiceDiscovery::ServiceInstance &instance) override;

private:
    IZkClientPtr zk_client_;
    ZkPaths      paths_;
    std::string  process_id_;
};

using ZkServiceRegistryPtr = std::shared_ptr<ZkServiceRegistry>;

} // namespace BaseNode::ServiceDiscovery::Zookeeper


