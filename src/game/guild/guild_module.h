#pragma once


#include "module_interface.h"

namespace BaseNode
{
class Guild : public IModule
{
public:

ErrorCode OnPlayerLogin(uint64_t player_id);
    
protected:
    virtual ErrorCode DoInit() override;
    virtual ErrorCode DoUpdate() override;
    virtual ErrorCode DoUninit() override;
};

#define GuildMgr ToolBox::Singleton<Guild>::Instance()

// RPC包装函数，用于跨模块调用
inline ErrorCode Guild_OnPlayerLogin(uint64_t player_id) {
    return GuildMgr->OnPlayerLogin(player_id);
}

} // namespace BaseNode