#pragma once
#include "stdafx.h"
#include <assert.h>
#include "CursorInfo.h"
#include "../common/commands.h"

#define DEFAULT_GOP 0x7FFFFFFF

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <emmintrin.h>  // SSE2
#include "X264Encoder.h"
#include "ScrollDetector.h"
#include "common/file_upload.h"

inline bool HasSSE2()
{
    auto static has = IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE);
    return has;
}

class ThreadPool
{
public:
    // 构造函数：创建固定数量的线程
    ThreadPool(size_t numThreads) : stop(false)
    {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] {
                while (true)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queueMutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty()) return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    try {
                        task();
                    } catch (...) {
                        // 处理异常
                    }
                }
            });
        }
    }

    // 析构函数：销毁线程池
    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers)
            worker.join();
    }

    // 任务提交
    template<typename F>
    auto enqueue(F&& f) -> std::future<decltype(f())>
    {
        using ReturnType = decltype(f());
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(f));
        std::future<ReturnType> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.emplace([task]() {
                (*task)();
            });
        }
        condition.notify_one();
        return res;
    }

    void waitAll()
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        condition.wait(lock, [this] { return tasks.empty(); });
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};

class ScreenCapture
{
private:
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
    {
        std::vector<MONITORINFOEX>* monitors = reinterpret_cast<std::vector<MONITORINFOEX>*>(dwData);

        MONITORINFOEX mi;
        mi.cbSize = sizeof(MONITORINFOEX);
        if (GetMonitorInfo(hMonitor, &mi)) {
            monitors->push_back(mi); // 保存显示器信息
        }
        return TRUE;
    }
public:
    static std::vector<MONITORINFOEX> GetAllMonitors()
    {
        std::vector<MONITORINFOEX> monitors;
        EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&monitors);
        return monitors;
    }
public:
    ThreadPool*      m_ThreadPool;		 // 线程池
    BYTE*            m_FirstBuffer;		 // 上一帧数据
    BYTE*			 m_RectBuffer;       // 当前缓存区
    LPBYTE*		     m_BlockBuffers;	 // 分块缓存
    ULONG*			 m_BlockSizes;       // 分块差异像素数
    int				 m_BlockNum;         // 分块个数
    int				 m_SendQuality;		 // 发送质量

    LPBITMAPINFO     m_BitmapInfor_Full; // BMP信息
    LPBITMAPINFO     m_BitmapInfor_Send; // 发送的BMP信息
    BYTE             *m_BmpZoomFirst;    // 第一个缩放帧
    BYTE             *m_BmpZoomBuffer;   // 缩放缓存
    BYTE             m_bAlgorithm;       // 屏幕差异算法

    int				 m_iScreenX;		 // 起始x坐标
    int				 m_iScreenY;		 // 起始y坐标
    ULONG            m_ulFullWidth;      // 屏幕宽
    ULONG            m_ulFullHeight;     // 屏幕高
    bool             m_bZoomed;          // 屏幕被缩放
    double           m_wZoom;            // 屏幕横向缩放比
    double           m_hZoom;            // 屏幕纵向缩放比

    int			     m_biBitCount;		 // 每像素比特数
    int              m_FrameID;          // 帧序号
    int              m_GOP;              // 关键帧间隔
    bool		     m_SendKeyFrame;	 // 发送关键帧
    CX264Encoder	 *m_encoder;		 // 编码器
    int             m_nScreenCount;      // 屏幕数量
    BOOL            m_bEnableMultiScreen;// 多显示器支持

    // 滚动检测相关
    CScrollDetector* m_pScrollDetector;  // 滚动检测器
    bool            m_bEnableScrollDetect;   // 是否启用滚动检测
    bool            m_bServerSupportsScroll; // 服务端是否支持滚动
    bool            m_bLastFrameWasScroll;   // 上一帧是否是滚动帧（用于强制同步）
    int             m_nScrollDetectInterval; // 滚动检测间隔（0=禁用, 1=每帧, 2=每2帧, ...）
    int             m_nInstructionSet = 0;
    int             m_nBitRate = 0;      // H264 编码码率 (kbps), 0=自动

protected:
    int             m_nVScreenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int             m_nVScreenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int             m_nVScreenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int             m_nVScreenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

public:
    ScreenCapture(int n = 32, BYTE algo = ALGORITHM_DIFF, BOOL all = FALSE) :
        m_ThreadPool(nullptr), m_FirstBuffer(nullptr), m_RectBuffer(nullptr),
        m_BitmapInfor_Full(nullptr), m_bAlgorithm(algo), m_SendQuality(100),
        m_ulFullWidth(0), m_ulFullHeight(0), m_bZoomed(false), m_wZoom(1), m_hZoom(1),
        m_FrameID(0), m_GOP(DEFAULT_GOP), m_iScreenX(0), m_iScreenY(0), m_biBitCount(n),
        m_SendKeyFrame(false), m_encoder(nullptr),
        m_pScrollDetector(nullptr), m_bEnableScrollDetect(false), m_bServerSupportsScroll(false),
        m_bLastFrameWasScroll(false), m_nScrollDetectInterval(1)
    {
        m_BitmapInfor_Send = nullptr;
        m_BmpZoomBuffer = nullptr;
        m_BmpZoomFirst = nullptr;
        m_BlockNum = 8;
        m_ThreadPool = new ThreadPool(m_BlockNum);
        auto monitors = GetAllMonitors();
        static int index = 0;
        m_nScreenCount = monitors.size();
        m_bEnableMultiScreen = all;
        if (all && !monitors.empty()) {
            int idx = index++ % (monitors.size()+1);
            if (idx == 0) {
                m_iScreenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
                m_iScreenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
                m_ulFullWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                m_ulFullHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            } else {
                RECT rt = monitors[idx-1].rcMonitor;
                m_iScreenX = rt.left;
                m_iScreenY = rt.top;
                m_ulFullWidth = rt.right - rt.left;
                m_ulFullHeight = rt.bottom - rt.top;
            }
        } else {
            //::GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)获取屏幕大小不准
            //例如当屏幕显示比例为125%时，获取到的屏幕大小需要乘以1.25才对
            DEVMODE devmode;
            memset(&devmode, 0, sizeof(devmode));
            devmode.dmSize = sizeof(DEVMODE);
            devmode.dmDriverExtra = 0;
            BOOL Isgetdisplay = EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &devmode);
            m_ulFullWidth = devmode.dmPelsWidth;
            m_ulFullHeight = devmode.dmPelsHeight;
            int w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
            m_bZoomed = (w != m_ulFullWidth) || (h != m_ulFullHeight);
            m_wZoom = double(m_ulFullWidth) / w, m_hZoom = double(m_ulFullHeight) / h;
            Mprintf("=> 桌面缩放比例: %.2f, %.2f\t分辨率：%d x %d\n", m_wZoom, m_hZoom, m_ulFullWidth, m_ulFullHeight);
            m_wZoom = 1.0 / m_wZoom, m_hZoom = 1.0 / m_hZoom;
        }

        m_BlockBuffers = new LPBYTE[m_BlockNum];
        m_BlockSizes = new ULONG[m_BlockNum];
        for (int blockY = 0; blockY < m_BlockNum; ++blockY) {
            m_BlockBuffers[blockY] = new BYTE[m_ulFullWidth * m_ulFullHeight * 4 * 2 / m_BlockNum + 12];
        }
    }
    virtual ~ScreenCapture()
    {
        if (m_BitmapInfor_Full != NULL) {
            delete[](char*)m_BitmapInfor_Full;
            m_BitmapInfor_Full = NULL;
        }
        SAFE_DELETE_ARRAY(m_RectBuffer);
        SAFE_DELETE(m_BitmapInfor_Send);
        SAFE_DELETE_ARRAY(m_BmpZoomBuffer);
        SAFE_DELETE_ARRAY(m_BmpZoomFirst);

        for (int blockY = 0; blockY < m_BlockNum; ++blockY) {
            SAFE_DELETE_ARRAY(m_BlockBuffers[blockY]);
        }
        SAFE_DELETE_ARRAY(m_BlockBuffers);
        SAFE_DELETE_ARRAY(m_BlockSizes);

        SAFE_DELETE(m_ThreadPool);
        SAFE_DELETE(m_encoder);
        SAFE_DELETE(m_pScrollDetector);
    }

    virtual int GetScreenCount() const
    {
        return m_nScreenCount;
    }

    virtual int GetCurrentWidth() const
    {
        return m_BitmapInfor_Send->bmiHeader.biWidth;
    }
    virtual int GetCurrentHeight() const
    {
        return m_BitmapInfor_Send->bmiHeader.biHeight;
    }
    virtual int GetScreenWidth() const
    {
        return m_ulFullWidth;
    }
    virtual int GetScreenHeight() const
    {
        return m_ulFullHeight;
    }
    virtual int GetScreenLeft() const
    {
        return m_iScreenX;
    }
    virtual int GetScreenTop() const
    {
        return m_iScreenY;
    }

    inline int GetVScreenWidth() const
    {
        return m_nVScreenWidth;
    }
    inline int GetVScreenHeight() const
    {
        return m_nVScreenHeight;
    }
    inline int GetVScreenLeft() const
    {
        return m_nVScreenLeft;
    }
    inline int GetVScreenTop() const
    {
        return m_nVScreenTop;
    }

    virtual bool IsOriginalSize() const
    {
        return m_BitmapInfor_Full->bmiHeader.biWidth == m_BitmapInfor_Send->bmiHeader.biWidth &&
               m_BitmapInfor_Full->bmiHeader.biHeight == m_BitmapInfor_Send->bmiHeader.biHeight;
    }

    virtual bool IsLargeScreen(int width, int height) const
    {
        return m_BitmapInfor_Send->bmiHeader.biWidth > width && m_BitmapInfor_Send->bmiHeader.biHeight > height;
    }

    virtual BOOL IsMultiScreenEnabled() const
    {
        return m_bEnableMultiScreen;
    }

    virtual int SendQuality(int quality)
    {
        int old = m_SendQuality;
        m_SendQuality = quality;
        return old;
    }

    virtual RECT GetScreenRect() const
    {
        RECT rect;
        rect.left = m_iScreenX;
        rect.top = m_iScreenY;
        rect.right = m_ulFullWidth;
        rect.bottom = m_ulFullHeight;
        return rect;
    }

public:
    virtual BOOL UsingDXGI() const
    {
        return FALSE;
    }
    //*************************************** 图像差异算法（串行） *************************************
    ULONG CompareBitmapDXGI(LPBYTE CompareSourData, LPBYTE CompareDestData, LPBYTE szBuffer,
                            DWORD ulCompareLength, BYTE algo, int startPostion = 0)
    {
        // Windows规定一个扫描行所占的字节数必须是4的倍数, 所以用DWORD比较
        LPDWORD	p1 = (LPDWORD)CompareDestData, p2 = (LPDWORD)CompareSourData;
        LPBYTE p = szBuffer;
        // channel: 输出每像素字节数
        // ratio: 长度字段的除数 (GRAY/RGB565 发送像素数量，DIFF 发送字节数)
        ULONG channel = (algo == ALGORITHM_GRAY) ? 1 : (algo == ALGORITHM_RGB565) ? 2 : 4;
        ULONG ratio = (algo == ALGORITHM_GRAY || algo == ALGORITHM_RGB565) ? 4 : 1;
        for (ULONG i = 0; i < ulCompareLength; i += 4, ++p1, ++p2) {
            if (*p1 == *p2)
                continue;
            ULONG index = i;
            LPDWORD pos1 = p1++, pos2 = p2++;
            // 计算有几个像素值不同
            for (i += 4; i < ulCompareLength && *p1 != *p2; i += 4, ++p1, ++p2);
            ULONG ulCount = i - index;
            memcpy(pos1, pos2, ulCount); // 更新目标像素

            *(LPDWORD)(p) = index + startPostion;
            *(LPDWORD)(p + sizeof(ULONG)) = ulCount / ratio;
            p += 2 * sizeof(ULONG);
            if (algo == ALGORITHM_RGB565) {
                // BGRA → RGB565 转换
                ConvertBGRAtoRGB565((const BYTE*)pos2, (uint16_t*)p, ulCount / 4);
                p += ulCount / 2;
            } else if (channel != 1) {
                memcpy(p, pos2, ulCount);
                p += ulCount;
            } else {
                for (LPBYTE end = p + ulCount / ratio; p < end; p += channel, ++pos2) {
                    LPBYTE src = (LPBYTE)pos2;
                    *p = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
                }
            }
        }

        return p - szBuffer;
    }

    //*************************************** 图像差异算法 SSE2 优化版 *************************************
    virtual ULONG CompareBitmap(LPBYTE CompareSourData, LPBYTE CompareDestData, LPBYTE szBuffer,
                                DWORD ulCompareLength, BYTE algo, int startPostion = 0)
    {
        if (m_nInstructionSet == 0 || UsingDXGI() || !HasSSE2())
            return CompareBitmapDXGI(CompareSourData, CompareDestData, szBuffer, ulCompareLength, algo, startPostion);

        LPBYTE p = szBuffer;
        // channel: 输出每像素字节数
        // ratio: 长度字段的除数 (GRAY/RGB565 发送像素数量，DIFF 发送字节数)
        ULONG channel = (algo == ALGORITHM_GRAY) ? 1 : (algo == ALGORITHM_RGB565) ? 2 : 4;
        ULONG ratio = (algo == ALGORITHM_GRAY || algo == ALGORITHM_RGB565) ? 4 : 1;

        // SSE2: 每次比较 16 字节 (4 个像素)
        const ULONG SSE_BLOCK = 16;
        const ULONG alignedLength = ulCompareLength & ~(SSE_BLOCK - 1);

        __m128i* v1 = (__m128i*)CompareDestData;
        __m128i* v2 = (__m128i*)CompareSourData;

        ULONG i = 0;
        while (i < alignedLength) {
            // SSE2 快速比较: 一次比较 16 字节
            __m128i cmp = _mm_cmpeq_epi32(*v1, *v2);
            int mask = _mm_movemask_epi8(cmp);

            if (mask == 0xFFFF) {
                // 16 字节完全相同，跳过
                i += SSE_BLOCK;
                ++v1;
                ++v2;
                continue;
            }

            // 发现差异，记录起始位置
            ULONG index = i;
            LPBYTE pos1 = (LPBYTE)v1;
            LPBYTE pos2 = (LPBYTE)v2;

            // 继续扫描连续的差异区域（带间隙容忍）
            // GAP_TOLERANCE: 允许的最大间隙，小于此值的相同区域会被合并
            // 设为 32 字节（8像素），因为每个差异区域头部开销是 8 字节
            const ULONG GAP_TOLERANCE = 32;

            // 首先必须前进一个块（当前块已知有差异）
            i += SSE_BLOCK;
            ++v1;
            ++v2;

            // 继续扫描更多差异，应用间隙容忍
            ULONG gapCount = 0;
            while (i < alignedLength) {
                cmp = _mm_cmpeq_epi32(*v1, *v2);
                mask = _mm_movemask_epi8(cmp);

                if (mask == 0xFFFF) {
                    // 相同块 - 累计间隙
                    gapCount += SSE_BLOCK;
                    if (gapCount > GAP_TOLERANCE) {
                        // 间隙太大 - 停止（不包含间隙）
                        break;
                    }
                    // 间隙仍可接受 - 继续扫描
                    i += SSE_BLOCK;
                    ++v1;
                    ++v2;
                } else {
                    // 差异块 - 重置间隙并包含它
                    gapCount = 0;
                    i += SSE_BLOCK;
                    ++v1;
                    ++v2;
                }
            }

            // 排除末尾累积的间隙
            if (gapCount > 0 && gapCount <= GAP_TOLERANCE) {
                i -= gapCount;
                v1 = (__m128i*)((LPBYTE)v1 - gapCount);
                v2 = (__m128i*)((LPBYTE)v2 - gapCount);
            }

            ULONG ulCount = i - index;

            // 更新目标缓冲区
            memcpy(pos1, pos2, ulCount);

            // 写入差异信息: [位置][长度][数据]
            *(LPDWORD)(p) = index + startPostion;
            *(LPDWORD)(p + sizeof(ULONG)) = ulCount / ratio;
            p += 2 * sizeof(ULONG);

            if (algo == ALGORITHM_RGB565) {
                // RGB565 转换：使用 SSE2 优化
                ConvertBGRAtoRGB565(pos2, (uint16_t*)p, ulCount / 4);
                p += ulCount / 2;
            } else if (channel != 1) {
                memcpy(p, pos2, ulCount);
                p += ulCount;
            } else {
                // 灰度转换：使用优化的批量处理
                ConvertToGray_SSE2(p, pos2, ulCount);
                p += ulCount / ratio;
            }
        }

        // 处理剩余的非对齐部分 (0-12 字节)
        if (i < ulCompareLength) {
            LPDWORD p1 = (LPDWORD)((LPBYTE)CompareDestData + i);
            LPDWORD p2 = (LPDWORD)((LPBYTE)CompareSourData + i);

            for (; i < ulCompareLength; i += 4, ++p1, ++p2) {
                if (*p1 == *p2)
                    continue;

                ULONG index = i;
                LPDWORD pos1 = p1++;
                LPDWORD pos2 = p2++;

                for (i += 4; i < ulCompareLength && *p1 != *p2; i += 4, ++p1, ++p2);
                ULONG ulCount = i - index;
                memcpy(pos1, pos2, ulCount);

                *(LPDWORD)(p) = index + startPostion;
                *(LPDWORD)(p + sizeof(ULONG)) = ulCount / ratio;
                p += 2 * sizeof(ULONG);

                if (algo == ALGORITHM_RGB565) {
                    // RGB565 转换
                    ConvertBGRAtoRGB565((const BYTE*)pos2, (uint16_t*)p, ulCount / 4);
                    p += ulCount / 2;
                } else if (channel != 1) {
                    memcpy(p, pos2, ulCount);
                    p += ulCount;
                } else {
                    // 剩余部分用标量处理
                    LPDWORD srcPtr = pos2;
                    for (LPBYTE end = p + ulCount / ratio; p < end; ++p, ++srcPtr) {
                        LPBYTE src = (LPBYTE)srcPtr;
                        *p = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
                    }
                }
            }
        }

        return p - szBuffer;
    }

    //*************************************** 图像差异算法（并行） *************************************
    ULONG MultiCompareBitmap(LPBYTE srcData, LPBYTE dstData, LPBYTE szBuffer,
                             DWORD ulCompareLength, BYTE algo)
    {

        int N = m_BlockNum;
        ULONG blockLength = ulCompareLength / N;  // 每个任务的基本字节数
        ULONG remainingLength = ulCompareLength % N;  // 剩余的字节数

        std::vector<std::future<ULONG>> futures;
        for (int blockY = 0; blockY < N; ++blockY) {
            // 计算当前任务的字节数
            ULONG currentBlockLength = blockLength + (blockY == N - 1 ? remainingLength : 0);
            // 计算当前任务的起始位置
            ULONG startPosition = blockY * blockLength;

            futures.emplace_back(m_ThreadPool->enqueue([=]() -> ULONG {
                LPBYTE srcBlock = srcData + startPosition;
                LPBYTE dstBlock = dstData + startPosition;
                LPBYTE blockBuffer = m_BlockBuffers[blockY];

                // 处理当前任务并返回比对数据大小
                return m_BlockSizes[blockY] = CompareBitmap(srcBlock, dstBlock, blockBuffer, currentBlockLength, algo, startPosition);
            }));
        }

        // 等待所有任务完成并获取返回值
        for (auto& future : futures) {
            future.get();
        }

        // 合并所有块的差异信息到 szBuffer
        ULONG offset = 0;
        for (int blockY = 0; blockY < N; ++blockY) {
            memcpy(szBuffer + offset, m_BlockBuffers[blockY], m_BlockSizes[blockY]);
            offset += m_BlockSizes[blockY];
        }

        return offset;  // 返回缓冲区的大小
    }

    virtual int GetFrameID() const
    {
        return m_FrameID;
    }

    virtual LPBYTE GetFirstBuffer() const
    {
        return m_FirstBuffer;
    }

    virtual int GetBMPSize() const
    {
        assert(m_BitmapInfor_Send);
        return m_BitmapInfor_Send->bmiHeader.biSizeImage;
    }

    // ===================== 滚动检测相关方法 =====================

    // 启用/禁用滚动检测
    virtual void EnableScrollDetection(bool enable)
    {
        m_bEnableScrollDetect = enable;
        if (enable && !m_pScrollDetector && m_BitmapInfor_Send) {
            m_pScrollDetector = new CScrollDetector(
                m_BitmapInfor_Send->bmiHeader.biWidth,
                m_BitmapInfor_Send->bmiHeader.biHeight,
                4  // BGRA
            );
        }
    }

    // 设置服务端能力标志
    virtual void SetServerCapabilities(uint32_t caps)
    {
        m_bServerSupportsScroll = (caps & CAP_SCROLL_DETECT) != 0;
    }

    // 设置滚动检测间隔（0=禁用, 1=每帧, 2=每2帧, ...）
    virtual void SetScrollDetectInterval(int interval)
    {
        m_nScrollDetectInterval = interval;
        // 间隔为0时禁用滚动检测
        if (interval <= 0) {
            m_bEnableScrollDetect = false;
        }
    }

    // 构建滚动帧
    // scrollAmount > 0: 向下滚动，底部露出新内容
    // scrollAmount < 0: 向上滚动，顶部露出新内容
    virtual LPBYTE BuildScrollFrame(int scrollAmount, LPBYTE currentFrame, ULONG* outLength)
    {
        BYTE algo = m_bAlgorithm;
        BYTE direction = (scrollAmount > 0) ? SCROLL_DIR_DOWN : SCROLL_DIR_UP;
        int absScroll = abs(scrollAmount);

        // 消息头: TOKEN(1) + 算法(1) + 光标位置(8) + 光标类型(1) = 11字节
        m_RectBuffer[0] = TOKEN_SCROLL_FRAME;
        LPBYTE data = m_RectBuffer + 1;

        // 写入算法类型
        data[0] = algo;

        // 写入光标位置
        POINT CursorPos;
        GetCursorPos(&CursorPos);
        CursorPos.x /= m_wZoom;
        CursorPos.y /= m_hZoom;
        memcpy(data + 1, &CursorPos, sizeof(POINT));

        // 写入当前光标类型
        static CCursorInfo m_CursorInfor;
        BYTE bCursorIndex = m_CursorInfor.getCurrentCursorIndex();
        data[1 + sizeof(POINT)] = bCursorIndex;

        ULONG headerOffset = 1 + sizeof(POINT) + sizeof(BYTE);  // 11字节

        // 滚动帧特有字段
        // 方向(1) + 滚动量(4) + 边缘偏移(4) + 边缘长度(4) = 13字节
        data[headerOffset] = direction;
        *(int*)(data + headerOffset + 1) = absScroll;

        int edgeOffset, edgePixelCount;
        m_pScrollDetector->GetEdgeRegion(scrollAmount, &edgeOffset, &edgePixelCount);

        *(int*)(data + headerOffset + 5) = edgeOffset;
        *(int*)(data + headerOffset + 9) = edgePixelCount;

        ULONG scrollHeaderSize = 13;  // 滚动特有头部大小
        LPBYTE edgeData = data + headerOffset + scrollHeaderSize;

        // 复制边缘数据（根据算法格式转换）
        LPBYTE srcEdge = currentFrame + edgeOffset;
        ULONG edgeByteSize;

        switch (algo) {
        case ALGORITHM_RGB565:
            ConvertBGRAtoRGB565(srcEdge, (uint16_t*)edgeData, edgePixelCount);
            edgeByteSize = edgePixelCount * 2;
            break;
        case ALGORITHM_GRAY:
            ConvertToGray_SSE2(edgeData, srcEdge, edgePixelCount * 4);
            edgeByteSize = edgePixelCount;
            break;
        case ALGORITHM_DIFF:
        default:
            memcpy(edgeData, srcEdge, edgePixelCount * 4);
            edgeByteSize = edgePixelCount * 4;
            break;
        }

        *outLength = 1 + headerOffset + scrollHeaderSize + edgeByteSize;

        // 更新 FirstBuffer：应用滚动并复制新边缘数据
        ApplyScrollToFirstBuffer(scrollAmount, currentFrame);

        m_FrameID++;
        return m_RectBuffer;
    }

    // 将滚动应用到 FirstBuffer（必须与服务端 DrawScrollFrame 操作完全一致）
    // 这样下一帧的差分计算才能正确反映实际变化
    virtual void ApplyScrollToFirstBuffer(int scrollAmount, LPBYTE currentFrame)
    {
        int stride = m_BitmapInfor_Send->bmiHeader.biWidth * 4;
        int height = m_BitmapInfor_Send->bmiHeader.biHeight;
        int absScroll = abs(scrollAmount);
        LPBYTE first = GetFirstBuffer();
        int copyHeight = height - absScroll;

        if (scrollAmount > 0) {
            // 向下滚动：BMP 中低地址移到高地址
            memmove(first + absScroll * stride, first, copyHeight * stride);
            // 边缘数据填充到低地址
            memcpy(first, currentFrame, absScroll * stride);
        } else {
            // 向上滚动：BMP 中高地址移到低地址
            memmove(first, first + absScroll * stride, copyHeight * stride);
            // 边缘数据填充到高地址
            memcpy(first + copyHeight * stride, currentFrame + copyHeight * stride, absScroll * stride);
        }
    }

    // SSE2 优化：BGRA 转单通道灰度，一次处理 4 个像素，输出 4 字节
    // 灰度公式: Y = 0.299*R + 0.587*G + 0.114*B ≈ (306*R + 601*G + 117*B) >> 10
    // 输入: BGRA 像素数据 (每像素 4 字节)
    // 输出: 灰度值 (每像素 1 字节)
    // count: 输入数据的字节数 (必须是 4 的倍数)
    inline void ConvertToGray_SSE2(LPBYTE dst, LPBYTE src, ULONG count)
    {
        ULONG pixels = count / 4;
        ULONG i = 0;
        ULONG aligned = pixels & ~3;  // 4 像素对齐

        // 一次处理 4 个像素
        for (; i < aligned; i += 4, src += 16, dst += 4) {
            // 计算 4 个灰度值
            dst[0] = (306 * src[2]  + 601 * src[0]  + 117 * src[1])  >> 10;
            dst[1] = (306 * src[6]  + 601 * src[4]  + 117 * src[5])  >> 10;
            dst[2] = (306 * src[10] + 601 * src[8]  + 117 * src[9])  >> 10;
            dst[3] = (306 * src[14] + 601 * src[12] + 117 * src[13]) >> 10;
        }

        // 处理剩余像素
        for (; i < pixels; i++, src += 4, dst++) {
            *dst = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
        }
    }

    // ToGray: BGRA 转 BGRA 灰度 (三通道相同值)，用于关键帧
    // 直接标量处理，编译器会自动向量化
    void ToGray(LPBYTE dst, LPBYTE src, int biSizeImage)
    {
        ULONG pixels = biSizeImage / 4;
        for (ULONG i = 0; i < pixels; i++, src += 4, dst += 4) {
            BYTE g = (306 * src[2] + 601 * src[0] + 117 * src[1]) >> 10;
            dst[0] = dst[1] = dst[2] = g;
            dst[3] = 0xFF;
        }
    }

    // ===================== RGB565 转换函数 =====================

    // BGRA → RGB565 标量版本 (后备，用于不支持 SSE2 的 CPU)
    // 输入: BGRA 像素数据 (每像素 4 字节)
    // 输出: RGB565 像素数据 (每像素 2 字节)
    // RGB565 格式: RRRRRGGG GGGBBBBB (R:5位, G:6位, B:5位)
    inline void ConvertBGRAtoRGB565_Scalar(const BYTE* src, uint16_t* dst, ULONG pixelCount)
    {
        for (ULONG i = 0; i < pixelCount; i++, src += 4, dst++) {
            // src[0]=B, src[1]=G, src[2]=R, src[3]=A
            *dst = ((src[2] >> 3) << 11) | ((src[1] >> 2) << 5) | (src[0] >> 3);
        }
    }

    // BGRA → RGB565 SSE2 优化版本
    // 一次处理 4 个像素 (16字节输入 → 8字节输出)
    inline void ConvertBGRAtoRGB565_SSE2(const BYTE* src, uint16_t* dst, ULONG pixelCount)
    {
        ULONG i = 0;

        // SSE2 掩码: 提取 R、G、B 高位
        __m128i r_mask = _mm_set1_epi32(0x00F80000);  // R: bit16-23 中取高5位
        __m128i g_mask = _mm_set1_epi32(0x0000FC00);  // G: bit8-15 中取高6位
        __m128i b_mask = _mm_set1_epi32(0x000000F8);  // B: bit0-7 中取高5位

        // 无符号打包技巧：先减去 0x8000，有符号打包后再加回来
        __m128i offset32 = _mm_set1_epi32(0x8000);
        __m128i offset16 = _mm_set1_epi16((short)0x8000);

        // 每次处理 4 个像素
        for (; i + 4 <= pixelCount; i += 4, src += 16, dst += 4) {
            // 加载 4 个 BGRA 像素 [B0G0R0A0 | B1G1R1A1 | B2G2R2A2 | B3G3R3A3]
            __m128i bgra = _mm_loadu_si128((const __m128i*)src);

            // 提取并移位各通道
            __m128i r = _mm_srli_epi32(_mm_and_si128(bgra, r_mask), 8);   // R >> 8 → bit11-15
            __m128i g = _mm_srli_epi32(_mm_and_si128(bgra, g_mask), 5);   // G >> 5 → bit5-10
            __m128i b = _mm_srli_epi32(_mm_and_si128(bgra, b_mask), 3);   // B >> 3 → bit0-4

            // 合并 RGB565 (值范围 0-65535)
            __m128i rgb565_32 = _mm_or_si128(_mm_or_si128(r, g), b);

            // 无符号 32→16 打包：减去 0x8000 使其变成有符号范围
            __m128i rgb565_signed = _mm_sub_epi32(rgb565_32, offset32);
            // 有符号饱和打包 (现在不会截断了)
            __m128i packed_signed = _mm_packs_epi32(rgb565_signed, _mm_setzero_si128());
            // 加回 0x8000 还原为无符号值
            __m128i packed = _mm_add_epi16(packed_signed, offset16);

            // 存储 4 个 RGB565 像素 (8字节)
            _mm_storel_epi64((__m128i*)dst, packed);
        }

        // 处理剩余像素 (标量)
        for (; i < pixelCount; i++, src += 4, dst++) {
            *dst = ((src[2] >> 3) << 11) | ((src[1] >> 2) << 5) | (src[0] >> 3);
        }
    }

    // BGRA → RGB565 运行时分发 (根据 CPU 特性自动选择)
    inline void ConvertBGRAtoRGB565(const BYTE* src, uint16_t* dst, ULONG pixelCount)
    {
        if (m_nInstructionSet && HasSSE2()) {
            ConvertBGRAtoRGB565_SSE2(src, dst, pixelCount);
        } else {
            ConvertBGRAtoRGB565_Scalar(src, dst, pixelCount);
        }
    }

    virtual LPBITMAPINFO ConstructBitmapInfo(int biBitCount, int biWidth, int biHeight)
    {
        assert(biBitCount == 32);
        BITMAPINFO* bmpInfo = (BITMAPINFO*) new BYTE[sizeof(BITMAPINFO)]();
        bmpInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmpInfo->bmiHeader.biWidth = biWidth;
        bmpInfo->bmiHeader.biHeight = biHeight;
        bmpInfo->bmiHeader.biPlanes = 1;
        bmpInfo->bmiHeader.biBitCount = biBitCount;
        bmpInfo->bmiHeader.biCompression = BI_RGB;
        bmpInfo->bmiHeader.biSizeImage = biWidth * biHeight * 4;
        return bmpInfo;
    }

    // 算法+光标位置+光标类型
    virtual LPBYTE GetNextScreenData(ULONG* ulNextSendLength)
    {
        BYTE algo = m_bAlgorithm;
        int frameID = m_FrameID + 1;
        bool keyFrame = (frameID % m_GOP == 0);
        m_RectBuffer[0] = keyFrame ? TOKEN_KEYFRAME : TOKEN_NEXTSCREEN;
        LPBYTE data = m_RectBuffer + 1;
        // 写入使用了哪种算法
        memcpy(data, (LPBYTE)&algo, sizeof(BYTE));

        // 写入光标位置
        POINT	CursorPos;
        GetCursorPos(&CursorPos);
        CursorPos.x /= m_wZoom;
        CursorPos.y /= m_hZoom;
        memcpy(data + sizeof(BYTE), (LPBYTE)&CursorPos, sizeof(POINT));

        // 写入当前光标类型
        static CCursorInfo m_CursorInfor;
        BYTE	bCursorIndex = m_CursorInfor.getCurrentCursorIndex();
        memcpy(data + sizeof(BYTE) + sizeof(POINT), &bCursorIndex, sizeof(BYTE));
        ULONG offset = sizeof(BYTE) + sizeof(POINT) + sizeof(BYTE);

        // 分段扫描全屏幕  将新的位图放入到m_hDiffMemDC中
        LPBYTE nextData = ScanNextScreen();
        if (nullptr == nextData) {
            // 扫描下一帧失败也需要发送光标信息到控制端
            *ulNextSendLength = 1 + offset;
            return m_RectBuffer;
        }

        // 滚动检测：在非关键帧且启用滚动检测时进行
        // 注意：H264 模式跳过（编码器内置运动估计会自动处理）
        // 如果上一帧是滚动帧，则跳过本帧的滚动检测，发送差分帧来同步可能的误差
        // m_nScrollDetectInterval: 0=禁用, 1=每帧, 2=每2帧, ...
        bool shouldDetectScroll = !keyFrame && algo != ALGORITHM_H264 &&
                                  m_bEnableScrollDetect && m_bServerSupportsScroll && m_pScrollDetector &&
                                  !m_bLastFrameWasScroll && m_nScrollDetectInterval > 0 &&
                                  (m_FrameID % m_nScrollDetectInterval == 0);

        if (shouldDetectScroll) {
            int scrollAmount = m_pScrollDetector->DetectVerticalScroll(GetFirstBuffer(), nextData);
            if (scrollAmount != 0 && abs(scrollAmount) >= MIN_SCROLL_LINES) {
                // 检测到滚动，构建滚动帧
                m_bLastFrameWasScroll = true;
                return BuildScrollFrame(scrollAmount, nextData, ulNextSendLength);
            }
        }
        m_bLastFrameWasScroll = false;

#if SCREENYSPY_IMPROVE
        memcpy(data + offset, &++m_FrameID, sizeof(int));
        offset += sizeof(int);
#if SCREENSPY_WRITE
        WriteBitmap(m_BitmapInfor_Send, nextData, "GHOST", m_FrameID);
#endif
#else
        m_FrameID++;
#endif

        if (keyFrame) {
            switch (algo) {
            case ALGORITHM_DIFF: {
                *ulNextSendLength = 1 + offset + m_BitmapInfor_Send->bmiHeader.biSizeImage;
                memcpy(data + offset, nextData, m_BitmapInfor_Send->bmiHeader.biSizeImage);
                break;
            }
            case ALGORITHM_GRAY: {
                *ulNextSendLength = 1 + offset + m_BitmapInfor_Send->bmiHeader.biSizeImage;
                ToGray(data + offset, nextData, m_BitmapInfor_Send->bmiHeader.biSizeImage);
                break;
            }
            case ALGORITHM_RGB565: {
                // RGB565 关键帧：BGRA 整帧转换为 RGB565
                ULONG pixelCount = m_BitmapInfor_Send->bmiHeader.biSizeImage / 4;
                ULONG rgb565Size = pixelCount * 2;
                ConvertBGRAtoRGB565(nextData, (uint16_t*)(data + offset), pixelCount);
                *ulNextSendLength = 1 + offset + rgb565Size;
                break;
            }
            case ALGORITHM_H264: {
                uint8_t* encoded_data = nullptr;
                uint32_t  encoded_size = 0;
                int width = m_BitmapInfor_Send->bmiHeader.biWidth, height = m_BitmapInfor_Send->bmiHeader.biHeight;
                if (m_encoder == nullptr) {
                    m_encoder = new CX264Encoder();
                    int bitrate = (m_nBitRate > 0) ? m_nBitRate : (width * height / 1266);
                    m_encoder->open(width, height, 20, bitrate);
                }
                int err = m_encoder->encode(nextData, 32, 4 * width, width, height, &encoded_data, &encoded_size);
                if (err) {
                    return nullptr;
                }
                *ulNextSendLength = 1 + offset + encoded_size;
                memcpy(data + offset, encoded_data, encoded_size);
                break;
            }
            default:
                break;
            }
            memcpy(GetFirstBuffer(), nextData, m_BitmapInfor_Send->bmiHeader.biSizeImage);
        } else {
            switch (algo) {
            case ALGORITHM_DIFF:
            case ALGORITHM_GRAY:
            case ALGORITHM_RGB565: {
                // DIFF/GRAY/RGB565 都走差分逻辑，区别在 CompareBitmap 内部处理
                *ulNextSendLength = 1 + offset + MultiCompareBitmap(nextData, GetFirstBuffer(), data + offset, GetBMPSize(), algo);
                break;
            }
            case ALGORITHM_H264: {
                uint8_t* encoded_data = nullptr;
                uint32_t  encoded_size = 0;
                int width = m_BitmapInfor_Send->bmiHeader.biWidth, height = m_BitmapInfor_Send->bmiHeader.biHeight;
                if (m_encoder == nullptr) {
                    m_encoder = new CX264Encoder();
                    int bitrate = (m_nBitRate > 0) ? m_nBitRate : (width * height / 1266);
                    m_encoder->open(width, height, 20, bitrate);
                }
                int err = m_encoder->encode(nextData, 32, 4 * width, width, height, &encoded_data, &encoded_size);
                if (err) {
                    return nullptr;
                }
                *ulNextSendLength = 1 + offset + encoded_size;
                memcpy(data + offset, encoded_data, encoded_size);
                break;
            }
            default:
                break;
            }
        }

        return m_RectBuffer;
    }

    // 获取屏幕传输算法
    virtual BYTE GetAlgorithm() const
    {
        return m_bAlgorithm;
    }

    // 设置屏幕传输算法
    virtual BYTE SetAlgorithm(int algo)
    {
        BYTE oldAlgo = m_bAlgorithm;
        m_bAlgorithm = algo;
        return oldAlgo;
    }

    // 设置 H264 编码码率 (kbps), 0=自动计算
    // 返回码率是否变化，调用者需要在变化时 RestartScreen
    virtual bool SetBitRate(int bitrate)
    {
        if (m_nBitRate == bitrate)
            return false;
        m_nBitRate = bitrate;
        return true;
    }

    virtual int GetBitRate() const
    {
        return m_nBitRate;
    }

    // 鼠标位置转换
    virtual void PointConversion(POINT& pt) const
    {
        if (m_bZoomed) {
            pt.x *= m_wZoom;
            pt.y *= m_hZoom;
        }
        pt.x += m_iScreenX;
        pt.y += m_iScreenY;
    }

    // 获取位图结构信息
    virtual const LPBITMAPINFO& GetBIData() const
    {
        return m_BitmapInfor_Send;
    }

public: // 纯虚接口

    // 获取第一帧屏幕
    virtual LPBYTE GetFirstScreenData(ULONG* ulFirstScreenLength) = 0;

    // 获取下一帧屏幕
    virtual LPBYTE ScanNextScreen() = 0;

    virtual LPBYTE scaleBitmap(LPBYTE target, LPBYTE bitmap)
    {
        if (m_ulFullWidth == m_BitmapInfor_Send->bmiHeader.biWidth && m_ulFullHeight == m_BitmapInfor_Send->bmiHeader.biHeight)
            return bitmap;
        return ScaleBitmap(target, (uint8_t*)bitmap, m_ulFullWidth, m_ulFullHeight, m_BitmapInfor_Send->bmiHeader.biWidth,
                           m_BitmapInfor_Send->bmiHeader.biHeight, m_nInstructionSet);
    }
};
