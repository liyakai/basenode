#include "service_discovery/zookeeper/zk_service_registry.h"

#include <utility>

namespace BaseNode::ServiceDiscovery::Zookeeper
{

namespace
{
// 一个非常简单的序列化，将 ServiceInstance 序列化为 "host:port;key1=val1;key2=val2" 形式
inline std::string SerializeInstance(const BaseNode::ServiceDiscovery::ServiceInstance &inst)
{
    std::string data = inst.host + ":" + std::to_string(inst.port);
    for (const auto &kv : inst.metadata)
    {
        data.append(";");
        data.append(kv.first).append("=").append(kv.second);
    }
    return data;
}
} // namespace

bool ZkServiceRegistry::Register(const BaseNode::ServiceDiscovery::ServiceInstance &instance)
{
    if (!zk_client_)
    {
        return false;
    }
    const auto instances_root = paths_.ServiceInstancesPath(instance.service_name);
    zk_client_->EnsurePath(paths_.ServicePath(instance.service_name));
    zk_client_->EnsurePath(instances_root);

    const auto inst_path = paths_.ServiceInstancePath(instance.service_name, instance.instance_id);
    const auto data      = SerializeInstance(instance);
    return zk_client_->CreateEphemeral(inst_path, data);
}

bool ZkServiceRegistry::Deregister(const BaseNode::ServiceDiscovery::ServiceInstance &instance)
{
    if (!zk_client_)
    {
        return false;
    }
    const auto inst_path = paths_.ServiceInstancePath(instance.service_name, instance.instance_id);
    zk_client_->Delete(inst_path);
    return true;
}

bool ZkServiceRegistry::Renew(const BaseNode::ServiceDiscovery::ServiceInstance &instance)
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
    const auto data = SerializeInstance(instance);
    return zk_client_->SetData(inst_path, data);
}

} // namespace BaseNode::ServiceDiscovery::Zookeeper




