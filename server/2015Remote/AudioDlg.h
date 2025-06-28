#pragma once

#include "IOCPServer.h"
#include "../../client/Audio.h"

// CAudioDlg 对话框

class CAudioDlg : public DialogBase
{
	DECLARE_DYNAMIC(CAudioDlg)

public:
	CAudioDlg(CWnd* pParent = NULL, IOCPServer* IOCPServer = NULL, CONTEXT_OBJECT *ContextObject = NULL);   // 标准构造函数
	virtual ~CAudioDlg();

	DWORD         m_nTotalRecvBytes;
	BOOL          m_bIsWorking;
	BOOL		  m_bThreadRun;
	HANDLE        m_hWorkThread;
	CAudio		  m_AudioObject;

	static DWORD WINAPI WorkThread(LPVOID lParam);

	void CAudioDlg::OnReceiveComplete(void);
// 对话框数据
	enum { IDD = IDD_DIALOG_AUDIO };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	BOOL m_bSend; // 是否发送本地语音到远程
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	afx_msg void OnBnClickedCheck();
};
