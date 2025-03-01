cmake_minimum_required(VERSION 3.12.0)  

# project name
PROJECT(basenode)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/3rdparty/cmake)

set(CMAKE_CXX_STANDARD 20)

# 设置编译器为clang
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# 编译库
add_subdirectory(src)