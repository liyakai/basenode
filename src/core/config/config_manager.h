#pragma once

#include "config_loader.h"
#include "config_value.h"
#include <string>
#include <unordered_map>
#include <vector>

// 前向声明
namespace BaseNode::Config {
    class JsonConfigLoader;
    class XmlConfigLoader;
    class YamlConfigLoader;
    class ApolloConfigLoader;
}

namespace BaseNode::Config
{

/**
 * @brief 配置管理器
 * 
 * 统一管理所有配置源，提供简洁的配置访问接口
 * 支持多种配置格式和配置中心
 */
class ConfigManager
{
public:
    ConfigManager();
    ~ConfigManager() = default;

    /**
     * @brief 注册配置加载器
     * @param loader 配置加载器指针
     */
    void RegisterLoader(IConfigLoaderPtr loader);

    /**
     * @brief 加载配置
     * @param source 配置源（文件路径或配置中心命名空间）
     * @param name 配置名称（用于后续访问），如果为空则使用源路径作为名称
     * @return 是否加载成功
     */
    bool LoadConfig(const std::string& source, const std::string& name = "");

    /**
     * @brief 获取配置值（返回 ConfigValue，保留原生类型）
     * @param name 配置名称
     * @param path 配置路径，支持 "key.subkey" 格式，为空则返回根配置
     * @return 配置值对象（ConfigValue，可能是 nlohmann::json 或 YAML::Node）
     */
    ConfigValue Get(const std::string& name, const std::string& path = "") const;

    /**
     * @brief 获取配置值（模板方法，自动类型转换）
     * @tparam T 目标类型（可以是 nlohmann::json、YAML::Node 或基本类型）
     * @param name 配置名称
     * @param path 配置路径（支持 "key.subkey" 格式）
     * @param default_val 默认值
     * @return 配置值
     */
    template<typename T>
    T Get(const std::string& name, const std::string& path, const T& default_val) const
    {
        ConfigValue value = Get(name, path);
        
        // 如果请求的是原生类型，直接返回
        if constexpr (std::is_same_v<T, nlohmann::json>)
        {
            if (std::holds_alternative<nlohmann::json>(value))
            {
                return std::get<nlohmann::json>(value);
            }
            // YAML/XML 转换为 JSON
            return ConfigValueHelper::ToJson(value);
        }
        else if constexpr (std::is_same_v<T, YAML::Node>)
        {
            if (std::holds_alternative<YAML::Node>(value))
            {
                return std::get<YAML::Node>(value);
            }
            return default_val;  // JSON/XML 无法直接转换为 YAML::Node
        }
        else if constexpr (std::is_same_v<T, pugi::xml_document>)
        {
            if (std::holds_alternative<pugi::xml_document>(value))
            {
                // 使用移动语义，因为 xml_document 不可拷贝
                return std::move(std::get<pugi::xml_document>(value));
            }
            // 返回默认值（空文档）
            return pugi::xml_document();
        }
        else
        {
            // 基本类型：转换为 JSON 后获取
            nlohmann::json j = ConfigValueHelper::ToJson(value);
            if (j.is_null()) return default_val;
            try {
                return j.get<T>();
            } catch (...) {
                return default_val;
            }
        }
    }

    /**
     * @brief 获取配置值（返回 nlohmann::json，兼容旧接口）
     * @param name 配置名称
     * @param path 配置路径
     * @return nlohmann::json 对象
     */
    nlohmann::json GetAsJson(const std::string& name, const std::string& path = "") const
    {
        return Get<nlohmann::json>(name, path, nlohmann::json());
    }

    /**
     * @brief 获取配置值（返回 YAML::Node）
     * @param name 配置名称
     * @param path 配置路径
     * @return YAML::Node 对象
     */
    YAML::Node GetAsYaml(const std::string& name, const std::string& path = "") const
    {
        return Get<YAML::Node>(name, path, YAML::Node());
    }

    /**
     * @brief 获取配置值（返回 pugi::xml_document）
     * @param name 配置名称
     * @param path 配置路径
     * @return pugi::xml_document 对象
     */
    pugi::xml_document GetAsXml(const std::string& name, const std::string& path = "") const
    {
        ConfigValue value = Get(name, path);
        if (std::holds_alternative<pugi::xml_document>(value))
        {
            // 使用移动语义，因为 xml_document 不可拷贝
            return std::move(std::get<pugi::xml_document>(value));
        }
        return pugi::xml_document();
    }

    /**
     * @brief 检查配置是否存在
     * @param name 配置名称
     * @return 是否存在
     */
    bool HasConfig(const std::string& name) const;

    /**
     * @brief 卸载配置
     * @param name 配置名称
     */
    void UnloadConfig(const std::string& name);

    /**
     * @brief 获取所有已加载的配置名称
     * @return 配置名称列表
     */
    std::vector<std::string> GetLoadedConfigNames() const;

    /**
     * @brief 根据文件扩展名自动选择加载器并加载配置
     * @param file_path 文件路径
     * @param name 配置名称，如果为空则使用文件名（不含扩展名）
     * @return 是否加载成功
     */
    bool LoadConfigFromFile(const std::string& file_path, const std::string& name = "");

    /**
     * @brief 从 Apollo 配置中心加载配置
     * @param namespace_name 命名空间
     * @param name 配置名称，如果为空则使用命名空间名称
     * @return 是否加载成功
     */
    bool LoadConfigFromApollo(
        const std::string& namespace_name,
        const std::string& name = "",
        const std::string& config_server_url = "",
        const std::string& app_id = "",
        const std::string& cluster = "default");

private:
    /**
     * @brief 根据文件扩展名选择加载器
     * @param file_path 文件路径
     * @return 加载器指针，如果找不到则返回 nullptr
     */
    IConfigLoaderPtr SelectLoaderByExtension(const std::string& file_path) const;

    /**
     * @brief 从文件路径提取配置名称
     * @param file_path 文件路径
     * @return 配置名称
     */
    std::string ExtractConfigNameFromPath(const std::string& file_path) const;

    std::unordered_map<std::string, IConfigLoaderPtr> loaders_by_format_;
    std::vector<IConfigLoaderPtr> loaders_;
    std::unordered_map<std::string, ConfigValue> configs_;
    std::unordered_map<std::string, std::string> config_sources_; // name -> source
};

/// 全局单例访问宏
#define ConfigMgr ::ToolBox::Singleton<BaseNode::Config::ConfigManager>::Instance()

} // namespace BaseNode::Config

