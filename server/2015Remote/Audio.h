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
	LPWAVEHDR m_InAudioHeader[2];   //两个头
	LPBYTE    m_InAudioData[2];     //两个数据   保持声音的连续
	HANDLE	  m_hEventWaveIn; 
	HANDLE	  m_hStartRecord;    //两个事件
	HWAVEIN   m_hWaveIn;         //设备句柄	
	DWORD     m_nWaveInIndex;
	bool    m_hThreadCallBack;
	static DWORD WINAPI waveInCallBack(LPVOID lParam);   //发送到主控端
	LPBYTE GetRecordBuffer(LPDWORD dwBufferSize);
	BOOL InitializeWaveIn();
	BOOL m_bIsWaveInUsed;

	HWAVEOUT m_hWaveOut;
	BOOL	m_bExit;
	BOOL     m_bIsWaveOutUsed;
	DWORD    m_nWaveOutIndex;
	LPWAVEHDR m_OutAudioHeader[2];   //两个头
	LPBYTE    m_OutAudioData[2];     //两个数据   保持声音的连续
	BOOL PlayBuffer(LPBYTE szBuffer, DWORD dwBufferSize);
	BOOL InitializeWaveOut();
};

#endif // !defined(AFX_AUDIO_H__56854DE7_5FE4_486F_9AFC_CE3726EF7CBC__INCLUDED_)
