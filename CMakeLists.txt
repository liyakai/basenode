cmake_minimum_required(VERSION 3.12.0)  

# project name
PROJECT(basenode)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/3rdparty/cmake)

set(CMAKE_CXX_STANDARD 20)

# 如果没有指定构建类型，默认为 Debug（便于调试）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# 设置编译器为clang
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# 导出编译命令，用于生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译库
add_subdirectory(src)