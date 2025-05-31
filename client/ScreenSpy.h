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


class EnumHwndsPrintData {
public:
	EnumHwndsPrintData() {
		memset(this, 0, sizeof(EnumHwndsPrintData));
	}
	void Create(HDC desktop, int _x, int _y, int w, int h) {
		x = _x;
		y = _y;
		width = w;
		height = h;
		hDcWindow = CreateCompatibleDC(desktop);
		hBmpWindow = CreateCompatibleBitmap(desktop, w, h);
	}
	EnumHwndsPrintData& SetScreenDC(HDC dc) {
		hDcScreen = dc;
		return *this;
	}
	~EnumHwndsPrintData() {
		if (hDcWindow) DeleteDC(hDcWindow);
		hDcWindow = nullptr;
		if (hBmpWindow)DeleteObject(hBmpWindow);
		hBmpWindow = nullptr;
	}
	HDC GetWindowDC() const {
		return hDcWindow;
	}
	HDC GetScreenDC() const {
		return hDcScreen;
	}
	HBITMAP GetWindowBmp() const {
		return hBmpWindow;
	}
	int X() const {
		return x;
	}
	int Y() const {
		return y;
	}
	int Width()const {
		return width;
	}
	int Height()const {
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

class CScreenSpy  : public ScreenCapture
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
	CScreenSpy(ULONG ulbiBitCount, BYTE algo, BOOL vDesk = FALSE, int gop = DEFAULT_GOP);

	virtual ~CScreenSpy();

	int GetWidth()const {
		return m_ulFullWidth;
	}

	int GetHeight() const {
		return m_ulFullHeight;
	}

	bool IsZoomed() const {
		return m_bZoomed;
	}

	double GetWZoom() const {
		return m_wZoom;
	}

	double GetHZoom() const {
		return m_hZoom;
	}

	const LPBITMAPINFO& GetBIData() const
	{
		return m_BitmapInfor_Full;
	}

	ULONG GetFirstScreenLength() const
	{
		return m_BitmapInfor_Full->bmiHeader.biSizeImage;
	}

	static BOOL PaintWindow(HWND hWnd, EnumHwndsPrintData* data)
	{
		if (!IsWindowVisible(hWnd) || IsIconic(hWnd))
			return TRUE;

		RECT rect;
		if (!GetWindowRect(hWnd, &rect))
			return FALSE;

		HDC hDcWindow = data->GetWindowDC();
		HBITMAP hOldBmp = (HBITMAP)SelectObject(hDcWindow, data->GetWindowBmp());
		BOOL ret = FALSE;
		if (PrintWindow(hWnd, hDcWindow, PW_RENDERFULLCONTENT) || SendMessageTimeout(hWnd, WM_PRINT,
			(WPARAM)hDcWindow, PRF_CLIENT | PRF_NONCLIENT, SMTO_BLOCK, 50, NULL)) {
			BitBlt(data->GetScreenDC(), rect.left - data->X(), rect.top - data->Y(),
				rect.right - rect.left, rect.bottom - rect.top, hDcWindow, 0, 0, SRCCOPY);

			ret = TRUE;
		}
		SelectObject(hDcWindow, hOldBmp);

		return ret;
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
		AUTO_TICK_C(50);
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

	virtual LPBYTE ScanNextScreen() {
		ScanScreen(m_hDiffMemDC, m_hDeskTopDC, m_ulFullWidth, m_ulFullHeight);
		return (LPBYTE)m_DiffBitmapData_Full;
	}

	VOID ScanScreen(HDC hdcDest, HDC hdcSour, ULONG ulWidth, ULONG ulHeight);
};

#endif // !defined(AFX_SCREENSPY_H__5F74528D_9ABD_404E_84D2_06C96A0615F4__INCLUDED_)
