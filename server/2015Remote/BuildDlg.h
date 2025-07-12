#pragma once

#include "Buffer.h"

LPBYTE ReadResource(int resourceId, DWORD& dwSize);

// CBuildDlg �Ի���

class CBuildDlg : public CDialog
{
	DECLARE_DYNAMIC(CBuildDlg)

public:
	CBuildDlg(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CBuildDlg();

// �Ի�������
	enum { IDD = IDD_DIALOG_BUILD };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CString m_strIP;
	CString m_strPort;
	afx_msg void OnBnClickedOk();
	virtual BOOL OnInitDialog();
	CComboBox m_ComboExe;

	afx_msg void OnCbnSelchangeComboExe();
	CStatic m_OtherItem;
	CComboBox m_ComboBits;
	CComboBox m_ComboRunType;
	CComboBox m_ComboProto;
	CComboBox m_ComboEncrypt;
	afx_msg void OnHelpParameters();
};
