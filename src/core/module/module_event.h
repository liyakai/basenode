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
            std::string rpc_req_data_;
            RpcRequest() = default;
            RpcRequest(const RpcRequest&) = default;
            RpcRequest(RpcRequest&&) noexcept = default;
            RpcRequest& operator=(const RpcRequest&) = default;
            RpcRequest& operator=(RpcRequest&&) noexcept = default;
            ~RpcRequest() = default;
        } rpc_request_;

        struct RpcResponse
        {
            std::string rpc_rsp_data_;
            RpcResponse() = default;
            RpcResponse(const RpcResponse&) = default;
            RpcResponse(RpcResponse&&) noexcept = default;
            RpcResponse& operator=(const RpcResponse&) = default;
            RpcResponse& operator=(RpcResponse&&) noexcept = default;
            ~RpcResponse() = default;
        } rpc_rsponse_;
        
        // 默认构造函数（C++17 允许 union 包含非 POD 类型）
        EventData() : rpc_request_{} {}
        ~EventData() {}  // 需要显式析构函数来处理 std::string
    } data_;
    
    // 提供默认构造函数
    ModuleEvent() : type_(EventType::ET_NONE), data_() {}
    
    // 复制构造函数
    ModuleEvent(const ModuleEvent& other) : type_(other.type_) {
        switch (type_) {
            case EventType::ET_RPC_REQUEST:
                new (&data_.rpc_request_) EventData::RpcRequest(other.data_.rpc_request_);
                break;
            case EventType::ET_RPC_RESPONSE:
                new (&data_.rpc_rsponse_) EventData::RpcResponse(other.data_.rpc_rsponse_);
                break;
            case EventType::ET_NONE:
            default:
                break;
        }
    }
    
    // 移动构造函数
    ModuleEvent(ModuleEvent&& other) noexcept : type_(other.type_) {
        switch (type_) {
            case EventType::ET_RPC_REQUEST:
                new (&data_.rpc_request_) EventData::RpcRequest(std::move(other.data_.rpc_request_));
                break;
            case EventType::ET_RPC_RESPONSE:
                new (&data_.rpc_rsponse_) EventData::RpcResponse(std::move(other.data_.rpc_rsponse_));
                break;
            case EventType::ET_NONE:
            default:
                break;
        }
        other.type_ = EventType::ET_NONE;
    }
    
    // 复制赋值运算符
    ModuleEvent& operator=(const ModuleEvent& other) {
        if (this != &other) {
            // 先销毁当前对象
            this->~ModuleEvent();
            // 构造新对象
            new (this) ModuleEvent(other);
        }
        return *this;
    }
    
    // 移动赋值运算符
    ModuleEvent& operator=(ModuleEvent&& other) noexcept {
        if (this != &other) {
            // 先销毁当前对象
            this->~ModuleEvent();
            // 构造新对象
            new (this) ModuleEvent(std::move(other));
        }
        return *this;
    }
    
    // 析构函数（需要显式处理 union 中的非 POD 类型）
    ~ModuleEvent() {
        switch (type_) {
            case EventType::ET_RPC_REQUEST:
                data_.rpc_request_.~RpcRequest();
                break;
            case EventType::ET_RPC_RESPONSE:
                data_.rpc_rsponse_.~RpcResponse();
                break;
            case EventType::ET_NONE:
            default:
                break;
        }
    }
};

} // namespace BaseNode