#pragma once
#include <string_view>
#include <map>
#include "tools/singleton.h"

namespace BaseNode
{
class PluginLoadManager
{
public:
    int Init();
    int Update();

private:
    int LoadPluginSo_(const std::string& so_path);

private:
    std::map<std::string, void*> plugin_map_;
};
} // namespace BaseNode

#define PluginLoadMgr ToolBox::Singleton<BaseNode::PluginLoadManager>::Instance()