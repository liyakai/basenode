#include "service_discovery/zookeeper/zk_service_registry.h"

#include <utility>

namespace BaseNode::ServiceDiscovery::Zookeeper
{

namespace
{

} // namespace

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

    // 注册进程
    const auto host_port = paths_.BaseNodeRoot() + "/" + instance.host + ":" + std::to_string(instance.port);
    if(!zk_client_->EnsurePath(host_port))
    {
        BaseNodeLogError("[ZkServiceRegistry] EnsurePath host_port path failed. host_port:%s.", host_port.c_str());
        return false;
    }

    const auto module_path = host_port + "/" + instance.module_name;
    if (!zk_client_->EnsurePath(module_path))
    {
        BaseNodeLogError("[ZkServiceRegistry] EnsurePath module path failed. module_path:%s.", module_path.c_str());
        return false;
    }
    const auto service_path = module_path + "/" + instance.service_name;
    const auto service_data = instance.SerializeInstance();
    bool result = zk_client_->CreateEphemeral(service_path, service_data) || zk_client_->SetData(service_path, service_data);
    BaseNodeLogInfo("[ZkServiceRegistry] Register service to zk success. service_path:%s, service_data:%s.", service_path.c_str(), service_data.c_str());
    return result;
}

bool ZkServiceRegistry::DeRegisterService(const BaseNode::ServiceDiscovery::ServiceInstance &instance)
{
    if (!zk_client_)
    {
        return false;
    }
    // 使用与注册时相同的路径结构：/basenode/{host}:{port}/{module_name}/{service_name}
    const auto host_port = paths_.BaseNodeRoot() + "/" + instance.host + ":" + std::to_string(instance.port);
    const auto module_path = host_port + "/" + instance.module_name;
    const auto service_path = module_path + "/" + instance.service_name;
    bool result = zk_client_->Delete(service_path);
    BaseNodeLogInfo("[ZkServiceRegistry] DeRegisterService service from zk success. service_path:%s, result:%d.", service_path.c_str(), result);
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

} // namespace BaseNode::ServiceDiscovery::Zookeeper




