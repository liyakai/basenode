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

}