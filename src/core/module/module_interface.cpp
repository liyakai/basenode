#include "module_interface.h"
#include "module_event.h"
#include "utils/basenode_def_internal.h"

namespace BaseNode
{
    ErrorCode IModule::Init() {
        // 先注册模块到路由管理器
        ErrorCode err = RegisterToRouter_();
        if (err != ErrorCode::BN_SUCCESS) {
            BaseNodeLogError("[module] Failed to register module (id: %u) to router, error: %d", GetModuleId(), err);
            return err;
        }
        DoInit();  // 然后调用子类的初始化逻辑
        return ErrorCode::BN_SUCCESS;
    }

    ErrorCode IModule::Update() {
        ProcessRingBufferData_();  // 先处理环形缓冲区数据
        DoUpdate();                  // 然后调用子类的更新逻辑
        return ErrorCode::BN_SUCCESS;
    }

    ErrorCode IModule::UnInit() {
        ErrorCode err = DoUninit();
        if (err != ErrorCode::BN_SUCCESS) {
            BaseNodeLogError("[module] UnInit failed, error: %d", err);
            return err;
        }
        err = ModuleRouterMgr->UnregisterModule(this);
        if (err != ErrorCode::BN_SUCCESS) {
            BaseNodeLogError("[module] UnregisterModule failed");
            return err;
        }
        return ErrorCode::BN_SUCCESS;
    }

    ErrorCode IModule::PushModuleEvent(ModuleEvent&& module_event)
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

    ErrorCode IModule::SetServerSendCallback(std::function<void(uint64_t, std::string_view &&)>&& callback)
    {
        ToolBox::CoroRpc::Errc errc = rpc_server_.SetSendCallback(std::move(callback));
        if (errc != ToolBox::CoroRpc::Errc::SUCCESS) {
            BaseNodeLogError("[module] SetSendCallback failed, errc: %d", errc);
            return ErrorCode::BN_SET_SEND_CALLBACK_FAILED;
        }
        return ErrorCode::BN_SUCCESS;
    }

    ErrorCode IModule::SetClientSendCallback(std::function<void(std::string_view &&)>&& callback)
    {
        rpc_client_.SetSendCallback(std::move(callback));
        return ErrorCode::BN_SUCCESS;
    }

    std::vector<uint32_t> IModule::GetAllServiceHandlerKeys()
    {
        return rpc_server_.GetAllServiceHandlerKeys();
    }

    uint32_t IModule::GetModuleId() const
    {
        return MD5Hash32Constexpr(GetFinalClassName_());
    }

    void IModule::ProcessRingBufferData_()
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
            case ModuleEvent::EventType::ET_RPC_RESPONSE:
                rpc_client_.OnRecvResp(event.data_.rpc_rsponse_.rpc_rsp_data_);
            default:
                BaseNodeLogError("[module] invalid event type:%d", event.type_);
                break;
            }
            return;
        }
    }
    // 获取最终子类的类名
    std::string IModule::GetFinalClassName_() const
    {
        const char* name = typeid(*this).name();
        return name;
    }

    ErrorCode IModule::RegisterToRouter_()
    {
        return ModuleRouterMgr->RegisterModule(this);
    }

} // namespace BaseNode