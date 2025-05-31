#pragma once
#include "stdafx.h"
#include "ScreenCapture.h"
#include "common/commands.h"

// ֻҪ�㰲װ�� Windows 8 SDK ����߰汾�� Windows SDK�������������ҵ� dxgi1_2.h ���ɹ����롣
// ���� Windows 8 �����°汾����֧��
#include <dxgi1_2.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

// author: ChatGPT 
// update: 962914132@qq.com
// DXGI 1.2��IDXGIOutputDuplication����ȴ�ͳ GDI ��������������ͨ���� 3 ���� 10 ��֮�䣬����ȡ����Ӳ�����ֱ��ʺ�ʹ�ó�����
class ScreenCapturerDXGI : public ScreenCapture {
private:
	ID3D11Device* d3dDevice = nullptr;
	ID3D11DeviceContext* d3dContext = nullptr;
	IDXGIOutputDuplication* deskDupl = nullptr;
	ID3D11Texture2D* cpuTexture = nullptr;
	BYTE* m_NextBuffer = nullptr;

public:
	ScreenCapturerDXGI(BYTE algo, int gop = DEFAULT_GOP) : ScreenCapture(32, algo) {
		m_GOP = gop;
		InitDXGI();
		Mprintf("Capture screen with DXGI: GOP= %d\n", m_GOP);
	}

	~ScreenCapturerDXGI() {
		CleanupDXGI();

		SAFE_DELETE_ARRAY(m_FirstBuffer);
		SAFE_DELETE_ARRAY(m_NextBuffer);
		SAFE_DELETE_ARRAY(m_RectBuffer);
	}

	void InitDXGI() {
		// 1. ���� D3D11 �豸
		D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, nullptr, 0, D3D11_SDK_VERSION, &d3dDevice, nullptr, &d3dContext);

		IDXGIDevice * dxgiDevice = nullptr;
		IDXGIAdapter* dxgiAdapter = nullptr;
		IDXGIOutput* dxgiOutput = nullptr;
		IDXGIOutput1* dxgiOutput1 = nullptr;

		do {
			// 2. ��ȡ DXGI �豸
			d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
			if(!dxgiDevice)break;

			// 3. ��ȡ DXGI ������
			dxgiDevice->GetAdapter(&dxgiAdapter);
			if (!dxgiAdapter)break;

			// 4. ��ȡ DXGI �������Ļ��
			dxgiAdapter->EnumOutputs(0, &dxgiOutput);
			if (!dxgiOutput)break;

			// 5. ��ȡ DXGI ��� 1
			dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&dxgiOutput1);
			if (!dxgiOutput1)break;

			// 6. ���� Desktop Duplication
			dxgiOutput1->DuplicateOutput(d3dDevice, &deskDupl);
			if (!deskDupl)break;

			// 7. ��ȡ��Ļ��С
			DXGI_OUTDUPL_DESC duplDesc;
			deskDupl->GetDesc(&duplDesc);
			m_ulFullWidth = duplDesc.ModeDesc.Width;
			m_ulFullHeight = duplDesc.ModeDesc.Height;

			// 8. ���� CPU ��������
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

			// 9. ��ʼ�� BITMAPINFO
			m_BitmapInfor_Full = (BITMAPINFO*)new char[sizeof(BITMAPINFO)];
			memset(m_BitmapInfor_Full, 0, sizeof(BITMAPINFO));
			m_BitmapInfor_Full->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			m_BitmapInfor_Full->bmiHeader.biWidth = m_ulFullWidth;
			m_BitmapInfor_Full->bmiHeader.biHeight = m_ulFullHeight;
			m_BitmapInfor_Full->bmiHeader.biPlanes = 1;
			m_BitmapInfor_Full->bmiHeader.biBitCount = 32;
			m_BitmapInfor_Full->bmiHeader.biCompression = BI_RGB;
			m_BitmapInfor_Full->bmiHeader.biSizeImage = m_ulFullWidth * m_ulFullHeight * 4;

			// 10. ������Ļ������
			m_FirstBuffer = new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage + 1];
			m_NextBuffer = new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage + 1];
			m_RectBuffer = new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage * 2 + 12];
		} while (false);

		// �ͷ� DXGI ��Դ
		if (dxgiOutput1) dxgiOutput1->Release();
		if (dxgiOutput) dxgiOutput->Release();
		if (dxgiAdapter) dxgiAdapter->Release();
		if (dxgiDevice) dxgiDevice->Release();
	}

	bool IsInitSucceed() const {
		return cpuTexture;
	}

	void CleanupDXGI() {
		if (cpuTexture) cpuTexture->Release();
		if (deskDupl) deskDupl->Release();
		if (d3dContext) d3dContext->Release();
		if (d3dDevice) d3dDevice->Release();
	}

	LPBYTE GetFirstScreenData(ULONG* ulFirstScreenLength) override {
		int ret = CaptureFrame(m_FirstBuffer, ulFirstScreenLength, 1);
		if (ret)
			return nullptr;
		if (m_bAlgorithm == ALGORITHM_GRAY) {
			ToGray(1 + m_RectBuffer, 1 + m_RectBuffer, m_BitmapInfor_Full->bmiHeader.biSizeImage);
		}
		m_FirstBuffer[0] = TOKEN_FIRSTSCREEN;
		return m_FirstBuffer;
	}

	LPBYTE ScanNextScreen() override {
		ULONG ulNextScreenLength = 0;
		int ret = CaptureFrame(m_NextBuffer, &ulNextScreenLength, 0);
		if (ret)
			return nullptr;
		
		return m_NextBuffer;
	}

	virtual LPBYTE GetFirstBuffer() const {
		return m_FirstBuffer + 1;
	}

private:
	int CaptureFrame(LPBYTE buffer, ULONG* frameSize, int reservedBytes) {
		// 1. ��ȡ��һ֡
		IDXGIResource* desktopResource = nullptr;
		DXGI_OUTDUPL_FRAME_INFO frameInfo;
		HRESULT hr = deskDupl->AcquireNextFrame(100, &frameInfo, &desktopResource);
		if (FAILED(hr)) {
			return -1;
		}

		// 2. ��ȡ ID3D11Texture2D
		ID3D11Texture2D* texture = nullptr;
		hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture);
		if (FAILED(hr)) {
			deskDupl->ReleaseFrame();
			return -2;
		}

		// 3. ���Ƶ� CPU ����
		d3dContext->CopyResource(cpuTexture, texture);

		// 4. �ͷ� DXGI ��Դ
		deskDupl->ReleaseFrame();
		texture->Release();
		desktopResource->Release();

		// 5. ��ȡ��������
		D3D11_MAPPED_SUBRESOURCE mapped;
		hr = d3dContext->Map(cpuTexture, 0, D3D11_MAP_READ, 0, &mapped);
		if (FAILED(hr)) {
			return -3;
		}

		// 6. �������ݵ�����������ֱ��ת��
		BYTE* pData = (BYTE*)mapped.pData;
		int rowSize = m_ulFullWidth * 4;  // ÿ�е��ֽ�����RGBA��

		for (int y = 0; y < m_ulFullHeight; y++) {
			// ����Ŀ�껺������λ�ã��ӵײ��������
			int destIndex = reservedBytes + (m_ulFullHeight - 1 - y) * rowSize;
			int srcIndex = y * mapped.RowPitch;  // Direct3D ����������ƫ��
			memcpy(buffer + destIndex, pData + srcIndex, rowSize);
		}

		// 7. ����
		d3dContext->Unmap(cpuTexture, 0);

		*frameSize = m_ulFullWidth * m_ulFullHeight * 4;
		return 0;
	}
};
