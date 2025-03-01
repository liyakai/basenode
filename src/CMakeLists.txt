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

add_subdirectory(gatenode)

find_library(DL_LIBRARY dl)
target_link_libraries(basenode ${DL_LIBRARY})
