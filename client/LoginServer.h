#pragma once

#include "IOCPClient.h"
#include <Vfw.h>

#pragma comment(lib,"Vfw32.lib")

BOOL IsAuthKernel(std::string& str);
LOGIN_INFOR GetLoginInfo(DWORD dwSpeed, CONNECT_ADDRESS &conn, const std::string& expiredDate);
DWORD CPUClockMHz();
BOOL WebCamIsExist();
