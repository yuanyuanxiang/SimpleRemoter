#pragma once


// CInjectCodeDlg �Ի���

class CInjectCodeDlg : public CDialog
{
    DECLARE_DYNAMIC(CInjectCodeDlg)

public:
    CInjectCodeDlg(CWnd* pParent = nullptr);
    virtual ~CInjectCodeDlg();

    CComboBox m_combo_main;
    int isel;
    CString Str_loacal;
    CString Str_remote;

    // �Ի�������
#ifdef AFX_DESIGN_TIME
    enum {
        IDD = IDD_INJECTINFO
    };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

    DECLARE_MESSAGE_MAP()
public:
    virtual BOOL OnInitDialog();

    afx_msg void OnBnClickedButtonChoose();
    afx_msg void OnBnClickedButtonInject();
    afx_msg void OnCbnSelchangeComboInjects();
};
