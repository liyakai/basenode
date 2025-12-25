cmake_minimum_required(VERSION 3.12.0)  

# project name
PROJECT(basenode)

include(common_flag_libs.cmake)

# 添加第三方库 toolbox
ADD_TOOLBOX_LIBRARY()

# ============================================================================
# 创建核心共享库 basenode_core
# 包含 ModuleRouter 和 IModule 基础实现，确保所有模块共享同一个实例
# ============================================================================
ADD_CORE_LIBRARY(basenode_core 
    ${SRC_PATH}/core/module/module_router.cpp
    ${SRC_PATH}/core/module/module_interface.cpp
)

# ============================================================================
# 创建模块共享库
# 所有模块都链接 basenode_core，共享同一个 ModuleRouter 实例
# 使用 PROTOBUF 参数自动链接 protobuf 库
# ============================================================================
ADD_SHARED_LIBRARY_FROM_DIR(gatenode ${SRC_PATH}/framework/gatenode)
# service_discovery 是基础库，使用 NO_BASE_LIBS 避免自己链接自己，使用 EXPORT_SYMBOLS 导出符号供其他模块使用，使用 ZOOKEEPER 链接 zookeeper 库
ADD_SHARED_LIBRARY_FROM_DIR(service_discovery ${SRC_PATH}/core/service_discovery/zookeeper NO_BASE_LIBS EXPORT_SYMBOLS ZOOKEEPER)
# 业务模块自动链接基础库（如 service_discovery），无需手动指定
ADD_SHARED_LIBRARY_FROM_DIR(player_module ${SRC_PATH}/game/player PROTOBUF)
ADD_SHARED_LIBRARY_FROM_DIR(guild_module ${SRC_PATH}/game/guild PROTOBUF)
ADD_SHARED_LIBRARY_FROM_DIR(network ${SRC_PATH}/core/net)

# Network 模块需要特殊链接选项
target_link_options(network PRIVATE
    -Wl,--whole-archive
    $<TARGET_FILE:toolbox>
    -Wl,--no-whole-archive
    -rdynamic
    -Wl,-z,nodefs  # 允许未定义的符号（共享库通常需要）
)


# ============================================================================
# 生成主程序 basenode
# ============================================================================
ADD_EXECUTABLE_FROM_DIRS(basenode 
    ${SRC_PATH}/boot 
    ${SRC_PATH}/core/plugin_system 
    LIBS dl pthread basenode_core
)


message(STATUS "SRC_PATH -> ${SRC_PATH}")