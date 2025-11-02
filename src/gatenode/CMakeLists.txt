set(name gatesvr)

# 头文件
INCLUDE_DIRECTORIES(
    .
    ..
    ../3rdparty/toolbox/include
)

AUX_SOURCE_DIRECTORY(. DIR_SRCS)


ADD_LIBRARY(${name} SHARED ${DIR_SRCS})

# 设置所有构建类型的输出路径
set_target_properties(${name} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

# 确保目标目录存在
add_custom_command(TARGET ${name} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_SOURCE_DIR}/lib
)

target_compile_options(${name} PRIVATE 
    -fvisibility=hidden             # 1. 隐藏所有符号
    -fvisibility-inlines-hidden     # 2. 隐藏内联函数符号
    -fmacro-prefix-map=${PROJECT_SOURCE_DIR}/=  # 设置 __FILE__ 显示相对路径（相对于项目根目录）                           # 禁用优化，确保调试时代码不被优化掉
)