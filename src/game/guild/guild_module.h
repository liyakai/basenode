#pragma once


#include "module_interface.h"

namespace BaseNode
{
class Guild : public IModule
{
public:
    virtual void Init() override;
    virtual void UnInit() override;
    
protected:
    virtual void DoUpdate() override;
};

#define GuildMgr ToolBox::Singleton<Guild>::Instance()

} // namespace BaseNode