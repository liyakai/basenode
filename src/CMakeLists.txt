set(name basenode)

# 头文件
INCLUDE_DIRECTORIES(
    .
    ..
    ../3rdparty/toolbox/include
)

AUX_SOURCE_DIRECTORY(. GATE_DIR_SRCS)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)


ADD_EXECUTABLE(${name} ${GATE_DIR_SRCS})

# 设置 __FILE__ 显示相对路径（相对于项目根目录）
target_compile_options(${name} PRIVATE 
    -fmacro-prefix-map=${PROJECT_SOURCE_DIR}/=                           # 禁用优化，确保调试时代码不被优化掉
)

add_subdirectory(gatenode)

find_library(DL_LIBRARY dl)
target_link_libraries(basenode ${DL_LIBRARY})
