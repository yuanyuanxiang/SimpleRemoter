#pragma once
#include <windows.h>

// 提升权限
inline int DebugPrivilege()
{
	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return -1;

	// 动态分配空间，包含 3 个 LUID
	TOKEN_PRIVILEGES* tp = (TOKEN_PRIVILEGES*)malloc(sizeof(TOKEN_PRIVILEGES) + 2 * sizeof(LUID_AND_ATTRIBUTES));
	if (!tp) { CloseHandle(hToken); return 1; }

	tp->PrivilegeCount = 3;

	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp->Privileges[0].Luid)) { free(tp); CloseHandle(hToken); return 2; }
	tp->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if (!LookupPrivilegeValue(NULL, SE_INCREASE_QUOTA_NAME, &tp->Privileges[1].Luid)) { free(tp); CloseHandle(hToken); return 3; }
	tp->Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;

	if (!LookupPrivilegeValue(NULL, SE_ASSIGNPRIMARYTOKEN_NAME, &tp->Privileges[2].Luid)) { free(tp); CloseHandle(hToken); return 4; }
	tp->Privileges[2].Attributes = SE_PRIVILEGE_ENABLED;

	AdjustTokenPrivileges(hToken, FALSE, tp, sizeof(TOKEN_PRIVILEGES) + 2 * sizeof(LUID_AND_ATTRIBUTES), NULL, NULL);

	free(tp);
	CloseHandle(hToken);
	return 0;
}

/**
* @brief 设置本身开机自启动
* @param[in] *sPath 注册表的路径
* @param[in] *sNmae 注册表项名称
* @return 返回注册结果
* @details Win7 64位机器上测试结果表明，注册项在：\n
* HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Run
* @note 首次运行需要以管理员权限运行，才能向注册表写入开机启动项
*/
inline BOOL SetSelfStart(const char* sPath, const char* sNmae)
{
	DebugPrivilege();

	// 写入的注册表路径
#define REGEDIT_PATH "Software\\Microsoft\\Windows\\CurrentVersion\\Run\\"

	// 在注册表中写入启动信息
	HKEY hKey = NULL;
	LONG lRet = RegOpenKeyExA(HKEY_CURRENT_USER, REGEDIT_PATH, 0, KEY_ALL_ACCESS, &hKey);

	// 判断是否成功
	if (lRet != ERROR_SUCCESS)
		return FALSE;

	lRet = RegSetValueExA(hKey, sNmae, 0, REG_SZ, (const BYTE*)sPath, strlen(sPath) + 1);

	// 关闭注册表
	RegCloseKey(hKey);

	// 判断是否成功
	return lRet == ERROR_SUCCESS;
}
