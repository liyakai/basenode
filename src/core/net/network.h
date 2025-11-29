#pragma once

#include "module_interface.h"
#include "network/network_api.h"
#include "tools/singleton.h"

namespace BaseNode
{

class Network : public IModule
{
public:
    Network();
    virtual ~Network();

    // 提供访问底层网络库的接口（可选，用于高级功能）
    ToolBox::Network* GetNetwork() { return network_impl_; }

protected:
    virtual ErrorCode DoInit() override;
    virtual ErrorCode DoUpdate() override;
    virtual ErrorCode DoUninit() override;

private:
    ToolBox::Network* network_impl_;  // 第三方网络库实例
};

#define NetworkMgr ToolBox::Singleton<Network>::Instance()

} // namespace BaseNode