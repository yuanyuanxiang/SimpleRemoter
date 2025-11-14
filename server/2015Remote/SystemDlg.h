#pragma once
#include "afxcmn.h"

#include "IOCPServer.h"
// CSystemDlg 对话框

class CSystemDlg : public DialogBase
{
    DECLARE_DYNAMIC(CSystemDlg)

public:
    CSystemDlg(CWnd* pParent = NULL, Server* IOCPServer = NULL, CONTEXT_OBJECT *ContextObject = NULL);
    virtual ~CSystemDlg();

    VOID GetProcessList(void);
    VOID ShowProcessList(void);
    void ShowWindowsList(void);
    void GetWindowsList(void);
    void OnReceiveComplete(void);
    CWnd* m_pParent;
    BOOL   m_bHow;
// 对话框数据
    enum { IDD = IDD_DIALOG_SYSTEM };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    CListCtrl m_ControlList;
    virtual BOOL OnInitDialog();

    void DeleteAllItems();
    void SortByColumn(int nColumn);
    afx_msg VOID OnHdnItemclickList(NMHDR* pNMHDR, LRESULT* pResult);
    static int CALLBACK CompareFunction(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

    afx_msg void OnClose();
    afx_msg void OnNMRClickListSystem(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnPlistKill();
    afx_msg void OnPlistRefresh();
    afx_msg void OnWlistRefresh();
    afx_msg void OnWlistClose();
    afx_msg void OnWlistHide();
    afx_msg void OnWlistRecover();
    afx_msg void OnWlistMax();
    afx_msg void OnWlistMin();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPlistInject();
};
