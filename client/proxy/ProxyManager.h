#pragma once
#include "Manager.h"
#include <map>

class CProxyManager : public CManager
{
public:
    BOOL m_bUse;
    CProxyManager(ISocketBase* pClient, int n = 0, void* user = nullptr);
    virtual ~CProxyManager();
    virtual void OnReceive(PBYTE lpBuffer, ULONG nSize);
    int Send(LPBYTE lpData, UINT nSize);
    void Disconnect(DWORD index);
    void SendConnectResult(LPBYTE lpBuffer, DWORD ip, USHORT port);
    static DWORD __stdcall SocksThread(LPVOID lparam);
    static DWORD __stdcall SocksThreadhostname(LPVOID lparam);
    DWORD	m_nSend;
    std::map<DWORD, SOCKET*> list;
    SOCKET* GetSocket(DWORD index,BOOL del=FALSE);
    CRITICAL_SECTION m_cs;
    int Threads;
    void AddThread(int n = 1) {
        CAutoLock L(m_cs);
        Threads += n;
    }
    void Wait() {
        while (GetThread())
            Sleep(50);
    }
    int GetThread() {
		CAutoLock L(m_cs);
        return Threads;
    }
};

struct SocksThreadArg {
    CProxyManager* pThis;
    LPBYTE lpBuffer;
    int len;
};
