#pragma once
#include "module_event.h"
#include "utils/basenode_def_internal.h"
#include "tools/ringbuffer.h"
#include "coro_rpc/coro_rpc_server.h"
#include "coro_rpc/coro_rpc_client.h"
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
    ErrorCode Init();
    
    // 非虚函数，确保基类逻辑总是被执行
    // 子类不应该重写此方法，而是重写 DoUpdate()
    ErrorCode Update();

    // 非虚函数，确保基类逻辑总是被执行
    // 子类不应该重写此方法，而是重写 DoUninit()
    ErrorCode UnInit();

    ErrorCode PushModuleEvent(ModuleEvent&& module_event);

    ErrorCode SetServerSendCallback(std::function<void(uint64_t, std::string_view &&)>&& callback);
    ErrorCode SetClientSendCallback(std::function<void(std::string_view &&)>&& callback);

    std::vector<uint32_t> GetAllServiceHandlerKeys();
    
    /**
     * @brief 获取模块ID
     * @return 模块ID（基于类名的MD5哈希）
     */
    uint32_t GetModuleId() const;

    /**
     * @brief 注册RPC服务函数（非成员函数）
     * @tparam first 第一个函数指针
     * @tparam func 其他函数指针（可变参数）
     */
    template <auto first, auto... func>
    void RegisterService() {
        rpc_server_.template RegisterService<first, func...>();
    }

    /**
     * @brief 注册RPC服务函数（成员函数）
     * @tparam first 第一个成员函数指针
     * @tparam functions 其他成员函数指针（可变参数）
     * @param self 对象指针
     */
    template <auto first, auto... functions>
    void RegisterService(decltype(first) *self) {
        rpc_server_.template RegisterService<first, functions...>(self);
    }

    /**
     * @brief 调用RPC服务（使用默认5秒超时）
     * @tparam func RPC函数指针
     * @tparam Args 参数类型
     * @param args RPC函数参数
     * @return 返回协程Task，包含RPC调用结果
     */
    template <auto func, typename... Args>
    auto Call(Args &&...args) -> ToolBox::coro::Task<ToolBox::CoroRpc::async_rpc_result_value_t<std::invoke_result_t<decltype(func), Args...>>, ToolBox::coro::SharedLooperExecutor> {
        return rpc_client_.template Call<func>(std::forward<Args>(args)...);
    }

    /**
     * @brief 设置RPC请求的附件数据
     * @param attachment 附件数据（string_view）
     * @return 是否设置成功（如果附件数据过长会返回false）
     */
    bool SetReqAttachment(std::string_view attachment) {
        return rpc_client_.SetReqAttachment(attachment);
    }

protected:
    // 子类重写此方法来实现自己的初始化逻辑
    virtual ErrorCode DoInit() = 0;
    
    // 子类重写此方法来实现自己的更新逻辑
    virtual ErrorCode DoUpdate() = 0;

    virtual ErrorCode DoUninit() = 0;
    
private:
    void ProcessRingBufferData_();
    // 获取最终子类的类名
    std::string GetFinalClassName_() const;
    /**
     * @brief 注册模块到路由管理器
     * 在基类Init()中自动调用，子类无需关心
     * @return 错误码
     */
     ErrorCode RegisterToRouter_();

private:
    ToolBox::RingBufferSPSC<ModuleEvent, DEFAULT_MODULE_RING_BUFF_SIZE> recv_ring_buffer_; // 接收缓冲区
    ToolBox::CoroRpc::CoroRpcServer<ToolBox::CoroRpc::CoroRpcProtocol> rpc_server_; // RPC 服务器
    ToolBox::CoroRpc::CoroRpcClient<ToolBox::CoroRpc::CoroRpcProtocol> rpc_client_; // RPC 客户端
    
};


} // namespace BaseNode