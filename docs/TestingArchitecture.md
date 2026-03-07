# SimpleRemoter 测试架构

本文档描述项目的测试架构、框架选择和最佳实践。

## 目录

- [测试框架](#测试框架)
- [目录结构](#目录结构)
- [测试分类](#测试分类)
- [命名规范](#命名规范)
- [编写指南](#编写指南)
- [运行测试](#运行测试)
- [CI/CD 集成](#cicd-集成)

## 测试框架

### Google Test (gtest)

项目采用 [Google Test](https://github.com/google/googletest) 作为主要测试框架。

**选择理由：**

| 特性 | 说明 |
|------|------|
| 功能完整 | 支持断言、Mock、参数化测试、死亡测试 |
| 跨平台 | Windows/Linux 均可使用 |
| VS 集成 | Visual Studio Test Explorer 直接支持 |
| CI 友好 | 输出 JUnit XML 格式，CI 系统可直接解析 |
| 社区活跃 | 文档完善，问题易解决 |

### 版本要求

- Google Test: 1.14.0+
- CMake: 3.14+
- C++: 17+ (使用结构化绑定等特性)
- Visual Studio: 2019 或 2022

## 目录结构

```
SimpleRemoter/
├── test/                           # 测试根目录
│   ├── CMakeLists.txt              # 测试构建配置 (14 个测试可执行文件)
│   ├── test.bat                    # Windows 测试管理脚本
│   ├── unit/                       # 单元测试
│   │   ├── client/                 # 客户端模块测试
│   │   │   └── BufferTest.cpp      # 客户端 Buffer (33 用例) ✅
│   │   ├── server/                 # 服务端模块测试
│   │   │   └── BufferTest.cpp      # 服务端 Buffer (40 用例) ✅
│   │   ├── protocol/               # 协议层测试 (Phase 1)
│   │   │   ├── PacketTest.cpp      # 文件传输包 (39 用例) ✅
│   │   │   └── PathUtilsTest.cpp   # 路径处理 (19 用例) ✅
│   │   ├── file/                   # 文件传输测试 (Phase 2)
│   │   │   ├── FileTransferV2Test.cpp   # V2 传输逻辑 (37 用例) ✅
│   │   │   ├── ChunkManagerTest.cpp     # 分块管理 (36 用例) ✅
│   │   │   ├── SHA256VerifyTest.cpp     # SHA-256 校验 (28 用例) ✅
│   │   │   └── ResumeStateTest.cpp      # 断点续传状态 (26 用例) ✅
│   │   ├── network/                # 网络通信测试 (Phase 3)
│   │   │   ├── HeaderTest.cpp      # 协议头验证 (29 用例) ✅
│   │   │   ├── PacketFragmentTest.cpp  # 粘包/分包 (24 用例) ✅
│   │   │   └── HttpMaskTest.cpp    # HTTP 伪装 (27 用例) ✅
│   │   └── screen/                 # 图像处理测试 (Phase 4)
│   │       ├── DiffAlgorithmTest.cpp    # 差分算法 (32 用例) ✅
│   │       ├── RGB565Test.cpp           # RGB565 压缩 (286 用例) ✅
│   │       ├── ScrollDetectorTest.cpp   # 滚动检测 (43 用例) ✅
│   │       └── QualityAdaptiveTest.cpp  # 质量自适应 (64 用例) ✅
│   ├── integration/                # 集成测试 (待实现)
│   ├── mocks/                      # Mock 对象 (待实现)
│   └── fixtures/                   # 测试数据/夹具 (待实现)
├── docs/
│   └── TestingArchitecture.md      # 本文档
└── ...
```

## 测试分类

### 单元测试 (Unit Tests)

测试单个类或函数的独立功能。

**特点：**
- 快速执行（毫秒级）
- 无外部依赖（网络、文件系统等）
- 高覆盖率目标（>80%）

**示例：**
```cpp
TEST(BufferTest, WriteBuffer_ValidData_ReturnsTrue) {
    CBuffer buffer;
    BYTE data[] = {1, 2, 3};
    EXPECT_TRUE(buffer.WriteBuffer(data, 3));
    EXPECT_EQ(buffer.GetBufferLength(), 3);
}
```

### 集成测试 (Integration Tests)

测试多个组件的协作。

**特点：**
- 测试组件间交互
- 可能涉及真实资源
- 执行时间较长

### 端到端测试 (E2E Tests)

测试完整的用户场景。

**特点：**
- 模拟真实使用场景
- 客户端 ↔ 服务端完整流程
- 执行时间最长

## 命名规范

### 测试文件

```
<ClassName>Test.cpp
例：BufferTest.cpp, FileManagerTest.cpp
```

### 测试套件

```
<ClassName>Test
例：TEST(BufferTest, ...)
```

### 测试用例

采用 `MethodName_StateUnderTest_ExpectedBehavior` 格式：

```cpp
// 好的命名
TEST(BufferTest, ReadBuffer_WhenBufferEmpty_ReturnsZero)
TEST(BufferTest, WriteBuffer_ExceedsCapacity_ReallocatesMemory)
TEST(BufferTest, Skip_ExceedsDataLength_ClampsToAvailable)

// 避免的命名
TEST(BufferTest, Test1)
TEST(BufferTest, ReadWorks)
```

## 编写指南

### AAA 模式

所有测试应遵循 Arrange-Act-Assert 模式：

```cpp
TEST(BufferTest, ReadBuffer_PartialRead_ReturnsRequestedLength) {
    // Arrange - 准备测试数据和对象
    CBuffer buffer;
    BYTE writeData[] = {1, 2, 3, 4, 5};
    buffer.WriteBuffer(writeData, 5);

    // Act - 执行被测操作
    BYTE readData[3];
    ULONG bytesRead = buffer.ReadBuffer(readData, 3);

    // Assert - 验证结果
    EXPECT_EQ(bytesRead, 3);
    EXPECT_EQ(readData[0], 1);
    EXPECT_EQ(readData[1], 2);
    EXPECT_EQ(readData[2], 3);
    EXPECT_EQ(buffer.GetBufferLength(), 2);
}
```

### 边界条件测试

确保覆盖边界情况：

```cpp
// 空缓冲区
TEST(BufferTest, ReadBuffer_EmptyBuffer_ReturnsZero)

// 零长度操作
TEST(BufferTest, WriteBuffer_ZeroLength_ReturnsTrue)

// 请求超过可用数据
TEST(BufferTest, ReadBuffer_RequestExceedsAvailable_ReturnsAvailableOnly)

// 大数据量
TEST(BufferTest, WriteBuffer_LargeData_HandlesCorrectly)
```

### 参数化测试

对于需要多组数据验证的场景：

```cpp
class BufferReadTest : public ::testing::TestWithParam<std::tuple<size_t, size_t, size_t>> {};

TEST_P(BufferReadTest, ReadBuffer_VariousLengths) {
    auto [writeLen, readLen, expectedRead] = GetParam();

    CBuffer buffer;
    std::vector<BYTE> data(writeLen, 0x42);
    buffer.WriteBuffer(data.data(), writeLen);

    std::vector<BYTE> result(readLen);
    ULONG actual = buffer.ReadBuffer(result.data(), readLen);

    EXPECT_EQ(actual, expectedRead);
}

INSTANTIATE_TEST_SUITE_P(
    ReadLengths,
    BufferReadTest,
    ::testing::Values(
        std::make_tuple(10, 5, 5),    // 正常读取
        std::make_tuple(5, 10, 5),    // 请求超过可用
        std::make_tuple(0, 5, 0),     // 空缓冲区
        std::make_tuple(100, 0, 0)    // 零长度读取
    )
);
```

### 线程安全测试（服务端 Buffer）

```cpp
TEST(ServerBufferTest, ConcurrentReadWrite_NoDataCorruption) {
    CBuffer buffer;
    std::atomic<bool> running{true};

    // 写线程
    std::thread writer([&]() {
        BYTE data[100];
        while (running) {
            buffer.WriteBuffer(data, sizeof(data));
        }
    });

    // 读线程
    std::thread reader([&]() {
        BYTE data[50];
        while (running) {
            buffer.ReadBuffer(data, sizeof(data));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    writer.join();
    reader.join();

    // 验证无崩溃即为成功
    SUCCEED();
}
```

## 运行测试

### 使用 test.bat (推荐)

Windows 下最简单的方式是使用 `test.bat` 脚本：

```batch
cd test

test.bat build           # 构建测试 (14 个可执行文件)
test.bat run             # 运行所有测试 (763 个)
test.bat run client      # 只运行客户端 Buffer 测试 (33)
test.bat run server      # 只运行服务端 Buffer 测试 (40)
test.bat run protocol    # 只运行协议测试 (58)
test.bat run file        # 只运行文件传输测试 (127)
test.bat run network     # 只运行网络通信测试 (80)
test.bat run screen      # 只运行图像处理测试 (425)
test.bat run verbose     # 详细模式运行
test.bat clean           # 清理构建目录
test.bat rebuild         # 清理后重新构建
test.bat help            # 显示帮助
```

### CMake 手动构建

```bash
cd test

# 配置
cmake -B build -G "Visual Studio 17 2022"

# 构建
cmake --build build --config Release

# 运行所有测试
ctest --test-dir build -C Release --output-on-failure

# 运行特定测试
ctest --test-dir build -C Release -R Client   # 客户端测试
ctest --test-dir build -C Release -R Server   # 服务端测试

# 详细输出
ctest --test-dir build -C Release -V
```

### 直接运行可执行文件

```batch
:: Phase 1: Buffer 测试
test\build\Release\client_buffer_test.exe
test\build\Release\server_buffer_test.exe

:: Phase 1: 协议测试
test\build\Release\protocol_test.exe

:: Phase 2: 文件传输测试
test\build\Release\file_transfer_test.exe
test\build\Release\chunk_manager_test.exe
test\build\Release\sha256_verify_test.exe
test\build\Release\resume_state_test.exe

:: Phase 3: 网络通信测试
test\build\Release\header_test.exe
test\build\Release\packet_fragment_test.exe
test\build\Release\http_mask_test.exe

:: Phase 4: 图像处理测试
test\build\Release\diff_algorithm_test.exe
test\build\Release\rgb565_test.exe
test\build\Release\scroll_detector_test.exe
test\build\Release\quality_adaptive_test.exe

:: 使用 gtest 过滤器
header_test.exe --gtest_filter="*Encrypt*"
rgb565_test.exe --gtest_filter="*Conversion*"

:: 列出所有测试（不运行）
quality_adaptive_test.exe --gtest_list_tests

:: 输出 XML 报告
protocol_test.exe --gtest_output=xml:report.xml
```

## CI/CD 集成

### GitHub Actions 示例

```yaml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: windows-latest

    steps:
    - uses: actions/checkout@v4

    - name: Configure CMake
      working-directory: test
      run: cmake -B build -G "Visual Studio 17 2022"

    - name: Build
      working-directory: test
      run: cmake --build build --config Release

    - name: Run Tests
      working-directory: test
      run: ctest --test-dir build -C Release --output-on-failure

    - name: Upload Test Results
      uses: actions/upload-artifact@v4
      if: always()
      with:
        name: test-results
        path: test/build/Release/*.xml
```

## 覆盖率

### 使用 gcov/lcov (Linux)

```bash
# 配置（启用覆盖率）
cmake -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="--coverage"

# 构建并运行测试
cmake --build build
ctest --test-dir build

# 生成报告
lcov --capture --directory build --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

## 当前测试状态

**总计: 763 个测试用例，全部通过** ✅ (执行时间 < 3 秒)

### 已覆盖模块

| 阶段 | 模块 | 测试文件 | 用例数 | 状态 |
|------|------|----------|--------|------|
| Phase 1 | 客户端 Buffer | `unit/client/BufferTest.cpp` | 33 | ✅ |
| Phase 1 | 服务端 Buffer | `unit/server/BufferTest.cpp` | 40 | ✅ |
| Phase 1 | 协议/数据包 | `unit/protocol/PacketTest.cpp` | 39 | ✅ |
| Phase 1 | 路径处理 | `unit/protocol/PathUtilsTest.cpp` | 19 | ✅ |
| Phase 2 | V2 文件传输 | `unit/file/FileTransferV2Test.cpp` | 37 | ✅ |
| Phase 2 | 分块管理 | `unit/file/ChunkManagerTest.cpp` | 36 | ✅ |
| Phase 2 | SHA-256 校验 | `unit/file/SHA256VerifyTest.cpp` | 28 | ✅ |
| Phase 2 | 断点续传 | `unit/file/ResumeStateTest.cpp` | 26 | ✅ |
| Phase 3 | 协议头验证 | `unit/network/HeaderTest.cpp` | 29 | ✅ |
| Phase 3 | 粘包/分包 | `unit/network/PacketFragmentTest.cpp` | 24 | ✅ |
| Phase 3 | HTTP 伪装 | `unit/network/HttpMaskTest.cpp` | 27 | ✅ |
| Phase 4 | 差分算法 | `unit/screen/DiffAlgorithmTest.cpp` | 32 | ✅ |
| Phase 4 | RGB565 压缩 | `unit/screen/RGB565Test.cpp` | 286 | ✅ |
| Phase 4 | 滚动检测 | `unit/screen/ScrollDetectorTest.cpp` | 43 | ✅ |
| Phase 4 | 质量自适应 | `unit/screen/QualityAdaptiveTest.cpp` | 64 | ✅ |
| | **合计** | | **763** | ✅ |

### 测试分布概览

| 阶段 | 测试类别 | 用例数 | 说明 |
|------|---------|--------|------|
| Phase 1 | Buffer 操作 | 73 | 读写、跳过、边界、线程安全 |
| Phase 1 | 协议解析 | 58 | V2 数据包、路径处理 |
| Phase 2 | 文件传输 | 127 | 分块、续传、校验 |
| Phase 3 | 网络通信 | 80 | 包头、分包、伪装 |
| Phase 4 | 图像处理 | 425 | 差分、RGB565、滚动、自适应 |

### 待测试模块

| 优先级 | 模块 | 说明 |
|-------|------|------|
| 中 | 系统管理 | 进程列表、服务管理 |
| 中 | 终端 | 命令解析、输出处理 |
| 低 | 压缩模块 | zstd 压缩/解压 |

## 参考资料

- [Google Test 官方文档](https://google.github.io/googletest/)
- [Google Mock 入门](https://google.github.io/googletest/gmock_for_dummies.html)
- [C++ 单元测试最佳实践](https://github.com/cpp-best-practices/cppbestpractices/blob/master/08-Considering_Safety.md)
