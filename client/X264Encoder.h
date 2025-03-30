#pragma once

extern "C"{
	#include <libyuv\libyuv.h>
	#include <x264\x264.h>
}

class CX264Encoder
{
private:
	x264_t*	m_pCodec;		//±àÂëÆ÷ÊµÀý
	x264_picture_t *m_pPicIn;
	x264_picture_t *m_pPicOut;
	x264_param_t    m_Param;
public:
	bool open(int width, int height, int fps,int bitrate);
	bool open(x264_param_t * param);

	void close();

	int encode(
		uint8_t * rgb,
		uint8_t   bpp,
		uint32_t stride,
		uint32_t width,
		uint32_t height,
		uint8_t ** lppData,
		uint32_t * lpSize,
		int        direction = 1
		);

	CX264Encoder();
	~CX264Encoder();
};

