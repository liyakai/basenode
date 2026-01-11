#include "config_value.h"
#include "utils/basenode_def_internal.h"

namespace BaseNode::Config
{

namespace ConfigValueHelper
{

nlohmann::json ToJson(const ConfigValue& value)
{
    return std::visit([](auto&& arg) -> nlohmann::json {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, nlohmann::json>)
        {
            return arg;
        }
        else if constexpr (std::is_same_v<T, YAML::Node>)
        {
            // 将 YAML::Node 转换为 nlohmann::json
            if (!arg.IsDefined() || arg.IsNull())
            {
                return nlohmann::json();
            }
            
            if (arg.IsScalar())
            {
                std::string scalar = arg.template as<std::string>();
                try
                {
                    if (scalar.find('.') == std::string::npos && 
                        scalar.find('e') == std::string::npos && 
                        scalar.find('E') == std::string::npos)
                    {
                        return nlohmann::json(std::stoll(scalar));
                    }
                    return nlohmann::json(std::stod(scalar));
                }
                catch (...)
                {
                    if (scalar == "true" || scalar == "True" || scalar == "TRUE")
                    {
                        return nlohmann::json(true);
                    }
                    if (scalar == "false" || scalar == "False" || scalar == "FALSE")
                    {
                        return nlohmann::json(false);
                    }
                    return nlohmann::json(scalar);
                }
            }
            else if (arg.IsSequence())
            {
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& item : arg)
                {
                    arr.push_back(ToJson(ConfigValue(item)));
                }
                return arr;
            }
            else if (arg.IsMap())
            {
                nlohmann::json obj = nlohmann::json::object();
                for (const auto& it : arg)
                {
                    std::string key = it.first.template as<std::string>();
                    obj[key] = ToJson(ConfigValue(it.second));
                }
                return obj;
            }
            
            return nlohmann::json();
        }
        else if constexpr (std::is_same_v<T, pugi::xml_document>)
        {
            // 将 pugi::xml_document 转换为 nlohmann::json
            nlohmann::json root = nlohmann::json::object();
            
            // 获取根节点
            pugi::xml_node root_node = arg.document_element();
            if (root_node.empty())
            {
                return root;
            }
            
            // 递归转换 XML 节点为 JSON
            std::function<nlohmann::json(const pugi::xml_node&)> convertNode = 
                [&convertNode](const pugi::xml_node& node) -> nlohmann::json {
                if (node.type() == pugi::node_element)
                {
                    nlohmann::json obj = nlohmann::json::object();
                    
                    // 处理属性
                    for (pugi::xml_attribute attr = node.first_attribute(); attr; attr = attr.next_attribute())
                    {
                        std::string attr_name = "@" + std::string(attr.name());
                        obj[attr_name] = std::string(attr.value());
                    }
                    
                    // 处理子节点
                    std::map<std::string, int> child_counts;
                    for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling())
                    {
                        if (child.type() == pugi::node_element)
                        {
                            std::string child_name = child.name();
                            child_counts[child_name]++;
                        }
                        else if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
                        {
                            std::string text = child.value();
                            if (!text.empty())
                            {
                                obj["#text"] = text;
                            }
                        }
                    }
                    
                    for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling())
                    {
                        if (child.type() == pugi::node_element)
                        {
                            std::string child_name = child.name();
                            nlohmann::json child_json = convertNode(child);
                            
                            if (child_counts[child_name] > 1)
                            {
                                // 多个同名节点，转换为数组
                                if (!obj.contains(child_name))
                                {
                                    obj[child_name] = nlohmann::json::array();
                                }
                                obj[child_name].push_back(child_json);
                            }
                            else
                            {
                                // 单个节点
                                obj[child_name] = child_json;
                            }
                        }
                    }
                    
                    return obj;
                }
                return nlohmann::json();
            };
            
            return convertNode(root_node);
        }
        else
        {
            return nlohmann::json();
        }
    }, value);
}

ConfigValue GetByPath(const ConfigValue& value, const std::string& path)
{
    if (path.empty())
    {
        // 需要返回一个副本，但 variant 可能包含不可拷贝的类型
        // 使用 visit 来处理每种类型
        return std::visit([](auto&& arg) -> ConfigValue {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, pugi::xml_document>)
            {
                // xml_document 不可拷贝，创建一个新的空文档
                // 返回空的 xml_document，使用 in_place_type 构造
            return ConfigValue(std::in_place_type<pugi::xml_document>);
            }
            else
            {
                // 其他类型可以拷贝
                return ConfigValue(arg);
            }
        }, value);
    }

    return std::visit([&path](auto&& arg) -> ConfigValue {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, nlohmann::json>)
        {
            // JSON Pointer 访问
            std::string json_pointer = path;
            if (path[0] != '/')
            {
                json_pointer = "/" + path;
                std::replace(json_pointer.begin(), json_pointer.end(), '.', '/');
            }
            try
            {
                return ConfigValue(arg[nlohmann::json::json_pointer(json_pointer)]);
            }
            catch (...)
            {
                return ConfigValue(std::in_place_type<nlohmann::json>);
            }
        }
        else if constexpr (std::is_same_v<T, YAML::Node>)
        {
            // YAML 路径访问（支持 "key.subkey" 格式）
            YAML::Node current = arg;
            std::istringstream iss(path);
            std::string segment;
            
            while (std::getline(iss, segment, '.'))
            {
                if (current.IsMap() && current[segment].IsDefined())
                {
                    current = current[segment];
                }
                else
                {
                    return ConfigValue(YAML::Node());
                }
            }
            
            return ConfigValue(current);
        }
        else if constexpr (std::is_same_v<T, pugi::xml_document>)
        {
            // XML 路径访问（支持 "key.subkey" 格式，使用 XPath）
            pugi::xml_node root = arg.document_element();
            if (root.empty())
            {
                // 返回空的 xml_document，使用 in_place_type 构造
            return ConfigValue(std::in_place_type<pugi::xml_document>);
            }
            
            // 将点号路径转换为 XPath
            std::string xpath = path;
            std::replace(xpath.begin(), xpath.end(), '.', '/');
            if (xpath[0] != '/')
            {
                xpath = "/" + xpath;
            }
            
            try
            {
                pugi::xpath_node result = root.select_node(xpath.c_str());
                if (result)
                {
                    // 创建一个新的文档包含选中的节点
                    pugi::xml_document new_doc;
                    new_doc.append_copy(result.node());
                    // 使用 in_place_type 构造 variant，避免拷贝
                    return ConfigValue(std::in_place_type<pugi::xml_document>, std::move(new_doc));
                }
            }
            catch (...)
            {
                // XPath 解析失败，尝试简单路径
                pugi::xml_node current = root;
                std::istringstream iss(path);
                std::string segment;
                
                while (std::getline(iss, segment, '.'))
                {
                    current = current.child(segment.c_str());
                    if (current.empty())
                    {
                        // 返回空的 xml_document，使用 in_place_type 构造
            return ConfigValue(std::in_place_type<pugi::xml_document>);
                    }
                }
                
                pugi::xml_document new_doc;
                new_doc.append_copy(current);
                // 使用 in_place_type 构造 variant，避免拷贝
                return ConfigValue(std::in_place_type<pugi::xml_document>, std::move(new_doc));
            }
            
            // 返回空的 xml_document，使用 in_place_type 构造
            return ConfigValue(std::in_place_type<pugi::xml_document>);
        }
        else
        {
            return ConfigValue(std::in_place_type<nlohmann::json>);
        }
    }, value);
}

} // namespace ConfigValueHelper

} // namespace BaseNode::Config

