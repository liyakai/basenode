
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

#include "plugin_system_proc.h"

// 全局退出标志
static std::atomic<bool> g_running(true);

// 信号处理函数
void signalHandler(int signal)
{
    if (signal == SIGINT) {
        g_running = false;
    }
}

int processNode();

int main() 
{
    processNode();
    return 0;
}


int processNode()
{
    // 注册 SIGINT 信号处理器（Ctrl+C）
    signal(SIGINT, signalHandler);
    PluginLoadMgr->Init();
    while (g_running) {
        PluginLoadMgr->Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    PluginLoadMgr->Uninit();
    return 0;
}