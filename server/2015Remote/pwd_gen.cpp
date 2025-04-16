#include "stdafx.h"
#include "pwd_gen.h"

#pragma comment(lib, "Advapi32.lib")

// 执行系统命令，获取硬件信息
std::string execCommand(const char* cmd) {
	// 设置管道，用于捕获命令的输出
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// 创建用于接收输出的管道
	HANDLE hStdOutRead, hStdOutWrite;
	if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
		Mprintf("CreatePipe failed with error: %d\n", GetLastError());
		return "ERROR";
	}

	// 设置启动信息
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	// 设置窗口隐藏
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = hStdOutWrite;  // 将标准输出重定向到管道

	// 创建进程
	if (!CreateProcess(
		NULL,          // 应用程序名称
		(LPSTR)cmd,    // 命令行
		NULL,          // 进程安全属性
		NULL,          // 线程安全属性
		TRUE,          // 是否继承句柄
		0,             // 创建标志
		NULL,          // 环境变量
		NULL,          // 当前目录
		&si,           // 启动信息
		&pi            // 进程信息
	)) {
		Mprintf("CreateProcess failed with error: %d\n", GetLastError());
		return "ERROR";
	}

	// 关闭写入端句柄
	CloseHandle(hStdOutWrite);

	// 读取命令输出
	char buffer[128];
	std::string result = "";
	DWORD bytesRead;
	while (ReadFile(hStdOutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
		result.append(buffer, bytesRead);
	}

	// 关闭读取端句柄
	CloseHandle(hStdOutRead);

	// 等待进程完成
	WaitForSingleObject(pi.hProcess, INFINITE);

	// 关闭进程和线程句柄
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	// 去除换行符和空格
	result.erase(remove(result.begin(), result.end(), '\n'), result.end());
	result.erase(remove(result.begin(), result.end(), '\r'), result.end());

	// 返回命令的输出结果
	return result;
}

// 获取硬件 ID（CPU + 主板 + 硬盘）
std::string getHardwareID() {
	std::string cpuID = execCommand("wmic cpu get processorid");
	std::string boardID = execCommand("wmic baseboard get serialnumber");
	std::string diskID = execCommand("wmic diskdrive get serialnumber");

	std::string combinedID = cpuID + "|" + boardID + "|" + diskID;
	return combinedID;
}

// 使用 SHA-256 计算哈希
std::string hashSHA256(const std::string& data) {
	HCRYPTPROV hProv;
	HCRYPTHASH hHash;
	BYTE hash[32];
	DWORD hashLen = 32;
	std::ostringstream result;

	if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
		CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {

		CryptHashData(hHash, (BYTE*)data.c_str(), data.length(), 0);
		CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);

		for (DWORD i = 0; i < hashLen; i++) {
			result << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
		}

		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
	}
	return result.str();
}

// 生成 16 字符的唯一设备 ID
std::string getFixedLengthID(const std::string& hash) {
	return hash.substr(0, 4) + "-" + hash.substr(4, 4) + "-" + hash.substr(8, 4) + "-" + hash.substr(12, 4);
}

std::string deriveKey(const std::string& password, const std::string& hardwareID) {
	return hashSHA256(password + " + " + hardwareID);
}
