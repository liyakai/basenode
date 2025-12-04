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
    
protected:
    virtual ErrorCode DoInit() override;
    virtual ErrorCode DoUpdate() override;
    virtual ErrorCode DoUninit() override;
};

#define PlayerMgr ToolBox::Singleton<Player>::Instance()

} // namespace BaseNode