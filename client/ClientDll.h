#pragma once

#include "Common.h"
#include "IOCPClient.h"
#include <IOSTREAM>
#include "LoginServer.h"
#include "KernelManager.h"
#include <iosfwd>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <shellapi.h>
#include <corecrt_io.h>
#include "domain_pool.h"
#include "ClientApp.h"

BOOL IsProcessExit();

typedef BOOL(*IsRunning)(void* thisApp);

BOOL IsSharedRunning(void* thisApp);

BOOL IsClientAppRunning(void* thisApp);

DWORD WINAPI StartClientApp(LPVOID param);

// 客户端类：将全局变量打包到一起.
class ClientApp : public App
{
public:
    State			g_bExit;			// 应用程序状态（1-被控端退出 2-主控端退出 3-其他条件）
    BOOL			g_bThreadExit;		// 工作线程状态
    HINSTANCE		g_hInstance;		// 进程句柄
    CONNECT_ADDRESS* g_Connection;		// 连接信息
    HANDLE			g_hEvent;			// 全局事件
    BOOL			m_bShared;			// 是否分享
    IsRunning		m_bIsRunning;		// 运行状态
    unsigned		m_ID;				// 唯一标识
    static int		m_nCount;			// 计数器
    static CLock	m_Locker;
    ClientApp(CONNECT_ADDRESS*conn, IsRunning run, BOOL shared=FALSE)
    {
        g_bExit = S_CLIENT_NORMAL;
        g_bThreadExit = FALSE;
        g_hInstance = NULL;
        g_Connection = new CONNECT_ADDRESS(*conn);
        g_hEvent = NULL;
        m_bIsRunning = run;
        m_bShared = shared;
        m_ID = 0;
        g_bThreadExit = TRUE;
    }
    std::vector<std::string> GetSharedMasterList()
    {
        DomainPool pool = g_Connection->ServerIP();
        auto list = pool.GetIPList();
        return list;
    }
    ~ClientApp()
    {
        SAFE_DELETE(g_Connection);
    }
    ClientApp* SetID(unsigned id)
    {
        m_ID = id;
        return this;
    }
    static void AddCount(int n=1)
    {
        m_Locker.Lock();
        m_nCount+=n;
        m_Locker.Unlock();
    }
    static int GetCount()
    {
        m_Locker.Lock();
        int n = m_nCount;
        m_Locker.Unlock();
        return n;
    }
    static void Wait()
    {
        while (GetCount())
            Sleep(50);
    }
    bool IsThreadRun()
    {
        m_Locker.Lock();
        BOOL n = g_bThreadExit;
        m_Locker.Unlock();
        return FALSE == n;
    }
    void SetThreadRun(BOOL run = TRUE)
    {
        m_Locker.Lock();
        g_bThreadExit = !run;
        m_Locker.Unlock();
    }
    void SetProcessState(State state = S_CLIENT_NORMAL)
    {
        m_Locker.Lock();
        g_bExit = state;
        m_Locker.Unlock();
    }
    virtual bool Initialize() override
    {
        g_Connection->SetType(CLIENT_TYPE_ONE);
        return true;
    }
    virtual bool Start() override
    {
        StartClientApp(this);
        return true;
    }
    virtual bool Stop() override
    {
        g_bExit = S_CLIENT_EXIT;
        return true;
    }
    bool Run()
    {
        if (!Initialize()) return false;
        if (!Start()) return false;
        if (!Stop()) return false;
        return true;
    }
};

ClientApp* NewClientStartArg(const char* remoteAddr, IsRunning run = IsClientAppRunning, BOOL shared=FALSE);

// 启动核心线程，参数为：ClientApp
DWORD WINAPI StartClient(LPVOID lParam);

// 启动核心线程，参数为：ClientApp
DWORD WINAPI StartClientApp(LPVOID param);
