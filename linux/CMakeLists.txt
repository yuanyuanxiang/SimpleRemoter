# 编译方法：cmake . && make
# 设置 CMake 最低版本要求
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED 11)

cmake_minimum_required(VERSION 3.22)

# 定义项目名称和版本
project(SimpleRemoter VERSION 1.0)

include_directories(${CMAKE_SOURCE_DIR}/mterm)

# 额外的包含目录
include_directories(../)
include_directories(../client)
include_directories(../compress)

# 添加可执行文件
set(SOURCES
    main.cpp
    ../client/Buffer.cpp
    ../client/IOCPClient.cpp
)
add_executable(ghost ${SOURCES})


# 设置为可以调试
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

# 链接 ZSTD 库
message(STATUS "链接库文件: ${CMAKE_SOURCE_DIR}/lib/libzstd.a")
target_link_libraries(ghost PRIVATE "${CMAKE_SOURCE_DIR}/lib/libzstd.a")
