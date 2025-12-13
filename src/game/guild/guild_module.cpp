#include "guild_module.h"
#include "utils/basenode_def_internal.h"
#include "coro_rpc/impl/stream_rpc.h"

namespace BaseNode
{

ErrorCode Guild::DoInit()
{
    BaseNodeLogInfo("GuildModule Init");
    // 注册RPC服务函数（直接使用成员函数指针）
    // 同时注册普通版本和协程版本的 OnPlayerLogin
    // 注册流式RPC服务：GetGuildMembersStream
    RegisterService<&Guild::OnPlayerLogin, &Guild::OnPlayerLoginCoro, &Guild::GetGuildMembersStream>(this);
    return ErrorCode::BN_SUCCESS;
}

ErrorCode Guild::DoUpdate()
{
    BaseNodeLogInfo("GuildModule Update");
    return ErrorCode::BN_SUCCESS;
}

ErrorCode Guild::DoUninit()
{
    BaseNodeLogInfo("GuildModule UnInit");
    return ErrorCode::BN_SUCCESS;
}

ErrorCode Guild::OnPlayerLogin(uint64_t player_id)
{
    BaseNodeLogInfo("GuildModule OnPlayerLogin, player_id: %llu", player_id);
    // 使用协程版本处理玩家登陆逻辑
    OnPlayerLoginCoro(player_id);
    return ErrorCode::BN_SUCCESS;
}

ToolBox::coro::Task<std::monostate> Guild::OnPlayerLoginCoro(uint64_t player_id)
{
    BaseNodeLogInfo("GuildModule OnPlayerLoginCoro with coroutine, player_id: %llu", player_id);

    // TODO: 在这里可以 co_await 其他异步任务 / RPC 调用

    co_return std::monostate{};
}

// 流式RPC服务实现：分批返回公会成员列表
ToolBox::CoroRpc::StreamGenerator<std::string> Guild::GetGuildMembersStream(uint64_t guild_id)
{
    BaseNodeLogInfo("GuildModule GetGuildMembersStream: guild_id: %llu", guild_id);
    
    // 模拟公会成员数据（实际应该从数据库或缓存中获取）
    // 假设有100个成员，每次返回10个
    constexpr int kTotalMembers = 100;
    constexpr int kBatchSize = 10;
    
    for (int batch = 0; batch < kTotalMembers / kBatchSize; ++batch) {
        // 构造一批成员信息（实际应该是结构化的数据，这里用字符串模拟）
        std::string batch_data = "Guild " + std::to_string(guild_id) + " Members Batch " + 
                                 std::to_string(batch + 1) + ": ";
        
        for (int i = 0; i < kBatchSize; ++i) {
            uint64_t member_id = batch * kBatchSize + i + 1;
            batch_data += "Member" + std::to_string(member_id);
            if (i < kBatchSize - 1) {
                batch_data += ", ";
            }
        }
        
        BaseNodeLogInfo("GuildModule GetGuildMembersStream: yielding batch %d, data: %s", 
                       batch + 1, batch_data.c_str());
        
        // 使用 co_yield 返回一批数据
        co_yield batch_data;
        
        // 模拟处理延迟（实际场景中可能是数据库查询或其他IO操作）
        // 这里可以 co_await 其他异步操作
    }
    
    BaseNodeLogInfo("GuildModule GetGuildMembersStream: completed, guild_id: %llu", guild_id);
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_INIT() {
    GuildMgr->Init();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UPDATE() {
    GuildMgr->Update();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UNINIT() {
    GuildMgr->UnInit();  // 调用基类的UnInit方法
}

} // namespace BaseNode