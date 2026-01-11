#pragma once
#include "IOCPServer.h"
#include "..\..\client\CursorInfo.h"
#include "VideoDlg.h"
#include "ToolbarDlg.h"
#include "2015RemoteDlg.h"

extern "C"
{
#include "libavcodec\avcodec.h"
#include "libavutil\avutil.h"
#include "libyuv\libyuv.h"
}

#ifndef _WIN64
// https://github.com/Terodee/FFMpeg-windows-static-build/releases
#pragma comment(lib,"libavcodec.lib")
#pragma comment(lib,"libavutil.lib")
#pragma comment(lib,"libswresample.lib")

#pragma comment(lib,"libyuv/libyuv.lib")
#else
#pragma comment(lib,"x264/libx264_x64.lib")
#pragma comment(lib,"libyuv/libyuv_x64.lib")
// https://github.com/ShiftMediaProject/FFmpeg
#ifdef _DEBUG
#pragma comment(lib,"libavcodec_x64d.lib")
#pragma comment(lib,"libavutil_x64d.lib")
#pragma comment(lib,"libswresample_x64d.lib")
#else
#pragma comment(lib,"libavcodec_x64.lib")
#pragma comment(lib,"libavutil_x64.lib")
#pragma comment(lib,"libswresample_x64.lib")
#endif
#endif

#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "Mfuuid.lib")
#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Strmiids.lib")

// CScreenSpyDlg 对话框

class CScreenSpyDlg : public DialogBase
{
    DECLARE_DYNAMIC(CScreenSpyDlg)
    CToolbarDlg* m_pToolbar = nullptr;
    CMy2015RemoteDlg* m_pParent = nullptr;
    ScreenSettings m_Settings = { 20 };

public:
    CScreenSpyDlg(CMy2015RemoteDlg* Parent, Server* IOCPServer=NULL, CONTEXT_OBJECT *ContextObject=NULL);
    virtual ~CScreenSpyDlg();
    virtual BOOL ShouldReconnect()
    {
        return TRUE;
    }

    VOID SendNext(void);
    VOID OnReceiveComplete();
    HDC  m_hFullDC;
    HDC  m_hFullMemDC;
    HBITMAP	m_BitmapHandle;
    PVOID   m_BitmapData_Full;
    LPBITMAPINFO m_BitmapInfor_Full;
    VOID DrawFirstScreen(void);
    VOID DrawNextScreenDiff(bool keyFrame);
    BOOL         m_bIsFirst;
    ULONG m_ulHScrollPos;
    ULONG m_ulVScrollPos;
    // fillMode: 0=不填充, 1=全黑, 2=半透明
    VOID DrawTipString(CString strString, int fillMode=1);

    POINT  m_ClientCursorPos;
    BYTE m_bCursorIndex;
    BOOL     m_bIsTraceCursor;
    CCursorInfo	m_CursorInfo; //自定义的一个系统的光标类
    VOID SendCommand(const MYMSG* Msg);
    void SendScaledMouseMessage(MSG* pMsg, bool makeLP);
    VOID UpdateServerClipboard(char *szBuffer,ULONG ulLength);
    VOID SendServerClipboard(void);

    BOOL  m_bIsCtrl;
    LPBYTE m_szData;
    BOOL  m_bSend;
    ULONG m_ulMsgCount;
    int m_FrameID;
    bool m_bHide = false;

    BOOL SaveSnapshot(void);
    // 对话框数据
    enum { IDD = IDD_DIALOG_SCREEN_SPY };

    WINDOWPLACEMENT m_struOldWndpl;

    AVCodec*			m_pCodec;
    AVCodecContext*		m_pCodecContext;
    AVPacket			m_AVPacket;
    AVFrame				m_AVFrame;

    clock_t				m_lastMouseMove; // 鼠标移动时间
    POINT				m_lastMousePoint;// 上次鼠标位置
    BOOL				m_bAdaptiveSize = TRUE;
    HCURSOR				m_hRemoteCursor = NULL;
    CRect				m_CRect;
    double				m_wZoom=1, m_hZoom=1;
    bool				m_bMouseTracking = false;

    CString             m_aviFile;
    CBmpToAvi	        m_aviStream;
    void OnTimer(UINT_PTR nIDEvent);

    bool Decode(LPBYTE Buffer, int size);
    void EnterFullScreen();
    bool LeaveFullScreen();
    void UpdateCtrlStatus(BOOL ctrl);
    void OnDropFiles(HDROP hDropInfo);

    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
    afx_msg LRESULT OnDisconnect(WPARAM wParam, LPARAM lParam);
    afx_msg void OnExitFullscreen()
    {
		BYTE cmd[4] = { CMD_FULL_SCREEN, m_Settings.FullScreen = FALSE };
		m_ContextObject->Send2Client(cmd, sizeof(cmd));
        LeaveFullScreen();
    }

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    void PrepareDrawing(const LPBITMAPINFO bmp);
    virtual BOOL OnInitDialog();
    afx_msg void OnClose();
    afx_msg void OnPaint();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    void OnLButtonDblClk(UINT nFlags, CPoint point);
};
