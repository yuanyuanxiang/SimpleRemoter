#pragma once

#include <afx.h>
#include <afxwin.h>
#include "Resource.h"
#include "common/commands.h"

// 密码的哈希值
// 提示：请用hashSHA256函数获得密码的哈希值，你应该用自己的密码生成哈希值，并替换这个默认值.
#define PWD_HASH256 "61f04dd637a74ee34493fc1025de2c131022536da751c29e3ff4e9024d8eec43"

// CPasswordDlg 对话框
std::string GetPwdHash();

const Validation* GetValidation(int offset=100);

std::string GetMasterId();

std::string GetHMAC(int offset=100);

bool IsPwdHashValid(const char* pwdHash = nullptr);

bool WritePwdHash(char* target, const std::string& pwdHash, const Validation &verify);

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


class CPwdGenDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CPwdGenDlg)

public:
	CPwdGenDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CPwdGenDlg();

	enum {
		IDD = IDD_DIALOG_KEYGEN
	};

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	HICON m_hIcon;
	CEdit m_EditDeviceID;
	CEdit m_EditPassword;
	CEdit m_EditUserPwd;
	CString m_sDeviceID;
	CString m_sPassword;
	CString m_sUserPwd;
	afx_msg void OnBnClickedButtonGenkey();
	CDateTimeCtrl m_PwdExpireDate;
	COleDateTime m_ExpireTm;
	CDateTimeCtrl m_StartDate;
	COleDateTime m_StartTm;
	virtual BOOL OnInitDialog();
};
