

# 规范化路径，解析 .. 和符号链接（输入输出为同一变量）
function(NORMALIZE_PATH path_var)
    file(REAL_PATH "${${path_var}}" normalized_path)
    set(${path_var} ${normalized_path} PARENT_SCOPE)
endfunction()

# common flags for cmake==================================
set(ROOT_PATH ${PROJECT_SOURCE_DIR}/..)
NORMALIZE_PATH(ROOT_PATH)
set(SRC_PATH ${ROOT_PATH}/src)
set(SRC_CORE_PATH ${SRC_PATH}/core)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 如果没有指定构建类型，默认为 Debug（便于调试）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()




# cmake 公共函数==================================

# 递归添加所有子目录到包含路径
function(ADD_SUBDIRECTORIES result cur_dir)
    file(GLOB children RELATIVE ${cur_dir} ${cur_dir}/*)
    set(tmp_dirlist "")
    list(APPEND ${result} ${cur_dir})
    foreach(child ${children})
        if(IS_DIRECTORY ${cur_dir}/${child})
            list(APPEND tmp_dirlist ${cur_dir}/${child})
            # 递归调用以找到所有子目录
            ADD_SUBDIRECTORIES(${result} ${cur_dir}/${child})
        endif()
    endforeach()
    set(${result} ${${result}} ${tmp_dirlist} PARENT_SCOPE)
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
    set_target_properties(${target_name} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${ROOT_PATH}/lib
        LIBRARY_OUTPUT_DIRECTORY ${ROOT_PATH}/lib
        RUNTIME_OUTPUT_DIRECTORY ${ROOT_PATH}/lib
    )
    # 导出所有符号，让其他共享库可以访问
    target_compile_options(${target_name} PRIVATE -fvisibility=default)
    target_link_options(${target_name} PRIVATE -rdynamic)
    add_custom_command(TARGET ${target_name} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ROOT_PATH}/lib
    )
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
endfunction()

# 配置模块共享库的通用属性
# 参数:
#   target_name - 目标名称
#   source_dir - 源文件目录路径
function(CONFIGURE_MODULE_LIBRARY target_name source_dir)
    # 设置头文件搜索路径
    target_include_directories(${target_name} PRIVATE
        ${source_dir}
        ${source_dir}/..
        ${ROOT_PATH}/3rdparty/toolbox/include
        ${SRC_CORE_PATH}
        ${SRC_CORE_PATH}/module
    )
    # 链接 basenode_core 库，以访问 ModuleRouter
    target_link_libraries(${target_name} PRIVATE basenode_core)
    # 设置所有构建类型的输出路径
    set_target_properties(${target_name} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${ROOT_PATH}/lib
        LIBRARY_OUTPUT_DIRECTORY ${ROOT_PATH}/lib
        RUNTIME_OUTPUT_DIRECTORY ${ROOT_PATH}/lib
    )
    # 确保目标目录存在
    add_custom_command(TARGET ${target_name} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ROOT_PATH}/lib
    )
    # 设置编译选项
    target_compile_options(${target_name} PRIVATE 
        -fvisibility=hidden             # 隐藏所有符号
        -fvisibility-inlines-hidden     # 隐藏内联函数符号
        -fmacro-prefix-map=${ROOT_PATH}/=  # 设置 __FILE__ 显示相对路径
    )
    # 如果是 Debug 构建类型，添加调试选项
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${target_name} PRIVATE 
            -g3                              # 生成详细的调试信息（包括宏定义）
            -O0                              # 禁用优化，确保调试时代码不被优化掉
        )
    endif()
endfunction()

# 从指定目录生成共享库
# 参数:
#   name - 库名称（也是输出文件名）
#   source_dir - 源文件目录路径
#   output_dir - 输出目录（可选，默认为 ${PROJECT_SOURCE_DIR}/lib）
function(ADD_SHARED_LIBRARY_FROM_DIR name source_dir)
    # 处理可选参数：输出目录
    if(ARGC GREATER 2)
        set(output_dir ${ARGV2})
    else()
        set(output_dir ${ROOT_PATH}/lib)
    endif()

    # 收集源文件
    AUX_SOURCE_DIRECTORY(${source_dir} ${name}_SRCS)
    # 收集 module 目录的源文件，但排除 module_router.cpp 和 module_interface.cpp（它们在 basenode_core 中编译）
    AUX_SOURCE_DIRECTORY(${SRC_CORE_PATH}/module ${name}_TMP_MODULE_SRCS)
    list(FILTER ${name}_TMP_MODULE_SRCS EXCLUDE REGEX ".*module_router\\.cpp$")
    list(FILTER ${name}_TMP_MODULE_SRCS EXCLUDE REGEX ".*module_interface\\.cpp$")
    list(APPEND ${name}_SRCS ${${name}_TMP_MODULE_SRCS})
    Message(STATUS "${name}_SRCS -> ${${name}_SRCS}")
    # 创建共享库
    ADD_LIBRARY(${name} SHARED ${${name}_SRCS})
    
    # 配置模块库属性
    CONFIGURE_MODULE_LIBRARY(${name} ${source_dir})
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
    set(parse_output_dir FALSE)
    set(parse_libs FALSE)
    
    # 解析参数
    foreach(arg ${ARGN})
        if(arg STREQUAL "OUTPUT_DIR")
            set(parse_output_dir TRUE)
            set(parse_libs FALSE)
        elseif(arg STREQUAL "LIBS")
            set(parse_libs TRUE)
            set(parse_output_dir FALSE)
        elseif(parse_output_dir)
            set(output_dir ${arg})
            set(parse_output_dir FALSE)
        elseif(parse_libs)
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
    
    # 设置编译选项
    target_compile_options(${name} PRIVATE 
        -fmacro-prefix-map=${ROOT_PATH}/=  # 设置 __FILE__ 显示相对路径
    )
    
    # 如果是 Debug 构建类型，添加调试选项
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${name} PRIVATE 
            -g3                              # 生成详细的调试信息（包括宏定义）
            -O0                              # 禁用优化，确保调试时代码不被优化掉
        )
    endif()
    
    # 链接库
    if(libs)
        target_link_libraries(${name} PRIVATE ${libs})
    endif()
endfunction()

