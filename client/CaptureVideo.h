// CaptureVideo.h: interface for the CCaptureVideo class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CAPTUREVIDEO_H__0984BB8E_6DCB_4A5C_8E03_1217AE6E409D__INCLUDED_)
#define AFX_CAPTUREVIDEO_H__0984BB8E_6DCB_4A5C_8E03_1217AE6E409D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include <Objbase.h>
#include <uuids.h>
#include <strmif.h>
#include <CONTROL.H>
#include <ATLBASE.H>
#include <amvideo.h>
#include <DShow.h>

#pragma comment(lib,"Strmiids.lib")

// TODO 全局变量, 定义位置：qedit.h

// 接口 ID	回调接口，用于在每一帧抓取时通知应用
EXTERN_C const IID IID_ISampleGrabberCB;

// 类 ID	创建 Sample Grabber COM 对象
EXTERN_C const CLSID CLSID_SampleGrabber;

// 	接口 ID	设置 Sample Grabber 的参数、格式、回调等操作接口
EXTERN_C const IID IID_ISampleGrabber;

struct ISampleGrabberCB : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE SampleCB(
        double SampleTime,
        IMediaSample * pSample) = 0;

    virtual HRESULT STDMETHODCALLTYPE BufferCB(
        double SampleTime,
        BYTE* pBuffer,
        long BufferLen) = 0;
};

struct ISampleGrabber : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(
        BOOL OneShot) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetMediaType(
        const AM_MEDIA_TYPE* pType) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(
        AM_MEDIA_TYPE* pType) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(
        BOOL BufferThem) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(
        /* [out][in] */ long* pBufferSize,
        /* [out] */ long* pBuffer) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(
        /* [retval][out] */ IMediaSample** ppSample) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetCallback(
        ISampleGrabberCB* pCallback,
        long WhichMethodToCallback) = 0;

};

enum {
    CMD_CAN_COPY,
    CMD_CAN_SEND
};

#ifndef SAFE_RELEASE
#define SAFE_RELEASE( x ) if ( NULL != x ){ x->Release(); x = NULL; }
#endif

class CSampleGrabberCB : public ISampleGrabberCB
{
public:
    ULONG  m_ulFullWidth;
    ULONG  m_ulFullHeight;
    LPBITMAPINFO m_BitmapInfor_Full;
    BYTE*        m_BitmapData_Full;
    BOOL         bStact;
    DWORD        m_dwSize; // 视频图像数据大小

    CSampleGrabberCB()
    {
        m_ulFullWidth = 0 ;
        m_ulFullHeight = 0 ;
        m_BitmapInfor_Full = NULL;
        m_BitmapData_Full = NULL;

        m_dwSize = 0;
        bStact  = CMD_CAN_COPY;
    }

    ~CSampleGrabberCB()
    {
        if (m_BitmapInfor_Full!=NULL) {
            delete[] m_BitmapInfor_Full;
        }

        if (m_BitmapData_Full!=NULL) {
            delete[] m_BitmapData_Full;
        }

        m_ulFullWidth = 0 ;
        m_ulFullHeight = 0 ;
    }

    LPBITMAPINFO GetBmpInfor()
    {
        if (m_BitmapInfor_Full==NULL) { //头信息
            ConstructBI(24);
        }

        return m_BitmapInfor_Full;
    }

    LPBITMAPINFO ConstructBI(ULONG ulbiBitCount)
    {
        int	ColorNum = ulbiBitCount <= 8 ? 1 << ulbiBitCount : 0;
        ULONG ulBitmapLength  = sizeof(BITMAPINFOHEADER) + (ColorNum * sizeof(RGBQUAD));   //BITMAPINFOHEADER +　调色板的个数

        m_BitmapInfor_Full = (BITMAPINFO *) new BYTE[ulBitmapLength];

        BITMAPINFOHEADER* BitmapInforHeader = &(m_BitmapInfor_Full->bmiHeader);

        BitmapInforHeader->biSize = sizeof(BITMAPINFOHEADER);//pi si
        BitmapInforHeader->biWidth = m_ulFullWidth;
        BitmapInforHeader->biHeight = m_ulFullHeight;
        BitmapInforHeader->biPlanes = 1;
        BitmapInforHeader->biBitCount = ulbiBitCount;
        BitmapInforHeader->biCompression = BI_RGB;
        BitmapInforHeader->biXPelsPerMeter = 0;
        BitmapInforHeader->biYPelsPerMeter = 0;
        BitmapInforHeader->biClrUsed = 0;
        BitmapInforHeader->biClrImportant = 0;

        BitmapInforHeader->biSizeImage =                   //图像数据
            (((BitmapInforHeader->biWidth * BitmapInforHeader->biBitCount + 31) & ~31) >> 3)
            * BitmapInforHeader->biHeight;
        // 16位和以后的没有颜色表，直接返回

        //!!
        m_dwSize=BitmapInforHeader->biSizeImage;    //数据大小
        m_BitmapData_Full=new BYTE[m_dwSize+10];
        ZeroMemory(m_BitmapData_Full,m_dwSize+10);

        return m_BitmapInfor_Full;
    }

    STDMETHODIMP_(ULONG) AddRef()
    {
        return 2;
    }
    STDMETHODIMP_(ULONG) Release()
    {
        return 1;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void ** lParam)
    {
        if( riid == IID_ISampleGrabberCB || riid == IID_IUnknown ) {
            *lParam = (void *) static_cast<ISampleGrabberCB*> ( this );
            return NOERROR;
        }

        return E_NOINTERFACE;
    }

    BYTE* GetNextScreen(DWORD &dwSize)
    {
        dwSize=m_dwSize;
        return (BYTE*)m_BitmapData_Full;
    }

    STDMETHODIMP SampleCB(double dbSampleTime, IMediaSample * Sample)
    {
        return 0;
    }

    //回调函数 在这里得到 bmp 的数据
    STDMETHODIMP BufferCB(double dblSampleTime, BYTE * szBuffer, long ulBufferSize)
    {
        if (!szBuffer) {
            return E_POINTER;
        }

        if (bStact==CMD_CAN_COPY) {       //未初始化 发送的同差异的一样
            //将图像数据拷贝的我们的内存
            memcpy(m_BitmapData_Full,szBuffer,ulBufferSize);    //位图

            InterlockedExchange((LPLONG)&bStact,CMD_CAN_SEND);      //原子自增可以发送
            return S_OK;
        }
        return -1;
    }
};

class CCaptureVideo
{
public:
    CCaptureVideo();
    virtual ~CCaptureVideo();
    LPBITMAPINFO GetBmpInfor();
    HRESULT InitCaptureGraphBuilder();
    HRESULT Open(int iDeviceID,int iPress);
    BOOL BindVideoFilter(int deviceId, IBaseFilter **pFilter);

    LPBYTE GetDIB(DWORD& dwSize);

    int GetDIBBufSize() const
    {
        return mCB.m_dwSize;
    }

    BOOL		m_bExit;

    HWND      m_hWnd;

    static CSampleGrabberCB mCB;
    IGraphBuilder *    m_pGB;   //通过该值可以访问 FCDO   Filter Control Device Object
    ICaptureGraphBuilder2* m_pCapture;   //通过该值可以访问 真实CDO

    IMediaControl*    m_pMC;             //过滤设备的接口
    IVideoWindow*     m_pVW;

    IBaseFilter*       m_pBF;              //FDO
    ISampleGrabber*    m_pGrabber;		   //引脚 24Color

    void FreeMediaType(AM_MEDIA_TYPE& mt);
    void ResizeVideoWindow();
    HRESULT SetupVideoWindow();
    void  SendEnd();
};

#endif // !defined(AFX_CAPTUREVIDEO_H__0984BB8E_6DCB_4A5C_8E03_1217AE6E409D__INCLUDED_)
