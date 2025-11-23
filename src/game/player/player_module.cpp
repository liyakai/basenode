#include "player_module.h"
#include "utils/basenode_def_internal.h"

namespace BaseNode
{
void Player::Init()
{
    BaseNodeLogInfo("PlayerModule Init");
}

void Player::DoUpdate()
{
    BaseNodeLogInfo("PlayerModule Update");
}

void Player::UnInit()
{
    BaseNodeLogInfo("PlayerModule UnInit");
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