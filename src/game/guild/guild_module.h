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

// 前向声明 PB 消息类型
namespace guild {
class GetGuildInfoRequest;
class GetGuildInfoResponse;
class GetGuildMembersStreamRequest;
class GetGuildMembersStreamResponse;
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
    
    // 流式RPC服务：获取公会成员ID列表（返回数值类型）
    // 返回: StreamGenerator<uint64_t> - 每个值是一个成员ID
    ToolBox::CoroRpc::StreamGenerator<uint64_t> GetGuildMemberIdsStream(uint64_t guild_id);
    
    // 基于 PB 的 RPC 服务：获取公会信息
    // 接收: guild::GetGuildInfoRequest - 包含 guild_id
    // 返回: guild::GetGuildInfoResponse - 包含公会信息和返回码
    guild::GetGuildInfoResponse GetGuildInfo(const guild::GetGuildInfoRequest& request);
    
    // 基于 PB 的协程 RPC 服务：获取公会信息（协程版本）
    // 接收: guild::GetGuildInfoRequest - 包含 guild_id
    // 返回: Task<guild::GetGuildInfoResponse> - 包含公会信息和返回码
    ToolBox::coro::Task<guild::GetGuildInfoResponse> GetGuildInfoCoro(const guild::GetGuildInfoRequest& request);
    
    // 基于 PB 的流式 RPC 服务：获取公会成员列表（分批返回 PB 消息）
    // 接收: guild::GetGuildMembersStreamRequest - 包含 guild_id
    // 返回: StreamGenerator<guild::GetGuildMembersStreamResponse> - 每个消息包含一批成员信息
    ToolBox::CoroRpc::StreamGenerator<guild::GetGuildMembersStreamResponse> GetGuildMembersStreamPB(const guild::GetGuildMembersStreamRequest& request);
    
    
protected:
    virtual ErrorCode DoInit() override;
    virtual ErrorCode DoUpdate() override;
    virtual ErrorCode DoUninit() override;
};

#define GuildMgr ToolBox::Singleton<Guild>::Instance()

} // namespace BaseNode