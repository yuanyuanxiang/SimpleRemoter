// Audio.h: interface for the CAudio class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_AUDIO_H__56854DE7_5FE4_486F_9AFC_CE3726EF7CBC__INCLUDED_)
#define AFX_AUDIO_H__56854DE7_5FE4_486F_9AFC_CE3726EF7CBC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include <MMSYSTEM.H>
#include <MMReg.h>


class CAudio
{
public:
    CAudio();
    virtual ~CAudio();
    GSM610WAVEFORMAT m_GSMWavefmt;
    ULONG     m_ulBufferLength;
    LPWAVEHDR m_InAudioHeader[2];   //����ͷ
    LPBYTE    m_InAudioData[2];     //��������   ��������������
    HANDLE	  m_hEventWaveIn;
    HANDLE	  m_hStartRecord;    //�����¼�
    HWAVEIN   m_hWaveIn;         //�豸���
    DWORD     m_nWaveInIndex;
    bool    m_hThreadCallBack;
    HANDLE m_Thread;// waveInCallBack�߳�
    static DWORD WINAPI waveInCallBack(LPVOID lParam);   //���͵����ض�
    LPBYTE GetRecordBuffer(LPDWORD dwBufferSize);
    BOOL InitializeWaveIn();
    BOOL m_bIsWaveInUsed;

    HWAVEOUT m_hWaveOut;
    BOOL	m_bExit;
    BOOL     m_bIsWaveOutUsed;
    DWORD    m_nWaveOutIndex;
    LPWAVEHDR m_OutAudioHeader[2];   //����ͷ
    LPBYTE    m_OutAudioData[2];     //��������   ��������������
    BOOL PlayBuffer(LPBYTE szBuffer, DWORD dwBufferSize);
    BOOL InitializeWaveOut();
};

#endif // !defined(AFX_AUDIO_H__56854DE7_5FE4_486F_9AFC_CE3726EF7CBC__INCLUDED_)
