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

// ScreenSpyDlg 系统菜单命令 ID
enum {
    IDM_CONTROL = 0x1010,
    IDM_FULLSCREEN,
    IDM_SEND_CTRL_ALT_DEL,
    IDM_TRACE_CURSOR,       // 跟踪显示远程鼠标
    IDM_BLOCK_INPUT,        // 锁定远程计算机输入
    IDM_SAVEDIB,            // 保存图片
    IDM_GET_CLIPBOARD,      // 获取剪贴板
    IDM_SET_CLIPBOARD,      // 设置剪贴板
    IDM_ADAPTIVE_SIZE,
    IDM_SAVEAVI,
    IDM_SAVEAVI_H264,
    IDM_SWITCHSCREEN,
    IDM_MULTITHREAD_COMPRESS,
    IDM_FPS_10,
    IDM_FPS_15,
    IDM_FPS_20,
    IDM_FPS_25,
    IDM_FPS_30,
    IDM_FPS_UNLIMITED,
    IDM_ORIGINAL_SIZE,
    IDM_SCREEN_1080P,
    IDM_REMOTE_CURSOR,
    IDM_SCROLL_DETECT_OFF,      // 滚动检测：关闭（局域网）
    IDM_SCROLL_DETECT_2,        // 滚动检测：跨网推荐
    IDM_SCROLL_DETECT_4,        // 滚动检测：标准模式
    IDM_SCROLL_DETECT_8,        // 滚动检测：省CPU模式
    IDM_QUALITY_OFF,            // 关闭质量控制（使用原有算法）
    IDM_ADAPTIVE_QUALITY,       // 自适应质量
    IDM_QUALITY_ULTRA,          // 手动质量：Ultra
    IDM_QUALITY_HIGH,           // 手动质量：High
    IDM_QUALITY_GOOD,           // 手动质量：Good
    IDM_QUALITY_MEDIUM,         // 手动质量：Medium
    IDM_QUALITY_LOW,            // 手动质量：Low
    IDM_QUALITY_MINIMAL,        // 手动质量：Minimal
    IDM_ENABLE_SSE2,
    IDM_FAST_STRETCH,           // 快速缩放模式（降低CPU占用）
    IDM_RESTORE_CONSOLE,        // RDP会话归位
    IDM_RESET_VIRTUAL_DESKTOP,  // 重置虚拟桌面
};

// 状态信息窗口 - 全屏时显示帧率/速度/质量
class CStatusInfoWnd : public CWnd
{
public:
    CStatusInfoWnd() : m_nOpacityLevel(0), m_bVisible(false), m_bDragging(false) {}

    BOOL Create(CWnd* pParent);
    void UpdateInfo(double fps, double kbps, const CString& quality);
    void Show();
    void Hide();
    void SetOpacityLevel(int level);
    void UpdatePosition(const RECT& rcMonitor);
    void LoadSettings();
    void SaveSettings();

    bool IsVisible() const { return m_bVisible; }
    bool IsParentInControlMode();  // 检查父窗口是否处于控制模式

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP()

private:
    CString m_strInfo;
    int m_nOpacityLevel;
    bool m_bVisible;

    // 拖动支持
    bool m_bDragging;
    CPoint m_ptDragStart;

    // 位置保存
    double m_dOffsetXRatio = 0.5;
    int m_nOffsetY = 50;
    bool m_bHasCustomPosition = false;
};

// CScreenSpyDlg 对话框

class CScreenSpyDlg : public DialogBase
{
    DECLARE_DYNAMIC(CScreenSpyDlg)
    CToolbarDlg* m_pToolbar = nullptr;
    CMy2015RemoteDlg* m_pParent = nullptr;

public:
    CStatusInfoWnd* m_pStatusInfoWnd = nullptr;
    // MaxFPS=20, ScrollDetectInterval=2, Reserved={}, Capabilities=0
    ScreenSettings m_Settings = { 20, 0, 0, 0, 0, 0, 0, 2, -1, {}, 0 };

public:
    // 快速缩放模式（全局配置，所有实例共享）
    static int s_nFastStretch;  // -1=未初始化, 0=关闭, 1=开启
    static bool GetFastStretchMode();
    static void SetFastStretchMode(bool bFast);

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
    VOID DrawScrollFrame(void);
    BOOL         m_bIsFirst;
    bool         m_bQualitySwitch = false; // 质量切换中，不显示"请等待"
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
    std::string m_strSaveNotice;     // 截图保存路径提示
    ULONGLONG m_nSaveNoticeTime = 0; // 截图提示开始时间
    BOOL m_bUsingFRP = FALSE;

    void SaveSnapshot(void);
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

    // 传输速率统计
    ULONG               m_ulBytesThisSecond = 0;    // 本秒累计字节
    double              m_dTransferRate = 0;        // 当前速率 (KB/s)
    // 帧率统计 (使用EMA平滑)
    ULONG               m_ulFramesThisSecond = 0;   // 本秒累计帧数
    double              m_dFrameRate = 0;           // 平滑后的帧率 (FPS)

    // 自适应质量
    struct {
        bool enabled = false;                // 是否启用自适应 (默认关闭)
        int currentLevel = QUALITY_HIGH;     // 当前质量等级
        int currentMaxWidth = 0;             // 当前分辨率限制 (0=原始)
        int lastRTT = 0;                     // 上次RTT值
        ULONGLONG lastChangeTime = 0;        // 上次切换时间
        ULONGLONG lastResChangeTime = 0;     // 上次分辨率变化时间
        int stableCount = 0;                 // 稳定计数 (用于防抖)
    } m_AdaptiveQuality;

    int  GetClientRTT();                     // 获取客户端RTT(ms)
    void EvaluateQuality();                  // 评估并调整质量
    void ApplyQualityLevel(int level, bool persist = false); // 应用质量等级
    const char* GetQualityName(int level);   // 获取质量等级名称
    void UpdateQualityMenuCheck(CMenu* SysMenu = nullptr); // 更新质量菜单勾选状态

    void OnTimer(UINT_PTR nIDEvent);
    void UpdateWindowTitle();

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
    afx_msg void OnShowStatusInfo()
    {
        if (m_pStatusInfoWnd) m_pStatusInfoWnd->Show();
    }
    afx_msg void OnHideStatusInfo()
    {
        if (m_pStatusInfoWnd) m_pStatusInfoWnd->Hide();
    }

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

    DECLARE_MESSAGE_MAP()
public:
    void PrepareDrawing(const LPBITMAPINFO bmp);
    virtual BOOL OnInitDialog();
    afx_msg void OnClose();
    afx_msg void OnPaint();
    BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    void OnLButtonDblClk(UINT nFlags, CPoint point);
};
