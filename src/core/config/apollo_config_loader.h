#pragma once

#include "config_loader.h"
#include "config_value.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

namespace BaseNode::Config
{

/**
 * @brief Apollo 配置中心加载器
 * 
 * 支持从 Apollo 配置中心加载配置
 * 支持配置变更监听和热更新
 */
class ApolloConfigLoader final : public IHotReloadConfigLoader
{
public:
    /**
     * @brief 构造函数
     * @param config_server_url Apollo 配置服务器地址，如 "http://localhost:8080"
     * @param app_id 应用ID
     * @param cluster 集群名称，默认为 "default"
     * @param namespace_name 命名空间，默认为 "application"
     */
    explicit ApolloConfigLoader(
        const std::string& config_server_url,
        const std::string& app_id,
        const std::string& cluster = "default",
        const std::string& namespace_name = "application");

    ~ApolloConfigLoader() override;

    std::string GetName() const override { return "ApolloConfigLoader"; }

    std::vector<std::string> GetSupportedFormats() const override
    {
        return {"apollo"};
    }

    ConfigValue Load(const std::string& source) override;

    bool IsAvailable(const std::string& source) const override;

    void RegisterChangeCallback(ConfigChangeCallback callback) override
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback_ = callback;
    }

    bool StartWatch(const std::string& source) override;

    void StopWatch(const std::string& source) override;

private:
    /**
     * @brief 从 Apollo 获取配置
     * @param namespace_name 命名空间
     * @return 配置值对象（ConfigValue，Apollo 返回 JSON 格式）
     */
    ConfigValue FetchFromApollo(const std::string& namespace_name);

    /**
     * @brief 解析 Apollo 配置格式（key=value 格式）
     * @param content 配置内容
     * @return 配置值对象（ConfigValue，转换为 nlohmann::json）
     */
    ConfigValue ParseApolloContent(const std::string& content);

    /**
     * @brief 监听配置变更的线程函数
     */
    void WatchThreadFunc();

    /**
     * @brief HTTP GET 请求（简化实现）
     * @param url 请求URL
     * @return 响应内容
     */
    std::string HttpGet(const std::string& url);

    std::string config_server_url_;
    std::string app_id_;
    std::string cluster_;
    std::string default_namespace_;

    ConfigChangeCallback callback_;
    std::mutex callback_mutex_;

    std::string watching_namespace_;
    std::atomic<bool> watching_{false};
    std::thread watch_thread_;
    std::mutex watch_mutex_;
};

} // namespace BaseNode::Config

