#pragma once

#include <string>
#include <vector>
#include <map>

#pragma pack(push, 1)

// ==================== V1 协议（保持不变）====================
struct FileChunkPacket {
    unsigned char       cmd;          // COMMAND_SEND_FILE = 68
    uint32_t            fileIndex;    // 文件编号
    uint32_t            totalNum;     // 文件总数
    uint64_t            fileSize;     // 整个文件大小
    uint64_t            offset;       // 当前块在文件中的偏移
    uint64_t            dataLength;   // 本块数据长度
    uint64_t            nameLength;   // 文件名长度（不含 '\0'）
    // char filename[nameLength];
    // uint8_t data[dataLength];
};  // 41 bytes


// ==================== V2 协议（C2C + 断点续传）====================

// V2 标志位
enum FileFlagsV2 : uint16_t {
    FFV2_NONE         = 0x0000,
    FFV2_LAST_CHUNK   = 0x0001,   // 最后一块
    FFV2_RESUME_REQ   = 0x0002,   // 请求续传信息
    FFV2_RESUME_RESP  = 0x0004,   // 续传信息响应
    FFV2_CANCEL       = 0x0008,   // 取消传输
    FFV2_DIRECTORY    = 0x0010,   // 目录项（不是文件）
    FFV2_COMPRESSED   = 0x0020,   // 数据已压缩
    FFV2_ERROR        = 0x0040,   // 传输错误
};

// V2 文件传输包
struct FileChunkPacketV2 {
    uint8_t     cmd;            // COMMAND_SEND_FILE_V2 = 85
    uint64_t    transferID;     // 传输会话ID（唯一标识一次传输任务）
    uint64_t    srcClientID;    // 源客户端 (0=主控端)
    uint64_t    dstClientID;    // 目标客户端 (0=主控端)
    uint32_t    fileIndex;      // 文件编号 (0-based)
    uint32_t    totalFiles;     // 总文件数
    uint64_t    fileSize;       // 当前文件大小
    uint64_t    offset;         // 当前块在文件中的偏移
    uint64_t    dataLength;     // 本块数据长度
    uint64_t    nameLength;     // 文件名长度
    uint16_t    flags;          // FileFlagsV2 组合
    uint16_t    checksum;       // CRC16（可选）
    uint8_t     reserved[8];    // 预留扩展
    // char filename[nameLength];   // UTF-8 相对路径
    // uint8_t data[dataLength];    // 文件数据
};  // 77 bytes

// V2 断点续传区间
struct FileRangeV2 {
    uint64_t    offset;         // 起始偏移
    uint64_t    length;         // 长度
};  // 16 bytes

// V2 断点续传控制包
struct FileResumePacketV2 {
    uint8_t     cmd;            // COMMAND_FILE_RESUME = 86
    uint64_t    transferID;     // 传输会话ID
    uint64_t    srcClientID;    // 源客户端
    uint64_t    dstClientID;    // 目标客户端
    uint32_t    fileIndex;      // 文件编号
    uint64_t    fileSize;       // 文件大小
    uint64_t    receivedBytes;  // 已接收字节数
    uint16_t    flags;          // FFV2_RESUME_REQ 或 FFV2_RESUME_RESP
    uint16_t    rangeCount;     // 已接收区间数量
    // FileRangeV2 ranges[rangeCount];  // 已接收的区间列表
};  // 49 bytes + ranges

// V2 剪贴板请求（C2C 触发）
struct ClipboardRequestV2 {
    uint8_t     cmd;            // COMMAND_CLIPBOARD_V2 = 87
    uint64_t    srcClientID;    // 源客户端（发送方）
    uint64_t    dstClientID;    // 目标客户端（接收方）
    uint64_t    transferID;     // 传输ID
    char        hash[64];       // 认证哈希
    char        hmac[16];       // HMAC
};  // 105 bytes

// C2C 准备接收通知（发给目标客户端，让它捕获当前目录）
struct C2CPreparePacket {
    uint8_t     cmd;            // COMMAND_C2C_PREPARE = 89
    uint64_t    transferID;     // 传输ID（与后续文件关联）
    uint64_t    srcClientID;    // 源客户端ID（发送方，用于返回响应）
};  // 17 bytes

// C2C 准备响应（返回目标目录给发送方）
struct C2CPrepareRespPacket {
    uint8_t     cmd;            // COMMAND_C2C_PREPARE_RESP = 92
    uint64_t    transferID;     // 传输ID
    uint64_t    srcClientID;    // 原始发送方客户端ID
    uint16_t    pathLength;     // 目录路径长度
    // char path[pathLength];   // UTF-8 目标目录
};  // 19 bytes + path

// V2 文件完成校验包
struct FileCompletePacketV2 {
    uint8_t     cmd;            // COMMAND_FILE_COMPLETE_V2 = 91
    uint64_t    transferID;     // 传输ID
    uint64_t    srcClientID;    // 源客户端
    uint64_t    dstClientID;    // 目标客户端
    uint32_t    fileIndex;      // 文件编号
    uint64_t    fileSize;       // 文件大小
    uint8_t     sha256[32];     // SHA-256 哈希
};  // 69 bytes

// C2C 准备接收：捕获当前目录并存储（由 COMMAND_C2C_PREPARE 调用，接收方使用）
void SetC2CTargetFolder(uint64_t transferID);

// C2C 发送方：存储目标目录（收到 C2C_PREPARE_RESP 后调用）
void SetSenderTargetFolder(uint64_t transferID, const std::wstring& targetDir);

// C2C 发送方：获取目标目录（用于构建完整路径）
std::wstring GetSenderTargetFolder(uint64_t transferID);

// V2 错误码
enum FileErrorV2 : uint8_t {
    FEV2_OK               = 0,
    FEV2_TARGET_OFFLINE   = 1,   // 目标客户端离线
    FEV2_VERSION_MISMATCH = 2,   // 版本不匹配
    FEV2_FILE_NOT_FOUND   = 3,   // 文件不存在
    FEV2_ACCESS_DENIED    = 4,   // 访问被拒绝
    FEV2_DISK_FULL        = 5,   // 磁盘空间不足
    FEV2_TRANSFER_CANCEL  = 6,   // 传输被取消
    FEV2_CHECKSUM_ERROR   = 7,   // 校验和错误（块级CRC）
    FEV2_HASH_MISMATCH    = 8,   // 哈希不匹配（文件级SHA-256）
};

// V2 续传查询包（发送方在传输前发送）
struct FileQueryResumeV2 {
    uint8_t     cmd;            // COMMAND_FILE_QUERY_RESUME = 88
    uint64_t    transferID;     // 传输会话ID（用于C2C目标目录匹配）
    uint64_t    srcClientID;    // 源客户端
    uint64_t    dstClientID;    // 目标客户端 (0=主控端)
    uint32_t    fileCount;      // 文件数量
    // 后跟 fileCount 个 FileQueryResumeEntryV2
};  // 29 bytes + entries

// 续传查询条目
struct FileQueryResumeEntryV2 {
    uint64_t    fileSize;       // 文件大小
    uint16_t    nameLength;     // 文件名长度
    // char filename[nameLength]; // UTF-8 相对路径
};  // 10 bytes + filename

// V2 续传查询响应包
struct FileResumeResponseV2 {
    uint8_t     cmd;            // COMMAND_FILE_RESUME = 86
    uint64_t    srcClientID;    // 原始的源客户端
    uint64_t    dstClientID;    // 原始的目标客户端
    uint16_t    flags;          // FFV2_RESUME_RESP
    uint32_t    fileCount;      // 响应条目数
    // 后跟 fileCount 个 FileResumeResponseEntryV2
};  // 23 bytes + entries

// 续传响应条目
struct FileResumeResponseEntryV2 {
    uint32_t    fileIndex;      // 文件编号
    uint64_t    receivedBytes;  // 已接收字节数（0=需要完整传输）
};  // 12 bytes

#pragma pack(pop)

typedef void (*LogFunc)(const char* file, int line, const char* format, ...);

int InitFileUpload(const std::string& key, const std::string& msg, const std::string& hmac,
                   int chunkSizeKb = 64, int sendDurationMs = 50, LogFunc logFunc = NULL);

int UninitFileUpload();

std::vector<std::string> GetClipboardFiles(int& result);

bool GetCurrentFolderPath(std::string& outDir);

typedef bool (*OnTransform)(void* user, FileChunkPacket* chunk, unsigned char* data, int size);

typedef void (*OnFinish)(void* user);

int FileBatchTransferWorker(const std::vector<std::string>& files, const std::string& targetDir,
                            void* user, OnTransform f, OnFinish finish, const std::string& hash, const std::string& hmac);

int RecvFileChunk(char* buf, size_t len, void* user, OnFinish f, const std::string& hash, const std::string& hmac);


// ==================== V2 接口 ====================

// V2 传输选项
struct TransferOptionsV2 {
    uint64_t    transferID;     // 传输ID (0=自动生成)
    uint64_t    srcClientID;    // 源客户端ID (0=主控端)
    uint64_t    dstClientID;    // 目标客户端ID (0=主控端)
    bool        enableResume;   // 启用断点续传
    std::map<uint32_t, uint64_t> startOffsets;  // 每个文件的起始偏移 (fileIndex -> offset)

    TransferOptionsV2() : transferID(0), srcClientID(0), dstClientID(0), enableResume(true) {}
};

// V2 发送回调
typedef bool (*OnTransformV2)(void* user, FileChunkPacketV2* chunk, unsigned char* data, int size);

// V2 文件发送
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

// V2 文件接收
int RecvFileChunkV2(
    char* buf, size_t len,
    void* user,
    OnFinish f,
    const std::string& hash,
    const std::string& hmac,
    uint64_t myClientID
);

// 生成传输ID
uint64_t GenerateTransferID();

// 断点续传：保存状态
bool SaveResumeState(uint64_t transferID, const char* targetDir);

// 断点续传：加载状态
bool LoadResumeState(uint64_t transferID);

// 断点续传：清理状态
void CleanupResumeState(uint64_t transferID);

// 断点续传：获取未完成的传输
std::vector<uint64_t> GetPendingTransfers();

// 传输取消：标记传输为已取消（由接收到 FFV2_CANCEL 的模块调用）
void CancelTransfer(uint64_t transferID);

// 续传协商：设置待处理的偏移响应（接收线程调用）
void SetPendingResumeOffsets(const std::map<uint32_t, uint64_t>& offsets);

// 续传协商：获取并清除待处理的偏移（发送线程调用，阻塞等待）
bool WaitForResumeOffsets(std::map<uint32_t, uint64_t>& offsets, int timeoutMs = 5000);

// 续传协商：构建查询包（发送方在传输前发送）
// files: 要发送的文件列表 (相对路径 + 大小)
std::vector<uint8_t> BuildResumeQuery(
    uint64_t transferID,
    uint64_t srcClientID,
    uint64_t dstClientID,
    const std::vector<std::pair<std::string, uint64_t>>& files  // (relativePath, fileSize)
);

// 续传协商：处理查询请求，返回响应包
// 接收方调用，根据文件名+大小查找已有的接收状态
std::vector<uint8_t> HandleResumeQuery(
    const char* buf, size_t len
);

// 续传协商：解析响应包，获取每个文件的已接收偏移
// offsets: 输出参数，fileIndex -> receivedBytes
bool ParseResumeResponse(
    const char* buf, size_t len,
    std::map<uint32_t, uint64_t>& offsets  // fileIndex -> receivedBytes
);

// 断点续传：构建续传请求包（客户端作为接收方时使用）
// 返回完整的包数据，包含已接收的区间信息
std::vector<uint8_t> BuildResumeRequest(uint64_t transferID, uint64_t myClientID);

// 断点续传：解析续传请求/响应包，返回缺失的区间
// missingRanges: 输出参数，返回需要重传的区间
bool ParseResumePacket(const char* buf, size_t len,
    uint64_t& transferID, uint64_t& fileSize,
    std::vector<std::pair<uint64_t, uint64_t>>& receivedRanges);

// 断点续传：从指定偏移继续发送文件
// skipRanges: 已接收的区间，发送时跳过这些部分
int FileSendFromOffset(
    const std::string& filePath,
    const std::string& targetName,
    uint64_t fileSize,
    const std::vector<std::pair<uint64_t, uint64_t>>& skipRanges,
    void* user,
    OnTransformV2 f,
    const TransferOptionsV2& options
);

// 获取传输的详细状态（用于构建 RESUME_RESP）
struct TransferStateInfo {
    uint64_t transferID;
    uint64_t fileSize;
    uint64_t receivedBytes;
    std::vector<std::pair<uint64_t, uint64_t>> receivedRanges;
    std::string filePath;
};
bool GetTransferState(uint64_t transferID, uint32_t fileIndex, TransferStateInfo& info);


// ==================== V2 文件完整性校验 ====================

// 处理文件完成校验包（接收方调用）
// 返回: true=校验通过, false=校验失败
bool HandleFileCompleteV2(const char* buf, size_t len, uint64_t myClientID);


// ==================== 通用工具 ====================

uint8_t* ScaleBitmap(uint8_t* dst, const uint8_t* src, int srcW, int srcH, int dstW, int dstH, int instructionSet);

std::vector<std::string> PreprocessFilesSimple(const std::vector<std::string>& inputFiles);

std::vector<char> BuildMultiStringPath(const std::vector<std::string>& paths);

std::vector<std::string> ParseMultiStringPath(const char* buffer, size_t size);

std::vector<std::string> GetForegroundSelectedFiles(int &result);

// 获取文件列表的公共根目录
std::string GetCommonRoot(const std::vector<std::string>& files);

// 获取相对路径
std::string GetRelativePath(const std::string& root, const std::string& fullPath);
