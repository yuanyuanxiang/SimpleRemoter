#pragma once

#include <Windows.h>

// 是否使用全局键盘钩子
#define USING_KB_HOOK 1

#define GET_PROCESS_EASY(p)
#define GET_PROCESS(p, q)

typedef int (CALLBACK* Callback)(const char* record, void* user);

bool SetHook(Callback callback, void* user);

void ReleaseHook();
