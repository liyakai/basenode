cmake_minimum_required(VERSION 3.12.0)  

# project name
PROJECT(basenode)

include(common_flag_libs.cmake)





# 给组件生成共享库
set(gatenode_dir ${SRC_PATH}/framework/gatenode)
ADD_SHARED_LIBRARY_FROM_DIR(gatenode ${gatenode_dir})

set(player_module_dir ${SRC_PATH}/game/player)
ADD_SHARED_LIBRARY_FROM_DIR(player_module ${player_module_dir})

set(guild_module_dir ${SRC_PATH}/game/guild)
ADD_SHARED_LIBRARY_FROM_DIR(guild_module ${guild_module_dir})

# 生成底座
ADD_EXECUTABLE_FROM_DIRS(basenode ${SRC_PATH}/boot ${SRC_PATH}/core/plugin_system LIBS dl)
message(STATUS "SRC_PATH -> ${SRC_PATH}")