#pragma once

#include "module/module_interface.h"
#include "service_discovery/service_discovery_core.h"
#include "service_discovery/invoker.h"
#include "service_discovery/zookeeper/zk_client.h"
#include "service_discovery/zookeeper/zk_paths.h"
#include "service_discovery/zookeeper/zk_service_registry.h"
#include "service_discovery/zookeeper/zk_service_discovery.h"
#include "tools/singleton.h"

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
                   const ZkPaths &paths,
                   const std::string &process_id);

    /// 注册当前进程内的服务实例（通常在对应业务模块 Init 时调用）
    bool RegisterInstance(const ServiceInstance &instance);
    bool DeregisterInstance(const ServiceInstance &instance);

    /// 注册 / 注销模块及其 RPC 函数信息
    bool RegisterModuleInServiceDiscovery(BaseNode::IModule *module);
    bool DeregisterModuleInServiceDiscovery(BaseNode::IModule *module);

    /// 基于 RequestContext 进行服务发现
    std::optional<ServiceInstance>
    ChooseInstance(const std::string &service_name,
                   const RequestContext &ctx);

    /// 构建带重试 + 熔断的 Invoker
    IInvokerPtr
    CreateInvoker(DoCallFunc              do_call,
                  int                     max_retries,
                  int                     failure_threshold,
                  std::chrono::milliseconds open_interval);

protected:
    ErrorCode DoInit() override;
    ErrorCode DoUpdate() override;
    ErrorCode DoUninit() override;
    ErrorCode DoAfterAllModulesInit() override;

private:
    IZkClientPtr         zk_client_;
    ZkPaths              paths_{"/basenode"};
    std::string          process_id_;

    ZkServiceRegistryPtr   registry_;
    ZkServiceDiscoveryPtr  discovery_;
    ILoadBalancerPtr       load_balancer_;
    IDiscoveryClientPtr    discovery_client_;
};

/// 全局单例访问宏（与 NetworkMgr 风格保持一致）
#define ZkServiceDiscoveryMgr ::ToolBox::Singleton<BaseNode::ServiceDiscovery::Zookeeper::ZkServiceDiscoveryModule>::Instance()

} // namespace BaseNode::ServiceDiscovery::Zookeeper


