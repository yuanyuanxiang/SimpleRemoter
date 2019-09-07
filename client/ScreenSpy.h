// ScreenSpy.h: interface for the CScreenSpy class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCREENSPY_H__5F74528D_9ABD_404E_84D2_06C96A0615F4__INCLUDED_)
#define AFX_SCREENSPY_H__5F74528D_9ABD_404E_84D2_06C96A0615F4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#define ALGORITHM_DIFF 1
#define COPY_ALL 1	// 拷贝全部屏幕，不分块拷贝（added by yuanyuanxiang 2019-1-7）
#include "CursorInfo.h"


class CScreenSpy  
{
private:
	BYTE             m_bAlgorithm;       // 屏幕差异算法
	ULONG            m_ulbiBitCount;     // 每像素位数

	ULONG            m_ulFullWidth;      // 屏幕宽
	ULONG            m_ulFullHeight;     //屏幕高
	bool             m_bZoomed;          // 屏幕被缩放
	double           m_wZoom;            // 屏幕横向缩放比
	double           m_hZoom;            // 屏幕纵向缩放比

	LPBITMAPINFO     m_BitmapInfor_Full; // BMP信息

	HWND             m_hDeskTopWnd;      //当前工作区的窗口句柄
	HDC              m_hFullDC;          //Explorer.exe 的窗口设备DC

	HDC              m_hFullMemDC;
	HBITMAP	         m_BitmapHandle;
	PVOID            m_BitmapData_Full;

	HDC              m_hDiffMemDC;
	HBITMAP	         m_DiffBitmapHandle;
	PVOID            m_DiffBitmapData_Full;

	ULONG            m_RectBufferOffset; // 缓存区位移
	BYTE*            m_RectBuffer;       // 缓存区

public:
	CScreenSpy(ULONG ulbiBitCount);

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

	FORCEINLINE VOID WriteRectBuffer(LPBYTE szBuffer, ULONG ulLength)
	{
		memcpy(m_RectBuffer + m_RectBufferOffset, szBuffer, ulLength);
		m_RectBufferOffset += ulLength;
	}

	LPVOID GetFirstScreenData();

	LPVOID GetNextScreenData(ULONG* ulNextSendLength);

	ULONG CompareBitmap(LPBYTE CompareSourData, LPBYTE CompareDestData, LPBYTE szBuffer, DWORD ulCompareLength);

	VOID ScanScreen(HDC hdcDest, HDC hdcSour, ULONG ulWidth, ULONG ulHeight);

};

#endif // !defined(AFX_SCREENSPY_H__5F74528D_9ABD_404E_84D2_06C96A0615F4__INCLUDED_)
