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
    std::string instance_id;
    std::string host;
    uint16_t    port{0};
    bool        healthy{true};
    std::unordered_map<std::string, std::string> metadata;
};

// ------------------------- Registry 接口 ---------------------------

class IServiceRegistry
{
public:
    virtual ~IServiceRegistry() = default;

    virtual bool Register(const ServiceInstance &instance)   = 0;
    virtual bool Deregister(const ServiceInstance &instance) = 0;
    virtual bool Renew(const ServiceInstance &instance)      = 0;
};

using IServiceRegistryPtr = std::shared_ptr<IServiceRegistry>;

// ------------------------- Discovery 接口 --------------------------

using InstanceList = std::vector<ServiceInstance>;

using InstanceChangeCallback = std::function<void(const std::string &service_name,
                                                  const InstanceList &instances)>;

class IServiceDiscovery
{
public:
    virtual ~IServiceDiscovery() = default;

    virtual InstanceList GetInstances(const std::string &service_name) = 0;
    virtual void Watch(const std::string &service_name,
                       InstanceChangeCallback cb) = 0;
};

using IServiceDiscoveryPtr = std::shared_ptr<IServiceDiscovery>;

// ------------------------- 负载均衡 -------------------------------

struct RequestContext
{
    std::string caller_zone;
    std::string caller_idc;
    std::string hash_key;
    std::unordered_map<std::string, std::string> labels;
};

class ILoadBalancer
{
public:
    virtual ~ILoadBalancer() = default;

    virtual std::optional<ServiceInstance>
    Choose(const std::string &service_name,
           const std::vector<ServiceInstance> &instances,
           const RequestContext &ctx) const = 0;
};

/**
 * @brief 简单的多机房感知负载均衡
 */
class ZoneAwareLoadBalancer final : public ILoadBalancer
{
public:
    std::optional<ServiceInstance>
    Choose(const std::string &/*service_name*/,
           const std::vector<ServiceInstance> &instances,
           const RequestContext &ctx) const override
    {
        if (instances.empty())
        {
            return std::nullopt;
        }

        auto filter_by = [&](auto pred) -> std::vector<ServiceInstance>
        {
            std::vector<ServiceInstance> result;
            result.reserve(instances.size());
            for (const auto &inst : instances)
            {
                if (!inst.healthy)
                {
                    continue;
                }
                if (pred(inst))
                {
                    result.push_back(inst);
                }
            }
            return result;
        };

        auto same_idc = filter_by([&](const ServiceInstance &inst)
                                  {
                                      auto it_zone = inst.metadata.find("zone");
                                      auto it_idc = inst.metadata.find("idc");
                                      return it_zone != inst.metadata.end() &&
                                             it_idc != inst.metadata.end() &&
                                             it_zone->second == ctx.caller_zone &&
                                             it_idc->second == ctx.caller_idc;
                                  });
        if (!same_idc.empty())
        {
            return PickRoundRobin_(same_idc);
        }

        auto same_zone = filter_by([&](const ServiceInstance &inst)
                                   {
                                       auto it_zone = inst.metadata.find("zone");
                                       return it_zone != inst.metadata.end() &&
                                              it_zone->second == ctx.caller_zone;
                                   });
        if (!same_zone.empty())
        {
            return PickRoundRobin_(same_zone);
        }

        std::vector<ServiceInstance> healthy;
        healthy.reserve(instances.size());
        for (const auto &inst : instances)
        {
            if (inst.healthy)
            {
                healthy.push_back(inst);
            }
        }
        if (healthy.empty())
        {
            return std::nullopt;
        }
        return PickRoundRobin_(healthy);
    }

private:
    std::optional<ServiceInstance> PickRoundRobin_(const std::vector<ServiceInstance> &instances) const
    {
        const auto pos = next_index_.fetch_add(1, std::memory_order_relaxed) % instances.size();
        return instances[pos];
    }

private:
    mutable std::atomic<std::size_t> next_index_{0};
};

using ILoadBalancerPtr = std::shared_ptr<ILoadBalancer>;

// ------------------------- DiscoveryClient ------------------------

struct CacheEntry
{
    std::vector<ServiceInstance>          instances;
    std::chrono::steady_clock::time_point expire_at;
};

class LocalCache
{
public:
    void Put(const std::string &service_name,
             const std::vector<ServiceInstance> &instances,
             std::chrono::seconds ttl)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        CacheEntry                  entry;
        entry.instances = instances;
        entry.expire_at = std::chrono::steady_clock::now() + ttl;
        cache_[service_name] = std::move(entry);
    }

    std::vector<ServiceInstance> Get(const std::string &service_name)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        it = cache_.find(service_name);
        if (it == cache_.end())
        {
            return {};
        }
        auto now = std::chrono::steady_clock::now();
        if (now >= it->second.expire_at)
        {
            cache_.erase(it);
            return {};
        }
        return it->second.instances;
    }

private:
    std::mutex                                  mutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
};

class IDiscoveryClient
{
public:
    virtual ~IDiscoveryClient() = default;

    virtual std::optional<ServiceInstance>
    ChooseInstance(const std::string &service_name,
                   const RequestContext &ctx) = 0;
};

class DefaultDiscoveryClient final : public IDiscoveryClient
{
public:
    DefaultDiscoveryClient(IServiceDiscoveryPtr discovery,
                           ILoadBalancerPtr     lb,
                           std::chrono::seconds cache_ttl)
        : discovery_(std::move(discovery))
        , lb_(std::move(lb))
        , cache_ttl_(cache_ttl)
    {
    }

    std::optional<ServiceInstance>
    ChooseInstance(const std::string &service_name,
                   const RequestContext &ctx) override
    {
        auto instances = cache_.Get(service_name);
        if (instances.empty())
        {
            instances = discovery_ ? discovery_->GetInstances(service_name) : std::vector<ServiceInstance>{};
            if (!instances.empty())
            {
                cache_.Put(service_name, instances, cache_ttl_);
            }
        }
        if (!lb_)
        {
            return std::nullopt;
        }
        return lb_->Choose(service_name, instances, ctx);
    }

private:
    IServiceDiscoveryPtr discovery_;
    ILoadBalancerPtr     lb_;
    LocalCache           cache_;
    std::chrono::seconds cache_ttl_;
};

using IDiscoveryClientPtr = std::shared_ptr<IDiscoveryClient>;

} // namespace BaseNode::ServiceDiscovery



