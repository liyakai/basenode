#include "guild_module.h"
#include "utils/basenode_def_internal.h"

namespace BaseNode
{

ErrorCode Guild::DoInit()
{
    BaseNodeLogInfo("GuildModule Init");
    // 注册RPC服务函数（直接使用成员函数指针）
    RegisterService<&Guild::OnPlayerLogin>(GuildMgr);
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
    return ErrorCode::BN_SUCCESS;
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