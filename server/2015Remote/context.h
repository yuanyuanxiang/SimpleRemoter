#pragma once

#include "common/header.h"
#include "common/commands.h"
#include <atlstr.h>

enum {
    ONLINELIST_IP = 0,          // IP的列顺序
    ONLINELIST_ADDR,            // 地址
    ONLINELIST_LOCATION,        // 地理位置
    ONLINELIST_COMPUTER_NAME,   // 计算机名/备注
    ONLINELIST_OS,              // 操作系统
    ONLINELIST_CPU,             // CPU
    ONLINELIST_VIDEO,           // 摄像头(有无)
    ONLINELIST_PING,            // PING(对方的网速)
    ONLINELIST_VERSION,	        // 版本信息
    ONLINELIST_INSTALLTIME,     // 安装时间
    ONLINELIST_LOGINTIME,       // 活动窗口
    ONLINELIST_CLIENTTYPE,		// 客户端类型
    ONLINELIST_PATH,			// 文件路径
    ONLINELIST_PUBIP,
    ONLINELIST_MAX,
};

class context
{
public:
    // 纯虚函数
    virtual VOID InitMember(SOCKET s, VOID* svr) = 0;
    virtual BOOL Send2Client(PBYTE szBuffer, ULONG ulOriginalLength) = 0;
    virtual CString GetClientData(int index)const = 0;
    virtual void GetAdditionalData(CString(&s)[RES_MAX]) const = 0;
    virtual CString GetAdditionalData(int index) const = 0;
    virtual uint64_t GetClientID() const = 0;
    virtual std::string GetPeerName() const = 0;
    virtual int GetPort() const = 0;
    virtual std::string GetProtocol() const = 0;
    virtual int GetServerPort() const = 0;
    virtual FlagType GetFlagType() const = 0;
    virtual std::string GetGroupName() const = 0;
    virtual uint64_t GetAliveTime()const = 0;
public:
    virtual ~context() {}
    virtual void Destroy() {}
    virtual BOOL IsLogin() const
    {
        return TRUE;
    }
    virtual bool IsEqual(context* ctx) const
    {
        return this == ctx || this->GetPort() == ctx->GetPort();
    }
};
