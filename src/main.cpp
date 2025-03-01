#include "plugin_system_proc.h"
#include <thread>
#include <chrono>

int processNode();

int main() 
{
    processNode();
    return 0;
}


int processNode()
{
    PluginLoadMgr->Init();
    while (true) {
        PluginLoadMgr->Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    return 0;
}