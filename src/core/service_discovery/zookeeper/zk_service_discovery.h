#pragma once

#include "service_discovery/service_discovery_core.h"
#include "service_discovery/zookeeper/zk_client.h"
#include "service_discovery/zookeeper/zk_paths.h"

#include <memory>
#include <string>
#include <vector>

namespace BaseNode::ServiceDiscovery::Zookeeper
{

/**
 * @brief 基于 Zookeeper 的服务发现实现
 *
 * 只负责“读”视角：根据服务名获取实例列表，并可监听子节点变化。
 */
class ZkServiceDiscovery final : public BaseNode::ServiceDiscovery::IServiceDiscovery
{
public:
    ZkServiceDiscovery(IZkClientPtr zk_client, ZkPaths paths)
        : zk_client_(std::move(zk_client))
        , paths_(std::move(paths))
    {
    }

    InstanceList GetInstances(const std::string &service_name) override;

    void Watch(const std::string &service_name,
               InstanceChangeCallback cb) override;

private:
    BaseNode::ServiceDiscovery::ServiceInstance
    ParseInstance(const std::string &service_name,
                  const std::string &instance_id,
                  const std::string &data) const;

private:
    IZkClientPtr zk_client_;
    ZkPaths      paths_;
};

using ZkServiceDiscoveryPtr = std::shared_ptr<ZkServiceDiscovery>;

} // namespace BaseNode::ServiceDiscovery::Zookeeper



