#include "stdafx.h"
#include "CListCtrlEx.h"
#include "2015Remote.h"

// CHeaderCtrlEx 实现
BEGIN_MESSAGE_MAP(CHeaderCtrlEx, CHeaderCtrl)
    ON_NOTIFY_REFLECT(HDN_BEGINTRACKA, &CHeaderCtrlEx::OnBeginTrack)
    ON_NOTIFY_REFLECT(HDN_BEGINTRACKW, &CHeaderCtrlEx::OnBeginTrack)
END_MESSAGE_MAP()

void CHeaderCtrlEx::OnBeginTrack(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMHEADER pNMHeader = reinterpret_cast<LPNMHEADER>(pNMHDR);
    int nCol = pNMHeader->iItem;

    if (m_pListCtrl && nCol >= 0 && nCol < (int)m_pListCtrl->m_Columns.size()) {
        if (!m_pListCtrl->m_Columns[nCol].Visible) {
            *pResult = TRUE;  // 阻止拖拽
            return;
        }
    }
    *pResult = FALSE;
}

// CListCtrlEx 实现
IMPLEMENT_DYNAMIC(CListCtrlEx, CListCtrl)

CListCtrlEx::CListCtrlEx()
{
}

CListCtrlEx::~CListCtrlEx()
{
}

BEGIN_MESSAGE_MAP(CListCtrlEx, CListCtrl)
    ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

void CListCtrlEx::SetConfigKey(const CString& strKey)
{
    m_strConfigKey = strKey;
}

int CListCtrlEx::AddColumn(int nCol, LPCTSTR lpszColumnHeading, int nWidth, int nFormat, BOOL bCanHide)
{
    // 添加到列表控件
    int nResult = InsertColumnL(nCol, lpszColumnHeading, nFormat, nWidth);

    if (nResult != -1) {
        // 保存列信息
        ColumnInfoEx info;
        info.Name = lpszColumnHeading;
        info.Width = nWidth;
        info.Percent = 0.0f;  // 稍后在 InitColumns 中计算
        info.Visible = TRUE;
        info.CanHide = bCanHide;

        // 确保 vector 大小足够
        if (nCol >= (int)m_Columns.size()) {
            m_Columns.resize(nCol + 1);
        }
        m_Columns[nCol] = info;
    }

    return nResult;
}

void CListCtrlEx::InitColumns()
{
    if (m_Columns.empty()) {
        return;
    }

    // 子类化 Header 控件
    SubclassHeader();

    // 计算总宽度和百分比
    int totalWidth = 0;
    for (const auto& col : m_Columns) {
        totalWidth += col.Width;
    }

    if (totalWidth > 0) {
        for (auto& col : m_Columns) {
            col.Percent = (float)col.Width / totalWidth;
        }
    }

    // 从配置加载列可见性
    LoadColumnVisibility();

    // 应用列宽
    AdjustColumnWidths();
}

void CListCtrlEx::SubclassHeader()
{
    CHeaderCtrl* pHeader = GetHeaderCtrl();
    if (pHeader && !m_HeaderCtrl.GetSafeHwnd()) {
        m_HeaderCtrl.SubclassWindow(pHeader->GetSafeHwnd());
        m_HeaderCtrl.m_pListCtrl = this;
    }
}

void CListCtrlEx::AdjustColumnWidths()
{
    if (m_Columns.empty() || GetSafeHwnd() == NULL) {
        return;
    }

    CRect rect;
    GetClientRect(&rect);
    int totalWidth = rect.Width() - 20;  // 减去滚动条宽度

    // 计算可见列的总百分比
    float visiblePercent = 0.0f;
    for (const auto& col : m_Columns) {
        if (col.Visible) {
            visiblePercent += col.Percent;
        }
    }

    // 按比例分配宽度给可见列
    for (int i = 0; i < (int)m_Columns.size(); i++) {
        if (m_Columns[i].Visible) {
            int width = (visiblePercent > 0) ? (int)(totalWidth * m_Columns[i].Percent / visiblePercent) : 0;
            SetColumnWidth(i, width);
        } else {
            SetColumnWidth(i, 0);  // 隐藏列
        }
    }
}

BOOL CListCtrlEx::IsColumnVisible(int nCol) const
{
    if (nCol >= 0 && nCol < (int)m_Columns.size()) {
        return m_Columns[nCol].Visible;
    }
    return TRUE;
}

void CListCtrlEx::SetColumnVisible(int nCol, BOOL bVisible)
{
    if (nCol >= 0 && nCol < (int)m_Columns.size()) {
        if (m_Columns[nCol].CanHide || bVisible) {  // 不允许隐藏的列只能设为可见
            m_Columns[nCol].Visible = bVisible;
            AdjustColumnWidths();
        }
    }
}

void CListCtrlEx::OnContextMenu(CWnd* pWnd, CPoint pt)
{
    // 检查是否点击在表头区域
    CHeaderCtrl* pHeader = GetHeaderCtrl();
    if (pHeader) {
        CRect headerRect;
        pHeader->GetWindowRect(&headerRect);
        if (headerRect.PtInRect(pt)) {
            ShowColumnContextMenu(pt);
            return;
        }
    }

    // 非表头区域，调用父类处理
    CListCtrl::OnContextMenu(pWnd, pt);
}

void CListCtrlEx::ShowColumnContextMenu(CPoint pt)
{
    CMenu menu;
    menu.CreatePopupMenu();

    // 添加所有列到菜单（跳过空标题的列）
    for (int i = 0; i < (int)m_Columns.size(); i++) {
        // 跳过空标题的列
        if (m_Columns[i].Name.IsEmpty()) {
            continue;
        }

        UINT flags = MF_STRING;
        if (m_Columns[i].Visible) {
            flags |= MF_CHECKED;
        }
        // 不允许隐藏的列显示为灰色
        if (!m_Columns[i].CanHide) {
            flags |= MF_GRAYED;
        }
        menu.AppendMenuL(flags, 1000 + i, m_Columns[i].Name);
    }

    // 显示菜单并获取选择
    int nCmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
    if (nCmd >= 1000 && nCmd < 1000 + (int)m_Columns.size()) {
        ToggleColumnVisibility(nCmd - 1000);
    }
}

void CListCtrlEx::ToggleColumnVisibility(int nColumn)
{
    if (nColumn < 0 || nColumn >= (int)m_Columns.size()) {
        return;
    }

    // 不允许隐藏的列不处理
    if (!m_Columns[nColumn].CanHide) {
        return;
    }

    // 切换可见性
    m_Columns[nColumn].Visible = !m_Columns[nColumn].Visible;

    // 保存到配置
    SaveColumnVisibility();

    // 重新调整列宽
    AdjustColumnWidths();
}

void CListCtrlEx::LoadColumnVisibility()
{
    if (m_strConfigKey.IsEmpty()) {
        return;  // 没有设置配置键，不加载
    }

    // 配置结构：list\{ConfigKey}，如 list\ClientList
    // 格式：逗号分隔的隐藏列名，如 "备注,程序路径"
    CT2A configKeyA(m_strConfigKey);
    std::string strHidden = THIS_CFG.GetStr("list", std::string(configKeyA), "");
    if (strHidden.empty()) {
        return;  // 使用默认值（全部显示）
    }

    // 解析隐藏的列名
    std::vector<std::string> hiddenNames = StringToVector(strHidden, ',');
    for (const auto& name : hiddenNames) {
        // 查找列名对应的索引
        for (int i = 0; i < (int)m_Columns.size(); i++) {
            CT2A colNameA(m_Columns[i].Name);
            if (name == std::string(colNameA)) {
                if (m_Columns[i].CanHide) {
                    m_Columns[i].Visible = FALSE;
                }
                break;
            }
        }
    }
}

void CListCtrlEx::SaveColumnVisibility()
{
    if (m_strConfigKey.IsEmpty()) {
        return;  // 没有设置配置键，不保存
    }

    // 只保存隐藏的列名
    std::string strHidden;
    for (int i = 0; i < (int)m_Columns.size(); i++) {
        if (!m_Columns[i].Visible) {
            if (!strHidden.empty()) strHidden += ",";
            CT2A colNameA(m_Columns[i].Name);
            strHidden += std::string(colNameA);
        }
    }

    // 配置结构：list\{ConfigKey}，如 list\ClientList
    CT2A configKeyA(m_strConfigKey);
    THIS_CFG.SetStr("list", std::string(configKeyA), strHidden);
}
