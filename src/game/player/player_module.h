#pragma once

#include "module_interface.h"
#include "utils/basenode_def_internal.h"
#include <cstdint>

namespace BaseNode
{
class Player : public IModule
{
public:

    ErrorCode OnLogin(uint64_t player_id);
    
protected:
    virtual ErrorCode DoInit() override;
    virtual ErrorCode DoUpdate() override;
    virtual ErrorCode DoUninit() override;
};

#define PlayerMgr ToolBox::Singleton<Player>::Instance()

} // namespace BaseNode