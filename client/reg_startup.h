#pragma once
#include <stdbool.h>

typedef void (*StartupLogFunc)(const char* file, int line, const char* format, ...);

// return > 0 means to continue running else terminate.
int RegisterStartup(const char* startupName, const char* exeName, bool lockFile, bool runasAdmin, StartupLogFunc log);
