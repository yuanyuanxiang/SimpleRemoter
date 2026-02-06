#pragma once

#include <string>
#include <vector>

#pragma pack(push, 1)
struct FileChunkPacket {
    unsigned char       cmd;          // COMMAND_SEND_FILE
    uint32_t			fileIndex;	  // 文件编号
    uint32_t			totalNum;	  // 文件总数
    uint64_t            fileSize;     // 整个文件大小
    uint64_t            offset;       // 当前块在文件中的偏移
    uint64_t            dataLength;   // 本块数据长度
    uint64_t            nameLength;   // 文件名长度（不含 '\0'）
};
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

uint8_t* ScaleBitmap(uint8_t* dst, const uint8_t* src, int srcW, int srcH, int dstW, int dstH);

std::vector<std::string> PreprocessFilesSimple(const std::vector<std::string>& inputFiles);

std::vector<char> BuildMultiStringPath(const std::vector<std::string>& paths);

std::vector<std::string> ParseMultiStringPath(const char* buffer, size_t size);

std::vector<std::string> GetForegroundSelectedFiles(int &result);
