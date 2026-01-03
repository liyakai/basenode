#pragma once

// 顶层 Service Discovery 抽象与默认实现集合：
//  - ServiceInstance        : 服务实例领域模型
//  - IServiceRegistry       : 注册接口
//  - IServiceDiscovery      : 发现接口
//  - RequestContext / ILoadBalancer / ZoneAwareLoadBalancer
//  - IDiscoveryClient / DefaultDiscoveryClient

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>

namespace BaseNode::ServiceDiscovery
{

// ------------------------- ServiceInstance -------------------------

struct ServiceInstance
{
    std::string service_name;
    std::string module_name;
    std::string instance_id;
    std::string host;
    uint16_t    port{0};
    bool        healthy{true};
    std::unordered_map<std::string, std::string> metadata;


    // 一个非常简单的序列化，将 ServiceInstance 序列化为 "host:port;key1=val1;key2=val2" 形式
    std::string SerializeInstance() const
    {
        std::string data = host + ":" + std::to_string(port);
        data.append(";module_name:").append(module_name);
        data.append(";instance_id:").append(instance_id);
        data.append(";healthy:").append(healthy ? "true" : "false");
        for (const auto &kv : metadata)
        {
            data.append(";");
            data.append(kv.first).append("=").append(kv.second);
        }
        return data;
    }
    static ServiceInstance ParseInstance(const std::string &data)
    {
        ServiceInstance instance;
        auto colon_pos = data.find(':');
        if (colon_pos != std::string::npos)
        {
            instance.host = data.substr(0, colon_pos);
            instance.port = static_cast<uint16_t>(std::stoi(data.substr(colon_pos + 1)));
        }
        auto semicolon_pos = data.find(';');
        if (semicolon_pos != std::string::npos)
        {
            instance.module_name = data.substr(semicolon_pos + 1);
        }
        auto equal_pos = instance.module_name.find('=');
        if (equal_pos != std::string::npos)
        {
            instance.instance_id = instance.module_name.substr(equal_pos + 1);
        }
        return instance;
    }

};


// ------------------------- Registry 接口 ---------------------------

class IServiceRegistry
{
public:
    virtual ~IServiceRegistry() = default;

    virtual bool RegistService(const ServiceInstance &instance)   = 0;
    virtual bool DeRegisterService(const ServiceInstance &instance) = 0;
    virtual bool RenewService(const ServiceInstance &instance)      = 0;
};

// ------------------------- Discovery 接口 --------------------------

using InstanceList = std::vector<ServiceInstance>;

using InstanceChangeCallback = std::function<void(const std::string &service_name,
                                                  const InstanceList &instances)>;

class IServiceDiscovery
{
public:
    virtual ~IServiceDiscovery() = default;

    virtual InstanceList GetServiceInstances(const std::string &service_name) = 0;
    virtual void WatchServiceInstances(const std::string &service_name,
                       InstanceChangeCallback cb) = 0;
};

using IServiceDiscoveryPtr = std::shared_ptr<IServiceDiscovery>;


} // namespace BaseNode::ServiceDiscovery



