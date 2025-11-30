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

} // namespace BaseNode