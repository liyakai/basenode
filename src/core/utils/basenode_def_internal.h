#pragma once

#include "tools/log.h"

namespace BaseNode
{

#define SO_EXPORT_SYMBOL __attribute__((visibility("default")))
#define SO_EXPORT_FUNC_INIT initSo
#define SO_EXPORT_FUNC_UPDATE updateSo
#define SO_EXPORT_FUNC_UNINIT uninitSo

// 定义 网络库日志接口
#ifndef BaseNodeLogTrace
#define BaseNodeLogTrace(LogFormat, ...)     LogTrace(LogFormat, ## __VA_ARGS__)
#endif  // BaseNodeLogTrace
#ifndef BaseNodeLogDebug
#define BaseNodeLogDebug(LogFormat, ...)     LogDebug(LogFormat, ## __VA_ARGS__)
#endif  // BaseNodeLogDebug
#ifndef BaseNodeLogInfo
#define BaseNodeLogInfo(LogFormat, ...)      LogInfo(LogFormat, ## __VA_ARGS__)
#endif  // BaseNodeLogInfo
#ifndef BaseNodeLogWarn
#define BaseNodeLogWarn(LogFormat, ...)      LogWarn(LogFormat, ## __VA_ARGS__)
#endif  // BaseNodeLogWarn
#ifndef BaseNodeLogError
#define BaseNodeLogError(LogFormat, ...)     LogError(LogFormat, ## __VA_ARGS__)
#endif  // BaseNodeLogError
#ifndef BaseNodeLogFatal
#define BaseNodeLogFatal(LogFormat, ...)     LogFatal(LogFormat, ## __VA_ARGS__)
#endif  // BaseNodeLogFatal

enum class ErrorCode : int32_t
{
    BN_SUCCESS = 0,
    BN_INVALID_ARGUMENTS = 1,
    BN_SEND_BUFF_OVERFLOW = 2,   // 发送缓冲区满
    BN_RECV_BUFF_OVERFLOW = 3,   // 接收缓冲区满
    BN_SET_SEND_CALLBACK_FAILED = 4,   // 设置发送回调失败
    BN_SERVICE_ID_NOT_FOUND = 5,   // 服务ID未找到
};

} // namespace BaseNode