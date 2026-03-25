#pragma once
// LANChecker.h - 检测本进程的TCP连接是否有外网IP
// 用于试用版License限制：仅允许内网连接

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <vector>
#include <string>
#include <mutex>
#include <set>
#include <atomic>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

class LANChecker
{
public:
    struct WanConnection
    {
        std::string remoteIP;
        uint16_t remotePort;
        uint16_t localPort;
    };

    // 检查IP是否为内网地址 (网络字节序)
    static bool IsPrivateIP(uint32_t networkOrderIP)
    {
        uint32_t ip = ntohl(networkOrderIP);

        // 10.0.0.0/8
        if ((ip & 0xFF000000) == 0x0A000000) return true;
        // 172.16.0.0/12
        if ((ip & 0xFFF00000) == 0xAC100000) return true;
        // 192.168.0.0/16
        if ((ip & 0xFFFF0000) == 0xC0A80000) return true;
        // 127.0.0.0/8 (loopback)
        if ((ip & 0xFF000000) == 0x7F000000) return true;
        // 169.254.0.0/16 (link-local)
        if ((ip & 0xFFFF0000) == 0xA9FE0000) return true;
        // 0.0.0.0
        if (ip == 0) return true;

        return false;
    }

    // 获取本进程所有入站的外网TCP连接（只检测别人连进来的，不检测本进程连出去的）
    static std::vector<WanConnection> GetWanConnections()
    {
        std::vector<WanConnection> result;
        DWORD pid = GetCurrentProcessId();

        // 先获取本进程监听的端口列表
        std::set<uint16_t> listeningPorts;
        {
            DWORD size = 0;
            GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
            if (size > 0)
            {
                std::vector<BYTE> buffer(size);
                auto table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());
                if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) == NO_ERROR)
                {
                    for (DWORD i = 0; i < table->dwNumEntries; i++)
                    {
                        if (table->table[i].dwOwningPid == pid)
                            listeningPorts.insert(ntohs((uint16_t)table->table[i].dwLocalPort));
                    }
                }
            }
        }

        if (listeningPorts.empty())
            return result;  // 没有监听端口，不可能有入站连接

        // 获取已建立的连接，只保留入站连接（本地端口是监听端口的）
        DWORD size = 0;
        GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0);
        if (size == 0) return result;

        std::vector<BYTE> buffer(size);
        auto table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());

        if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0) != NO_ERROR)
            return result;

        for (DWORD i = 0; i < table->dwNumEntries; i++)
        {
            auto& row = table->table[i];

            // 只检查本进程、已建立的连接
            if (row.dwOwningPid != pid || row.dwState != MIB_TCP_STATE_ESTAB)
                continue;

            uint16_t localPort = ntohs((uint16_t)row.dwLocalPort);

            // 只检查入站连接（本地端口是监听端口）
            if (listeningPorts.find(localPort) == listeningPorts.end())
                continue;

            // 检查远端IP是否为外网
            if (!IsPrivateIP(row.dwRemoteAddr))
            {
                WanConnection conn;
                char ipStr[INET_ADDRSTRLEN];
                in_addr addr;
                addr.s_addr = row.dwRemoteAddr;
                inet_ntop(AF_INET, &addr, ipStr, sizeof(ipStr));

                conn.remoteIP = ipStr;
                conn.remotePort = ntohs((uint16_t)row.dwRemotePort);
                conn.localPort = localPort;
                result.push_back(conn);
            }
        }

        return result;
    }

    // 检测是否有外网连接，首次检测到时弹框警告（异步，不阻塞）
    // 返回: true=纯内网, false=检测到外网连接
    static bool CheckAndWarn()
    {
        auto wanConns = GetWanConnections();
        if (wanConns.empty())
            return true;  // 没有外网连接

        // 检查是否已经警告过这些IP
        bool hasNewWanIP = false;
        {
            std::lock_guard<std::mutex> lock(GetMutex());
            for (const auto& conn : wanConns)
            {
                if (GetWarnedIPs().find(conn.remoteIP) == GetWarnedIPs().end())
                {
                    GetWarnedIPs().insert(conn.remoteIP);
                    hasNewWanIP = true;
                }
            }
        }

        if (!hasNewWanIP)
            return false;  // 已警告过，不重复弹框

        // 在新线程中弹框，避免阻塞网络线程
        std::string* msgPtr = new std::string();
        *msgPtr = "Detected WAN connection(s):\n\n";
        for (const auto& conn : wanConns)
        {
            *msgPtr += "  " + conn.remoteIP + ":" + std::to_string(conn.remotePort) + "\n";
        }
        *msgPtr += "\nTrial version is restricted to LAN only.\n";
        *msgPtr += "Please purchase a license for remote connections.";

        HANDLE hThread = CreateThread(NULL, 0, WarningDialogThread, msgPtr, 0, NULL);
        if (hThread) CloseHandle(hThread);

        return false;
    }

    // 仅检测，不弹框
    static bool HasWanConnection()
    {
        return !GetWanConnections().empty();
    }

    // 重置警告状态（允许再次弹框）
    static void Reset()
    {
        std::lock_guard<std::mutex> lock(GetMutex());
        GetWarnedIPs().clear();
        GetWarnedPortCount() = false;
    }

    // 获取本进程监听的TCP端口列表
    static std::vector<uint16_t> GetListeningPorts()
    {
        std::vector<uint16_t> result;
        DWORD pid = GetCurrentProcessId();

        DWORD size = 0;
        GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
        if (size == 0) return result;

        std::vector<BYTE> buffer(size);
        auto table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());

        if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) != NO_ERROR)
            return result;

        for (DWORD i = 0; i < table->dwNumEntries; i++)
        {
            auto& row = table->table[i];
            if (row.dwOwningPid == pid && row.dwState == MIB_TCP_STATE_LISTEN)
            {
                result.push_back(ntohs((uint16_t)row.dwLocalPort));
            }
        }

        return result;
    }

    // 获取本进程监听的端口数量
    static int GetListeningPortCount()
    {
        return (int)GetListeningPorts().size();
    }

    // 检查端口数量限制（试用版限制）
    // 返回: true=符合限制, false=超过限制
    static bool CheckPortLimit(int maxPorts = 1)
    {
        auto ports = GetListeningPorts();
        if ((int)ports.size() <= maxPorts)
            return true;

        // 检查是否已警告过
        {
            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetWarnedPortCount())
                return false;
            GetWarnedPortCount() = true;
        }

        // 构建警告信息
        std::string* msgPtr = new std::string();
        *msgPtr = "Trial version is limited to " + std::to_string(maxPorts) + " listening port(s).\n\n";
        *msgPtr += "Current listening ports (" + std::to_string(ports.size()) + "):\n";
        for (auto port : ports)
        {
            *msgPtr += "  Port " + std::to_string(port) + "\n";
        }
        *msgPtr += "\nPlease purchase a license for multiple ports.";

        HANDLE hThread = CreateThread(NULL, 0, WarningDialogThread, msgPtr, 0, NULL);
        if (hThread) CloseHandle(hThread);

        return false;
    }

private:
    static DWORD WINAPI WarningDialogThread(LPVOID lpParam)
    {
        std::string* msg = (std::string*)lpParam;
        MessageBoxA(NULL, msg->c_str(), "Trial Version - LAN Only",
                   MB_OK | MB_ICONWARNING | MB_TOPMOST);
        delete msg;
        return 0;
    }

    static std::mutex& GetMutex()
    {
        static std::mutex s_mutex;
        return s_mutex;
    }

    static std::set<std::string>& GetWarnedIPs()
    {
        static std::set<std::string> s_warnedIPs;
        return s_warnedIPs;
    }

    static bool& GetWarnedPortCount()
    {
        static bool s_warnedPortCount = false;
        return s_warnedPortCount;
    }
};

// 授权连接超时检测器
// 用于检测试用版/未授权用户是否长时间无法连接授权服务器
class AuthTimeoutChecker
{
public:
    // 默认超时时间（秒）
#ifdef _DEBUG
    static const int DEFAULT_WARNING_TIMEOUT = 30;   // 30秒弹警告
#else
    static const int DEFAULT_WARNING_TIMEOUT = 300;   // 5分钟弹警告
#endif

    // 重置计时器（连接成功或收到心跳响应时调用）
    static void ResetTimer()
    {
        GetLastAuthTime() = GetTickCount64();
        // 关闭弹窗标记，允许下次超时再弹
        GetDialogShowing() = false;
    }

    // 检查是否超时（在心跳循环中调用）
    // 超时后弹窗提醒，弹窗关闭后如果仍超时则再次弹窗
    static bool Check(int warningTimeoutSec = DEFAULT_WARNING_TIMEOUT)
    {
        ULONGLONG now = GetTickCount64();
        ULONGLONG lastAuth = GetLastAuthTime();

        // 首次调用，初始化时间
        if (lastAuth == 0)
        {
            GetLastAuthTime() = now;
            return true;
        }

        ULONGLONG elapsed = (now - lastAuth) / 1000;  // 转换为秒

        // 超过警告时间，弹出警告（弹窗关闭后可再次弹出）
        if (elapsed >= (ULONGLONG)warningTimeoutSec && !GetDialogShowing())
        {
            GetDialogShowing() = true;

            // 在新线程中弹窗，弹窗关闭后重置标记允许再次弹窗
            HANDLE hThread = CreateThread(NULL, 0, WarningThread, (LPVOID)elapsed, 0, NULL);
            if (hThread) CloseHandle(hThread);
        }

        return true;
    }

    // 标记已授权（已授权用户不需要超时检测）
    static void SetAuthorized()
    {
        GetAuthorized() = true;
    }

    // 检查是否需要进行超时检测
    static bool NeedCheck()
    {
        return !GetAuthorized();
    }

private:
    static DWORD WINAPI WarningThread(LPVOID lpParam)
    {
        ULONGLONG elapsed = (ULONGLONG)lpParam;
        std::string msg = "Warning: Unable to connect to authorization server.\n\n";
        msg += "Elapsed time: " + std::to_string(elapsed) + " seconds\n\n";
        msg += "Please check your network connection.";

        MessageBoxA(NULL, msg.c_str(), "Authorization Warning",
                   MB_OK | MB_ICONWARNING | MB_TOPMOST);

        // 弹窗关闭后，重置标记，允许再次弹窗
        GetDialogShowing() = false;
        return 0;
    }

    static ULONGLONG& GetLastAuthTime()
    {
        static ULONGLONG s_lastAuthTime = 0;
        return s_lastAuthTime;
    }

    static std::atomic<bool>& GetDialogShowing()
    {
        static std::atomic<bool> s_dialogShowing(false);
        return s_dialogShowing;
    }

    static std::atomic<bool>& GetAuthorized()
    {
        static std::atomic<bool> s_authorized(false);
        return s_authorized;
    }
};
