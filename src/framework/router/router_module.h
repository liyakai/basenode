#pragma once

#include "module_interface.h"
#include "module/module_zk.h"
#include "service_discovery/service_discovery_core.h"
#include "utils/basenode_def_internal.h"
#include "network/network_api.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <utility>

namespace BaseNode
{

/**
 * @brief 中心路由模块
 * 
 * 作为独立的进程运行，负责：
 * 1. 通过 Zookeeper 服务发现获取所有服务实例
 * 2. 主动连接所有业务进程
 * 3. 维护 service_id -> conn_id 的路由表
 * 4. 在不同进程间转发 RPC 请求/响应
 */
class RouterModule : public IModule
{
public:
    RouterModule();
    virtual ~RouterModule();

protected:
    virtual ErrorCode DoInit() override;
    virtual ErrorCode DoUpdate() override;
    virtual ErrorCode DoUninit() override;
    virtual ErrorCode DoAfterAllModulesInit() override;

private:
    /**
     * @brief 处理主动连接成功
     */
    void OnConnected(ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id);

    /**
     * @brief 处理主动连接失败
     */
    void OnConnectFailed(ToolBox::NetworkType type, uint64_t opaque, 
                         ToolBox::ENetErrCode err_code, int32_t err_no);

    /**
     * @brief 处理连接关闭
     */
    void OnClose(ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id,
                ToolBox::ENetErrCode net_err, int32_t sys_err);

    /**
     * @brief 处理来自业务进程的数据
     */
    void OnReceived(ToolBox::NetworkType type, uint64_t opaque, uint64_t conn_id,
                   const char* data, size_t size);

    /**
     * @brief 处理服务实例变化
     */
    void OnServiceInstancesChanged(const std::string& service_name,
                                const ServiceDiscovery::InstanceList& instances);

    /**
     * @brief 从服务实例连接或更新连接
     */
    void ConnectToInstance(const ServiceDiscovery::ServiceInstance& instance);

    /**
     * @brief 断开到实例的连接
     */
    void DisconnectFromInstance(const ServiceDiscovery::ServiceInstance& instance);

    /**
     * @brief 从 RPC 数据中提取服务ID和客户端ID
     */
    std::tuple<uint32_t, uint64_t> ExtractServiceIdClientId(std::string_view rpc_data);

    /**
     * @brief 路由 RPC 请求到目标进程
     */
    ErrorCode RouteRpcRequest(uint32_t service_id, uint64_t client_id, uint64_t source_conn_id, std::string&& rpc_data);

    /**
     * @brief 路由 RPC 响应到源进程
     * @param target_module_id 目标模块ID（响应要发送到的模块）
     * @param response_conn_id 响应来自的连接（目标进程的连接）
     * @param rpc_data RPC 响应数据
     */
    ErrorCode RouteRpcResponse(uint64_t target_module_id, uint64_t response_conn_id, std::string&& rpc_data);

    /**
     * @brief 发现所有服务并建立连接
     */
    void DiscoverAndConnectAllServices();

    /**
     * @brief 监听服务目录变化（发现新服务）
     */
    void OnServicesDirectoryChanged(const std::string& path);

    /**
     * @brief 获取实例
     **/
     uint64_t GetConnectionIDbyIPPort(const std::string& ip, uint16_t port);

    /**
     * @brief 获取实例
     **/
    bool CheckIPPortExist(const std::string& ip, uint16_t port);

    /**
     * @brief 设置连接ID
     * @param ip 地址
     * @param port 端口
     * @param connection_id 连接ID
     * @return 设置了多少个实例的连接ID
    */
    int SetConnectionID(const std::string& ip, uint16_t port, uint64_t connection_id);

    std::vector<uint64_t> GetInstanceIDsByConnectionID(uint64_t connection_id);

private:
    ToolBox::Network* network_impl_ = nullptr;

    // 服务ID -> 连接ID 的映射（路由表）
    std::unordered_map<uint32_t, uint64_t> service_to_conn_;

    // 服务ID -> instance 的映射
    std::unordered_map<uint64_t, ServiceDiscovery::ServiceInstance> key_to_instance_;

    // 待连接：opaque -> (host, port)，同一 host:port 只建立一条连接，OnConnected 时对该地址下所有实例设置 conn_id
    std::unordered_map<uint64_t, std::pair<std::string, uint16_t>> pending_connections_;
    std::atomic<uint64_t> next_opaque_{1};

    // 已监听的服务名集合
    std::unordered_set<std::string> watched_services_;
    std::mutex watched_services_mutex_;

    bool initialized_ = false;
};

#define RouterModuleMgr ToolBox::Singleton<RouterModule>::Instance()

} // namespace BaseNode
