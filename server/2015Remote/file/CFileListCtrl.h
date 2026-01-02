#pragma once
#include <afxcmn.h>


class CFileListCtrl :public CListCtrl
{
    DECLARE_MESSAGE_MAP()


public:
    void* m_pCFileManagerDlg;
    void SetParenDlg(VOID* p_CFileManagerDlg);
    afx_msg void OnDropFiles(HDROP hDropInfo);
};
