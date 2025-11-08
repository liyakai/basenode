#include "guild_module.h"
#include "utils/basenode_def_internal.h"

void Guild::Init()
{
    BaseNodeLogInfo("GuildModule Init");
}

void Guild::Update()
{
    BaseNodeLogInfo("GuildModule Update");
}

void Guild::UnInit()
{
    BaseNodeLogInfo("GuildModule UnInit");
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_INIT() {
    GuildMgr->Init();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UPDATE() {
    GuildMgr->Update();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UNINIT() {
    GuildMgr->UnInit();
}