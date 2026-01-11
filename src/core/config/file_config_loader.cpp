#include "file_config_loader.h"
#include "config_value.h"
#include "utils/basenode_def_internal.h"
#include <sstream>
#include <cctype>

namespace BaseNode::Config
{

ConfigValue JsonConfigLoader::ParseContent(const std::string& content)
{
    try
    {
        return ConfigValue(nlohmann::json::parse(content));
    }
    catch (const nlohmann::json::parse_error& e)
    {
        BaseNodeLogError("[JsonConfigLoader] Parse error: %s", e.what());
        return ConfigValue(std::in_place_type<nlohmann::json>);
    }
    catch (const std::exception& e)
    {
        BaseNodeLogError("[JsonConfigLoader] Error: %s", e.what());
        return ConfigValue(std::in_place_type<nlohmann::json>);
    }
}

ConfigValue XmlConfigLoader::ParseContent(const std::string& content)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(content.c_str());
    
    if (!result)
    {
        BaseNodeLogError("[XmlConfigLoader] Parse error: %s at offset %zu", 
                         result.description(), result.offset);
        // 返回空的 xml_document，使用 in_place_type 构造
        return ConfigValue(std::in_place_type<pugi::xml_document>);
    }
    
    // 返回解析后的文档，使用 in_place_type 构造 variant，避免拷贝
    return ConfigValue(std::in_place_type<pugi::xml_document>, std::move(doc));
}

ConfigValue YamlConfigLoader::ParseContent(const std::string& content)
{
    try
    {
        YAML::Node node = YAML::Load(content);
        return ConfigValue(node);  // 直接返回 YAML::Node，不转换
    }
    catch (const YAML::Exception& e)
    {
        BaseNodeLogError("[YamlConfigLoader] Parse error: %s", e.what());
        return ConfigValue(YAML::Node());
    }
    catch (const std::exception& e)
    {
        BaseNodeLogError("[YamlConfigLoader] Error: %s", e.what());
        return ConfigValue(YAML::Node());
    }
}

} // namespace BaseNode::Config
