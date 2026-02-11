#pragma once

#include "service_discovery/service_discovery_core.h"
#include "service_discovery/in_memory/in_memory_service_registry.h"

#include <memory>

namespace BaseNode::ServiceDiscovery
{

/**
 * @brief 简单进程内实现，直接从 InMemoryServiceRegistry 读取
 */
class InMemoryServiceDiscovery final : public IServiceDiscovery
{
public:
    explicit InMemoryServiceDiscovery(std::shared_ptr<InMemoryServiceRegistry> registry)
        : registry_(std::move(registry))
    {
    }

    InstanceList GetServiceInstances(const std::string &service_name) override
    {
        if (!registry_)
        {
            return {};
        }
        return registry_->GetServiceInstances(service_name);
    }

    void WatchServiceInstances(const std::string &service_name,
               const InstanceList &instance_list,
               InstanceChangeCallback cb) override
    {
        // 纯内存实现暂不做主动推送，先立即回调一次当前视图
        if (!cb)
        {
            return;
        }
        cb(service_name, instance_list);

        // TODO: 如果后续改为真实注册中心，可以在这里注册 Watch 回调
    }

private:
    std::shared_ptr<InMemoryServiceRegistry> registry_;
};

} // namespace BaseNode::ServiceDiscovery


