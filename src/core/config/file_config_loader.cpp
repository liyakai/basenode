#include "file_config_loader.h"
#include "config_value.h"
#include "utils/basenode_def_internal.h"
#include <sstream>
#include <cctype>
#include <set>

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

ConfigValue JsonConfigLoaderWithRef::ParseContentWithRef(const std::string& content, const std::string& base_dir)
{
    try
    {
        nlohmann::json json = nlohmann::json::parse(content);
        
        // 递归处理 $ref 引用
        std::set<std::string> visited_refs;
        ResolveRefs(json, base_dir, visited_refs);
        
        return ConfigValue(std::move(json));
    }
    catch (const nlohmann::json::parse_error& e)
    {
        BaseNodeLogError("[JsonConfigLoaderWithRef] Parse error: %s", e.what());
        return ConfigValue(std::in_place_type<nlohmann::json>);
    }
    catch (const std::exception& e)
    {
        BaseNodeLogError("[JsonConfigLoaderWithRef] Error: %s", e.what());
        return ConfigValue(std::in_place_type<nlohmann::json>);
    }
}

void JsonConfigLoaderWithRef::ResolveRefs(nlohmann::json& json, const std::string& base_dir, std::set<std::string>& visited_refs)
{
    if (json.is_object())
    {
        // 检查是否是 $ref 对象
        if (json.contains("$ref") && json.size() == 1)
        {
            std::string ref_path = json["$ref"].get<std::string>();
            std::string abs_path = ResolveRefPath(base_dir, ref_path);
            
            // 检测循环引用
            if (visited_refs.find(abs_path) != visited_refs.end())
            {
                BaseNodeLogError("[JsonConfigLoaderWithRef] Circular reference detected: %s", abs_path.c_str());
                json = nlohmann::json::object(); // 返回空对象避免循环
                return;
            }
            
            // 读取引用的文件
            std::ifstream ref_file(abs_path);
            if (!ref_file.is_open())
            {
                BaseNodeLogError("[JsonConfigLoaderWithRef] Failed to open referenced file: %s", abs_path.c_str());
                json = nlohmann::json::object(); // 返回空对象
                return;
            }
            
            std::stringstream ref_buffer;
            ref_buffer << ref_file.rdbuf();
            ref_file.close();
            
            try
            {
                nlohmann::json ref_json = nlohmann::json::parse(ref_buffer.str());
                
                // 递归处理引用文件中的 $ref
                std::string ref_base_dir = GetBaseDir(abs_path);
                visited_refs.insert(abs_path);
                ResolveRefs(ref_json, ref_base_dir, visited_refs);
                visited_refs.erase(abs_path);
                
                // 替换当前对象为引用内容
                json = std::move(ref_json);
            }
            catch (const nlohmann::json::parse_error& e)
            {
                BaseNodeLogError("[JsonConfigLoaderWithRef] Parse error in referenced file %s: %s", abs_path.c_str(), e.what());
                json = nlohmann::json::object(); // 返回空对象
            }
        }
        else
        {
            // 递归处理对象的所有成员
            for (auto& [key, value] : json.items())
            {
                ResolveRefs(value, base_dir, visited_refs);
            }
        }
    }
    else if (json.is_array())
    {
        // 递归处理数组的所有元素
        for (auto& item : json)
        {
            ResolveRefs(item, base_dir, visited_refs);
        }
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
