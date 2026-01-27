#pragma once

#include <vector>
#include <string>

// 列信息结构
struct ColumnInfoEx {
    CString Name;       // 列名
    int     Width;      // 初始宽度
    float   Percent;    // 占比
    BOOL    Visible;    // 是否可见
    BOOL    CanHide;    // 是否允许隐藏（如序号列不允许）
};

class CListCtrlEx;

// 自定义 Header 控件，用于阻止隐藏列被拖拽
class CHeaderCtrlEx : public CHeaderCtrl
{
public:
    CListCtrlEx* m_pListCtrl = nullptr;

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnBeginTrack(NMHDR* pNMHDR, LRESULT* pResult);
};

// CListCtrlEx - 支持列显示/隐藏功能的列表控件
class CListCtrlEx : public CListCtrl
{
    DECLARE_DYNAMIC(CListCtrlEx)
    friend class CHeaderCtrlEx;  // 允许 CHeaderCtrlEx 访问 protected 成员

public:
    CListCtrlEx();
    virtual ~CListCtrlEx();

    // 设置配置键名（用于区分不同列表的配置，如 "ClientList", "FileList"）
    void SetConfigKey(const CString& strKey);

    // 添加列（替代 InsertColumnL）
    // nCol: 列索引
    // lpszColumnHeading: 列标题
    // nFormat: 对齐方式，默认左对齐
    // nWidth: 列宽
    // bCanHide: 是否允许隐藏，默认允许
    int AddColumn(int nCol, LPCTSTR lpszColumnHeading, int nWidth, int nFormat = LVCFMT_LEFT, BOOL bCanHide = TRUE);

    // 初始化完成后调用，计算百分比并加载配置
    void InitColumns();

    // 调整列宽（窗口大小改变时调用）
    void AdjustColumnWidths();

    // 获取列是否可见
    BOOL IsColumnVisible(int nCol) const;

    // 设置列可见性
    void SetColumnVisible(int nCol, BOOL bVisible);

protected:
    std::vector<ColumnInfoEx> m_Columns;    // 列信息
    CString m_strConfigKey;                  // 配置键名
    CHeaderCtrlEx m_HeaderCtrl;              // 子类化的 Header 控件

    void ShowColumnContextMenu(CPoint pt);
    void ToggleColumnVisibility(int nColumn);
    void LoadColumnVisibility();
    void SaveColumnVisibility();
    void SubclassHeader();                   // 子类化 Header 控件

    DECLARE_MESSAGE_MAP()
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint pt);
};
