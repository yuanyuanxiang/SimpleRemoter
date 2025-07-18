
// 2015Remote.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������
#include "common/iniFile.h"
#include "IOCPServer.h"
#include "IOCPUDPServer.h"

// CMy2015RemoteApp:
// �йش����ʵ�֣������ 2015Remote.cpp
//

// ServerPair: 
// һ��SOCKET����ˣ������˿�: ͬʱ����TCP��UDP.
class ServerPair
{
private:
	Server* m_tcpServer;
	Server* m_udpServer;
public:
	ServerPair() : m_tcpServer(new IOCPServer), m_udpServer(new IOCPUDPServer) {}
	virtual ~ServerPair() { SAFE_DELETE(m_tcpServer); SAFE_DELETE(m_udpServer); }

	BOOL StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort) {
		UINT ret1 = m_tcpServer->StartServer(NotifyProc, OffProc, uPort);
		if (ret1) MessageBox(NULL, CString("����TCP����ʧ��: ") + std::to_string(uPort).c_str() 
			+ CString("��������: ") + std::to_string(ret1).c_str(), "��ʾ", MB_ICONINFORMATION);
		UINT ret2 = m_udpServer->StartServer(NotifyProc, OffProc, uPort);
		if (ret2) MessageBox(NULL, CString("����UDP����ʧ��: ") + std::to_string(uPort).c_str()
			+ CString("��������: ") + std::to_string(ret2).c_str(), "��ʾ", MB_ICONINFORMATION);
		return (ret1 == 0 || ret2 == 0);
	}

	void UpdateMaxConnection(int maxConn) {
		if (m_tcpServer) m_tcpServer->UpdateMaxConnection(maxConn);
		if (m_udpServer) m_udpServer->UpdateMaxConnection(maxConn);
	}

	void Destroy() {
		if (m_tcpServer) m_tcpServer->Destroy();
		if (m_udpServer) m_udpServer->Destroy();
	}

	void Disconnect(CONTEXT_OBJECT* ctx) {
		if (m_tcpServer) m_tcpServer->Disconnect(ctx);
		if (m_udpServer) m_udpServer->Disconnect(ctx);
	}
};

class CMy2015RemoteApp : public CWinApp
{
private:
	// �����ļ���ȡ��
	config* m_iniFile = nullptr;
	// ����������б�
	std::vector<ServerPair*> m_iocpServer;
	// ������
	HANDLE m_Mutex = nullptr;

public:
	CMy2015RemoteApp();

	CImageList m_pImageList_Large;  //ϵͳ��ͼ��
	CImageList m_pImageList_Small;	//ϵͳСͼ��

	virtual BOOL InitInstance();

	config* GetCfg() const {
		return m_iniFile;
	}

	// �����������ˣ��ɹ�����0
	// nPortʾ��: 6543;7543
	UINT StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, const std::string& uPort, int maxConn) {
		bool succeed = false;
		auto list = StringToVector(uPort, ';');
		for (int i=0; i<list.size(); ++i)
		{
			int port = std::atoi(list[i].c_str());
			auto svr = new ServerPair();
			BOOL ret = svr->StartServer(NotifyProc, OffProc, port);
			if (ret == FALSE) {
				SAFE_DELETE(svr);
				continue;
			}
			svr->UpdateMaxConnection(maxConn);
			succeed = true;
			m_iocpServer.push_back(svr);
		}

		return succeed ? 0 : -1;
	}

	// �ͷŷ���� SOCKET
	void Destroy() {
		for (int i=0; i<m_iocpServer.size(); ++i)
		{
			m_iocpServer[i]->Destroy();
		}
	}

	// �ͷŷ����ָ��
	void Delete() {
		for (int i = 0; i < m_iocpServer.size(); ++i)
		{
			SAFE_DELETE(m_iocpServer[i]);
		}
		m_iocpServer.clear();
	}

	// �������������
	void UpdateMaxConnection(int maxConn) {
		for (int i = 0; i < m_iocpServer.size(); ++i)
		{
			m_iocpServer[i]->UpdateMaxConnection(maxConn);
		}
	}

	DECLARE_MESSAGE_MAP()
	virtual int ExitInstance();
};

extern CMy2015RemoteApp theApp;

CMy2015RemoteApp* GetThisApp();

config& GetThisCfg();

std::string GetMasterHash();

#define THIS_APP GetThisApp()

#define THIS_CFG GetThisCfg()
