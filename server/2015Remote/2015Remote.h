
// 2015Remote.h : PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"		// 主符号
#include "common/iniFile.h"
#include "IOCPServer.h"
#include "IOCPUDPServer.h"

// CMy2015RemoteApp:
// 有关此类的实现，请参阅 2015Remote.cpp
//

// ServerPair: 
// 一对SOCKET服务端，监听端口: 同时监听TCP和UDP.
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
		if (ret1) MessageBox(NULL, CString("启动TCP服务失败: ") + std::to_string(uPort).c_str() 
			+ CString("。错误码: ") + std::to_string(ret1).c_str(), "提示", MB_ICONINFORMATION);
		UINT ret2 = m_udpServer->StartServer(NotifyProc, OffProc, uPort);
		if (ret2) MessageBox(NULL, CString("启动UDP服务失败: ") + std::to_string(uPort).c_str()
			+ CString("。错误码: ") + std::to_string(ret2).c_str(), "提示", MB_ICONINFORMATION);
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
	// 配置文件读取器
	config* m_iniFile = nullptr;
	// 服务端容器列表
	std::vector<ServerPair*> m_iocpServer;
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

	// 启动多个服务端，成功返回0
	// nPort示例: 6543;7543
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

std::string GetMasterHash();

#define THIS_APP GetThisApp()

#define THIS_CFG GetThisCfg()
