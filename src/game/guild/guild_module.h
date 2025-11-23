#pragma once


#include "module_interface.h"

namespace BaseNode
{
class Guild : public IModule
{
public:
    virtual void UnInit() override;
    
protected:
    virtual void DoInit() override;
    virtual void DoUpdate() override;
};

#define GuildMgr ToolBox::Singleton<Guild>::Instance()

} // namespace BaseNode