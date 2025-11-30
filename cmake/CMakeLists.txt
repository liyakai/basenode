cmake_minimum_required(VERSION 3.12.0)  

# project name
PROJECT(basenode)

include(common_flag_libs.cmake)


# 编译第三方库 toolbox
# 在 add_subdirectory 之前，确保编译器变量是完整路径
# 因为 common_flag_libs.cmake 中设置的是相对路径 "clang"，需要转换为完整路径
if(CMAKE_C_COMPILER STREQUAL "clang" OR CMAKE_C_COMPILER MATCHES "^clang$")
    find_program(CLANG_C_COMPILER clang PATHS /usr/lib64/ccache /usr/bin /usr/local/bin)
    if(CLANG_C_COMPILER)
        set(CMAKE_C_COMPILER ${CLANG_C_COMPILER} CACHE PATH "C compiler" FORCE)
    endif()
endif()
if(CMAKE_CXX_COMPILER STREQUAL "clang++" OR CMAKE_CXX_COMPILER MATCHES "^clang\\+\\+$")
    find_program(CLANG_CXX_COMPILER clang++ PATHS /usr/lib64/ccache /usr/bin /usr/local/bin)
    if(CLANG_CXX_COMPILER)
        set(CMAKE_CXX_COMPILER ${CLANG_CXX_COMPILER} CACHE PATH "C++ compiler" FORCE)
    endif()
endif()

set(toolbox_dir ${ROOT_PATH}/3rdparty/toolbox)
add_subdirectory(${toolbox_dir} ${CMAKE_BINARY_DIR}/toolbox_build EXCLUDE_FROM_ALL)

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
# ============================================================================
ADD_SHARED_LIBRARY_FROM_DIR(gatenode ${SRC_PATH}/framework/gatenode)
ADD_SHARED_LIBRARY_FROM_DIR(player_module ${SRC_PATH}/game/player)
ADD_SHARED_LIBRARY_FROM_DIR(guild_module ${SRC_PATH}/game/guild)
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