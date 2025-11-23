#pragma once

#include "module_interface.h"
#include "utils/basenode_def_internal.h"

namespace BaseNode
{
class Player : public IModule
{
public:
    virtual void Init() override;
    virtual void UnInit() override;
    
protected:
    virtual void DoUpdate() override;
};

#define PlayerMgr ToolBox::Singleton<Player>::Instance()

} // namespace BaseNode