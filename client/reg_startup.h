#pragma once
#include <stdbool.h>

const char* GetInstallDirectory(const char* startupName);

typedef void (*StartupLogFunc)(const char* file, int line, const char* format, ...);

// return > 0 means to continue running else terminate.
int RegisterStartup(const char* startupName, const char* exeName, bool lockFile, bool runasAdmin, StartupLogFunc log);

// 检测当前进程是否以SYSTEM身份运行在Session 0, 返回true表示需要启动用户Session代理
bool IsSystemInSession0();
