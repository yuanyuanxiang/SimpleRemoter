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

    // 重写接收函数：输入UDP数据给KCP，输出KCP层解包后的数据
    virtual int ReceiveData(char* buffer, int bufSize, int flags) override;

    virtual bool ProcessRecvData(CBuffer* m_CompressedBuffer, char* szBuffer, int len, int flag) override;

    // 重写发送函数：将应用数据通过KCP发送
    virtual int SendTo(const char* buf, int len, int flags) override;

private:
    // KCP发送数据的回调函数，负责调用UDP的sendto
    static int kcpOutput(const char* buf, int len, struct IKCPCB* kcp, void* user);

    // 定时调用ikcp_update的线程函数
    void KCPUpdateLoop();

private:
    ikcpcb* kcp_;
    std::thread updateThread_;
    std::atomic<bool> running_;
};
