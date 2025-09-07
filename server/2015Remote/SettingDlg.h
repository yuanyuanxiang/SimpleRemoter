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
	CString m_nListenPort;
	UINT m_nMax_Connect;
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedButtonSettingapply();
	afx_msg void OnEnChangeEditPort();
	afx_msg void OnEnChangeEditMax();
	CButton m_ApplyButton;
	virtual void OnOK();
	CComboBox m_ComboScreenCapture;
	CString m_sScreenCapture;
	CComboBox m_ComboScreenCompress;
	CString m_sScreenCompress;
	CEdit m_EditReportInterval;
	int m_nReportInterval;
	CComboBox m_ComboSoftwareDetect;
	CString m_sSoftwareDetect;
	CEdit m_EditPublicIP;
	CString m_sPublicIP;
	afx_msg void OnBnClickedRadioAllScreen();
	afx_msg void OnBnClickedRadioMainScreen();
	CEdit m_EditUdpOption;
	CString m_sUdpOption;
	afx_msg void OnBnClickedRadioFrpOff();
	afx_msg void OnBnClickedRadioFrpOn();
	CEdit m_EditFrpPort;
	int m_nFrpPort;
	CEdit m_EditFrpToken;
	CString m_sFrpToken;
	CComboBox m_ComboVideoWall;
};
