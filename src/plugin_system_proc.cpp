#include "plugin_system_proc.h"
#include <dlfcn.h>      // 新增头文件
#include <string>
#include "utils/basenode_def_internal.h"

namespace BaseNode
{

int PluginLoadManager::Init()
{
    LoadPluginSo_("../lib/libgatesvr.so");
    return 0;
}

int PluginLoadManager::Update()
{
    for (auto& [so_path, handle] : plugin_map_) {
        void* func = dlsym(handle, "Update");
        if (func) {
            ((void (*)(void))func)();
        }
    }
    return 0;
}

int PluginLoadManager::LoadPluginSo_(const std::string& so_path)
{
    // 加载动态库
    void* handle = dlopen(so_path.c_str(), RTLD_LAZY);
    if (!handle) {
        BaseNodeLogError("dlopen error: %s\n", dlerror());
        return -1;
    }
    plugin_map_[so_path] = handle;
    return 0;
}
} // namespace BaseNode