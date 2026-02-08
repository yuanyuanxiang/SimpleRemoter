#pragma once

#include <map>
#include <string>
#include <vector>
#include <afxwin.h>
#include "common/IniParser.h"

// 语言管理类 - 支持多语言切换
class CLangManager
{
private:
    std::map<CString, CString> m_strings;  // 中文 -> 目标语言
    CString m_currentLang;                  // 当前语言代码
    CString m_langDir;                      // 语言文件目录

    CLangManager() {}
    CLangManager(const CLangManager&) = delete;
    CLangManager& operator=(const CLangManager&) = delete;

public:
    static CLangManager& Instance()
    {
        static CLangManager instance;
        return instance;
    }

    // 初始化语言目录
    void Init(const CString& langDir = _T(""))
    {
        if (langDir.IsEmpty()) {
            // 默认使用 exe 所在目录下的 lang 文件夹
            TCHAR path[MAX_PATH];
            GetModuleFileName(NULL, path, MAX_PATH);
            CString exePath(path);
            int pos = exePath.ReverseFind(_T('\\'));
            if (pos > 0) {
                m_langDir = exePath.Left(pos) + _T("\\lang");
            }
        } else {
            m_langDir = langDir;
        }

        // 确保目录存在
        CreateDirectory(m_langDir, NULL);
    }

    // 获取可用的语言列表
    std::vector<CString> GetAvailableLanguages()
    {
        std::vector<CString> langs;
        CString searchPath = m_langDir + _T("\\*.ini");

        WIN32_FIND_DATA fd;
        HANDLE hFind = FindFirstFile(searchPath, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                CString filename(fd.cFileName);
                int dotPos = filename.ReverseFind(_T('.'));
                if (dotPos > 0) {
                    langs.push_back(filename.Left(dotPos));
                }
            } while (FindNextFile(hFind, &fd));
            FindClose(hFind);
        }
        return langs;
    }

    // 检查语言文件编码是否为 ANSI
    // 返回 false 表示文件不存在或编码不是 ANSI（检测 BOM 和 UTF-8 无 BOM）
    bool CheckEncoding(const CString& langCode)
    {
        if (langCode == _T("zh_CN") || langCode.IsEmpty()) {
            TRACE("[LangEnc] zh_CN or empty, skip check\n");
            return true;
        }

        CString langFile = m_langDir + _T("\\") + langCode + _T(".ini");
        TRACE("[LangEnc] Checking: %s\n", (LPCSTR)langFile);

        FILE* f = nullptr;
        if (fopen_s(&f, (LPCSTR)langFile, "rb") != 0 || !f) {
            TRACE("[LangEnc] fopen failed\n");
            return false;
        }

        // 读取文件内容（最多检测前 4KB 即可判断）
        unsigned char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf), f);
        fclose(f);
        TRACE("[LangEnc] Read %zu bytes\n", n);

        if (n == 0) return false;

        // 检测 BOM
        if (n >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
            TRACE("[LangEnc] Detected UTF-8 BOM\n");
            return false;
        }
        if (n >= 2 && buf[0] == 0xFF && buf[1] == 0xFE) {
            TRACE("[LangEnc] Detected UTF-16 LE BOM\n");
            return false;
        }
        if (n >= 2 && buf[0] == 0xFE && buf[1] == 0xFF) {
            TRACE("[LangEnc] Detected UTF-16 BE BOM\n");
            return false;
        }

        // 检测 UTF-8 无 BOM：扫描是否存在合法的 UTF-8 多字节序列
        // 中文 UTF-8 为 3 字节 (E0-EF + 80-BF + 80-BF)
        // GBK 为 2 字节 (81-FE + 40-FE)，字节模式不同
        int utf8SeqCount = 0;
        for (size_t i = 0; i < n; ) {
            unsigned char c = buf[i];
            if (c < 0x80) {
                i++;
            } else if ((c & 0xE0) == 0xC0 && i + 1 < n
                       && (buf[i+1] & 0xC0) == 0x80) {
                utf8SeqCount++;  // 2 字节 UTF-8
                i += 2;
            } else if ((c & 0xF0) == 0xE0 && i + 2 < n
                       && (buf[i+1] & 0xC0) == 0x80
                       && (buf[i+2] & 0xC0) == 0x80) {
                utf8SeqCount++;  // 3 字节 UTF-8（中文）
                i += 3;
            } else if ((c & 0xF8) == 0xF0 && i + 3 < n
                       && (buf[i+1] & 0xC0) == 0x80
                       && (buf[i+2] & 0xC0) == 0x80
                       && (buf[i+3] & 0xC0) == 0x80) {
                utf8SeqCount++;  // 4 字节 UTF-8
                i += 4;
            } else {
                // 高字节不符合 UTF-8 规则
                // 但如果在缓冲区末尾，可能是多字节序列被截断，不应误判
                if (i + 3 >= n && c >= 0xC0) {
                    TRACE("[LangEnc] Truncated at offset %zu: 0x%02X, skip\n", i, c);
                    break;  // 缓冲区尾部截断，跳出循环按已有结果判断
                }
                // 确实是 ANSI/GBK
                TRACE("[LangEnc] GBK byte at offset %zu: 0x%02X → ANSI\n", i, c);
                return true;
            }
        }

        TRACE("[LangEnc] utf8SeqCount=%d → %s\n", utf8SeqCount,
              utf8SeqCount > 0 ? "UTF-8 (not ANSI)" : "pure ASCII (ANSI)");
        // 存在多字节序列且全部符合 UTF-8 规则 → 判定为 UTF-8
        return (utf8SeqCount == 0);
    }

    // 加载语言文件
    bool Load(const CString& langCode)
    {
        m_strings.clear();
        m_currentLang = langCode;

        // 如果是中文，不需要加载翻译
        if (langCode == _T("zh_CN") || langCode.IsEmpty()) {
            return true;
        }

        CString langFile = m_langDir + _T("\\") + langCode + _T(".ini");

        // 检查文件是否存在
        if (GetFileAttributes(langFile) == INVALID_FILE_ATTRIBUTES) {
            return false;
        }

        // 使用 CIniParser 解析，无文件大小限制，且不 trim key
        CIniParser ini;
        if (!ini.LoadFile((LPCSTR)langFile)) {
            return false;
        }

        const CIniParser::TKeyVal* pSection = ini.GetSection("Strings");
        if (pSection) {
            for (const auto& kv : *pSection) {
                m_strings[CString(kv.first.c_str())] = CString(kv.second.c_str());
            }
        }

        return true;
    }

    // 获取翻译字符串
    // key: 中文原文
    // 返回: 翻译后的文本，如果没有翻译则返回原文
    CString Get(const CString& key)
    {
        if (m_currentLang == _T("zh_CN") || m_currentLang.IsEmpty()) {
            return key;  // 中文直接返回
        }

        auto it = m_strings.find(key);
        if (it != m_strings.end()) {
            return it->second;
        }
        return key;  // 没有翻译则返回原文
    }

    // 获取翻译字符串 (std::string 版本)
    std::string Get(const std::string& key)
    {
        CString result = Get(CString(key.c_str()));
        CT2A ansi(result);
        return std::string(ansi);
    }

    // 获取当前语言
    CString GetCurrentLanguage() const
    {
        return m_currentLang;
    }

    // 是否为中文模式（无需翻译）
    bool IsChinese() const
    {
        return m_currentLang.IsEmpty() || m_currentLang == _T("zh_CN");
    }

    // 获取可用语言数量（包括内置的简体中文）
    // 返回 1 表示只有简体中文，无其他语言文件
    size_t GetLanguageCount()
    {
        auto langs = GetAvailableLanguages();
        // 检查是否有 zh_CN.ini 文件
        bool hasZhCN = false;
        for (const auto& lang : langs) {
            if (lang == _T("zh_CN")) {
                hasZhCN = true;
                break;
            }
        }
        // 简体中文始终存在，若 zh_CN.ini 不存在则额外 +1
        return hasZhCN ? langs.size() : langs.size() + 1;
    }
};

// 全局访问宏
#define g_Lang CLangManager::Instance()

// 翻译宏 - 用于代码中的字符串字面量
// 用法: _TR("中文字符串")
#define _TR(str) g_Lang.Get(CString(_T(str)))

// 翻译宏 - 用于格式化函数 (sprintf_s, _stprintf_s 等可变参数函数)
// 用法: _stprintf_s(buf, _TRF("连接 %s 失败"), ip);
#define _TRF(str) ((LPCTSTR)_TR(str))

// 翻译函数 - 用于 CString 变量或 LPCTSTR
// 用法: _L(strVar) 或 _L(_T("中文"))
inline CString _L(const CString& str)
{
    return g_Lang.Get(str);
}
inline CString _L(LPCTSTR str)
{
    return g_Lang.Get(CString(str));
}

// 翻译宏 - 用于格式化函数中的变量 (返回 LPCTSTR)
// 用法: _stprintf_s(buf, _LF(strVar), arg);
// 注意: 必须是宏，函数版本会导致悬空指针
#define _LF(str) ((LPCTSTR)_L(str))

// CString::Format 的多语言版本 (用于全局替换 .FormatL)
// 用法: str.FormatLL("连接 %s 失败", ip);
// 展开: str.FormatL(_TR("连接 %s 失败"), ip);
// 注意: 不需要翻译的字符串也可以用，找不到翻译会返回原文
#define FormatL(fmt, ...) Format(_TR(fmt), __VA_ARGS__)

// ============================================
// 带自动翻译的 MFC 函数宏 (L 后缀 = Language)
// ============================================

// MessageBox 系列
// MFC 成员函数版本 (CWnd::MessageBox)
#define MessageBoxL(text, caption, type) \
    MessageBox(_TR(text), _TR(caption), type)

// 全局 API 版本 (::MessageBox / ::MessageBoxA / ::MessageBoxW)
#define MessageBoxAPI_L(hwnd, text, caption, type) \
    ::MessageBox(hwnd, _TR(text), _TR(caption), type)

// 简写：hwnd 为 NULL 时
#define MsgBoxL(text, caption, type) \
    ::MessageBox(NULL, _TR(text), _TR(caption), type)

#define AfxMessageBoxL(text, type) \
    AfxMessageBox(_TR(text), type)

// SetWindowText / SetDlgItemText
#define SetWindowTextL(text) \
    SetWindowText(_TR(text))

#define SetDlgItemTextL(id, text) \
    SetDlgItemText(id, _TR(text))

// 列表控件
#define InsertColumnL(index, text, format, width) \
    InsertColumn(index, _TR(text), format, width)

#define InsertItemL(index, text) \
    InsertItem(index, _TR(text))

#define SetItemTextL(item, subitem, text) \
    SetItemText(item, subitem, _TR(text))

// ComboBox / ListBox
#define AddStringL(text) \
    AddString(_TR(text))

#define InsertStringL(index, text) \
    InsertString(index, _TR(text))

// Tab 控件
#define InsertTabItemL(index, text) \
    InsertItem(index, _TR(text))

// 状态栏
#define SetPaneTextL(index, text) \
    SetPaneText(index, _TR(text))

// 菜单
#define AppendMenuL(flags, id, text) \
    AppendMenu(flags, id, _TR(text))

#define AppendMenuSeparator(p) \
    AppendMenu(p)

#define InsertMenuL(pos, flags, id, text) \
    InsertMenu(pos, flags, id, _TR(text))

#define ModifyMenuL(pos, flags, id, text) \
    ModifyMenu(pos, flags, id, _TR(text))

// 翻译对话框所有控件
inline void TranslateDialog(CWnd* pWnd)
{
    if (g_Lang.IsChinese()) {
        return;  // 中文模式不需要翻译
    }

    if (!pWnd || !pWnd->GetSafeHwnd()) {
        return;
    }

    // 翻译对话框标题
    CString title;
    pWnd->GetWindowText(title);
    if (!title.IsEmpty()) {
        CString newTitle = g_Lang.Get(title);
        if (newTitle != title) {
            pWnd->SetWindowText(newTitle);
        }
    }

    // 遍历所有子控件
    CWnd* pChild = pWnd->GetWindow(GW_CHILD);
    while (pChild) {
        // 获取控件文本
        CString text;
        pChild->GetWindowText(text);

        if (!text.IsEmpty()) {
            CString newText = g_Lang.Get(text);
            if (newText != text) {
                pChild->SetWindowText(newText);
            }
        }

        // 如果是菜单按钮或有子菜单，也需要处理
        // 递归处理子窗口（如 GroupBox 内的控件）
        if (pChild->GetWindow(GW_CHILD)) {
            TranslateDialog(pChild);
        }

        pChild = pChild->GetNextWindow();
    }
}

// 翻译菜单
inline void TranslateMenu(CMenu* pMenu)
{
    if (!pMenu || !pMenu->GetSafeHmenu() || g_Lang.IsChinese()) {
        return;
    }

    UINT count = pMenu->GetMenuItemCount();
    for (UINT i = 0; i < count; i++) {
        CString text;
        pMenu->GetMenuString(i, text, MF_BYPOSITION);
        if (!text.IsEmpty()) {
            CString newText = g_Lang.Get(text);
            if (newText != text) {
                // 保留快捷键部分 (Tab 后的内容，如 Ctrl+S)
                int tabPos = text.Find(_T('\t'));
                if (tabPos > 0) {
                    CString shortcut = text.Mid(tabPos);
                    int newTabPos = newText.Find(_T('\t'));
                    if (newTabPos < 0) {
                        newText += shortcut;
                    }
                }

                // 检查是否是弹出菜单（有子菜单）
                CMenu* pSubMenu = pMenu->GetSubMenu(i);
                if (pSubMenu) {
                    // 弹出菜单使用 MF_POPUP
                    pMenu->ModifyMenu(i, MF_BYPOSITION | MF_POPUP | MF_STRING,
                                      (UINT_PTR)pSubMenu->GetSafeHmenu(), newText);
                } else {
                    // 普通菜单项
                    pMenu->ModifyMenu(i, MF_BYPOSITION | MF_STRING,
                                      pMenu->GetMenuItemID(i), newText);
                }
            }
        }

        // 递归处理子菜单
        CMenu* pSubMenu = pMenu->GetSubMenu(i);
        if (pSubMenu) {
            TranslateMenu(pSubMenu);
        }
    }
}

// 加载菜单并翻译 (用于 LoadMenu 动态加载菜单后自动翻译)
inline BOOL LoadMenuL(CMenu& menu, UINT nIDResource)
{
    if (!menu.LoadMenu(nIDResource)) return FALSE;
    TranslateMenu(&menu);
    return TRUE;
}

inline BOOL LoadMenuL(CMenu* pMenu, UINT nIDResource)
{
    if (!pMenu || !pMenu->LoadMenu(nIDResource)) return FALSE;
    TranslateMenu(pMenu);
    return TRUE;
}

// 翻译列表控件表头
inline void TranslateListHeader(CListCtrl* pList)
{
    if (!pList || g_Lang.IsChinese()) {
        return;
    }

    CHeaderCtrl* pHeader = pList->GetHeaderCtrl();
    if (!pHeader) {
        return;
    }

    int count = pHeader->GetItemCount();
    for (int i = 0; i < count; i++) {
        HDITEM hdi;
        TCHAR text[256] = { 0 };
        hdi.mask = HDI_TEXT;
        hdi.pszText = text;
        hdi.cchTextMax = 256;

        if (pHeader->GetItem(i, &hdi)) {
            CString newText = g_Lang.Get(CString(text));
            if (newText != text) {
                hdi.pszText = (LPTSTR)(LPCTSTR)newText;
                pHeader->SetItem(i, &hdi);
            }
        }
    }
}

// 支持多语言的对话框基类 (基于 CDialog)
// 用法: 将 class CMyDlg : public CDialog 改为 class CMyDlg : public CDialogLang
class CDialogLang : public CDialog
{
public:
    CDialogLang() {}

    CDialogLang(UINT nIDTemplate, CWnd* pParent = NULL)
        : CDialog(nIDTemplate, pParent) {}

    CDialogLang(LPCTSTR lpszTemplateName, CWnd* pParent = NULL)
        : CDialog(lpszTemplateName, pParent) {}

protected:
    virtual BOOL OnInitDialog() override
    {
        BOOL ret = __super::OnInitDialog();
        TranslateDialog(this);
        TranslateMenu(GetMenu());  // 自动翻译菜单
        return ret;
    }
};

// 支持多语言的对话框基类 (基于 CDialogEx)
// 用法: 将 class CMyDlg : public CDialogEx 改为 class CMyDlg : public CDialogLangEx
class CDialogLangEx : public CDialogEx
{
public:
    CDialogLangEx(UINT nIDTemplate, CWnd* pParent = NULL)
        : CDialogEx(nIDTemplate, pParent) {}

    CDialogLangEx(LPCTSTR lpszTemplateName, CWnd* pParent = NULL)
        : CDialogEx(lpszTemplateName, pParent) {}

protected:
    virtual BOOL OnInitDialog() override
    {
        BOOL ret = __super::OnInitDialog();
        TranslateDialog(this);
        TranslateMenu(GetMenu());  // 自动翻译菜单
        return ret;
    }
};

// ============================================
// 语言选择对话框（动态创建，无需 RC 资源）
// ============================================
class CLangSelectDlg : public CDialog
{
public:
    CString m_strSelectedLang;
    CComboBox m_comboLang;

    CLangSelectDlg(CWnd* pParent = NULL) : CDialog(), m_pParent(pParent) {}

    virtual INT_PTR DoModal() override
    {
        InitModalIndirect(CreateDialogTemplate(), m_pParent);
        return CDialog::DoModal();
    }

    // 静态方法：显示对话框并返回选择的语言代码
    // 返回空字符串表示用户取消
    static CString Show(CWnd* pParent = NULL)
    {
        CLangSelectDlg dlg(pParent);
        if (dlg.DoModal() == IDOK) {
            return dlg.m_strSelectedLang;
        }
        return _T("");
    }

protected:
    CWnd* m_pParent;
    std::vector<BYTE> m_templateBuffer;
    std::vector<CString> m_langCodes;

    // 语言代码到显示名称的映射
    static CString GetLanguageDisplayName(const CString& langCode)
    {
        if (langCode == _T("zh_CN")) return _T("简体中文");
        if (langCode == _T("zh_TW")) return _T("繁體中文");
        if (langCode == _T("en_US")) return _T("English");
        return langCode;
    }

    LPCDLGTEMPLATE CreateDialogTemplate()
    {
        const WORD DLG_WIDTH = 200;
        const WORD DLG_HEIGHT = 75;

        m_templateBuffer.clear();

        DLGTEMPLATE dlgTemplate = { 0 };
        dlgTemplate.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
        dlgTemplate.cdit = 4;
        dlgTemplate.cx = DLG_WIDTH;
        dlgTemplate.cy = DLG_HEIGHT;

        AppendData(&dlgTemplate, sizeof(DLGTEMPLATE));
        AppendWord(0);  // 菜单
        AppendWord(0);  // 窗口类
        AppendString(_T("选择语言 / Select Language"));
        AlignToDword();

        // 静态文本
        AddControl(0x0082, 15, 15, 40, 12, (WORD)-1,
                   SS_LEFT | WS_CHILD | WS_VISIBLE, _T("语言:"));

        // ComboBox
        AddControl(0x0085, 55, 13, 130, 150, 1001,
                   CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL, _T(""));

        // 确定按钮
        AddControl(0x0080, 45, 50, 50, 14, IDOK,
                   BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP, _T("确定"));

        // 取消按钮
        AddControl(0x0080, 105, 50, 50, 14, IDCANCEL,
                   BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP, _T("取消"));

        return (LPCDLGTEMPLATE)m_templateBuffer.data();
    }

    void AppendData(const void* data, size_t size)
    {
        const BYTE* p = (const BYTE*)data;
        m_templateBuffer.insert(m_templateBuffer.end(), p, p + size);
    }

    void AppendWord(WORD w)
    {
        AppendData(&w, sizeof(WORD));
    }

    void AppendString(LPCTSTR str)
    {
#ifdef UNICODE
        AppendData(str, (_tcslen(str) + 1) * sizeof(WCHAR));
#else
        int len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
        std::vector<WCHAR> wstr(len);
        MultiByteToWideChar(CP_ACP, 0, str, -1, wstr.data(), len);
        AppendData(wstr.data(), len * sizeof(WCHAR));
#endif
    }

    void AlignToDword()
    {
        while (m_templateBuffer.size() % 4 != 0)
            m_templateBuffer.push_back(0);
    }

    void AddControl(WORD classAtom, short x, short y, short cx, short cy,
                    WORD id, DWORD style, LPCTSTR text)
    {
        AlignToDword();
        DLGITEMTEMPLATE item = { 0 };
        item.style = style;
        item.x = x;
        item.y = y;
        item.cx = cx;
        item.cy = cy;
        item.id = id;
        AppendData(&item, sizeof(DLGITEMTEMPLATE));
        AppendWord(0xFFFF);
        AppendWord(classAtom);
        AppendString(text);
        AppendWord(0);
    }

    virtual BOOL OnInitDialog() override
    {
        CDialog::OnInitDialog();

        // 翻译对话框控件（标题、标签、按钮）
        TranslateDialog(this);

        m_comboLang.SubclassDlgItem(1001, this);

        // 添加简体中文
        int idx = m_comboLang.AddString(_T("简体中文"));
        m_langCodes.push_back(_T("zh_CN"));
        m_comboLang.SetItemData(idx, 0);

        // 添加其他语言
        auto langs = g_Lang.GetAvailableLanguages();
        for (const auto& lang : langs) {
            if (lang == _T("zh_CN")) continue;
            CString displayName = GetLanguageDisplayName(lang);
            idx = m_comboLang.AddString(displayName);
            m_comboLang.SetItemData(idx, m_langCodes.size());
            m_langCodes.push_back(lang);
        }

        // 选中当前语言
        CString currentLang = g_Lang.GetCurrentLanguage();
        if (currentLang.IsEmpty()) currentLang = _T("zh_CN");
        for (size_t i = 0; i < m_langCodes.size(); i++) {
            if (m_langCodes[i] == currentLang) {
                m_comboLang.SetCurSel((int)i);
                break;
            }
        }

        CenterWindow();
        return TRUE;
    }

    virtual void OnOK() override
    {
        int sel = m_comboLang.GetCurSel();
        if (sel >= 0) {
            size_t idx = (size_t)m_comboLang.GetItemData(sel);
            if (idx < m_langCodes.size()) {
                m_strSelectedLang = m_langCodes[idx];
            }
        }
        CDialog::OnOK();
    }
};
