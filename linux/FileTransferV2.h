#pragma once
#include "common/commands.h"
#include "common/file_upload.h"
#include "client/IOCPClient.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <iconv.h>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <fstream>
#include <thread>

// ============== Linux V2 File Transfer ==============
// Implements V2 file transfer protocol for Linux client
// Supports: receive files from server/C2C, send files to server/C2C

class FileTransferV2
{
public:
    // V2 文件接收状态
    struct RecvState {
        uint64_t transferID;
        uint64_t fileSize;
        uint64_t receivedBytes;
        std::string filePath;
        int fd;

        RecvState() : transferID(0), fileSize(0), receivedBytes(0), fd(-1) {}
        ~RecvState() { if (fd >= 0) close(fd); }
    };

    // 编码转换
    static std::string gbkToUtf8(const std::string& gbk)
    {
        if (gbk.empty()) return gbk;
        iconv_t cd = iconv_open("UTF-8", "GBK");
        if (cd == (iconv_t)-1) return gbk;

        size_t inLeft = gbk.size();
        size_t outLeft = inLeft * 4;
        std::string output(outLeft, '\0');

        char* inPtr = const_cast<char*>(gbk.data());
        char* outPtr = &output[0];

        size_t ret = iconv(cd, &inPtr, &inLeft, &outPtr, &outLeft);
        iconv_close(cd);

        if (ret == (size_t)-1) return gbk;
        output.resize(output.size() - outLeft);
        return output;
    }

    static std::string utf8ToGbk(const std::string& utf8)
    {
        if (utf8.empty()) return utf8;
        iconv_t cd = iconv_open("GBK", "UTF-8");
        if (cd == (iconv_t)-1) return utf8;

        size_t inLeft = utf8.size();
        size_t outLeft = inLeft * 2;
        std::string output(outLeft, '\0');

        char* inPtr = const_cast<char*>(utf8.data());
        char* outPtr = &output[0];

        size_t ret = iconv(cd, &inPtr, &inLeft, &outPtr, &outLeft);
        iconv_close(cd);

        if (ret == (size_t)-1) return utf8;
        output.resize(output.size() - outLeft);
        return output;
    }

    // 递归创建目录
    static bool MakeDirs(const std::string& path)
    {
        std::string dir;
        size_t lastSlash = path.rfind('/');
        if (lastSlash != std::string::npos && lastSlash > 0) {
            dir = path.substr(0, lastSlash);
        } else {
            return true;
        }

        std::string current;
        for (size_t i = 0; i < dir.size(); ++i) {
            current += dir[i];
            if (dir[i] == '/' && current.size() > 1) {
                mkdir(current.c_str(), 0755);
            }
        }
        if (!current.empty() && current != "/") {
            mkdir(current.c_str(), 0755);
        }
        return true;
    }

    // 获取 C2C 目标目录（简化版：使用当前工作目录）
    static std::string GetTargetDir(uint64_t transferID)
    {
        std::lock_guard<std::mutex> lock(s_targetDirMtx);
        auto it = s_targetDirs.find(transferID);
        if (it != s_targetDirs.end()) {
            return it->second;
        }
        return "";
    }

    // 设置 C2C 目标目录
    static void SetTargetDir(uint64_t transferID, const std::string& dir)
    {
        std::lock_guard<std::mutex> lock(s_targetDirMtx);
        s_targetDirs[transferID] = dir;
        Mprintf("[V2] SetTargetDir: transferID=%llu, dir=%s\n", transferID, dir.c_str());
    }

    // 处理 COMMAND_C2C_PREPARE - 在当前目录下创建子目录接收文件
    static void HandleC2CPrepare(const uint8_t* data, size_t len, IOCPClient* client)
    {
        if (len < sizeof(C2CPreparePacket)) return;

        const C2CPreparePacket* pkt = (const C2CPreparePacket*)data;

        // 获取当前工作目录
        char cwd[4096] = {};
        if (!getcwd(cwd, sizeof(cwd))) {
            strcpy(cwd, "/tmp");
        }

        // 创建接收子目录: recv_YYYYMM
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char subdir[32];
        snprintf(subdir, sizeof(subdir), "recv_%04d%02d",
                 t->tm_year + 1900, t->tm_mon + 1);

        std::string targetDir = std::string(cwd) + "/" + subdir;
        mkdir(targetDir.c_str(), 0755);  // 已存在则忽略

        SetTargetDir(pkt->transferID, targetDir);

        Mprintf("[V2] C2C Prepare: transferID=%llu, targetDir=%s\n",
                pkt->transferID, targetDir.c_str());
    }

    // 处理 V2 文件数据包
    // 返回: 0=成功, >0=错误码
    static int RecvFileChunkV2(const uint8_t* buf, size_t len, uint64_t myClientID)
    {
        // 检查是否是 FILE_COMPLETE_V2 包
        if (len >= 1 && buf[0] == COMMAND_FILE_COMPLETE_V2) {
            return HandleFileComplete(buf, len, myClientID);
        }

        if (len < sizeof(FileChunkPacketV2)) {
            Mprintf("[V2 Recv] Invalid packet length: %zu\n", len);
            return 2;
        }

        const FileChunkPacketV2* pkt = (const FileChunkPacketV2*)buf;

        if (len < sizeof(FileChunkPacketV2) + pkt->nameLength + pkt->dataLength) {
            Mprintf("[V2 Recv] Incomplete packet: %zu < %zu\n", len,
                    sizeof(FileChunkPacketV2) + pkt->nameLength + pkt->dataLength);
            return 3;
        }

        // 验证目标
        if (pkt->dstClientID != 0 && pkt->dstClientID != myClientID) {
            Mprintf("[V2 Recv] Target mismatch: dst=%llu, my=%llu\n",
                    pkt->dstClientID, myClientID);
            return 4;
        }

        // 提取文件名（服务端发送的是 GBK 编码）
        std::string fileNameGbk((const char*)(pkt + 1), pkt->nameLength);
        std::string fileName = gbkToUtf8(fileNameGbk);

        // 构建保存路径
        std::string savePath;
        std::string targetDir = GetTargetDir(pkt->transferID);

        if (!targetDir.empty()) {
            // C2C 传输：使用预设的目标目录 + 相对路径（保留目录结构）
            savePath = targetDir;
            if (!savePath.empty() && savePath.back() != '/') {
                savePath += '/';
            }
            // 转换 Windows 路径分隔符
            std::string relPath = fileName;
            for (char& c : relPath) {
                if (c == '\\') c = '/';
            }
            // 移除盘符前缀 (如 C:/)
            if (relPath.size() >= 3 && relPath[1] == ':' && relPath[2] == '/') {
                relPath = relPath.substr(3);
            } else if (relPath.size() >= 2 && relPath[1] == ':') {
                relPath = relPath.substr(2);
            }
            // 移除开头的 /
            while (!relPath.empty() && relPath[0] == '/') {
                relPath = relPath.substr(1);
            }
            savePath += relPath;
        } else {
            // 服务端传输：使用相对路径
            // 将 Windows 路径转换为 Linux 路径
            savePath = fileName;
            for (char& c : savePath) {
                if (c == '\\') c = '/';
            }
            // 移除盘符前缀 (如 C:/)
            if (savePath.size() >= 3 && savePath[1] == ':' && savePath[2] == '/') {
                savePath = savePath.substr(2);
            }
            // 确保以 / 开头
            if (savePath.empty() || savePath[0] != '/') {
                savePath = "/" + savePath;
            }
        }

        // 目录处理
        if (pkt->flags & FFV2_DIRECTORY) {
            MakeDirs(savePath + "/dummy");
            Mprintf("[V2 Recv] Created directory: %s\n", savePath.c_str());
            return 0;
        }

        // 创建父目录
        MakeDirs(savePath);

        // 获取或创建接收状态
        uint64_t stateKey = pkt->transferID ^ ((uint64_t)pkt->fileIndex << 48);
        RecvState* state = GetOrCreateState(stateKey, pkt, savePath);
        if (!state) {
            Mprintf("[V2 Recv] Failed to create state\n");
            return 5;
        }

        // 写入数据
        const char* data = (const char*)buf + sizeof(FileChunkPacketV2) + pkt->nameLength;

        if (lseek(state->fd, pkt->offset, SEEK_SET) == -1) {
            Mprintf("[V2 Recv] lseek failed: errno=%d\n", errno);
            return 6;
        }

        size_t written = 0;
        size_t toWrite = pkt->dataLength;
        while (written < toWrite) {
            ssize_t n = write(state->fd, data + written, toWrite - written);
            if (n <= 0) {
                if (errno == EINTR) continue;
                Mprintf("[V2 Recv] write failed: errno=%d\n", errno);
                return 7;
            }
            written += n;
        }

        state->receivedBytes += pkt->dataLength;

        // 打印进度（每 10% 或最后一块）
        bool isLast = (pkt->flags & FFV2_LAST_CHUNK) ||
                      (state->receivedBytes >= state->fileSize);
        int progress = state->fileSize > 0 ?
                       (int)(100 * state->receivedBytes / state->fileSize) : 100;
        static std::map<uint64_t, int> s_lastProgress;
        if (isLast || progress >= s_lastProgress[stateKey] + 10) {
            s_lastProgress[stateKey] = progress;
            Mprintf("[V2 Recv] %s: %llu/%llu (%d%%)\n",
                    savePath.c_str(), state->receivedBytes, state->fileSize, progress);
        }

        // 文件完成
        if (isLast) {
            close(state->fd);
            state->fd = -1;
            Mprintf("[V2 Recv] File complete: %s\n", savePath.c_str());

            // 清理状态
            std::lock_guard<std::mutex> lock(s_statesMtx);
            s_states.erase(stateKey);
            s_lastProgress.erase(stateKey);
        }

        return 0;
    }

    // 处理文件完成校验包
    static int HandleFileComplete(const uint8_t* buf, size_t len, uint64_t myClientID)
    {
        if (len < sizeof(FileCompletePacketV2)) {
            return 1;
        }

        const FileCompletePacketV2* pkt = (const FileCompletePacketV2*)buf;

        // 验证目标
        if (pkt->dstClientID != 0 && pkt->dstClientID != myClientID) {
            return 0;  // 不是给我的，忽略
        }

        Mprintf("[V2] File complete verify: transferID=%llu, fileIndex=%u, size=%llu\n",
                pkt->transferID, pkt->fileIndex, pkt->fileSize);

        // TODO: 实现 SHA-256 校验
        // 目前简单返回成功
        return 0;
    }

    // ============== V2 文件发送 (Linux -> Server) ==============

    // 生成传输 ID
    static uint64_t GenerateTransferID()
    {
        static std::mutex s_mtx;
        static uint64_t s_counter = 0;
        std::lock_guard<std::mutex> lock(s_mtx);
        uint64_t t = (uint64_t)time(nullptr) << 32;
        return t | (++s_counter);
    }

    // 获取公共根目录 (Linux 路径)
    static std::string GetCommonRoot(const std::vector<std::string>& files)
    {
        if (files.empty()) return "/";
        if (files.size() == 1) {
            size_t lastSlash = files[0].rfind('/');
            if (lastSlash != std::string::npos) {
                return files[0].substr(0, lastSlash + 1);
            }
            return "/";
        }

        std::string common = files[0];
        for (size_t i = 1; i < files.size(); ++i) {
            size_t j = 0;
            while (j < common.size() && j < files[i].size() && common[j] == files[i][j]) {
                ++j;
            }
            common = common.substr(0, j);
        }

        // 截断到最后一个 /
        size_t lastSlash = common.rfind('/');
        if (lastSlash != std::string::npos) {
            return common.substr(0, lastSlash + 1);
        }
        return "/";
    }

    // 递归收集目录中的文件和子目录
    static void CollectFiles(const std::string& dir, std::vector<std::string>& files)
    {
        // 先添加目录本身（去掉末尾的斜杠）
        std::string dirEntry = dir;
        while (!dirEntry.empty() && dirEntry.back() == '/') {
            dirEntry.pop_back();
        }
        if (!dirEntry.empty()) {
            files.push_back(dirEntry + "/");  // 以 / 结尾表示目录
        }

        DIR* d = opendir(dir.c_str());
        if (!d) return;

        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;

            std::string fullPath = dir;
            if (fullPath.back() != '/') fullPath += '/';
            fullPath += ent->d_name;

            struct stat st;
            if (lstat(fullPath.c_str(), &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                CollectFiles(fullPath, files);
            } else if (S_ISREG(st.st_mode)) {
                files.push_back(fullPath);
            }
        }
        closedir(d);
    }

    // Windows 路径转 Linux 路径
    static std::string WinPathToLinux(const std::string& winPath)
    {
        std::string p = winPath;
        // 1. 反斜杠 -> 正斜杠
        for (auto& c : p) {
            if (c == '\\') c = '/';
        }
        // 2. 移除 "X:/" 或 "X:" 前缀
        if (p.size() >= 3 && p[1] == ':' && p[2] == '/') {
            p = p.substr(3);
        } else if (p.size() >= 2 && p[1] == ':') {
            p = p.substr(2);
        }
        // 3. 确保以 / 开头
        if (p.empty() || p[0] != '/') {
            p = "/" + p;
        }
        return p;
    }

    // 处理 CMD_DOWN_FILES_V2 请求 - 上传文件到服务端
    // 格式: [cmd:1][targetDir\0][file1\0][file2\0]...[\0]
    static void HandleDownFilesV2(const uint8_t* data, size_t len, IOCPClient* client, uint64_t myClientID)
    {
        if (len < 2) return;

        // 解析目标目录 (GBK 编码)
        const char* p = (const char*)(data + 1);
        const char* end = (const char*)(data + len);
        std::string targetDirGbk = p;
        std::string targetDir = gbkToUtf8(targetDirGbk);
        p += targetDirGbk.length() + 1;

        // 解析文件列表
        std::vector<std::string> remotePaths;
        while (p < end && *p != '\0') {
            std::string pathGbk = p;
            remotePaths.push_back(gbkToUtf8(pathGbk));
            p += pathGbk.length() + 1;
        }

        if (remotePaths.empty()) {
            Mprintf("[V2 Send] No files to send\n");
            return;
        }

        // 收集所有文件 (展开目录)
        std::vector<std::string> allFiles;
        std::vector<std::string> rootCandidates;

        for (const auto& remotePath : remotePaths) {
            std::string localPath = WinPathToLinux(remotePath);

            struct stat st;
            if (stat(localPath.c_str(), &st) != 0) {
                Mprintf("[V2 Send] File not found: %s\n", localPath.c_str());
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                // 目录：递归收集
                std::string dirPath = localPath;
                if (dirPath.back() != '/') dirPath += '/';
                // 使用父目录参与公共根计算
                size_t pos = dirPath.rfind('/', dirPath.length() - 2);
                std::string parentPath = (pos != std::string::npos) ? dirPath.substr(0, pos + 1) : dirPath;
                rootCandidates.push_back(parentPath);
                CollectFiles(dirPath, allFiles);
            } else {
                // 文件
                rootCandidates.push_back(localPath);
                allFiles.push_back(localPath);
            }
        }

        if (allFiles.empty()) {
            Mprintf("[V2 Send] No files found\n");
            return;
        }

        // 计算公共根目录
        std::string commonRoot = GetCommonRoot(rootCandidates);

        Mprintf("[V2 Send] Starting V2 transfer: %zu files, root=%s, target=%s\n",
                allFiles.size(), commonRoot.c_str(), targetDir.c_str());

        // 启动传输线程
        std::thread([allFiles, targetDir, commonRoot, client, myClientID]() {
            SendFilesV2(allFiles, targetDir, commonRoot, client, myClientID);
        }).detach();
    }

    // V2 文件发送核心函数
    static void SendFilesV2(const std::vector<std::string>& files,
                            const std::string& targetDir,
                            const std::string& commonRoot,
                            IOCPClient* client,
                            uint64_t myClientID)
    {
        const size_t CHUNK_SIZE = 60 * 1024;  // 60KB per chunk
        uint64_t transferID = GenerateTransferID();
        uint32_t totalFiles = (uint32_t)files.size();

        Mprintf("[V2 Send] TransferID=%llu, files=%u\n", transferID, totalFiles);

        for (uint32_t fileIndex = 0; fileIndex < totalFiles; ++fileIndex) {
            const std::string& filePath = files[fileIndex];

            // 检查是否是目录项（以 / 结尾）
            bool isDirectory = !filePath.empty() && filePath.back() == '/';
            std::string actualPath = isDirectory ? filePath.substr(0, filePath.length() - 1) : filePath;

            // 计算相对路径
            std::string relativePath;
            if (actualPath.find(commonRoot) == 0) {
                relativePath = actualPath.substr(commonRoot.length());
            } else {
                size_t lastSlash = actualPath.rfind('/');
                relativePath = (lastSlash != std::string::npos) ?
                               actualPath.substr(lastSlash + 1) : actualPath;
            }

            // 构建目标文件名 (服务端路径格式: targetDir + relativePath)
            std::string targetName = targetDir;
            if (!targetName.empty() && targetName.back() != '\\') {
                targetName += '\\';
            }
            // 转换为 Windows 风格路径
            for (char& c : relativePath) {
                if (c == '/') c = '\\';
            }
            targetName += relativePath;

            // 转为 GBK 编码
            std::string targetNameGbk = utf8ToGbk(targetName);

            // 目录项：发送单个包，不包含数据
            if (isDirectory) {
                std::vector<uint8_t> buffer(sizeof(FileChunkPacketV2) + targetNameGbk.length());
                FileChunkPacketV2* pkt = (FileChunkPacketV2*)buffer.data();

                memset(pkt, 0, sizeof(FileChunkPacketV2));
                pkt->cmd = COMMAND_SEND_FILE_V2;
                pkt->transferID = transferID;
                pkt->srcClientID = myClientID;
                pkt->dstClientID = 0;  // 发送给服务端
                pkt->fileIndex = fileIndex;
                pkt->totalFiles = totalFiles;
                pkt->fileSize = 0;
                pkt->offset = 0;
                pkt->dataLength = 0;
                pkt->nameLength = targetNameGbk.length();
                pkt->flags = FFV2_DIRECTORY | FFV2_LAST_CHUNK;

                // 写入目录名
                memcpy(buffer.data() + sizeof(FileChunkPacketV2),
                       targetNameGbk.c_str(), targetNameGbk.length());

                Mprintf("[V2 Send] [%u/%u] DIR: %s -> %s\n",
                        fileIndex + 1, totalFiles, actualPath.c_str(), targetName.c_str());

                if (!client->Send2Server((char*)buffer.data(), (ULONG)buffer.size())) {
                    Mprintf("[V2 Send] Send directory failed\n");
                }

                usleep(1000);  // 1ms 间隔
                continue;
            }

            // 获取文件信息
            struct stat st;
            if (stat(actualPath.c_str(), &st) != 0) {
                Mprintf("[V2 Send] stat failed: %s\n", actualPath.c_str());
                continue;
            }
            uint64_t fileSize = (uint64_t)st.st_size;

            // 打开文件
            int fd = open(actualPath.c_str(), O_RDONLY);
            if (fd < 0) {
                Mprintf("[V2 Send] open failed: %s, errno=%d\n", actualPath.c_str(), errno);
                continue;
            }

            Mprintf("[V2 Send] [%u/%u] %s -> %s (%llu bytes)\n",
                    fileIndex + 1, totalFiles, actualPath.c_str(), targetName.c_str(), fileSize);

            // 分块发送
            uint64_t offset = 0;
            std::vector<uint8_t> buffer;
            buffer.reserve(sizeof(FileChunkPacketV2) + targetNameGbk.length() + CHUNK_SIZE);

            while (offset < fileSize || (offset == 0 && fileSize == 0)) {
                size_t toRead = std::min(CHUNK_SIZE, (size_t)(fileSize - offset));
                bool isLast = (offset + toRead >= fileSize);

                // 构建包头
                buffer.resize(sizeof(FileChunkPacketV2) + targetNameGbk.length() + toRead);
                FileChunkPacketV2* pkt = (FileChunkPacketV2*)buffer.data();

                memset(pkt, 0, sizeof(FileChunkPacketV2));
                pkt->cmd = COMMAND_SEND_FILE_V2;
                pkt->transferID = transferID;
                pkt->srcClientID = myClientID;
                pkt->dstClientID = 0;  // 发送给服务端
                pkt->fileIndex = fileIndex;
                pkt->totalFiles = totalFiles;
                pkt->fileSize = fileSize;
                pkt->offset = offset;
                pkt->dataLength = toRead;
                pkt->nameLength = targetNameGbk.length();
                pkt->flags = isLast ? FFV2_LAST_CHUNK : FFV2_NONE;

                // 写入文件名
                memcpy(buffer.data() + sizeof(FileChunkPacketV2),
                       targetNameGbk.c_str(), targetNameGbk.length());

                // 读取文件数据
                if (toRead > 0) {
                    uint8_t* dataPtr = buffer.data() + sizeof(FileChunkPacketV2) + targetNameGbk.length();
                    ssize_t n = read(fd, dataPtr, toRead);
                    if (n < 0) {
                        Mprintf("[V2 Send] read failed: errno=%d\n", errno);
                        break;
                    }
                    if ((size_t)n < toRead) {
                        // 文件读取不完整，调整包大小
                        toRead = n;
                        pkt->dataLength = toRead;
                        buffer.resize(sizeof(FileChunkPacketV2) + targetNameGbk.length() + toRead);
                        isLast = true;
                        pkt->flags |= FFV2_LAST_CHUNK;
                    }
                }

                // 发送
                if (!client->Send2Server((char*)buffer.data(), (ULONG)buffer.size())) {
                    Mprintf("[V2 Send] Send failed\n");
                    break;
                }

                offset += toRead;

                // 空文件处理
                if (fileSize == 0) break;

                // 发送间隔 (避免发送过快)
                usleep(10000);  // 10ms
            }

            close(fd);
        }

        Mprintf("[V2 Send] Transfer complete: transferID=%llu\n", transferID);
    }

private:
    static RecvState* GetOrCreateState(uint64_t key, const FileChunkPacketV2* pkt,
                                       const std::string& savePath)
    {
        std::lock_guard<std::mutex> lock(s_statesMtx);

        auto it = s_states.find(key);
        if (it != s_states.end()) {
            return it->second.get();
        }

        // 创建新状态
        std::unique_ptr<RecvState> state(new RecvState());
        state->transferID = pkt->transferID;
        state->fileSize = pkt->fileSize;
        state->filePath = savePath;
        state->receivedBytes = 0;

        // 打开文件
        state->fd = open(savePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (state->fd < 0) {
            Mprintf("[V2 Recv] open failed: %s, errno=%d\n", savePath.c_str(), errno);
            return nullptr;
        }

        RecvState* ptr = state.get();
        s_states[key] = std::move(state);
        return ptr;
    }

    // 接收状态存储
    static inline std::map<uint64_t, std::unique_ptr<RecvState>> s_states;
    static inline std::mutex s_statesMtx;

    // C2C 目标目录存储
    static inline std::map<uint64_t, std::string> s_targetDirs;
    static inline std::mutex s_targetDirMtx;
};
