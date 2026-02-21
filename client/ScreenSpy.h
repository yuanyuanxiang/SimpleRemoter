// ScreenSpy.h: interface for the CScreenSpy class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCREENSPY_H__5F74528D_9ABD_404E_84D2_06C96A0615F4__INCLUDED_)
#define AFX_SCREENSPY_H__5F74528D_9ABD_404E_84D2_06C96A0615F4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define COPY_ALL 1	// 拷贝全部屏幕，不分块拷贝（added by yuanyuanxiang 2019-1-7）
#include "CursorInfo.h"
#include "ScreenCapture.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")


class EnumHwndsPrintData
{
public:
    EnumHwndsPrintData()
    {
        memset(this, 0, sizeof(EnumHwndsPrintData));
    }
    void Create(HDC desktop, int _x, int _y, int w, int h)
    {
        x = _x;
        y = _y;
        width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        hDcWindow = CreateCompatibleDC(desktop);
        hBmpWindow = CreateCompatibleBitmap(desktop, width, height);
    }
    EnumHwndsPrintData& SetScreenDC(HDC dc)
    {
        hDcScreen = dc;
        return *this;
    }
    ~EnumHwndsPrintData()
    {
        if (hDcWindow) DeleteDC(hDcWindow);
        hDcWindow = nullptr;
        if (hBmpWindow)DeleteObject(hBmpWindow);
        hBmpWindow = nullptr;
    }
    HDC GetWindowDC() const
    {
        return hDcWindow;
    }
    HDC GetScreenDC() const
    {
        return hDcScreen;
    }
    HBITMAP GetWindowBmp() const
    {
        return hBmpWindow;
    }
    int X() const
    {
        return x;
    }
    int Y() const
    {
        return y;
    }
    int Width()const
    {
        return width;
    }
    int Height()const
    {
        return height;
    }
private:
    HDC hDcWindow;
    HDC hDcScreen;
    HBITMAP hBmpWindow;
    int x;
    int y;
    int width;
    int height;
};

class CScreenSpy : public ScreenCapture
{
protected:
    HDC              m_hDeskTopDC;          // 屏幕上下文
    HDC              m_hFullMemDC;          // 上一个上下文
    HDC              m_hDiffMemDC;          // 差异上下文
    HBITMAP	         m_BitmapHandle;        // 上一帧位图
    HBITMAP	         m_DiffBitmapHandle;    // 差异帧位图
    PVOID            m_BitmapData_Full;     // 当前位图数据
    PVOID            m_DiffBitmapData_Full; // 差异位图数据

    BOOL					m_bVirtualPaint;// 是否虚拟绘制
    EnumHwndsPrintData		m_data;

public:
    CScreenSpy(ULONG ulbiBitCount, BYTE algo, BOOL vDesk = FALSE, int gop = DEFAULT_GOP, BOOL all = FALSE);

    virtual ~CScreenSpy();

    int GetWidth()const
    {
        return m_ulFullWidth;
    }

    int GetHeight() const
    {
        return m_ulFullHeight;
    }

    bool IsZoomed() const
    {
        return m_bZoomed;
    }

    double GetWZoom() const
    {
        return m_wZoom;
    }

    double GetHZoom() const
    {
        return m_hZoom;
    }

    const LPBITMAPINFO& GetBIData() const
    {
        return m_BitmapInfor_Send;
    }

    ULONG GetFirstScreenLength() const
    {
        return m_BitmapInfor_Send->bmiHeader.biSizeImage;
    }

    static BOOL PaintWindow(HWND hWnd, EnumHwndsPrintData* data)
    {
        if (!IsWindowVisible(hWnd) || IsIconic(hWnd))
            return TRUE;

        RECT rect;
        if (!GetWindowRect(hWnd, &rect))
            return FALSE;

        // 检查是否为菜单窗口
        char className[32] = {};
        GetClassNameA(hWnd, className, sizeof(className));
        BOOL isMenu = (strcmp(className, "#32768") == 0);

        // 获取不含 DWM 阴影的真实窗口边界（Windows 10 窗口有透明阴影导致黑边）
        // 菜单窗口不使用 DWM 裁剪
        RECT frameRect = rect;
        BOOL hasDwmFrame = FALSE;
        if (!isMenu) {
            hasDwmFrame = SUCCEEDED(DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                &frameRect, sizeof(frameRect)));
        }

        HDC hDcWindow = data->GetWindowDC();
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hDcWindow, data->GetWindowBmp());

        // 对于某些现代应用（WinUI 3 等），需要先请求重绘
        RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);

        BOOL captured = PrintWindow(hWnd, hDcWindow, PW_RENDERFULLCONTENT);
        if (!captured) {
            // PrintWindow 失败，尝试不带 PW_RENDERFULLCONTENT
            captured = PrintWindow(hWnd, hDcWindow, 0);
        }
        if (!captured) {
            // 仍然失败，尝试 WM_PRINT
            captured = SendMessageTimeout(hWnd, WM_PRINT, (WPARAM)hDcWindow,
                PRF_CLIENT | PRF_NONCLIENT, SMTO_BLOCK, 50, NULL) != 0;
        }
        if (!captured) {
            char title[128] = {};
            GetWindowTextA(hWnd, title, sizeof(title));
            Mprintf("PrintWindow failed: %s [%s]\n", className, title);
        }

        if (captured) {
            if (hasDwmFrame) {
                // 使用真实边界（不含阴影），从 PrintWindow 输出的正确偏移位置复制
                int shadowLeft = frameRect.left - rect.left;
                int shadowTop = frameRect.top - rect.top;
                int realWidth = frameRect.right - frameRect.left;
                int realHeight = frameRect.bottom - frameRect.top;
                BitBlt(data->GetScreenDC(), frameRect.left - data->X(), frameRect.top - data->Y(),
                       realWidth, realHeight, hDcWindow, shadowLeft, shadowTop, SRCCOPY);
            } else {
                // 菜单和其他窗口使用原始方式
                BitBlt(data->GetScreenDC(), rect.left - data->X(), rect.top - data->Y(),
                       rect.right - rect.left, rect.bottom - rect.top, hDcWindow, 0, 0, SRCCOPY);
            }
        }
        // 即使捕获失败也返回 TRUE，避免阻止其他窗口的绘制
        SelectObject(hDcWindow, hOldBmp);
        return TRUE;
    }

    static int EnumWindowsTopToDown(HWND owner, WNDENUMPROC proc, LPARAM param)
    {
        HWND currentWindow = GetTopWindow(owner);
        if (currentWindow == NULL)
            return -1;
        if ((currentWindow = GetWindow(currentWindow, GW_HWNDLAST)) == NULL)
            return -2;
        while (proc(currentWindow, param) && (currentWindow = GetWindow(currentWindow, GW_HWNDPREV)) != NULL);
        return 0;
    }

    static BOOL CALLBACK EnumHwndsPrint(HWND hWnd, LPARAM lParam)
    {
        AUTO_TICK_C(100, "");
        if (FALSE == PaintWindow(hWnd, (EnumHwndsPrintData*)lParam)) {
            char text[_MAX_PATH] = {};
            GetWindowText(hWnd, text, sizeof(text));
            Mprintf("PaintWindow %s[%p] failed!!!\n", text, hWnd);
        }

        static OSVERSIONINFOA versionInfo = { sizeof(OSVERSIONINFOA) };
        static BOOL result = GetVersionExA(&versionInfo);
        if (versionInfo.dwMajorVersion < 6) {
            DWORD style = GetWindowLongA(hWnd, GWL_EXSTYLE);
            SetWindowLongA(hWnd, GWL_EXSTYLE, style | WS_EX_COMPOSITED);
            EnumWindowsTopToDown(hWnd, EnumHwndsPrint, lParam);
        }

        return TRUE;
    }

    virtual LPBYTE GetFirstScreenData(ULONG* ulFirstScreenLength);

    virtual LPBYTE ScanNextScreen()
    {
        ScanScreen(m_hDiffMemDC, m_hDeskTopDC, m_ulFullWidth, m_ulFullHeight);
        LPBYTE bmp = scaleBitmap(m_BmpZoomBuffer, (LPBYTE)m_DiffBitmapData_Full);
        return (LPBYTE)bmp;
    }

    VOID ScanScreen(HDC hdcDest, HDC hdcSour, ULONG ulWidth, ULONG ulHeight);

    // 重置桌面 DC（桌面切换时调用）
    void ResetDesktopDC()
    {
        ReleaseDC(NULL, m_hDeskTopDC);
        m_hDeskTopDC = GetDC(NULL);
        m_data.Create(m_hDeskTopDC, m_iScreenX, m_iScreenY, m_ulFullWidth, m_ulFullHeight);
    }
};

#endif // !defined(AFX_SCREENSPY_H__5F74528D_9ABD_404E_84D2_06C96A0615F4__INCLUDED_)
