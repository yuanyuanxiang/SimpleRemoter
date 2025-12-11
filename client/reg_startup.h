#pragma once
#include <stdbool.h>

// return > 0 means to continue running else terminate.
int RegisterStartup(const char* startupName, const char* exeName, bool lockFile, bool runasAdmin);
