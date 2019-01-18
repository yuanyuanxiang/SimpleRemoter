#pragma once
#include "afxcmn.h"
#include "IOCPServer.h"
#include "afxwin.h"

// CServicesDlg 对话框

class CServicesDlg : public CDialog
{
	DECLARE_DYNAMIC(CServicesDlg)

public:
	CServicesDlg(CWnd* pParent = NULL, IOCPServer* IOCPServer = NULL, CONTEXT_OBJECT *ContextObject = NULL);   // 标准构造函数
	virtual ~CServicesDlg();
	CONTEXT_OBJECT* m_ContextObject;
	IOCPServer*     m_iocpServer;
	int ShowServicesList(void);
	void OnReceiveComplete(void);
	void ServicesConfig(BYTE bCmd);
// 对话框数据
	enum { IDD = IDD_DIALOG_SERVICES };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_ControlList;
	virtual BOOL OnInitDialog();
	CStatic m_ServicesCount;
	afx_msg void OnClose();
	afx_msg void OnServicesAuto();
	afx_msg void OnServicesManual();
	afx_msg void OnServicesStop();
	afx_msg void OnServicesStart();
	afx_msg void OnServicesReflash();
	afx_msg void OnNMRClickList(NMHDR *pNMHDR, LRESULT *pResult);
};
