#include "service_discovery/zookeeper/zk_service_discovery.h"

#include <sstream>

namespace BaseNode::ServiceDiscovery::Zookeeper
{

InstanceList ZkServiceDiscovery::GetInstances(const std::string &service_name)
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
        const auto inst_path = instances_root + "/" + child;
        std::string data;
        if (!zk_client_->GetData(inst_path, data))
        {
            continue;
        }
        result.push_back(ParseInstance(service_name, child, data));
    }
    return result;
}

void ZkServiceDiscovery::Watch(const std::string &service_name,
                               InstanceChangeCallback cb)
{
    if (!zk_client_ || !cb)
    {
        return;
    }
    const auto instances_root = paths_.ServiceInstancesPath(service_name);

    // 先立刻回调一次当前视图
    cb(service_name, GetInstances(service_name));

    // 注册子节点变化监听
    zk_client_->WatchChildren(instances_root,
                              [this, service_name, cb](const std::string &/*path*/)
                              {
                                  cb(service_name, GetInstances(service_name));
                              });
}

BaseNode::ServiceDiscovery::ServiceInstance
ZkServiceDiscovery::ParseInstance(const std::string &service_name,
                                  const std::string &instance_id,
                                  const std::string &data) const
{
    BaseNode::ServiceDiscovery::ServiceInstance inst;
    inst.service_name = service_name;
    inst.instance_id  = instance_id;
    inst.healthy      = true;

    // data 约定为 "host:port;key1=val1;key2=val2" 简单格式
    std::string host_port;
    std::string kv_pairs;

    auto pos = data.find(';');
    if (pos == std::string::npos)
    {
        host_port = data;
    }
    else
    {
        host_port = data.substr(0, pos);
        kv_pairs  = data.substr(pos + 1);
    }

    auto colon_pos = host_port.find(':');
    if (colon_pos != std::string::npos)
    {
        inst.host = host_port.substr(0, colon_pos);
        inst.port = static_cast<uint16_t>(std::stoi(host_port.substr(colon_pos + 1)));
    }
    else
    {
        inst.host = host_port;
    }

    // 解析 key=value;key2=value2
    std::istringstream ss(kv_pairs);
    std::string        token;
    while (std::getline(ss, token, ';'))
    {
        auto eq_pos = token.find('=');
        if (eq_pos == std::string::npos)
        {
            continue;
        }
        auto key   = token.substr(0, eq_pos);
        auto value = token.substr(eq_pos + 1);
        if (!key.empty())
        {
            inst.metadata.emplace(std::move(key), std::move(value));
        }
    }
    return inst;
}

} // namespace BaseNode::ServiceDiscovery::Zookeeper




