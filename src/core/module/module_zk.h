#pragma once

#include "utils/basenode_def_internal.h"
#include "service_discovery/service_discovery_core.h"

// 前向声明
namespace BaseNode {
    class IModule;
}

namespace BaseNode
{

/**
 * @brief 模块 ZK 注册接口
 * 用于将模块注册到 Zookeeper 服务发现系统
 * 此接口定义在 basenode_core 中，由 service_discovery 模块实现，避免循环依赖
 */
class IModuleZkRegistry
{
public:
    virtual ~IModuleZkRegistry() = default;

    /**
     * @brief 注册模块到 ZK
     * @param module 模块指针
     * @return 是否注册成功
     */
    virtual bool RegisterModule(IModule* module) = 0;

    /**
     * @brief 注销模块
     * @param module 模块指针
     * @return 是否注销成功
     */
    virtual bool DeregisterModule(IModule* module) = 0;
};

class IModuleZkDiscovery
{
public:
    virtual ~IModuleZkDiscovery() = default;

    /**
     * @brief 获取模块实例列表
     * @return 模块实例列表
     */

     virtual ServiceDiscovery::InstanceList GetServiceInstances(const std::string &service_name) = 0;

     virtual void WatchServiceInstances(const std::string &service_name,
                ServiceDiscovery::InstanceChangeCallback cb) = 0;

};

} // namespace BaseNode

// 获取模块 ZK 注册器实例的全局函数（在 service_discovery 模块中实现）
// 使用 extern "C" 确保符号在所有模块间共享
// 使用弱符号，如果 service_discovery 未加载则链接时不会报错，运行时返回 nullptr
extern "C" __attribute__((weak)) BaseNode::IModuleZkRegistry* GetModuleZkRegistryInstance();
extern "C" __attribute__((weak)) BaseNode::IModuleZkDiscovery* GetModuleZkDiscoveryInstance();

// 便捷宏，与 ModuleRouterMgr 风格保持一致
// 如果 service_discovery 未加载，GetModuleZkRegistryInstance 为 nullptr，调用时返回 nullptr
#define ModuleZkRegistryMgr GetModuleZkRegistryInstance()
#define ModuleZkDiscoveryMgr GetModuleZkDiscoveryInstance()