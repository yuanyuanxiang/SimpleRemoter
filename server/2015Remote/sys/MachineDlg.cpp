// ServiceDlg.cpp : implementation file
//

#include "stdafx.h"
#include "2015Remote.h"
#include "MachineDlg.h"
#include "ServiceInfoDlg.h"
#include"CCreateTaskDlg.h"
#include "CInjectCodeDlg.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_SHOW_MSG (WM_USER+103)
#define WM_WAIT_MSG (WM_USER+104)

/////////////////////////////////////////////////////////////////////////////
// CMachineDlg dialog
static UINT indicators[] = {
    ID_SEPARATOR,           // status line indicator
    ID_SEPARATOR,           // status line indicator
    ID_SEPARATOR,           // status line indicator
};


CMachineDlg::CMachineDlg(CWnd* pParent, Server* pIOCPServer, ClientContext* pContext)
    : DialogBase(CMachineDlg::IDD, pParent, pIOCPServer, pContext, IDI_MACHINE)
{
    m_pMainWnd = (CMy2015RemoteDlg*)pParent;

    m_nSortedCol = 1;
    m_bAscending = true;
    m_bIsReceiving = false;
    m_IPConverter = new IPConverter;
}

CMachineDlg::~CMachineDlg()
{
    m_bIsClosed = TRUE;
    SAFE_DELETE(m_IPConverter);
    DeleteList();
}

// 如果用`SortItemsEx`函数对列表排序则不需要定义这个结构体,
// 传递给排序函数的值就是行号.
class ListItem
{
public:
    CString* data;
    int len;
    int pid;
    ListItem(const CListCtrl& list, int idx, int process = 0)
    {
        len = list.GetHeaderCtrl()->GetItemCount();
        data = new CString[len];
        pid = process;
        for (int i=0; i < len; ++i) {
            data[i] = list.GetItemText(idx, i);
        }
    }
    void Destroy()
    {
        delete [] data;
        delete this;
    }
protected:
    ~ListItem() {}
};

int CALLBACK CMachineDlg::CompareFunction(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    auto* pSortInfo = reinterpret_cast<std::pair<int, bool>*>(lParamSort);
    int nColumn = pSortInfo->first;
    bool bAscending = pSortInfo->second;
    // 排序
    ListItem* it1 = (ListItem*)lParam1, * it2 = (ListItem*)lParam2;
    if (it1 == NULL || it2 == NULL) return 0;
    int n = it1->data[nColumn].Compare(it2->data[nColumn]);
    return bAscending ? n : -n;
}

void CMachineDlg::SortColumn(int iCol, bool bAsc)
{
    m_bAscending = bAsc;
    m_nSortedCol = iCol;
    std::pair<int, bool> sortInfo(m_nSortedCol, m_bAscending);
    m_list.SortItems(CompareFunction, reinterpret_cast<LPARAM>(&sortInfo));
}

BOOL CMachineDlg::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
    HD_NOTIFY* pHDNotify = (HD_NOTIFY*)lParam;

    if (pHDNotify->hdr.code == HDN_ITEMCLICKA || pHDNotify->hdr.code == HDN_ITEMCLICKW) {
        SortColumn(pHDNotify->iItem, pHDNotify->iItem == m_nSortedCol ? !m_bAscending : true);
    }

    return CDialog::OnNotify(wParam, lParam, pResult);
}


void CMachineDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_LIST, m_list);
    DDX_Control(pDX, IDC_TAB, m_tab);
}


BEGIN_MESSAGE_MAP(CMachineDlg, CDialog)
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_NOTIFY(NM_DBLCLK, IDC_LIST, OnDblclkList)
    ON_NOTIFY(NM_RCLICK, IDC_LIST, OnRclickList)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB, OnSelChangeTab)
    ON_NOTIFY(TCN_SELCHANGING, IDC_TAB, OnSelChangingTab)
    ON_MESSAGE(WM_SHOW_MSG, OnShowMessage)
    ON_MESSAGE(WM_WAIT_MSG, OnWaitMessage)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMachineDlg message handlers


BOOL CMachineDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon

    // TODO: Add extra initialization here
    CString str;
    str.Format(_T("主机管理 - %s"), m_ContextObject->PeerName.c_str());
    SetWindowText(str);

    m_tab.SetPadding(CSize(6, 3));
    m_tab.ModifyStyle(0, TCS_MULTILINE);
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_UNDERLINEHOT | LVS_EX_SUBITEMIMAGES | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);

    int i = 0;
    m_tab.InsertItem(i++, _T("进程管理"));
    m_tab.InsertItem(i++, _T("窗口管理"));
    m_tab.InsertItem(i++, _T("网络连接"));
    m_tab.InsertItem(i++, _T("软件信息"));
    m_tab.InsertItem(i++, _T("浏览记录"));
    m_tab.InsertItem(i++, _T("收 藏 夹"));
    m_tab.InsertItem(i++, _T("WIN32服务"));
    m_tab.InsertItem(i++, _T("驱动服务"));
    m_tab.InsertItem(i++, _T("计划任务"));
    m_tab.InsertItem(i++, _T("HOSTS"));

    if (!m_wndStatusBar.Create(this) ||
        !m_wndStatusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(UINT))) {
        TRACE0("Failed to create status bar\n");
        return -1;      // fail to create
    }

    m_wndStatusBar.SetPaneInfo(0, m_wndStatusBar.GetItemID(0), SBPS_NORMAL, 300);
    m_wndStatusBar.SetPaneInfo(1, m_wndStatusBar.GetItemID(1), SBPS_STRETCH, 0);
    m_wndStatusBar.SetPaneInfo(2, m_wndStatusBar.GetItemID(2), SBPS_NORMAL, 300);

    m_wndStatusBar.SetPaneText(0, _T("就绪"));
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0); //显示状态栏

    HWND hWndHeader = m_list.GetDlgItem(0)->GetSafeHwnd();

    AdjustList();
    BYTE lpBuffer = COMMAND_MACHINE_PROCESS;
    m_ContextObject->Send2Client((LPBYTE)&lpBuffer, 1);

    return TRUE;
}

CString CMachineDlg::__MakePriority(DWORD dwPriClass)
{
    CString strRet;
    switch (dwPriClass) {
    case REALTIME_PRIORITY_CLASS:
        strRet = _T("实时");
        break;
    case HIGH_PRIORITY_CLASS:
        strRet = _T("高");
        break;
    case ABOVE_NORMAL_PRIORITY_CLASS:
        strRet = _T("高于标准");
        break;
    case NORMAL_PRIORITY_CLASS:
        strRet = _T("标准");
        break;
    case BELOW_NORMAL_PRIORITY_CLASS:
        strRet = _T("低于标准");
        break;
    case IDLE_PRIORITY_CLASS:
        strRet = _T("空闲");
        break;
    default:
        strRet = _T("未知");
        break;
    }

    return strRet;
}

void CMachineDlg::OnReceiveComplete()
{
    if (m_bIsClosed) return;
    SetReceivingStatus(true);

    if (TOKEN_MACHINE_MSG == m_ContextObject->m_DeCompressionBuffer.GetBuffer(0)[0]) {
        CString strResult = (char*)m_ContextObject->m_DeCompressionBuffer.GetBuffer(1);
        PostMessage(WM_SHOW_MSG, (WPARAM)new CString(strResult), 0);
        SetReceivingStatus(false);
        return;
    }

    DeleteList();

    if (m_ContextObject->m_DeCompressionBuffer.GetBufferLen() <= 2) {
        PostMessage(WM_SHOW_MSG, (WPARAM)new CString(_T("无权限或无记录...")), 0);
        SetReceivingStatus(false);
        return;
    }

    PostMessage(WM_WAIT_MSG, TRUE, 0);
    switch (m_ContextObject->m_DeCompressionBuffer.GetBuffer(0)[0]) {
    case TOKEN_MACHINE_PROCESS:
        ShowProcessList();
        break;
    case TOKEN_MACHINE_WINDOWS:
        ShowWindowsList();
        break;
    case TOKEN_MACHINE_NETSTATE:
        ShowNetStateList();
        break;
    case TOKEN_MACHINE_SOFTWARE:
        ShowSoftWareList();
        break;
    case TOKEN_MACHINE_HTML:
        ShowIEHistoryList();
        break;
    case TOKEN_MACHINE_FAVORITES:
        ShowFavoritesUrlList();
        break;
    case TOKEN_MACHINE_HOSTS:
        ShowHostsList();
        break;
    case TOKEN_MACHINE_SERVICE_LIST:
        ShowServiceList();
        break;
    case TOKEN_MACHINE_TASKLIST:
        ShowTaskList();
        break;

    default:
        // 传输发生异常数据
        break;
    }
    SetReceivingStatus(false);
    PostMessage(WM_WAIT_MSG, FALSE, 0);
}

void CMachineDlg::OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult)
{
    int nID = m_tab.GetCurSel();
    switch (nID) {
    case COMMAND_MACHINE_WIN32SERVICE:
        OpenInfoDlg();
        break;
    case COMMAND_MACHINE_DRIVERSERVICE:
        OpenInfoDlg();
        break;
    default:
        break;
    }
    *pResult = 0;
}

void CMachineDlg::OnRclickList(NMHDR* pNMHDR, LRESULT* pResult)
{
    int nID = m_tab.GetCurSel();
    switch (nID) {
    case COMMAND_MACHINE_PROCESS:
        ShowProcessList_menu();
        break;
    case COMMAND_MACHINE_WINDOWS:
        ShowWindowsList_menu();
        break;
    case COMMAND_MACHINE_NETSTATE:
        ShowNetStateList_menu();
        break;
    case COMMAND_MACHINE_SOFTWARE:
        ShowSoftWareList_menu();
        break;
    case COMMAND_MACHINE_HTML:
        ShowIEHistoryList_menu();
        break;
    case COMMAND_MACHINE_FAVORITES:
        ShowFavoritesUrlList_menu();
        break;
    case COMMAND_MACHINE_WIN32SERVICE:
        ShowServiceList_menu();
        break;
    case COMMAND_MACHINE_DRIVERSERVICE:
        ShowServiceList_menu();
        break;
    case COMMAND_MACHINE_TASK:
        ShowTaskList_menu();
        break;
    case COMMAND_MACHINE_HOSTS:
        ShowHostsList_menu();
        break;
    default:
        break;
    }

    *pResult = 0;
}

void CMachineDlg::OnClose()
{
    CancelIO();
    // 等待数据处理完毕
    if (IsProcessing()) {
        ShowWindow(SW_HIDE);
        return;
    }
    DeleteList();
    if (m_wndStatusBar.GetSafeHwnd())
        m_wndStatusBar.DestroyWindow();
    SAFE_DELETE(m_IPConverter);
    CDialogBase::OnClose();
}

void CMachineDlg::reflush()
{
    int nID = m_tab.GetCurSel();
    DeleteList();
    BYTE TOKEN = MachineManager(nID);
    m_ContextObject->Send2Client((LPBYTE)&TOKEN, 1);
}


void CMachineDlg::OnSelChangeTab(NMHDR* pNMHDR, LRESULT* pResult)
{
    reflush();
    *pResult = 0;
}

void CMachineDlg::OnSelChangingTab(NMHDR* pNMHDR, LRESULT* pResult)
{
    if (*pResult = IsReceivingData()) {
        m_wndStatusBar.SetPaneText(0, "正在接收数据 - 请稍后...");
    }
}

LRESULT CMachineDlg::OnShowMessage(WPARAM wParam, LPARAM lParam)
{
    CString* msg = (CString*)wParam;
    m_wndStatusBar.SetPaneText(0, *msg);
    delete msg;

    return 0;
}


LRESULT CMachineDlg::OnWaitMessage(WPARAM wParam, LPARAM lParam)
{
    wParam ? BeginWaitCursor() : EndWaitCursor();
    return 0;
}

void CMachineDlg::DeleteList()
{
    if (!m_list.GetSafeHwnd()) return;

    for (int i=0, n=m_list.GetItemCount(); i<n; ++i) {
        ListItem* item = (ListItem*)m_list.GetItemData(i);
        if(item) item->Destroy();
    }
    m_list.DeleteAllItems();

    int nColumnCount = m_list.GetHeaderCtrl()->GetItemCount();
    for (int n = 0; n < nColumnCount; n++) {
        m_list.DeleteColumn(0);
    }
    if (!m_bIsClosed)
        PostMessage(WM_SHOW_MSG, (WPARAM)new CString(_T("请等待数据返回...")), 0);
}

void CMachineDlg::ShowProcessList()
{
    m_list.InsertColumn(0, _T("映像名称"), LVCFMT_LEFT, 100);
    m_list.InsertColumn(1, _T("PID"), LVCFMT_LEFT, 50);
    m_list.InsertColumn(2, _T("优先级"), LVCFMT_LEFT, 50);
    m_list.InsertColumn(3, _T("线程数"), LVCFMT_LEFT, 50);
    m_list.InsertColumn(4, _T("用户名"), LVCFMT_LEFT, 70);
    m_list.InsertColumn(5, _T("内存"), LVCFMT_LEFT, 70);
    m_list.InsertColumn(6, _T("文件大小"), LVCFMT_LEFT, 80);
    m_list.InsertColumn(7, _T("程序路径"), LVCFMT_LEFT, 300);
    m_list.InsertColumn(8, _T("窗口名称"), LVCFMT_LEFT, 100);
    m_list.InsertColumn(9, _T("进程位数"), LVCFMT_LEFT, 80);

    char* lpBuffer = (char*)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
    DWORD	dwOffset = 0;
    CString str;
    int i = 0;
    for (i = 0; dwOffset < m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1; i++) {
        LPDWORD	lpPID = LPDWORD(lpBuffer + dwOffset);
        bool* is64 = (bool*)(lpBuffer + dwOffset + sizeof(DWORD));
        char* szBuf_title = (char*)(lpBuffer + dwOffset + sizeof(DWORD) + sizeof(bool));
        char* strExeFile = (char*)((byte*)szBuf_title + MAX_PATH * sizeof(char));
        char* strProcessName = (char*)((byte*)strExeFile + lstrlen(strExeFile) * sizeof(char) + 2);
        char* strTemp = (char*)((byte*)strProcessName + lstrlen(strProcessName) * sizeof(char) + 2);
        LPDWORD	lpdwPriClass = LPDWORD((byte*)strTemp);
        LPDWORD	lpdwThreads = LPDWORD((byte*)strTemp + sizeof(DWORD));
        char* strProcessUser = (char*)((byte*)strTemp + sizeof(DWORD) * 2);
        LPDWORD	lpdwWorkingSetSize = LPDWORD((byte*)strProcessUser + lstrlen(strProcessUser) * sizeof(char) + 2);

        LPDWORD	lpdwFileSize = LPDWORD((byte*)strProcessUser + lstrlen(strProcessUser) * sizeof(char) + 2 + sizeof(DWORD));

        m_list.InsertItem(i, strExeFile, 0);

        str.Format(_T("%5u"), *lpPID);
        m_list.SetItemText(i, 1, str);

        m_list.SetItemText(i, 2, __MakePriority(*lpdwPriClass));

        str.Format(_T("%5u"), *lpdwThreads);
        m_list.SetItemText(i, 3, str);

        m_list.SetItemText(i, 4, strProcessUser);

        str.Format(_T("%5u K"), *lpdwWorkingSetSize);
        m_list.SetItemText(i, 5, str);

        str.Format(_T("%5u KB"), *lpdwFileSize);
        m_list.SetItemText(i, 6, str);

        m_list.SetItemText(i, 7, strProcessName);

        m_list.SetItemText(i, 8, szBuf_title);

        m_list.SetItemText(i, 9, (*is64) ? _T("x64") : _T("x86"));
        // ListItem 为进程ID
        m_list.SetItemData(i, (DWORD_PTR)new ListItem(m_list, i, *lpPID));
        dwOffset += sizeof(DWORD) * 5 + sizeof(bool) + MAX_PATH * sizeof(char) + lstrlen(strExeFile) * sizeof(char) +
                    lstrlen(strProcessName) * sizeof(char) + lstrlen(strProcessUser) * sizeof(char) + 6;
    }

    str.Format(_T("程序路径 / %d"), i);
    LVCOLUMN lvc;
    lvc.mask = LVCF_TEXT;
    lvc.pszText = str.GetBuffer(0);
    lvc.cchTextMax = str.GetLength();
    m_list.SetColumn(7, &lvc);

    PostMessage(WM_SHOW_MSG, (WPARAM)new CString("接收数据完成"), 0);
}


void CMachineDlg::ShowWindowsList()
{
    m_list.InsertColumn(0, _T("PID"), LVCFMT_LEFT, 75);
    m_list.InsertColumn(1, _T("句柄HWND"), LVCFMT_LEFT, 75);
    m_list.InsertColumn(2, _T("窗口名称"), LVCFMT_LEFT, 300);
    m_list.InsertColumn(3, _T("窗口状态"), LVCFMT_LEFT, 100);
    m_list.InsertColumn(4, _T("大小"), LVCFMT_LEFT, 100);

    LPBYTE lpBuffer = (LPBYTE)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
    DWORD	dwOffset = 0;
    CString	str;
    int i;
    WINDOWSINFO m_ibfo;
    for (i = 0; dwOffset < m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1; i++) {
        memcpy(&m_ibfo, lpBuffer + dwOffset, sizeof(WINDOWSINFO));

        str.Format(_T("%5u"), m_ibfo.m_poceessid);
        m_list.InsertItem(i, str, 25);
        char t_hwnd[250];
        _stprintf_s(t_hwnd, 250, _T("%d"), m_ibfo.m_hwnd);
        m_list.SetItemText(i, 1, t_hwnd);
        m_list.SetItemText(i, 2, m_ibfo.strTitle);
        m_list.SetItemText(i, 3, m_ibfo.canlook ? _T("显示") : _T("隐藏"));
        str.Format(_T("%d*%d"), m_ibfo.w, m_ibfo.h);
        m_list.SetItemText(i, 4, str);
        // ListItem 为进程ID
        m_list.SetItemData(i, (DWORD_PTR)new ListItem(m_list, i, m_ibfo.m_poceessid));
        dwOffset += sizeof(WINDOWSINFO);
    }
    str.Format(_T("窗口名称 / %d"), i);
    LVCOLUMN lvc = {};
    lvc.mask = LVCF_TEXT;
    lvc.pszText = str.GetBuffer(0);
    lvc.cchTextMax = str.GetLength();
    m_list.SetColumn(2, &lvc);

    PostMessage(WM_SHOW_MSG, (WPARAM)new CString("接收数据完成"), 0);
}


void CMachineDlg::ShowNetStateList()
{
    m_list.InsertColumn(0, _T("进程名"), LVCFMT_LEFT, 100);
    m_list.InsertColumn(1, _T("PID"), LVCFMT_LEFT, 50);
    m_list.InsertColumn(2, _T("协议"), LVCFMT_LEFT, 50);
    m_list.InsertColumn(3, _T("本地地址:端口"), LVCFMT_LEFT, 130);
    m_list.InsertColumn(4, _T("远程地址:端口"), LVCFMT_LEFT, 130);
    m_list.InsertColumn(5, _T("目标IP归属地"), LVCFMT_LEFT, 140);
    m_list.InsertColumn(6, _T("连接状态"), LVCFMT_LEFT, 80);

    LPBYTE	lpBuffer = (LPBYTE)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
    DWORD	dwOffset = 0;
    CString str, IPAddress;
    for (int i = 0; dwOffset < m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1; i++) {
        int pid = 0;
        for (int j = 0; j < 7; j++) {
            if (j == 0) {
                char* lpString = (char*)(lpBuffer + dwOffset);
                m_list.InsertItem(i, lpString, 0);
                dwOffset += lstrlen(lpString) * sizeof(char) + 2;
            } else if (j == 1) {
                LPDWORD	lpPID = LPDWORD(lpBuffer + dwOffset);
                pid = *lpPID;
                str.Format(_T("%d"), *lpPID);
                m_list.SetItemText(i, j, str);
                dwOffset += sizeof(DWORD) + 2;
            } else if (j == 5) {
                IPAddress = m_list.GetItemText(i, 4);

                int n = IPAddress.ReverseFind(':');
                if (n > 0) {
                    IPAddress = IPAddress.Left(n);
                    if (!IPAddress.Compare(_T("0.0.0.0")) || !IPAddress.Compare(_T("*.*.*.*"))) {
                        str = _T("---");
                    } else {
                        str = m_IPConverter->IPtoAddress(IPAddress.GetString()).c_str();
                    }
                    m_list.SetItemText(i, j, str);
                }
            } else {
                char* lpString = (char*)(lpBuffer + dwOffset);

                m_list.SetItemText(i, j, lpString);
                dwOffset += lstrlen(lpString) * sizeof(char) + 2;
            }
        }
        m_list.SetItemData(i, (DWORD_PTR)new ListItem(m_list, i, pid));
    }
    PostMessage(WM_SHOW_MSG, (WPARAM)new CString("接收数据完成"), 0);
}


void CMachineDlg::ShowSoftWareList()
{
    m_list.InsertColumn(0, _T("软件名称"), LVCFMT_LEFT, 150);
    m_list.InsertColumn(1, _T("发行商"), LVCFMT_LEFT, 150);
    m_list.InsertColumn(2, _T("版本"), LVCFMT_LEFT, 75);
    m_list.InsertColumn(3, _T("安装时间"), LVCFMT_LEFT, 80);
    m_list.InsertColumn(4, _T("卸载命令及参数"), LVCFMT_LEFT, 400);

    LPBYTE	lpBuffer = (LPBYTE)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
    DWORD	dwOffset = 0;
    for (int i = 0; dwOffset < m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1; i++) {
        for (int j = 0; j < 5; j++) {
            char* lpString = (char*)(lpBuffer + dwOffset);
            if (j == 0)
                m_list.InsertItem(i, lpString, 0);
            else
                m_list.SetItemText(i, j, lpString);

            dwOffset += lstrlen(lpString) * sizeof(char) + 2;
        }
        m_list.SetItemData(i, (DWORD_PTR)new ListItem(m_list, i));
    }
    PostMessage(WM_SHOW_MSG, (WPARAM)new CString("接收数据完成"), 0);
}

void CMachineDlg::ShowIEHistoryList()
{
    m_list.InsertColumn(0, _T("序号"), LVCFMT_LEFT, 70);
    m_list.InsertColumn(1, _T("访问时间"), LVCFMT_LEFT, 130);
    m_list.InsertColumn(2, _T("标题"), LVCFMT_LEFT, 150);
    m_list.InsertColumn(3, _T("网页地址"), LVCFMT_LEFT, 400);
    LPBYTE	lpBuffer = (LPBYTE)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
    DWORD	dwOffset = 0;
    CString	str;
    for (int i = 0; dwOffset < m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1; i++) {
        Browsinghistory* p_Browsinghistory = (Browsinghistory*)((char*)lpBuffer + dwOffset);
        str.Format(_T("%d"), i);
        m_list.InsertItem(i, str, 0);
        m_list.SetItemText(i, 1, p_Browsinghistory->strTime);
        m_list.SetItemText(i, 2, p_Browsinghistory->strTitle);
        m_list.SetItemText(i, 3, p_Browsinghistory->strUrl);
        dwOffset += sizeof(Browsinghistory);
        m_list.SetItemData(i, (DWORD_PTR)new ListItem(m_list, i));
    }
    PostMessage(WM_SHOW_MSG, (WPARAM)new CString("接收数据完成"), 0);
}

void CMachineDlg::ShowFavoritesUrlList()
{
    m_list.InsertColumn(0, _T("收藏名称"), LVCFMT_LEFT, 200);
    m_list.InsertColumn(1, _T("Url"), LVCFMT_LEFT, 300);

    LPBYTE	lpBuffer = (LPBYTE)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
    DWORD	dwOffset = 0;
    for (int i = 0; dwOffset < m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1; i++) {
        for (int j = 0; j < 2; j++) {
            char* lpString = (char*)((char*)lpBuffer + dwOffset);
            if (j == 0)
                m_list.InsertItem(i, lpString, 0);
            else
                m_list.SetItemText(i, j, lpString);

            dwOffset += lstrlen(lpString) * sizeof(char) + 2;
        }
        m_list.SetItemData(i, (DWORD_PTR)new ListItem(m_list, i));
    }
    PostMessage(WM_SHOW_MSG, (WPARAM)new CString("接收数据完成"), 0);
}

void CMachineDlg::ShowServiceList()
{
    m_list.InsertColumn(0, _T("显示名称"), LVCFMT_LEFT, 150);
    m_list.InsertColumn(1, _T("描述"), LVCFMT_LEFT, 200);
    m_list.InsertColumn(2, _T("状态"), LVCFMT_LEFT, 70);
    m_list.InsertColumn(3, _T("启动类型"), LVCFMT_LEFT, 85);
    m_list.InsertColumn(4, _T("登陆身份"), LVCFMT_LEFT, 135);
    m_list.InsertColumn(5, _T("桌面交互"), LVCFMT_LEFT, 60);
    m_list.InsertColumn(6, _T("服务名"), LVCFMT_LEFT, 140);
    m_list.InsertColumn(7, _T("可执行文件路径"), LVCFMT_LEFT, 400);

    char* lpBuffer = (char*)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
    DWORD	dwOffset = 0;
    int i = 0;
    for (i = 0; dwOffset < (m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1) / sizeof(char); i++) {
        char* DisplayName = lpBuffer + dwOffset;
        char* Describe = DisplayName + lstrlen(DisplayName) + 1;
        char* serRunway = Describe + lstrlen(Describe) + 1;
        char* serauto = serRunway + lstrlen(serRunway) + 1;
        char* Login = serauto + lstrlen(serauto) + 1;
        char* InterActive = Login + lstrlen(Login) + 1;
        char* ServiceName = InterActive + lstrlen(InterActive) + 1;
        char* serfile = ServiceName + lstrlen(ServiceName) + 1;

        m_list.InsertItem(i, DisplayName, 0);
        m_list.SetItemText(i, 1, Describe);
        m_list.SetItemText(i, 2, serRunway);
        m_list.SetItemText(i, 3, serauto);
        m_list.SetItemText(i, 4, Login);
        m_list.SetItemText(i, 5, InterActive);
        m_list.SetItemText(i, 6, ServiceName);
        m_list.SetItemText(i, 7, serfile);
        m_list.SetItemData(i, (DWORD_PTR)new ListItem(m_list, i));

        dwOffset += lstrlen(DisplayName) + lstrlen(Describe) + lstrlen(serRunway) + lstrlen(serauto) +
                    lstrlen(Login) + lstrlen(InterActive) + lstrlen(ServiceName) + lstrlen(serfile) + 8;
    }
    CString strMsgShow;
    if (i <= 0) {
        strMsgShow.Format(_T("无权限或无数据"));
    } else {
        strMsgShow.Format(_T("共 %d 个服务"), i);
    }
    PostMessage(WM_SHOW_MSG, (WPARAM)new CString(strMsgShow), 0);
}

void CMachineDlg::ShowTaskList()
{
    m_list.InsertColumn(0, _T("序号"), LVCFMT_LEFT, 50);
    m_list.InsertColumn(1, _T("目录"), LVCFMT_LEFT, 200);
    m_list.InsertColumn(2, _T("任务名称"), LVCFMT_LEFT, 300);
    m_list.InsertColumn(3, _T("程序路径"), LVCFMT_LEFT, 400);
    m_list.InsertColumn(4, _T("状态"), LVCFMT_LEFT, 50);
    m_list.InsertColumn(5, _T("最后执行时间"), LVCFMT_LEFT, 130);
    m_list.InsertColumn(6, _T("下次执行时间"), LVCFMT_LEFT, 130);

    BYTE* lpBuffer = (BYTE*)(m_ContextObject->m_DeCompressionBuffer.GetBuffer() + 1);
    DATE lasttime = 0;
    DATE nexttime = 0;
    DWORD	dwOffset = 0;
    CString str;
    for (int i = 0; dwOffset < m_ContextObject->m_DeCompressionBuffer.GetBufferLen() - 1; i++) {
        char* taskname = (char*)(lpBuffer + dwOffset);
        char* taskpath = taskname + lstrlen(taskname) + 1;
        char* exepath = taskpath + lstrlen(taskpath) + 1;
        char* status = exepath + lstrlen(exepath) + 1;
        lasttime = *((DATE*)(status + lstrlen(status) + 1));
        nexttime = *((DATE*)((CHAR*)(status + lstrlen(status) + 1) + sizeof(DATE)));
        ULONGLONG a = *((ULONGLONG*)(&lasttime));
        str.Format(_T("%d"), i + 1);
        if(!m_list.GetSafeHwnd())
            continue;
        m_list.InsertItem(i, str);

        str = taskpath;
        str.Replace(taskname, _T(""));
        m_list.SetItemText(i, 1, str);
        m_list.SetItemText(i, 2, taskname);
        m_list.SetItemText(i, 3, exepath);
        m_list.SetItemText(i, 4, status);
        str = oleTime2Str(lasttime);
        m_list.SetItemText(i, 5, str);
        str = oleTime2Str(nexttime);
        m_list.SetItemText(i, 6, str);
        m_list.SetItemData(i, (DWORD_PTR)new ListItem(m_list, i));

        dwOffset += lstrlen(taskname) + 1 + lstrlen(taskpath) + 1 + lstrlen(exepath) + 1 + lstrlen(status) + 1 + sizeof(DATE) * 2;

        if (lpBuffer[dwOffset] == 0 && lpBuffer[dwOffset + 1] == 0) {
            break;
        }
    }
    PostMessage(WM_SHOW_MSG, (WPARAM)new CString("接收数据完成"), 0);
}

void CMachineDlg::ShowHostsList()
{
    m_list.InsertColumn(0, _T("数据"), LVCFMT_LEFT, 600);

    LPBYTE	lpBuffer = (LPBYTE)(m_ContextObject->m_DeCompressionBuffer.GetBuffer(1));
    int i = 0;
    char* buf=nullptr;
    char* lpString = (char*)lpBuffer;
    const char* d = "\n";
    char* p = strtok_s(lpString, d, &buf);
    while (p) {
        CString tem = p;
        m_list.InsertItem(i, tem);
        p = strtok_s(NULL, d, &buf);
        m_list.SetItemData(i, (DWORD_PTR)new ListItem(m_list, i));
        i++;
    }
    PostMessage(WM_SHOW_MSG, (WPARAM)new CString("接收数据完成"), 0);
}

void CMachineDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    // TODO: Add your message handler code here
    if (IsWindowVisible())
        AdjustList();

    // 状态栏还没有创建
    if (m_wndStatusBar.m_hWnd == NULL)
        return;

    // 定位状态栏
    RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, 0); //显示工具栏
}

void CMachineDlg::AdjustList()
{
    RECT	rectClient;
    RECT	rectList;
    GetClientRect(&rectClient);
    rectList.left = 0;
    rectList.top = 22;
    rectList.right = rectClient.right;
    rectList.bottom = rectClient.bottom - 20;

    m_list.MoveWindow(&rectList);
}


void CMachineDlg::OpenInfoDlg()
{
    int   nItem = -1;
    nItem = m_list.GetNextItem(nItem, LVNI_SELECTED);
    if (nItem == -1)
        return;

    CServiceInfoDlg pDlg(this);
    pDlg.m_ContextObject = m_ContextObject;

    pDlg.m_ServiceInfo.strSerName = m_list.GetItemText(nItem, 6);
    pDlg.m_ServiceInfo.strSerDisPlayname = m_list.GetItemText(nItem, 0);
    pDlg.m_ServiceInfo.strSerDescription = m_list.GetItemText(nItem, 1);
    pDlg.m_ServiceInfo.strFilePath = m_list.GetItemText(nItem, 7);
    pDlg.m_ServiceInfo.strSerRunway = m_list.GetItemText(nItem, 3);
    pDlg.m_ServiceInfo.strSerState = m_list.GetItemText(nItem, 2);

    pDlg.DoModal();
}

void CMachineDlg::SendToken(BYTE bToken)
{
    CString		tSerName;

    int		nItem = m_list.GetNextItem(-1, LVNI_SELECTED);
    tSerName = m_list.GetItemText(nItem, 6);

    int s = tSerName.Find(_T("*"));
    if (s == 0) {
        tSerName = tSerName.Right(tSerName.GetLength() - 1);
    }

    int nPacketLength = (tSerName.GetLength() * sizeof(char) + 1);;
    LPBYTE lpBuffer = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
    lpBuffer[0] = bToken;

    memcpy(lpBuffer + 1, tSerName.GetBuffer(0), tSerName.GetLength() * sizeof(char));
    m_ContextObject->Send2Client(lpBuffer, nPacketLength);
    LocalFree(lpBuffer);
}

/////////////////////////////////////////// 菜单 ///////////////////////////////////////////

void CMachineDlg::SetClipboardText(CString& Data)
{
    CStringA source = Data;
    // 文本内容保存在source变量中
    if (OpenClipboard()) {
        HGLOBAL clipbuffer;
        char* buffer;
        EmptyClipboard();
        clipbuffer = GlobalAlloc(GMEM_DDESHARE, source.GetLength() + 1);
        buffer = (char*)GlobalLock(clipbuffer);
        strcpy_s(buffer, strlen(source) + 1, LPCSTR(source));
        GlobalUnlock(clipbuffer);
        SetClipboardData(CF_TEXT, clipbuffer);
        CloseClipboard();
    }
}


void CMachineDlg::ShowProcessList_menu()
{
    CMenu menu;
    VERIFY(menu.CreatePopupMenu());
    menu.AppendMenu(MF_STRING | MF_ENABLED, 50, _T("刷新数据(&F)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 100, _T("复制数据(&V)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 200, _T("删除文件(&C)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 300, _T("结束进程(&E)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 400, _T("冻结进程(&D)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 500, _T("解冻进程(&J)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 600, _T("强删文件(&Q)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 700, _T("注入管理(&I)"));

    CPoint	p;
    GetCursorPos(&p);
    int nMenuResult = ::TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, 0, GetSafeHwnd(), NULL);
    if (!nMenuResult) 	return;
    switch (nMenuResult) {
    case 50:
        reflush();
        break;
    case 100: {
        if (m_list.GetSelectedCount() < 1) {
            return;
        }
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        CString Data;
        CString temp;
        while (pos) {
            temp = _T("");
            int	nItem = m_list.GetNextSelectedItem(pos);
            for (int i = 0; i < m_list.GetHeaderCtrl()->GetItemCount(); i++) {
                temp += m_list.GetItemText(nItem, i);
                temp += _T("	");
            }
            Data += temp;
            Data += _T("\r\n");
        }
        SetClipboardText(Data);
        MessageBox(_T("已复制数据到剪切板!"), "提示");
    }
    break;
    case 200: {
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            int	nItem = m_list.GetNextSelectedItem(pos);
            LPBYTE lpBuffer = new BYTE[1 + sizeof(DWORD)];
            lpBuffer[0] = COMMAND_PROCESS_KILLDEL;
            DWORD dwProcessID = ((ListItem*)m_list.GetItemData(nItem))->pid;
            memcpy(lpBuffer + 1, &dwProcessID, sizeof(DWORD));
            m_ContextObject->Send2Client(lpBuffer, sizeof(DWORD) + 1);
            SAFE_DELETE_AR(lpBuffer);
        }
    }
    break;
    case 300: {
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            int	nItem = m_list.GetNextSelectedItem(pos);
            LPBYTE lpBuffer = new BYTE[1 + sizeof(DWORD)];
            lpBuffer[0] = COMMAND_PROCESS_KILL;
            DWORD dwProcessID = ((ListItem*)m_list.GetItemData(nItem))->pid;
            memcpy(lpBuffer + 1, &dwProcessID, sizeof(DWORD));
            m_ContextObject->Send2Client(lpBuffer, sizeof(DWORD) + 1);
            SAFE_DELETE_AR(lpBuffer);
        }
    }
    break;
    case 400: {
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            int	nItem = m_list.GetNextSelectedItem(pos);
            LPBYTE lpBuffer = new BYTE[1 + sizeof(DWORD)];
            lpBuffer[0] = COMMAND_PROCESS_FREEZING;
            DWORD dwProcessID = ((ListItem*)m_list.GetItemData(nItem))->pid;
            memcpy(lpBuffer + 1, &dwProcessID, sizeof(DWORD));
            m_ContextObject->Send2Client(lpBuffer, sizeof(DWORD) + 1);
            SAFE_DELETE_AR(lpBuffer);
        }
    }
    break;
    case 500: {
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            int	nItem = m_list.GetNextSelectedItem(pos);
            LPBYTE lpBuffer = new BYTE[1 + sizeof(DWORD)];
            lpBuffer[0] = COMMAND_PROCESS_THAW;
            DWORD dwProcessID = ((ListItem*)m_list.GetItemData(nItem))->pid;
            memcpy(lpBuffer + 1, &dwProcessID, sizeof(DWORD));
            m_ContextObject->Send2Client(lpBuffer, sizeof(DWORD) + 1);
            SAFE_DELETE_AR(lpBuffer);
        }
    }
    break;
    case 600: {
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            int	nItem = m_list.GetNextSelectedItem(pos);
            LPBYTE lpBuffer = new BYTE[1 + sizeof(DWORD)];
            lpBuffer[0] = COMMAND_PROCESS_DEL;
            DWORD dwProcessID = ((ListItem*)m_list.GetItemData(nItem))->pid;
            memcpy(lpBuffer + 1, &dwProcessID, sizeof(DWORD));
            m_ContextObject->Send2Client(lpBuffer, sizeof(DWORD) + 1);
            SAFE_DELETE_AR(lpBuffer);
        }
    }
    break;
    case 700: {
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            int	nItem = m_list.GetNextSelectedItem(pos);
            DWORD dwProcessID = ((ListItem*)m_list.GetItemData(nItem))->pid;

            CInjectCodeDlg dlg;
            if (dlg.DoModal() != IDOK) {
                return;
            }
            InjectData* p_InjectData = new InjectData;
            ZeroMemory(p_InjectData, sizeof(InjectData));
            p_InjectData->mode = dlg.isel;
            p_InjectData->dwProcessID = dwProcessID;
            CString strexeis86 = m_list.GetItemText(nItem, 9);
            strexeis86 == _T("x86") ? p_InjectData->ExeIsx86 = 1 : p_InjectData->ExeIsx86 = 0;
            memcpy(p_InjectData->strpath, dlg.Str_remote, dlg.Str_remote.GetLength() * 2 + 2);
            //读取文件
            BYTE* lpBuffer = NULL;
            HANDLE hFile = CreateFile(dlg.Str_loacal, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                PostMessage(WM_SHOW_MSG, (WPARAM)new CString(_T("打开文件失败...")), 0);
            } else {
                p_InjectData->datasize = GetFileSize(hFile, NULL);
                int allsize = p_InjectData->datasize + sizeof(InjectData)+1;
                lpBuffer = new BYTE[allsize];
                ZeroMemory(lpBuffer, allsize);
                lpBuffer[0]= COMMAND_INJECT;
                memcpy(lpBuffer+1, p_InjectData, sizeof(InjectData));
                DWORD wr = 0;
                ReadFile(hFile, lpBuffer + sizeof(InjectData)+1, p_InjectData->datasize, &wr, NULL);
                SAFE_CLOSE_HANDLE(hFile);
                m_ContextObject->Send2Client(lpBuffer, allsize);
                SAFE_DELETE_AR(lpBuffer);
            }
            SAFE_DELETE(p_InjectData);
            break;
        }
    }
    break;
    default:
        break;
    }

    menu.DestroyMenu();
}

void CMachineDlg::ShowWindowsList_menu()
{
    CMenu menu;
    VERIFY(menu.CreatePopupMenu());
    menu.AppendMenu(MF_STRING | MF_ENABLED, 50, _T("刷新数据(&F)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 100, _T("复制数据(&V)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 200, _T("还原窗口(&H)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 300, _T("隐藏窗口(&Y)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 400, _T("关闭窗口(&E)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 500, _T("最 大 化(&M)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 600, _T("最 小 化(&I)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 700, _T("冻结进程(&D)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 800, _T("解冻进程(&J)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 900, _T("结束进程(&E)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    CPoint	p;
    GetCursorPos(&p);
    int nMenuResult = ::TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, 0, GetSafeHwnd(), NULL);
    if (!nMenuResult) 	return;
    switch (nMenuResult) {
    case 50:
        reflush();
        break;
    case 100: {
        if (m_list.GetSelectedCount() < 1) {
            return;
        }
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        CString Data;
        CString temp;
        while (pos) {
            temp = _T("");
            int	nItem = m_list.GetNextSelectedItem(pos);
            for (int i = 0; i < m_list.GetHeaderCtrl()->GetItemCount(); i++) {
                temp += m_list.GetItemText(nItem, i);
                temp += _T("	");
            }
            Data += temp;
            Data += _T("\r\n");
        }
        SetClipboardText(Data);
        MessageBox(_T("已复制数据到剪切板!"), "提示");
    }
    break;
    case 200: {
        BYTE lpMsgBuf[20];
        int	nItem = m_list.GetSelectionMark();
        if (nItem >= 0) {
            ZeroMemory(lpMsgBuf, 20);
            lpMsgBuf[0] = COMMAND_WINDOW_OPERATE;
            DWORD hwnd = _tstoi(m_list.GetItemText(nItem, 1));
            m_list.SetItemText(nItem, 3, _T("发送还原命令"));
            memcpy(lpMsgBuf + 1, &hwnd, sizeof(DWORD));
            DWORD dHow = SW_RESTORE;
            memcpy(lpMsgBuf + 1 + sizeof(hwnd), &dHow, sizeof(DWORD));
            m_ContextObject->Send2Client(lpMsgBuf, sizeof(lpMsgBuf));
        }
    }
    break;
    case 300: {
        BYTE lpMsgBuf[20];
        int	nItem = m_list.GetSelectionMark();
        if (nItem >= 0) {
            ZeroMemory(lpMsgBuf, 20);
            lpMsgBuf[0] = COMMAND_WINDOW_OPERATE;
            DWORD hwnd = _tstoi(m_list.GetItemText(nItem, 1));
            m_list.SetItemText(nItem, 3, _T("发送隐藏命令"));
            memcpy(lpMsgBuf + 1, &hwnd, sizeof(DWORD));
            DWORD dHow = SW_HIDE;
            memcpy(lpMsgBuf + 1 + sizeof(hwnd), &dHow, sizeof(DWORD));
            m_ContextObject->Send2Client(lpMsgBuf, sizeof(lpMsgBuf));
        }
    }
    break;
    case 400: {
        // TODO: Add your command handler code here
        BYTE lpMsgBuf[20];
        int	nItem = m_list.GetSelectionMark();
        if (nItem >= 0) {
            ZeroMemory(lpMsgBuf, 20);
            lpMsgBuf[0] = COMMAND_WINDOW_CLOSE;
            DWORD hwnd = _tstoi(m_list.GetItemText(nItem, 1));
            m_list.SetItemText(nItem, 3, _T("发送关闭命令"));
            memcpy(lpMsgBuf + 1, &hwnd, sizeof(DWORD));
            m_ContextObject->Send2Client(lpMsgBuf, sizeof(lpMsgBuf));
        }
    }
    break;
    case 500: {
        BYTE lpMsgBuf[20];
        int	nItem = m_list.GetSelectionMark();
        if (nItem >= 0) {
            ZeroMemory(lpMsgBuf, 20);
            lpMsgBuf[0] = COMMAND_WINDOW_OPERATE;
            DWORD hwnd = _tstoi(m_list.GetItemText(nItem, 1));
            m_list.SetItemText(nItem, 3, _T("发送最大化命令"));
            memcpy(lpMsgBuf + 1, &hwnd, sizeof(DWORD));
            DWORD dHow = SW_MAXIMIZE;
            memcpy(lpMsgBuf + 1 + sizeof(hwnd), &dHow, sizeof(DWORD));
            m_ContextObject->Send2Client(lpMsgBuf, sizeof(lpMsgBuf));
        }
    }
    break;
    case 600: {
        BYTE lpMsgBuf[20];
        int	nItem = m_list.GetSelectionMark();
        if (nItem >= 0) {
            ZeroMemory(lpMsgBuf, 20);
            lpMsgBuf[0] = COMMAND_WINDOW_OPERATE;
            DWORD hwnd = _tstoi(m_list.GetItemText(nItem, 1));
            m_list.SetItemText(nItem, 3, _T("发送最小化命令"));
            memcpy(lpMsgBuf + 1, &hwnd, sizeof(DWORD));
            DWORD dHow = SW_MINIMIZE;
            memcpy(lpMsgBuf + 1 + sizeof(hwnd), &dHow, sizeof(DWORD));
            m_ContextObject->Send2Client(lpMsgBuf, sizeof(lpMsgBuf));
        }
    }
    break;
    case 700: {
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            int	nItem = m_list.GetNextSelectedItem(pos);
            LPBYTE lpBuffer = new BYTE[1 + sizeof(DWORD)];
            lpBuffer[0] = COMMAND_PROCESS_FREEZING;
            CString pid;
            pid = m_list.GetItemText(nItem, 0);
            DWORD	dwProcessID = _tstoi(pid);
            memcpy(lpBuffer + 1, &dwProcessID, sizeof(DWORD));
            m_ContextObject->Send2Client(lpBuffer, sizeof(DWORD) + 1);
            SAFE_DELETE_AR(lpBuffer);
        }
    }
    break;
    case 800: {
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            int	nItem = m_list.GetNextSelectedItem(pos);
            LPBYTE lpBuffer = new BYTE[1 + sizeof(DWORD)];
            lpBuffer[0] = COMMAND_PROCESS_THAW;
            CString pid;
            pid = m_list.GetItemText(nItem, 0);
            DWORD	dwProcessID = _tstoi(pid);
            memcpy(lpBuffer + 1, &dwProcessID, sizeof(DWORD));
            m_ContextObject->Send2Client(lpBuffer, sizeof(DWORD) + 1);
            SAFE_DELETE_AR(lpBuffer);
        }
    }
    break;
    case 900: {
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            int	nItem = m_list.GetNextSelectedItem(pos);
            LPBYTE lpBuffer = new BYTE[1 + sizeof(DWORD)];
            lpBuffer[0] = COMMAND_PROCESS_KILL;
            CString pid;
            pid = m_list.GetItemText(nItem, 0);
            DWORD	dwProcessID = _tstoi(pid);
            memcpy(lpBuffer + 1, &dwProcessID, sizeof(DWORD));
            m_ContextObject->Send2Client(lpBuffer, sizeof(DWORD) + 1);
            SAFE_DELETE_AR(lpBuffer);
        }
    }
    break;
    default:
        break;
    }

    menu.DestroyMenu();
}

void CMachineDlg::ShowNetStateList_menu()
{
    CMenu menu;
    VERIFY(menu.CreatePopupMenu());
    menu.AppendMenu(MF_STRING | MF_ENABLED, 50, _T("刷新数据(&F)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 100, _T("复制数据(&V)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 150, _T("结束进程(&C)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    CPoint	p;
    GetCursorPos(&p);
    int nMenuResult = ::TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, 0, GetSafeHwnd(), NULL);
    if (!nMenuResult) 	return;
    switch (nMenuResult) {
    case 50:
        reflush();
        break;
    case 100: {
        if (m_list.GetSelectedCount() < 1) {
            return;
        }
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        CString Data;
        CString temp;
        while (pos) {
            temp = _T("");
            int	nItem = m_list.GetNextSelectedItem(pos);
            for (int i = 0; i < m_list.GetHeaderCtrl()->GetItemCount(); i++) {
                temp += m_list.GetItemText(nItem, i);
                temp += _T("	");
            }
            Data += temp;
            Data += _T("\r\n");
        }
        SetClipboardText(Data);
        MessageBox(_T("已复制数据到剪切板!"), "提示");
    }
    break;
    case 150: {
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        while (pos) {
            int	nItem = m_list.GetNextSelectedItem(pos);
            LPBYTE lpBuffer = new BYTE[1 + sizeof(DWORD)];
            lpBuffer[0] = COMMAND_PROCESS_KILL;
            DWORD dwProcessID = ((ListItem*)m_list.GetItemData(nItem))->pid;
            memcpy(lpBuffer + 1, &dwProcessID, sizeof(DWORD));
            m_ContextObject->Send2Client(lpBuffer, sizeof(DWORD) + 1);
            SAFE_DELETE_AR(lpBuffer);
        }
    }
    break;
    default:
        break;
    }
    menu.DestroyMenu();
}


void CMachineDlg::ShowSoftWareList_menu()
{
    CMenu menu;
    VERIFY(menu.CreatePopupMenu());
    menu.AppendMenu(MF_STRING | MF_ENABLED, 50, _T("刷新数据(&F)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 100, _T("复制数据(&V)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 200, _T("卸载程序(&X)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    CPoint	p;
    GetCursorPos(&p);
    int nMenuResult = ::TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, 0, GetSafeHwnd(), NULL);
    if (!nMenuResult) 	return;
    switch (nMenuResult) {
    case 50:
        reflush();
        break;
    case 100: {
        if (m_list.GetSelectedCount() < 1) {
            return;
        }
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        CString Data;
        CString temp;
        while (pos) {
            temp = _T("");
            int	nItem = m_list.GetNextSelectedItem(pos);
            for (int i = 0; i < m_list.GetHeaderCtrl()->GetItemCount(); i++) {
                temp += m_list.GetItemText(nItem, i);
                temp += _T("	");
            }
            Data += temp;
            Data += _T("\r\n");
        }
        SetClipboardText(Data);
        MessageBox(_T("已复制数据到剪切板!"), "提示");
    }
    break;
    case 200: {
        if (m_list.GetSelectedCount() < 1) {
            return;
        }

        if (MessageBox(_T("确定要卸载该程序?"), _T("提示"), MB_YESNO | MB_ICONQUESTION) == IDNO)
            return;

        POSITION pos = m_list.GetFirstSelectedItemPosition();
        CString str;
        CStringA str_a;
        while (pos) {
            int	nItem = m_list.GetNextSelectedItem(pos);
            str = m_list.GetItemText(nItem, 4);

            if (str.GetLength() > 0) {
                str_a = str;
                LPBYTE lpBuffer = new BYTE[1 + str_a.GetLength()];
                lpBuffer[0] = COMMAND_APPUNINSTALL;
                memcpy(lpBuffer + 1, str_a.GetBuffer(0), str_a.GetLength());
                m_ContextObject->Send2Client(lpBuffer, str_a.GetLength() + 1);
                SAFE_DELETE_AR(lpBuffer);
            }
        }
    }
    break;
    default:
        break;
    }

    menu.DestroyMenu();
}

void CMachineDlg::ShowIEHistoryList_menu()
{
    CMenu menu;
    VERIFY(menu.CreatePopupMenu());
    menu.AppendMenu(MF_STRING | MF_ENABLED, 50, _T("刷新数据(&F)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 100, _T("复制数据(&V)"));

    menu.AppendMenu(MF_SEPARATOR, NULL);
    CPoint	p;
    GetCursorPos(&p);
    int nMenuResult = ::TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, 0, GetSafeHwnd(), NULL);
    if (!nMenuResult) 	return;
    switch (nMenuResult) {
    case 50:
        reflush();
        break;
    case 100: {
        if (m_list.GetSelectedCount() < 1) {
            return;
        }
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        CString Data;
        CString temp;
        while (pos) {
            temp = _T("");
            int	nItem = m_list.GetNextSelectedItem(pos);
            for (int i = 0; i < m_list.GetHeaderCtrl()->GetItemCount(); i++) {
                temp += m_list.GetItemText(nItem, i);
                temp += _T("	");
            }
            Data += temp;
            Data += _T("\r\n");
        }
        SetClipboardText(Data);
        MessageBox(_T("已复制数据到剪切板!"), "提示");
    }
    break;

    default:
        break;
    }

    menu.DestroyMenu();
}

void CMachineDlg::ShowTaskList_menu()
{
    CMenu menu;
    VERIFY(menu.CreatePopupMenu());
    menu.AppendMenu(MF_STRING | MF_ENABLED, 100, _T("&(R)执行任务"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 101, _T("&(T)停止任务"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 102, _T("&(D)删除任务"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 103, _T("&(C)创建任务"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 104, _T("&(F)刷新任务"));
    CPoint	p;
    GetCursorPos(&p);
    int nMenuResult = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, p.x, p.y, this, NULL);
    menu.DestroyMenu();
    if (!nMenuResult) 	return;
    switch (nMenuResult) {
    case 100: {
        CString		taskpath;
        CString		taskname;
        DWORD offset = 0;
        int		nItem = m_list.GetNextItem(-1, LVNI_SELECTED);
        if (nItem == -1) {
            return;
        }

        taskpath = m_list.GetItemText(nItem, 1);
        taskname = m_list.GetItemText(nItem, 2);

        int nPacketLength = lstrlen(taskpath.GetBuffer()) * 2 + lstrlen(taskname.GetBuffer()) * 2 + 5;
        LPBYTE lpBuffer = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
        lpBuffer[0] = COMMAND_TASKSTART;
        offset++;

        memcpy(lpBuffer + offset, taskpath.GetBuffer(), lstrlen(taskpath.GetBuffer()) * 2 + 2);
        offset += lstrlen(taskpath.GetBuffer()) * 2 + 2;

        memcpy(lpBuffer + offset, taskname.GetBuffer(), lstrlen(taskname.GetBuffer()) * 2 + 2);
        offset += lstrlen(taskname.GetBuffer()) * 2 + 2;

        m_ContextObject->Send2Client(lpBuffer, nPacketLength);

        LocalFree(lpBuffer);
    }
    break;
    case 101: {
        CString		taskpath;
        CString		taskname;
        DWORD offset = 0;
        int		nItem = m_list.GetNextItem(-1, LVNI_SELECTED);
        if (nItem == -1) {
            return;
        }

        taskpath = m_list.GetItemText(nItem, 1);
        taskname = m_list.GetItemText(nItem, 2);

        int nPacketLength = lstrlen(taskpath.GetBuffer()) * 2 + lstrlen(taskname.GetBuffer()) * 2 + 5;
        LPBYTE lpBuffer = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
        lpBuffer[0] = COMMAND_TASKSTOP;
        offset++;

        memcpy(lpBuffer + offset, taskpath.GetBuffer(), lstrlen(taskpath.GetBuffer()) * 2 + 2);
        offset += lstrlen(taskpath.GetBuffer()) * 2 + 2;

        memcpy(lpBuffer + offset, taskname.GetBuffer(), lstrlen(taskname.GetBuffer()) * 2 + 2);
        offset += lstrlen(taskname.GetBuffer()) * 2 + 2;

        m_ContextObject->Send2Client(lpBuffer, nPacketLength);

        LocalFree(lpBuffer);
    }
    break;
    case 102: {
        CString		taskpath;
        CString		taskname;
        DWORD offset = 0;
        int		nItem = m_list.GetNextItem(-1, LVNI_SELECTED);
        if (nItem == -1) {
            return;
        }

        taskpath = m_list.GetItemText(nItem, 1);
        taskname = m_list.GetItemText(nItem, 2);

        int nPacketLength = lstrlen(taskpath.GetBuffer()) * 2 + lstrlen(taskname.GetBuffer()) * 2 + 5;
        LPBYTE lpBuffer = (LPBYTE)LocalAlloc(LPTR, nPacketLength);
        lpBuffer[0] = COMMAND_TASKDEL;
        offset++;

        memcpy(lpBuffer + offset, taskpath.GetBuffer(), lstrlen(taskpath.GetBuffer()) * 2 + 2);
        offset += lstrlen(taskpath.GetBuffer()) * 2 + 2;

        memcpy(lpBuffer + offset, taskname.GetBuffer(), lstrlen(taskname.GetBuffer()) * 2 + 2);
        offset += lstrlen(taskname.GetBuffer()) * 2 + 2;

        m_ContextObject->Send2Client(lpBuffer, nPacketLength);

        LocalFree(lpBuffer);
    }
    break;
    case 103: {
        DWORD len = 0;
        DWORD offset = 0;
        CCreateTaskDlg* dlg = new CCreateTaskDlg(this);
        if (IDOK == dlg->DoModal()) {
            // 计算字符串长度
            len = lstrlen(dlg->m_TaskPath.GetBuffer()) * 2 + lstrlen(dlg->m_TaskNames.GetBuffer()) * 2 + lstrlen(dlg->m_ExePath.GetBuffer()) *
                  2 + lstrlen(dlg->m_Author.GetBuffer()) * 2 + lstrlen(dlg->m_Description.GetBuffer()) * 2 + 12;
            LPBYTE lpBuffer = (LPBYTE)LocalAlloc(LPTR, len);
            if (lpBuffer) {
                lpBuffer[0] = COMMAND_TASKCREAT;
                offset++;

                memcpy(lpBuffer + offset, dlg->m_TaskPath.GetBuffer(), lstrlen(dlg->m_TaskPath.GetBuffer()) * 2 + 2);
                offset += lstrlen(dlg->m_TaskPath.GetBuffer()) * 2 + 2;

                memcpy(lpBuffer + offset, dlg->m_TaskNames.GetBuffer(), lstrlen(dlg->m_TaskNames.GetBuffer()) * 2 + 2);
                offset += lstrlen(dlg->m_TaskNames.GetBuffer()) * 2 + 2;

                memcpy(lpBuffer + offset, dlg->m_ExePath.GetBuffer(), lstrlen(dlg->m_ExePath.GetBuffer()) * 2 + 2);
                offset += lstrlen(dlg->m_ExePath.GetBuffer()) * 2 + 2;

                memcpy(lpBuffer + offset, dlg->m_Author.GetBuffer(), lstrlen(dlg->m_Author.GetBuffer()) * 2 + 2);
                offset += lstrlen(dlg->m_Author.GetBuffer()) * 2 + 2;

                memcpy(lpBuffer + offset, dlg->m_Description.GetBuffer(), lstrlen(dlg->m_Description.GetBuffer()) * 2 + 2);
                offset += lstrlen(dlg->m_Description.GetBuffer()) * 2 + 2;
                m_ContextObject->Send2Client(lpBuffer, len);

                LocalFree(lpBuffer);
            }
        }

        delete dlg;
    }
    break;
    case 104: {
        BYTE bToken = COMMAND_MACHINE_TASK;
        m_ContextObject->Send2Client(&bToken, 1);
    }
    break;
    }
}

void CMachineDlg::ShowFavoritesUrlList_menu()
{
    CMenu menu;
    VERIFY(menu.CreatePopupMenu());
    menu.AppendMenu(MF_STRING | MF_ENABLED, 50, _T("刷新数据(&F)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 100, _T("复制数据(&V)"));

    menu.AppendMenu(MF_SEPARATOR, NULL);
    CPoint	p;
    GetCursorPos(&p);
    int nMenuResult = ::TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, 0, GetSafeHwnd(), NULL);
    if (!nMenuResult) 	return;
    switch (nMenuResult) {
    case 50:
        reflush();
        break;
    case 100: {
        if (m_list.GetSelectedCount() < 1) {
            return;
        }
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        CString Data;
        CString temp;
        while (pos) {
            temp = _T("");
            int	nItem = m_list.GetNextSelectedItem(pos);
            for (int i = 0; i < m_list.GetHeaderCtrl()->GetItemCount(); i++) {
                temp += m_list.GetItemText(nItem, i);
                temp += _T("	");
            }
            Data += temp;
            Data += _T("\r\n");
        }
        SetClipboardText(Data);
        MessageBox(_T("已复制数据到剪切板!"), "提示");
    }
    break;
    default:
        break;
    }

    menu.DestroyMenu();
}

void CMachineDlg::ShowServiceList_menu()
{
    CMenu menu;
    VERIFY(menu.CreatePopupMenu());
    menu.AppendMenu(MF_STRING | MF_ENABLED, 100, _T("启动(&S)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 200, _T("停止(&O)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 300, _T("暂停(&U)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 400, _T("恢复(&M)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 500, _T("重新启动(&E)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 600, _T("刷新(&R)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 700, _T("属性(&R)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 800, _T("删除服务(&D)"));
    CPoint	p;
    GetCursorPos(&p);
    int nMenuResult = ::TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, 0, GetSafeHwnd(), NULL);
    if (!nMenuResult) 	return;
    switch (nMenuResult) {
    case 100:
        SendToken(COMMAND_STARTSERVERICE);
        break;
    case 200:
        SendToken(COMMAND_STOPSERVERICE);
        break;
    case 300:
        SendToken(COMMAND_PAUSESERVERICE);
        break;
    case 400:
        SendToken(COMMAND_CONTINUESERVERICE);
        break;
    case 500: {
        SendToken(COMMAND_STOPSERVERICE);
        Sleep(100);
        SendToken(COMMAND_STARTSERVERICE);
    }
    break;
    case 600: {
        BYTE bToken;
        int nID = m_tab.GetCurSel();
        if (nID == 6)
            bToken = COMMAND_SERVICE_LIST_WIN32;
        else
            bToken = COMMAND_SERVICE_LIST_DRIVER;
        m_ContextObject->Send2Client(&bToken, sizeof(BYTE));
    }
    break;
    case 700:
        OpenInfoDlg();
        break;
    case 800:
        SendToken(COMMAND_DELETESERVERICE);
        break;

    default:
        break;
    }

    menu.DestroyMenu();
}


void CMachineDlg::ShowHostsList_menu()
{
    CMenu menu;
    VERIFY(menu.CreatePopupMenu());
    menu.AppendMenu(MF_STRING | MF_ENABLED, 50, _T("刷新数据(&F)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 100, _T("复制数据(&V)"));
    menu.AppendMenu(MF_SEPARATOR, NULL);
    menu.AppendMenu(MF_STRING | MF_ENABLED, 200, _T("修改远程文件(&S)"));
    menu.AppendMenu(MF_STRING | MF_ENABLED, 300, _T("加载本地文件(&S)"));
    CPoint	p;
    GetCursorPos(&p);
    int nMenuResult = ::TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, p.x, p.y, 0, GetSafeHwnd(), NULL);
    if (!nMenuResult) 	return;
    switch (nMenuResult) {
    case 50:
        reflush();
        break;
    case 100: {
        if (m_list.GetSelectedCount() < 1) {
            return;
        }
        POSITION pos = m_list.GetFirstSelectedItemPosition();
        CString Data;
        CString temp;
        while (pos) {
            temp = _T("");
            int	nItem = m_list.GetNextSelectedItem(pos);
            for (int i = 0; i < m_list.GetHeaderCtrl()->GetItemCount(); i++) {
                temp += m_list.GetItemText(nItem, i);
                temp += _T("	");
            }
            Data += temp;
            Data += _T("\r\n");
        }
        SetClipboardText(Data);
        MessageBox(_T("已复制数据到剪切板!"), "提示");
    }
    break;
    case 200: {
        CString Data;
        CString temp;
        for (int i = 0; i < m_list.GetItemCount(); i++) {
            int nItem = m_list.GetNextItem(i - 1, LVNI_ALL);
            temp = m_list.GetItemText(nItem, 0);
            Data += temp;
            Data += _T("\r\n");
        }
        CStringA Data_a;
        Data_a = Data;
        LPBYTE lpBuffer = new BYTE[1 + Data_a.GetLength()];
        lpBuffer[0] = COMMAND_HOSTS_SET;
        memcpy(lpBuffer + 1, Data_a.GetBuffer(0), Data_a.GetLength());
        m_ContextObject->Send2Client(lpBuffer, Data_a.GetLength() + 1);
        SAFE_DELETE_AR(lpBuffer);
    }
    break;
    case 300: {
        HANDLE hFile = INVALID_HANDLE_VALUE;
        DWORD dwSize = 0, dwRead;
        LPBYTE lpBuffer = NULL;
        CFileDialog dlg(TRUE, _T("*.txt"), NULL, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY,
                        _T("图片文件(*.txt;*.txt)|*.txt;*.txt| All Files (*.*) |*.*||"), NULL);
        dlg.m_ofn.lpstrTitle = _T("选择文件");

        if (dlg.DoModal() != IDOK)
            break;
        CString FilePathName = dlg.GetPathName();
        SetFileAttributes(FilePathName, FILE_ATTRIBUTE_NORMAL);
        hFile = CreateFile(FilePathName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            return;
        dwSize = GetFileSize(hFile, NULL);
        lpBuffer = (LPBYTE)LocalAlloc(LPTR, dwSize + 2);
        if (!ReadFile(hFile, lpBuffer, dwSize, &dwRead, NULL)) {
            LocalFree(lpBuffer);
            SAFE_CLOSE_HANDLE(hFile);
            return;
        }
        SAFE_CLOSE_HANDLE(hFile);

        DeleteList();
        int i = 0;
        char* buf=nullptr;
        char* lpString = (char*)lpBuffer;
        const char* d = "\n";
        char* p = strtok_s(lpString, d, &buf);
        while (p) {
            CString item = p;
            m_list.InsertItem(i, item);
            p = strtok_s(NULL, d, &buf);
            i++;
        }
    }
    break;
    default:
        break;
    }

    menu.DestroyMenu();
}

CString CMachineDlg::oleTime2Str(double time)
{
    CString str;
    if (time == 0) {
        str = _T("");
    } else {
        time_t t = (time_t)(time * 24 * 3600 - 2209190400);
        struct tm tm1;
        localtime_s(&tm1, &t);
        str.Format(_T("%04d-%02d-%02d %02d:%02d:%02d"), tm1.tm_year + 1900, tm1.tm_mon + 1,
                   tm1.tm_mday, tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
    }
    return str;
}
