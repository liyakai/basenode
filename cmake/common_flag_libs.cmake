# ============================================================================
# basenode CMake 公共配置和函数库
# ============================================================================

# 规范化路径，解析 .. 和符号链接（输入输出为同一变量）
function(NORMALIZE_PATH path_var)
    file(REAL_PATH "${${path_var}}" normalized_path)
    set(${path_var} ${normalized_path} PARENT_SCOPE)
endfunction()

# ============================================================================
# 路径配置
# ============================================================================
set(ROOT_PATH ${PROJECT_SOURCE_DIR}/..)
NORMALIZE_PATH(ROOT_PATH)
set(SRC_PATH ${ROOT_PATH}/src)
set(SRC_CORE_PATH ${SRC_PATH}/core)


# ============================================================================
# 编译选项配置
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 如果没有指定构建类型，默认为 Debug（便于调试）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ============================================================================
# 第三方库配置
# ============================================================================
# 添加 toolbox 第三方库
function(ADD_TOOLBOX_LIBRARY)
    if(NOT TARGET toolbox)
        set(toolbox_dir ${ROOT_PATH}/3rdparty/toolbox)
        add_subdirectory(${toolbox_dir} ${CMAKE_BINARY_DIR}/toolbox_build EXCLUDE_FROM_ALL)
    endif()
endfunction()

# ============================================================================
# CMake 公共函数
# ============================================================================

# 设置目标的输出目录（库和可执行文件）
function(SET_TARGET_OUTPUT_DIR target_name output_dir)
    set_target_properties(${target_name} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${output_dir}
        LIBRARY_OUTPUT_DIRECTORY ${output_dir}
        RUNTIME_OUTPUT_DIRECTORY ${output_dir}
    )
    add_custom_command(TARGET ${target_name} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}
    )
endfunction()

# 设置目标的编译选项（通用部分）
function(SET_TARGET_COMPILE_OPTIONS target_name export_symbols)
    if(export_symbols)
        target_compile_options(${target_name} PRIVATE 
            -fvisibility=default
            -fmacro-prefix-map=${ROOT_PATH}/=
        )
        target_link_options(${target_name} PRIVATE -rdynamic)
    else()
        target_compile_options(${target_name} PRIVATE 
            -fvisibility=hidden
            -fvisibility-inlines-hidden
            -fmacro-prefix-map=${ROOT_PATH}/=
        )
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${target_name} PRIVATE -g3 -O0)
    endif()
endfunction()

# 递归添加所有子目录到包含路径
function(ADD_SUBDIRECTORIES result cur_dir)
    set(dir_list ${cur_dir})
    file(GLOB children RELATIVE ${cur_dir} ${cur_dir}/*)
    foreach(child ${children})
        set(child_path ${cur_dir}/${child})
        if(IS_DIRECTORY ${child_path})
            list(APPEND dir_list ${child_path})
            # 递归处理子目录
            ADD_SUBDIRECTORIES(sub_dirs ${child_path})
            list(APPEND dir_list ${sub_dirs})
        endif()
    endforeach()
    set(${result} ${dir_list} PARENT_SCOPE)
endfunction()

# 配置核心共享库的通用属性（内部函数）
# 参数:
#   target_name - 目标名称
function(CONFIGURE_CORE_LIBRARY target_name)
    target_include_directories(${target_name} PUBLIC
        ${ROOT_PATH}/3rdparty/toolbox/include
        ${SRC_CORE_PATH}
        ${SRC_CORE_PATH}/module
    )
    target_link_libraries(${target_name} PUBLIC toolbox)
    SET_TARGET_OUTPUT_DIR(${target_name} ${ROOT_PATH}/lib)
    SET_TARGET_COMPILE_OPTIONS(${target_name} TRUE)
endfunction()

# 创建并配置核心共享库
# 参数:
#   name - 库名称（也是输出文件名）
#   source_files... - 一个或多个源文件路径
# 示例:
#   ADD_CORE_LIBRARY(basenode_core 
#       ${SRC_PATH}/core/module/module_router.cpp
#       ${SRC_PATH}/core/module/module_interface.cpp
#   )
function(ADD_CORE_LIBRARY name)
    # 收集所有源文件
    set(${name}_SRCS ${ARGN})
    
    # 创建共享库
    add_library(${name} SHARED ${${name}_SRCS})
    
    # 配置核心库属性
    CONFIGURE_CORE_LIBRARY(${name})
    
    # 确保 toolbox 在 basenode_core 之前构建（即使使用了 EXCLUDE_FROM_ALL）
    if(TARGET toolbox)
        add_dependencies(${name} toolbox)
    endif()
endfunction()

# 配置模块共享库的通用属性
# 参数:
#   target_name - 目标名称
#   source_dir - 源文件目录路径
#   export_symbols - 可选，如果为 TRUE 则导出所有符号（用于基础库）
function(CONFIGURE_MODULE_LIBRARY target_name source_dir)
    if(ARGC GREATER 2)
        set(should_export_symbols ${ARGV2})
    else()
        set(should_export_symbols FALSE)
    endif()
    
    target_include_directories(${target_name} PRIVATE
        ${source_dir}
        ${source_dir}/..
        ${ROOT_PATH}/3rdparty/toolbox/include
        ${SRC_CORE_PATH}
        ${SRC_CORE_PATH}/module
        ${SRC_CORE_PATH}/protobuf/pb_out
    )
    target_link_libraries(${target_name} PRIVATE basenode_core)
    SET_TARGET_OUTPUT_DIR(${target_name} ${ROOT_PATH}/lib)
    SET_TARGET_COMPILE_OPTIONS(${target_name} ${should_export_symbols})
    
    if(should_export_symbols)
        message(STATUS "Configured ${target_name} with exported symbols (base library)")
    endif()
endfunction()

# 查找 protobuf 库
# 参数:
#   PROTOBUF_LIB_VAR - 输出变量名，用于存储 protobuf 库的完整路径或库名
#   PROTOBUF_INCLUDE_DIR_VAR - 输出变量名，用于存储 protobuf 头文件目录
function(FIND_PROTOBUF_LIBRARY PROTOBUF_LIB_VAR PROTOBUF_INCLUDE_DIR_VAR)
    # 使用缓存变量，避免重复查找
    if(DEFINED _CACHED_PROTOBUF_LIB AND DEFINED _CACHED_PROTOBUF_INCLUDE)
        set(${PROTOBUF_LIB_VAR} ${_CACHED_PROTOBUF_LIB} PARENT_SCOPE)
        set(${PROTOBUF_INCLUDE_DIR_VAR} ${_CACHED_PROTOBUF_INCLUDE} PARENT_SCOPE)
        return()
    endif()
    
    # 首先尝试在第三方目录中查找
    set(PROTOBUF_ROOT ${ROOT_PATH}/3rdparty/toolbox/3rdparty/protobuf_3.21.11)
    set(PROTOBUF_LIB_PATH "${PROTOBUF_ROOT}/lib/libprotobuf.a")
    
    if(EXISTS ${PROTOBUF_LIB_PATH} AND EXISTS "${PROTOBUF_ROOT}/include")
        set(${PROTOBUF_LIB_VAR} ${PROTOBUF_LIB_PATH} PARENT_SCOPE)
        set(${PROTOBUF_INCLUDE_DIR_VAR} "${PROTOBUF_ROOT}/include" PARENT_SCOPE)
        set(_CACHED_PROTOBUF_LIB ${PROTOBUF_LIB_PATH} CACHE INTERNAL "Cached protobuf library path")
        set(_CACHED_PROTOBUF_INCLUDE "${PROTOBUF_ROOT}/include" CACHE INTERNAL "Cached protobuf include path")
        message(STATUS "Found protobuf library: ${PROTOBUF_LIB_PATH}")
        return()
    endif()
    
    # 查找系统安装的版本
    find_library(PROTOBUF_LIB_FOUND 
        NAMES protobuf libprotobuf
        PATHS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64
        NO_DEFAULT_PATH
    )
    if(NOT PROTOBUF_LIB_FOUND)
        find_library(PROTOBUF_LIB_FOUND NAMES protobuf libprotobuf)
    endif()
    
    find_path(PROTOBUF_INCLUDE_FOUND
        NAMES google/protobuf/message.h
        PATHS /usr/include /usr/local/include
        NO_DEFAULT_PATH
    )
    if(NOT PROTOBUF_INCLUDE_FOUND)
        find_path(PROTOBUF_INCLUDE_FOUND NAMES google/protobuf/message.h)
    endif()
    
    if(PROTOBUF_LIB_FOUND AND PROTOBUF_INCLUDE_FOUND)
        set(${PROTOBUF_LIB_VAR} ${PROTOBUF_LIB_FOUND} PARENT_SCOPE)
        set(${PROTOBUF_INCLUDE_DIR_VAR} ${PROTOBUF_INCLUDE_FOUND} PARENT_SCOPE)
        set(_CACHED_PROTOBUF_LIB ${PROTOBUF_LIB_FOUND} CACHE INTERNAL "Cached protobuf library path")
        set(_CACHED_PROTOBUF_INCLUDE ${PROTOBUF_INCLUDE_FOUND} CACHE INTERNAL "Cached protobuf include path")
        message(STATUS "Found system protobuf library: ${PROTOBUF_LIB_FOUND}")
        return()
    endif()
    
    # 如果都找不到，使用库名（让链接器在运行时查找）
    set(${PROTOBUF_LIB_VAR} "protobuf" PARENT_SCOPE)
    set(${PROTOBUF_INCLUDE_DIR_VAR} "" PARENT_SCOPE)
    set(_CACHED_PROTOBUF_LIB "protobuf" CACHE INTERNAL "Cached protobuf library name")
    set(_CACHED_PROTOBUF_INCLUDE "" CACHE INTERNAL "Cached protobuf include (empty)")
    message(WARNING "Protobuf library not found, will use -lprotobuf at link time")
endfunction()

# 创建并配置 protobuf 库（如果尚未创建）
# 返回: protobuf 库的目标名称
function(ENSURE_PROTOBUF_LIBRARY)
    # 如果已经创建过，直接返回
    if(TARGET basenode_protobuf)
        return()
    endif()
    
    # 自动查找所有 .pb.cc 文件
    file(GLOB PROTOBUF_SRCS "${SRC_CORE_PATH}/protobuf/pb_out/*.pb.cc")
    
    if(NOT PROTOBUF_SRCS)
        message(WARNING "No protobuf source files found in ${SRC_CORE_PATH}/protobuf/pb_out/")
        return()
    endif()
    
    # 创建 protobuf 静态库
    add_library(basenode_protobuf STATIC ${PROTOBUF_SRCS})
    
    # 启用位置无关代码（PIC），因为静态库会被链接到共享库中
    set_target_properties(basenode_protobuf PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
    
    # 查找 protobuf 库
    FIND_PROTOBUF_LIBRARY(PROTOBUF_LIB_PATH PROTOBUF_INCLUDE_DIR_PATH)
    
    # 配置包含目录
    set(INCLUDE_DIRS
        ${SRC_CORE_PATH}/protobuf/pb_out
        ${ROOT_PATH}/3rdparty/toolbox/include
    )
    
    # 添加 protobuf 包含目录
    if(NOT "${PROTOBUF_INCLUDE_DIR_PATH}" STREQUAL "")
        list(APPEND INCLUDE_DIRS ${PROTOBUF_INCLUDE_DIR_PATH})
    else()
        # 如果找不到，尝试使用第三方目录
        set(PROTOBUF_ROOT ${ROOT_PATH}/3rdparty/toolbox/3rdparty/protobuf_3.21.11)
        if(EXISTS "${PROTOBUF_ROOT}/include")
            list(APPEND INCLUDE_DIRS "${PROTOBUF_ROOT}/include")
        endif()
    endif()
    
    target_include_directories(basenode_protobuf PUBLIC ${INCLUDE_DIRS})
    
    # 链接 protobuf 库
    target_link_libraries(basenode_protobuf PUBLIC ${PROTOBUF_LIB_PATH})
    
    # 设置输出目录
    set_target_properties(basenode_protobuf PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${ROOT_PATH}/lib
    )
    
    message(STATUS "Created protobuf library with sources: ${PROTOBUF_SRCS}")
endfunction()

# 定义基础库列表（所有模块默认链接的基础库）
# 这些库会被自动链接到所有通过 ADD_SHARED_LIBRARY_FROM_DIR 创建的模块
set(BASENODE_BASE_LIBS service_discovery CACHE INTERNAL "Base libraries automatically linked to all modules")

# 从指定目录生成共享库
# 参数:
#   name - 库名称（也是输出文件名）
#   source_dir - 源文件目录路径
#   [PROTOBUF] - 可选参数，如果提供则自动链接 protobuf 库
#   [NO_BASE_LIBS] - 可选参数，如果提供则不自动链接基础库
#   [EXPORT_SYMBOLS] - 可选参数，如果提供则导出所有符号（用于基础库，供其他模块使用）
#   [DEPENDS lib1 lib2 ...] - 可选，指定需要链接的其他库（使用 PUBLIC 链接）
# 示例:
#   ADD_SHARED_LIBRARY_FROM_DIR(player_module ${SRC_PATH}/game/player)
#   ADD_SHARED_LIBRARY_FROM_DIR(player_module ${SRC_PATH}/game/player PROTOBUF)
#   ADD_SHARED_LIBRARY_FROM_DIR(player_module ${SRC_PATH}/game/player PROTOBUF DEPENDS network)
#   ADD_SHARED_LIBRARY_FROM_DIR(service_discovery ${SRC_PATH}/core/service_discovery/zookeeper NO_BASE_LIBS EXPORT_SYMBOLS)
# 参数说明:
#   name - 库名称
#   source_dir - 源文件目录
#   PROTOBUF - 可选，如果需要链接 protobuf 库
#   NO_BASE_LIBS - 可选，如果提供则不自动链接基础库（用于基础库自身）
#   EXPORT_SYMBOLS - 可选，如果提供则导出所有符号（用于基础库，确保 vtable 等符号在运行时可见）
#   DEPENDS lib1 lib2 ... - 可选，指定需要链接的其他库（使用 PUBLIC 链接）
function(ADD_SHARED_LIBRARY_FROM_DIR name source_dir)
    # 解析参数：PROTOBUF、NO_BASE_LIBS、EXPORT_SYMBOLS 和 DEPENDS
    set(need_protobuf FALSE)
    set(no_base_libs FALSE)
    set(export_symbols FALSE)
    set(depends_libs "")
    
    set(current_keyword "")
    foreach(arg ${ARGN})
        if(arg MATCHES "^(PROTOBUF|NO_BASE_LIBS|EXPORT_SYMBOLS|DEPENDS)$")
            set(current_keyword ${arg})
            if(arg STREQUAL "PROTOBUF")
                set(need_protobuf TRUE)
            elseif(arg STREQUAL "NO_BASE_LIBS")
                set(no_base_libs TRUE)
            elseif(arg STREQUAL "EXPORT_SYMBOLS")
                set(export_symbols TRUE)
            endif()
        elseif(current_keyword STREQUAL "DEPENDS")
            list(APPEND depends_libs ${arg})
        endif()
    endforeach()

    # 收集源文件
    AUX_SOURCE_DIRECTORY(${source_dir} ${name}_SRCS)
    # 收集 module 目录的源文件，但排除 module_router.cpp 和 module_interface.cpp（它们在 basenode_core 中编译）
    AUX_SOURCE_DIRECTORY(${SRC_CORE_PATH}/module ${name}_TMP_MODULE_SRCS)
    list(FILTER ${name}_TMP_MODULE_SRCS EXCLUDE REGEX ".*module_(router|interface)\\.cpp$")
    list(APPEND ${name}_SRCS ${${name}_TMP_MODULE_SRCS})
    message(STATUS "${name}_SRCS -> ${${name}_SRCS}")
    
    # 创建共享库
    ADD_LIBRARY(${name} SHARED ${${name}_SRCS})
    
    # 确保 basenode_core 在模块之前构建
    if(TARGET basenode_core)
        add_dependencies(${name} basenode_core)
    endif()
    
    # 配置模块库属性（传递 export_symbols 参数）
    CONFIGURE_MODULE_LIBRARY(${name} ${source_dir} ${export_symbols})
    
    # 如果需要 protobuf，确保 protobuf 库存在并链接
    if(need_protobuf)
        ENSURE_PROTOBUF_LIBRARY()
        if(TARGET basenode_protobuf)
            target_link_libraries(${name} PRIVATE basenode_protobuf)
            add_dependencies(${name} basenode_protobuf)
            message(STATUS "Linked protobuf library to ${name}")
        endif()
    endif()
    
    # 自动链接基础库（除非明确排除）
    if(NOT no_base_libs)
        foreach(base_lib ${BASENODE_BASE_LIBS})
            if(TARGET ${base_lib})
                target_link_libraries(${name} PUBLIC ${base_lib})
                add_dependencies(${name} ${base_lib})
                message(STATUS "Auto-linked base library ${base_lib} to ${name}")
            endif()
        endforeach()
    endif()
    
    # 链接额外依赖库（使用 PUBLIC 链接，确保符号在运行时也可用）
    foreach(dep ${depends_libs})
        if(TARGET ${dep})
            target_link_libraries(${name} PUBLIC ${dep})
            message(STATUS "Linked ${dep} library to ${name}")
        else()
            message(WARNING "Dependency target ${dep} not found for ${name}")
        endif()
    endforeach()
endfunction()


# 从多个目录生成可执行文件
# 参数:
#   name - 可执行文件名称
#   source_dirs... - 一个或多个源文件目录路径
#   OUTPUT_DIR dir - 可选，使用 OUTPUT_DIR 关键字指定输出目录（默认为 ${ROOT_PATH}/bin）
#   LIBS lib1 lib2 ... - 可选，使用 LIBS 关键字指定需要链接的库
# 示例:
#   ADD_EXECUTABLE_FROM_DIRS(myapp ./boot ./core/plugin_system)
#   ADD_EXECUTABLE_FROM_DIRS(myapp ./boot ./core OUTPUT_DIR ${CUSTOM_BIN_DIR} LIBS dl pthread)
function(ADD_EXECUTABLE_FROM_DIRS name)
    set(source_dirs "")
    set(output_dir ${ROOT_PATH}/bin)
    set(libs "")
    
    # 解析参数
    set(current_keyword "")
    foreach(arg ${ARGN})
        if(arg MATCHES "^(OUTPUT_DIR|LIBS)$")
            set(current_keyword ${arg})
        elseif(current_keyword STREQUAL "OUTPUT_DIR")
            set(output_dir ${arg})
            set(current_keyword "")
        elseif(current_keyword STREQUAL "LIBS")
            list(APPEND libs ${arg})
        else()
            list(APPEND source_dirs ${arg})
        endif()
    endforeach()
    
    # 检查是否提供了源文件目录
    if(NOT source_dirs)
        message(FATAL_ERROR "ADD_EXECUTABLE_FROM_DIRS: 至少需要提供一个源文件目录")
    endif()
    
    # 收集所有源文件
    set(${name}_SRCS "")
    foreach(source_dir ${source_dirs})
        AUX_SOURCE_DIRECTORY(${source_dir} ${name}_TMP_SRCS)
        list(APPEND ${name}_SRCS ${${name}_TMP_SRCS})
    endforeach()
    
    # 递归添加所有子目录到头文件搜索路径
    set(INCLUDE_DIRS "")
    foreach(source_dir ${source_dirs})
        ADD_SUBDIRECTORIES(INCLUDE_DIRS ${source_dir})
    endforeach()
    
    # 设置头文件搜索路径
    INCLUDE_DIRECTORIES(
        .
        ..
        ${ROOT_PATH}/3rdparty/toolbox/include
        ${INCLUDE_DIRS}
    )
    
    # 设置输出目录
    SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${output_dir})
    
    # 创建可执行文件
    ADD_EXECUTABLE(${name} ${${name}_SRCS})
    
    # 设置编译选项（可执行文件默认不导出符号）
    SET_TARGET_COMPILE_OPTIONS(${name} FALSE)
    
    # 链接库并添加依赖关系
    if(libs)
        target_link_libraries(${name} PRIVATE ${libs})
        # 为所有目标库添加显式依赖关系
        foreach(lib ${libs})
            if(TARGET ${lib})
                add_dependencies(${name} ${lib})
            endif()
        endforeach()
    endif()
endfunction()

