#pragma once

#include "module_interface.h"
#include "tools/cpp20_coroutine.h"
#include <cstdint>
#include <string>

// 前向声明
namespace ToolBox::CoroRpc {
template<typename T>
class StreamGenerator;
}

namespace BaseNode
{
class Guild : public IModule
{
public:

    ErrorCode OnPlayerLogin(uint64_t player_id);
    // 使用协程方式处理玩家登录的示例
    ToolBox::coro::Task<std::monostate> OnPlayerLoginCoro(uint64_t player_id);
    
    // 流式RPC服务：获取公会成员列表（分批返回）
    // 返回: StreamGenerator<std::string> - 每个字符串包含一批成员信息
    ToolBox::CoroRpc::StreamGenerator<std::string> GetGuildMembersStream(uint64_t guild_id);
    
protected:
    virtual ErrorCode DoInit() override;
    virtual ErrorCode DoUpdate() override;
    virtual ErrorCode DoUninit() override;
};

#define GuildMgr ToolBox::Singleton<Guild>::Instance()

} // namespace BaseNode