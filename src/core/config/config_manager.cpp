#include "config_manager.h"
#include "file_config_loader.h"
#include "apollo_config_loader.h"
#include "config_value.h"
#include "utils/basenode_def_internal.h"
#include <tools/singleton.h>
#include <filesystem>
#include <algorithm>

namespace BaseNode::Config
{

ConfigManager* GetConfigManagerInstance()
{
    return ToolBox::Singleton<ConfigManager>::Instance();
}

ConfigManager::ConfigManager()
{
    // 注册默认的文件配置加载器
    RegisterLoader(std::make_shared<JsonConfigLoader>());
    RegisterLoader(std::make_shared<XmlConfigLoader>());
    RegisterLoader(std::make_shared<YamlConfigLoader>());
}

void ConfigManager::RegisterLoader(IConfigLoaderPtr loader)
{
    if (!loader) return;

    loaders_.push_back(loader);

    // 根据支持的格式注册加载器
    for (const auto& format : loader->GetSupportedFormats())
    {
        loaders_by_format_[format] = loader;
    }

    BaseNodeLogInfo("[ConfigManager] Registered loader: %s", loader->GetName().c_str());
}

bool ConfigManager::LoadConfig(const std::string& source, const std::string& name)
{
    if (source.empty())
    {
        BaseNodeLogError("[ConfigManager] Empty config source");
        return false;
    }

    // 确定配置名称
    std::string config_name = name.empty() ? source : name;

    // 尝试所有加载器，找到能处理此源的加载器
    IConfigLoaderPtr selected_loader;
    for (const auto& loader : loaders_)
    {
        if (loader->IsAvailable(source))
        {
            selected_loader = loader;
            break;
        }
    }

    if (!selected_loader)
    {
        BaseNodeLogError("[ConfigManager] No available loader for source: %s", source.c_str());
        return false;
    }

    // 加载配置
    ConfigValue config = selected_loader->Load(source);
    
    // 检查配置是否有效
    bool is_valid = std::visit([](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, nlohmann::json>)
        {
            return !arg.is_null() && !arg.empty();
        }
        else if constexpr (std::is_same_v<T, YAML::Node>)
        {
            return arg.IsDefined() && !arg.IsNull();
        }
        else if constexpr (std::is_same_v<T, pugi::xml_document>)
        {
            return !arg.document_element().empty();
        }
        return false;
    }, config);
    
    if (!is_valid)
    {
        BaseNodeLogError("[ConfigManager] Failed to load config from: %s", source.c_str());
        return false;
    }

    // 保存配置（使用移动语义，因为 variant 可能包含不可拷贝的类型）
    configs_[config_name] = std::move(config);
    config_sources_[config_name] = source;

    BaseNodeLogInfo("[ConfigManager] Loaded config '%s' from '%s'", config_name.c_str(), source.c_str());
    return true;
}

ConfigValue ConfigManager::Get(const std::string& name, const std::string& path) const
{
    auto it = configs_.find(name);
    if (it == configs_.end())
    {
        BaseNodeLogWarn("[ConfigManager] Config '%s' not found", name.c_str());
        // 返回空的 json，使用 in_place_type 构造
        return ConfigValue(std::in_place_type<nlohmann::json>);
    }

    if (path.empty())
    {
        // 使用 visit 来正确处理不可拷贝的类型（如 pugi::xml_document）
        return std::visit([](auto&& arg) -> ConfigValue {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, pugi::xml_document>)
            {
                // xml_document 不可拷贝，需要克隆整个文档
                pugi::xml_document new_doc;
                new_doc.append_copy(arg.document_element());
                return ConfigValue(std::in_place_type<pugi::xml_document>, std::move(new_doc));
            }
            else
            {
                // 其他类型可以拷贝
                return ConfigValue(arg);
            }
        }, it->second);
    }

    // 使用 ConfigValueHelper 进行路径访问
    return ConfigValueHelper::GetByPath(it->second, path);
}

bool ConfigManager::HasConfig(const std::string& name) const
{
    return configs_.find(name) != configs_.end();
}

void ConfigManager::UnloadConfig(const std::string& name)
{
    auto it = configs_.find(name);
    if (it != configs_.end())
    {
        configs_.erase(it);
        config_sources_.erase(name);
        BaseNodeLogInfo("[ConfigManager] Unloaded config: %s", name.c_str());
    }
}

std::vector<std::string> ConfigManager::GetLoadedConfigNames() const
{
    std::vector<std::string> names;
    names.reserve(configs_.size());
    for (const auto& [name, _] : configs_)
    {
        names.push_back(name);
    }
    return names;
}

bool ConfigManager::LoadConfigFromFile(const std::string& file_path, const std::string& name)
{
    IConfigLoaderPtr loader = SelectLoaderByExtension(file_path);
    if (!loader)
    {
        BaseNodeLogError("[ConfigManager] Unsupported file format: %s", file_path.c_str());
        return false;
    }

    if (!loader->IsAvailable(file_path))
    {
        BaseNodeLogError("[ConfigManager] File not available: %s", file_path.c_str());
        return false;
    }

    std::string config_name = name.empty() ? ExtractConfigNameFromPath(file_path) : name;
    return LoadConfig(file_path, config_name);
}

bool ConfigManager::LoadConfigFromApollo(
    const std::string& namespace_name,
    const std::string& name,
    const std::string& config_server_url,
    const std::string& app_id,
    const std::string& cluster)
{
    if (namespace_name.empty())
    {
        BaseNodeLogError("[ConfigManager] Empty Apollo namespace");
        return false;
    }

    // 检查是否已有 Apollo 加载器
    IConfigLoaderPtr apollo_loader;
    for (const auto& loader : loaders_)
    {
        if (loader->GetName() == "ApolloConfigLoader")
        {
            apollo_loader = loader;
            break;
        }
    }

    // 如果没有，创建新的 Apollo 加载器
    if (!apollo_loader)
    {
        if (config_server_url.empty() || app_id.empty())
        {
            BaseNodeLogError("[ConfigManager] Apollo config_server_url and app_id are required");
            return false;
        }
        apollo_loader = std::make_shared<ApolloConfigLoader>(config_server_url, app_id, cluster, namespace_name);
        RegisterLoader(apollo_loader);
    }

    std::string config_name = name.empty() ? namespace_name : name;
    return LoadConfig(namespace_name, config_name);
}

IConfigLoaderPtr ConfigManager::SelectLoaderByExtension(const std::string& file_path) const
{
    std::filesystem::path path(file_path);
    std::string extension = path.extension().string();
    
    // 去除点号
    if (!extension.empty() && extension[0] == '.')
    {
        extension = extension.substr(1);
    }

    // 转换为小写
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    auto it = loaders_by_format_.find(extension);
    if (it != loaders_by_format_.end())
    {
        return it->second;
    }

    return nullptr;
}

std::string ConfigManager::ExtractConfigNameFromPath(const std::string& file_path) const
{
    std::filesystem::path path(file_path);
    std::string stem = path.stem().string();
    return stem.empty() ? "config" : stem;
}

} // namespace BaseNode::Config

