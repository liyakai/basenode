
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstring>

#include "plugin_system_proc.h"
#include "config/config_manager.h"

// 全局退出标志
static std::atomic<bool> g_running(true);

// 信号处理函数
void signalHandler(int signal)
{
    if (signal == SIGINT) {
        g_running = false;
    }
}

int processNode(const std::string& config_file);

int main(int argc, char* argv[]) 
{
    // 支持通过命令行参数指定配置文件
    // 用法: ./basenode [config_file]
    // 例如: ./basenode config/basenode.json
    //       ./basenode config/gatenode.json
    std::string config_file = "config/basenode.json";  // 默认配置文件
    
    if (argc > 1) {
        config_file = argv[1];
    }
    
    processNode(config_file);
    return 0;
}


int processNode(const std::string& config_file)
{
    // 注册 SIGINT 信号处理器（Ctrl+C）
    signal(SIGINT, signalHandler);

    // 加载配置（配置名称会自动从文件名提取）
    if (!ConfigMgr->LoadConfigFromFile(config_file)) {
        fprintf(stderr, "Failed to load config file: %s\n", config_file.c_str());
        return -1;
    }

    PluginLoadMgr->Init();
    while (g_running) {
        PluginLoadMgr->Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    PluginLoadMgr->Uninit();
    return 0;
}