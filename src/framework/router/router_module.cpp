#include "router/router_module.h"
#include "net/network.h"
#include "service_discovery/zookeeper/zk_paths.h"

namespace BaseNode
{

RouterModule::RouterModule()
    : network_impl_(nullptr)
    , initialized_(false)
{
}

RouterModule::~RouterModule()
{
}

ErrorCode RouterModule::DoInit()
{
    BaseNodeLogInfo("[RouterModule] DoInit");

    // Network 模块可能还未加载，在 DoAfterAllModulesInit 中初始化
    // 这里只做基本初始化
    initialized_ = true;
    BaseNodeLogInfo("[RouterModule] DoInit: initialized");
    return ErrorCode::BN_SUCCESS;
}

ErrorCode RouterModule::DoUpdate()
{
    return ErrorCode::BN_SUCCESS;
}

ErrorCode RouterModule::DoUninit()
{
    BaseNodeLogInfo("[RouterModule] DoUninit");

    service_to_conn_.clear();

    network_impl_ = nullptr;
    initialized_ = false;

    return ErrorCode::BN_SUCCESS;
}

ErrorCode RouterModule::DoAfterAllModulesInit()
{
    BaseNodeLogInfo("[RouterModule] DoAfterAllModulesInit: starting service discovery");

    // 获取 Network 模块（此时所有模块都已加载并初始化）
    // 注意：NetworkMgr 是 Singleton，但每个共享库可能有自己的 Singleton 实例
    // 通过 ModuleRouter 获取 Network 模块，确保使用正确的实例
    IModule* network_module = ModuleRouterMgr->GetNetworkModule();
    if (!network_module) {
        BaseNodeLogError("[RouterModule] DoAfterAllModulesInit: Network module not found in ModuleRouter");
        // 尝试通过 NetworkMgr 获取
        Network* network = dynamic_cast<Network*>(NetworkMgr);
        if (!network) {
            BaseNodeLogError("[RouterModule] DoAfterAllModulesInit: Network module not found, NetworkMgr=%p", NetworkMgr);
            return ErrorCode::BN_INVALID_ARGUMENTS;
        }
        network_module = network;
    }
    
    Network* network = dynamic_cast<Network*>(network_module);
    if (!network) {
        BaseNodeLogError("[RouterModule] DoAfterAllModulesInit: Failed to cast to Network, network_module=%p", network_module);
        return ErrorCode::BN_INVALID_ARGUMENTS;
    }
    
    BaseNodeLogInfo("[RouterModule] DoAfterAllModulesInit: Network module found, network=%p", network);

    // 获取 network_impl_
    network_impl_ = network->GetNetwork();
    if (!network_impl_) {
        BaseNodeLogError("[RouterModule] DoAfterAllModulesInit: network_impl_ is null, Network module may not be initialized");
        BaseNodeLogError("[RouterModule] DoAfterAllModulesInit: Network module state: network=%p, GetNetwork()=%p", network, network->GetNetwork());
        return ErrorCode::BN_INVALID_ARGUMENTS;
    }
    
    BaseNodeLogInfo("[RouterModule] DoAfterAllModulesInit: Network module found, network_impl_=%p", network_impl_);

    // 设置网络回调（主动连接模式）
    network_impl_->SetOnConnected([this](ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id) {
        OnConnected(type, opaque, conn_id);
    });

    network_impl_->SetOnConnectFailed([this](ToolBox::NetworkType type, uint64_t opaque,
                                            ToolBox::ENetErrCode err_code, int32_t err_no) {
        OnConnectFailed(type, opaque, err_code, err_no);
    });

    network_impl_->SetOnClose([this](ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id,
                                    ToolBox::ENetErrCode net_err, int32_t sys_err) {
        OnClose(type, opaque, conn_id, net_err, sys_err);
    });

    network_impl_->SetOnReceived([this](ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id,
                                       const char* data, size_t size) {
        OnReceived(type, opaque, conn_id, data, size);
    });

    if (!ModuleZkDiscoveryMgr) {
        BaseNodeLogError("[RouterModule] DoAfterAllModulesInit: ModuleZkDiscoveryMgr is null");
        return ErrorCode::BN_INVALID_ARGUMENTS;
    }

    // 发现所有服务并建立连接
    DiscoverAndConnectAllServices();

    // 监听服务目录变化，动态发现新服务
    // 通过 Zookeeper 客户端监听 /basenode/services 目录
    // 当发现新服务时，自动监听其实例变化
    
    BaseNodeLogInfo("[RouterModule] DoAfterAllModulesInit: service discovery ready");
    return ErrorCode::BN_SUCCESS;
}

void RouterModule::OnConnected(ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id)
{
    std::string host;
    uint16_t port = 0;
    {
        auto it = pending_connections_.find(opaque);
        if (it == pending_connections_.end()) {
            BaseNodeLogWarn("[RouterModule] OnConnected: pending connection not found, opaque=%lu", opaque);
            return;
        }
        host = it->second.first;
        port = it->second.second;
        pending_connections_.erase(it);
    }

    // 同一 host:port 下所有实例共用这一条连接
    int count = SetConnectionID(host, port, conn_id);
    BaseNodeLogInfo("[RouterModule] OnConnected: connected to %s:%u, conn_id=%lu, instances=%d (one connection shared)",
                   host.c_str(), port, conn_id, count);
}

void RouterModule::OnConnectFailed(ToolBox::NetworkType type, uint64_t opaque,
                                   ToolBox::ENetErrCode err_code, int32_t err_no)
{
    BaseNodeLogError("[RouterModule] OnConnectFailed: type=%d, opaque=%lu, err_code=%d, err_no=%d",
                    static_cast<int>(type), opaque, static_cast<int>(err_code), err_no);

    // 清理待连接记录
    {
        pending_connections_.erase(opaque);
    }
}

void RouterModule::OnClose(ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id,
                          ToolBox::ENetErrCode net_err, int32_t sys_err)
{
    BaseNodeLogInfo("[RouterModule] OnClose: type=%d, opaque=%lu, conn_id=%lu, net_err=%d, sys_err=%d",
                   static_cast<int>(type), opaque, conn_id, static_cast<int>(net_err), sys_err);

    // 查找对应的实例键
    std::vector<uint64_t> instance_keys = GetInstanceIDsByConnectionID(conn_id);

    for (const auto& instance_key : instance_keys) {
        key_to_instance_.erase(instance_key);
    }
}

void RouterModule::OnReceived(ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id,
                             const char* data, size_t size)
{
    std::string rpc_data(data, size);

    // 提取服务ID和客户端ID
    auto [service_id, client_id] = ExtractServiceIdClientId(std::string_view(rpc_data));
    if (service_id == 0 || client_id == 0) {
        BaseNodeLogError("[RouterModule] OnReceived: failed to extract service_id/client_id");
        return;
    }

    // 判断是请求还是响应
    using namespace ToolBox::CoroRpc;
    CoroRpcProtocol::ReqHeader header;
    Errc err = CoroRpcProtocol::ReadHeader(rpc_data, header);
    if (err != Errc::SUCCESS) {
        BaseNodeLogError("[RouterModule] OnReceived: failed to read header");
        return;
    }

    // 判断是请求还是响应：请求的 msg_type 通常是 0 或特定值
    // 简化判断：根据协议头判断，实际需要根据协议定义
    bool is_request = true;  // TODO: 根据实际协议判断请求/响应

    if (is_request) {
        // RPC 请求：路由到目标进程
        RouteRpcRequest(service_id, client_id, conn_id, std::move(rpc_data));
    } else {
        // RPC 响应：路由回源进程
        // 响应来自目标进程（conn_id），需要路由回源进程
        // 简化：通过 client_id（目标模块ID）查找源连接
        RouteRpcResponse(client_id, conn_id, std::move(rpc_data));
    }
}

void RouterModule::OnServiceInstancesChanged(const std::string& zk_path, const ServiceDiscovery::InstanceList& instances)
{
    BaseNodeLogInfo("[RouterModule] OnServiceInstancesChanged: service_name=%s, instances=%zu",
                   zk_path.c_str(), instances.size());

    // 收集当前实例的键
    std::unordered_set<uint64_t> current_instance_keys;
    for (const auto& instance : instances) {
        if (instance.healthy) {
            current_instance_keys.insert(instance.instance_id);
        }
    }

    // 断开不再存在的实例
    std::vector<ServiceDiscovery::ServiceInstance> instances_to_disconnect;
    {
        for (auto& [instance_key, instance_struct] : key_to_instance_) {
            if (current_instance_keys.find(instance_struct.instance_id) == current_instance_keys.end()) {
                instances_to_disconnect.push_back(instance_struct);
            }
        }
    }
    for (const auto& instance : instances_to_disconnect) {
        DisconnectFromInstance(instance);
    }

    // // 连接新的实例或更新路由表
    // std::lock_guard<std::mutex> lock(service_to_conn_mutex_);
    
    // // 先清理该服务的旧路由
    // service_to_conn_.erase(service_id);

    // 选择第一个健康的实例并连接
    for (const auto& instance : instances) {
        if (!instance.healthy) 
        {
            const std::string inst_str = instance.SerializeInstance();
            BaseNodeLogWarn("[RouterModule] OnServiceInstancesChanged: instance %s is not healthy", inst_str.c_str());
            continue;
        }
        BaseNodeLogDebug("[RouterModule] OnServiceInstancesChanged: instance %s.", instance.SerializeInstance().c_str());

        auto iter_instance = key_to_instance_.find(instance.instance_id);
        if (iter_instance == key_to_instance_.end()) {
            if (CheckIPPortExist(instance.host, instance.port)) {
                key_to_instance_[instance.instance_id] = instance;
            } else {
                // 新实例，需要连接
                ConnectToInstance(instance);
            }
            continue;
        }
        
        // 已存在的实例，检查是否需要更新
        auto &exist_instance = iter_instance->second;
        if (exist_instance.connection_id == 0) {
            ConnectToInstance(instance);
            continue;
        }
        if (!exist_instance.healthy) {
            DisconnectFromInstance(exist_instance);
            ConnectToInstance(instance);
            continue;
        }
        if (exist_instance.host == instance.host && exist_instance.port == instance.port) {
            BaseNodeLogDebug("[RouterModule] OnServiceInstancesChanged: instance %s is already connected", instance.SerializeInstance().c_str());
            continue;
        } else {
            DisconnectFromInstance(exist_instance);
            ConnectToInstance(instance);
            continue;
        }
    }
    BaseNodeLogInfo("[RouterModule] OnServiceInstancesChanged: instances changed, current_instance_keys=%zu, instances:%d, key_to_instance_size:%d", 
        current_instance_keys.size(), instances.size(), key_to_instance_.size());
}

void RouterModule::ConnectToInstance(const ServiceDiscovery::ServiceInstance& instance)
{
    if (instance.connection_id != 0) {
        BaseNodeLogTrace("[RouterModule] ConnectToInstance: already connected to %s", instance.SerializeInstance().c_str());
        return;
    }

    // 已有同一 host:port 的已建立连接 -> 复用，只把本实例加入并设 connection_id
    {
        uint64_t existing_conn_id = GetConnectionIDbyIPPort(instance.host, instance.port);
        if (existing_conn_id != 0) {
            auto inst_copy = instance;
            inst_copy.connection_id = existing_conn_id;
            inst_copy.healthy = true;
            key_to_instance_[instance.instance_id] = inst_copy;
            BaseNodeLogTrace("[RouterModule] ConnectToInstance: reusing connection %s:%u conn_id=%lu for instance %lu",
                             instance.host.c_str(), instance.port, existing_conn_id, instance.instance_id);
            return;
        }
    }

    // 已有同一 host:port 的待连接 -> 只把本实例加入，等 OnConnected 时一起设 conn_id
    {
        for (const auto& pair : pending_connections_) {
            if (pair.second.first == instance.host && pair.second.second == instance.port) {
                key_to_instance_[instance.instance_id] = instance;
                BaseNodeLogTrace("[RouterModule] ConnectToInstance: connection in progress to %s:%u for instance %lu",
                                 instance.host.c_str(), instance.port, instance.instance_id);
                return;
            }
        }
    }

    // 首次对该 host:port 发起连接
    uint64_t opaque = next_opaque_.fetch_add(1);
    pending_connections_[opaque] = std::make_pair(instance.host, instance.port);
    key_to_instance_[instance.instance_id] = instance;
    network_impl_->Connect(ToolBox::NetworkType::NT_TCP, opaque, instance.host, instance.port);
    BaseNodeLogInfo("[RouterModule] ConnectToInstance: connecting to %s:%u, opaque=%lu (one connection for all instances at this address)",
                    instance.host.c_str(), instance.port, opaque);
}

void RouterModule::DisconnectFromInstance(const ServiceDiscovery::ServiceInstance& instance)
{
    if (instance.connection_id == 0 || !network_impl_)
        return;
    uint64_t conn_id = instance.connection_id;
    // 同一连接被多个实例复用，只 Close 一次并清理所有共享该连接的实例
    std::vector<uint64_t> instance_ids = GetInstanceIDsByConnectionID(conn_id);
    network_impl_->Close(conn_id);
    for (uint64_t id : instance_ids)
        key_to_instance_.erase(id);
    BaseNodeLogInfo("[RouterModule] DisconnectFromInstance: closed conn_id=%lu, cleared %zu instances at %s:%u",
                   conn_id, instance_ids.size(), instance.host.c_str(), instance.port);
}

std::tuple<uint32_t, uint64_t> RouterModule::ExtractServiceIdClientId(std::string_view rpc_data)
{
    using namespace ToolBox::CoroRpc;
    
    CoroRpcProtocol::ReqHeader header;
    Errc err = CoroRpcProtocol::ReadHeader(rpc_data, header);
    if (err != Errc::SUCCESS) {
        return std::make_tuple(0, 0);
    }

    uint32_t service_id = CoroRpcProtocol::GetRpcFuncKey(header);
    uint64_t client_id = CoroRpcProtocol::GetClientID(header);
    return std::make_tuple(service_id, client_id);
}

ErrorCode RouterModule::RouteRpcRequest(uint32_t service_id, uint64_t client_id, uint64_t source_conn_id, std::string&& rpc_data)
{
    BaseNodeLogTrace("[RouterModule] RouteRpcRequest: service_id=%u, client_id=%lu, source_conn_id=%lu",
                    service_id, client_id, source_conn_id);

    // 查找目标连接
    uint64_t target_conn_id = 0;
    {
        auto it = service_to_conn_.find(service_id);
        if (it == service_to_conn_.end()) {
            BaseNodeLogError("[RouterModule] RouteRpcRequest: service_id %u not found in routing table", service_id);
            return ErrorCode::BN_SERVICE_ID_NOT_FOUND;
        }
        target_conn_id = it->second;
    }

    // 保存请求上下文（用于响应路由）
    // 简化：使用 (target_conn_id, client_id) 作为键
    // 更好的方式：使用请求ID
    // TODO: 实现请求上下文管理

    // 发送到目标进程
    if (network_impl_ && target_conn_id != 0) {
        ToolBox::ENetErrCode err = network_impl_->Send(target_conn_id, rpc_data.data(), static_cast<uint32_t>(rpc_data.size()));
        if (err != ToolBox::ENetErrCode::NET_SUCCESS) {
            BaseNodeLogError("[RouterModule] RouteRpcRequest: failed to send, error: %d", static_cast<int>(err));
            return ErrorCode::BN_NETWORK_START_FAILED;
        }
        BaseNodeLogTrace("[RouterModule] RouteRpcRequest: routed service_id=%u from conn_id=%lu to conn_id=%lu",
                        service_id, source_conn_id, target_conn_id);
    }

    return ErrorCode::BN_SUCCESS;
}

ErrorCode RouterModule::RouteRpcResponse(uint64_t target_module_id, uint64_t response_conn_id, std::string&& rpc_data)
{
    BaseNodeLogTrace("[RouterModule] RouteRpcResponse: target_module_id=%lu, response_conn_id=%lu", 
                    target_module_id, response_conn_id);

    // 响应路由：响应来自目标进程（response_conn_id），需要路由回源进程
    // 简化实现：通过请求上下文查找源连接
    // 更好的方式：在请求时保存上下文（源连接ID），响应时通过上下文查找
    
    // 暂时简化：假设响应会通过相同的连接返回（实际需要维护请求上下文）
    // TODO: 实现请求上下文管理，正确路由响应
    
    // 这里 response_conn_id 是目标进程的连接，需要找到源进程的连接
    // 简化：假设响应会原路返回（实际需要维护请求上下文）
    
    BaseNodeLogWarn("[RouterModule] RouteRpcResponse: simplified implementation, response routing needs improvement");
    return ErrorCode::BN_SUCCESS;
}

void RouterModule::DiscoverAndConnectAllServices()
{
    if (!ModuleZkDiscoveryMgr) {
        BaseNodeLogError("[RouterModule] DiscoverAndConnectAllServices: ModuleZkDiscoveryMgr is null");
        return;
    }
    
    // // 获取所有服务名
    // std::vector<std::string> service_names = ModuleZkDiscoveryMgr->GetAllServiceNames();
    // BaseNodeLogInfo("[RouterModule] DiscoverAndConnectAllServices: found %zu services", service_names.size());

    // 获取所有的实例
    ServiceDiscovery::InstanceList instance_list = ModuleZkDiscoveryMgr->GetServiceInstances("/basenode/services");

    BaseNodeLogInfo("[RouterModule] DiscoverAndConnectAllServices: found %zu instances---------------------------", instance_list.size());

    // 只对 /basenode/services 注册一次监听，避免同一路径被注册多个 watcher 导致重复回调和重复日志
    const std::string services_path("/basenode/services");
    {
        std::lock_guard<std::mutex> lock(watched_services_mutex_);
        if (watched_services_.find(services_path) != watched_services_.end()) {
            BaseNodeLogInfo("[RouterModule] DiscoverAndConnectAllServices: already watching %s", services_path.c_str());
        } else {
            watched_services_.insert(services_path);
            ModuleZkDiscoveryMgr->WatchServiceInstances(services_path,
                instance_list,
                [this](const std::string& svc_name, const ServiceDiscovery::InstanceList& instances) {
                    OnServiceInstancesChanged(svc_name, instances);
                });
        }
    }

    // 用当前实例列表做一次同步处理（连接等）
    // OnServiceInstancesChanged(services_path, instance_list);

    BaseNodeLogInfo("[RouterModule] DiscoverAndConnectAllServices, begin to watch services directory. --------------------------------");

    // 监听服务目录变化，动态发现新服务
    ModuleZkDiscoveryMgr->WatchServicesDirectory(
        [this, &instance_list](const std::string& service_name, const ServiceDiscovery::InstanceList& instances) {
            // 检查是否是新服务
            bool is_new_service = false;
            {
                std::lock_guard<std::mutex> lock(watched_services_mutex_);
                if (watched_services_.find(service_name) == watched_services_.end()) {
                    watched_services_.insert(service_name);
                    is_new_service = true;
                }
            }
            
            if (is_new_service) {
                // 监听新服务的实例变化
                ModuleZkDiscoveryMgr->WatchServiceInstances(service_name,
                    instance_list,
                    [this](const std::string& svc_name, const ServiceDiscovery::InstanceList& insts) {
                        OnServiceInstancesChanged(svc_name, insts);
                    });
            }
            
            // 处理服务实例变化
            // OnServiceInstancesChanged(service_name, instance_list);
        });

    BaseNodeLogInfo("[RouterModule] DiscoverAndConnectAllServices, end to watch services directory. --------------------------------");
}

void RouterModule::OnServicesDirectoryChanged(const std::string& path)
{
    BaseNodeLogInfo("[RouterModule] OnServicesDirectoryChanged: path=%s", path.c_str());
    
    // 获取所有服务名
    if (!ModuleZkDiscoveryMgr) {
        return;
    }

    // 遍历服务目录，对每个服务监听实例变化
    // 这里需要访问 Zookeeper 客户端来获取子节点
    // 暂时通过其他方式实现
}

uint64_t RouterModule::GetConnectionIDbyIPPort(const std::string& ip, uint16_t port)
{
    for (const auto& [key, instance] : key_to_instance_) {
        if (instance.host == ip && instance.port == port) {
            return instance.connection_id;
        }
    }
    return 0;
}

bool RouterModule::CheckIPPortExist(const std::string& ip, uint16_t port)
{
    for (const auto& [key, instance] : key_to_instance_) {
        if (instance.host == ip && instance.port == port) {
            return true;
        }
    }
    return false;
}

int RouterModule::SetConnectionID(const std::string& ip, uint16_t port, uint64_t connection_id)
{
    int count = 0;
    for (auto& [key, instance] : key_to_instance_) {
        if (instance.host == ip && instance.port == port) {
            instance.connection_id = connection_id;
            instance.healthy = true;
            count++;
            BaseNodeLogDebug("[RouterModule] SetConnectionID: conn_id=%lu for instance %lu", connection_id, instance.instance_id);
        }
    }
    return count;
}

std::vector<uint64_t> RouterModule::GetInstanceIDsByConnectionID(uint64_t connection_id)
{
    std::vector<uint64_t> instance_ids;
    for (const auto& [key, instance] : key_to_instance_) {
        if (instance.connection_id == connection_id) {
            instance_ids.push_back(instance.instance_id);
        }
    }
    return instance_ids;
}


} // namespace BaseNode

// 导出符号
extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_INIT() {
    using namespace BaseNode;
    RouterModuleMgr->Init();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UPDATE() {
    using namespace BaseNode;
    RouterModuleMgr->Update();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UNINIT() {
    using namespace BaseNode;
    RouterModuleMgr->UnInit();
}
