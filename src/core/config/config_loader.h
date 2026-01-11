#pragma once

#include "config_value.h"
#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace BaseNode::Config
{

/**
 * @brief 配置加载器接口
 * 
 * 所有配置源（文件、配置中心等）都需要实现此接口
 * 各加载器返回各自格式的原生类型，避免不必要的转换
 */
class IConfigLoader
{
public:
    virtual ~IConfigLoader() = default;

    /**
     * @brief 加载配置
     * @param source 配置源标识（文件路径、配置中心命名空间等）
     * @return 配置值对象（ConfigValue，可能是 nlohmann::json 或 YAML::Node），失败返回空对象
     */
    virtual ConfigValue Load(const std::string& source) = 0;

    /**
     * @brief 检查配置源是否可用
     * @param source 配置源标识
     * @return 是否可用
     */
    virtual bool IsAvailable(const std::string& source) const = 0;

    /**
     * @brief 获取加载器名称
     * @return 加载器名称
     */
    virtual std::string GetName() const = 0;

    /**
     * @brief 支持的文件扩展名或配置类型（用于自动选择加载器）
     * @return 支持的扩展名列表，如 {"json", "xml", "yaml"}
     */
    virtual std::vector<std::string> GetSupportedFormats() const = 0;
};

using IConfigLoaderPtr = std::shared_ptr<IConfigLoader>;

/**
 * @brief 配置变更回调函数类型
 */
using ConfigChangeCallback = std::function<void(const std::string& key, const ConfigValue& new_value)>;

/**
 * @brief 支持热更新的配置加载器接口
 */
class IHotReloadConfigLoader : public IConfigLoader
{
public:
    /**
     * @brief 注册配置变更回调
     * @param callback 变更回调函数
     */
    virtual void RegisterChangeCallback(ConfigChangeCallback callback) = 0;

    /**
     * @brief 开始监听配置变更
     * @param source 配置源标识
     * @return 是否成功开始监听
     */
    virtual bool StartWatch(const std::string& source) = 0;

    /**
     * @brief 停止监听配置变更
     * @param source 配置源标识
     */
    virtual void StopWatch(const std::string& source) = 0;
};

} // namespace BaseNode::Config



