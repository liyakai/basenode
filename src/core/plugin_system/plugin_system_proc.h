#pragma once
#include <string_view>
#include <map>
#include "tools/singleton.h"

#if defined(PLATFORM_WINDOWS)
    using LibHandle = HMODULE;
#else
    using LibHandle = void*;
#endif

// 函数指针类型定义
using PluginUpdateFunc = void (*)(void);

namespace BaseNode
{
class PluginLoadManager
{
public:
    int Init();
    int Update();
    int Uninit();

private:
    int LoadPluginSo_(const std::string& so_path);
    int AfterAllModulesInit_();

    LibHandle LoadDynamicLibrary_(const std::string& so_path);
    void* GetSymbolAddress_(LibHandle handle, const std::string& symbol_name);
    void CloseDynamicLibrary_(LibHandle handle, const std::string& so_path);
    std::string GetLastLibraryError_();
    void SafeCallSimple_(void* handle, const std::string& so_path, const std::string& symbol_name);

private:
    std::map<std::string, void*> plugin_map_;
};
} // namespace BaseNode

#define PluginLoadMgr ToolBox::Singleton<BaseNode::PluginLoadManager>::Instance()