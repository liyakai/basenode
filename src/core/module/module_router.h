#pragma once

#include "tools/singleton.h"
#include "utils/basenode_def_internal.h"
#include "module_event.h"
#include <unordered_map>
#include <vector>
#include <string_view>

namespace BaseNode
{
class IModule;
}

namespace BaseNode
{
/**
 * @brief 模块路由管理器
 * 负责维护服务ID到模块的映射，并提供路由功能
 */
class ModuleRouter
{
public:
    /**
     * @brief 注册模块及其服务ID
     * @param module 模块指针
     * @return 是否注册成功
     */
     ErrorCode RegisterModule(IModule* module,  bool is_network_module = false);

    /**
     * @brief 注销模块
     * @param module 模块指针
     */
    ErrorCode UnregisterModule(IModule* module);

    /**
     * @brief 路由网络协议包到对应的模块
     * @param protocol_data 协议数据包
     * @return 是否成功路由
     */
    ErrorCode RouteProtocolPacket(std::string &&protocol_data);

private:

    /**
     * @brief 根据服务ID查找对应的模块
     * @param service_id 服务ID
     * @return 模块指针，如果未找到返回nullptr
     */
    IModule* FindModuleByServiceId(uint32_t service_id) const;

    /**
     * @brief 根据模块ID查找模块
     * @param module_id 模块ID
     * @return 模块指针，如果未找到返回nullptr
     */
    IModule* FindModuleById(uint32_t module_id) const;

    /**
     * @brief 路由RPC请求到对应的模块
     * @param rpc_data RPC数据包（右值引用，强制移动语义，避免拷贝）
     * @return 错误码
     */
    ErrorCode RouteRpcRequest(std::string &&rpc_data);
    /**
     * @brief 路由RPC回包到对应的模块
     * @param rpc_data RPC数据包（右值引用，强制移动语义，避免拷贝）
     * @return 错误码
     */
    ErrorCode RouteRpcResponse(std::string &&rpc_data);


private:
    /**
     * @brief 从RPC数据包中提取服务ID
     * @param rpc_data RPC数据包
     * @return 服务ID，如果提取失败返回0
     */
     uint32_t ExtractServiceIdFromRpc_(std::string_view rpc_data);

     ErrorCode RouteRpcData_(std::string &&rpc_data, ModuleEvent::EventType event_type);



private:
    // 服务ID到模块的映射表
    std::unordered_map<uint32_t, IModule*> service_id_to_module_;
    
    // 模块ID到模块的映射表（用于快速查找）
    std::unordered_map<uint32_t, IModule*> module_id_to_module_;

    IModule* network_module_ = nullptr;
};

#define ModuleRouterMgr ToolBox::Singleton<BaseNode::ModuleRouter>::Instance()

} // namespace BaseNode

