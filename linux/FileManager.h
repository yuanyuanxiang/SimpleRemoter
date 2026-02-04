#pragma once
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <iconv.h>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

// ============== Linux 文件管理（参考 Windows 端 client/FileManager.cpp）==============
// 最小实现：仅支持浏览文件（驱动器列表 + 目录文件列表）

class FileManager : public IOCPManager
{
public:
    FileManager(IOCPClient* client)
        : m_client(client)
    {
        // 与 Windows 端一致：构造时立即发送驱动器列表
        SendDriveList();
    }

    ~FileManager() {}

    virtual VOID OnReceive(PBYTE szBuffer, ULONG ulLength)
    {
        if (!szBuffer || ulLength == 0) return;

        switch (szBuffer[0]) {
        case COMMAND_LIST_FILES:
            SendFilesList((char*)szBuffer + 1);
            break;
        case COMMAND_DELETE_FILE:
        case COMMAND_DELETE_DIRECTORY:
            // TODO: unlink / recursive rmdir
            SendToken(TOKEN_DELETE_FINISH);
            break;
        case COMMAND_DOWN_FILES:
        case COMMAND_CONTINUE:
        case COMMAND_STOP:
            // TODO: 文件上传（客户端→服务端）
            SendToken(TOKEN_TRANSFER_FINISH);
            break;
        case COMMAND_CREATE_FOLDER:
            // TODO: mkdir -p
            SendToken(TOKEN_CREATEFOLDER_FINISH);
            break;
        case COMMAND_RENAME_FILE:
            // TODO: rename()
            SendToken(TOKEN_RENAME_FINISH);
            break;
        case COMMAND_SET_TRANSFER_MODE:
            // 无需回复，仅存储模式
            break;
        case COMMAND_FILE_SIZE:
            // 服务端想上传文件到本地 → 回复跳过
            RejectIncomingFile();
            break;
        case COMMAND_FILE_DATA:
            // 已拒绝，正常不会收到；若收到则忽略
            break;
        default:
            Mprintf("[FileManager] Unhandled command: %d\n", (int)szBuffer[0]);
            break;
        }
    }

private:
    IOCPClient* m_client;

    // ---- 发送单字节 Token 回复 ----
    void SendToken(BYTE token)
    {
        if (!m_client) return;
        m_client->Send2Server((char*)&token, 1);
    }

    // ---- 拒绝服务端上传文件：回复 TOKEN_DATA_CONTINUE，偏移 low = -1（跳过）----
    void RejectIncomingFile()
    {
        if (!m_client) return;
        BYTE buf[9] = {};
        buf[0] = (BYTE)TOKEN_DATA_CONTINUE;
        DWORD skip = (DWORD)-1;
        memcpy(buf + 5, &skip, sizeof(DWORD));  // dwSizeLow = 0xFFFFFFFF
        m_client->Send2Server((char*)buf, sizeof(buf));
    }

    // ---- iconv 编码转换 ----
    static std::string iconvConvert(const std::string& input, const char* from, const char* to)
    {
        if (input.empty()) return input;
        iconv_t cd = iconv_open(to, from);
        if (cd == (iconv_t)-1) return input;

        size_t inLeft = input.size();
        size_t outLeft = inLeft * 4;  // 最大膨胀 4 倍
        std::string output(outLeft, '\0');

        char* inPtr  = const_cast<char*>(input.data());
        char* outPtr = &output[0];

        size_t ret = iconv(cd, &inPtr, &inLeft, &outPtr, &outLeft);
        iconv_close(cd);

        if (ret == (size_t)-1) return input;  // 转换失败，原样返回
        output.resize(output.size() - outLeft);
        return output;
    }

    // 服务端发来的 GBK 路径 → UTF-8（用于 opendir 等系统调用）
    static std::string gbkToUtf8(const std::string& gbk)
    {
        return iconvConvert(gbk, "GBK", "UTF-8");
    }

    // 本地 UTF-8 文件名 → GBK（发送给服务端显示）
    static std::string utf8ToGbk(const std::string& utf8)
    {
        return iconvConvert(utf8, "UTF-8", "GBK");
    }

    // ---- Windows 路径 → Linux 路径 ----
    // 服务端发送 "/:\home\user" 格式，需转换为 "/home/user"
    static std::string winPathToLinux(const char* winPath)
    {
        std::string p(winPath);
        // 1. 反斜杠 → 正斜杠
        for (auto& c : p) {
            if (c == '\\') c = '/';
        }
        // 2. 去掉 "X:/" 前缀（X 为驱动器字母，Linux 下为 '/'）
        if (p.size() >= 3 && p[1] == ':' && p[2] == '/') {
            p = p.substr(3);
        } else if (p.size() >= 2 && p[1] == ':') {
            p = p.substr(2);
        }
        // 3. 确保以 / 开头
        if (p.empty() || p[0] != '/') {
            p = "/" + p;
        }
        // 4. 合并连续斜杠
        std::string result;
        result.reserve(p.size());
        for (size_t i = 0; i < p.size(); i++) {
            if (i > 0 && p[i] == '/' && p[i - 1] == '/')
                continue;
            result += p[i];
        }
        return result.empty() ? "/" : result;
    }

    // ---- Unix time_t → Windows FILETIME（100ns since 1601-01-01）----
    static uint64_t unixToFiletime(time_t t)
    {
        return (uint64_t)t * 10000000ULL + 116444736000000000ULL;
    }

    // ---- 读取根分区文件系统类型（从 /proc/mounts）----
    static std::string getRootFsType()
    {
        std::ifstream f("/proc/mounts");
        std::string line;
        while (std::getline(f, line)) {
            std::istringstream iss(line);
            std::string dev, mp, fs;
            if (iss >> dev >> mp >> fs) {
                if (mp == "/") return fs;
            }
        }
        return "ext4";
    }

    // ---- 发送驱动器列表（Linux：只发送根分区 /）----
    void SendDriveList()
    {
        if (!m_client) return;

        BYTE buf[256];
        buf[0] = (BYTE)TOKEN_DRIVE_LIST;
        DWORD offset = 1;

        // 驱动器字母: '/'
        buf[offset] = '/';
        // 驱动器类型: DRIVE_FIXED = 3
        buf[offset + 1] = 3;

        // 磁盘大小（MB）
        unsigned long totalMB = 0, freeMB = 0;
        struct statvfs sv;
        if (statvfs("/", &sv) == 0) {
            unsigned long long totalBytes = (unsigned long long)sv.f_blocks * sv.f_frsize;
            unsigned long long freeBytes  = (unsigned long long)sv.f_bfree  * sv.f_frsize;
            totalMB = (unsigned long)(totalBytes / 1024 / 1024);
            freeMB  = (unsigned long)(freeBytes  / 1024 / 1024);
        }
        memcpy(buf + offset + 2, &totalMB, sizeof(unsigned long));
        memcpy(buf + offset + 6, &freeMB,  sizeof(unsigned long));

        // 卷标名（类型描述）
        const char* typeName = "Linux";
        int typeNameLen = strlen(typeName) + 1;
        memcpy(buf + offset + 10, typeName, typeNameLen);

        // 文件系统名
        std::string fsType = getRootFsType();
        int fsNameLen = fsType.size() + 1;
        memcpy(buf + offset + 10 + typeNameLen, fsType.c_str(), fsNameLen);

        offset += 10 + typeNameLen + fsNameLen;

        m_client->Send2Server((char*)buf, offset);
        Mprintf("[FileManager] SendDriveList: %u bytes (total=%luMB free=%luMB fs=%s)\n",
                (unsigned)offset, totalMB, freeMB, fsType.c_str());
    }

    // ---- 列出目录文件，构造 TOKEN_FILE_LIST 二进制数据 ----
    void SendFilesList(const char* path)
    {
        if (!m_client) return;

        std::string linuxPath = winPathToLinux(gbkToUtf8(path).c_str());
        Mprintf("[FileManager] SendFilesList: '%s' -> '%s'\n", path, linuxPath.c_str());

        std::vector<uint8_t> buf;
        buf.reserve(64 * 1024);
        buf.push_back((uint8_t)TOKEN_FILE_LIST);

        DIR* dir = opendir(linuxPath.c_str());
        if (!dir) {
            Mprintf("[FileManager] opendir failed: %s (errno=%d)\n", linuxPath.c_str(), errno);
            m_client->Send2Server((char*)buf.data(), (ULONG)buf.size());
            return;
        }

        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;

            // 完整路径用于 stat()
            std::string fullPath = linuxPath;
            if (fullPath.back() != '/') fullPath += '/';
            fullPath += ent->d_name;

            struct stat st;
            if (lstat(fullPath.c_str(), &st) != 0)
                continue;

            // 文件属性：0x10 = 目录，0x00 = 文件（对应 FILE_ATTRIBUTE_DIRECTORY）
            uint8_t isDir = S_ISDIR(st.st_mode) ? 0x10 : 0x00;
            buf.push_back(isDir);

            // 文件名（UTF-8 → GBK，null 结尾）
            std::string gbkName = utf8ToGbk(ent->d_name);
            buf.insert(buf.end(), gbkName.begin(), gbkName.end());
            buf.push_back(0);

            // 文件大小：高 4 字节 + 低 4 字节
            uint64_t fileSize = (uint64_t)st.st_size;
            DWORD sizeHigh = (DWORD)(fileSize >> 32);
            DWORD sizeLow  = (DWORD)(fileSize & 0xFFFFFFFF);
            const uint8_t* pH = (const uint8_t*)&sizeHigh;
            const uint8_t* pL = (const uint8_t*)&sizeLow;
            buf.insert(buf.end(), pH, pH + sizeof(DWORD));
            buf.insert(buf.end(), pL, pL + sizeof(DWORD));

            // 最后修改时间：Windows FILETIME 格式（8 字节）
            uint64_t ft = unixToFiletime(st.st_mtime);
            const uint8_t* pFT = (const uint8_t*)&ft;
            buf.insert(buf.end(), pFT, pFT + 8);
        }
        closedir(dir);

        m_client->Send2Server((char*)buf.data(), (ULONG)buf.size());
        Mprintf("[FileManager] SendFilesList: %u bytes\n", (unsigned)buf.size());
    }
};
