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

    // ��ʼ��KCP
    uint32_t conv = KCP_SESSION_ID; // conv Ҫ������ƥ��
    kcp_ = ikcp_create(conv, this);
    if (!kcp_)
        return FALSE;

    // ����KCP����
    ikcp_nodelay(kcp_, 1, 40, 2, 0);
    kcp_->rx_minrto = 30;
    kcp_->snd_wnd = 128;
    kcp_->rcv_wnd = 128;

    // ���÷��ͻص�������KCP��������ʱ���ã�
    kcp_->output = IOCPKCPClient::kcpOutput;

    running_ = true;
    updateThread_ = std::thread(&IOCPKCPClient::KCPUpdateLoop, this);
    m_bConnected = TRUE;

    return TRUE;
}

// UDP�հ��̵߳��ã����յ���UDP������KCP�����ٳ��Զ�ȡ����Ӧ�ð�
int IOCPKCPClient::ReceiveData(char* buffer, int bufSize, int flags)
{
    // �ȵ��û������UDPԭʼ����
    char udpBuffer[1500] = { 0 };
    int recvLen = IOCPUDPClient::ReceiveData(udpBuffer, sizeof(udpBuffer), flags);
    if (recvLen <= 0)
        return recvLen;

    // ����KCPЭ��ջ
    int inputRet = ikcp_input(kcp_, udpBuffer, recvLen);
    if (inputRet < 0)
        return -1;

    // ��KCP�ж�ȡӦ�ò����ݣ�д��buffer
    int kcpRecvLen = ikcp_recv(kcp_, buffer, bufSize);
    return kcpRecvLen; // >0��ʾ�յ�����Ӧ�����ݣ�0��ʾ��������
}

bool IOCPKCPClient::ProcessRecvData(CBuffer* m_CompressedBuffer, char* szBuffer, int len, int flag)
{
    int iReceivedLength = ReceiveData(szBuffer, len, flag);
    if (iReceivedLength <= 0)
    {}
    else {
        szBuffer[iReceivedLength] = 0;
        //��ȷ���վ͵���OnRead����,ת��OnRead
        OnServerReceiving(m_CompressedBuffer, szBuffer, iReceivedLength);
    }
    return true;
}

// ����Ӧ�ò�����ʱ���ã�ת����KCPЭ��ջ
int IOCPKCPClient::SendTo(const char* buf, int len, int flags)
{
    if (!kcp_)
        return -1;

    int ret = ikcp_send(kcp_, buf, len);
    if (ret < 0)
        return -1;

    // ��������flush���ӿ췢��
    ikcp_flush(kcp_);
    return ret;
}

// KCP�������ݻص�����KCP���ɵ�UDP�����ͳ�ȥ
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

// �����̶߳�ʱ����ikcp_update������KCPЭ����������
void IOCPKCPClient::KCPUpdateLoop()
{
    while (running_ && !g_bExit) {
        IUINT32 current = GetTickCount64();
        ikcp_update(kcp_, current);
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 20ms���ڣ����������
    }
}
