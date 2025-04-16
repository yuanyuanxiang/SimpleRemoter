#pragma once

#include <afx.h>
#include <afxwin.h>
#include "Resource.h"

// 密码的哈希值
#define PWD_HASH256 "61f04dd637a74ee34493fc1025de2c131022536da751c29e3ff4e9024d8eec43"

// CPasswordDlg 对话框

class CPasswordDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CPasswordDlg)

public:
	CPasswordDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CPasswordDlg();

	enum { IDD = IDD_DIALOG_PASSWORD };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	HICON m_hIcon;
	CEdit m_EditDeviceID;
	CEdit m_EditPassword;
	CString m_sDeviceID;
	CString m_sPassword;
	virtual BOOL OnInitDialog();
};
