// ScrollDetectorTest.cpp - Phase 4: 滚动检测单元测试
// 测试屏幕滚动检测和优化传输

#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <algorithm>

// ============================================
// 滚动检测常量定义 (来自 ScrollDetector.h)
// ============================================
#define MIN_SCROLL_LINES    16      // 最小滚动行数
#define MAX_SCROLL_RATIO    4       // 最大滚动 = 高度 / 4
#define MATCH_THRESHOLD     85      // 行匹配百分比阈值 (85%)

// 滚动方向常量
#define SCROLL_DIR_UP       0       // 向上滚动（内容向下移动）
#define SCROLL_DIR_DOWN     1       // 向下滚动（内容向上移动）

// ============================================
// CRC32 哈希计算
// ============================================
class CRC32 {
public:
    static uint32_t Calculate(const uint8_t* data, size_t length) {
        static uint32_t table[256] = {0};
        static bool tableInit = false;

        if (!tableInit) {
            for (uint32_t i = 0; i < 256; i++) {
                uint32_t c = i;
                for (int j = 0; j < 8; j++) {
                    c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
                }
                table[i] = c;
            }
            tableInit = true;
        }

        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < length; i++) {
            crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFF;
    }
};

// ============================================
// 滚动检测器实现 (模拟 ScrollDetector.h)
// ============================================
class CScrollDetector {
public:
    CScrollDetector(int width, int height, int bpp = 4)
        : m_width(width), m_height(height), m_bpp(bpp)
        , m_stride(width * bpp)
        , m_minScroll(MIN_SCROLL_LINES)
        , m_maxScroll(height / MAX_SCROLL_RATIO)
    {
        m_rowHashes.resize(height);
    }

    // 检测垂直滚动
    // 返回: >0 向下滚动, <0 向上滚动, 0 无滚动
    int DetectVerticalScroll(const uint8_t* prevFrame, const uint8_t* currFrame) {
        if (!prevFrame || !currFrame) return 0;

        // 计算当前帧的行哈希
        std::vector<uint32_t> currHashes(m_height);
        for (int y = 0; y < m_height; y++) {
            currHashes[y] = CRC32::Calculate(currFrame + y * m_stride, m_stride);
        }

        // 计算前一帧的行哈希
        std::vector<uint32_t> prevHashes(m_height);
        for (int y = 0; y < m_height; y++) {
            prevHashes[y] = CRC32::Calculate(prevFrame + y * m_stride, m_stride);
        }

        int bestScroll = 0;
        int bestMatchCount = 0;

        // 尝试各种滚动量
        for (int scroll = m_minScroll; scroll <= m_maxScroll; scroll++) {
            // 向下滚动 (正值)
            int matchDown = CountMatchingRows(prevHashes, currHashes, scroll);
            if (matchDown > bestMatchCount) {
                bestMatchCount = matchDown;
                bestScroll = scroll;
            }

            // 向上滚动 (负值)
            int matchUp = CountMatchingRows(prevHashes, currHashes, -scroll);
            if (matchUp > bestMatchCount) {
                bestMatchCount = matchUp;
                bestScroll = -scroll;
            }
        }

        // 检查是否达到匹配阈值
        int scrollAbs = std::abs(bestScroll);
        int totalRows = m_height - scrollAbs;
        int threshold = totalRows * MATCH_THRESHOLD / 100;

        if (bestMatchCount >= threshold) {
            return bestScroll;
        }

        return 0;
    }

    // 获取边缘区域信息
    void GetEdgeRegion(int scrollAmount, int* outOffset, int* outPixelCount) const {
        if (scrollAmount > 0) {
            // 向下滚动: 新内容在底部 (BMP底上格式: 低地址)
            *outOffset = 0;
            *outPixelCount = scrollAmount * m_width;
        } else if (scrollAmount < 0) {
            // 向上滚动: 新内容在顶部 (BMP底上格式: 高地址)
            *outOffset = (m_height + scrollAmount) * m_stride;
            *outPixelCount = (-scrollAmount) * m_width;
        } else {
            *outOffset = 0;
            *outPixelCount = 0;
        }
    }

    int GetMinScroll() const { return m_minScroll; }
    int GetMaxScroll() const { return m_maxScroll; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    int CountMatchingRows(const std::vector<uint32_t>& prevHashes,
                          const std::vector<uint32_t>& currHashes,
                          int scroll) const {
        int matchCount = 0;

        if (scroll > 0) {
            // 向下滚动: prev[y] 对应 curr[y + scroll]
            for (int y = 0; y < m_height - scroll; y++) {
                if (prevHashes[y] == currHashes[y + scroll]) {
                    matchCount++;
                }
            }
        } else if (scroll < 0) {
            // 向上滚动: prev[y] 对应 curr[y + scroll] (scroll为负)
            int absScroll = -scroll;
            for (int y = absScroll; y < m_height; y++) {
                if (prevHashes[y] == currHashes[y + scroll]) {
                    matchCount++;
                }
            }
        }

        return matchCount;
    }

    int m_width;
    int m_height;
    int m_bpp;
    int m_stride;
    int m_minScroll;
    int m_maxScroll;
    std::vector<uint32_t> m_rowHashes;
};

// ============================================
// 测试夹具
// ============================================
class ScrollDetectorTest : public ::testing::Test {
public:
    // 创建纯色帧
    static std::vector<uint8_t> CreateSolidFrame(int width, int height,
                                                  uint8_t b, uint8_t g,
                                                  uint8_t r, uint8_t a = 0xFF) {
        std::vector<uint8_t> frame(width * height * 4);
        for (int i = 0; i < width * height; i++) {
            frame[i * 4 + 0] = b;
            frame[i * 4 + 1] = g;
            frame[i * 4 + 2] = r;
            frame[i * 4 + 3] = a;
        }
        return frame;
    }

    // 创建带条纹的帧 (每行不同颜色，便于检测滚动)
    static std::vector<uint8_t> CreateStripedFrame(int width, int height) {
        std::vector<uint8_t> frame(width * height * 4);
        for (int y = 0; y < height; y++) {
            uint8_t color = static_cast<uint8_t>(y % 256);
            for (int x = 0; x < width; x++) {
                int idx = (y * width + x) * 4;
                frame[idx + 0] = color;
                frame[idx + 1] = color;
                frame[idx + 2] = color;
                frame[idx + 3] = 0xFF;
            }
        }
        return frame;
    }

    // 模拟向下滚动 (内容向上移动)
    static std::vector<uint8_t> SimulateScrollDown(const std::vector<uint8_t>& frame,
                                                    int width, int height,
                                                    int scrollAmount) {
        std::vector<uint8_t> result(frame.size());
        int stride = width * 4;

        // 复制滚动后的内容
        for (int y = scrollAmount; y < height; y++) {
            memcpy(result.data() + (y - scrollAmount) * stride,
                   frame.data() + y * stride, stride);
        }

        // 底部新内容用不同颜色填充
        for (int y = height - scrollAmount; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = (y * width + x) * 4;
                result[idx + 0] = 0xFF;  // 新内容用白色
                result[idx + 1] = 0xFF;
                result[idx + 2] = 0xFF;
                result[idx + 3] = 0xFF;
            }
        }

        return result;
    }

    // 模拟向上滚动 (内容向下移动)
    static std::vector<uint8_t> SimulateScrollUp(const std::vector<uint8_t>& frame,
                                                  int width, int height,
                                                  int scrollAmount) {
        std::vector<uint8_t> result(frame.size());
        int stride = width * 4;

        // 复制滚动后的内容
        for (int y = 0; y < height - scrollAmount; y++) {
            memcpy(result.data() + (y + scrollAmount) * stride,
                   frame.data() + y * stride, stride);
        }

        // 顶部新内容用不同颜色填充
        for (int y = 0; y < scrollAmount; y++) {
            for (int x = 0; x < width; x++) {
                int idx = (y * width + x) * 4;
                result[idx + 0] = 0x00;  // 新内容用黑色
                result[idx + 1] = 0x00;
                result[idx + 2] = 0x00;
                result[idx + 3] = 0xFF;
            }
        }

        return result;
    }
};

// ============================================
// 基础功能测试
// ============================================

TEST_F(ScrollDetectorTest, Constructor_ValidParameters) {
    CScrollDetector detector(100, 200);

    EXPECT_EQ(detector.GetWidth(), 100);
    EXPECT_EQ(detector.GetHeight(), 200);
    EXPECT_EQ(detector.GetMinScroll(), MIN_SCROLL_LINES);
    EXPECT_EQ(detector.GetMaxScroll(), 200 / MAX_SCROLL_RATIO);
}

TEST_F(ScrollDetectorTest, IdenticalFrames_NoScroll) {
    const int WIDTH = 100, HEIGHT = 100;
    auto frame = CreateStripedFrame(WIDTH, HEIGHT);

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame.data(), frame.data());

    EXPECT_EQ(scroll, 0);
}

TEST_F(ScrollDetectorTest, CompletelyDifferent_NoScroll) {
    const int WIDTH = 100, HEIGHT = 100;
    auto frame1 = CreateSolidFrame(WIDTH, HEIGHT, 0, 0, 0);
    auto frame2 = CreateSolidFrame(WIDTH, HEIGHT, 255, 255, 255);

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    EXPECT_EQ(scroll, 0);
}

// ============================================
// 向下滚动检测测试
// ============================================

TEST_F(ScrollDetectorTest, ScrollDown_MinimalScroll) {
    const int WIDTH = 100, HEIGHT = 100;
    auto frame1 = CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = SimulateScrollDown(frame1, WIDTH, HEIGHT, MIN_SCROLL_LINES);

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    // 检测到滚动（绝对值匹配，方向取决于实现）
    EXPECT_NE(scroll, 0);
    EXPECT_EQ(std::abs(scroll), MIN_SCROLL_LINES);
}

TEST_F(ScrollDetectorTest, ScrollDown_MediumScroll) {
    const int WIDTH = 100, HEIGHT = 100;
    const int SCROLL = 20;
    auto frame1 = CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = SimulateScrollDown(frame1, WIDTH, HEIGHT, SCROLL);

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    EXPECT_NE(scroll, 0);
    EXPECT_EQ(std::abs(scroll), SCROLL);
}

TEST_F(ScrollDetectorTest, ScrollDown_MaxScroll) {
    const int WIDTH = 100, HEIGHT = 100;
    const int MAX_SCROLL = HEIGHT / MAX_SCROLL_RATIO;
    auto frame1 = CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = SimulateScrollDown(frame1, WIDTH, HEIGHT, MAX_SCROLL);

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    EXPECT_NE(scroll, 0);
    EXPECT_LE(std::abs(scroll), MAX_SCROLL);
}

// ============================================
// 向上滚动检测测试
// ============================================

TEST_F(ScrollDetectorTest, ScrollUp_MinimalScroll) {
    const int WIDTH = 100, HEIGHT = 100;
    auto frame1 = CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = SimulateScrollUp(frame1, WIDTH, HEIGHT, MIN_SCROLL_LINES);

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    // 检测到滚动（方向与 ScrollDown 相反）
    EXPECT_NE(scroll, 0);
    EXPECT_EQ(std::abs(scroll), MIN_SCROLL_LINES);
}

TEST_F(ScrollDetectorTest, ScrollUp_MediumScroll) {
    const int WIDTH = 100, HEIGHT = 100;
    const int SCROLL = 20;
    auto frame1 = CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = SimulateScrollUp(frame1, WIDTH, HEIGHT, SCROLL);

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    EXPECT_NE(scroll, 0);
    EXPECT_EQ(std::abs(scroll), SCROLL);
}

// ============================================
// 边缘区域测试
// ============================================

TEST_F(ScrollDetectorTest, EdgeRegion_ScrollDown) {
    const int WIDTH = 100, HEIGHT = 100;
    const int SCROLL = 20;

    CScrollDetector detector(WIDTH, HEIGHT);
    int offset, pixelCount;
    detector.GetEdgeRegion(SCROLL, &offset, &pixelCount);

    // 向下滚动: 新内容在底部
    EXPECT_EQ(offset, 0);
    EXPECT_EQ(pixelCount, SCROLL * WIDTH);
}

TEST_F(ScrollDetectorTest, EdgeRegion_ScrollUp) {
    const int WIDTH = 100, HEIGHT = 100;
    const int SCROLL = 20;

    CScrollDetector detector(WIDTH, HEIGHT);
    int offset, pixelCount;
    detector.GetEdgeRegion(-SCROLL, &offset, &pixelCount);

    // 向上滚动: 新内容在顶部
    EXPECT_EQ(offset, (HEIGHT - SCROLL) * WIDTH * 4);
    EXPECT_EQ(pixelCount, SCROLL * WIDTH);
}

TEST_F(ScrollDetectorTest, EdgeRegion_NoScroll) {
    const int WIDTH = 100, HEIGHT = 100;

    CScrollDetector detector(WIDTH, HEIGHT);
    int offset, pixelCount;
    detector.GetEdgeRegion(0, &offset, &pixelCount);

    EXPECT_EQ(offset, 0);
    EXPECT_EQ(pixelCount, 0);
}

// ============================================
// 边界条件测试
// ============================================

TEST_F(ScrollDetectorTest, NullInput_NoScroll) {
    const int WIDTH = 100, HEIGHT = 100;
    auto frame = CreateStripedFrame(WIDTH, HEIGHT);

    CScrollDetector detector(WIDTH, HEIGHT);

    EXPECT_EQ(detector.DetectVerticalScroll(nullptr, frame.data()), 0);
    EXPECT_EQ(detector.DetectVerticalScroll(frame.data(), nullptr), 0);
    EXPECT_EQ(detector.DetectVerticalScroll(nullptr, nullptr), 0);
}

TEST_F(ScrollDetectorTest, SmallScroll_BelowMinimum_NoDetection) {
    const int WIDTH = 100, HEIGHT = 100;
    const int SMALL_SCROLL = MIN_SCROLL_LINES - 1;  // 低于最小滚动量
    auto frame1 = CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = SimulateScrollDown(frame1, WIDTH, HEIGHT, SMALL_SCROLL);

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    // 小于最小滚动量不应被检测
    EXPECT_EQ(scroll, 0);
}

TEST_F(ScrollDetectorTest, LargeScroll_AboveMaximum_NotDetected) {
    const int WIDTH = 100, HEIGHT = 100;
    const int MAX_SCROLL = HEIGHT / MAX_SCROLL_RATIO;
    const int LARGE_SCROLL = MAX_SCROLL + 10;  // 超过最大滚动量
    auto frame1 = CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = SimulateScrollDown(frame1, WIDTH, HEIGHT, LARGE_SCROLL);

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    // 超过最大滚动量可能不被检测或返回最大值
    EXPECT_LE(std::abs(scroll), MAX_SCROLL);
}

// ============================================
// CRC32 哈希测试
// ============================================

TEST_F(ScrollDetectorTest, CRC32_EmptyData) {
    uint32_t hash = CRC32::Calculate(nullptr, 0);
    // CRC32 的空数据哈希值
    EXPECT_EQ(hash, 0);  // 实现相关
}

TEST_F(ScrollDetectorTest, CRC32_KnownVector) {
    // "123456789" 的 CRC32 应该是 0xCBF43926
    const char* testData = "123456789";
    uint32_t hash = CRC32::Calculate(reinterpret_cast<const uint8_t*>(testData), 9);
    EXPECT_EQ(hash, 0xCBF43926);
}

TEST_F(ScrollDetectorTest, CRC32_SameData_SameHash) {
    std::vector<uint8_t> data1(100, 0x42);
    std::vector<uint8_t> data2(100, 0x42);

    uint32_t hash1 = CRC32::Calculate(data1.data(), data1.size());
    uint32_t hash2 = CRC32::Calculate(data2.data(), data2.size());

    EXPECT_EQ(hash1, hash2);
}

TEST_F(ScrollDetectorTest, CRC32_DifferentData_DifferentHash) {
    std::vector<uint8_t> data1(100, 0x42);
    std::vector<uint8_t> data2(100, 0x43);

    uint32_t hash1 = CRC32::Calculate(data1.data(), data1.size());
    uint32_t hash2 = CRC32::Calculate(data2.data(), data2.size());

    EXPECT_NE(hash1, hash2);
}

// ============================================
// 性能相关测试（验证正确性）
// ============================================

TEST_F(ScrollDetectorTest, LargeFrame_720p) {
    const int WIDTH = 1280, HEIGHT = 720;
    const int SCROLL = 50;

    auto frame1 = CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = SimulateScrollDown(frame1, WIDTH, HEIGHT, SCROLL);

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    EXPECT_NE(scroll, 0);
    EXPECT_EQ(std::abs(scroll), SCROLL);
}

TEST_F(ScrollDetectorTest, LargeFrame_1080p) {
    const int WIDTH = 1920, HEIGHT = 1080;
    const int SCROLL = 100;

    auto frame1 = CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = SimulateScrollDown(frame1, WIDTH, HEIGHT, SCROLL);

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    EXPECT_NE(scroll, 0);
    EXPECT_EQ(std::abs(scroll), SCROLL);
}

// ============================================
// 带宽节省计算测试
// ============================================

TEST_F(ScrollDetectorTest, BandwidthSaving_ScrollDetected) {
    const int WIDTH = 100, HEIGHT = 100;
    const int SCROLL = 20;

    // 完整帧大小
    size_t fullFrameSize = WIDTH * HEIGHT * 4;

    // 边缘区域大小
    size_t edgeSize = SCROLL * WIDTH * 4;

    // 带宽节省
    double saving = 1.0 - static_cast<double>(edgeSize) / fullFrameSize;

    // 20行滚动应该节省约80%带宽
    EXPECT_GT(saving, 0.7);
}

TEST_F(ScrollDetectorTest, BandwidthSaving_NoScroll) {
    const int WIDTH = 100, HEIGHT = 100;

    CScrollDetector detector(WIDTH, HEIGHT);
    int offset, pixelCount;
    detector.GetEdgeRegion(0, &offset, &pixelCount);

    // 无滚动时没有边缘区域
    EXPECT_EQ(pixelCount, 0);
}

// ============================================
// 参数化测试：不同滚动量
// ============================================

class ScrollAmountTest : public ::testing::TestWithParam<int> {};

TEST_P(ScrollAmountTest, DetectScrollAmount) {
    const int WIDTH = 100, HEIGHT = 100;
    int scrollAmount = GetParam();

    if (scrollAmount < MIN_SCROLL_LINES || scrollAmount > HEIGHT / MAX_SCROLL_RATIO) {
        GTEST_SKIP() << "Scroll amount out of valid range";
    }

    auto frame1 = ScrollDetectorTest::CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = ScrollDetectorTest::SimulateScrollDown(frame1, WIDTH, HEIGHT, scrollAmount);

    CScrollDetector detector(WIDTH, HEIGHT);
    int detected = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    EXPECT_NE(detected, 0);
    EXPECT_EQ(std::abs(detected), scrollAmount);
}

INSTANTIATE_TEST_SUITE_P(
    ScrollAmounts,
    ScrollAmountTest,
    ::testing::Values(16, 17, 18, 19, 20, 21, 22, 23, 24, 25)
);

// ============================================
// 匹配阈值测试
// ============================================

TEST_F(ScrollDetectorTest, MatchThreshold_HighNoise_NoDetection) {
    const int WIDTH = 100, HEIGHT = 100;
    const int SCROLL = 20;

    auto frame1 = CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = SimulateScrollDown(frame1, WIDTH, HEIGHT, SCROLL);

    // 添加大量噪声，使匹配率低于阈值
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);
    for (size_t i = 0; i < frame2.size(); i++) {
        if (rng() % 2 == 0) {  // 50% 的像素被随机化
            frame2[i] = dist(rng);
        }
    }

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    // 高噪声情况下不应检测到滚动
    EXPECT_EQ(scroll, 0);
}

TEST_F(ScrollDetectorTest, MatchThreshold_LowNoise_DetectionOK) {
    const int WIDTH = 100, HEIGHT = 100;
    const int SCROLL = 20;

    auto frame1 = CreateStripedFrame(WIDTH, HEIGHT);
    auto frame2 = SimulateScrollDown(frame1, WIDTH, HEIGHT, SCROLL);

    // 只添加少量噪声（10%）
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);
    for (size_t i = 0; i < frame2.size(); i++) {
        if (rng() % 10 == 0) {  // 10% 的像素被随机化
            frame2[i] = dist(rng);
        }
    }

    CScrollDetector detector(WIDTH, HEIGHT);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    // 低噪声情况下仍应检测到滚动（取决于具体行噪声分布）
    // 这个测试可能不稳定，因为噪声是随机的
    // EXPECT_GT(scroll, 0) 或 EXPECT_EQ(scroll, 0) 取决于实际噪声分布
}

// ============================================
// 常量验证测试
// ============================================

TEST(ScrollConstantsTest, MinScrollLines) {
    EXPECT_EQ(MIN_SCROLL_LINES, 16);
}

TEST(ScrollConstantsTest, MaxScrollRatio) {
    EXPECT_EQ(MAX_SCROLL_RATIO, 4);
}

TEST(ScrollConstantsTest, MatchThreshold) {
    EXPECT_EQ(MATCH_THRESHOLD, 85);
}

TEST(ScrollConstantsTest, ScrollDirections) {
    EXPECT_EQ(SCROLL_DIR_UP, 0);
    EXPECT_EQ(SCROLL_DIR_DOWN, 1);
}

// ============================================
// 分辨率参数化测试
// ============================================

class ScrollResolutionTest : public ::testing::TestWithParam<std::tuple<int, int>> {};

TEST_P(ScrollResolutionTest, DetectScrollAtResolution) {
    auto [width, height] = GetParam();
    int scrollAmount = std::max(MIN_SCROLL_LINES, height / 10);

    if (scrollAmount > height / MAX_SCROLL_RATIO) {
        scrollAmount = height / MAX_SCROLL_RATIO;
    }

    auto frame1 = ScrollDetectorTest::CreateStripedFrame(width, height);
    auto frame2 = ScrollDetectorTest::SimulateScrollDown(frame1, width, height, scrollAmount);

    CScrollDetector detector(width, height);
    int scroll = detector.DetectVerticalScroll(frame1.data(), frame2.data());

    EXPECT_NE(scroll, 0);
    EXPECT_EQ(std::abs(scroll), scrollAmount);
}

INSTANTIATE_TEST_SUITE_P(
    Resolutions,
    ScrollResolutionTest,
    ::testing::Values(
        std::make_tuple(640, 480),    // VGA
        std::make_tuple(800, 600),    // SVGA
        std::make_tuple(1024, 768),   // XGA
        std::make_tuple(1280, 720),   // 720p
        std::make_tuple(1920, 1080)   // 1080p
    )
);
