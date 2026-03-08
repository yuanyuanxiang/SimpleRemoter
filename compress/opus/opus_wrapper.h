#pragma once
// Opus 编解码器封装
//
// === 启用 Opus 压缩的步骤 ===
//
// 1. 下载 opus 预编译库:
//    - Windows: https://opus-codec.org/downloads/ 或使用 vcpkg
//    - 推荐版本: opus-1.4 或更高
//
// 2. 将以下文件放入 compress/opus/ 目录:
//    - opus.h, opus_defines.h, opus_types.h, opus_multistream.h (头文件)
//    - opus.lib (Win32 Release)
//    - opus_x64.lib (x64 Release)
//    - opusd.lib (Win32 Debug, 可选)
//    - opus_x64d.lib (x64 Debug, 可选)
//
// 3. 在 StdAfx.h 中设置 USING_OPUS 为 1:
//    - client/StdAfx.h: #define USING_OPUS 1
//    - server/2015Remote/stdafx.h: #define USING_OPUS 1
//
// 4. 重新编译客户端和服务端
//
// === 带宽对比 ===
// PCM (未压缩): 192 KB/s @ 48kHz 立体声
// Opus 64kbps:   8 KB/s (压缩率 24:1)
//

#ifdef USING_OPUS
#if USING_OPUS

#include "opus.h"

// 根据平台选择库文件 (库文件在 compress/opus/ 目录)
#ifdef _WIN64
  #pragma comment(lib, "opus/opus_x64.lib")
#else
  #pragma comment(lib, "opus/opus.lib")
#endif

// Opus 编码器封装
class COpusEncoder {
public:
    COpusEncoder() : m_pEncoder(nullptr), m_nChannels(2), m_nFrameSize(960) {}

    ~COpusEncoder() {
        Destroy();
    }

    // 初始化编码器
    // sampleRate: 采样率 (48000)
    // channels: 声道数 (1 或 2)
    // bitrate: 码率 (64000 = 64kbps)
    BOOL Init(int sampleRate, int channels, int bitrate = 64000) {
        int error = 0;
        m_pEncoder = opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_AUDIO, &error);
        if (error != OPUS_OK || !m_pEncoder) {
            return FALSE;
        }

        m_nChannels = channels;
        // 20ms 帧 @ 48kHz = 960 samples
        m_nFrameSize = sampleRate / 50;  // 20ms

        // 设置码率
        opus_encoder_ctl(m_pEncoder, OPUS_SET_BITRATE(bitrate));
        // 设置复杂度 (0-10, 越高质量越好但 CPU 占用越高)
        opus_encoder_ctl(m_pEncoder, OPUS_SET_COMPLEXITY(5));
        // 关闭 DTX (不连续传输) 以保持稳定延迟
        opus_encoder_ctl(m_pEncoder, OPUS_SET_DTX(0));

        return TRUE;
    }

    void Destroy() {
        if (m_pEncoder) {
            opus_encoder_destroy(m_pEncoder);
            m_pEncoder = nullptr;
        }
    }

    // 编码 PCM 数据
    // pPCM: 输入 PCM 数据 (16-bit signed)
    // nSamples: 输入样本数 (每声道)
    // pOutput: 输出缓冲区
    // nMaxOutput: 输出缓冲区大小
    // 返回: 编码后的字节数, 失败返回 -1
    int Encode(const short* pPCM, int nSamples, unsigned char* pOutput, int nMaxOutput) {
        if (!m_pEncoder) return -1;

        int encoded = opus_encode(m_pEncoder, pPCM, nSamples, pOutput, nMaxOutput);
        return encoded;
    }

    int GetFrameSize() const { return m_nFrameSize; }
    int GetChannels() const { return m_nChannels; }

private:
    OpusEncoder* m_pEncoder;
    int m_nChannels;
    int m_nFrameSize;
};

// Opus 解码器封装
class COpusDecoder {
public:
    COpusDecoder() : m_pDecoder(nullptr), m_nChannels(2), m_nFrameSize(960) {}

    ~COpusDecoder() {
        Destroy();
    }

    // 初始化解码器
    BOOL Init(int sampleRate, int channels) {
        int error = 0;
        m_pDecoder = opus_decoder_create(sampleRate, channels, &error);
        if (error != OPUS_OK || !m_pDecoder) {
            return FALSE;
        }

        m_nChannels = channels;
        m_nFrameSize = sampleRate / 50;  // 20ms

        return TRUE;
    }

    void Destroy() {
        if (m_pDecoder) {
            opus_decoder_destroy(m_pDecoder);
            m_pDecoder = nullptr;
        }
    }

    // 解码 Opus 数据
    // pOpus: 输入 Opus 数据
    // nOpusLen: 输入数据长度
    // pPCM: 输出 PCM 缓冲区
    // nMaxSamples: 输出缓冲区大小 (样本数)
    // 返回: 解码的样本数, 失败返回 -1
    int Decode(const unsigned char* pOpus, int nOpusLen, short* pPCM, int nMaxSamples) {
        if (!m_pDecoder) return -1;

        int decoded = opus_decode(m_pDecoder, pOpus, nOpusLen, pPCM, nMaxSamples, 0);
        return decoded;
    }

    int GetFrameSize() const { return m_nFrameSize; }
    int GetChannels() const { return m_nChannels; }

private:
    OpusDecoder* m_pDecoder;
    int m_nChannels;
    int m_nFrameSize;
};

#endif // USING_OPUS
#endif // USING_OPUS defined
