#include "service_discovery/zookeeper/zk_service_discovery_module.h"
#include "service_discovery/zookeeper/zk_client_impl.h"

#include "utils/basenode_def_internal.h"
#include "protobuf/pb_out/errcode.pb.h"
#include <unistd.h>  // for getpid()

namespace BaseNode::ServiceDiscovery::Zookeeper
{

void ZkServiceDiscoveryModule::Configure(IZkClientPtr zk_client,
                                         const ZkPaths &paths,
                                         const std::string &service_hosts)
{
    zk_client_  = std::move(zk_client);
    paths_      = paths;
    service_hosts_ = service_hosts;
    BaseNodeLogInfo("[ZkServiceDiscovery] Configure success. paths:%s, service_hosts:%s", paths.root.c_str(), service_hosts.c_str());
}

ErrorCode ZkServiceDiscoveryModule::DoInit()
{
    BaseNodeLogInfo("[ZkServiceDiscovery] Init");

    if (!zk_client_)
    {
        BaseNodeLogError("[ZkServiceDiscovery] zk_client is null, Configure() must be called before Init()");
        return ErrorCode::BN_INVALID_ARGUMENTS;
    }

    registry_ = std::make_shared<ZkServiceRegistry>(zk_client_, paths_, service_hosts_);
    if (!registry_->Init())
    {
        BaseNodeLogError("[ZkServiceDiscovery] ZkServiceRegistry Init failed");
        return ErrorCode::BN_INVALID_ARGUMENTS; // TODO: 应该添加一个通用的 BN_FAILED 错误码
    }

    discovery_ = std::make_shared<ZkServiceDiscovery>(zk_client_, paths_);
    load_balancer_ = std::make_shared<ZoneAwareLoadBalancer>();
    discovery_client_ = std::make_shared<DefaultDiscoveryClient>(
        discovery_, load_balancer_, std::chrono::seconds(5));

    if (!registry_)
    {
        BaseNodeLogError("[ZkServiceDiscovery] DoAfterAllModulesInit: registry_ is null");
        return ErrorCode::BN_INVALID_ARGUMENTS;
    }

    // 构造进程级的 ServiceInstance
    // 注意：以下字段应该从配置中心/启动参数/环境变量中读取，这里仅作示例
    ServiceInstance process_instance;

    // service_name: 进程的逻辑服务名（例如 "basenode-game", "basenode-gate" 等）
    // TODO: 从配置读取，例如 process_instance.service_name = ConfigMgr->GetProcessServiceName();
    process_instance.service_name = "basenode-process";

    // instance_id: 进程的唯一标识（使用 Configure 时传入的 process_id_）
    process_instance.instance_id = service_hosts_;

    // host/port: 进程对外提供 RPC 服务的监听地址
    // TODO: 从 Network 模块或配置读取实际监听地址
    // 例如：process_instance.host = NetworkMgr->GetListenIP();
    //       process_instance.port = NetworkMgr->GetListenPort();
    process_instance.host = "127.0.0.1"; // 示例：实际应从配置/Network 模块获取
    process_instance.port = 9000;         // 示例：实际应从配置/Network 模块获取

    // metadata: 进程的元数据（zone/idc/version 等）
    // TODO: 从配置中心读取
    process_instance.metadata["zone"]    = "sh";   // 示例
    process_instance.metadata["idc"]     = "sh01"; // 示例
    process_instance.metadata["version"] = "v1";   // 示例
    process_instance.metadata["weight"]  = "100";   // 示例

    process_instance.healthy = true;

    // 注册进程级 ServiceInstance 到 Zookeeper
    if (!RegisterInstance(process_instance))
    {
        BaseNodeLogError("[ZkServiceDiscovery] DoAfterAllModulesInit: failed to register process-level ServiceInstance");
        return ErrorCode::BN_INVALID_ARGUMENTS; // TODO: 应该添加一个通用的 BN_FAILED 错误码
    }

    BaseNodeLogInfo("[ZkServiceDiscovery] DoAfterAllModulesInit: registered process instance, service_name=%s, instance_id=%s, host=%s, port=%u",
                    process_instance.service_name.c_str(),
                    process_instance.instance_id.c_str(),
                    process_instance.host.c_str(),
                    process_instance.port);

    return ErrorCode::BN_SUCCESS;
}

ErrorCode ZkServiceDiscoveryModule::DoAfterAllModulesInit()
{
    BaseNodeLogInfo("[ZkServiceDiscovery] DoAfterAllModulesInit: registering process-level ServiceInstance");

    

    return ErrorCode::BN_SUCCESS;
}

ErrorCode ZkServiceDiscoveryModule::DoUpdate()
{
    // 预留：可在此进行心跳上报、统计、缓存刷新等
    return ErrorCode::BN_SUCCESS;
}

ErrorCode ZkServiceDiscoveryModule::DoUninit()
{
    BaseNodeLogInfo("[ZkServiceDiscovery] UnInit");
    discovery_client_.reset();
    load_balancer_.reset();
    discovery_.reset();
    registry_.reset();
    zk_client_.reset();
    return ErrorCode::BN_SUCCESS;
}

bool ZkServiceDiscoveryModule::RegisterInstance(const ServiceInstance &instance)
{
    if (!registry_)
    {
        return false;
    }
    return registry_->Register(instance);
}

bool ZkServiceDiscoveryModule::DeregisterInstance(const ServiceInstance &instance)
{
    if (!registry_)
    {
        return false;
    }
    return registry_->Deregister(instance);
}

bool ZkServiceDiscoveryModule::RegisterModuleInServiceDiscovery(BaseNode::IModule *module)
{
    if (!registry_ || !module)
    {
        return false;
    }
    return registry_->RegisterModuleInServiceDiscovery(module);
}

bool ZkServiceDiscoveryModule::DeregisterModuleInServiceDiscovery(BaseNode::IModule *module)
{
    if (!registry_ || !module)
    {
        return false;
    }
    return registry_->DeregisterModuleInServiceDiscovery(module);
}

std::optional<ServiceInstance>
ZkServiceDiscoveryModule::ChooseInstance(const std::string &service_name,
                                         const RequestContext &ctx)
{
    if (!discovery_client_)
    {
        return std::nullopt;
    }
    return discovery_client_->ChooseInstance(service_name, ctx);
}

IInvokerPtr ZkServiceDiscoveryModule::CreateInvoker(DoCallFunc              do_call,
                                                    int                     max_retries,
                                                    int                     failure_threshold,
                                                    std::chrono::milliseconds open_interval)
{
    if (!discovery_client_ || !do_call)
    {
        return nullptr;
    }

    auto simple   = std::make_shared<SimpleInvoker>(discovery_client_, std::move(do_call));
    auto retry    = std::make_shared<RetryInvoker>(simple, max_retries);
    auto circuit  = std::make_shared<CircuitBreakerInvoker>(retry, failure_threshold, open_interval);
    return circuit;
}

// 插件导出符号，方便通过 PluginLoadManager 装载
extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_INIT()
{
    // 在 Init() 之前先调用 Configure()
    // TODO: 这些配置应该从配置文件或环境变量中读取
    std::string zk_hosts = "127.0.0.1:2181";  // 示例：实际应从配置读取
    std::string process_id = "basenode-process-" + std::to_string(getpid());  // 示例：实际应从配置读取
    std::string service_hosts = "127.0.0.1:9527";
    ZkPaths paths{"/basenode"};  // 示例：实际应从配置读取
    
    // 创建真实的 Zookeeper 客户端并连接
    auto zk_client = std::make_shared<ZkClientImpl>();
    if (!zk_client->Connect(zk_hosts, /*timeout_ms=*/3000))
    {
        BaseNodeLogError("[ZkServiceDiscovery] initSo: ZkClientImpl Connect failed to %s", zk_hosts.c_str());
        return;
    }
    
    // 如果 Zookeeper 启用了 digest 认证，需要添加认证信息
    // TODO: 从配置读取用户名和密码
    std::string zk_username = "admin";  // 示例：实际应从配置读取
    std::string zk_password = "password";  // 示例：实际应从配置读取
    if (!zk_client->AddAuth(zk_username, zk_password))
    {
        BaseNodeLogError("[ZkServiceDiscovery] initSo: AddAuth failed");
        return;
    }
    
    ZkServiceDiscoveryMgr->Configure(zk_client, paths, service_hosts);
    ZkServiceDiscoveryMgr->Init();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UPDATE()
{
    ZkServiceDiscoveryMgr->Update();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UNINIT()
{
    ZkServiceDiscoveryMgr->UnInit();
}

// 全局单例实例，用于实现 GetModuleZkRegistryInstance()
static BaseNode::IModuleZkRegistry* g_module_zk_registry_instance = nullptr;

// 实现 IModuleZkRegistry 接口的全局函数
// 使用 SO_EXPORT_SYMBOL 确保符号在所有模块间共享
extern "C" SO_EXPORT_SYMBOL BaseNode::IModuleZkRegistry* GetModuleZkRegistryInstance()
{
    if (!g_module_zk_registry_instance)
    {
        // 在 ZkServiceDiscoveryModule 初始化后创建
        auto* zk_module = ZkServiceDiscoveryMgr;
        if (zk_module)
        {
            // 使用静态变量确保生命周期
            static ModuleZkRegistryImpl impl(zk_module);
            g_module_zk_registry_instance = &impl;
            BaseNodeLogInfo("[ZkServiceDiscovery] GetModuleZkRegistryInstance: created ModuleZkRegistryImpl instance");
        }
        else
        {
            BaseNodeLogWarn("[ZkServiceDiscovery] GetModuleZkRegistryInstance: ZkServiceDiscoveryMgr is null");
        }
    }
    return g_module_zk_registry_instance;
}

} // namespace BaseNode::ServiceDiscovery::Zookeeper



