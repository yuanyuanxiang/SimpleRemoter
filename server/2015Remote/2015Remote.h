
// 2015Remote.h : PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"		// 主符号
#include "common/iniFile.h"
#include "IOCPServer.h"

// CMy2015RemoteApp:
// 有关此类的实现，请参阅 2015Remote.cpp
//

class CMy2015RemoteApp : public CWinApp
{
private:
	// 配置文件读取器
	config* m_iniFile = nullptr;
	// 服务端容器列表
	std::vector<Server*> m_iocpServer;
	// 互斥锁
	HANDLE m_Mutex = nullptr;

public:
	CMy2015RemoteApp();

	CImageList m_pImageList_Large;  //系统大图标
	CImageList m_pImageList_Small;	//系统小图标

	virtual BOOL InitInstance();

	config* GetCfg() const {
		return m_iniFile;
	}

	// 启动一个服务端，成功返回0
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

	// 释放服务端 SOCKET
	void Destroy() {
		for (int i=0; i<m_iocpServer.size(); ++i)
		{
			m_iocpServer[i]->Destroy();
		}
	}

	// 释放服务端指针
	void Delete() {
		for (int i = 0; i < m_iocpServer.size(); ++i)
		{
			SAFE_DELETE(m_iocpServer[i]);
		}
		m_iocpServer.clear();
	}

	// 更新最大连接数
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
