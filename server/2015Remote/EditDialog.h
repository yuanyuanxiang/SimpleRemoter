#pragma once


// CEditDialog �Ի���

class CEditDialog : public CDialog
{
    DECLARE_DYNAMIC(CEditDialog)

public:
    CEditDialog(CWnd* pParent = NULL);   // ��׼���캯��
    virtual ~CEditDialog();

    // �Ի�������
    enum { IDD = IDD_DIALOG_NEWFOLDER };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

    DECLARE_MESSAGE_MAP()
public:
    CString m_EditString;
    afx_msg void OnBnClickedOk();
};
