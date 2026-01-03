#include "service_discovery/zookeeper/zk_service_discovery.h"

#include <sstream>

namespace BaseNode::ServiceDiscovery::Zookeeper
{

InstanceList ZkServiceDiscovery::GetServiceInstances(const std::string &service_name)
{
    InstanceList result;
    if (!zk_client_)
    {
        return result;
    }

    const auto instances_root = paths_.ServiceInstancesPath(service_name);
    auto       children       = zk_client_->GetChildren(instances_root);
    result.reserve(children.size());

    for (const auto &child : children)
    {
        const auto inst_path = paths_.ServiceInstancePath(service_name, child);
        std::string data;
        if (!zk_client_->GetData(inst_path, data))
        {
            continue;
        }
        result.push_back(ParseServiceInstance(data));
    }
    return result;
}

void ZkServiceDiscovery::WatchServiceInstances(const std::string &service_name,
                               InstanceChangeCallback cb)
{
    if (!zk_client_ || !cb)
    {
        return;
    }
    const auto instances_root = paths_.ServiceInstancesPath(service_name);

    // 先立刻回调一次当前视图
    cb(service_name, GetServiceInstances(service_name));

    // 注册子节点变化监听
    zk_client_->WatchChildren(instances_root,
                              [this, service_name, cb](const std::string &/*path*/)
                              {
                                  cb(service_name, GetServiceInstances(service_name));
                              });
}

BaseNode::ServiceDiscovery::ServiceInstance
ZkServiceDiscovery::ParseServiceInstance(const std::string &data) const
{
    return BaseNode::ServiceDiscovery::ServiceInstance::ParseInstance(data);
}

} // namespace BaseNode::ServiceDiscovery::Zookeeper




