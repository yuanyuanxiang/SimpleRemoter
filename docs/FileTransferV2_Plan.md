# 文件传输 V2 协议方案

> 支持 C2C（客户端到客户端）传输 + 断点续传

## 实施进度

| 阶段 | 内容 | 状态 | 完成日期 |
|------|------|------|---------|
| Phase 1 | V2 结构体 + 接口定义 | ✅ 完成 | 2026-02-23 |
| Phase 2 | V2 基础收发（不含 C2C） | ✅ 完成 | 2026-02-23 |
| Phase 3 | 断点续传 | ✅ 完成 | 2026-02-23 |
| Phase 4 | C2C | ✅ 完成 | 2026-02-23 |
| Phase 5 | SHA-256 文件校验 | ✅ 完成 | 2026-02-26 |
| Phase 6 | 能力协商 + 服务端开关 + 缓存整合 | ✅ 完成 | 2026-02-27 |

---

## 一、命令设计

```cpp
// common/commands.h

// 老命令（保持不变）
COMMAND_SEND_FILE       = 68,   // V1 文件传输

// 新命令（V2 独立）
COMMAND_SEND_FILE_V2      = 85,   // V2 文件传输（支持 C2C + 断点续传）
COMMAND_FILE_RESUME       = 86,   // V2 断点续传控制
COMMAND_CLIPBOARD_V2      = 87,   // V2 剪贴板请求（C2C）
COMMAND_FILE_QUERY_RESUME = 88,   // V2 断点续传查询
COMMAND_C2C_PREPARE       = 89,   // C2C 准备接收通知
COMMAND_C2C_TEXT          = 90,   // C2C 文本传输
COMMAND_FILE_COMPLETE_V2  = 91,   // V2 文件完成校验（SHA-256）
COMMAND_C2C_PREPARE_RESP  = 92,   // C2C 准备响应（返回目标目录）

// 客户端能力位
#define CLIENT_CAP_V2   0x0001    // 支持 V2 文件传输

// 功能引入日期
#define FILE_TRANSFER_V2_DATE  "Feb 27 2026"
```

---

## 二、结构体设计

```cpp
// common/file_upload.h

#pragma pack(push, 1)

// ==================== V1 协议（不改动）====================
struct FileChunkPacket {
    uint8_t     cmd;            // = 68
    uint32_t    fileIndex;
    uint32_t    totalNum;
    uint64_t    fileSize;
    uint64_t    offset;
    uint64_t    dataLength;
    uint64_t    nameLength;
};  // 41 bytes


// ==================== V2 协议（新增，独立）====================
struct FileChunkPacketV2 {
    uint8_t     cmd;            // = 85
    uint64_t    transferID;     // 传输会话ID
    uint64_t    srcClientID;    // 源客户端 (0=主控端)
    uint64_t    dstClientID;    // 目标客户端 (0=主控端)
    uint32_t    fileIndex;      // 文件编号
    uint32_t    totalFiles;     // 总文件数
    uint64_t    fileSize;       // 文件大小
    uint64_t    offset;         // 偏移
    uint64_t    dataLength;     // 数据长度
    uint64_t    nameLength;     // 文件名长度
    uint16_t    flags;          // 标志位
    uint16_t    checksum;       // CRC16
    uint8_t     reserved[8];    // 预留
};  // 81 bytes

enum FileFlagsV2 : uint16_t {
    FFV2_NONE         = 0x0000,
    FFV2_LAST_CHUNK   = 0x0001,
    FFV2_RESUME_REQ   = 0x0002,
    FFV2_RESUME_RESP  = 0x0004,
    FFV2_CANCEL       = 0x0008,
    FFV2_DIRECTORY    = 0x0010,
    FFV2_COMPRESSED   = 0x0020,
    FFV2_ERROR        = 0x0040,
};

struct FileResumePacketV2 {
    uint8_t     cmd;            // = 86
    uint64_t    transferID;
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    uint32_t    fileIndex;
    uint64_t    fileSize;
    uint64_t    receivedBytes;
    uint16_t    flags;
    uint16_t    rangeCount;
    // FileRangeV2 ranges[rangeCount];
};  // 51 bytes + ranges

struct FileRangeV2 {
    uint64_t    offset;
    uint64_t    length;
};  // 16 bytes

struct ClipboardRequestV2 {
    uint8_t     cmd;            // = 87
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    uint64_t    transferID;
    char        hash[64];
    char        hmac[16];
};  // 105 bytes

struct C2CPreparePacket {
    uint8_t     cmd;            // = 89
    uint64_t    transferID;
    uint64_t    srcClientID;    // 发送方客户端ID
};  // 17 bytes

struct C2CPrepareRespPacket {
    uint8_t     cmd;            // = 92
    uint64_t    transferID;
    uint64_t    srcClientID;    // 原始发送方客户端ID
    uint16_t    pathLength;
    // char path[pathLength];   // UTF-8 目标目录
};  // 19 bytes + path

struct FileCompletePacketV2 {
    uint8_t     cmd;            // = 91
    uint64_t    transferID;
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    uint32_t    fileIndex;
    uint64_t    fileSize;
    uint8_t     sha256[32];     // SHA-256 哈希
};  // 69 bytes

#pragma pack(pop)
```

---

## 三、接口设计

```cpp
// common/file_upload.h

// ==================== V1 接口（保持不变）====================
int FileBatchTransferWorker(...);
int RecvFileChunk(...);

// ==================== V2 接口（新增）====================
struct TransferOptionsV2 {
    uint64_t    transferID;     // 0=自动生成
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    bool        enableResume;   // 启用断点续传
};

int FileBatchTransferWorkerV2(
    const std::vector<std::string>& files,
    const std::string& targetDir,
    void* user,
    OnTransformV2 f,
    OnFinish finish,
    const std::string& hash,
    const std::string& hmac,
    const TransferOptionsV2& options
);

int RecvFileChunkV2(
    char* buf, size_t len,
    void* user,
    OnFinish f,
    const std::string& hash,
    const std::string& hmac,
    uint64_t myClientID
);

// 断点续传
uint64_t GenerateTransferID();
bool SaveResumeState(uint64_t transferID, ...);
bool LoadResumeState(uint64_t transferID, ...);
void CleanupResumeState(uint64_t transferID);
std::vector<uint64_t> GetPendingTransfers();

// 文件完整性校验
bool HandleFileCompleteV2(
    const char* buf, size_t len,
    uint64_t myClientID
);
```

---

## 四、持久化设计

### 4.1 状态文件位置

所有状态文件统一存放在：
- Windows: `%TEMP%\FileTransfer\`
- Linux: `/tmp/FileTransfer/`

### 4.2 状态文件类型

| 后缀 | 用途 | 文件名格式 |
|------|------|-----------|
| `.resume` | 断点续传状态 | `{transferID}.resume` |
| `.recv` | C2C 接收方目标目录 | `{transferID}.recv` |
| `.send` | C2C 发送方文件路径 | `{transferID}.send` |

### 4.3 自动清理

- 状态文件超过 7 天自动删除 (`RESUME_EXPIRE_DAYS = 7`)
- 清理在 `GetPendingTransfers()` 时触发
- 三种后缀文件均会被清理

### 4.4 .resume 文件结构

```cpp
struct ResumeFileHeader {
    uint32_t    magic;              // = 0x52455355 "RESU"
    uint64_t    transferID;
    uint64_t    srcClientID;
    uint64_t    dstClientID;
    uint32_t    totalFiles;
    char        targetDir[260];
};

struct ResumeFileEntry {
    uint64_t    fileSize;
    uint64_t    receivedBytes;
    uint16_t    rangeCount;
    char        fileName[260];
    // FileRangeV2 ranges[rangeCount];
};
```

### 4.5 .recv / .send 文件结构

简单文本文件，存储 UTF-8 编码的目录路径或文件路径列表（每行一个）。

---

## 五、服务端路由

```cpp
// 2015RemoteDlg.cpp

case COMMAND_SEND_FILE: {
    // V1 逻辑（保持不变）
}

case COMMAND_SEND_FILE_V2: {
    // V2 逻辑（独立）
    FileChunkPacketV2* pkt = (FileChunkPacketV2*)szBuffer;
    if (pkt->dstClientID == 0) {
        HandleLocalReceiveV2(...);
    } else {
        // C2C 转发
        ForwardToClientV2(...);
    }
}

case COMMAND_FILE_RESUME: {
    // V2 断点续传
}
```

---

## 六、版本检测

### 6.1 能力位协商

```cpp
// common/commands.h
#define CLIENT_CAP_V2   0x0001      // 支持 V2 文件传输

// LOGIN_INFOR 构造函数
sprintf_s(moduleVersion, "%s-%04X", DLL_VERSION, CLIENT_CAP_V2);
// 结果示例: "Feb 27 2026-0001"
```

### 6.2 服务端检测

```cpp
// 双重检测：能力位 + 版本日期
bool SupportsFileTransferV2(context* ctx) {
    // 1. 检查服务端开关
    if (!g_2015RemoteDlg || !g_2015RemoteDlg->m_bEnableFileV2) return false;

    // 2. 检查能力位
    if (ctx->SupportsFileV2()) return true;

    // 3. 兼容旧版：检查版本日期
    CString version = ctx->GetClientData(ONLINELIST_VERSION);
    return IsDateGreaterOrEqual(version, FILE_TRANSFER_V2_DATE);
}

// C2C 要求双方都支持 V2
if (!SupportsFileTransferV2(src) || !SupportsFileTransferV2(dst)) {
    // 提示版本不支持
}
```

### 6.3 服务端开关

| 菜单位置 | 默认值 | 配置键 |
|---------|--------|--------|
| 参数 → 文件传输V2 | 关闭 | `settings/EnableFileV2` |

- 未勾选时使用 V1 协议
- 勾选后根据客户端能力选择 V1/V2
- 配置通过 `THIS_CFG` 持久化

---

## 七、行为矩阵

| 场景 | 源版本 | 目标版本 | 协议 | 结果 |
|------|--------|---------|------|------|
| 本地→远程 | - | V1 | V1 | ✅ 正常 |
| 本地→远程 | - | V2 | V2 | ✅ 正常 + 断点续传 |
| 远程→本地 | V1 | - | V1 | ✅ 正常 |
| 远程→本地 | V2 | - | V2 | ✅ 正常 + 断点续传 |
| C2C | V1 | V1 | - | ❌ 不支持 |
| C2C | V1 | V2 | - | ❌ 不支持（提示升级） |
| C2C | V2 | V1 | - | ❌ 不支持（提示升级） |
| C2C | V2 | V2 | V2 | ✅ 正常 + 断点续传 |

---

## 八、改动文件清单

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `common/commands.h` | 修改 | 新增命令字、能力位 |
| `common/file_upload.h` | 修改 | 新增 V2 结构体和接口 |
| `SimplePlugins/file_upload.cpp` | 修改 | V2 实现、缓存整合、自动清理 |
| `client/ScreenManager.cpp` | 修改 | 新增 case 分支 |
| `client/KernelManager.cpp` | 修改 | 处理 V2 命令 |
| `server/2015Remote/context.h` | 修改 | 新增能力位列、SupportsFileV2() |
| `server/2015Remote/2015RemoteDlg.h` | 修改 | 新增 m_bEnableFileV2、函数声明 |
| `server/2015Remote/2015RemoteDlg.cpp` | 修改 | case 分支、菜单处理、能力解析 |
| `server/2015Remote/resource.h` | 修改 | 新增菜单ID |
| `server/2015Remote/2015Remote.rc` | 修改 | 新增菜单项 |
| `server/2015Remote/CDlgFileSend.cpp` | 修改 | C2C 校验包转发 |
| `server/2015Remote/ScreenSpyDlg.cpp` | 修改 | 剪贴板同步 |

---

## 九、实施记录

### Phase 1: V2 结构体 + 接口定义 ✅

**状态**: 已完成

**完成时间**: 2026-02-23

**改动文件**:
- [x] `common/commands.h`
  - 新增 `FEATURE_FILE_V2 = "Mar  1 2026"`
  - 新增 `COMMAND_SEND_FILE_V2 = 85`
  - 新增 `COMMAND_FILE_RESUME = 86`
  - 新增 `COMMAND_CLIPBOARD_V2 = 87`

- [x] `common/file_upload.h`
  - 新增 `FileFlagsV2` 枚举
  - 新增 `FileChunkPacketV2` 结构体 (81 bytes)
  - 新增 `FileRangeV2` 结构体 (16 bytes)
  - 新增 `FileResumePacketV2` 结构体 (51 bytes + ranges)
  - 新增 `ClipboardRequestV2` 结构体 (105 bytes)
  - 新增 `FileErrorV2` 错误码枚举
  - 新增 `TransferOptionsV2` 结构体
  - 新增 `FileBatchTransferWorkerV2()` 接口声明
  - 新增 `RecvFileChunkV2()` 接口声明
  - 新增断点续传相关接口声明

---

### Phase 2: V2 基础收发 ✅

**状态**: 已完成

**完成时间**: 2026-02-23

**改动文件**:
- [x] `SimplePlugins/file_upload.cpp` - 实现 `FileBatchTransferWorkerV2()`
- [x] `SimplePlugins/file_upload.cpp` - 实现 `RecvFileChunkV2()`
- [x] `SimplePlugins/file_upload.cpp` - 实现 `GenerateTransferID()`
- [x] `client/ScreenManager.cpp` - 新增 `COMMAND_SEND_FILE_V2` case
- [x] `server/2015Remote/2015RemoteDlg.cpp` - 新增 `COMMAND_SEND_FILE_V2` case

---

### Phase 3: 断点续传 ✅

**状态**: 已完成

**完成时间**: 2026-02-23

**改动文件**:
- [x] `SimplePlugins/file_upload.cpp` - `.resume` 文件头结构体定义
- [x] `SimplePlugins/file_upload.cpp` - 实现 `GetResumeDir()` / `GetResumeFilePath()`
- [x] `SimplePlugins/file_upload.cpp` - 实现 `SaveResumeState()` 保存传输状态
- [x] `SimplePlugins/file_upload.cpp` - 实现 `LoadResumeState()` 恢复传输状态
- [x] `SimplePlugins/file_upload.cpp` - 实现 `CleanupResumeState()` 清理状态文件
- [x] `SimplePlugins/file_upload.cpp` - 实现 `GetPendingTransfers()` 枚举未完成传输
- [x] `SimplePlugins/file_upload.cpp` - `RecvFileChunkV2()` 中定期自动保存状态
- [x] `client/ScreenManager.cpp` - `OnReconnect()` 中检测并恢复未完成传输
- [x] `client/ScreenManager.cpp` - 处理 `COMMAND_FILE_RESUME` 续传控制
- [x] `server/2015Remote/2015RemoteDlg.cpp` - 处理/转发 `COMMAND_FILE_RESUME`

**实现细节**:
- `.resume` 文件存储位置: `%TEMP%\FileTransfer\{transferID}.resume`
- 自动保存触发条件: 每 5 秒或每 1MB 新数据
- 重连时自动恢复本地状态，等待数据继续传输
- 所有文件完成后自动清理 `.resume` 文件

---

### Phase 4: C2C ✅

**状态**: 已完成

**完成时间**: 2026-02-23

**改动文件**:
- [x] `server/2015Remote/2015RemoteDlg.cpp` - 键盘钩子新增静态变量 `remoteCtrlCTime`
- [x] `server/2015Remote/2015RemoteDlg.cpp` - Ctrl+C 时区分本地/远程，记录时间
- [x] `server/2015Remote/2015RemoteDlg.cpp` - 键盘钩子分支[3]：远程A→远程B
- [x] `server/2015Remote/2015RemoteDlg.cpp` - 发送 `COMMAND_CLIPBOARD_V2` 触发 C2C
- [x] `client/ScreenManager.cpp` - 处理 `COMMAND_CLIPBOARD_V2`，获取剪贴板并发送到目标
- [x] `SimplePlugins/file_upload.cpp` - 新增 C2C 文件跟踪 (`g_c2cReceivedFiles`)
- [x] `SimplePlugins/file_upload.cpp` - 新增 `SetFilesToClipboard()` 函数
- [x] `SimplePlugins/file_upload.cpp` - `RecvFileChunkV2()` 完成后设置剪贴板

**实现细节**:
- 远程A 按 Ctrl+C → 记录 `operateWnd` 和 `remoteCtrlCTime`
- 切换到远程B 按 Ctrl+V → 检测 `operateWnd != dlg` 且时间有效
- 服务端发送 `COMMAND_CLIPBOARD_V2` 到源客户端A
- 客户端A 获取剪贴板文件，使用 V2 协议发送到客户端B
- 客户端B 接收文件，传输完成后自动设置到剪贴板 (CF_HDROP)

---

### Phase 5: SHA-256 文件校验 ✅

**状态**: 已完成

**完成时间**: 2026-02-26

**改动文件**:
- [x] `common/commands.h` - 新增 `COMMAND_FILE_COMPLETE_V2 = 91`
- [x] `common/file_upload.h` - 新增 `FileCompletePacketV2` 结构体 (69 bytes)
- [x] `SimplePlugins/file_upload.cpp` - 实现 `SHA256Context` 类 (Windows bcrypt API)
- [x] `SimplePlugins/file_upload.cpp` - `FileRecvStateV2` 新增 `sha256Ctx` 流式计算
- [x] `SimplePlugins/file_upload.cpp` - 实现 `HandleFileCompleteV2()` 校验函数
- [x] `SimplePlugins/file_upload.cpp` - `FileBatchTransferWorkerV2()` 发送完成后发送校验包
- [x] `client/KernelManager.cpp` - 处理 `COMMAND_FILE_COMPLETE_V2`
- [x] `client/ScreenManager.cpp` - 处理 `COMMAND_FILE_COMPLETE_V2`
- [x] `server/2015Remote/2015RemoteDlg.cpp` - 处理/转发 `COMMAND_FILE_COMPLETE_V2`
- [x] `server/2015Remote/CDlgFileSend.cpp` - 处理/转发 C2C 校验包

**实现细节**:
- 使用 Windows bcrypt API (`BCRYPT_SHA256_ALGORITHM`) 计算 SHA-256
- 接收端在写入数据时同步更新 SHA-256（流式计算，无需重读文件）
- 每个文件传输完成后，发送端发送 `FILE_COMPLETE_V2` 校验包
- 接收端收到校验包后对比本地计算的哈希值
- C2C 场景下服务端负责转发校验包到目标客户端

**支持的传输方向**:
| 方向 | 状态 |
|------|------|
| 客户端 → 服务端 | ✅ 已测试 |
| 服务端 → 客户端 | ✅ 已测试 |
| 客户端A → 客户端B (C2C) | ✅ 已测试 |

---

### Phase 5.1: SHA-256 校验 Bug 修复 ✅

**状态**: 已完成

**完成时间**: 2026-02-26

**修复的问题**:

| 问题 | 原因 | 修复 |
|------|------|------|
| C2C 校验包未转发 | `CDlgFileSend` 未检查 `dstClientID` | 检查并转发到目标客户端 |
| 断点续传校验失败 | 从 `.resume` 恢复时 `sha256Valid` 仍为 true | 恢复时设为 false，强制从文件重算 |
| 已完成状态被复用 | 内存匹配未排除已完成的状态 | 增加 `receivedBytes < fileSize` 条件 |
| 不同目录文件被误匹配 | C2C 续传按文件名匹配，忽略目标目录 | 匹配时包含目标目录 |

**改动文件**:
- [x] `server/2015Remote/CDlgFileSend.cpp` - C2C 校验包转发
- [x] `SimplePlugins/file_upload.cpp` - 断点续传时设 `sha256Valid=false`
- [x] `SimplePlugins/file_upload.cpp` - 内存状态匹配跳过已完成
- [x] `SimplePlugins/file_upload.cpp` - C2C 续传匹配包含目标目录

**设计决策**:
- C2C 断点续传匹配时包含目标目录，避免跨目录误匹配
- 断点续传时从文件重新计算 SHA-256（无法恢复流式上下文）
- 校验失败自动删除损坏文件

---

### Phase 6: 能力协商 + 服务端开关 + 缓存整合 ✅

**状态**: 已完成

**完成时间**: 2026-02-27

**改动内容**:

1. **能力位协商**
   - `LOGIN_INFOR` 构造函数在版本字符串后附加能力位（格式：`Feb 27 2026-0001`）
   - `context.h` 新增 `ONLINELIST_CAPABILITIES` 列、`SupportsFileV2()` 方法
   - `AddList()` 解析能力位并存储到列表项

2. **服务端开关**
   - 参数菜单新增 "文件传输V2" 选项（`ID_PARAM_FILE_V2`）
   - `m_bEnableFileV2` 类成员控制 V2 开关
   - 配置通过 `THIS_CFG` 持久化到 `settings/EnableFileV2`
   - `SupportsFileTransferV2()` 统一判断函数

3. **缓存目录整合**
   - 原目录：
     - `%TEMP%\FileTransfer\` (.resume)
     - `%LOCALAPPDATA%\ServerD11\c2c_recv_targets\` (.target)
     - `%LOCALAPPDATA%\ServerD11\c2c_targets\` (.target)
   - 整合后：`%TEMP%\FileTransfer\` 统一存放 `.resume`、`.recv`、`.send`
   - 移除 `GetC2CTargetDir()` 函数
   - `CleanupExpiredStateFiles()` 处理三种文件类型

4. **自动清理**
   - `RESUME_EXPIRE_DAYS = 7`
   - `GetPendingTransfers()` 时触发清理
   - 检查 `ftLastWriteTime` 判断过期

**改动文件**:
- [x] `common/commands.h` - 新增 `CLIENT_CAP_V2`
- [x] `server/2015Remote/context.h` - 新增 `ONLINELIST_CAPABILITIES`、`SupportsFileV2()`
- [x] `server/2015Remote/2015RemoteDlg.h` - 新增 `m_bEnableFileV2`、声明 `SupportsFileTransferV2()`
- [x] `server/2015Remote/2015RemoteDlg.cpp` - 实现菜单处理、能力解析、判断函数
- [x] `server/2015Remote/resource.h` - 新增 `ID_PARAM_FILE_V2`
- [x] `server/2015Remote/2015Remote.rc` - 新增菜单项
- [x] `SimplePlugins/file_upload.cpp` - 整合缓存目录、自动清理
