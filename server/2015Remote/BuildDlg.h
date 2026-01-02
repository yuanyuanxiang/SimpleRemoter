#pragma once

#include "Buffer.h"

LPBYTE ReadResource(int resourceId, DWORD& dwSize);

std::string ReleaseEXE(int resID, const char* name);

// CBuildDlg 对话框

class CBuildDlg : public CDialog
{
    DECLARE_DYNAMIC(CBuildDlg)

public:
    CBuildDlg(CWnd* pParent = NULL);   // 标准构造函数
    virtual ~CBuildDlg();

// 对话框数据
    enum { IDD = IDD_DIALOG_BUILD };
    CMenu m_MainMenu;
    BOOL m_runasAdmin;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
    CString GetFilePath(CString type, CString filter, BOOL isOpen = TRUE);
	BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);

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
    CComboBox m_ComboCompress;
    CString m_strFindden;
    afx_msg void OnHelpFindden();
    CEdit m_EditGroup;
    CString m_sGroupName;
    CString m_strEncryptIP;
    afx_msg void OnMenuEncryptIp();
    afx_msg void OnClientRunasAdmin();
    CComboBox m_ComboPayload;
    afx_msg void OnCbnSelchangeComboCompress();
    CStatic m_StaticPayload;
    CSliderCtrl m_SliderClientSize;
};
