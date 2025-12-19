#pragma once

#include "service_discovery_core.h"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace BaseNode::ServiceDiscovery
{

struct InvokeResult
{
    bool        success{false};
    int         status_code{0}; ///< 可映射为业务错误码 / HTTP 状态码 / RPC 错误码等
    std::string body;           ///< 可用于返回序列化后的响应体
};

/**
 * @brief 通用调用接口
 *
 * 不直接依赖具体 RPC 框架（例如 coro_rpc），而是接收一个“如何调用某个 ServiceInstance” 的函数对象，
 * 从而保持和 RPC 实现解耦。
 */
class IInvoker
{
public:
    virtual ~IInvoker() = default;

    virtual InvokeResult
    Invoke(const std::string &service_name,
           const RequestContext &ctx,
           int timeout_ms) = 0;
};

/// 实际发起 RPC 调用的函数类型
using DoCallFunc = std::function<InvokeResult(const ServiceInstance &instance,
                                              int timeout_ms)>;

/**
 * @brief 基础 Invoker：只做一次服务发现 + 一次调用，不包含重试/熔断等策略
 */
class SimpleInvoker final : public IInvoker
{
public:
    SimpleInvoker(IDiscoveryClientPtr discovery_client,
                  DoCallFunc          do_call)
        : discovery_client_(std::move(discovery_client))
        , do_call_(std::move(do_call))
    {
    }

    InvokeResult Invoke(const std::string &service_name,
                        const RequestContext &ctx,
                        int timeout_ms) override
    {
        if (!discovery_client_ || !do_call_)
        {
            return {false, -1, "invoker not initialized"};
        }

        auto instance = discovery_client_->ChooseInstance(service_name, ctx);
        if (!instance)
        {
            return {false, -2, "no available instance"};
        }
        return do_call_(*instance, timeout_ms);
    }

private:
    IDiscoveryClientPtr discovery_client_;
    DoCallFunc          do_call_;
};

/**
 * @brief 带重试的 Invoker 装饰器
 */
class RetryInvoker final : public IInvoker
{
public:
    RetryInvoker(std::shared_ptr<IInvoker> inner, int max_retries)
        : inner_(std::move(inner))
        , max_retries_(max_retries)
    {
    }

    InvokeResult Invoke(const std::string &service_name,
                        const RequestContext &ctx,
                        int timeout_ms) override
    {
        if (!inner_)
        {
            return {false, -1, "inner invoker null"};
        }
        InvokeResult last;
        for (int i = 0; i <= max_retries_; ++i)
        {
            last = inner_->Invoke(service_name, ctx, timeout_ms);
            if (last.success)
            {
                break;
            }
        }
        return last;
    }

private:
    std::shared_ptr<IInvoker> inner_;
    int                       max_retries_{0};
};

/**
 * @brief 简化版熔断器装饰器
 */
class CircuitBreakerInvoker final : public IInvoker
{
public:
    CircuitBreakerInvoker(std::shared_ptr<IInvoker> inner,
                          int failure_threshold,
                          std::chrono::milliseconds open_interval)
        : inner_(std::move(inner))
        , failure_threshold_(failure_threshold)
        , open_interval_(open_interval)
    {
    }

    InvokeResult Invoke(const std::string &service_name,
                        const RequestContext &ctx,
                        int timeout_ms) override
    {
        const auto now = std::chrono::steady_clock::now();

        if (state_ == State::OPEN &&
            now - last_open_time_ < open_interval_)
        {
            return {false, -3, "circuit open"};
        }

        if (!inner_)
        {
            return {false, -1, "inner invoker null"};
        }

        // 允许一次尝试
        auto result = inner_->Invoke(service_name, ctx, timeout_ms);
        if (!result.success)
        {
            ++consecutive_failures_;
            if (consecutive_failures_ >= failure_threshold_)
            {
                state_ = State::OPEN;
                last_open_time_ = now;
            }
        }
        else
        {
            consecutive_failures_ = 0;
            state_ = State::CLOSED;
        }
        return result;
    }

private:
    enum class State
    {
        CLOSED,
        OPEN
    };

    std::shared_ptr<IInvoker>  inner_;
    int                        failure_threshold_{0};
    std::chrono::milliseconds  open_interval_{0};
    int                        consecutive_failures_{0};
    State                      state_{State::CLOSED};
    std::chrono::steady_clock::time_point last_open_time_{};
};

using IInvokerPtr = std::shared_ptr<IInvoker>;

} // namespace BaseNode::ServiceDiscovery


