// ScreenSpy.h: interface for the CScreenSpy class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCREENSPY_H__5F74528D_9ABD_404E_84D2_06C96A0615F4__INCLUDED_)
#define AFX_SCREENSPY_H__5F74528D_9ABD_404E_84D2_06C96A0615F4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#define ALGORITHM_DIFF 1
#include "CursorInfor.h"


class CScreenSpy  
{
public:
	CScreenSpy::CScreenSpy(ULONG ulbiBitCount);
	virtual ~CScreenSpy();
	ULONG CScreenSpy::GetBISize(); 
	LPBITMAPINFO CScreenSpy::GetBIData();
	ULONG  m_ulbiBitCount;
	LPBITMAPINFO m_BitmapInfor_Full;
	ULONG  m_ulFullWidth, m_ulFullHeight;               //屏幕的分辨率
	LPBITMAPINFO CScreenSpy::ConstructBI(ULONG ulbiBitCount, 
		ULONG ulFullWidth, ULONG ulFullHeight);

	HWND m_hDeskTopWnd;      //当前工作区的窗口句柄
	HDC  m_hFullDC;          //Explorer.exe 的窗口设备DC
	HDC  m_hFullMemDC;
	HBITMAP	m_BitmapHandle;
	PVOID   m_BitmapData_Full;
	DWORD  m_dwBitBltRop;
	LPVOID CScreenSpy::GetFirstScreenData();
	ULONG CScreenSpy::GetFirstScreenLength();
	LPVOID CScreenSpy::GetNextScreenData(ULONG* ulNextSendLength);
	BYTE* m_RectBuffer;
	ULONG m_RectBufferOffset;
	BYTE   m_bAlgorithm;
	VOID CScreenSpy::WriteRectBuffer(LPBYTE	szBuffer,ULONG ulLength);
	CCursorInfor m_CursorInfor;
	HDC  m_hDiffMemDC;
	HBITMAP	m_DiffBitmapHandle;
	PVOID   m_DiffBitmapData_Full;
	ULONG CScreenSpy::CompareBitmap(LPBYTE CompareSourData, LPBYTE CompareDestData, 
		LPBYTE szBuffer, DWORD ulCompareLength);
	VOID CScreenSpy::ScanScreen(HDC hdcDest, HDC hdcSour, ULONG ulWidth, ULONG ulHeight);
};

#endif // !defined(AFX_SCREENSPY_H__5F74528D_9ABD_404E_84D2_06C96A0615F4__INCLUDED_)
