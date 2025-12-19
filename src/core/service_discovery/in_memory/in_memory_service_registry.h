#pragma once

#include "service_discovery/service_registry.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace BaseNode::ServiceDiscovery
{

/**
 * @brief 简单的进程内内存注册中心实现
 *
 * 主要用于单机 / 测试场景，或作为默认实现。
 */
class InMemoryServiceRegistry final : public IServiceRegistry
{
public:
    bool Register(const ServiceInstance &instance) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        &vec = services_[instance.service_name];
        // 如果 instance_id 已存在则覆盖
        for (auto &inst : vec)
        {
            if (inst.instance_id == instance.instance_id)
            {
                inst = instance;
                return true;
            }
        }
        vec.push_back(instance);
        return true;
    }

    bool Deregister(const ServiceInstance &instance) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        it = services_.find(instance.service_name);
        if (it == services_.end())
        {
            return true;
        }
        auto &vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [&](const ServiceInstance &inst)
                                 {
                                     return inst.instance_id == instance.instance_id;
                                 }),
                  vec.end());
        if (vec.empty())
        {
            services_.erase(it);
        }
        return true;
    }

    bool Renew(const ServiceInstance &instance) override
    {
        // 仅演示占位：真正的注册中心通常需要心跳 / 租约续约
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        it = services_.find(instance.service_name);
        if (it == services_.end())
        {
            return false;
        }
        for (auto &inst : it->second)
        {
            if (inst.instance_id == instance.instance_id)
            {
                inst.healthy = true;
                return true;
            }
        }
        return false;
    }

    /// 提供给同进程的 ServiceDiscovery 使用
    std::vector<ServiceInstance> GetInstances(const std::string &service_name) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        it = services_.find(service_name);
        if (it == services_.end())
        {
            return {};
        }
        return it->second;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<ServiceInstance>> services_;
};

} // namespace BaseNode::ServiceDiscovery



