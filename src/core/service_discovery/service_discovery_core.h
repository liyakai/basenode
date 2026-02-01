#pragma once

// 顶层 Service Discovery 抽象与默认实现集合：
//  - ServiceInstance        : 服务实例领域模型
//  - IServiceRegistry       : 注册接口
//  - IServiceDiscovery      : 发现接口

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
    uint64_t instance_id;
    std::string host;
    uint16_t    port{0};
    bool        healthy{true};
    uint64_t    connection_id{0};
    std::unordered_map<std::string, std::string> metadata;

    ServiceInstance()
    {
        service_name = "";
        module_name = "";
        instance_id = 0;
        host = "";
        port = 0;
        healthy = true;
        connection_id = 0;
        metadata.clear();
    }


    // 一个非常简单的序列化，将 ServiceInstance 序列化为 "host:port;key1=val1;key2=val2" 形式
    std::string SerializeInstance() const
    {
        std::string data = host + ":" + std::to_string(port);
        data.append(";module_name:").append(module_name);
        data.append(";service_name:").append(service_name);
        data.append(";instance_id:").append(std::to_string(instance_id));
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
        
        // 解析 host:port (第一个分号之前的部分)
        auto first_semicolon = data.find(';');
        std::string host_port = (first_semicolon != std::string::npos) 
                                ? data.substr(0, first_semicolon) 
                                : data;
        
        auto colon_pos = host_port.find(':');
        if (colon_pos != std::string::npos)
        {
            instance.host = host_port.substr(0, colon_pos);
            instance.port = static_cast<uint16_t>(std::stoi(host_port.substr(colon_pos + 1)));
        }
        
        // 解析分号分隔的键值对
        if (first_semicolon != std::string::npos)
        {
            size_t pos = first_semicolon + 1;
            while (pos < data.length())
            {
                auto next_semicolon = data.find(';', pos);
                std::string kv_pair = (next_semicolon != std::string::npos)
                                      ? data.substr(pos, next_semicolon - pos)
                                      : data.substr(pos);
                
                // 尝试用冒号分隔 (module_name, instance_id, healthy)
                auto kv_colon = kv_pair.find(':');
                if (kv_colon != std::string::npos)
                {
                    std::string key = kv_pair.substr(0, kv_colon);
                    std::string value = kv_pair.substr(kv_colon + 1);
                    
                    if (key == "module_name")
                    {
                        instance.module_name = value;
                    }
                    else if (key == "service_name")
                    {
                        instance.service_name = value;
                    }
                    else if (key == "instance_id")
                    {
                        instance.instance_id = static_cast<uint64_t>(std::stoul(value));
                    }
                    else if (key == "healthy")
                    {
                        instance.healthy = (value == "true");
                    }
                }
                else
                {
                    // 尝试用等号分隔 (metadata)
                    auto kv_equal = kv_pair.find('=');
                    if (kv_equal != std::string::npos)
                    {
                        std::string key = kv_pair.substr(0, kv_equal);
                        std::string value = kv_pair.substr(kv_equal + 1);
                        instance.metadata[key] = value;
                    }
                }
                
                if (next_semicolon == std::string::npos)
                    break;
                pos = next_semicolon + 1;
            }
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



