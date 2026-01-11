#pragma once

#include "3rdparty/nlohmann_json/json.hpp"
#include <variant>
#include <string>

// yaml-cpp 头文件：由于 yaml.h 内部使用 "yaml-cpp/xxx.h" 相对路径，
// 需要将 yaml_cpp/include 添加到包含路径（已在 CMakeLists.txt 中配置）
// 使用系统包含方式，依赖 CMake 配置的包含路径
#include <yaml-cpp/yaml.h>

// pugixml 头文件
#include "3rdparty/pugixml/pugixml.hpp"

namespace BaseNode::Config
{

/**
 * @brief 配置值类型，使用 variant 存储不同格式的原生类型
 * 
 * 保留各格式的原生类型，避免不必要的转换：
 * - nlohmann::json：JSON 格式
 * - YAML::Node：YAML 格式
 * - pugi::xml_document：XML 格式
 */
using ConfigValue = std::variant<nlohmann::json, YAML::Node, pugi::xml_document>;

/**
 * @brief 配置值访问辅助函数
 */
namespace ConfigValueHelper
{
    /**
     * @brief 将 ConfigValue 转换为 nlohmann::json（用于统一访问）
     */
    nlohmann::json ToJson(const ConfigValue& value);

    /**
     * @brief 从路径获取值（支持 JSON Pointer 和 YAML 路径）
     */
    ConfigValue GetByPath(const ConfigValue& value, const std::string& path);
}

} // namespace BaseNode::Config
