
// 2015Remote.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������
#include "common/iniFile.h"
#include "IOCPServer.h"

// CMy2015RemoteApp:
// �йش����ʵ�֣������ 2015Remote.cpp
//

class CMy2015RemoteApp : public CWinApp
{
private:
	// �����ļ���ȡ��
	config* m_iniFile = nullptr;
	// ����������б�
	std::vector<Server*> m_iocpServer;
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

	// ����һ������ˣ��ɹ�����0
	UINT StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort) {
		auto svr = new IOCPServer();
		UINT ret = svr->StartServer(NotifyProc, OffProc, uPort);
		if (ret != 0) {
			SAFE_DELETE(svr);
			return ret;
		}
		m_iocpServer.push_back(svr);
		return 0;
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

#define THIS_APP GetThisApp()

#define THIS_CFG GetThisCfg()
