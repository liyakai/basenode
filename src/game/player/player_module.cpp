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
    // 异步调用Guild模块的OnPlayerLogin RPC服务（直接使用成员函数指针）
    CallModuleService<&Guild::OnPlayerLogin>(player_id).then([](auto result) {
        BaseNodeLogInfo("PlayerModule OnLogin: Guild::OnPlayerLogin completed, result: %d", static_cast<int>(*result));
    });
    return ErrorCode::BN_SUCCESS;
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