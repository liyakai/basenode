#include "service_discovery/zookeeper/zk_service_discovery.h"

#include <string>
#include <functional>

#include "utils/basenode_def_internal.h"

namespace BaseNode::ServiceDiscovery::Zookeeper
{

InstanceList ZkServiceDiscovery::GetServiceInstances(const std::string &service_name)
{
    InstanceList result;
    if (!zk_client_)
    {
        BaseNodeLogError("[ZkServiceDiscovery] GetServiceInstances: zk_client_ is null");
        return result;
    }

    

    // 路径结构：/basenode/services/{host:port}/{service_name}/{instance_id}
    const std::string services_root = paths_.ServicesRoot();
    BaseNodeLogInfo("[ZkServiceDiscovery] GetServiceInstances: service_name:%s, services_root:%s", 
                    service_name.c_str(), services_root.c_str());

    if(service_name == services_root)
    {
        // 获取所有 host:port 节点
        auto host_port_list = zk_client_->GetChildren(services_root);
        BaseNodeLogInfo("[ZkServiceDiscovery] GetServiceInstances: host_port_list size:%zu", host_port_list.size());
        for (const auto &host_port : host_port_list)
        {
            auto host_port_path = services_root + '/' + host_port;
            // 获取该 host:port 下的所有服务名
            auto service_list = zk_client_->GetChildren(host_port_path);
            BaseNodeLogInfo("[ZkServiceDiscovery] GetServiceInstances, host_port_path:%s, children service_list size:%zu", host_port_path.c_str(), service_list.size());
            for (const auto &svc : service_list)
            {   
                auto service_path = host_port_path + '/' + svc;
                // 获取该服务下的所有实例 ID
                auto instance_id_list = zk_client_->GetChildren(service_path);
                BaseNodeLogInfo("[ZkServiceDiscovery] GetServiceInstances, service_path:%s, children instance_id_list size:%zu", service_path.c_str(), instance_id_list.size());
                bool has_instance = false;
                for (const auto &instance_id : instance_id_list)
                {
                    auto instance_path = service_path + '/' + instance_id;
                    std::string instance_data;
                    if (!zk_client_->GetData(instance_path, instance_data))
                    {
                        BaseNodeLogError("[ZkServiceDiscovery] GetServiceInstances: get data failed. inst_path:%s", 
                                        instance_path.c_str());
                        continue;
                    }
                    BaseNodeLogInfo("[ZkServiceDiscovery] GetServiceInstances, instance_path:%s, instance_data:%s", instance_path.c_str(), instance_data.c_str());
                    result.push_back(ParseServiceInstance(instance_data));
                    has_instance = true;
                }
                if(!has_instance)
                {
                    ServiceInstance instance;
                    instance.service_name = "";
                    instance.module_name = svc;
                    instance.instance_id = 0;
                    instance.host = host_port.substr(0, host_port.find(':'));
                    instance.port = static_cast<uint16_t>(std::stoi(host_port.substr(host_port.find(':') + 1)));
                    instance.healthy = true;
                    instance.connection_id = 0;
                    instance.metadata.clear();
                    result.push_back(instance);
                    BaseNodeLogInfo("[ZkServiceDiscovery] GetServiceInstances, added instance %s, service_path:%s.", instance.SerializeInstance().c_str(), service_path.c_str());
                }
            }
        }
    } else 
    {
        ServiceInstance instance;
        instance.service_name = service_name;
        result.push_back(instance);
    }
    
    
    BaseNodeLogInfo("[ZkServiceDiscovery] GetServiceInstances: result size:%zu", result.size());
    return result;
}

void ZkServiceDiscovery::WatchServiceInstances(const std::string &service_name, 
                                const ServiceDiscovery::InstanceList &instance_list,
                                InstanceChangeCallback cb)
{
    if (!zk_client_ || !cb)
    {
        return;
    }

    // 先立刻回调一次当前视图
    cb(service_name, instance_list);

    // 递归监听三层结构：/basenode/services/{host:port}/{service_name}/{instance_id}
    const std::string services_root = paths_.ServicesRoot();
    
    // 使用 std::function 允许递归调用
    std::function<void(const std::string&, int)> setupWatch;
    setupWatch = [this, service_name, cb, instance_list, &setupWatch](const std::string &path, int level) {
        // level 0: /basenode/services (监听 host:port 变化)
        // level 1: /basenode/services/{host:port} (监听 service_name 变化)
        // level 2: /basenode/services/{host:port}/{service_name} (监听 instance_id 变化)
        
        zk_client_->WatchChildren(path, [this, service_name, cb, level, &setupWatch, path, instance_list](const std::string &changed_path) {
            // changed_path 是被监听的路径（即 path 参数），当它的子节点变化时触发
            // 当子节点变化时，重新获取所有实例并回调
            cb(changed_path, instance_list);
            
            // 根据层级递归设置新的监听
            if (level == 0) {
                // services_root 的子节点（host:port）变化，需要为所有 host:port 设置监听
                auto host_port_list = zk_client_->GetChildren(changed_path);
                for (const auto &host_port : host_port_list) {
                    std::string host_port_path = changed_path + "/" + host_port;
                    setupWatch(host_port_path, 1);
                    
                    // 检查该 host:port 下是否有目标 service_name
                    auto service_list = zk_client_->GetChildren(host_port_path);
                    for (const auto &svc : service_list) {
                        if (svc == service_name) {
                            std::string service_path = host_port_path + "/" + service_name;
                            setupWatch(service_path, 2);
                        }
                    }
                }
            } else if (level == 1) {
                // host:port 的子节点（service_name）变化，检查是否有目标 service_name
                auto service_list = zk_client_->GetChildren(changed_path);
                for (const auto &svc : service_list) {
                    if (svc == service_name) {
                        std::string service_path = changed_path + "/" + service_name;
                        setupWatch(service_path, 2);
                    }
                }
            }
            // level 2 不需要再递归，因为 instance_id 是叶子节点
        });
    };
    
    // 初始设置：监听 services_root 下的 host:port 变化
    setupWatch(services_root, 0);
    
    // 为当前已存在的所有 host:port 设置监听
    auto host_port_list = zk_client_->GetChildren(services_root);
    for (const auto &host_port : host_port_list) {
        std::string host_port_path = services_root + "/" + host_port;
        setupWatch(host_port_path, 1);
        
        // 检查该 host:port 下是否有目标 service_name
        auto service_list = zk_client_->GetChildren(host_port_path);
        for (const auto &svc : service_list) {
            if (svc == service_name) {
                std::string service_path = host_port_path + "/" + service_name;
                setupWatch(service_path, 2);
            }
        }
    }
}

BaseNode::ServiceDiscovery::ServiceInstance
ZkServiceDiscovery::ParseServiceInstance(const std::string &data) const
{
    return BaseNode::ServiceDiscovery::ServiceInstance::ParseInstance(data);
}

} // namespace BaseNode::ServiceDiscovery::Zookeeper




