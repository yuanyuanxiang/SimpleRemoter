
#pragma once

/////////////////////////////////////////////////////////////////////////////
// CFileTransferModeDlg dialog
#include "LangManager.h"

namespace file
{

class CFileTransferModeDlg : public CDialogLang
{
public:
    CString m_strFileName;
    CFileTransferModeDlg(CWnd* pParent = NULL);

    enum { IDD = IDD_TRANSFERMODE_DLG };
protected:
    virtual void DoDataExchange(CDataExchange* pDX);

protected:
    afx_msg	void OnEndDialog(UINT id);
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedOverwrite();
    afx_msg void OnBnClickedOverwriteAll();
    afx_msg void OnBnClickedAddition();
    afx_msg void OnBnClickedAdditionAll();
    afx_msg void OnBnClickedJump();
    afx_msg void OnBnClickedJumpAll();
    afx_msg void OnBnClickedCancel();
    DECLARE_MESSAGE_MAP()
};
}
