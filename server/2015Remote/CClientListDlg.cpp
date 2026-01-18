// CClientListDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "afxdialogex.h"
#include "CClientListDlg.h"


// CClientListDlg 对话框

IMPLEMENT_DYNAMIC(CClientListDlg, CDialogEx)

CClientListDlg::CClientListDlg(_ClientList* clients, CMy2015RemoteDlg* pParent)
    : g_ClientList(clients), g_pParent(pParent), CDialogEx(IDD_DIALOG_CLIENTLIST, pParent)
    , m_nSortColumn(-1)
    , m_bSortAscending(TRUE)
{
}

CClientListDlg::~CClientListDlg()
{
}

void CClientListDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CLIENT_LIST, m_ClientList);
}


BEGIN_MESSAGE_MAP(CClientListDlg, CDialogEx)
    ON_WM_SIZE()
    ON_NOTIFY(LVN_COLUMNCLICK, IDC_CLIENT_LIST, &CClientListDlg::OnColumnClick)
END_MESSAGE_MAP()


// CClientListDlg 消息处理程序

BOOL CClientListDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    HICON hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_MACHINE));
    SetIcon(hIcon, FALSE);

    // 设置扩展样式
    m_ClientList.SetExtendedStyle(
        LVS_EX_FULLROWSELECT |  // 整行选中
        LVS_EX_GRIDLINES       // 显示网格线
    );

    // 添加列
    m_ClientList.InsertColumn(0, _T("序号"), LVCFMT_LEFT, 50);
    m_ClientList.InsertColumn(1, _T("ID"), LVCFMT_LEFT, 120);
    m_ClientList.InsertColumn(2, _T("备注"), LVCFMT_LEFT, 80);
    m_ClientList.InsertColumn(3, _T("位置"), LVCFMT_LEFT, 100);
    m_ClientList.InsertColumn(4, _T("IP"), LVCFMT_LEFT, 120);
    m_ClientList.InsertColumn(5, _T("系统"), LVCFMT_LEFT, 120);
    m_ClientList.InsertColumn(6, _T("安装时间"), LVCFMT_LEFT, 130);
    m_ClientList.InsertColumn(7, _T("最后登录"), LVCFMT_LEFT, 130);
    m_ClientList.InsertColumn(8, _T("关注级别"), LVCFMT_LEFT, 70);
    m_ClientList.InsertColumn(9, _T("已授权"), LVCFMT_LEFT, 60);

    // 首次加载数据
    AdjustColumnWidths();
    RefreshClientList();

    return TRUE;
}

void CClientListDlg::RefreshClientList()
{
    m_clients = g_ClientList->GetAll();  // 保存到成员变量

    // 如果之前有排序，保持排序
    if (m_nSortColumn >= 0) {
        SortByColumn(m_nSortColumn, m_bSortAscending);
    }
    else {
        m_ClientList.SetRedraw(FALSE);
        DisplayClients();
        m_ClientList.SetRedraw(TRUE);
        m_ClientList.Invalidate();
    }
}

void CClientListDlg::DisplayClients()
{
    m_ClientList.DeleteAllItems();

    int i = 0;
    for (const auto& pair : m_clients) {
        const ClientKey& key = pair.first;
        const ClientValue& val = pair.second;

        CString strNo;
        strNo.Format(_T("%d"), i + 1);  // 序号从1开始

        CString strID;
        strID.Format(_T("%llu"), key);

        CString strLevel;
        strLevel.Format(_T("%d"), val.Level);

        CString strAuth = val.Authorized ? _T("Y") : _T("N");

        int nItem = m_ClientList.InsertItem(i, strNo);  // 第一列是序号
        m_ClientList.SetItemText(nItem, 1, strID);
        m_ClientList.SetItemText(nItem, 2, val.Note);
        m_ClientList.SetItemText(nItem, 3, val.Location);
        m_ClientList.SetItemText(nItem, 4, val.IP);
        m_ClientList.SetItemText(nItem, 5, val.OsName);
        m_ClientList.SetItemText(nItem, 6, val.InstallTime);
        m_ClientList.SetItemText(nItem, 7, val.LastLoginTime);
        m_ClientList.SetItemText(nItem, 8, strLevel);
        m_ClientList.SetItemText(nItem, 9, strAuth);
        m_ClientList.SetItemData(nItem, (DWORD_PTR)key);
        i++;
    }
}

void CClientListDlg::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    int nColumn = pNMLV->iSubItem;

    // 序号列不排序
    if (nColumn == 0) {
        *pResult = 0;
        return;
    }

    // 点击同一列切换排序方向
    if (nColumn == m_nSortColumn) {
        m_bSortAscending = !m_bSortAscending;
    }
    else {
        m_nSortColumn = nColumn;
        m_bSortAscending = TRUE;
    }

    SortByColumn(nColumn, m_bSortAscending);

    *pResult = 0;
}

void CClientListDlg::SortByColumn(int nColumn, BOOL bAscending)
{
    std::sort(m_clients.begin(), m_clients.end(),
        [nColumn, bAscending](const std::pair<ClientKey, ClientValue>& a,
            const std::pair<ClientKey, ClientValue>& b) {
                int result = 0;

                switch (nColumn) {
                case 1:  // ID
                    result = (a.first < b.first) ? -1 : ((a.first > b.first) ? 1 : 0);
                    break;
                case 2:  // 备注
                    result = strcmp(a.second.Note, b.second.Note);
                    break;
                case 3:  // 位置
                    result = strcmp(a.second.Location, b.second.Location);
                    break;
                case 4:  // IP
                    result = strcmp(a.second.IP, b.second.IP);
                    break;
                case 5:  // 系统
                    result = strcmp(a.second.OsName, b.second.OsName);
                    break;
                case 6:  // 安装时间
                    result = strcmp(a.second.InstallTime, b.second.InstallTime);
                    break;
                case 7:  // 最后登录
                    result = strcmp(a.second.LastLoginTime, b.second.LastLoginTime);
                    break;
                case 8:  // 关注级别
                    result = a.second.Level - b.second.Level;
                    break;
                case 9:  // 已授权
                    result = a.second.Authorized - b.second.Authorized;
                    break;
                default:
                    return false;
                }

                return bAscending ? (result < 0) : (result > 0);
        });

    m_ClientList.SetRedraw(FALSE);
    DisplayClients();
    m_ClientList.SetRedraw(TRUE);
    m_ClientList.Invalidate();
}

void CClientListDlg::AdjustColumnWidths()
{
    CRect rect;
    m_ClientList.GetClientRect(&rect);
    int totalWidth = rect.Width() - 20;

    m_ClientList.SetColumnWidth(0, totalWidth * 5 / 100);   // 序号
    m_ClientList.SetColumnWidth(1, totalWidth * 12 / 100);  // ID
    m_ClientList.SetColumnWidth(2, totalWidth * 10 / 100);  // 备注
    m_ClientList.SetColumnWidth(3, totalWidth * 11 / 100);  // 位置
    m_ClientList.SetColumnWidth(4, totalWidth * 11 / 100);  // IP
    m_ClientList.SetColumnWidth(5, totalWidth * 11 / 100);  // 系统
    m_ClientList.SetColumnWidth(6, totalWidth * 13 / 100);  // 安装时间
    m_ClientList.SetColumnWidth(7, totalWidth * 13 / 100);  // 最后登录
    m_ClientList.SetColumnWidth(8, totalWidth * 7 / 100);   // 关注级别
    m_ClientList.SetColumnWidth(9, totalWidth * 7 / 100);   // 已授权
}

void CClientListDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    if (m_ClientList.GetSafeHwnd() == NULL) {
        return;  // 控件还没创建
    }

    // 留点边距
    int margin = 10;

    // 列表控件填满整个对话框（留边距）
    m_ClientList.MoveWindow(margin, margin, cx - margin * 2, cy - margin * 2);

    AdjustColumnWidths();
}

void CClientListDlg::OnCancel()
{
    DestroyWindow();
}

void CClientListDlg::PostNcDestroy()
{
    if (g_pParent) {
        g_pParent->m_pClientListDlg = nullptr;
    }

    CDialogEx::PostNcDestroy();

    delete this;
}

void CClientListDlg::OnOK()
{
}
