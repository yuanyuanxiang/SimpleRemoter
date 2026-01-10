#include "file_upload.h"
#include <Windows.h>
#include <ShlObj.h>
#include <ExDisp.h>
#include <ShlGuid.h>
#include <atlbase.h>
#include <atlcom.h>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

static std::vector<std::string> GetDesktopSelectedFiles()
{
    CComPtr<IShellWindows> pShellWindows;
    if (FAILED(pShellWindows.CoCreateInstance(CLSID_ShellWindows)))
        return {};

    CComVariant vLoc(CSIDL_DESKTOP);
    CComVariant vEmpty;
    long lhwnd;
    CComPtr<IDispatch> pDisp;

    if (FAILED(pShellWindows->FindWindowSW(&vLoc, &vEmpty, SWC_DESKTOP, &lhwnd, SWFO_NEEDDISPATCH, &pDisp)))
        return {};

    CComQIPtr<IServiceProvider> pServiceProvider(pDisp);
    if (!pServiceProvider)
        return {};

    CComPtr<IShellBrowser> pShellBrowser;
    if (FAILED(pServiceProvider->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void**)&pShellBrowser)))
        return {};

    CComPtr<IShellView> pShellView;
    if (FAILED(pShellBrowser->QueryActiveShellView(&pShellView)))
        return {};

    CComPtr<IDataObject> pDataObject;
    if (FAILED(pShellView->GetItemObject(SVGIO_SELECTION, IID_IDataObject, (void**)&pDataObject)))
        return {};

    FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = {};

    if (FAILED(pDataObject->GetData(&fmt, &stg)))
        return {};

    std::vector<std::string> vecFiles;
    HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
    if (hDrop)
    {
        UINT nFiles = DragQueryFileA(hDrop, 0xFFFFFFFF, NULL, 0);
        for (UINT i = 0; i < nFiles; i++)
        {
            char szPath[MAX_PATH];
            if (DragQueryFileA(hDrop, i, szPath, MAX_PATH))
            {
                vecFiles.push_back(szPath);
            }
        }
        GlobalUnlock(stg.hGlobal);
    }
    ReleaseStgMedium(&stg);

    return vecFiles;
}

std::vector<std::string> GetForegroundSelectedFiles()
{
    HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool bNeedUninit = SUCCEEDED(hrInit);

    HWND hFore = GetForegroundWindow();

    // 检查是否是桌面
    HWND hDesktop = FindWindow("Progman", NULL);
    HWND hWorkerW = NULL;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (FindWindowEx(hwnd, NULL, "SHELLDLL_DefView", NULL))
        {
            *(HWND*)lParam = hwnd;
            return FALSE;
        }
        return TRUE;
        }, (LPARAM)&hWorkerW);

    if (hFore == hDesktop || hFore == hWorkerW) {
        if (bNeedUninit) CoUninitialize();
        return GetDesktopSelectedFiles();
    }

    // 检查是否是资源管理器窗口
    char szClass[256] = {};
    GetClassNameA(hFore, szClass, 256);

    if (strcmp(szClass, "CabinetWClass") != 0 && strcmp(szClass, "ExploreWClass") != 0) {
        if (bNeedUninit) CoUninitialize();
        return {};
    }

    // 获取该窗口的选中项
    CComPtr<IShellWindows> pShellWindows;
    if (FAILED(pShellWindows.CoCreateInstance(CLSID_ShellWindows))) {
        if (bNeedUninit) CoUninitialize();
        return {};
    } 

    std::vector<std::string> vecFiles;
    long nCount = 0;
    pShellWindows->get_Count(&nCount);

    for (long i = 0; i < nCount; i++)
    {
        CComVariant vIndex(i);
        CComPtr<IDispatch> pDisp;
        if (FAILED(pShellWindows->Item(vIndex, &pDisp)) || !pDisp)
            continue;

        CComQIPtr<IWebBrowserApp> pBrowser(pDisp);
        if (!pBrowser)
            continue;

        SHANDLE_PTR hWnd = 0;
        pBrowser->get_HWND(&hWnd);
        if ((HWND)hWnd != hFore)
            continue;

        CComPtr<IDispatch> pDoc;
        if (FAILED(pBrowser->get_Document(&pDoc)) || !pDoc)
            break;

        CComQIPtr<IShellFolderViewDual> pView(pDoc);
        if (!pView)
            break;

        CComPtr<FolderItems> pItems;
        if (FAILED(pView->SelectedItems(&pItems)) || !pItems)
            break;

        long nItems = 0;
        pItems->get_Count(&nItems);

        for (long j = 0; j < nItems; j++)
        {
            CComVariant vj(j);
            CComPtr<FolderItem> pItem;
            if (SUCCEEDED(pItems->Item(vj, &pItem)) && pItem)
            {
                CComBSTR bstrPath;
                if (SUCCEEDED(pItem->get_Path(&bstrPath)))
                {
                    // BSTR (宽字符) 转 多字节
                    int nLen = WideCharToMultiByte(CP_ACP, 0, bstrPath, -1, NULL, 0, NULL, NULL);
                    if (nLen > 0)
                    {
                        std::string strPath(nLen - 1, '\0');
                        WideCharToMultiByte(CP_ACP, 0, bstrPath, -1, &strPath[0], nLen, NULL, NULL);
                        vecFiles.push_back(strPath);
                    }
                }
            }
        }
        break;
    }

    if (bNeedUninit) CoUninitialize();

    return vecFiles;
}

// 将多个路径组合成单\0分隔的char数组
// 格式: "path1\0path2\0path3\0"
std::vector<char> BuildMultiStringPath(const std::vector<std::string>& paths)
{
    std::vector<char> result;

    for (const auto& path : paths) {
        result.insert(result.end(), path.begin(), path.end());
        result.push_back('\0');
    }

    return result;
}

// 从char数组解析出多个路径
std::vector<std::string> ParseMultiStringPath(const char* buffer, size_t size)
{
    std::vector<std::string> paths;

    const char* p = buffer;
    const char* end = buffer + size;

    while (p < end) {
        size_t len = strlen(p);
        if (len > 0) {
            paths.emplace_back(p, len);
        }
        p += len + 1;
    }

    return paths;
}
