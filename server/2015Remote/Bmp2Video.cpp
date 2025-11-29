#include "stdafx.h"
#include "Bmp2Video.h"

#define USE_JPEG 0

#if USE_JPEG
#include "common/turbojpeg.h"
#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "jpeg/turbojpeg_64_d.lib")
#else
#pragma comment(lib, "jpeg/turbojpeg_64_d.lib")
#endif

#else
#ifdef _DEBUG
#pragma comment(lib, "jpeg/turbojpeg_32_d.lib")
#else
#pragma comment(lib, "jpeg/turbojpeg_32_d.lib")
#endif

#endif
#else
#define tjFree free
#endif


AVISTREAMINFO CBmpToAvi::m_si = {};

CBmpToAvi::CBmpToAvi()
{
    m_nFrames = 0;
    m_pfile = NULL;
    m_pavi = NULL;
    m_hic = NULL;
    AVIFileInit();
}

CBmpToAvi::~CBmpToAvi()
{
    Close();
    AVIFileExit();
}

int CBmpToAvi::Open(LPCTSTR szFile, LPBITMAPINFO lpbmi, int rate, FCCHandler h)
{
    if (szFile == NULL)
        return ERR_INVALID_PARAM;

    m_nFrames = 0;
    if (AVIFileOpen(&m_pfile, szFile, OF_WRITE | OF_CREATE, NULL))
        return ERR_INTERNAL;

    m_fccHandler = h;
    m_si.fccType = streamtypeVIDEO;
    m_si.fccHandler = m_fccHandler;
    m_si.dwScale = 1;
    m_si.dwRate = rate;
    m_width = lpbmi->bmiHeader.biWidth;
    m_height = lpbmi->bmiHeader.biHeight;
    SetRect(&m_si.rcFrame, 0, 0, m_width, m_height);

    m_bitCount = lpbmi->bmiHeader.biBitCount;
    if (m_bitCount != 24 && m_bitCount != 32) {
        AVIFileRelease(m_pfile);
        m_pfile = NULL;
        return ERR_NOT_SUPPORT;
    }

    // 创建正确的BITMAPINFO用于MJPEG
    BITMAPINFO bmiFormat = *lpbmi;

    if (m_fccHandler == ENCODER_H264) {
        // 打开H.264压缩器
        m_hic = ICOpen(ICTYPE_VIDEO, mmioFOURCC('X', '2', '6', '4'), ICMODE_COMPRESS);
        if (!m_hic) {
            AVIFileRelease(m_pfile);
            m_pfile = NULL;
            return ERR_NO_ENCODER;
        }

        // 设置输入格式（未压缩的24位BMP）
        BITMAPINFOHEADER inputFormat = { 0 };
        inputFormat.biSize = sizeof(BITMAPINFOHEADER);
        inputFormat.biWidth = m_width;
        inputFormat.biHeight = m_height;
        inputFormat.biPlanes = 1;
        inputFormat.biBitCount = 24;
        inputFormat.biCompression = BI_RGB;
        inputFormat.biSizeImage = m_width * m_height * 3;

        // 设置输出格式（H.264压缩）
        BITMAPINFOHEADER outputFormat = inputFormat;
        outputFormat.biCompression = mmioFOURCC('X', '2', '6', '4');

        // 查询压缩器能否处理这个格式
        DWORD result = ICCompressQuery(m_hic, &inputFormat, &outputFormat);
        if (result != ICERR_OK) {
            ICClose(m_hic);
            m_hic = NULL;
            AVIFileRelease(m_pfile);
            m_pfile = NULL;
            Mprintf("ICCompressQuery failed: %d\n", result);
            return ERR_NO_ENCODER;
        }

        // 开始压缩
        result = ICCompressBegin(m_hic, &inputFormat, &outputFormat);
        if (result != ICERR_OK) {
            ICClose(m_hic);
            m_hic = NULL;
            AVIFileRelease(m_pfile);
            m_pfile = NULL;
            Mprintf("ICCompressBegin failed: %d\n", result);
            return ERR_NO_ENCODER;
        }

        // 设置质量
        m_quality = 7500;

        // AVI流配置
        bmiFormat.bmiHeader.biCompression = mmioFOURCC('X', '2', '6', '4');
        bmiFormat.bmiHeader.biBitCount = 24;
        bmiFormat.bmiHeader.biSizeImage = m_width * m_height * 3;
        m_si.dwSuggestedBufferSize = bmiFormat.bmiHeader.biSizeImage;
    } else if (m_fccHandler == ENCODER_MJPEG) {
        // MJPEG需要特殊设置
        bmiFormat.bmiHeader.biCompression = mmioFOURCC('M', 'J', 'P', 'G');
        bmiFormat.bmiHeader.biBitCount = 24; // MJPEG解码后是24位
        // 计算正确的图像大小
        bmiFormat.bmiHeader.biSizeImage = m_width * m_height * 3;
        m_si.dwSuggestedBufferSize = bmiFormat.bmiHeader.biSizeImage * 2; // 预留足够空间
        m_quality = 85; // 默认质量
    } else {
        m_si.dwSuggestedBufferSize = lpbmi->bmiHeader.biSizeImage;
    }

    if (AVIFileCreateStream(m_pfile, &m_pavi, &m_si)) {
        if (m_hic) {
            ICCompressEnd(m_hic);
            ICClose(m_hic);
            m_hic = NULL;
        }
        AVIFileRelease(m_pfile);
        m_pfile = NULL;
        return ERR_INTERNAL;
    }

    if (AVIStreamSetFormat(m_pavi, 0, &bmiFormat, sizeof(BITMAPINFOHEADER))) {
        if (m_hic) {
            ICCompressEnd(m_hic);
            ICClose(m_hic);
            m_hic = NULL;
        }
        AVIStreamRelease(m_pavi);
        m_pavi = NULL;
        AVIFileRelease(m_pfile);
        m_pfile = NULL;
        return ERR_INTERNAL;
    }

    return 0;
}

#if USE_JPEG
// 优化的BMP到JPEG转换
bool BmpToJpeg(LPVOID lpBuffer, int width, int height, int quality,	unsigned char** jpegData, unsigned long* jpegSize)
{
    if (!lpBuffer || !jpegData || !jpegSize) {
        return false;
    }
    tjhandle jpegCompressor = tjInitCompress();
    if (!jpegCompressor) {
        Mprintf("TurboJPEG initialization failed: %s\n", tjGetErrorStr());
        return false;
    }

    // 确保质量在合理范围内
    if (quality < 1) quality = 85;
    if (quality > 100) quality = 100;

    int pitch = width * 3; // BGR24格式，每行字节数

    // 重要：初始化为NULL，让TurboJPEG自己分配内存
    *jpegData = NULL;
    *jpegSize = 0;

    // 去掉TJFLAG_NOREALLOC标志，让TurboJPEG自动分配内存
    int tjError = tjCompress2(
                      jpegCompressor,
                      (unsigned char*)lpBuffer,
                      width,
                      pitch,           // 每行字节数
                      height,
                      TJPF_BGR,        // BGR格式
                      jpegData,        // TurboJPEG会自动分配内存
                      jpegSize,
                      TJSAMP_422,      // 4:2:2色度子采样
                      quality,         // 压缩质量
                      0                // 不使用特殊标志
                  );

    if (tjError != 0) {
        Mprintf("TurboJPEG compression failed: %s\n", tjGetErrorStr2(jpegCompressor));
        tjDestroy(jpegCompressor);
        return false;
    }

    tjDestroy(jpegCompressor);

    // 验证输出
    if (*jpegData == NULL || *jpegSize == 0) {
        Mprintf("JPEG compression produced no data\n");
        return false;
    }

    Mprintf("JPEG compression successful: %lu bytes\n", *jpegSize);
    return true;
}
#else
#include <windows.h>
#include <gdiplus.h>
#include <shlwapi.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")

using namespace Gdiplus;

// ==================== 辅助函数 ====================

// 获取 JPEG 编码器的 CLSID
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT num = 0;
    UINT size = 0;

    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)malloc(size);
    if (pImageCodecInfo == NULL) return -1;

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }

    free(pImageCodecInfo);
    return -1;
}

// ==================== 主函数 ====================

bool BmpToJpeg(LPVOID lpBuffer, int width, int height, int quality,
               unsigned char** jpegData, unsigned long* jpegSize)
{
    if (!lpBuffer || !jpegData || !jpegSize || width <= 0 || height <= 0) {
        return false;
    }

    // 计算 DIB 的行字节数（4字节对齐）
    int rowSize = ((width * 3 + 3) / 4) * 4;

    // 创建 Bitmap 对象（24位 BGR 格式）
    Bitmap* bitmap = new Bitmap(width, height, PixelFormat24bppRGB);
    if (!bitmap || bitmap->GetLastStatus() != Ok) {
        if (bitmap) delete bitmap;
        return false;
    }

    // 锁定 Bitmap 以写入数据
    BitmapData bitmapData;
    Rect rect(0, 0, width, height);
    Status status = bitmap->LockBits(&rect, ImageLockModeWrite,
                                     PixelFormat24bppRGB, &bitmapData);

    if (status != Ok) {
        delete bitmap;
        return false;
    }

    // 复制数据（注意：DIB 是底部到顶部，需要翻转）
    BYTE* srcData = (BYTE*)lpBuffer;
    BYTE* dstData = (BYTE*)bitmapData.Scan0;

    for (int y = 0; y < height; y++) {
        // DIB 是从底部开始的，所以需要翻转
        BYTE* srcRow = srcData + (height - 1 - y) * rowSize;
        BYTE* dstRow = dstData + y * bitmapData.Stride;
        memcpy(dstRow, srcRow, width * 3);
    }

    bitmap->UnlockBits(&bitmapData);

    // 获取 JPEG 编码器
    CLSID jpegClsid;
    if (GetEncoderClsid(L"image/jpeg", &jpegClsid) < 0) {
        delete bitmap;
        return false;
    }

    // 设置 JPEG 质量参数
    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = EncoderQuality;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;

    ULONG qualityValue = (ULONG)quality;
    encoderParams.Parameter[0].Value = &qualityValue;

    // 创建内存流用于保存 JPEG
    IStream* stream = NULL;
    HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    if (FAILED(hr)) {
        delete bitmap;
        return false;
    }

    // 保存为 JPEG
    status = bitmap->Save(stream, &jpegClsid, &encoderParams);
    delete bitmap;

    if (status != Ok) {
        stream->Release();
        return false;
    }

    // 获取 JPEG 数据
    HGLOBAL hMem = NULL;
    hr = GetHGlobalFromStream(stream, &hMem);
    if (FAILED(hr)) {
        stream->Release();
        return false;
    }

    SIZE_T memSize = GlobalSize(hMem);
    if (memSize == 0) {
        stream->Release();
        return false;
    }

    // 分配输出缓冲区
    *jpegSize = (unsigned long)memSize;
    *jpegData = new unsigned char[*jpegSize];

    // 复制数据
    void* pMem = GlobalLock(hMem);
    if (pMem) {
        memcpy(*jpegData, pMem, *jpegSize);
        GlobalUnlock(hMem);
    } else {
        delete[] * jpegData;
        *jpegData = NULL;
        stream->Release();
        return false;
    }

    stream->Release();
    return true;
}

// ==================== GDI+ 初始化/清理 ====================

class GdiplusManager
{
private:
    ULONG_PTR gdiplusToken;
    bool initialized;

public:
    GdiplusManager() : gdiplusToken(0), initialized(false)
    {
        GdiplusStartupInput gdiplusStartupInput;
        if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) == Ok) {
            initialized = true;
        }
    }

    ~GdiplusManager()
    {
        if (initialized) {
            GdiplusShutdown(gdiplusToken);
        }
    }

    bool IsInitialized() const
    {
        return initialized;
    }
};

// 全局对象，自动初始化和清理
static GdiplusManager g_gdiplusManager;
#endif


// 正确的32位转24位转换（含翻转）
unsigned char* ConvertScreenshot32to24(unsigned char* p32bitBmp, int width, int height)
{
    // 计算BMP的实际行大小（4字节对齐）
    int srcRowSize = ((width * 32 + 31) / 32) * 4;
    int dstRowSize = width * 3; // 目标是紧凑的24位

    unsigned char* p24bitBmp = (unsigned char*)malloc(dstRowSize * height);
    if (!p24bitBmp) return nullptr;

    for (int y = 0; y < height; y++) {
        // BMP是从下到上存储，需要翻转
        unsigned char* src = p32bitBmp + (height - 1 - y) * srcRowSize;
        unsigned char* dst = p24bitBmp + y * dstRowSize;

        for (int x = 0; x < width; x++) {
            dst[x * 3 + 0] = src[x * 4 + 0]; // B
            dst[x * 3 + 1] = src[x * 4 + 1]; // G
            dst[x * 3 + 2] = src[x * 4 + 2]; // R
            // 忽略Alpha通道
        }
    }

    return p24bitBmp;
}

// 24位BMP处理（含翻转和去对齐）
unsigned char* Process24BitBmp(unsigned char* lpBuffer, int width, int height)
{
    // BMP 24位行大小（4字节对齐）
    int srcRowSize = ((width * 24 + 31) / 32) * 4;
    int dstRowSize = width * 3; // 紧凑格式

    unsigned char* processed = (unsigned char*)malloc(dstRowSize * height);
    if (!processed) return nullptr;

    for (int y = 0; y < height; y++) {
        // 翻转并去除填充字节
        unsigned char* src = lpBuffer + (height - 1 - y) * srcRowSize;
        unsigned char* dst = processed + y * dstRowSize;
        memcpy(dst, src, dstRowSize);
    }

    return processed;
}

bool CBmpToAvi::Write(unsigned char* lpBuffer)
{
    if (m_pfile == NULL || m_pavi == NULL || lpBuffer == NULL)
        return false;

    unsigned char* writeData = nullptr;
    unsigned long writeSize = 0;
    bool needFree = false;

    switch (m_fccHandler) {
    case ENCODER_BMP:
        writeData = lpBuffer;
        writeSize = m_si.dwSuggestedBufferSize;
        break;

    case ENCODER_H264: {
        unsigned char* processedBuffer = nullptr;

        if (m_bitCount == 32) {
            processedBuffer = ConvertScreenshot32to24(lpBuffer, m_width, m_height);
        } else if (m_bitCount == 24) {
            processedBuffer = Process24BitBmp(lpBuffer, m_width, m_height);
        }

        if (!processedBuffer) {
            Mprintf("Failed to process buffer\n");
            return false;
        }

        // 创建正确的格式头
        BITMAPINFOHEADER inputHeader = { 0 };
        inputHeader.biSize = sizeof(BITMAPINFOHEADER);
        inputHeader.biWidth = m_width;
        inputHeader.biHeight = -m_height;
        inputHeader.biPlanes = 1;
        inputHeader.biBitCount = 24;
        inputHeader.biCompression = BI_RGB;
        inputHeader.biSizeImage = m_width * m_height * 3;

        BITMAPINFOHEADER outputHeader = inputHeader;
        outputHeader.biCompression = mmioFOURCC('X', '2', '6', '4');

        // 分配输出缓冲区
        DWORD maxCompressedSize = m_width * m_height * 3;
        unsigned char* compressedData = (unsigned char*)malloc(maxCompressedSize);
        if (!compressedData) {
            free(processedBuffer);
            Mprintf("Failed to allocate compression buffer\n");
            return false;
        }

        DWORD flags = 0;

        // 正确调用ICCompress
        DWORD result = ICCompress(
                           m_hic,              // 压缩器句柄
                           0,                  // 标志（0=自动决定关键帧）
                           &outputHeader,      // 输出格式头
                           compressedData,     // 输出数据
                           &inputHeader,       // 输入格式头
                           processedBuffer,    // 输入数据
                           NULL,               // ckid
                           &flags,             // 输出标志
                           m_nFrames,          // 帧号
                           0,                  // 期望大小（0=自动）
                           m_quality,          // 质量
                           NULL,               // 前一帧格式头
                           NULL                // 前一帧数据
                       );

        if (result != ICERR_OK) {
            free(compressedData);
            free(processedBuffer);
            Mprintf("ICCompress failed: %d\n", result);
            return false;
        }

        // 实际压缩大小在outputHeader.biSizeImage中
        writeData = compressedData;
        writeSize = outputHeader.biSizeImage;
        needFree = true;

        free(processedBuffer);
        break;
    }

    case ENCODER_MJPEG: {
        unsigned char* processedBuffer = nullptr;

        // 处理不同位深度
        if (m_bitCount == 32) {
            processedBuffer = ConvertScreenshot32to24(lpBuffer, m_width, m_height);
        } else if (m_bitCount == 24) {
            processedBuffer = Process24BitBmp(lpBuffer, m_width, m_height);
        }

        if (!processedBuffer) {
            return false;
        }
        // 压缩为JPEG
        if (!BmpToJpeg(processedBuffer, m_width, m_height, m_quality, &writeData, &writeSize)) {
            free(processedBuffer);
            Mprintf("Failed to compress JPEG\n");
            return false;
        }

        free(processedBuffer);
        needFree = true;
        break;
    }
    default:
        return false;
    }

    // 写入AVI流
    LONG bytesWritten = 0;
    LONG samplesWritten = 0;
    HRESULT hr = AVIStreamWrite(m_pavi, m_nFrames, 1,
                                writeData, writeSize,
                                AVIIF_KEYFRAME,
                                &samplesWritten, &bytesWritten);

    if (needFree && writeData) {
        if (m_fccHandler == ENCODER_MJPEG) {
            tjFree(writeData);
        } else {
            free(writeData);
        }
    }

    if (hr != AVIERR_OK) {
        Mprintf("AVIStreamWrite failed: 0x%08X\n", hr);
        return false;
    }

    m_nFrames++;
    return true;
}

void CBmpToAvi::Close()
{
    if (m_hic) {
        ICCompressEnd(m_hic);
        ICClose(m_hic);
        m_hic = NULL;
    }
    if (m_pavi) {
        AVIStreamRelease(m_pavi);
        m_pavi = NULL;
    }
    if (m_pfile) {
        AVIFileRelease(m_pfile);
        m_pfile = NULL;
    }
    m_nFrames = 0;
}
