#include "player_module.h"
#include "guild/guild_module.h"

namespace BaseNode
{
ErrorCode Player::DoInit()
{
    BaseNodeLogInfo("PlayerModule Init");
    return ErrorCode::BN_SUCCESS;
}

ErrorCode Player::DoUpdate()
{
    OnLogin(10001);
    BaseNodeLogInfo("PlayerModule Update");
    return ErrorCode::BN_SUCCESS;
}

ErrorCode Player::DoUninit()
{
    BaseNodeLogInfo("PlayerModule UnInit");
    return ErrorCode::BN_SUCCESS;
}

ErrorCode Player::OnLogin(uint64_t player_id)
{
    BaseNodeLogInfo("PlayerModule OnLogin, player_id: %llu", player_id);
    // // 异步调用Guild模块的OnPlayerLogin RPC服务（直接使用成员函数指针）
    // CallModuleService<&Guild::OnPlayerLogin>(player_id).then([](auto result) {
    //     BaseNodeLogInfo("PlayerModule OnLogin: Guild::OnPlayerLogin completed, version 1, result: %d", static_cast<int>(*result));
    // });

    // // 异步调用Guild模块的协程版 OnPlayerLoginCoro RPC 服务
    // CallModuleService<&Guild::OnPlayerLoginCoro>(player_id).then([](auto) {
    //     BaseNodeLogInfo("PlayerModule OnLogin: Guild::OnPlayerLoginCoro completed, version 2");
    // });

    // // 使用协程方式调用 Guild 模块的 OnPlayerLogin RPC 服务
    // OnLoginCoroutine(player_id);
    // // 使用协程方式直接调用 Guild 模块的 OnPlayerLoginCoro RPC 服务
    // OnLoginCoroutineWithGuildCoro(player_id);
    // 使用流式RPC获取公会成员列表
    FetchGuildMembers(1001).then([](auto) {
        BaseNodeLogInfo("PlayerModule OnLogin: FetchGuildMembers completed");
    });

    
    return ErrorCode::BN_SUCCESS;
}

ToolBox::coro::Task<std::monostate> Player::OnLoginCoroutine(uint64_t player_id)
{
    BaseNodeLogInfo("PlayerModule OnLoginCoroutine with coroutine, player_id: %llu", player_id);

    // 协程方式调用 RPC：co_await 模块间 RPC 调用结果（调用普通版本 OnPlayerLogin）
    auto result = co_await CallModuleService<&Guild::OnPlayerLogin>(player_id);
    BaseNodeLogInfo("PlayerModule OnLoginCoroutine  with coroutine: Guild::OnPlayerLogin completed, version 3, result: %d", static_cast<int>(*result));

    co_return std::monostate{};
}

ToolBox::coro::Task<std::monostate> Player::OnLoginCoroutineWithGuildCoro(uint64_t player_id)
{
    BaseNodeLogInfo("PlayerModule OnLoginCoroutineWithGuildCoro with coroutine, player_id: %llu", player_id);

    // 协程方式直接调用 Guild::OnPlayerLoginCoro 的 RPC 接口
    co_await CallModuleService<&Guild::OnPlayerLoginCoro>(player_id);
    BaseNodeLogInfo("PlayerModule OnLoginCoroutineWithGuildCoro: Guild::OnPlayerLoginCoro completed, version 4");

    co_return std::monostate{};
}

// 流式RPC调用示例：获取公会成员列表
ToolBox::coro::Task<std::monostate> Player::FetchGuildMembers(uint64_t guild_id)
{
    BaseNodeLogInfo("PlayerModule FetchGuildMembers: starting stream RPC for guild_id: %llu", guild_id);
    
    // 调用流式RPC服务，返回StreamReader
    auto reader = co_await CallModuleServiceStream<&Guild::GetGuildMembersStream>(guild_id);
    
    if (!reader) {
        BaseNodeLogError("PlayerModule FetchGuildMembers: failed to start stream RPC for guild_id: %llu", guild_id);
        co_return std::monostate{};
    }
    
    BaseNodeLogInfo("PlayerModule FetchGuildMembers: stream RPC started, receiving data...");
    
    int batch_count = 0;
    // 持续接收流式数据
    while (!reader->IsFinished()) {
        // 使用 co_await 接收数据
        auto value = co_await *reader;
        
        if (value.has_value()) {
            batch_count++;
            BaseNodeLogInfo("PlayerModule FetchGuildMembers: received batch %d, data: %s", 
                           batch_count, value->c_str());
        } else {
            // 流结束或出错
            BaseNodeLogInfo("PlayerModule FetchGuildMembers: stream ended or error");
            break;
        }
    }
    
    // 检查是否有错误
    if (auto err = reader->GetError()) {
        BaseNodeLogError("PlayerModule FetchGuildMembers: stream error: %d", static_cast<int>(*err));
    } else {
        BaseNodeLogInfo("PlayerModule FetchGuildMembers: completed successfully, received %d batches for guild_id: %llu", 
                       batch_count, guild_id);
    }
    
    co_return std::monostate{};
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_INIT() {
    PlayerMgr->Init();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UPDATE() {
    PlayerMgr->Update();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UNINIT() {
    PlayerMgr->UnInit();
}

} // namespace BaseNode