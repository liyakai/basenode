#pragma once

#include <string>

namespace BaseNode::ServiceDiscovery::Zookeeper
{

/**
 * @brief 统一管理 ZK 路径规则，便于后续迁移 / 重构
 *
 * 约定根路径 layout（举例）：
 *  /basenode
 *      /processes/{process_id}                         进程维度
 *          /modules/{module_id}                        模块维度
 *              /rpcs/{rpc_handler_key}                 模块下可提供的 RPC 函数
 *      /services/{service_name}
 *          /instances/{instance_id}                    服务实例维度（承载 ServiceInstance 数据）
 */
struct ZkPaths
{
    std::string root; ///< 例如 "/basenode"

    explicit ZkPaths(std::string r) : root(std::move(r)) {}

    std::string BaseNodeRoot() const { return root; }
    std::string ProcessesRoot() const { return root + "/processes"; }
    std::string ModulesRoot() const { return root + "/modules"; }
    std::string ServicesRoot() const { return root + "/services"; }

    std::string ProcessPath(const std::string &process_id) const
    {
        return ProcessesRoot() + "/" + process_id;
    }

    std::string ModulePath(const std::string &module_class_name) const
    {
        return ModulesRoot() + "/" + module_class_name;
    }

    std::string ServicePath(const std::string &service_name) const
    {
        return ServicesRoot() + "/" + service_name;
    }

    std::string ServiceInstancesPath(const std::string &service_name) const
    {
        return ServicePath(service_name) + "/instances";
    }

    std::string ServiceInstancePath(const std::string &service_name,
                                    const std::string &instance_id) const
    {
        return ServiceInstancesPath(service_name) + "/" + instance_id;
    }
};

} // namespace BaseNode::ServiceDiscovery::Zookeeper




