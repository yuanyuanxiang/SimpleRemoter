#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include "../../common/commands.h"

extern "C" __declspec(dllexport) DWORD WINAPI run(LPVOID param)
{
	PluginParam* p = reinterpret_cast<PluginParam*>(param);

	char buf[200] = {};
	sprintf(buf, "主控地址: %s:%d", p->IP, p->Port);
	MessageBoxA(NULL, buf, "插件消息", MB_OK | MB_ICONINFORMATION);

	return 0;
}
