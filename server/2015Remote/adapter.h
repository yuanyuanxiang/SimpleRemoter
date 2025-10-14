#pragma once
#include <wtypes.h>

/******************************* WinOS RAT Adapter ****************************************/

#define TOKEN_GETVERSION 4
#define TOKEN_ACTIVED 202

enum SENDTASK {
    TASK_MAIN,
    TASK_PLUG,
};

struct DllSendData {
    SENDTASK sendtask;
    WCHAR DllName[255];			// DL名称
    BOOL is_64;					// 位数
    int DataSize;				// DLL大小
    WCHAR szVersion[50];		// 版本
    WCHAR szcommand[1000];
    int i;
};
