#include "network.h"
#include "module/module_router.h"
#include "utils/basenode_def_internal.h"
#include "config/config_manager.h"
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

ErrorCode Network::DoInit()
{
    BaseNodeLogInfo("Network Init");
    
    // 创建第三方网络库实例
    // 注意：pthread 和 C++ 运行时已经在共享库加载时通过 __attribute__((constructor)) 初始化
    network_impl_ = new ToolBox::Network();
    
    // 从配置读取 worker_threads，默认值为 1
    int worker_threads = 1;
    std::string listen_ip = "0.0.0.0";
    uint16_t listen_port = 9527;
    std::vector<std::string> loaded_configs = ConfigMgr->GetLoadedConfigNames();
    if (!loaded_configs.empty()) {
        std::string config_name = loaded_configs[0];
        std::string worker_threads_path = config_name + ".network.worker_threads";
        worker_threads = ConfigMgr->Get<int>(config_name, worker_threads_path, 1);
        BaseNodeLogInfo("[Network] Loaded worker_threads from config '%s': %d", config_name.c_str(), worker_threads);
        
        // 从配置读取监听参数
        std::string listen_ip_path = config_name + ".network.listen.ip";
        std::string listen_port_path = config_name + ".network.listen.port";
        listen_ip = ConfigMgr->Get<std::string>(config_name, listen_ip_path, "0.0.0.0");
        listen_port = static_cast<uint16_t>(ConfigMgr->Get<int>(config_name, listen_port_path, 9527));
        BaseNodeLogInfo("[Network] Loaded listen config from '%s': %s:%d", config_name.c_str(), listen_ip.c_str(), listen_port);
    } else {
        BaseNodeLogWarn("[Network] No config name in ConfigManager (GetLoadedConfigNames empty), using default worker_threads: %d, listen: %s:%d", worker_threads, listen_ip.c_str(), listen_port);
    }

    // 设置监听成功回调
    network_impl_->SetOnBinded([this](ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id, const std::string& ip, uint16_t port) {
        BaseNodeLogInfo("[Network] Listen binded successfully, type=%d, opaque=%lu, conn_id=%lu, ip=%s, port=%d", 
                        type, opaque, conn_id, ip.c_str(), port);
    });

    // 监听端口（在启动网络库之前设置）
    network_impl_->Accept(ToolBox::NT_TCP, 0, listen_ip, listen_port);
    BaseNodeLogInfo("[Network] Accept called: %s:%d", listen_ip.c_str(), listen_port);
    
    // 启动网络库
    if (!network_impl_->Start(worker_threads))
    {
        BaseNodeLogError("Network Start failed");
        return ErrorCode::BN_NETWORK_START_FAILED;
    }

    ErrorCode err = ModuleRouterMgr->RegisterModule(this, true);
    if (err != ErrorCode::BN_SUCCESS) {
        BaseNodeLogError("[Network] Failed to register network module to router, error: %d", err);
        return err;
    }
    
    // 设置网络接收回调，使用ModuleRouter路由RPC数据包
    // RouterModule 会主动连接业务进程，接收来自 RouterModule 的请求
    network_impl_->SetOnReceived([this](ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id, const char* data, size_t size) {
        // 创建 std::string 并移动传递，避免拷贝
        std::string data_str(data, size);
        ErrorCode err = ModuleRouterMgr->RouteProtocolPacket(std::move(data_str));
        if (err != ErrorCode::BN_SUCCESS) {
            BaseNodeLogWarn("[Network] Failed to route protocol packet, error: %d, size: %zu", static_cast<int>(err), size);
        }
    });

    // 设置连接接受回调（RouterModule 主动连接时会触发）
    network_impl_->SetOnAccepted([this](ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id) {
        BaseNodeLogInfo("[Network] RouterModule connected, conn_id=%lu", conn_id);
    });

    SetClientSendCallback([this](std::string &&){

    });

    SetServerSendCallback([this](uint64_t, std::string &&){

    });
    
    BaseNodeLogInfo("Network Init success");
    return ErrorCode::BN_SUCCESS;
}

ErrorCode Network::DoUpdate()
{
    // 驱动网络库主线程事件处理
    if (network_impl_)
    {
        network_impl_->Update();
    }
    return ErrorCode::BN_SUCCESS;
}

ErrorCode Network::DoUninit()
{
    BaseNodeLogInfo("Network DoUninit");
    
    if (network_impl_)
    {
        // 停止并等待工作线程结束
        network_impl_->StopWait();
        
        delete network_impl_;
        network_impl_ = nullptr;
    }
    
    BaseNodeLogInfo("Network UnInit success");
    return ErrorCode::BN_SUCCESS;
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