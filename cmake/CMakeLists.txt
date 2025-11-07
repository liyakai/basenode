cmake_minimum_required(VERSION 3.12.0)  

# project name
PROJECT(basenode)

include(common_flag_libs.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# 给组件生成共享库
set(gatenode_dir ${SRC_PATH}/framework/gatenode)
ADD_SHARED_LIBRARY_FROM_DIR(gatenode ${gatenode_dir})

# 生成底座
ADD_EXECUTABLE_FROM_DIRS(basenode ${SRC_PATH}/boot ${SRC_PATH}/core/plugin_system LIBS dl)
message(STATUS "SRC_PATH -> ${SRC_PATH}")