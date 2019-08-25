#pragma once
#include "afxwin.h"


// CSettingDlg 对话框

class CSettingDlg : public CDialog
{
	DECLARE_DYNAMIC(CSettingDlg)

public:
	CSettingDlg(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CSettingDlg();

	// 对话框数据
	enum { IDD = IDD_DIALOG_SET };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	UINT m_nListenPort;
	UINT m_nMax_Connect;
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedButtonSettingapply();
	afx_msg void OnEnChangeEditPort();
	afx_msg void OnEnChangeEditMax();
	CButton m_ApplyButton;
	virtual void OnOK();
};
