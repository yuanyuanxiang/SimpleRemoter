#include "IOCPKCPClient.h"
#include <windows.h>
#include <chrono>
#include <iostream>

IOCPKCPClient::IOCPKCPClient(State& bExit, bool exit_while_disconnect)
    : IOCPUDPClient(bExit, exit_while_disconnect), kcp_(nullptr), running_(false)
{
}

IOCPKCPClient::~IOCPKCPClient()
{
    running_ = false;
    if (updateThread_.joinable())
        updateThread_.join();

    if (kcp_)
        ikcp_release(kcp_);
}

BOOL IOCPKCPClient::ConnectServer(const char* szServerIP, unsigned short uPort)
{
    BOOL ret = IOCPUDPClient::ConnectServer(szServerIP, uPort);
    if (!ret)
        return FALSE;

    // 初始化KCP
    uint32_t conv = KCP_SESSION_ID; // conv 要与服务端匹配
    kcp_ = ikcp_create(conv, this);
    if (!kcp_)
        return FALSE;

    // 设置KCP参数
    ikcp_nodelay(kcp_, 1, 40, 2, 0);
    kcp_->rx_minrto = 30;
    kcp_->snd_wnd = 128;
    kcp_->rcv_wnd = 128;

    // 设置发送回调函数（KCP发送数据时调用）
    kcp_->output = IOCPKCPClient::kcpOutput;

    running_ = true;
    updateThread_ = std::thread(&IOCPKCPClient::KCPUpdateLoop, this);
    m_bConnected = TRUE;

    return TRUE;
}

// UDP收包线程调用，将收到的UDP包送入KCP处理，再尝试读取完整应用包
int IOCPKCPClient::ReceiveData(char* buffer, int bufSize, int flags)
{
    // 先调用基类接收UDP原始数据
    char udpBuffer[1500] = { 0 };
    int recvLen = IOCPUDPClient::ReceiveData(udpBuffer, sizeof(udpBuffer), flags);
    if (recvLen <= 0)
        return recvLen;

    // 输入KCP协议栈
    int inputRet = ikcp_input(kcp_, udpBuffer, recvLen);
    if (inputRet < 0)
        return -1;

    // 从KCP中读取应用层数据，写入buffer
    int kcpRecvLen = ikcp_recv(kcp_, buffer, bufSize);
    return kcpRecvLen; // >0表示收到完整应用数据，0表示无完整包
}

bool IOCPKCPClient::ProcessRecvData(CBuffer* m_CompressedBuffer, char* szBuffer, int len, int flag)
{
    int iReceivedLength = ReceiveData(szBuffer, len, flag);
    if (iReceivedLength <= 0)
    {}
    else {
        szBuffer[iReceivedLength] = 0;
        //正确接收就调用OnRead处理,转到OnRead
        OnServerReceiving(m_CompressedBuffer, szBuffer, iReceivedLength);
    }
    return true;
}

// 发送应用层数据时调用，转发给KCP协议栈
int IOCPKCPClient::SendTo(const char* buf, int len, int flags)
{
    if (!kcp_)
        return -1;

    int ret = ikcp_send(kcp_, buf, len);
    if (ret < 0)
        return -1;

    // 主动调用flush，加快发送
    ikcp_flush(kcp_);
    return ret;
}

// KCP发送数据回调，将KCP生成的UDP包发送出去
int IOCPKCPClient::kcpOutput(const char* buf, int len, struct IKCPCB* kcp, void* user)
{
    IOCPKCPClient* client = reinterpret_cast<IOCPKCPClient*>(user);
    if (client->m_sClientSocket == INVALID_SOCKET)
        return -1;

    int sentLen = sendto(client->m_sClientSocket, buf, len, 0, (sockaddr*)&client->m_ServerAddr, sizeof(client->m_ServerAddr));
    if (sentLen == len)
        return 0;
    else
        return -1;
}

// 独立线程定时调用ikcp_update，保持KCP协议正常工作
void IOCPKCPClient::KCPUpdateLoop()
{
    while (running_ && !g_bExit) {
        IUINT32 current = GetTickCount64();
        ikcp_update(kcp_, current);
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 20ms周期，视需求调整
    }
}
