#pragma once
#include "stdafx.h"
#include "ScreenCapture.h"
#include "common/commands.h"

// 只要你安装了 Windows 8 SDK 或更高版本的 Windows SDK，编译器就能找到 dxgi1_2.h 并成功编译。
// 仅在 Windows 8 及更新版本上受支持
#include <dxgi1_2.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// author: ChatGPT
// update: 962914132@qq.com
// DXGI 1.2（IDXGIOutputDuplication）相比传统 GDI 截屏，性能提升通常在 3 倍到 10 倍之间，具体取决于硬件、分辨率和使用场景。
class ScreenCapturerDXGI : public ScreenCapture
{
private:
    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;
    IDXGIOutputDuplication* deskDupl = nullptr;
    ID3D11Texture2D* cpuTexture = nullptr;
    BYTE* m_NextBuffer = nullptr;

public:
    ScreenCapturerDXGI(BYTE algo, int gop = DEFAULT_GOP, BOOL all = FALSE) : ScreenCapture(32, algo, all)
    {
        m_GOP = gop;
        InitDXGI(all);
        Mprintf("Capture screen with DXGI: GOP= %d\n", m_GOP);
    }

    ~ScreenCapturerDXGI()
    {
        CleanupDXGI();

        SAFE_DELETE_ARRAY(m_FirstBuffer);
        SAFE_DELETE_ARRAY(m_NextBuffer);
        SAFE_DELETE_ARRAY(m_RectBuffer);
    }

    virtual BOOL UsingDXGI() const {
        return TRUE;
    }

    void InitDXGI(BOOL all)
    {
        m_iScreenX = 0;
        m_iScreenY = 0;
        // 1. 创建 D3D11 设备
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, nullptr, 0, D3D11_SDK_VERSION, &d3dDevice, nullptr, &d3dContext);

        IDXGIFactory1* pFactory = nullptr;
        IDXGIAdapter1* dxgiAdapter = nullptr;
        IDXGIOutput* dxgiOutput = nullptr;
        IDXGIOutput1* dxgiOutput1 = nullptr;

        // 2. 创建工厂
        CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory);
        if (!pFactory) return;

        do {
            // 3. 获取设备
            static UINT idx = 0;
            idx = pFactory->EnumAdapters1(idx, &dxgiAdapter) == DXGI_ERROR_NOT_FOUND ? 0 : idx;
            if (!dxgiAdapter) pFactory->EnumAdapters1(idx, &dxgiAdapter);
            if (!dxgiAdapter)break;

            // 4. 获取 DXGI 输出（屏幕）
            static UINT screen = 0;
            HRESULT r = dxgiAdapter->EnumOutputs(screen++, &dxgiOutput);
            if (r == DXGI_ERROR_NOT_FOUND && all) {
                screen = 0;
                idx ++;
                dxgiAdapter->Release();
                dxgiAdapter = nullptr;
                continue;
            }
            if (!dxgiOutput)break;

            // 5. 获取 DXGI 输出 1
            dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&dxgiOutput1);
            if (!dxgiOutput1)break;

            // 6. 创建 Desktop Duplication
            dxgiOutput1->DuplicateOutput(d3dDevice, &deskDupl);
            if (!deskDupl)break;

            // 7. 获取屏幕大小
            DXGI_OUTDUPL_DESC duplDesc;
            deskDupl->GetDesc(&duplDesc);
            m_ulFullWidth = duplDesc.ModeDesc.Width;
            m_ulFullHeight = duplDesc.ModeDesc.Height;

            // 8. 创建 CPU 访问纹理
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = m_ulFullWidth;
            desc.Height = m_ulFullHeight;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            d3dDevice->CreateTexture2D(&desc, NULL, &cpuTexture);

            // 9. 初始化 BITMAPINFO
            m_BitmapInfor_Full = (BITMAPINFO*)new char[sizeof(BITMAPINFO)];
            memset(m_BitmapInfor_Full, 0, sizeof(BITMAPINFO));
            m_BitmapInfor_Full->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            m_BitmapInfor_Full->bmiHeader.biWidth = m_ulFullWidth;
            m_BitmapInfor_Full->bmiHeader.biHeight = m_ulFullHeight;
            m_BitmapInfor_Full->bmiHeader.biPlanes = 1;
            m_BitmapInfor_Full->bmiHeader.biBitCount = 32;
            m_BitmapInfor_Full->bmiHeader.biCompression = BI_RGB;
            m_BitmapInfor_Full->bmiHeader.biSizeImage = m_ulFullWidth * m_ulFullHeight * 4;

            // 10. 分配屏幕缓冲区
            m_FirstBuffer = new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage + 1];
            m_NextBuffer = new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage + 1];
            m_RectBuffer = new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage * 2 + 12];

            break;
        } while (true);

        // 释放 DXGI 资源
        if (dxgiOutput1) dxgiOutput1->Release();
        if (dxgiOutput) dxgiOutput->Release();
        if (dxgiAdapter) dxgiAdapter->Release();
        if (pFactory) pFactory->Release();
    }

    bool IsInitSucceed() const
    {
        return cpuTexture;
    }

    void CleanupDXGI()
    {
        if (cpuTexture) cpuTexture->Release();
        if (deskDupl) deskDupl->Release();
        if (d3dContext) d3dContext->Release();
        if (d3dDevice) d3dDevice->Release();
    }

    LPBYTE GetFirstScreenData(ULONG* ulFirstScreenLength) override
    {
        int ret = CaptureFrame(m_FirstBuffer, ulFirstScreenLength, 1);
        if (ret)
            return nullptr;
        if (m_bAlgorithm == ALGORITHM_GRAY) {
            ToGray(1 + m_RectBuffer, 1 + m_RectBuffer, m_BitmapInfor_Full->bmiHeader.biSizeImage);
        }
        m_FirstBuffer[0] = TOKEN_FIRSTSCREEN;
        return m_FirstBuffer;
    }

    LPBYTE ScanNextScreen() override
    {
        ULONG ulNextScreenLength = 0;
        int ret = CaptureFrame(m_NextBuffer, &ulNextScreenLength, 0);
        if (ret)
            return nullptr;

        return m_NextBuffer;
    }

    virtual LPBYTE GetFirstBuffer() const
    {
        return m_FirstBuffer + 1;
    }

private:
    // 重新初始化 Desktop Duplication
    BOOL ReinitDuplication()
    {
        if (deskDupl) {
            deskDupl->Release();
            deskDupl = nullptr;
        }

        if (!d3dDevice) return FALSE;

        IDXGIDevice* dxgiDevice = nullptr;
        IDXGIAdapter* dxgiAdapter = nullptr;
        IDXGIOutput* dxgiOutput = nullptr;
        IDXGIOutput1* dxgiOutput1 = nullptr;

        HRESULT hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
        if (FAILED(hr)) return FALSE;

        hr = dxgiDevice->GetAdapter(&dxgiAdapter);
        dxgiDevice->Release();
        if (FAILED(hr)) return FALSE;

        hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
        dxgiAdapter->Release();
        if (FAILED(hr)) return FALSE;

        hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&dxgiOutput1);
        dxgiOutput->Release();
        if (FAILED(hr)) return FALSE;

        Sleep(100);

        hr = dxgiOutput1->DuplicateOutput(d3dDevice, &deskDupl);
        dxgiOutput1->Release();

        return SUCCEEDED(hr) && deskDupl;
    }
    int CaptureFrame(LPBYTE buffer, ULONG* frameSize, int reservedBytes)
    {
        if (!deskDupl) {
            if (!ReinitDuplication()) return -10;
        }

        // 1. 获取下一帧
        IDXGIResource* desktopResource = nullptr;
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        HRESULT hr = deskDupl->AcquireNextFrame(100, &frameInfo, &desktopResource);
        // 处理全屏切换导致的访问丢失
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            if (ReinitDuplication()) {
                hr = deskDupl->AcquireNextFrame(100, &frameInfo, &desktopResource);
            }
        }

        if (FAILED(hr)) {
            return -1;
        }

        // 2. 获取 ID3D11Texture2D
        ID3D11Texture2D* texture = nullptr;
        hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture);
        if (FAILED(hr)) {
            deskDupl->ReleaseFrame();
            return -2;
        }

        // 3. 复制到 CPU 纹理
        d3dContext->CopyResource(cpuTexture, texture);

        // 4. 释放 DXGI 资源
        deskDupl->ReleaseFrame();
        texture->Release();
        desktopResource->Release();

        // 5. 读取纹理数据
        D3D11_MAPPED_SUBRESOURCE mapped;
        hr = d3dContext->Map(cpuTexture, 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) {
            return -3;
        }

        // 6. 复制数据到缓冲区（垂直翻转）
        BYTE* pData = (BYTE*)mapped.pData;
        int rowSize = m_ulFullWidth * 4;  // 每行的字节数（RGBA）

        for (int y = 0; y < m_ulFullHeight; y++) {
            // 计算目标缓冲区的位置，从底部往上填充
            int destIndex = reservedBytes + (m_ulFullHeight - 1 - y) * rowSize;
            int srcIndex = y * mapped.RowPitch;  // Direct3D 纹理数据行偏移
            memcpy(buffer + destIndex, pData + srcIndex, rowSize);
        }

        // 7. 清理
        d3dContext->Unmap(cpuTexture, 0);

        *frameSize = m_ulFullWidth * m_ulFullHeight * 4;
        return 0;
    }
};
