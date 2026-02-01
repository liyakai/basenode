#include "service_discovery/zookeeper/zk_service_registry.h"

#include <utility>
#include <algorithm>

namespace BaseNode::ServiceDiscovery::Zookeeper
{

namespace
{

} // namespace

bool ZkServiceRegistry::Init()
{
    if (!zk_client_)
    {
        BaseNodeLogError("Invalid zk_client_");
        return false;
    }
    BaseNodeLogInfo("Ready to EnsurePath in zookeeper. root:%s, ProcessesRoot:%s, ServicesRoot:%s."
                    , paths_.root.c_str()
                    , paths_.ProcessesRoot().c_str()
                    , paths_.ServicesRoot().c_str()
                );
    
    // 注册会话状态回调，在会话断开时清理节点
    zk_client_->WatchSessionState([this](bool connected) {
        if (!connected)
        {
            BaseNodeLogWarn("[ZkServiceRegistry] Session disconnected, cleaning up tracked nodes");
            CleanupSessionNodes();
        }
    });
    
    return true;
}

bool ZkServiceRegistry::RegistService(const BaseNode::ServiceDiscovery::ServiceInstance &instance)
{
    if (!zk_client_)
    {
        BaseNodeLogError("[ZkServiceRegistry] Invalid zk_client_");
        return false;
    }
    if(!zk_client_->EnsurePath(paths_.BaseNodeRoot()))
    {
        BaseNodeLogError("[ZkServiceRegistry] EnsurePath base node root path failed. base node root path:%s.", paths_.BaseNodeRoot().c_str());
        return false;
    }

    if(instance.host.empty() || instance.port == 0)
    {
        BaseNodeLogError("[ZkServiceRegistry] Invalid instance. host:%s, port:%d.", instance.host.c_str(), instance.port);
        return false;
    }

    // 注册进程
    const auto host_port = paths_.ServicesRoot() + "/" + instance.host + ":" + std::to_string(instance.port);
    if(!zk_client_->EnsurePath(host_port))
    {
        BaseNodeLogError("[ZkServiceRegistry] EnsurePath host_port path failed. host_port:%s.", host_port.c_str());
        return false;
    }

    // 跟踪创建的IP:Port节点
    {
        std::lock_guard<std::mutex> lock(tracked_nodes_mutex_);
        tracked_host_port_nodes_.insert(host_port);
    }

    const auto module_path = host_port + "/" + instance.module_name;
    if (!zk_client_->EnsurePath(module_path))
    {
        BaseNodeLogError("[ZkServiceRegistry] EnsurePath module path failed. module_path:%s.", module_path.c_str());
        return false;
    }

    // 跟踪创建的模块节点
    {
        std::lock_guard<std::mutex> lock(tracked_nodes_mutex_);
        tracked_module_nodes_.insert(module_path);
    }
    if(!instance.service_name.empty())
    {
        const auto service_path = module_path + "/" + instance.service_name;
        const auto service_data = instance.SerializeInstance();
        bool ok = zk_client_->CreateEphemeral(service_path, service_data) || zk_client_->SetData(service_path, service_data);
        BaseNodeLogInfo("[ZkServiceRegistry] Register service to zk success. service_path:%s, service_data:%s.", service_path.c_str(), service_data.c_str());
        return ok;
    }
    // 无 service_name 时仅创建模块路径（无 RPC 的模块如 ZkServiceDiscoveryModule），视为成功
    BaseNodeLogInfo("[ZkServiceRegistry] Register service to zk success. module_path:%s, instance:%s", module_path.c_str(), instance.SerializeInstance().c_str());
    return true;
}

bool ZkServiceRegistry::DeRegisterService(const BaseNode::ServiceDiscovery::ServiceInstance &instance)
{
    if (!zk_client_)
    {
        return false;
    }
    bool result = true;
    // 使用与注册时相同的路径结构：/basenode/services/{host}:{port}/{module_name}/{service_name}
    const auto host_port = paths_.ServicesRoot() + "/" + instance.host + ":" + std::to_string(instance.port);
    const auto module_path = host_port + "/" + instance.module_name;
    if(!instance.service_name.empty())
    {
        const auto service_path = module_path + "/" + instance.service_name;
        result = result && zk_client_->Delete(service_path);
        BaseNodeLogInfo("[ZkServiceRegistry] DeRegisterService service from zk success. service_path:%s, result:%d.", service_path.c_str(), result);
    }
    result = result && zk_client_->Delete(module_path);
    BaseNodeLogInfo("[ZkServiceRegistry] DeRegisterService module from zk success. module_path:%s, result:%d.", module_path.c_str(), result);
    result = result && zk_client_->Delete(host_port);
    BaseNodeLogInfo("[ZkServiceRegistry] DeRegisterService host_port from zk success. host_port:%s, result:%d.", host_port.c_str(), result);
    return result;
}

bool ZkServiceRegistry::RenewService(const BaseNode::ServiceDiscovery::ServiceInstance &instance)
{
    if (!zk_client_)
    {
        return false;
    }
    const auto inst_path = paths_.ServiceInstancePath(instance.service_name, instance.instance_id);
    std::string dummy;
    if (!zk_client_->GetData(inst_path, dummy))
    {
        return false;
    }
    // 简单实现：更新数据，相当于“心跳”信息
    const auto data = instance.SerializeInstance();
    return zk_client_->SetData(inst_path, data);
}

void ZkServiceRegistry::CleanupOrphanNodes(const std::string &base_path)
{
    if (!zk_client_)
    {
        return;
    }

    const std::string root_path = base_path.empty() ? paths_.ServicesRoot() : base_path;
    
    // 获取所有IP:Port节点
    auto host_port_children = zk_client_->GetChildren(root_path);
    
    for (const auto &host_port : host_port_children)
    {
        const std::string host_port_path = root_path + "/" + host_port;
        
        // 获取该IP:Port下的所有模块节点
        auto module_children = zk_client_->GetChildren(host_port_path);
        
        // 检查每个模块节点是否有子节点（临时节点）
        for (const auto &module : module_children)
        {
            const std::string module_path = host_port_path + "/" + module;
            auto service_children = zk_client_->GetChildren(module_path);
            
            // 如果模块节点下没有任何子节点，删除该模块节点
            if (service_children.empty())
            {
                BaseNodeLogInfo("[ZkServiceRegistry] Cleaning up empty module node: %s", module_path.c_str());
                zk_client_->Delete(module_path);
            }
        }
        
        // 如果IP:Port节点下没有任何模块节点，删除该IP:Port节点
        auto remaining_modules = zk_client_->GetChildren(host_port_path);
        if (remaining_modules.empty())
        {
            BaseNodeLogInfo("[ZkServiceRegistry] Cleaning up empty host_port node: %s", host_port_path.c_str());
            zk_client_->Delete(host_port_path);
        }
    }
}

void ZkServiceRegistry::CleanupSessionNodes()
{
    if (!zk_client_)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(tracked_nodes_mutex_);
    
    // 清理所有跟踪的模块节点（从下往上清理）
    for (const auto &module_path : tracked_module_nodes_)
    {
        // 检查模块节点是否还有子节点
        auto children = zk_client_->GetChildren(module_path);
        if (children.empty())
        {
            BaseNodeLogInfo("[ZkServiceRegistry] Cleaning up tracked module node: %s", module_path.c_str());
            zk_client_->Delete(module_path);
        }
    }
    tracked_module_nodes_.clear();
    
    // 清理所有跟踪的IP:Port节点
    for (const auto &host_port_path : tracked_host_port_nodes_)
    {
        // 检查IP:Port节点是否还有子节点
        auto children = zk_client_->GetChildren(host_port_path);
        if (children.empty())
        {
            BaseNodeLogInfo("[ZkServiceRegistry] Cleaning up tracked host_port node: %s", host_port_path.c_str());
            zk_client_->Delete(host_port_path);
        }
    }
    tracked_host_port_nodes_.clear();
}

bool ZkServiceRegistry::RecursiveCleanupEmptyNode(const std::string &path)
{
    if (!zk_client_)
    {
        return false;
    }

    // 获取子节点
    auto children = zk_client_->GetChildren(path);
    
    // 如果有子节点，递归清理子节点
    for (const auto &child : children)
    {
        const std::string child_path = path + "/" + child;
        RecursiveCleanupEmptyNode(child_path);
    }
    
    // 再次检查是否还有子节点（子节点可能已被删除）
    children = zk_client_->GetChildren(path);
    if (children.empty())
    {
        BaseNodeLogInfo("[ZkServiceRegistry] Deleting empty node: %s", path.c_str());
        return zk_client_->Delete(path);
    }
    
    return false;
}

} // namespace BaseNode::ServiceDiscovery::Zookeeper




