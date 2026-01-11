#include "apollo_config_loader.h"
#include "config_value.h"
#include "utils/basenode_def_internal.h"
#include <sstream>
#include <fstream>

// 注意：这是一个简化实现，实际生产环境建议使用 HTTP 客户端库如 curl 或 libcurl
// 这里提供一个基础框架，可以根据实际需求扩展

namespace BaseNode::Config
{

ApolloConfigLoader::ApolloConfigLoader(
    const std::string& config_server_url,
    const std::string& app_id,
    const std::string& cluster,
    const std::string& namespace_name)
    : config_server_url_(config_server_url)
    , app_id_(app_id)
    , cluster_(cluster)
    , default_namespace_(namespace_name)
{
}

ApolloConfigLoader::~ApolloConfigLoader()
{
    StopWatch("");
}

ConfigValue ApolloConfigLoader::Load(const std::string& source)
{
    std::string namespace_name = source.empty() ? default_namespace_ : source;
    return FetchFromApollo(namespace_name);
}

bool ApolloConfigLoader::IsAvailable(const std::string& source) const
{
    // 简化实现：检查配置服务器是否可达
    // 实际应该发送 HTTP 请求验证
    return !config_server_url_.empty() && !app_id_.empty();
}

bool ApolloConfigLoader::StartWatch(const std::string& source)
{
    std::lock_guard<std::mutex> lock(watch_mutex_);
    
    if (watching_)
    {
        BaseNodeLogWarn("[ApolloConfigLoader] Already watching namespace: %s", watching_namespace_.c_str());
        return false;
    }

    watching_namespace_ = source.empty() ? default_namespace_ : source;
    watching_ = true;

    // 启动监听线程
    watch_thread_ = std::thread(&ApolloConfigLoader::WatchThreadFunc, this);

    BaseNodeLogInfo("[ApolloConfigLoader] Started watching namespace: %s", watching_namespace_.c_str());
    return true;
}

void ApolloConfigLoader::StopWatch(const std::string& /*source*/)
{
    {
        std::lock_guard<std::mutex> lock(watch_mutex_);
        if (!watching_) return;
        watching_ = false;
    }

    if (watch_thread_.joinable())
    {
        watch_thread_.join();
    }

    BaseNodeLogInfo("[ApolloConfigLoader] Stopped watching");
}

ConfigValue ApolloConfigLoader::FetchFromApollo(const std::string& namespace_name)
{
    // 构建 Apollo 配置查询 URL
    // 格式: {config_server_url}/configs/{appId}/{clusterName}/{namespaceName}
    std::ostringstream url_stream;
    url_stream << config_server_url_ << "/configs/" << app_id_ << "/" << cluster_ << "/" << namespace_name;

    std::string url = url_stream.str();
    BaseNodeLogDebug("[ApolloConfigLoader] Fetching config from: %s", url.c_str());

    // 发送 HTTP 请求获取配置
    std::string content = HttpGet(url);
    if (content.empty())
    {
        BaseNodeLogError("[ApolloConfigLoader] Failed to fetch config from Apollo");
        return ConfigValue(std::in_place_type<nlohmann::json>);
    }

    return ParseApolloContent(content);
}

ConfigValue ApolloConfigLoader::ParseApolloContent(const std::string& content)
{
    // Apollo 返回的配置通常是 key=value 格式，每行一个配置项
    // 也可能返回 JSON 格式（取决于配置类型）
    
    // 尝试解析为 JSON
    if (!content.empty() && (content[0] == '{' || content[0] == '['))
    {
        try
        {
            return ConfigValue(nlohmann::json::parse(content));
        }
        catch (...)
        {
            BaseNodeLogWarn("[ApolloConfigLoader] Failed to parse as JSON, trying key=value format");
        }
    }

    // 解析 key=value 格式
    nlohmann::json obj = nlohmann::json::object();
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line))
    {
        // 去除首尾空白
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        if (line.empty() || line[0] == '#') continue;

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        // 去除 key 和 value 的空白
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // 尝试解析值的类型
        if (value == "true" || value == "True")
        {
            obj[key] = true;
        }
        else if (value == "false" || value == "False")
        {
            obj[key] = false;
        }
        else
        {
            try
            {
                if (value.find('.') != std::string::npos)
                {
                    obj[key] = std::stod(value);
                }
                else
                {
                    obj[key] = std::stoll(value);
                }
            }
            catch (...)
            {
                obj[key] = value;
            }
        }
    }

    return ConfigValue(obj);
}

void ApolloConfigLoader::WatchThreadFunc()
{
    ConfigValue last_config;

    while (watching_)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 每5秒检查一次

        if (!watching_) break;

        std::string namespace_name;
        {
            std::lock_guard<std::mutex> lock(watch_mutex_);
            namespace_name = watching_namespace_;
        }

        // 获取最新配置
        ConfigValue current_config = FetchFromApollo(namespace_name);
        
        // 检查配置是否有效
        bool is_valid = std::visit([](auto&& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, nlohmann::json>)
            {
                return !arg.is_null();
            }
            else if constexpr (std::is_same_v<T, YAML::Node>)
            {
                return arg.IsDefined() && !arg.IsNull();
            }
            return false;
        }, current_config);
        
        if (!is_valid) continue;

        // 比较配置是否变化（Apollo 返回 JSON 格式）
        if (std::holds_alternative<nlohmann::json>(last_config) && 
            std::holds_alternative<nlohmann::json>(current_config))
        {
            const auto& last_json = std::get<nlohmann::json>(last_config);
            const auto& current_json = std::get<nlohmann::json>(current_config);
            
            if (!last_json.is_null() && last_json.is_object() && current_json.is_object())
            {
                // 检查变更
                for (auto it = current_json.begin(); it != current_json.end(); ++it)
                {
                    std::string key = it.key();
                    nlohmann::json new_value = it.value();
                    
                    if (!last_json.contains(key) || last_json[key] != new_value)
                    {
                        // 配置变更，触发回调
                        std::lock_guard<std::mutex> lock(callback_mutex_);
                        if (callback_)
                        {
                            callback_(key, ConfigValue(new_value));
                        }
                    }
                }
            }
        }

        // 使用移动语义，因为 variant 可能包含不可拷贝的类型
        last_config = std::move(current_config);
    }
}

std::string ApolloConfigLoader::HttpGet(const std::string& url)
{
    // 注意：这是一个占位实现
    // 实际应该使用 HTTP 客户端库（如 libcurl）发送 HTTP GET 请求
    // 这里提供一个框架，可以根据项目需求集成实际的 HTTP 库

    BaseNodeLogWarn("[ApolloConfigLoader] HttpGet not implemented, please integrate HTTP client library");
    
    // 示例：如果使用 curl，可以这样实现：
    // std::ostringstream response;
    // CURL* curl = curl_easy_init();
    // if (curl) {
    //     curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    //     curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    //     curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    //     CURLcode res = curl_easy_perform(curl);
    //     curl_easy_cleanup(curl);
    //     if (res == CURLE_OK) {
    //         return response.str();
    //     }
    // }

    // 临时实现：尝试从本地文件读取（用于测试）
    // 实际应该替换为真实的 HTTP 请求
    std::ifstream file(url);
    if (file.is_open())
    {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    return "";
}

} // namespace BaseNode::Config



