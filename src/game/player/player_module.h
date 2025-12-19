#pragma once

#include "module_interface.h"
#include "utils/basenode_def_internal.h"
#include "tools/cpp20_coroutine.h"
#include <cstdint>

namespace BaseNode
{
class Player : public IModule
{
public:

    ErrorCode OnLogin(uint64_t player_id);
    // 使用协程方式调用 RPC 的示例（调用 Guild::OnPlayerLogin）
    ToolBox::coro::Task<std::monostate> OnLoginCoroutine(uint64_t player_id);
    // 使用协程方式直接调用 Guild::OnPlayerLoginCoro 的示例
    ToolBox::coro::Task<std::monostate> OnLoginCoroutineWithGuildCoro(uint64_t player_id);
    // 使用流式RPC获取公会成员列表的示例
    ToolBox::coro::Task<std::monostate> FetchGuildMembers(uint64_t guild_id);
    // 使用流式RPC获取公会成员ID列表的示例（返回数值类型）
    ToolBox::coro::Task<std::monostate> FetchGuildMemberIds(uint64_t guild_id);
    // 使用基于 PB 的 RPC 获取公会信息的示例
    ToolBox::coro::Task<std::monostate> GetGuildInfoByPB(uint64_t guild_id);
    // 使用基于 PB 的协程 RPC 获取公会信息的示例
    ToolBox::coro::Task<std::monostate> GetGuildInfoByPBCoro(uint64_t guild_id);
    // 使用基于 PB 的流式 RPC 获取公会成员列表的示例
    ToolBox::coro::Task<std::monostate> FetchGuildMembersByPB(uint64_t guild_id);
    
    // 使用 Zookeeper 服务发现 + RequestContext 选择 Guild 实例，再调用 PB RPC 的示例
    ToolBox::coro::Task<std::monostate> GetGuildInfoByServiceDiscovery(uint64_t guild_id);
    
protected:
    virtual ErrorCode DoInit() override;
    virtual ErrorCode DoUpdate() override;
    virtual ErrorCode DoUninit() override;

private:
    // 示例：如果你后续希望缓存 Invoker，可以在这里存一份
    // BaseNode::ServiceDiscovery::IInvokerPtr guild_info_invoker_;
};

#define PlayerMgr ToolBox::Singleton<Player>::Instance()

} // namespace BaseNode