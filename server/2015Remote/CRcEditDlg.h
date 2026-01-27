#pragma once

#include <afx.h>
#include <afxwin.h>
#include "LangManager.h"

// CRcEditDlg 对话框

class CRcEditDlg : public CDialogLangEx
{
    DECLARE_DYNAMIC(CRcEditDlg)

public:
    CRcEditDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~CRcEditDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DIALOG_RCEDIT };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    CEdit m_EditExe;
    CEdit m_EditIco;
    CString m_sExePath;
    CString m_sIcoPath;
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    afx_msg void OnBnClickedBtnSelectExe();
    afx_msg void OnBnClickedBtnSelectIco();
};
