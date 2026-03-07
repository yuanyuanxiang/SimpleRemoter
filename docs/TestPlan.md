# SimpleRemoter 测试计划

本文档定义了项目的测试策略、优先级和实施路线图。

## 测试目标

1. **发现潜在隐患** - 边界条件、异常情况、内存问题
2. **回归保护** - 确保修改不引入新 bug
3. **提升代码质量** - 通过测试驱动改进代码

## 模块概览

### 代码复杂度分析

| 模块 | 客户端文件 | 服务端文件 | LOC | 复杂度 |
|------|-----------|-----------|-----|--------|
| 网络核心 | IOCPClient.cpp | IOCPServer.cpp | 1500+ | 高 |
| 文件传输 | FileManager.cpp | FileManagerDlg.cpp | 4400+ | 高 |
| 远程桌面 | ScreenManager.cpp | ScreenSpyDlg.cpp | 4700+ | 高 |
| 缓冲区 | Buffer.cpp | Buffer.cpp | 500+ | 中 ✅ 已测试 |
| 系统管理 | SystemManager.cpp | SystemDlg.cpp | 800+ | 中 |
| 终端 | ShellManager/ConPTY | ShellDlg/TerminalDlg | 600+ | 中 |
| 注册表 | RegisterManager.cpp | RegisterDlg.cpp | 400+ | 低 |
| 服务管理 | ServicesManager.cpp | ServicesDlg.cpp | 400+ | 低 |

### 协议数据结构

| 结构体 | 文件 | 大小 | 用途 |
|--------|------|------|------|
| FileChunkPacket | file_upload.h | 41B | V1 文件传输 |
| FileChunkPacketV2 | file_upload.h | 77B | V2 文件传输 |
| FileResumePacketV2 | file_upload.h | 49B+ | 断点续传 |
| FileCompletePacketV2 | file_upload.h | 69B | 完成校验 |
| C2CPreparePacket | file_upload.h | 17B | C2C 准备 |
| HeaderFlag | header.h | - | 包头标识 |
| ZstaHeader | ZstdArchive.h | - | 压缩头 |

## 测试优先级

### P0 - 关键（必须测试）

这些是系统核心，问题会导致功能完全失效：

| 模块 | 测试类型 | 测试重点 | 用例数 | 状态 |
|------|---------|---------|--------|------|
| Buffer | 单元测试 | 读写、边界、下溢 | 73 | ✅ 完成 |
| 协议编解码 | 单元测试 | 数据包序列化/反序列化 | 58 | ✅ 完成 |
| 协议头验证 | 单元测试 | 包头加密/解密/版本 | 29 | ✅ 完成 |
| 路径处理 | 单元测试 | 路径拼接、相对路径、特殊字符 | 包含在协议测试 | ✅ 完成 |

### P1 - 高优先级

核心功能，问题会严重影响用户体验：

| 模块 | 测试类型 | 测试重点 | 用例数 | 状态 |
|------|---------|---------|--------|------|
| 文件传输逻辑 | 单元测试 | V2协议、分块、传输选项 | 37 | ✅ 完成 |
| 分块管理 | 单元测试 | 范围管理、合并、缺失检测 | 36 | ✅ 完成 |
| SHA-256 校验 | 单元测试 | 流式哈希、完整性验证 | 28 | ✅ 完成 |
| 断点续传 | 单元测试 | 状态序列化、恢复逻辑 | 26 | ✅ 完成 |
| 粘包/分包 | 单元测试 | 数据包边界处理 | 24 | ✅ 完成 |
| HTTP 伪装 | 单元测试 | 协议伪装/解除 | 27 | ✅ 完成 |
| 屏幕捕获 | 单元测试 | 图像压缩、差分算法 | 425 | ✅ 完成 |

### P2 - 中优先级

重要功能，但问题影响范围有限：

| 模块 | 测试类型 | 测试重点 | 预计用例 |
|------|---------|---------|---------|
| 系统管理 | 单元测试 | 进程列表解析 | 20+ |
| 终端 | 单元测试 | 命令解析、输出处理 | 15+ |
| 压缩模块 | 单元测试 | zstd 压缩/解压 | 15+ |

### P3 - 低优先级

辅助功能，可延后测试：

| 模块 | 测试类型 | 测试重点 | 预计用例 |
|------|---------|---------|---------|
| 注册表 | 单元测试 | 路径解析 | 10+ |
| 服务管理 | 单元测试 | 状态解析 | 10+ |
| 音视频 | 集成测试 | 数据流 | 10+ |

## 实施路线图

### Phase 1: 协议层测试 ✅ 已完成

**目标：** 测试所有数据包的序列化/反序列化

**状态：** 131 个测试全部通过

```
test/
└── unit/
    ├── client/
    │   └── BufferTest.cpp           # 客户端 Buffer (33 用例) ✅
    ├── server/
    │   └── BufferTest.cpp           # 服务端 Buffer (40 用例) ✅
    └── protocol/
        ├── PacketTest.cpp           # 文件传输包、命令解析 (39 用例) ✅
        └── PathUtilsTest.cpp        # 路径处理工具 (19 用例) ✅
```

**发现并修复的问题：**
- Buffer ULONG 下溢漏洞：当 `Skip(len)` 中 `len > m_nDataLength` 时导致下溢

**测试覆盖：**
- V2 文件传输包结构 (FileChunkPacketV2, FileResumePacketV2 等)
- 路径处理工具 (相对路径、特殊字符、边界条件)
- Buffer 读写操作、边界处理、线程安全

### Phase 2: 文件传输测试 ✅ 已完成

**目标：** 确保文件传输逻辑正确

**状态：** 127 个测试全部通过

```
test/
└── unit/
    └── file/
        ├── FileTransferV2Test.cpp       # V2 传输逻辑 (37 用例) ✅
        ├── ChunkManagerTest.cpp         # 分块管理 (36 用例) ✅
        ├── SHA256VerifyTest.cpp         # 文件校验 (28 用例) ✅
        └── ResumeStateTest.cpp          # 断点续传状态 (26 用例) ✅
```

**测试覆盖：**

| 类别 | 测试内容 | 用例数 |
|------|---------|--------|
| V2 传输选项 | FFV2_* 标志组合、TransferID 生成 | 12 |
| 数据包构建 | FileChunkPacketV2、FileQueryResumeV2 等 | 25 |
| 范围管理 | 添加、合并、缺失检测、边界条件 | 36 |
| SHA-256 | 流式计算、空数据、大数据、已知向量 | 28 |
| 断点续传 | 状态序列化/反序列化、恢复请求/响应 | 26 |

**关键发现：**
- 测试使用独立实现验证协议设计正确性
- RangeManager 相邻范围自动合并 (0-100 + 100-200 → 0-200)

### Phase 3: 网络通信测试 ✅ 已完成

**目标：** 验证网络层协议处理的正确性

**状态：** 80 个测试全部通过

```
test/
└── unit/
    └── network/
        ├── HeaderTest.cpp               # 协议头验证 (29 用例) ✅
        ├── PacketFragmentTest.cpp       # 粘包/分包处理 (24 用例) ✅
        └── HttpMaskTest.cpp             # HTTP 伪装 (27 用例) ✅
```

**测试覆盖：**

| 类别 | 测试内容 | 用例数 |
|------|---------|--------|
| 协议头 | FLAG 生成、加密/解密、多版本验证 | 29 |
| 粘包/分包 | 完整包、分片、多包粘连、边界条件 | 24 |
| HTTP 伪装 | GET/POST 伪装、解除、边界检测 | 27 |

**协议细节验证：**
- 包头结构: FLAG(8B) + PackedLen(4B) + OrigLen(4B) = 16 bytes
- 位置相关 XOR 加密 (key ^ (i * 31))
- 支持 HeaderEncV0-V6 多版本兼容

### Phase 4: 图像处理测试 ✅ 已完成

**目标：** 验证屏幕捕获和压缩算法

**状态：** 425 个测试全部通过

```
test/
└── unit/
    └── screen/
        ├── DiffAlgorithmTest.cpp        # 差分算法 (32 用例) ✅
        ├── RGB565Test.cpp               # RGB565 压缩 (286 用例) ✅
        ├── ScrollDetectorTest.cpp       # 滚动检测 (43 用例) ✅
        └── QualityAdaptiveTest.cpp      # 质量自适应 (64 用例) ✅
```

**测试覆盖：**

| 类别 | 测试内容 | 用例数 |
|------|---------|--------|
| 差分算法 | 帧比较、差分编码/解码、SSE2 优化验证 | 32 |
| RGB565 | BGRA↔RGB565 转换、量化误差、批量处理 | 286 |
| 滚动检测 | CRC32 行哈希、垂直滚动检测、边缘区域 | 43 |
| 质量自适应 | RTT→等级映射、防抖逻辑、冷却时间 | 64 |

**关键实现细节：**
- RGB565 量化误差: 5-bit 最大误差 7，6-bit 最大误差 3
- 滚动检测: MIN_SCROLL_LINES=16, MATCH_THRESHOLD=85%
- 质量自适应: 降级 2 次稳定，升级 5 次稳定，分辨率变化冷却 30 秒

### Phase 5: 其他模块 (按需)

根据实际情况逐步扩展测试覆盖。

## 测试基础设施

### 目录结构

```
test/
├── CMakeLists.txt              # 构建配置
├── test.bat                    # Windows 测试脚本
├── unit/                       # 单元测试
│   ├── client/                 # 客户端测试
│   │   └── BufferTest.cpp      # ✅ 已完成
│   ├── server/                 # 服务端测试
│   │   └── BufferTest.cpp      # ✅ 已完成
│   ├── protocol/               # 协议测试 (Phase 1)
│   ├── file/                   # 文件传输测试 (Phase 2)
│   └── screen/                 # 屏幕测试 (Phase 4)
├── integration/                # 集成测试
│   └── network/                # 网络测试 (Phase 3)
├── fixtures/                   # 测试数据
│   ├── images/                 # 测试图像
│   └── files/                  # 测试文件
└── mocks/                      # Mock 对象
    └── MockSocket.h
```

### 测试工具函数

需要创建的辅助函数：

```cpp
// test/TestUtils.h

// 创建临时测试目录
std::wstring CreateTempTestDir();

// 创建测试文件
void CreateTestFile(const std::wstring& path, size_t size);

// 比较文件内容
bool CompareFiles(const std::wstring& file1, const std::wstring& file2);

// 加载测试图像
std::vector<BYTE> LoadTestImage(const std::string& name);

// 模拟网络延迟
void SimulateNetworkDelay(int ms);
```

## 代码覆盖率目标

| 阶段 | 覆盖率目标 | 说明 |
|------|-----------|------|
| Phase 1 | 90%+ | 协议层应接近完全覆盖 |
| Phase 2 | 80%+ | 文件传输核心逻辑 |
| Phase 3 | 70%+ | 网络层关键路径 |
| Phase 4 | 60%+ | 图像处理算法 |

## 持续集成

建议在 GitHub Actions 中自动运行测试：

```yaml
name: Unit Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: windows-latest
    steps:
    - uses: actions/checkout@v4
    - name: Build and Test
      run: |
        cd test
        test.bat rebuild
        test.bat run
```

## 当前进度

**总计: 763 个测试用例，全部通过** ✅

| 阶段 | 模块 | 用例数 | 状态 |
|------|------|--------|------|
| Phase 1 | Buffer (客户端+服务端) | 73 | ✅ |
| Phase 1 | 协议层 (Packet + PathUtils) | 58 | ✅ |
| Phase 2 | 文件传输 | 127 | ✅ |
| Phase 3 | 网络通信 | 80 | ✅ |
| Phase 4 | 图像处理 | 425 | ✅ |
| **合计** | | **763** | ✅ |

## 下一步行动

1. **Phase 5: 其他模块 (按需)**
   - 系统管理模块测试
   - 终端模块测试
   - 压缩模块测试

2. **可选扩展**
   - 集成测试
   - 持续集成配置

## 附录：已发现并修复的问题

| 日期 | 阶段 | 模块 | 问题 | 修复 |
|------|------|------|------|------|
| 2026-03-08 | Phase 1 | Buffer | ULONG 下溢：`Skip(len)` 当 len > m_nDataLength 导致下溢 | 添加防护检查 |

**测试设计说明：**
- Phase 2-3 测试使用独立实现（测试副本）验证协议设计
- 这种方式验证的是协议规范正确性，而非生产代码 bug
- 发现的测试问题：`RangeManagerTest.MergeOutOfOrder` 预期修正（相邻范围自动合并）

---

*文档版本: 2.0*
*最后更新: 2026-03-08*
