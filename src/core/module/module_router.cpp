#include "module_router.h"
#include "module_event.h"
#include "module_interface.h"
#include "utils/basenode_def_internal.h"
#include <cstdint>
#include <string>
#include <string_view>

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

    if (is_network_module) {
        network_module_ = module;

        BaseNodeLogInfo("[ModuleRouter] RegisterModule: module (id: %u) registered with network service", module_id);
    } else {
        // 检查模块是否已经注册
        if (module_id_to_module_.find(module_id) != module_id_to_module_.end()) {
            BaseNodeLogWarn("[ModuleRouter] RegisterModule: module (id: %u) already registered", module_id);
            return ErrorCode::BN_MODULE_ALREADY_REGISTERED;
        }

        // 获取模块的所有服务ID
        std::vector<uint32_t> service_ids = module->GetAllServiceHandlerKeys();

        if (service_ids.empty()) {
            BaseNodeLogWarn("[ModuleRouter] RegisterModule: module (id: %u) has no service handlers", module_id);
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
            BaseNodeLogDebug("[ModuleRouter] RegisterModule: service_id %u -> module_id %u", service_id, module_id);
        }

        // 注册模块ID到模块的映射
        module_id_to_module_[module_id] = module;

        module->SetClientSendCallback([this](std::string_view && data){
            RouteRpcRequest(data);
        });
        module->SetServerSendCallback([this](uint64_t conn_id, std::string_view&& data)
        {
            RouteRpcResponse(data);
        });

        BaseNodeLogInfo("[ModuleRouter] RegisterModule: module (id: %u) registered with %zu services", 
            module_id, service_ids.size());
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

IModule* ModuleRouter::FindModuleByServiceId(uint32_t service_id) const
{
    auto it = service_id_to_module_.find(service_id);
    if (it != service_id_to_module_.end()) {
        return it->second;
    }
    return nullptr;
}

IModule* ModuleRouter::FindModuleById(uint32_t module_id) const
{
    auto it = module_id_to_module_.find(module_id);
    if (it != module_id_to_module_.end()) {
        return it->second;
    }
    return nullptr;
}

ErrorCode ModuleRouter::RouteRpcRequest(std::string_view rpc_data)
{
    return RouteRpcData_(rpc_data, ModuleEvent::EventType::ET_RPC_REQUEST);
}

ErrorCode ModuleRouter::RouteRpcResponse(std::string_view rpc_data)
{
    return RouteRpcData_(rpc_data, ModuleEvent::EventType::ET_RPC_RESPONSE);
}

ErrorCode ModuleRouter::RouteProtocolPacket(std::string_view protocol_data)
{
    // 网络协议包也是RPC格式，使用相同的路由逻辑
    return RouteRpcRequest(protocol_data);
}

uint32_t ModuleRouter::ExtractServiceIdFromRpc_(std::string_view rpc_data)
{
    using namespace ToolBox::CoroRpc;
    
    // 读取RPC协议头
    CoroRpcProtocol::ReqHeader header;
    Errc err = CoroRpcProtocol::ReadHeader(rpc_data, header);
    if (err != Errc::SUCCESS) {
        BaseNodeLogError("[ModuleRouter] ExtractServiceIdFromRpc_: failed to read header, err: %d", static_cast<int>(err));
        return 0;
    }

    // 从协议头中提取服务ID
    uint32_t service_id = CoroRpcProtocol::GetRpcFuncKey(header);
    return service_id;
}

ErrorCode ModuleRouter::RouteRpcData_(std::string_view rpc_data, ModuleEvent::EventType event_type)
{
    // 从RPC数据包中提取服务ID
    uint32_t service_id = ExtractServiceIdFromRpc_(rpc_data);
    if (service_id == 0) {
        BaseNodeLogError("[ModuleRouter] RouteRpcRequest: failed to extract service_id from RPC data");
        return ErrorCode::BN_INVALID_ARGUMENTS;
    }

    // 创建RPC请求事件并推送到模块
    ModuleEvent event;
    event.type_ = event_type;
    event.data_.rpc_request_.rpc_req_data_ = rpc_data;

    // 查找对应的模块
    IModule* module = FindModuleByServiceId(service_id);
    if (!module) {
        if (network_module_) 
        {
            ErrorCode err = network_module_->PushModuleEvent(std::move(event));
            if (err != ErrorCode::BN_SUCCESS) {
                BaseNodeLogError("[ModuleRouter] RouteRpcData(type:%d): failed to push event to network module, error: %d", event_type, static_cast<int>(err));
                return err;
            }
        }
        return ErrorCode::BN_SERVICE_ID_NOT_FOUND;
    }

    ErrorCode err = module->PushModuleEvent(std::move(event));
    if (err != ErrorCode::BN_SUCCESS) {
        BaseNodeLogError("[ModuleRouter] RouteRpcRequest: failed to push event to module, error: %d", static_cast<int>(err));
        return err;
    }

    BaseNodeLogDebug("[ModuleRouter] RouteRpcData(type:%d): routed service_id %u to module_id %u", 
                    event_type, service_id, module->GetModuleId());

    return ErrorCode::BN_SUCCESS;
}

} // namespace BaseNode

