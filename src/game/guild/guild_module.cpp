#include "guild_module.h"
#include "utils/basenode_def_internal.h"
#include "coro_rpc/impl/stream_rpc.h"
#include "protobuf/pb_out/guild.pb.h"
#include "protobuf/pb_out/errcode.pb.h"
#include "service_discovery/service_discovery_core.h"
#include "service_discovery/zookeeper/zk_service_discovery_module.h"
#include <chrono>
#include <exception>

namespace BaseNode
{

ErrorCode Guild::DoInit()
{
    BaseNodeLogInfo("GuildModule Init");
    // 注册RPC服务函数（直接使用成员函数指针）
    // 同时注册普通版本和协程版本的 OnPlayerLogin
    // 注册流式RPC服务：GetGuildMembersStream 和 GetGuildMemberIdsStream
    // 注册基于 PB 的 RPC 服务：GetGuildInfo、GetGuildInfoCoro
    // 注册基于 PB 的流式 RPC 服务：GetGuildMembersStreamPB
    RegisterService<&Guild::OnPlayerLogin,
                    &Guild::OnPlayerLoginCoro,
                    &Guild::GetGuildMembersStream,
                    &Guild::GetGuildMemberIdsStream,
                    &Guild::GetGuildInfo,
                    &Guild::GetGuildInfoCoro,
                    &Guild::GetGuildMembersStreamPB>(this);


    return ErrorCode::BN_SUCCESS;
}

ErrorCode Guild::DoUpdate()
{
    // BaseNodeLogInfo("GuildModule Update");
    return ErrorCode::BN_SUCCESS;
}

ErrorCode Guild::DoUninit()
{
    BaseNodeLogInfo("GuildModule UnInit");
    return ErrorCode::BN_SUCCESS;
}

ErrorCode Guild::OnPlayerLogin(uint64_t player_id)
{
    BaseNodeLogInfo("GuildModule OnPlayerLogin, player_id: %llu", player_id);
    // 使用协程版本处理玩家登陆逻辑
    OnPlayerLoginCoro(player_id);
    return ErrorCode::BN_SUCCESS;
}

ToolBox::coro::Task<std::monostate> Guild::OnPlayerLoginCoro(uint64_t player_id)
{
    BaseNodeLogInfo("GuildModule OnPlayerLoginCoro with coroutine, player_id: %llu", player_id);

    // TODO: 在这里可以 co_await 其他异步任务 / RPC 调用

    co_return std::monostate{};
}

// 流式RPC服务实现：分批返回公会成员列表
ToolBox::CoroRpc::StreamGenerator<std::string> Guild::GetGuildMembersStream(uint64_t guild_id)
{
    BaseNodeLogInfo("GuildModule GetGuildMembersStream: guild_id: %llu", guild_id);
    
    // 模拟公会成员数据（实际应该从数据库或缓存中获取）
    // 假设有100个成员，每次返回10个
    constexpr int kTotalMembers = 100;
    constexpr int kBatchSize = 10;
    
    for (int batch = 0; batch < kTotalMembers / kBatchSize; ++batch) {
        // 构造一批成员信息（实际应该是结构化的数据，这里用字符串模拟）
        std::string batch_data = "Guild " + std::to_string(guild_id) + " Members Batch " + 
                                 std::to_string(batch + 1) + ": ";
        
        for (int i = 0; i < kBatchSize; ++i) {
            uint64_t member_id = batch * kBatchSize + i + 1;
            batch_data += "Member" + std::to_string(member_id);
            if (i < kBatchSize - 1) {
                batch_data += ", ";
            }
        }
        
        BaseNodeLogInfo("GuildModule GetGuildMembersStream: yielding batch %d, data: %s", 
                       batch + 1, batch_data.c_str());
        
        // 使用 co_yield 返回一批数据
        co_yield batch_data;
        
        // 模拟处理延迟（实际场景中可能是数据库查询或其他IO操作）
        // 这里可以 co_await 其他异步操作
    }
    
    BaseNodeLogInfo("GuildModule GetGuildMembersStream: completed, guild_id: %llu", guild_id);
}

// 流式RPC服务实现：返回公会成员ID列表（数值类型）
ToolBox::CoroRpc::StreamGenerator<uint64_t> Guild::GetGuildMemberIdsStream(uint64_t guild_id)
{
    BaseNodeLogInfo("GuildModule GetGuildMemberIdsStream: guild_id: %llu", guild_id);
    
    // 模拟公会成员ID数据（实际应该从数据库或缓存中获取）
    // 假设有50个成员，逐个返回成员ID
    constexpr int kTotalMembers = 50;
    
    for (int i = 0; i < kTotalMembers; ++i) {
        // 生成成员ID（实际应该从数据库查询）
        uint64_t member_id = guild_id * 10000 + (i + 1);
        
        BaseNodeLogInfo("GuildModule GetGuildMemberIdsStream: yielding member_id: %llu", member_id);
        
        // 使用 co_yield 返回成员ID（数值类型）
        co_yield member_id;
        
        // 模拟处理延迟（实际场景中可能是数据库查询或其他IO操作）
        // 这里可以 co_await 其他异步操作
    }
    
    BaseNodeLogInfo("GuildModule GetGuildMemberIdsStream: completed, guild_id: %llu, total members: %d", 
                   guild_id, kTotalMembers);
}

// 基于 PB 的 RPC 服务实现：获取公会信息
guild::GetGuildInfoResponse Guild::GetGuildInfo(const guild::GetGuildInfoRequest& request)
{
    using errcode::ErrCode;
    
    uint64_t guild_id = request.guild_id();
    BaseNodeLogInfo("GuildModule GetGuildInfo: guild_id: %llu", guild_id);
    
    guild::GetGuildInfoResponse response;
    
    // 输入验证
    if (guild_id == 0) {
        BaseNodeLogError("GuildModule GetGuildInfo: invalid guild_id: %llu", guild_id);
        response.set_ret(static_cast<int32_t>(ErrCode::ERR_GUILD_INVALID_ID));
        return response;
    }
    
    // 模拟从数据库或缓存中获取公会信息
    // TODO: 实际应该从数据库查询
    try {
        guild::Guild* guild = response.mutable_guild();
        if (!guild) {
            BaseNodeLogError("GuildModule GetGuildInfo: failed to create guild object");
            response.set_ret(static_cast<int32_t>(ErrCode::ERR_INTERNAL_ERROR));
            return response;
        }
        
        guild->set_id(guild_id);
        guild->set_name("Guild_" + std::to_string(guild_id));
        
        // 使用当前时间戳（实际应该从数据库获取）
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        guild->set_created_at(timestamp);
        guild->set_is_active(true);
        
        // 设置返回码（使用全服错误码枚举）
        response.set_ret(static_cast<int32_t>(ErrCode::ERR_SUCCESS));
        
        BaseNodeLogInfo("GuildModule GetGuildInfo: completed, guild_id: %llu, guild_name: %s", 
                       guild_id, guild->name().c_str());
    } catch (const std::exception& e) {
        BaseNodeLogError("GuildModule GetGuildInfo: exception occurred: %s", e.what());
        response.set_ret(static_cast<int32_t>(ErrCode::ERR_INTERNAL_ERROR));
    }
    
    return response;
}

// 基于 PB 的协程 RPC 服务实现：获取公会信息（协程版本）
ToolBox::coro::Task<guild::GetGuildInfoResponse> Guild::GetGuildInfoCoro(const guild::GetGuildInfoRequest& request)
{
    using errcode::ErrCode;
    
    uint64_t guild_id = request.guild_id();
    BaseNodeLogInfo("GuildModule GetGuildInfoCoro: guild_id: %llu", guild_id);
    
    guild::GetGuildInfoResponse response;
    
    // 输入验证
    if (guild_id == 0) {
        BaseNodeLogError("GuildModule GetGuildInfoCoro: invalid guild_id: %llu", guild_id);
        response.set_ret(static_cast<int32_t>(ErrCode::ERR_GUILD_INVALID_ID));
        co_return response;
    }
    
    // 模拟异步操作（实际场景中可能是数据库查询或其他IO操作）
    // 这里可以 co_await 其他异步任务
    // TODO: 实际应该从数据库查询，这里使用 co_await 模拟异步操作
    
    // 模拟从数据库或缓存中获取公会信息
    try {
        guild::Guild* guild = response.mutable_guild();
        if (!guild) {
            BaseNodeLogError("GuildModule GetGuildInfoCoro: failed to create guild object");
            response.set_ret(static_cast<int32_t>(ErrCode::ERR_INTERNAL_ERROR));
            co_return response;
        }
        
        guild->set_id(guild_id);
        guild->set_name("Guild_" + std::to_string(guild_id));
        
        // 使用当前时间戳（实际应该从数据库获取）
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        guild->set_created_at(timestamp);
        guild->set_is_active(true);
        
        // 设置返回码（使用全服错误码枚举）
        response.set_ret(static_cast<int32_t>(ErrCode::ERR_SUCCESS));
        
        BaseNodeLogInfo("GuildModule GetGuildInfoCoro: completed, guild_id: %llu, guild_name: %s", 
                       guild_id, guild->name().c_str());
    } catch (const std::exception& e) {
        BaseNodeLogError("GuildModule GetGuildInfoCoro: exception occurred: %s", e.what());
        response.set_ret(static_cast<int32_t>(ErrCode::ERR_INTERNAL_ERROR));
    }
    
    co_return response;
}

// 基于 PB 的流式 RPC 服务实现：分批返回公会成员列表（PB 消息）
ToolBox::CoroRpc::StreamGenerator<guild::GetGuildMembersStreamResponse> Guild::GetGuildMembersStreamPB(const guild::GetGuildMembersStreamRequest& request)
{
    using errcode::ErrCode;
    
    uint64_t guild_id = request.guild_id();
    BaseNodeLogInfo("GuildModule GetGuildMembersStreamPB: guild_id: %llu", guild_id);
    
    // 输入验证
    if (guild_id == 0) {
        BaseNodeLogError("GuildModule GetGuildMembersStreamPB: invalid guild_id: %llu", guild_id);
        guild::GetGuildMembersStreamResponse error_response;
        error_response.set_ret(static_cast<int32_t>(ErrCode::ERR_GUILD_INVALID_ID));
        co_yield error_response;
        co_return;
    }
    
    // 模拟公会成员数据（实际应该从数据库或缓存中获取）
    // 假设有100个成员，每次返回10个
    constexpr int kTotalMembers = 100;
    constexpr int kBatchSize = 10;
    
    int total_batches = (kTotalMembers + kBatchSize - 1) / kBatchSize;  // 向上取整
    
    for (int batch = 0; batch < total_batches; ++batch) {
        guild::GetGuildMembersStreamResponse response;
        
        // 构造一批成员信息
        int start_idx = batch * kBatchSize;
        int end_idx = std::min(start_idx + kBatchSize, kTotalMembers);
        
        for (int i = start_idx; i < end_idx; ++i) {
            guild::Guild* member = response.add_members();
            if (member) {
                uint64_t member_id = guild_id * 10000 + (i + 1);
                member->set_id(member_id);
                member->set_name("Member_" + std::to_string(member_id));
                
                // 使用当前时间戳
                auto now = std::chrono::system_clock::now();
                auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch()).count();
                member->set_created_at(timestamp);
                member->set_is_active(true);
            }
        }
        
        // 如果是最后一批，设置返回码
        if (batch == total_batches - 1) {
            response.set_ret(static_cast<int32_t>(ErrCode::ERR_SUCCESS));
        } else {
            response.set_ret(static_cast<int32_t>(ErrCode::ERR_SUCCESS));  // 中间批次也设置成功码
        }
        
        BaseNodeLogInfo("GuildModule GetGuildMembersStreamPB: yielding batch %d/%d, members: %d", 
                       batch + 1, total_batches, end_idx - start_idx);
        
        // 使用 co_yield 返回一批数据（PB 消息）
        co_yield response;
        
        // 模拟处理延迟（实际场景中可能是数据库查询或其他IO操作）
        // 这里可以 co_await 其他异步操作
    }
    
    BaseNodeLogInfo("GuildModule GetGuildMembersStreamPB: completed, guild_id: %llu", guild_id);
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_INIT() {
    GuildMgr->Init();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UPDATE() {
    GuildMgr->Update();
}

extern "C" SO_EXPORT_SYMBOL void SO_EXPORT_FUNC_UNINIT() {
    GuildMgr->UnInit();  // 调用基类的UnInit方法
}

} // namespace BaseNode