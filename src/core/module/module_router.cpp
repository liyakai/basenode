#include "module_router.h"
#include "module_event.h"
#include "module_interface.h"
#include "utils/basenode_def_internal.h"
#include "tools/string_util.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

namespace BaseNode
{

ErrorCode ModuleRouter::RegisterModule(IModule* module,  bool is_network_module /*  = false */)
{
    if (!module) {
        BaseNodeLogError("[ModuleRouter] RegisterModule: module is nullptr");
        return ErrorCode::BN_INVALID_ARGUMENTS;
    }

    // 获取模块ID
    uint32_t module_id = module->GetModuleId();
    
    BaseNodeLogDebug("[ModuleRouter] RegisterModule: this=%p, module=%p, is_network_module=%d, service_id_to_module_ size=%zu", 
        this, module, is_network_module, service_id_to_module_.size());

    if (is_network_module) {
        network_module_ = module;

        BaseNodeLogInfo("[ModuleRouter] RegisterModule: module (id: %u, class: %s) registered with network service", module_id, module->GetModuleClassName().c_str());
    } else {
        // 检查模块是否已经是网络模块
        if (network_module_ == module) {
            BaseNodeLogWarn("[ModuleRouter] RegisterModule: module (id: %u, class: %s) is already registered as network module, skip normal registration", module_id, module->GetModuleClassName().c_str());
            return ErrorCode::BN_SUCCESS;
        }
        
        BaseNodeLogDebug("[ModuleRouter] RegisterModule: checking normal registration for module (id: %u, class: %s), network_module_: %p, module: %p", 
            module_id, module->GetModuleClassName().c_str(), network_module_, module);
        
        // 检查模块是否已经注册
        if (module_id_to_module_.find(module_id) != module_id_to_module_.end()) {
            BaseNodeLogWarn("[ModuleRouter] RegisterModule: module (id: %u, class: %s) already registered", module_id, module->GetModuleClassName().c_str());
            return ErrorCode::BN_MODULE_ALREADY_REGISTERED;
        }

        // 获取模块的所有服务ID
        std::vector<uint32_t> service_ids = module->GetAllServiceHandlerKeys();

        if (service_ids.empty()) {
            BaseNodeLogWarn("[ModuleRouter] RegisterModule: module (id: %u, class: %s) has no service handlers", module_id, module->GetModuleClassName().c_str());
        }

        // 注册服务ID到模块的映射
        for (uint32_t service_id : service_ids) {
            if (service_id_to_module_.find(service_id) != service_id_to_module_.end()) {
                BaseNodeLogError("[ModuleRouter] RegisterModule: service_id %u already registered to another module", service_id);
                // 清理已注册的服务ID
                for (uint32_t sid : service_ids) {
                    auto it = service_id_to_module_.find(sid);
                    if (it != service_id_to_module_.end() && it->second == module) {
                        service_id_to_module_.erase(it);
                    }
                }
                return ErrorCode::BN_SERVICE_ID_ALREADY_REGISTERED;
            }
            service_id_to_module_[service_id] = module;
            BaseNodeLogDebug("[ModuleRouter] RegisterService: service_id %u -> module_id %u (class: %s), service_id_to_module_ size: %zu", service_id, module_id, module->GetModuleClassName().c_str(), service_id_to_module_.size());
        }

        // 注册模块ID到模块的映射
        module_id_to_module_[module_id] = module;

        module->SetClientSendCallback([this](std::string &&data){
            RouteRpcRequest(std::move(data));
        });
        module->SetServerSendCallback([this](uint64_t conn_id, std::string &&data)
        {
            RouteRpcResponse(std::move(data));
        });

        BaseNodeLogInfo("[ModuleRouter] RegisterService: module (id: %u, class: %s) registered with %zu services, services: %s", 
            module_id, module->GetModuleClassName().c_str(), service_ids.size(), ToolBox::VectorToStr(service_ids).c_str());
    }

    return ErrorCode::BN_SUCCESS;
}

ErrorCode ModuleRouter::UnregisterModule(IModule* module)
{
    if (!module) {
        return ErrorCode::BN_INVALID_ARGUMENTS;
    }

    uint32_t module_id = module->GetModuleId();
    
    // 移除服务ID映射
    auto it = service_id_to_module_.begin();
    while (it != service_id_to_module_.end()) {
        if (it->second == module) {
            it = service_id_to_module_.erase(it);
        } else {
            ++it;
        }
    }

    // 移除模块ID映射
    module_id_to_module_.erase(module_id);
    
    BaseNodeLogInfo("[ModuleRouter] UnregisterModule: module (id: %u) unregistered", module_id);

    return ErrorCode::BN_SUCCESS;
}

ErrorCode ModuleRouter::CallAllModulesAfterInit()
{
    BaseNodeLogInfo("[ModuleRouter] CallAllModulesAfterInit: calling AfterAllModulesInit for %zu modules", module_id_to_module_.size());
    
    ErrorCode first_error = ErrorCode::BN_SUCCESS;
    for (const auto &pair : module_id_to_module_)
    {
        IModule *module = pair.second;
        if (!module)
        {
            continue;
        }
        ErrorCode err = module->AfterAllModulesInit();
        if (err != ErrorCode::BN_SUCCESS)
        {
            BaseNodeLogError("[ModuleRouter] CallAllModulesAfterInit: module (id: %u, class: %s) AfterAllModulesInit failed, error: %d",
                            pair.first, module->GetModuleClassName().c_str(), static_cast<int>(err));
            if (first_error == ErrorCode::BN_SUCCESS)
            {
                first_error = err;
            }
        }
    }
    
    BaseNodeLogInfo("[ModuleRouter] CallAllModulesAfterInit: completed, %zu modules processed", module_id_to_module_.size());
    return first_error;
}

IModule* ModuleRouter::FindModuleByServiceId(uint32_t service_id) const
{
    BaseNodeLogTrace("[ModuleRouter] FindModuleByServiceId: this=%p, service_id=%u, service_id_to_module_ size=%zu", 
        this, service_id, service_id_to_module_.size());
    auto it = service_id_to_module_.find(service_id);
    if (it != service_id_to_module_.end()) {
        return it->second;
    }
    BaseNodeLogError("[ModuleRouter] FindModuleByServiceId: service_id %u not found in any module, service_id_to_module_ size: %zu", service_id, service_id_to_module_.size());
    return nullptr;
}

IModule* ModuleRouter::FindModuleByModuleId(uint32_t module_id) const
{
    auto it = module_id_to_module_.find(module_id);
    if (it != module_id_to_module_.end()) {
        return it->second;
    }
    return nullptr;
}

ErrorCode ModuleRouter::RouteRpcRequest(std::string &&rpc_data)
{
    return RouteRpcData_(std::move(rpc_data), ModuleEvent::EventType::ET_RPC_REQUEST);
}

ErrorCode ModuleRouter::RouteRpcResponse(std::string &&rpc_data)
{
    return RouteRpcData_(std::move(rpc_data), ModuleEvent::EventType::ET_RPC_RESPONSE);
}

ErrorCode ModuleRouter::RouteProtocolPacket(std::string &&protocol_data)
{
    // 网络协议包也是RPC格式，使用相同的路由逻辑
    return RouteRpcRequest(std::move(protocol_data));
}

std::tuple<uint32_t, uint64_t> ModuleRouter::ExtractServiceIdClientIDFromRpc_(std::string_view rpc_data)
{
    using namespace ToolBox::CoroRpc;
    
    // 读取RPC协议头
    CoroRpcProtocol::ReqHeader header;
    Errc err = CoroRpcProtocol::ReadHeader(rpc_data, header);
    if (err != Errc::SUCCESS) {
        BaseNodeLogError("[ModuleRouter] ExtractServiceIdClientIDFromRpc_: failed to read header, err: %d", static_cast<int>(err));
        return std::tuple<uint32_t, uint64_t>(0, 0);;
    }

    // 从协议头中提取服务ID
    uint32_t service_id = CoroRpcProtocol::GetRpcFuncKey(header);
    uint64_t client_id = CoroRpcProtocol::GetClientID(header);
    return std::tuple<uint32_t, uint64_t>(service_id, client_id);
}

ErrorCode ModuleRouter::RouteRpcData_(std::string &&rpc_data, ModuleEvent::EventType event_type)
{
    auto [service_id, client_id] = ExtractServiceIdClientIDFromRpc_(std::string_view(rpc_data));
    if (service_id == 0 || client_id == 0) {
        BaseNodeLogError("[ModuleRouter] RouteRpcRequest: failed to extract service_id from RPC data");
        return ErrorCode::BN_INVALID_ARGUMENTS;
    }

    uint32_t module_service_id = 0;
    IModule* module = nullptr;
    ModuleEvent event;
    event.type_ = event_type;
    if (event_type == ModuleEvent::EventType::ET_RPC_REQUEST) {
        event.data_.rpc_request_.rpc_req_data_ = std::move(rpc_data);
        module_service_id = service_id;
        module = FindModuleByServiceId(module_service_id);
    } else if (event_type == ModuleEvent::EventType::ET_RPC_RESPONSE) {
        event.data_.rpc_rsponse_.rpc_rsp_data_ = std::move(rpc_data);
        module_service_id = client_id;
        module = FindModuleByModuleId(module_service_id);
    }

    // 查找对应的模块

    if (!module) {
        if (network_module_) 
        {
            ErrorCode err = network_module_->PushModuleEvent(std::move(event));
            if (err != ErrorCode::BN_SUCCESS) {
                BaseNodeLogError("[ModuleRouter] RouteRpcData(type:%d): failed to push event to network module, error: %d", event_type, static_cast<int>(err));
                return err;
            }
        }
        BaseNodeLogError("[ModuleRouter] RouteRpcData(type:%d): module_service_id %u not found in any module", event_type, module_service_id);
        return ErrorCode::BN_SERVICE_ID_NOT_FOUND;
    }

    ErrorCode err = module->PushModuleEvent(std::move(event));
    if (err != ErrorCode::BN_SUCCESS) {
        BaseNodeLogError("[ModuleRouter] RouteRpcRequest: failed to push event to module, error: %d", static_cast<int>(err));
        return err;
    }

    BaseNodeLogTrace("[ModuleRouter] RouteRpcData(type:%d): routed module_service_id %u to module_id %u", 
                    event_type, module_service_id, module->GetModuleId());

    return ErrorCode::BN_SUCCESS;
}

} // namespace BaseNode

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_INIT() {
    // ModuleRouterMgr->Init();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UPDATE() {
    // ModuleRouterMgr->Update();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UNINIT() {
    // ModuleRouterMgr->UnInit();  // 调用基类的UnInit方法
}