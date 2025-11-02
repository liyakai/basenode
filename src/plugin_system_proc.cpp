#include "plugin_system_proc.h"
#include <string>
#include <filesystem>
#include "utils/basenode_def_internal.h"
#include "tools/safe_call.h"
#if defined(PLATFORM_WINDOWS)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace BaseNode
{

int PluginLoadManager::Init()
{
    std::filesystem::path cwd = std::filesystem::current_path();
    LoadPluginSo_(cwd.string() + "/lib/libgatesvr.so");
    return 0;
}

int PluginLoadManager::Update()
{
    // 自定义错误回调
    auto error_callback = [](const std::string& context, const std::string& error_msg) {
        BaseNodeLogError("XXX ---> SafeCall Error - Plugin: [%s], Error: %s", context.c_str(), error_msg.c_str());
    };

    for (auto& [so_path, handle] : plugin_map_) {
        if (!handle) continue; // 跳过无效 handle
        void* func = GetSymbolAddress_(handle, "Update");
        if (func) {
            auto update_func = reinterpret_cast<PluginUpdateFunc>(func);
            
            // 使用 ToolBox 子库中的 SafeCall 工具安全调用插件函数
            // 捕获异常和信号（SIGSEGV/SIGFPE），使程序能够继续运行
            bool success = ToolBox::SafeCallSimple(update_func, so_path, error_callback);
            
            if (!success) {
                // 注意：由于 siglongjmp 回退栈帧的特性，SafeCall 内部的错误回调
                // 可能不会输出。这里手动调用错误回调，确保能看到错误信息。
                error_callback(so_path, "caught signal or exception");
                
                BaseNodeLogError("Plugin [%s] Update() failed, but continuing...", so_path.c_str());
                fflush(stderr);
            }
        }
    }
    return 0;
}

int PluginLoadManager::Uninit()
{
    for (auto& [so_path, handle] : plugin_map_) {
        CloseDynamicLibrary_(handle);
    }
    return 0;
}

int PluginLoadManager::LoadPluginSo_(const std::string& so_path)
{
    // 加载动态库
    void* handle = LoadDynamicLibrary_(so_path.c_str());
    if (!handle) {
        BaseNodeLogError("dlopen error: %s\n", GetLastLibraryError_().c_str());
        return -1;
    }
    plugin_map_[so_path] = handle;
    return 0;
}

LibHandle PluginLoadManager::LoadDynamicLibrary_(const std::string& so_path)
{
#if defined(PLATFORM_WINDOWS)
    return LoadLibraryA(path.c_str());
#else
    return dlopen(so_path.c_str(), RTLD_LAZY);
#endif
}

void* PluginLoadManager::GetSymbolAddress_(LibHandle handle, const std::string& symbol_name)
{
#if defined(PLATFORM_WINDOWS)
    return reinterpret_cast<void*>(GetProcAddress(handle, symbol_name.c_str()));
#else
    return dlsym(handle, symbol_name.c_str());
#endif
}

void PluginLoadManager::CloseDynamicLibrary_(LibHandle handle)
{
#if defined(PLATFORM_WINDOWS)
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
}

inline std::string PluginLoadManager::GetLastLibraryError_() {
#if defined(PLATFORM_WINDOWS)
    DWORD errorMessageID = ::GetLastError();
    if(errorMessageID == 0)
        return std::string();
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
#else
    const char* err_cstr = dlerror();
    return err_cstr ? std::string(err_cstr) : std::string();
#endif
}





} // namespace BaseNode