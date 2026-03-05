// CursorInfor.h: interface for the CCursorInfor class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CURSORINFOR_H__ABC3705B_9461_4A94_B825_26539717C0D6__INCLUDED_)
#define AFX_CURSORINFOR_H__ABC3705B_9461_4A94_B825_26539717C0D6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// ScreenType enum (USING_GDI, USING_DXGI, USING_VIRTUAL) 已移至 common/commands.h

#define ALGORITHM_GRAY 0
#define ALGORITHM_DIFF 1
#define ALGORITHM_DEFAULT 1
#define ALGORITHM_H264 2
#define ALGORITHM_HOME 3
#define ALGORITHM_RGB565 3

#define MAX_CURSOR_TYPE	16
#define MAX_CURSOR_SIZE 64          // 最大光标尺寸
#define CURSOR_THROTTLE_MS 50       // 光标发送节流间隔 (ms)
#define CURSOR_INDEX_CUSTOM 254     // -2: 使用自定义光标
#define CURSOR_INDEX_UNSUPPORTED 255 // -1: 不支持的光标

// 自定义光标位图信息
struct CursorBitmapInfo {
    WORD hotspotX;
    WORD hotspotY;
    BYTE width;
    BYTE height;
    DWORD hash;
    BYTE bgraData[MAX_CURSOR_SIZE * MAX_CURSOR_SIZE * 4];  // 最大 16KB
    DWORD dataSize;
};

class CCursorInfo
{
private:
    LPCTSTR	m_CursorResArray[MAX_CURSOR_TYPE];
    HCURSOR	m_CursorHandleArray[MAX_CURSOR_TYPE];

public:
    CCursorInfo()
    {
        LPCTSTR	CursorResArray[MAX_CURSOR_TYPE] = {
            IDC_APPSTARTING,
            IDC_ARROW,
            IDC_CROSS,
            IDC_HAND,
            IDC_HELP,
            IDC_IBEAM,
            IDC_ICON,
            IDC_NO,
            IDC_SIZE,
            IDC_SIZEALL,
            IDC_SIZENESW,
            IDC_SIZENS,
            IDC_SIZENWSE,
            IDC_SIZEWE,
            IDC_UPARROW,
            IDC_WAIT
        };

        for (int i = 0; i < MAX_CURSOR_TYPE; ++i) {
            m_CursorResArray[i] = CursorResArray[i];
            m_CursorHandleArray[i] = LoadCursor(NULL, CursorResArray[i]);
        }
    }

    int getCurrentCursorIndex() const
    {
        CURSORINFO	ci;
        ci.cbSize = sizeof(CURSORINFO);
        if (!GetCursorInfo(&ci) || ci.flags != CURSOR_SHOWING)
            return -1;

        int i;
        for (i = 0; i < MAX_CURSOR_TYPE; ++i) {
            if (ci.hCursor == m_CursorHandleArray[i])
                break;
        }

        int	nIndex = i == MAX_CURSOR_TYPE ? -1 : i;
        return nIndex;
    }

    HCURSOR getCursorHandle( int nIndex ) const
    {
        return (nIndex >= 0 && nIndex < MAX_CURSOR_TYPE) ? m_CursorHandleArray[nIndex] : NULL;
    }

    // 获取当前光标的位图信息（用于自定义光标）
    bool getCurrentCursorBitmap(CursorBitmapInfo* info) const
    {
        if (!info) return false;

        CURSORINFO ci = { sizeof(CURSORINFO) };
        if (!GetCursorInfo(&ci) || ci.flags != CURSOR_SHOWING)
            return false;

        ICONINFO iconInfo;
        if (!GetIconInfo(ci.hCursor, &iconInfo))
            return false;

        // 获取位图信息
        BITMAP bm = { 0 };
        HBITMAP hBmp = iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask;
        if (!GetObject(hBmp, sizeof(BITMAP), &bm)) {
            if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
            if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
            return false;
        }

        // 限制尺寸
        int width = min((int)bm.bmWidth, MAX_CURSOR_SIZE);
        int height = min((int)bm.bmHeight, MAX_CURSOR_SIZE);

        // 如果是掩码位图（无彩色），高度是实际的两倍
        if (!iconInfo.hbmColor && bm.bmHeight > bm.bmWidth) {
            height = min((int)bm.bmWidth, MAX_CURSOR_SIZE);
        }

        info->hotspotX = (WORD)iconInfo.xHotspot;
        info->hotspotY = (WORD)iconInfo.yHotspot;
        info->width = (BYTE)width;
        info->height = (BYTE)height;
        info->dataSize = width * height * 4;

        // 创建兼容 DC 和位图来获取 BGRA 数据
        HDC hScreenDC = GetDC(NULL);
        if (!hScreenDC) {
            if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
            if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
            return false;
        }

        HDC hMemDC = CreateCompatibleDC(hScreenDC);
        if (!hMemDC) {
            ReleaseDC(NULL, hScreenDC);
            if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
            if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
            return false;
        }

        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;  // 负数表示从上到下
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = NULL;
        HBITMAP hDibBmp = CreateDIBSection(hScreenDC, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);

        bool success = false;
        if (hDibBmp && pBits) {
            HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hDibBmp);

            // 绘制光标到位图
            DrawIconEx(hMemDC, 0, 0, ci.hCursor, width, height, 0, NULL, DI_NORMAL);

            // 复制数据
            memcpy(info->bgraData, pBits, info->dataSize);
            success = true;

            SelectObject(hMemDC, hOldBmp);
            DeleteObject(hDibBmp);
        }

        DeleteDC(hMemDC);
        ReleaseDC(NULL, hScreenDC);

        // 清理
        if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
        if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);

        if (!success) return false;

        // 计算哈希
        info->hash = calculateBitmapHash(info->bgraData, info->dataSize);

        return true;
    }

    // FNV-1a 哈希算法（采样加速）
    static DWORD calculateBitmapHash(const BYTE* data, DWORD size)
    {
        DWORD hash = 2166136261;  // FNV offset basis
        // 每 16 字节采样一次，加速计算
        for (DWORD i = 0; i < size; i += 16) {
            hash ^= data[i];
            hash *= 16777619;  // FNV prime
        }
        return hash;
    }
};


#endif // !defined(AFX_CURSORINFOR_H__ABC3705B_9461_4A94_B825_26539717C0D6__INCLUDED_)
