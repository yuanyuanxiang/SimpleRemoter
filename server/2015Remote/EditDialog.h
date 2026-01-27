#pragma once
#include "LangManager.h"

// CEditDialog 对话框

class CEditDialog : public CDialogLang
{
    DECLARE_DYNAMIC(CEditDialog)

public:
    CEditDialog(CWnd* pParent = NULL);   // 标准构造函数
    virtual ~CEditDialog();

    // 对话框数据
    enum { IDD = IDD_DIALOG_NEWFOLDER };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    CString m_EditString;
    afx_msg void OnBnClickedOk();
};
