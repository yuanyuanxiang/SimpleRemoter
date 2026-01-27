#pragma once
#include "LangManager.h"

// CTextDlg 对话框

class CTextDlg : public CDialogLang
{
    DECLARE_DYNAMIC(CTextDlg)

public:
    CTextDlg(CWnd* pParent = nullptr);   // 标准构造函数
    virtual ~CTextDlg();
    CString	oldstr;
    CString nowstr;
    CString cmeline;
    // 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TEXT };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnBnClickedOk();
};
