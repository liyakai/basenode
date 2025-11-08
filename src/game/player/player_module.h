#pragma once

#include "module_interface.h"
#include "utils/basenode_def_internal.h"

class Player : public IModule
{
public:
    virtual void Init() override;
    virtual void Update() override;
    virtual void UnInit() override;
};

#define PlayerMgr ToolBox::Singleton<Player>::Instance()
