#pragma once
#include "IOCPUDPClient.h"
#include "ikcp.h"
#include <thread>
#include <atomic>

class IOCPKCPClient : public IOCPUDPClient
{
public:
    IOCPKCPClient(State& bExit, bool exit_while_disconnect = false);
    virtual ~IOCPKCPClient();

    virtual BOOL ConnectServer(const char* szServerIP, unsigned short uPort) override;

    // ��д���պ���������UDP���ݸ�KCP�����KCP�����������
    virtual int ReceiveData(char* buffer, int bufSize, int flags) override;

    virtual bool ProcessRecvData(CBuffer* m_CompressedBuffer, char* szBuffer, int len, int flag) override;

    // ��д���ͺ�������Ӧ������ͨ��KCP����
    virtual int SendTo(const char* buf, int len, int flags) override;

private:
    // KCP�������ݵĻص��������������UDP��sendto
    static int kcpOutput(const char* buf, int len, struct IKCPCB* kcp, void* user);

    // ��ʱ����ikcp_update���̺߳���
    void KCPUpdateLoop();

private:
    ikcpcb* kcp_;
    std::thread updateThread_;
    std::atomic<bool> running_;
};
