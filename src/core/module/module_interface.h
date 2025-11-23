#pragma once
#include "module_event.h"
#include "utils/basenode_def_internal.h"
#include "tools/ringbuffer.h"
#include "coro_rpc/coro_rpc_server.h"
#include "module_router.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <typeinfo>

// 前向声明
namespace BaseNode {
    class ModuleRouter;
}

namespace BaseNode
{

#define DEFAULT_MODULE_RING_BUFF_SIZE 256 * 1024

class IModule
{
public:
    virtual ~IModule() = default;
    
    // 非虚函数，确保基类逻辑总是被执行
    // 子类不应该重写此方法，而是重写 DoInit()
    void Init() {
        // 先注册模块到路由管理器
        if (!RegisterToRouter()) {
            BaseNodeLogError("[module] Failed to register module (id: %u) to router", GetModuleId());
        }
        DoInit();  // 然后调用子类的初始化逻辑
    }
    
    // 非虚函数，确保基类逻辑总是被执行
    // 子类不应该重写此方法，而是重写 DoUpdate()
    void Update() {
        ProcessRingBufferData_();  // 先处理环形缓冲区数据
        DoUpdate();                  // 然后调用子类的更新逻辑
    }
    
    virtual void UnInit() = 0;

    ErrorCode PushModuleEvent(ModuleEvent&& module_event)
    {
        if (recv_ring_buffer_.Full()) {
            ProcessRingBufferData_();
            if (recv_ring_buffer_.Full()) {
                return ErrorCode::BN_RECV_BUFF_OVERFLOW;
            }
        }
        recv_ring_buffer_.Push(std::move(module_event));
        return ErrorCode::BN_SUCCESS;
    }

    ErrorCode SetSendCallback(std::function<void(uint64_t, std::string_view &&)>&& callback)
    {
        ToolBox::CoroRpc::Errc errc = rpc_server_.SetSendCallback(std::move(callback));
        if (errc != ToolBox::CoroRpc::Errc::SUCCESS) {
            BaseNodeLogError("[module] SetSendCallback failed, errc: %d", errc);
            return ErrorCode::BN_SET_SEND_CALLBACK_FAILED;
        }
        return ErrorCode::BN_SUCCESS;
    }

    std::vector<uint32_t> GetAllServiceHandlerKeys()
    {
        return rpc_server_.GetAllServiceHandlerKeys();
    }
    
    /**
     * @brief 获取模块ID
     * @return 模块ID（基于类名的MD5哈希）
     */
    uint32_t GetModuleId() const
    {
        return MD5Hash32Constexpr(this->GetFinalClassName_());
    }

protected:
    // 子类重写此方法来实现自己的初始化逻辑
    virtual void DoInit() = 0;
    
    // 子类重写此方法来实现自己的更新逻辑
    virtual void DoUpdate() = 0;
    
private:
    void ProcessRingBufferData_()
    {
        while (!recv_ring_buffer_.Empty())
        {
            // char* data = recv_ring_buffer_.GetReadPtr();
            const ModuleEvent& event = recv_ring_buffer_.Pop();
            switch (event.type_)
            {
            case ModuleEvent::EventType::ET_RPC_REQUEST:
                rpc_server_.OnRecvReq(0, event.data_.rpc_request_.rpc_req_data_);
                break;
            default:
                break;
            }
            return;
        }
    }
    // 获取最终子类的类名
    std::string GetFinalClassName_() const
    {
        const char* name = typeid(*this).name();
        return name;
    }
private:
    ToolBox::RingBufferSPSC<ModuleEvent, DEFAULT_MODULE_RING_BUFF_SIZE> recv_ring_buffer_; // 接收缓冲区
    ToolBox::CoroRpc::CoroRpcServer<ToolBox::CoroRpc::CoroRpcProtocol> rpc_server_; // RPC 服务器
    
    /**
     * @brief 注册模块到路由管理器
     * 在基类Init()中自动调用，子类无需关心
     * @return 是否注册成功
     */
    bool RegisterToRouter()
    {
        return ModuleRouterMgr->RegisterModule(this);
    }
};

} // namespace BaseNode