#pragma once

#include "config_loader.h"
#include "config_value.h"
#include <fstream>
#include <sstream>

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

