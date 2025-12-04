#pragma once

#include "module_interface.h"
#include "tools/cpp20_coroutine.h"

namespace BaseNode
{
class Guild : public IModule
{
public:

    ErrorCode OnPlayerLogin(uint64_t player_id);
    // 使用协程方式处理玩家登录的示例
    ToolBox::coro::Task<std::monostate> OnPlayerLoginCoro(uint64_t player_id);
    
protected:
    virtual ErrorCode DoInit() override;
    virtual ErrorCode DoUpdate() override;
    virtual ErrorCode DoUninit() override;
};

#define GuildMgr ToolBox::Singleton<Guild>::Instance()

} // namespace BaseNode