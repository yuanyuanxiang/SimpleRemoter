#pragma once
#include <Vfw.h>
#pragma comment(lib,"Vfw32.lib")

#define ERR_INVALID_PARAM 1
#define ERR_NO_ENCODER 2
#define ERR_INTERNAL 3
#define ERR_NOT_SUPPORT 4

enum FCCHandler {
    ENCODER_BMP = BI_RGB,
    ENCODER_MJPEG = mmioFOURCC('M', 'J', 'P', 'G'),
    // 安装x264vfw编解码器: https://sourceforge.net/projects/x264vfw/
    ENCODER_H264 = mmioFOURCC('X', '2', '6', '4'),
};

/************************************************************************
* @class CBmpToAvi
* @brief 位图转AVI帧
************************************************************************/
class CBmpToAvi
{
public:
    CBmpToAvi();
    virtual ~CBmpToAvi();
    int Open(LPCTSTR szFile, LPBITMAPINFO lpbmi, int rate = 4, FCCHandler h = ENCODER_BMP);
    bool Write(unsigned char* lpBuffer);
    void Close();
    static std::string GetErrMsg(int result)
    {
        switch (result) {
        case ERR_INVALID_PARAM:
            return ("无效参数");
        case ERR_NOT_SUPPORT:
            return ("不支持的位深度，需要24位或32位");
        case ERR_NO_ENCODER:
            return ("未安装x264编解码器 \n下载地址：https://sourceforge.net/projects/x264vfw");
        case ERR_INTERNAL:
            return("创建AVI文件失败");
        default:
            return "succeed";
        }
    }
private:
    FCCHandler m_fccHandler;
    PAVIFILE m_pfile;
    PAVISTREAM m_pavi;
    int m_nFrames;
    static AVISTREAMINFO m_si; // 这个参数需要是静态的

    int m_bitCount = 24;
    int m_width = 1920;
    int m_height = 1080;
    int m_quality = 90;
    HIC m_hic = NULL;
};
