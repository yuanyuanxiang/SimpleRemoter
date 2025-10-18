// VideoManager.h: interface for the CVideoManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_VIDEOMANAGER_H__883F2A96_1F93_4657_A169_5520CB142D46__INCLUDED_)
#define AFX_VIDEOMANAGER_H__883F2A96_1F93_4657_A169_5520CB142D46__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"
#include "CaptureVideo.h"
#include "VideoCodec.h"

class CVideoManager : public CManager
{
public:
    CVideoManager(IOCPClient* ClientObject, int n, void* user = nullptr) ;
    virtual ~CVideoManager();

    BOOL  m_bIsWorking;
    HANDLE m_hWorkThread;

    void SendBitMapInfor();
    BOOL SendNextScreen();
    static DWORD WINAPI WorkThread(LPVOID lParam);

    CCaptureVideo  m_CapVideo;
    VOID OnReceive(PBYTE szBuffer, ULONG ulLength);
    BOOL Initialize();

    DWORD	m_fccHandler;
    bool    m_bIsCompress;
    LPBYTE  lpBuffer; // ץͼ������

    CVideoCodec	*m_pVideoCodec;   //ѹ����
    void Destroy();
};

#endif // !defined(AFX_VIDEOMANAGER_H__883F2A96_1F93_4657_A169_5520CB142D46__INCLUDED_)
