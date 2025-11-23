#include "network.h"
#include "module/module_router.h"
#include "utils/basenode_def_internal.h"
#include <pthread.h>

namespace BaseNode
{
Network::Network()
    : network_impl_(nullptr)
{
}

Network::~Network()
{
    if (network_impl_)
    {
        delete network_impl_;
        network_impl_ = nullptr;
    }
}

void Network::DoInit()
{
    BaseNodeLogInfo("Network Init");
    
    // 创建第三方网络库实例
    // 注意：pthread 和 C++ 运行时已经在共享库加载时通过 __attribute__((constructor)) 初始化
    network_impl_ = new ToolBox::Network();
    
    // 启动网络库（默认1个工作线程）
    if (!network_impl_->Start(1))
    {
        BaseNodeLogError("Network Start failed");
        return;
    }
    
    // 设置网络接收回调，使用ModuleRouter路由RPC数据包
    network_impl_->SetOnReceived([this](ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id, const char* data, size_t size) {
        // 将接收到的数据路由到对应的模块
        std::string_view data_view(data, size);
        if (!ModuleRouterMgr->RouteProtocolPacket(data_view)) {
            BaseNodeLogWarn("[Network] Failed to route protocol packet, size: %zu", size);
        }
    });
    
    BaseNodeLogInfo("Network Init success");
}

void Network::DoUpdate()
{
    // 驱动网络库主线程事件处理
    if (network_impl_)
    {
        network_impl_->Update();
    }
}

void Network::UnInit()
{
    BaseNodeLogInfo("Network UnInit");
    
    if (network_impl_)
    {
        // 停止并等待工作线程结束
        network_impl_->StopWait();
        
        delete network_impl_;
        network_impl_ = nullptr;
    }
    
    BaseNodeLogInfo("Network UnInit success");
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_INIT() {
    NetworkMgr->Init();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UPDATE() {
    NetworkMgr->Update();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UNINIT() {
    NetworkMgr->UnInit();
}

} // namespace BaseNode