#include "guild_module.h"
#include "utils/basenode_def_internal.h"

namespace BaseNode
{

ErrorCode Guild::DoInit()
{
    BaseNodeLogInfo("GuildModule Init");
    // 注册RPC服务函数（直接使用成员函数指针）
    // 同时注册普通版本和协程版本的 OnPlayerLogin
    RegisterService<&Guild::OnPlayerLogin, &Guild::OnPlayerLoginCoro>(this);
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