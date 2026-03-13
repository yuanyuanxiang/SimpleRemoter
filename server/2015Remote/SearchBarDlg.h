// SearchBarDlg.h - 浮动搜索工具栏
#pragma once
#include "Resource.h"
#include "LangManager.h"
#include "CIconButton.h"
#include <vector>

class CMy2015RemoteDlg;

class CSearchBarDlg : public CDialogLangEx
{
    DECLARE_DYNAMIC(CSearchBarDlg)

public:
    CSearchBarDlg(CMy2015RemoteDlg* pParent = nullptr);
    virtual ~CSearchBarDlg();

    enum { IDD = IDD_SEARCH_BAR };

    CMy2015RemoteDlg* m_pParent;

    // 控件
    CEdit m_editSearch;
    CIconButton m_btnPrev;
    CIconButton m_btnNext;
    CIconButton m_btnClose;
    CStatic m_staticCount;
    CToolTipCtrl m_tooltip;

    // 搜索状态
    std::vector<int> m_Results;      // 匹配项的列表索引
    int m_nCurrentIndex;             // 当前高亮的结果索引
    CString m_strLastSearch;         // 上次搜索文本

    // 方法
    void Show();                     // 显示搜索栏
    void Hide();                     // 隐藏搜索栏
    void DoSearch();                 // 执行搜索
    void GotoPrev();                 // 上一个结果
    void GotoNext();                 // 下一个结果
    void GotoResult(int index);      // 跳转到指定结果
    void UpdateCountText();          // 更新计数显示
    void UpdatePosition();           // 更新位置（居中于父窗口）

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnBnClickedPrev();
    afx_msg void OnBnClickedNext();
    afx_msg void OnBnClickedClose();
    afx_msg void OnEnChangeSearch();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

    static const UINT_PTR TIMER_SEARCH = 1;  // 延迟搜索定时器
    static const UINT SEARCH_DELAY_MS = 500; // 延迟时间(毫秒)

private:
    CBrush m_brushEdit;              // 输入框背景画刷
    CBrush m_brushStatic;            // 静态控件背景画刷
};
