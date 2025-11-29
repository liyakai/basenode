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
    void Init();
    
    // 非虚函数，确保基类逻辑总是被执行
    // 子类不应该重写此方法，而是重写 DoUpdate()
    void Update();
    virtual void UnInit() = 0;

    ErrorCode PushModuleEvent(ModuleEvent&& module_event);

    ErrorCode SetServerSendCallback(std::function<void(uint64_t, std::string_view &&)>&& callback);
    ErrorCode SetClientSendCallback(std::function<void(std::string_view &&)>&& callback);

    std::vector<uint32_t> GetAllServiceHandlerKeys();
    
    /**
     * @brief 获取模块ID
     * @return 模块ID（基于类名的MD5哈希）
     */
    uint32_t GetModuleId() const;

protected:
    // 子类重写此方法来实现自己的初始化逻辑
    virtual void DoInit() = 0;
    
    // 子类重写此方法来实现自己的更新逻辑
    virtual void DoUpdate() = 0;
    
private:
    void ProcessRingBufferData_();
    // 获取最终子类的类名
    std::string GetFinalClassName_() const;
    /**
     * @brief 注册模块到路由管理器
     * 在基类Init()中自动调用，子类无需关心
     * @return 是否注册成功
     */
     bool RegisterToRouter_();

private:
    ToolBox::RingBufferSPSC<ModuleEvent, DEFAULT_MODULE_RING_BUFF_SIZE> recv_ring_buffer_; // 接收缓冲区
    ToolBox::CoroRpc::CoroRpcServer<ToolBox::CoroRpc::CoroRpcProtocol> rpc_server_; // RPC 服务器
    ToolBox::CoroRpc::CoroRpcClient<ToolBox::CoroRpc::CoroRpcProtocol> rpc_client_; // RPC 客户端
    
};


} // namespace BaseNode