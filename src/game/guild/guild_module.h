#pragma once


#include "module_interface.h"

class Guild : public IModule
{
public:
    virtual void Init() override;
    virtual void Update() override;
    virtual void UnInit() override;
};

#define GuildMgr ToolBox::Singleton<Guild>::Instance()