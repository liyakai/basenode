#pragma once

#include "config_loader.h"
#include "config_value.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace BaseNode::Config
{

/**
 * @brief 文件配置加载器基类
 * 
 * 提供文件读取的通用功能，子类实现具体的解析逻辑
 */
class FileConfigLoader : public IConfigLoader
{
public:
    ConfigValue Load(const std::string& source) override
    {
        std::ifstream file(source);
        if (!file.is_open())
        {
            return ConfigValue(std::in_place_type<nlohmann::json>);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

        return ParseContent(content);
    }

    bool IsAvailable(const std::string& source) const override
    {
        std::ifstream file(source);
        bool available = file.good();
        file.close();
        return available;
    }

protected:
    /**
     * @brief 解析配置内容（由子类实现）
     * @param content 文件内容
     * @return 配置值对象（ConfigValue，各格式返回原生类型）
     */
    virtual ConfigValue ParseContent(const std::string& content) = 0;

    /**
     * @brief 获取文件的基础目录（用于解析相对路径的引用）
     * @param file_path 文件路径
     * @return 基础目录路径
     */
    static std::string GetBaseDir(const std::string& file_path)
    {
        std::filesystem::path path(file_path);
        return path.parent_path().string();
    }

    /**
     * @brief 解析相对路径为绝对路径
     * @param base_dir 基础目录
     * @param ref_path 引用路径（可能是相对路径或绝对路径）
     * @return 解析后的绝对路径
     */
    static std::string ResolveRefPath(const std::string& base_dir, const std::string& ref_path)
    {
        if (std::filesystem::path(ref_path).is_absolute())
        {
            return ref_path;
        }
        std::filesystem::path base(base_dir);
        std::filesystem::path ref(ref_path);
        return (base / ref).lexically_normal().string();
    }
};

/**
 * @brief JSON 配置加载器
 */
class JsonConfigLoader final : public FileConfigLoader
{
public:
    std::string GetName() const override { return "JsonConfigLoader"; }
    
    std::vector<std::string> GetSupportedFormats() const override
    {
        return {"json"};
    }

protected:
    ConfigValue ParseContent(const std::string& content) override;
};

/**
 * @brief 支持 $ref 引用的 JSON 配置加载器
 * 
 * 支持在 JSON 配置中使用 $ref 关键字引用其他 JSON 文件
 * 引用会在加载时自动解析并合并到当前位置
 * 
 * 使用示例:
 * {
 *   "process": {"$ref": "config/process.json"},
 *   "log": {"$ref": "config/log.json"}
 * }
 */
class JsonConfigLoaderWithRef final : public FileConfigLoader
{
public:
    std::string GetName() const override { return "JsonConfigLoaderWithRef"; }
    
    std::vector<std::string> GetSupportedFormats() const override
    {
        return {"json"};
    }

    /**
     * @brief 加载配置（支持 $ref 引用）
     * @param source 配置文件路径
     * @return 解析后的配置值，$ref 已自动展开
     */
    ConfigValue Load(const std::string& source) override
    {
        std::ifstream file(source);
        if (!file.is_open())
        {
            return ConfigValue(std::in_place_type<nlohmann::json>);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

        std::string base_dir = GetBaseDir(source);
        return ParseContentWithRef(content, base_dir);
    }

private:
    /**
     * @brief 解析配置内容，支持 $ref 引用
     * @param content JSON 内容
     * @param base_dir 基础目录（用于解析相对路径的 $ref）
     * @return 解析后的配置值
     */
    ConfigValue ParseContentWithRef(const std::string& content, const std::string& base_dir);

    /**
     * @brief 递归处理 JSON 对象中的 $ref 引用
     * @param json JSON 对象（会被修改）
     * @param base_dir 基础目录
     * @param visited_refs 已访问的引用路径集合（用于检测循环引用）
     */
    void ResolveRefs(nlohmann::json& json, const std::string& base_dir, std::set<std::string>& visited_refs);
};

/**
 * @brief XML 配置加载器
 * 
 */
class XmlConfigLoader final : public FileConfigLoader
{
public:
    std::string GetName() const override { return "XmlConfigLoader"; }
    
    std::vector<std::string> GetSupportedFormats() const override
    {
        return {"xml"};
    }

protected:
    ConfigValue ParseContent(const std::string& content) override;
};

/**
 * @brief YAML 配置加载器
 * 
 */
class YamlConfigLoader final : public FileConfigLoader
{
public:
    std::string GetName() const override { return "YamlConfigLoader"; }
    
    std::vector<std::string> GetSupportedFormats() const override
    {
        return {"yaml", "yml"};
    }

protected:
    ConfigValue ParseContent(const std::string& content) override;
};

} // namespace BaseNode::Config
