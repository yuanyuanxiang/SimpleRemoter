#include "X264Encoder.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN64
#pragma comment(lib,"libyuv/libyuv_x64.lib")
#pragma comment(lib,"x264/libx264_x64.lib")
#else
#pragma comment(lib,"libyuv/libyuv.lib")
#pragma comment(lib,"x264/libx264.lib")
#endif

CX264Encoder::CX264Encoder()
{
    memset(&m_Param, 0, sizeof(m_Param));
    m_pCodec = NULL;
    m_pPicIn = NULL;
    m_pPicOut = NULL;
}


CX264Encoder::~CX264Encoder()
{
    close();
}

bool CX264Encoder::open(int width, int height, int fps, int bitrate)
{
    x264_param_t param = { 0 };
    x264_param_default_preset(&param, "ultrafast", "zerolatency");

    param.i_width  = width  & 0xfffffffe;
    param.i_height = height & 0xfffffffe;

    //x264_LOG_NONE
    param.i_log_level = X264_LOG_NONE;
    param.i_threads = 1;
    param.i_frame_total = 0;
    param.i_keyint_max = 10;
    param.i_bframe = 0;					//²»ÆôÓÃbÖ¡
    param.b_open_gop = 0;
    param.i_fps_num = fps;
    param.i_csp = X264_CSP_I420;

    if (bitrate) {
        param.rc.i_rc_method = X264_RC_ABR;
        param.rc.i_bitrate = bitrate;
    }

    //ÉèÖÃprofile.
    if (x264_param_apply_profile(&param, x264_profile_names[0])) {
        return false;
    }

    return open(&param);
}

bool CX264Encoder::open(x264_param_t * param)
{
    m_pPicIn  = (x264_picture_t*)calloc(1, sizeof(x264_picture_t));
    m_pPicOut = (x264_picture_t*)calloc(1, sizeof(x264_picture_t));

    //input pic.
    x264_picture_init(m_pPicIn);

    x264_picture_alloc(
        m_pPicIn,
        X264_CSP_I420,
        param->i_width,
        param->i_height);

    //create codec instance.
    m_pCodec = x264_encoder_open(param);
    if (m_pCodec == NULL) {
        return false;
    }
    memcpy(&m_Param, param, sizeof(m_Param));
    return true;
}


void CX264Encoder::close()
{
    if (m_pCodec) {
        x264_encoder_close(m_pCodec);
        m_pCodec = NULL;
    }

    if (m_pPicIn) {
        x264_picture_clean(m_pPicIn);
        free(m_pPicIn);
        m_pPicIn = NULL;
    }

    if (m_pPicOut) {
        free(m_pPicOut);
        m_pPicOut = NULL;
    }
}


int CX264Encoder::encode(
    uint8_t * rgb,
    uint8_t   bpp,
    uint32_t  stride,
    uint32_t  width,
    uint32_t  height,
    uint8_t ** lppData,
    uint32_t * lpSize,
    int        direction)
{
    int encode_size = 0;
    x264_nal_t *pNal = NULL;
    int			iNal;

    if ((width  & 0xfffffffe) != m_Param.i_width ||
        (height & 0xfffffffe) != m_Param.i_height) {
        return -1;
    }

    switch (bpp) {
    case 24:
        libyuv::RGB24ToI420((uint8_t*)rgb, stride,
                            m_pPicIn->img.plane[0], m_pPicIn->img.i_stride[0],
                            m_pPicIn->img.plane[1], m_pPicIn->img.i_stride[1],
                            m_pPicIn->img.plane[2], m_pPicIn->img.i_stride[2],
                            m_Param.i_width, direction * m_Param.i_height);
        break;
    case 32:
        libyuv::ARGBToI420((uint8_t*)rgb, stride,
                           m_pPicIn->img.plane[0], m_pPicIn->img.i_stride[0],
                           m_pPicIn->img.plane[1], m_pPicIn->img.i_stride[1],
                           m_pPicIn->img.plane[2], m_pPicIn->img.i_stride[2],
                           m_Param.i_width, direction * m_Param.i_height);
        break;
    default:
        return -2;
    }


    encode_size = x264_encoder_encode(
                      m_pCodec,
                      &pNal,
                      &iNal,
                      m_pPicIn,
                      m_pPicOut);

    if (encode_size < 0) {
        return -3;
    }

    *lppData = pNal->p_payload;
    *lpSize  = encode_size;
    return 0;
}
