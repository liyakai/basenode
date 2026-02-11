#pragma once

#include "module/module_interface.h"
#include "module/module_zk.h"
#include "service_discovery/service_discovery_core.h"
#include "service_discovery/zookeeper/zk_client.h"
#include "service_discovery/zookeeper/zk_paths.h"
#include "service_discovery/zookeeper/zk_service_registry.h"
#include "service_discovery/zookeeper/zk_service_discovery.h"
#include "tools/singleton.h"
#include "tools/string_util.h"
namespace BaseNode::ServiceDiscovery::Zookeeper
{

/**
 * @brief 基于 Zookeeper 的服务发现模块（IModule 实现）
 *
 * 负责：
 *  - 建立到 Zookeeper 的连接（通过外部注入的 IZkClient）
 *  - 在 ZK 上注册进程 / 模块 / RPC 函数 / 服务实例
 *  - 提供基于 ZK 的服务发现与多机房感知负载均衡
 *  - 为上层封装带重试 / 熔断策略的 Invoker
 *
 * 顶层业务不直接依赖 Zookeeper，只依赖本模块暴露的抽象能力。
 */
class ZkServiceDiscoveryModule final : public BaseNode::IModule
{
public:
    ZkServiceDiscoveryModule() = default;
    ~ZkServiceDiscoveryModule() override = default;

    /// 在 Init() 前注入 ZK 客户端与路径配置
    void Configure(IZkClientPtr zk_client,
                   const ZkPaths &paths);

    /// 注册当前进程内的服务实例（通常在对应业务模块 Init 时调用）
    bool RegisterInstance(const ServiceInstance &instance);
    bool DeregisterInstance(const ServiceInstance &instance);

    /// 注册 / 注销模块及其 RPC 函数信息
    bool RegisterModuleInServiceDiscovery(BaseNode::IModule *module);
    bool DeregisterModuleInServiceDiscovery(BaseNode::IModule *module);

    /// 监听服务实例变化
    void WatchServiceInstances(const std::string &service_name,
                const ServiceDiscovery::InstanceList &instance_list,
                ServiceDiscovery::InstanceChangeCallback cb);

protected:
    ErrorCode DoInit() override;
    ErrorCode DoUpdate() override;
    ErrorCode DoUninit() override;
    ErrorCode DoAfterAllModulesInit() override;

private:
    IZkClientPtr         zk_client_;
    ZkPaths              paths_{"/basenode"};

    ZkServiceRegistryPtr   registry_;
    ZkServiceDiscoveryPtr  discovery_;

    // 允许 ModuleZkDiscoveryImpl 访问私有成员
    friend class ModuleZkDiscoveryImpl;
};

/// 全局单例访问宏（与 NetworkMgr 风格保持一致）
#define ZkServiceDiscoveryMgr ::ToolBox::Singleton<BaseNode::ServiceDiscovery::Zookeeper::ZkServiceDiscoveryModule>::Instance()

/**
 * @brief IModuleZkRegistry 接口的实现类
 * 将模块注册请求转发给 ZkServiceDiscoveryModule
 */
class ModuleZkRegistryImpl final : public BaseNode::IModuleZkRegistry
{
public:
    explicit ModuleZkRegistryImpl(ZkServiceDiscoveryModule* zk_module)
        : zk_module_(zk_module)
    {
    }

    bool RegisterModule(BaseNode::IModule* module) override
    {
        if (!zk_module_ || !module)
        {
            return false;
        }
        return zk_module_->RegisterModuleInServiceDiscovery(module);
    }

    bool DeregisterModule(BaseNode::IModule* module) override
    {
        if (!zk_module_ || !module)
        {
            return false;
        }
        return zk_module_->DeregisterModuleInServiceDiscovery(module);
    }

private:
    ZkServiceDiscoveryModule* zk_module_;
};


/**
 * @brief IModuleZkRegistry 接口的实现类
 * 将模块注册请求转发给 ZkServiceDiscoveryModule
 */
 class ModuleZkDiscoveryImpl final : public BaseNode::IModuleZkDiscovery
 {
 public:
     explicit ModuleZkDiscoveryImpl(ZkServiceDiscoveryModule* zk_module)
         : zk_module_(zk_module)
     {
     }
 
     BaseNode::ServiceDiscovery::InstanceList GetServiceInstances(const std::string &service_name) override
     {
         if (!zk_module_ || !zk_module_->discovery_)
         {
             return BaseNode::ServiceDiscovery::InstanceList();
         }
         return zk_module_->discovery_->GetServiceInstances(service_name);
     }
 
     void WatchServiceInstances(const std::string &service_name,
                const ServiceDiscovery::InstanceList &instance_list,
                ServiceDiscovery::InstanceChangeCallback cb) override
     {
         if (!zk_module_)
         {
             return;
         }
         return zk_module_->WatchServiceInstances(service_name, instance_list, cb);
     }

     std::vector<std::string> GetAllServiceNames() override
     {
         if (!zk_module_ || !zk_module_->zk_client_)
         {
             return std::vector<std::string>();
         }
         return zk_module_->zk_client_->GetChildren(zk_module_->paths_.BaseNodeRoot());
     }

     void WatchServicesDirectory(ServiceDiscovery::InstanceChangeCallback cb) override
     {
         if (!zk_module_ || !zk_module_->zk_client_)
         {
             return;
         }
         // 确保服务目录路径存在
         const std::string services_root = zk_module_->paths_.ServicesRoot();
         if (!zk_module_->zk_client_->EnsurePath(services_root))
         {
             BaseNodeLogError("[ModuleZkDiscoveryImpl] WatchServicesDirectory: Failed to ensure path %s", services_root.c_str());
             return;
         }
         // 监听服务目录变化
         zk_module_->zk_client_->WatchChildren(services_root,
             [this, cb](const std::string& path) {
                 // 获取所有服务名
                 auto service_names = GetAllServiceNames();
                 BaseNodeLogInfo("[ModuleZkDiscoveryImpl] WatchServicesDirectory: found %zu services, path:%s, service_names:%s", service_names.size(), path.c_str(), ToolBox::VectorToStr(service_names).c_str());
                 // 对每个服务获取实例并回调
                 for (const auto& service_name : service_names) {
                     auto instances = GetServiceInstances(service_name);
                     if (cb) {
                         cb(service_name, instances);
                     }
                 }
             });
     }
 
 private:
     ZkServiceDiscoveryModule* zk_module_;
 };

} // namespace BaseNode::ServiceDiscovery::Zookeeper


