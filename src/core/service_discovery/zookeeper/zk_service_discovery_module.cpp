#include "service_discovery/zookeeper/zk_service_discovery_module.h"
#include "service_discovery/zookeeper/zk_client_impl.h"

#include "tools/md5.h"
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

    // // 构造进程级的 ServiceInstance
    // // 注意：以下字段应该从配置中心/启动参数/环境变量中读取，这里仅作示例
    // ServiceInstance process_instance;

    // // service_name: 进程的逻辑服务名（例如 "basenode-game", "basenode-gate" 等）
    // // TODO: 从配置读取，例如 process_instance.service_name = ConfigMgr->GetProcessServiceName();
    // process_instance.service_name = "basenode-process";

    // // instance_id: 进程的唯一标识（使用 Configure 时传入的 process_id_）
    // process_instance.instance_id = service_hosts_;

    // // host/port: 进程对外提供 RPC 服务的监听地址
    // // TODO: 从 Network 模块或配置读取实际监听地址
    // // 例如：process_instance.host = NetworkMgr->GetListenIP();
    // //       process_instance.port = NetworkMgr->GetListenPort();
    // process_instance.host = "127.0.0.1"; // 示例：实际应从配置/Network 模块获取
    // process_instance.port = 9000;         // 示例：实际应从配置/Network 模块获取

    // // metadata: 进程的元数据（zone/idc/version 等）
    // // TODO: 从配置中心读取
    // process_instance.metadata["zone"]    = "sh";   // 示例
    // process_instance.metadata["idc"]     = "sh01"; // 示例
    // process_instance.metadata["version"] = "v1";   // 示例
    // process_instance.metadata["weight"]  = "100";   // 示例

    // process_instance.healthy = true;

    // // 注册进程级 ServiceInstance 到 Zookeeper
    // if (!registry_->RegistService(process_instance))
    // {
    //     BaseNodeLogError("[ZkServiceDiscovery] DoInit: failed to register process-level ServiceInstance");
    //     return ErrorCode::BN_INVALID_ARGUMENTS; // TODO: 应该添加一个通用的 BN_FAILED 错误码
    // }

    // BaseNodeLogInfo("[ZkServiceDiscovery] DoAfterAllModulesInit: registered process instance, service_name=%s, instance_id=%s, host=%s, port=%u",
    //                 process_instance.service_name.c_str(),
    //                 process_instance.instance_id.c_str(),
    //                 process_instance.host.c_str(),
    //                 process_instance.port);

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
    
    // 在重置资源之前，先完成自己的 ZK 注销
    // 因为 IModule::UnInit() 会在 DoUninit() 之后才调用 ZK 注销，
    // 但此时 zk_client_ 和 registry_ 已经被重置了
    if (zk_client_ && registry_)
    {
        DeregisterModuleInServiceDiscovery(this);
    }
    
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
    return registry_->RegistService(instance);
}

bool ZkServiceDiscoveryModule::DeregisterInstance(const ServiceInstance &instance)
{
    if (!registry_)
    {
        return false;
    }
    return registry_->DeRegisterService(instance);
}

bool ZkServiceDiscoveryModule::RegisterModuleInServiceDiscovery(BaseNode::IModule *module)
{
    if (!registry_ || !module)
    {
        return false;
    }
    if (!module || !zk_client_)
    {
        return false;
    }
    const auto module_path   = paths_.ModulePath(module->GetModuleClassName());

    // // 确保 /modules 路径存在（应该在 Init() 中已创建，这里再次确保）
    // const std::string modules_path = paths_.ProcessPath(service_hosts_) + "/modules";
    // if (!zk_client_->EnsurePath(modules_path))
    // {
    //     BaseNodeLogError("Failed to ensure modules path: %s", modules_path.c_str());
    //     return false;
    // }
    
    // if (!zk_client_->CreateEphemeral(module_path, module->GetModuleClassName()))
    // {
    //     // 已存在也可以视为成功
    // }

    // 注册该模块下所有 RPC 函数 HandlerKey
    auto handler_keys = module->GetAllServiceHandlerKeys();
    // if (!handler_keys.empty())
    // {
    //     zk_client_->EnsurePath(module_path + "/rpcs");
    // }
    for (auto key : handler_keys)
    {
        BaseNode::ServiceDiscovery::ServiceInstance service_instance;
        service_instance.service_name = std::to_string(key);
        service_instance.instance_id = std::to_string(MD5Hash32Constexpr(std::to_string(key)));
        service_instance.module_name = module->GetModuleClassName();
        service_instance.host = "127.0.0.1";  // TODO: 从配置读取/Network 模块获取
        service_instance.port = 9000;         // TODO: 从配置读取/Network 模块获取
        service_instance.healthy = true;    
        if (!registry_->RegistService(service_instance))
        {
            BaseNodeLogError("[ZkServiceDiscoveryModule] RegisterModuleInServiceDiscovery: failed to register service instance. service_instance:%s."
                            , service_instance.SerializeInstance().c_str());
            return false;
        }
    }
    BaseNodeLogInfo("RegisterModuleInServiceDiscovery success. module_class_name:%s, module_path:%s, handler_keys size:%d."
                    , module->GetModuleClassName().c_str(), module_path.c_str(), handler_keys.size());
    return true;
}

// 递归删除 ZK 节点的辅助函数
static bool RecursiveDelete(IZkClient* zk_client, const std::string& path)
{
    if (!zk_client || path.empty())
    {
        BaseNodeLogWarn("[ZkServiceDiscoveryModule] RecursiveDelete: invalid parameters, path: %s", path.c_str());
        return false;
    }

    // 获取子节点列表
    std::vector<std::string> children = zk_client->GetChildren(path);
    
    // 如果节点不存在（GetChildren 返回空且节点不存在），直接返回成功
    // 注意：GetChildren 在节点不存在时可能返回空列表，但 Delete 会正确处理 ZNONODE 错误
    // 这里我们记录信息，但继续执行删除操作，让 Delete 方法处理节点不存在的情况
    if (children.empty())
    {
        BaseNodeLogInfo("[ZkServiceDiscoveryModule] RecursiveDelete: path %s has no children (may not exist)", path.c_str());
    }
    else
    {
        BaseNodeLogInfo("[ZkServiceDiscoveryModule] RecursiveDelete: path %s has %zu children", path.c_str(), children.size());
    }
    
    // 先递归删除所有子节点
    bool all_children_deleted = true;
    for (const auto& child : children)
    {
        std::string child_path = path;
        if (path.back() != '/')
        {
            child_path += "/";
        }
        child_path += child;
        
        if (!RecursiveDelete(zk_client, child_path))
        {
            BaseNodeLogWarn("[ZkServiceDiscoveryModule] RecursiveDelete: failed to delete child node: %s", child_path.c_str());
            all_children_deleted = false;
            // 继续删除其他子节点，不立即返回
        }
    }
    
    // 再次检查子节点，确保所有子节点都已删除
    // 注意：如果节点不存在，GetChildren 可能返回空列表或失败，这是正常的
    children = zk_client->GetChildren(path);
    if (!children.empty())
    {
        BaseNodeLogWarn("[ZkServiceDiscoveryModule] RecursiveDelete: path %s still has %zu children after deletion attempt, cannot delete parent", 
                       path.c_str(), children.size());
        return false;
    }
    
    // 删除当前节点
    bool result = zk_client->Delete(path);
    if (result)
    {
        BaseNodeLogInfo("[ZkServiceDiscoveryModule] RecursiveDelete: successfully deleted node: %s", path.c_str());
    }
    else
    {
        BaseNodeLogWarn("[ZkServiceDiscoveryModule] RecursiveDelete: failed to delete node: %s", path.c_str());
    }
    return result;
}

bool ZkServiceDiscoveryModule::DeregisterModuleInServiceDiscovery(BaseNode::IModule *module)
{
    if (!module)
    {
        BaseNodeLogError("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: module is null");
        return false;
    }
    
    // 防御性检查：如果 zk_client_ 或 registry_ 已经被重置，说明资源已经被清理
    // 如果这是 ZkServiceDiscoveryModule 自己，说明已经在 DoUninit() 中注销过了，返回 true
    // 如果是其他模块，说明 ZkServiceDiscoveryModule 已经注销，无法为其他模块注销，返回 false
    if (!zk_client_ || !registry_)
    {
        // 检查是否是 ZkServiceDiscoveryModule 自己
        if (module == this)
        {
            BaseNodeLogInfo("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: ZkServiceDiscoveryModule itself "
                           "has already been deregistered in DoUninit(), skip duplicate deregistration");
            return true;
        }
        else
        {
            BaseNodeLogWarn("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: zk_client_ or registry_ is null, "
                           "module (id: %u, class: %s) cannot be deregistered, ZkServiceDiscoveryModule resources already cleaned up",
                           module->GetModuleId(), module->GetModuleClassName().c_str());
            return false;
        }
    }
    
    const auto module_id_str = module->GetModuleClassName();
    
    // 获取模块的所有 RPC handler keys
    auto handler_keys = module->GetAllServiceHandlerKeys();
    
    // 如果模块没有服务，说明注册时没有创建任何节点，直接返回成功
    if (handler_keys.empty())
    {
        BaseNodeLogInfo("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: module (id: %u, class: %s) has no services, "
                       "no ZK nodes were created during registration, skip deregistration",
                       module->GetModuleId(), module_id_str.c_str());
        return true;
    }
    
    // 使用与注册时相同的路径结构：/basenode/{host}:{port}/{module_name}
    // 注意：注册时使用的是硬编码的 "127.0.0.1:9000"，所以注销时也必须使用相同的值
    // TODO: 应该从配置或 Network 模块获取实际的 host:port，确保注册和注销使用相同的值
    const std::string host = "127.0.0.1";
    const uint32_t port = 9000;
    const std::string host_port_str = host + ":" + std::to_string(port);
    const auto host_port = paths_.BaseNodeRoot() + "/" + host_port_str;
    const auto module_path = host_port + "/" + module_id_str;
    
    // 先注销该模块注册的所有服务实例（这些是 ephemeral 节点，会自动删除）
    // 然后再删除模块目录（持久节点，需要手动删除）
    bool all_deregistered = true;
    
    for (auto key : handler_keys)
    {
        BaseNode::ServiceDiscovery::ServiceInstance service_instance;
        service_instance.service_name = std::to_string(key);
        service_instance.instance_id = std::to_string(MD5Hash32Constexpr(std::to_string(key)));
        service_instance.module_name = module->GetModuleClassName();
        // 使用与注册时相同的 host 和 port
        service_instance.host = host;
        service_instance.port = port;
        
        if (!registry_->DeRegisterService(service_instance))
        {
            BaseNodeLogWarn("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: failed to deregister service instance. service_instance:%s.",
                service_instance.SerializeInstance().c_str());
            all_deregistered = false;
        }
    }
    
    // 递归删除模块路径及其所有子节点（持久节点需要手动删除）
    BaseNodeLogInfo("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: attempting to delete module path: %s", module_path.c_str());
    bool delete_result = RecursiveDelete(zk_client_.get(), module_path);
    BaseNodeLogInfo("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: delete module path result: %s, path: %s", 
                   delete_result ? "success" : "failed", module_path.c_str());
    
    // 检查 host_port 目录是否为空，如果为空则删除
    // 注意：只有当该目录下没有其他模块时，才会删除它
    std::vector<std::string> remaining_modules = zk_client_->GetChildren(host_port);
    if (remaining_modules.empty())
    {
        // host_port 目录为空，删除它
        bool host_port_deleted = zk_client_->Delete(host_port);
        if (host_port_deleted)
        {
            BaseNodeLogInfo("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: deleted empty host_port directory: %s", host_port.c_str());
        }
        else
        {
            BaseNodeLogWarn("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: failed to delete empty host_port directory: %s", host_port.c_str());
        }
    }
    else
    {
        BaseNodeLogInfo("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: host_port directory %s still has %zu modules, not deleted", 
                       host_port.c_str(), remaining_modules.size());
    }
    
    if (delete_result && all_deregistered)
    {
        BaseNodeLogInfo("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: successfully deregistered module (id: %u, class: %s) from ZK",
            module->GetModuleId(), module_id_str.c_str());
        return true;
    }
    else
    {
        BaseNodeLogWarn("[ZkServiceDiscoveryModule] DeregisterModuleInServiceDiscovery: partially deregistered module (id: %u, class: %s) from ZK",
            module->GetModuleId(), module_id_str.c_str());
        return false;
    }
}

void ZkServiceDiscoveryModule::WatchServiceInstances(const std::string &service_name,
                ServiceDiscovery::InstanceChangeCallback cb)
{
    if (!discovery_)
    {
        BaseNodeLogError("[ZkServiceDiscoveryModule] WatchServiceInstances: discovery_ is null");
        return;
    }
    discovery_->WatchServiceInstances(service_name, std::move(cb));
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
static BaseNode::IModuleZkDiscovery* g_module_zk_discovery_instance = nullptr;
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

// 实现 IModuleZkDiscovery 接口的全局函数
// 使用 SO_EXPORT_SYMBOL 确保符号在所有模块间共享
extern "C" SO_EXPORT_SYMBOL BaseNode::IModuleZkDiscovery* GetModuleZkDiscoveryInstance()
{
    if (!g_module_zk_discovery_instance)
    {
        // 在 ZkServiceDiscoveryModule 初始化后创建
        auto* zk_module = ZkServiceDiscoveryMgr;
        if (zk_module)
        {
            // 使用静态变量确保生命周期
            static ModuleZkDiscoveryImpl impl(zk_module);
            g_module_zk_discovery_instance = &impl;
            BaseNodeLogInfo("[ZkServiceDiscovery] GetModuleZkDiscoveryInstance: created ModuleZkDiscoveryImpl instance");
        }
        else
        {
            BaseNodeLogWarn("[ZkServiceDiscovery] GetModuleZkDiscoveryInstance: ZkServiceDiscoveryMgr is null");
        }
    }
    else
    {
        BaseNodeLogWarn("[ZkServiceDiscovery] GetModuleZkDiscoveryInstance: ModuleZkDiscoveryImpl instance already exists");
    }
    return g_module_zk_discovery_instance;
}

} // namespace BaseNode::ServiceDiscovery::Zookeeper



