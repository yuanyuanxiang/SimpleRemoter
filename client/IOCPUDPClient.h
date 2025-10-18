#pragma once
#include "IOCPClient.h"

class IOCPUDPClient : public IOCPClient
{
public:
    IOCPUDPClient(State& bExit, bool exit_while_disconnect = false):IOCPClient(bExit, exit_while_disconnect) {}

    virtual ~IOCPUDPClient() {}

    virtual BOOL ConnectServer(const char* szServerIP, unsigned short uPort) override;

    virtual int ReceiveData(char* buffer, int bufSize, int flags) override;

    virtual int SendTo(const char* buf, int len, int flags) override;
};
