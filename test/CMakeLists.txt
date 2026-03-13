# SimpleRemoter 测试构建配置
cmake_minimum_required(VERSION 3.14)
project(SimpleRemoterTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC 编码设置
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 选项：是否构建测试
option(BUILD_TESTING "Build tests" ON)

if(NOT BUILD_TESTING)
    return()
endif()

# 使用 FetchContent 获取 Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# Windows 下避免覆盖 /MT /MD 设置
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

enable_testing()

# 包含项目头文件目录
include_directories(${CMAKE_SOURCE_DIR}/..)
include_directories(${CMAKE_SOURCE_DIR}/../client)
include_directories(${CMAKE_SOURCE_DIR}/../server/2015Remote)
include_directories(${CMAKE_SOURCE_DIR}/../common)

# ============================================
# 客户端 Buffer 测试
# ============================================
add_executable(client_buffer_test
    unit/client/BufferTest.cpp
)

# 测试文件内联包含了 Buffer 实现的测试版本
# 无需链接原始源文件，支持跨平台测试

target_link_libraries(client_buffer_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(client_buffer_test PRIVATE _WIN32)
endif()

# ============================================
# 服务端 Buffer 测试
# ============================================
add_executable(server_buffer_test
    unit/server/BufferTest.cpp
)

# 测试文件内联包含了 Buffer 实现的测试版本
# 支持跨平台测试

target_link_libraries(server_buffer_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(server_buffer_test PRIVATE _WIN32)
endif()

# Linux 需要 pthread
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(client_buffer_test Threads::Threads)
    target_link_libraries(server_buffer_test Threads::Threads)
endif()

# ============================================
# 协议测试 (Phase 1)
# ============================================
add_executable(protocol_test
    unit/protocol/PacketTest.cpp
    unit/protocol/PathUtilsTest.cpp
)

target_link_libraries(protocol_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(protocol_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(protocol_test Threads::Threads)
endif()

# ============================================
# 文件传输测试 (Phase 2)
# ============================================
add_executable(file_transfer_test
    unit/file/FileTransferV2Test.cpp
)

target_link_libraries(file_transfer_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(file_transfer_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(file_transfer_test Threads::Threads)
endif()

# ============================================
# 分块管理测试 (Phase 2)
# ============================================
add_executable(chunk_manager_test
    unit/file/ChunkManagerTest.cpp
)

target_link_libraries(chunk_manager_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(chunk_manager_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(chunk_manager_test Threads::Threads)
endif()

# ============================================
# SHA-256 校验测试 (Phase 2)
# ============================================
add_executable(sha256_verify_test
    unit/file/SHA256VerifyTest.cpp
)

target_link_libraries(sha256_verify_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(sha256_verify_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(sha256_verify_test Threads::Threads)
endif()

# ============================================
# 断点续传状态测试 (Phase 2)
# ============================================
add_executable(resume_state_test
    unit/file/ResumeStateTest.cpp
)

target_link_libraries(resume_state_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(resume_state_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(resume_state_test Threads::Threads)
endif()

# ============================================
# 协议头验证测试 (Phase 3)
# ============================================
add_executable(header_test
    unit/network/HeaderTest.cpp
)

target_link_libraries(header_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(header_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(header_test Threads::Threads)
endif()

# ============================================
# 粘包/分包测试 (Phase 3)
# ============================================
add_executable(packet_fragment_test
    unit/network/PacketFragmentTest.cpp
)

target_link_libraries(packet_fragment_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(packet_fragment_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(packet_fragment_test Threads::Threads)
endif()

# ============================================
# HTTP 伪装测试 (Phase 3)
# ============================================
add_executable(http_mask_test
    unit/network/HttpMaskTest.cpp
)

target_link_libraries(http_mask_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(http_mask_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(http_mask_test Threads::Threads)
endif()

# ============================================
# 差分算法测试 (Phase 4)
# ============================================
add_executable(diff_algorithm_test
    unit/screen/DiffAlgorithmTest.cpp
)

target_link_libraries(diff_algorithm_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(diff_algorithm_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(diff_algorithm_test Threads::Threads)
endif()

# ============================================
# RGB565 压缩测试 (Phase 4)
# ============================================
add_executable(rgb565_test
    unit/screen/RGB565Test.cpp
)

target_link_libraries(rgb565_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(rgb565_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(rgb565_test Threads::Threads)
endif()

# ============================================
# 滚动检测测试 (Phase 4)
# ============================================
add_executable(scroll_detector_test
    unit/screen/ScrollDetectorTest.cpp
)

target_link_libraries(scroll_detector_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(scroll_detector_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(scroll_detector_test Threads::Threads)
endif()

# ============================================
# 质量自适应测试 (Phase 4)
# ============================================
add_executable(quality_adaptive_test
    unit/screen/QualityAdaptiveTest.cpp
)

target_link_libraries(quality_adaptive_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(quality_adaptive_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(quality_adaptive_test Threads::Threads)
endif()

# ============================================
# 时间验证测试 (授权模块)
# ============================================
add_executable(date_verify_test
    unit/auth/DateVerifyTest.cpp
)

target_link_libraries(date_verify_test
    GTest::gtest_main
)

if(WIN32)
    target_compile_definitions(date_verify_test PRIVATE _WIN32)
endif()

if(UNIX)
    target_link_libraries(date_verify_test Threads::Threads)
endif()

# 注册测试
include(GoogleTest)
gtest_discover_tests(client_buffer_test)
gtest_discover_tests(server_buffer_test)
gtest_discover_tests(protocol_test)
gtest_discover_tests(file_transfer_test)
gtest_discover_tests(chunk_manager_test)
gtest_discover_tests(sha256_verify_test)
gtest_discover_tests(resume_state_test)
gtest_discover_tests(header_test)
gtest_discover_tests(packet_fragment_test)
gtest_discover_tests(http_mask_test)
gtest_discover_tests(diff_algorithm_test)
gtest_discover_tests(rgb565_test)
gtest_discover_tests(scroll_detector_test)
gtest_discover_tests(quality_adaptive_test)
gtest_discover_tests(date_verify_test)

# 自定义目标：运行所有测试
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS client_buffer_test server_buffer_test protocol_test
            file_transfer_test chunk_manager_test sha256_verify_test resume_state_test
            header_test packet_fragment_test http_mask_test
            diff_algorithm_test rgb565_test scroll_detector_test quality_adaptive_test
            date_verify_test
)
