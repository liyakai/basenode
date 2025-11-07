#include "tools/plugin_system.h"
#include "utils/basenode_def_internal.h"
// 1. 声明导出符号
#define PLUGIN_EXPORT __attribute__((visibility("default")))

namespace BaseNode
{

class PluginGate : public ToolBox::PluginInterface
{
public:
    void pluginUpdate()
    {
        BaseNodeLogInfo("PluginGate pluginUpdate");
    };
};

} // namespace BaseNode

#define PluginGateMgr ToolBox::Singleton<BaseNode::PluginGate>::Instance()

// 4. 定义C接口导出符号
// extern "C" PLUGIN_EXPORT ToolBox::PluginInterface* CreatePlugin() {
//     return new BaseNode::PluginGate();
// }

// extern "C" PLUGIN_EXPORT void DestroyPlugin(ToolBox::PluginInterface* plugin) {
//     delete plugin;
// }

extern "C" PLUGIN_EXPORT void updateSo() {
    PluginGateMgr->pluginUpdate();
}