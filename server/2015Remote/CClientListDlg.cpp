// CClientListDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "afxdialogex.h"
#include "CClientListDlg.h"
#include "2015Remote.h"


// CClientListDlg 对话框

typedef struct {
    LPCTSTR Name;
    int     Width;
    float   Percent;
} ColumnInfo;

static ColumnInfo g_ColumnInfos[] = {
    { _T("序号"),       40,  0.0f },
    { _T("ID"),         130, 0.0f },
    { _T("备注"),       60,  0.0f },
    { _T("计算机名称"), 105, 0.0f },
    { _T("位置"),       115, 0.0f },
    { _T("IP"),         95,  0.0f },
    { _T("系统"),       100, 0.0f },
    { _T("安装时间"),   115, 0.0f },
    { _T("最后登录"),   115, 0.0f },
    { _T("程序路径"),   150, 0.0f },
    { _T("关注"),       40,  0.0f },
    { _T("授权"),       40,  0.0f },
};

static const int g_nColumnCount = _countof(g_ColumnInfos);

// 列索引枚举（与 g_ColumnInfos 顺序一致）
enum ColumnIndex {
    COL_NO = 0,         // 序号
    COL_ID,             // ID
    COL_NOTE,           // 备注
    COL_COMPUTER_NAME,  // 计算机名称
    COL_LOCATION,       // 位置
    COL_IP,             // IP
    COL_OS,             // 系统
    COL_INSTALL_TIME,   // 安装时间
    COL_LAST_LOGIN,     // 最后登录
    COL_PROGRAM_PATH,   // 程序路径
    COL_LEVEL,          // 关注
    COL_AUTH,           // 授权
};

// ========== 分组字段配置 ==========
// 可用的分组字段枚举
enum GroupField {
    GF_IP,
    GF_ComputerName,
    GF_OsName,
    GF_Location,
    GF_ProgramPath,
};

// 分组字段配置：字段枚举 + 对应的列索引
struct GroupFieldConfig {
    GroupField Field;
    int ColumnIndex;
};

// ★★★ 修改这里即可改变分组方式 ★★★
static GroupFieldConfig g_GroupFieldConfigs[] = {
    { GF_IP,           COL_IP },             // 按 IP 分组
    { GF_ComputerName, COL_COMPUTER_NAME },  // 按 计算机名称 分组
    // { GF_OsName,    COL_OS },             // 取消注释：增加按操作系统分组
    // { GF_Location,  COL_LOCATION },       // 取消注释：增加按位置分组
};

static const int g_nGroupFieldCount = _countof(g_GroupFieldConfigs);

// 根据字段枚举获取 ClientValue 中的值
static CString GetFieldValue(const ClientValue& val, GroupField field)
{
    switch (field) {
    case GF_IP:
        return CString(val.IP);
    case GF_ComputerName:
        return CString(val.ComputerName);
    case GF_OsName:
        return CString(val.OsName);
    case GF_Location:
        return CString(val.Location);
    case GF_ProgramPath:
        return CString(val.ProgramPath);
    default:
        return _T("");
    }
}

// 比较两个客户端的指定列，返回 <0, 0, >0
static int CompareClientByColumn(const std::pair<ClientKey, ClientValue>& a,
                                 const std::pair<ClientKey, ClientValue>& b,
                                 int nColumn)
{
    switch (nColumn) {
    case COL_ID:
        return (a.first < b.first) ? -1 : ((a.first > b.first) ? 1 : 0);
    case COL_NOTE:
        return strcmp(a.second.Note, b.second.Note);
    case COL_COMPUTER_NAME:
        return strcmp(a.second.ComputerName, b.second.ComputerName);
    case COL_LOCATION:
        return strcmp(a.second.Location, b.second.Location);
    case COL_IP:
        return strcmp(a.second.IP, b.second.IP);
    case COL_OS:
        return strcmp(a.second.OsName, b.second.OsName);
    case COL_INSTALL_TIME:
        return strcmp(a.second.InstallTime, b.second.InstallTime);
    case COL_LAST_LOGIN:
        return strcmp(a.second.LastLoginTime, b.second.LastLoginTime);
    case COL_PROGRAM_PATH:
        return strcmp(a.second.ProgramPath, b.second.ProgramPath);
    case COL_LEVEL:
        return a.second.Level - b.second.Level;
    case COL_AUTH:
        return a.second.Authorized - b.second.Authorized;
    default:
        return 0;
    }
}

IMPLEMENT_DYNAMIC(CClientListDlg, CDialogEx)

CClientListDlg::CClientListDlg(_ClientList* clients, CMy2015RemoteDlg* pParent)
    : g_ClientList(clients), g_pParent(pParent), CDialogLangEx(IDD_DIALOG_CLIENTLIST, pParent)
    , m_nSortColumn(-1)
    , m_bSortAscending(TRUE)
    , m_nTipItem(-1)
    , m_nTipSubItem(-1)
{
}

CClientListDlg::~CClientListDlg()
{
}

void CClientListDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CLIENT_LIST, m_ClientList);
}


BEGIN_MESSAGE_MAP(CClientListDlg, CDialogEx)
    ON_WM_SIZE()
    ON_NOTIFY(LVN_COLUMNCLICK, IDC_CLIENT_LIST, &CClientListDlg::OnColumnClick)
    ON_NOTIFY(NM_CLICK, IDC_CLIENT_LIST, &CClientListDlg::OnListClick)
END_MESSAGE_MAP()


// CClientListDlg 消息处理程序

BOOL CClientListDlg::OnInitDialog()
{
    __super::OnInitDialog();

    HICON hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_MACHINE));
    SetIcon(hIcon, FALSE);

    // 设置扩展样式
    m_ClientList.SetExtendedStyle(
        LVS_EX_FULLROWSELECT |  // 整行选中
        LVS_EX_GRIDLINES       // 显示网格线
    );

    // 初始化ToolTip
    m_ToolTip.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX);
    m_ToolTip.AddTool(&m_ClientList);
    m_ToolTip.SetMaxTipWidth(500);
    m_ToolTip.SetDelayTime(TTDT_AUTOPOP, 10000);
    m_ToolTip.Activate(TRUE);

    // 设置配置键名
    m_ClientList.SetConfigKey(_T("ClientList"));

    // 添加列（第一列序号不允许隐藏）
    for (int i = 0; i < g_nColumnCount; i++) {
        BOOL bCanHide = (i != COL_NO);  // 序号列不允许隐藏
        m_ClientList.AddColumn(i, _L(g_ColumnInfos[i].Name), g_ColumnInfos[i].Width, LVCFMT_LEFT, bCanHide);
    }

    // 初始化列（计算百分比、加载配置、应用列宽）
    m_ClientList.InitColumns();

    // 首次加载数据
    RefreshClientList();

    return TRUE;
}

void CClientListDlg::RefreshClientList()
{
    m_clients = g_ClientList->GetAll();  // 保存到成员变量
    BuildGroups();  // 构建分组

    m_ClientList.SetRedraw(FALSE);
    DisplayClients();
    m_ClientList.SetRedraw(TRUE);
    m_ClientList.Invalidate();
}

void CClientListDlg::BuildGroups()
{
    // 保留已有分组的展开状态
    std::map<GroupKey, BOOL> expandedStates;
    for (const auto& pair : m_groups) {
        expandedStates[pair.first] = pair.second.bExpanded;
    }

    m_groups.clear();

    // 根据配置的字段进行分组
    for (const auto& client : m_clients) {
        GroupKey key;
        for (int i = 0; i < g_nGroupFieldCount; i++) {
            key.Values.push_back(GetFieldValue(client.second, g_GroupFieldConfigs[i].Field));
        }

        if (m_groups.find(key) == m_groups.end()) {
            GroupInfo info;
            info.GroupId = 0;  // 显示时重新编号
            // 恢复展开状态，新分组默认收起
            auto it = expandedStates.find(key);
            info.bExpanded = (it != expandedStates.end()) ? it->second : FALSE;
            m_groups[key] = info;
        }
        m_groups[key].Clients.push_back(client);
    }
}

void CClientListDlg::DisplayClients()
{
    m_ClientList.DeleteAllItems();

    // 创建分组指针列表用于排序
    std::vector<std::pair<const GroupKey, GroupInfo>*> sortedGroups;
    for (auto& pair : m_groups) {
        sortedGroups.push_back(&pair);
    }

    // 如果有排序列，按第一个设备的该列值排序
    if (m_nSortColumn >= 0) {
        int sortCol = m_nSortColumn;
        BOOL ascending = m_bSortAscending;
        std::sort(sortedGroups.begin(), sortedGroups.end(),
                  [sortCol, ascending](const std::pair<const GroupKey, GroupInfo>* a,
        const std::pair<const GroupKey, GroupInfo>* b) {
            // 取每个分组的第一个设备进行比较
            const auto& clientA = a->second.Clients[0];
            const auto& clientB = b->second.Clients[0];
            int result = CompareClientByColumn(clientA, clientB, sortCol);
            return ascending ? (result < 0) : (result > 0);
        });
    }

    int nRow = 0;
    int nGroupIndex = 0;
    for (auto* pPair : sortedGroups) {
        const GroupKey& groupKey = pPair->first;
        GroupInfo& groupInfo = pPair->second;
        nGroupIndex++;
        groupInfo.GroupId = nGroupIndex;  // 按显示顺序重新编号

        int nItem;
        size_t clientCount = groupInfo.Clients.size();

        // 只有一个设备时，直接显示设备详情
        if (clientCount == 1) {
            const ClientKey& key = groupInfo.Clients[0].first;
            const ClientValue& val = groupInfo.Clients[0].second;

            CString strNo;
            strNo.FormatL(_T("%d"), groupInfo.GroupId);

            CString strID;
            strID.FormatL(_T("%llu"), key);

            CString strLevel;
            strLevel.FormatL(_T("%d"), val.Level);

            CString strAuth = val.Authorized ? _T("Y") : _T("N");

            nItem = m_ClientList.InsertItem(nRow, strNo);
            m_ClientList.SetItemText(nItem, COL_ID, strID);
            m_ClientList.SetItemText(nItem, COL_NOTE, val.Note);
            m_ClientList.SetItemText(nItem, COL_COMPUTER_NAME, CString(val.ComputerName));
            m_ClientList.SetItemText(nItem, COL_LOCATION, val.Location);
            m_ClientList.SetItemText(nItem, COL_IP, val.IP);
            m_ClientList.SetItemText(nItem, COL_OS, val.OsName);
            m_ClientList.SetItemText(nItem, COL_INSTALL_TIME, val.InstallTime);
            m_ClientList.SetItemText(nItem, COL_LAST_LOGIN, val.LastLoginTime);
            m_ClientList.SetItemText(nItem, COL_PROGRAM_PATH, CString(val.ProgramPath));
            m_ClientList.SetItemText(nItem, COL_LEVEL, strLevel);
            m_ClientList.SetItemText(nItem, COL_AUTH, strAuth);
            m_ClientList.SetItemData(nItem, (DWORD_PTR)key);
            nRow++;
        } else {
            // 多个设备时，显示可展开的分组行
            CString strNo;
            strNo.FormatL(_T("%d"), groupInfo.GroupId);

            CString strCount;
            strCount.FormatL(_T("%s (%d\u53f0\u8bbe\u5907)"), groupInfo.bExpanded ? _T("-") : _T("+"), (int)clientCount);

            nItem = m_ClientList.InsertItem(nRow, strNo);
            m_ClientList.SetItemText(nItem, COL_ID, strCount);

            // 清空所有列
            for (int col = COL_NOTE; col < g_nColumnCount; col++) {
                m_ClientList.SetItemText(nItem, col, _T(""));
            }
            // 根据配置填充分组字段到对应列
            for (int i = 0; i < g_nGroupFieldCount; i++) {
                m_ClientList.SetItemText(nItem, g_GroupFieldConfigs[i].ColumnIndex, groupKey.Values[i]);
            }

            // 分组行的 ItemData 使用高位标记: 0x8000000000000000 | groupId
            m_ClientList.SetItemData(nItem, 0x8000000000000000ULL | groupInfo.GroupId);
            nRow++;

            // 如果展开，显示组内设备
            if (groupInfo.bExpanded) {
                for (const auto& client : groupInfo.Clients) {
                    const ClientKey& key = client.first;
                    const ClientValue& val = client.second;

                    CString strSubNo, strID;
                    strID.FormatL(_T("%llu"), key);

                    CString strLevel;
                    strLevel.FormatL(_T("%d"), val.Level);

                    CString strAuth = val.Authorized ? _T("Y") : _T("N");

                    nItem = m_ClientList.InsertItem(nRow, strSubNo);
                    m_ClientList.SetItemText(nItem, COL_ID, strID);
                    m_ClientList.SetItemText(nItem, COL_NOTE, val.Note);
                    m_ClientList.SetItemText(nItem, COL_COMPUTER_NAME, CString(val.ComputerName));
                    m_ClientList.SetItemText(nItem, COL_LOCATION, val.Location);
                    m_ClientList.SetItemText(nItem, COL_IP, val.IP);
                    m_ClientList.SetItemText(nItem, COL_OS, val.OsName);
                    m_ClientList.SetItemText(nItem, COL_INSTALL_TIME, val.InstallTime);
                    m_ClientList.SetItemText(nItem, COL_LAST_LOGIN, val.LastLoginTime);
                    m_ClientList.SetItemText(nItem, COL_PROGRAM_PATH, CString(val.ProgramPath));
                    m_ClientList.SetItemText(nItem, COL_LEVEL, strLevel);
                    m_ClientList.SetItemText(nItem, COL_AUTH, strAuth);
                    m_ClientList.SetItemData(nItem, (DWORD_PTR)key);
                    nRow++;
                }
            }
        }
    }
}

void CClientListDlg::OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    int nColumn = pNMLV->iSubItem;

    // 序号列不排序
    if (nColumn == COL_NO) {
        *pResult = 0;
        return;
    }

    // 点击同一列切换排序方向
    if (nColumn == m_nSortColumn) {
        m_bSortAscending = !m_bSortAscending;
    } else {
        m_nSortColumn = nColumn;
        m_bSortAscending = TRUE;
    }

    SortByColumn(nColumn, m_bSortAscending);

    *pResult = 0;
}

void CClientListDlg::SortByColumn(int /*nColumn*/, BOOL /*bAscending*/)
{
    // 排序在 DisplayClients 中进行（使用成员变量 m_nSortColumn, m_bSortAscending）
    m_ClientList.SetRedraw(FALSE);
    DisplayClients();
    m_ClientList.SetRedraw(TRUE);
    m_ClientList.Invalidate();
}

void CClientListDlg::AdjustColumnWidths()
{
    m_ClientList.AdjustColumnWidths();
}

void CClientListDlg::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    if (m_ClientList.GetSafeHwnd() == NULL) {
        return;  // 控件还没创建
    }

    // 留点边距
    int margin = 10;

    // 列表控件填满整个对话框（留边距）
    m_ClientList.MoveWindow(margin, margin, cx - margin * 2, cy - margin * 2);

    AdjustColumnWidths();
}

void CClientListDlg::OnListClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pNMIA = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
    int nItem = pNMIA->iItem;

    if (nItem < 0) {
        *pResult = 0;
        return;
    }

    DWORD_PTR itemData = m_ClientList.GetItemData(nItem);

    // 检查是否为分组行 (高位被设置)
    if (itemData & 0x8000000000000000ULL) {
        int groupId = (int)(itemData & 0x7FFFFFFFFFFFFFFFULL);

        // 查找对应的分组并切换展开状态
        for (auto& pair : m_groups) {
            if (pair.second.GroupId == groupId) {
                pair.second.bExpanded = !pair.second.bExpanded;
                break;
            }
        }

        // 刷新显示
        m_ClientList.SetRedraw(FALSE);
        DisplayClients();
        m_ClientList.SetRedraw(TRUE);

        // 恢复选中状态：找到刷新后的分组行并选中
        for (int i = 0; i < m_ClientList.GetItemCount(); i++) {
            DWORD_PTR data = m_ClientList.GetItemData(i);
            if ((data & 0x8000000000000000ULL) &&
                (int)(data & 0x7FFFFFFFFFFFFFFFULL) == groupId) {
                m_ClientList.SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                m_ClientList.EnsureVisible(i, FALSE);
                break;
            }
        }

        m_ClientList.Invalidate();
    }

    *pResult = 0;
}

BOOL CClientListDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_MOUSEMOVE && pMsg->hwnd == m_ClientList.GetSafeHwnd()) {
        m_ToolTip.RelayEvent(pMsg);

        CPoint pt(pMsg->lParam);
        LVHITTESTINFO hitInfo = {};
        hitInfo.pt = pt;
        m_ClientList.SubItemHitTest(&hitInfo);

        int nItem = hitInfo.iItem;
        int nSubItem = hitInfo.iSubItem;

        if (nItem != m_nTipItem || nSubItem != m_nTipSubItem) {
            m_nTipItem = nItem;
            m_nTipSubItem = nSubItem;

            if (nItem >= 0) {
                CString strText = m_ClientList.GetItemText(nItem, nSubItem);

                // 判断文本是否被截断
                CClientDC dc(&m_ClientList);
                CFont* pOldFont = dc.SelectObject(m_ClientList.GetFont());
                CSize textSize = dc.GetTextExtent(strText);
                dc.SelectObject(pOldFont);

                int colWidth = m_ClientList.GetColumnWidth(nSubItem);
                if (textSize.cx + 12 > colWidth && !strText.IsEmpty()) {
                    m_ToolTip.UpdateTipText(strText, &m_ClientList);
                } else {
                    m_ToolTip.UpdateTipText(_T(""), &m_ClientList);
                }
            } else {
                m_ToolTip.UpdateTipText(_T(""), &m_ClientList);
            }
        }
    }

    return __super::PreTranslateMessage(pMsg);
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

    __super::PostNcDestroy();

    delete this;
}

void CClientListDlg::OnOK()
{
}
