#pragma once

#include "IOCPClient.h"
#include <Vfw.h>

#pragma comment(lib,"Vfw32.lib")

LOGIN_INFOR GetLoginInfo(DWORD dwSpeed, CONNECT_ADDRESS &conn);
DWORD CPUClockMHz();
BOOL WebCamIsExist();
