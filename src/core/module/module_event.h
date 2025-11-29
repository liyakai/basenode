#pragma once
#include "tools/ringbuffer.h"
#include "coro_rpc/coro_rpc_server.h"
#include <string>
#include <string_view>

namespace BaseNode
{

#define DEFAULT_MODULE_RING_BUFF_SIZE 256 * 1024

struct ModuleEvent
{
    enum class EventType
    {
        ET_NONE,
        ET_RPC_REQUEST,
        ET_RPC_RESPONSE,
    };
    EventType type_;
    union EventData
    {
        struct RpcRequest
        {
            std::string_view rpc_req_data_;
        } rpc_request_;

        struct RpcResponse
        {
            std::string_view rpc_rsp_data_;
        } rpc_rsponse_;
        
        // 默认构造函数（C++17 允许 union 包含非 POD 类型）
        EventData() : rpc_request_{} {}
        ~EventData() {}  // 需要显式析构函数来处理 std::string_view
    } data_;
    
    // 提供默认构造函数
    ModuleEvent() : type_(EventType::ET_NONE), data_() {}
    
    // 析构函数（需要显式处理 union 中的非 POD 类型）
    ~ModuleEvent() {
        // std::string_view 是 trivial destructible，不需要特殊处理
        // 但如果将来添加其他非 POD 类型，需要在这里处理
    }
};

} // namespace BaseNode