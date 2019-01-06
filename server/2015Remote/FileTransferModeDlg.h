#if !defined(AFX_FILETRANSFERMODEDLG_H__6EE95488_A679_4F78_AF95_B4D0F747455A__INCLUDED_)
#define AFX_FILETRANSFERMODEDLG_H__6EE95488_A679_4F78_AF95_B4D0F747455A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FileTransferModeDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CFileTransferModeDlg dialog

class CFileTransferModeDlg : public CDialog
{
// Construction
public:
	CString m_strFileName;
	CFileTransferModeDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CFileTransferModeDlg)
	enum { IDD = IDD_TRANSFERMODE_DLG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CFileTransferModeDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CFileTransferModeDlg)
	afx_msg	void OnEndDialog(UINT id);
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_FILETRANSFERMODEDLG_H__6EE95488_A679_4F78_AF95_B4D0F747455A__INCLUDED_)
