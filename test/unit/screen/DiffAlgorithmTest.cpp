// DiffAlgorithmTest.cpp - Phase 4: 差分算法单元测试
// 测试帧间差异计算和编码逻辑

#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <random>

// ============================================
// 算法常量定义 (来自 CursorInfo.h)
// ============================================
#define ALGORITHM_GRAY    0      // 灰度算法
#define ALGORITHM_DIFF    1      // 差分算法（默认）
#define ALGORITHM_H264    2      // H264 视频编码
#define ALGORITHM_RGB565  3      // RGB565 压缩

// ============================================
// 差分输出结构
// ============================================
struct DiffRegion {
    uint32_t offset;     // 字节偏移
    uint32_t length;     // 长度（含义取决于算法）
    std::vector<uint8_t> data;  // 差异数据
};

// ============================================
// 测试用差分算法实现
// 模拟 ScreenCapture.h 中的 CompareBitmapDXGI
// ============================================
class DiffAlgorithm {
public:
    // 比较两帧，返回差异区域列表
    // 输出格式: [offset:4][length:4][data:N]...
    static std::vector<DiffRegion> CompareBitmap(
        const uint8_t* srcData,    // 新帧
        const uint8_t* dstData,    // 旧帧
        size_t dataLength,         // 数据长度（字节，必须是4的倍数）
        int algorithm              // 压缩算法
    ) {
        std::vector<DiffRegion> regions;

        if (dataLength == 0 || dataLength % 4 != 0) {
            return regions;
        }

        const uint32_t* src32 = reinterpret_cast<const uint32_t*>(srcData);
        const uint32_t* dst32 = reinterpret_cast<const uint32_t*>(dstData);
        size_t pixelCount = dataLength / 4;

        size_t i = 0;
        while (i < pixelCount) {
            // 找到差异起始点
            while (i < pixelCount && src32[i] == dst32[i]) {
                i++;
            }

            if (i >= pixelCount) break;

            // 记录起始位置
            size_t startPos = i;

            // 找到差异结束点
            while (i < pixelCount && src32[i] != dst32[i]) {
                i++;
            }

            // 创建差异区域
            DiffRegion region;
            region.offset = static_cast<uint32_t>(startPos * 4);  // 字节偏移

            size_t diffPixels = i - startPos;
            const uint8_t* pixelStart = srcData + startPos * 4;

            switch (algorithm) {
                case ALGORITHM_GRAY: {
                    // 灰度: 1字节/像素
                    region.length = static_cast<uint32_t>(diffPixels);  // 像素数
                    region.data.resize(diffPixels);
                    for (size_t p = 0; p < diffPixels; p++) {
                        const uint8_t* pixel = pixelStart + p * 4;
                        // BGRA格式: B=0, G=1, R=2, A=3
                        // 灰度公式: Y = 0.299*R + 0.587*G + 0.114*B
                        int gray = (306 * pixel[2] + 601 * pixel[1] + 117 * pixel[0]) >> 10;
                        region.data[p] = static_cast<uint8_t>(std::min(255, std::max(0, gray)));
                    }
                    break;
                }

                case ALGORITHM_RGB565: {
                    // RGB565: 2字节/像素
                    region.length = static_cast<uint32_t>(diffPixels);  // 像素数
                    region.data.resize(diffPixels * 2);
                    uint16_t* out = reinterpret_cast<uint16_t*>(region.data.data());
                    for (size_t p = 0; p < diffPixels; p++) {
                        const uint8_t* pixel = pixelStart + p * 4;
                        // BGRA -> RGB565
                        out[p] = ((pixel[2] >> 3) << 11) |  // R: 5位
                                 ((pixel[1] >> 2) << 5) |   // G: 6位
                                 (pixel[0] >> 3);            // B: 5位
                    }
                    break;
                }

                case ALGORITHM_DIFF:
                case ALGORITHM_H264:
                default: {
                    // DIFF/H264: 4字节/像素，原始BGRA
                    region.length = static_cast<uint32_t>(diffPixels * 4);  // 字节数
                    region.data.resize(diffPixels * 4);
                    memcpy(region.data.data(), pixelStart, diffPixels * 4);
                    break;
                }
            }

            regions.push_back(region);
        }

        return regions;
    }

    // 序列化差异区域到缓冲区
    static size_t SerializeDiffRegions(
        const std::vector<DiffRegion>& regions,
        uint8_t* buffer,
        size_t bufferSize
    ) {
        size_t offset = 0;

        for (const auto& region : regions) {
            size_t needed = 8 + region.data.size();  // offset(4) + length(4) + data
            if (offset + needed > bufferSize) break;

            memcpy(buffer + offset, &region.offset, 4);
            offset += 4;
            memcpy(buffer + offset, &region.length, 4);
            offset += 4;
            memcpy(buffer + offset, region.data.data(), region.data.size());
            offset += region.data.size();
        }

        return offset;
    }

    // 应用差异到目标帧
    static void ApplyDiff(
        uint8_t* dstData,
        size_t dstLength,
        const std::vector<DiffRegion>& regions,
        int algorithm
    ) {
        for (const auto& region : regions) {
            if (region.offset >= dstLength) continue;

            uint8_t* dst = dstData + region.offset;

            switch (algorithm) {
                case ALGORITHM_GRAY: {
                    // 灰度 -> BGRA
                    for (uint32_t p = 0; p < region.length && region.offset + p * 4 < dstLength; p++) {
                        uint8_t gray = region.data[p];
                        dst[p * 4 + 0] = gray;  // B
                        dst[p * 4 + 1] = gray;  // G
                        dst[p * 4 + 2] = gray;  // R
                        dst[p * 4 + 3] = 0xFF;  // A
                    }
                    break;
                }

                case ALGORITHM_RGB565: {
                    // RGB565 -> BGRA
                    const uint16_t* src = reinterpret_cast<const uint16_t*>(region.data.data());
                    for (uint32_t p = 0; p < region.length && region.offset + p * 4 < dstLength; p++) {
                        uint16_t c = src[p];
                        uint8_t r5 = (c >> 11) & 0x1F;
                        uint8_t g6 = (c >> 5) & 0x3F;
                        uint8_t b5 = c & 0x1F;
                        dst[p * 4 + 0] = (b5 << 3) | (b5 >> 2);  // B
                        dst[p * 4 + 1] = (g6 << 2) | (g6 >> 4);  // G
                        dst[p * 4 + 2] = (r5 << 3) | (r5 >> 2);  // R
                        dst[p * 4 + 3] = 0xFF;                    // A
                    }
                    break;
                }

                case ALGORITHM_DIFF:
                case ALGORITHM_H264:
                default: {
                    // 原始BGRA
                    size_t copyLen = std::min(static_cast<size_t>(region.length),
                                             dstLength - region.offset);
                    memcpy(dst, region.data.data(), copyLen);
                    break;
                }
            }
        }
    }
};

// ============================================
// 测试夹具
// ============================================
class DiffAlgorithmTest : public ::testing::Test {
protected:
    // 创建纯色帧 (BGRA格式)
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

    // 创建渐变帧
    static std::vector<uint8_t> CreateGradientFrame(int width, int height) {
        std::vector<uint8_t> frame(width * height * 4);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = (y * width + x) * 4;
                frame[idx + 0] = static_cast<uint8_t>(x * 255 / width);      // B
                frame[idx + 1] = static_cast<uint8_t>(y * 255 / height);     // G
                frame[idx + 2] = static_cast<uint8_t>((x + y) * 128 / (width + height));  // R
                frame[idx + 3] = 0xFF;
            }
        }
        return frame;
    }

    // 创建带随机区域变化的帧
    static std::vector<uint8_t> CreateFrameWithChanges(
        const std::vector<uint8_t>& baseFrame,
        int width, int height,
        int changeX, int changeY,
        int changeW, int changeH,
        uint8_t newB, uint8_t newG, uint8_t newR
    ) {
        std::vector<uint8_t> frame = baseFrame;
        for (int y = changeY; y < changeY + changeH && y < height; y++) {
            for (int x = changeX; x < changeX + changeW && x < width; x++) {
                int idx = (y * width + x) * 4;
                frame[idx + 0] = newB;
                frame[idx + 1] = newG;
                frame[idx + 2] = newR;
                frame[idx + 3] = 0xFF;
            }
        }
        return frame;
    }
};

// ============================================
// 基础功能测试
// ============================================

TEST_F(DiffAlgorithmTest, IdenticalFrames_NoDifference) {
    auto frame = CreateSolidFrame(100, 100, 128, 128, 128);

    auto regions = DiffAlgorithm::CompareBitmap(
        frame.data(), frame.data(), frame.size(), ALGORITHM_DIFF);

    EXPECT_EQ(regions.size(), 0u);
}

TEST_F(DiffAlgorithmTest, CompletelyDifferent_SingleRegion) {
    auto frame1 = CreateSolidFrame(10, 10, 0, 0, 0);
    auto frame2 = CreateSolidFrame(10, 10, 255, 255, 255);

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_DIFF);

    EXPECT_EQ(regions.size(), 1u);
    EXPECT_EQ(regions[0].offset, 0u);
    EXPECT_EQ(regions[0].length, 100u * 4);  // 100像素 * 4字节
}

TEST_F(DiffAlgorithmTest, PartialChange_SingleRegion) {
    const int WIDTH = 100, HEIGHT = 100;
    auto frame1 = CreateSolidFrame(WIDTH, HEIGHT, 0, 0, 0);
    auto frame2 = CreateFrameWithChanges(frame1, WIDTH, HEIGHT,
                                         10, 10, 20, 20, 255, 255, 255);

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_DIFF);

    // 应该检测到变化区域
    EXPECT_GT(regions.size(), 0u);

    // 验证总变化像素数
    size_t totalChangedPixels = 0;
    for (const auto& r : regions) {
        totalChangedPixels += r.length / 4;  // DIFF算法length是字节数
    }
    EXPECT_EQ(totalChangedPixels, 20u * 20u);  // 20x20区域
}

TEST_F(DiffAlgorithmTest, MultipleRegions_NonContiguous) {
    const int WIDTH = 100, HEIGHT = 10;
    auto frame1 = CreateSolidFrame(WIDTH, HEIGHT, 128, 128, 128);
    auto frame2 = frame1;

    // 创建两个不相邻的变化区域
    // 区域1: 像素 5-14
    for (int i = 5; i < 15; i++) {
        frame2[i * 4 + 0] = 0;
        frame2[i * 4 + 1] = 0;
        frame2[i * 4 + 2] = 255;
    }
    // 区域2: 像素 50-59 (与区域1不相邻)
    for (int i = 50; i < 60; i++) {
        frame2[i * 4 + 0] = 255;
        frame2[i * 4 + 1] = 0;
        frame2[i * 4 + 2] = 0;
    }

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_DIFF);

    EXPECT_EQ(regions.size(), 2u);
}

// ============================================
// 算法特定测试
// ============================================

TEST_F(DiffAlgorithmTest, GrayAlgorithm_CorrectOutput) {
    auto frame1 = CreateSolidFrame(10, 10, 0, 0, 0);        // 黑色
    auto frame2 = CreateSolidFrame(10, 10, 255, 255, 255);  // 白色

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_GRAY);

    ASSERT_EQ(regions.size(), 1u);
    EXPECT_EQ(regions[0].length, 100u);  // 100像素
    EXPECT_EQ(regions[0].data.size(), 100u);  // 1字节/像素

    // 白色应该转换为灰度255
    EXPECT_EQ(regions[0].data[0], 255);
}

TEST_F(DiffAlgorithmTest, GrayAlgorithm_GrayConversionFormula) {
    // 测试灰度转换公式: Y = 0.299*R + 0.587*G + 0.114*B
    std::vector<uint8_t> frame1(4, 0);  // 1像素黑色
    std::vector<uint8_t> frame2(4);

    // 测试纯红色 (R=255, G=0, B=0)
    frame2[0] = 0;    // B
    frame2[1] = 0;    // G
    frame2[2] = 255;  // R
    frame2[3] = 255;  // A

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), 4, ALGORITHM_GRAY);

    ASSERT_EQ(regions.size(), 1u);
    // 期望: (306 * 255 + 601 * 0 + 117 * 0) >> 10 ≈ 76
    uint8_t expectedGray = (306 * 255) >> 10;
    EXPECT_NEAR(regions[0].data[0], expectedGray, 1);
}

TEST_F(DiffAlgorithmTest, RGB565Algorithm_CorrectOutput) {
    auto frame1 = CreateSolidFrame(10, 10, 0, 0, 0);
    auto frame2 = CreateSolidFrame(10, 10, 255, 255, 255);

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_RGB565);

    ASSERT_EQ(regions.size(), 1u);
    EXPECT_EQ(regions[0].length, 100u);  // 100像素
    EXPECT_EQ(regions[0].data.size(), 200u);  // 2字节/像素

    // 白色 RGB565 = 0xFFFF
    uint16_t* rgb565 = reinterpret_cast<uint16_t*>(regions[0].data.data());
    EXPECT_EQ(rgb565[0], 0xFFFF);
}

TEST_F(DiffAlgorithmTest, RGB565Algorithm_ColorConversion) {
    std::vector<uint8_t> frame1(4, 0);  // 1像素黑色
    std::vector<uint8_t> frame2(4);

    // 纯红色 (R=255, G=0, B=0) -> RGB565 = 0xF800
    frame2[0] = 0;    // B
    frame2[1] = 0;    // G
    frame2[2] = 255;  // R
    frame2[3] = 255;  // A

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), 4, ALGORITHM_RGB565);

    ASSERT_EQ(regions.size(), 1u);
    uint16_t* rgb565 = reinterpret_cast<uint16_t*>(regions[0].data.data());
    EXPECT_EQ(rgb565[0], 0xF800);  // 纯红色
}

TEST_F(DiffAlgorithmTest, DiffAlgorithm_PreservesOriginalData) {
    std::vector<uint8_t> frame1(8, 0);  // 2像素黑色
    std::vector<uint8_t> frame2 = {
        0x12, 0x34, 0x56, 0x78,  // 像素1
        0xAB, 0xCD, 0xEF, 0xFF   // 像素2
    };

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), 8, ALGORITHM_DIFF);

    ASSERT_EQ(regions.size(), 1u);
    EXPECT_EQ(regions[0].length, 8u);  // 8字节
    EXPECT_EQ(regions[0].data, frame2);  // 原始数据完整保留
}

// ============================================
// 边界条件测试
// ============================================

TEST_F(DiffAlgorithmTest, EmptyInput_NoRegions) {
    auto regions = DiffAlgorithm::CompareBitmap(nullptr, nullptr, 0, ALGORITHM_DIFF);
    EXPECT_EQ(regions.size(), 0u);
}

TEST_F(DiffAlgorithmTest, SinglePixel_Difference) {
    std::vector<uint8_t> frame1 = {0, 0, 0, 255};
    std::vector<uint8_t> frame2 = {255, 255, 255, 255};

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), 4, ALGORITHM_DIFF);

    ASSERT_EQ(regions.size(), 1u);
    EXPECT_EQ(regions[0].offset, 0u);
    EXPECT_EQ(regions[0].length, 4u);
}

TEST_F(DiffAlgorithmTest, SinglePixel_NoDifference) {
    std::vector<uint8_t> frame = {100, 150, 200, 255};

    auto regions = DiffAlgorithm::CompareBitmap(
        frame.data(), frame.data(), 4, ALGORITHM_DIFF);

    EXPECT_EQ(regions.size(), 0u);
}

TEST_F(DiffAlgorithmTest, NonAlignedLength_Rejected) {
    std::vector<uint8_t> data(7);  // 不是4的倍数
    auto regions = DiffAlgorithm::CompareBitmap(
        data.data(), data.data(), data.size(), ALGORITHM_DIFF);
    EXPECT_EQ(regions.size(), 0u);
}

TEST_F(DiffAlgorithmTest, FirstPixelOnly_Changed) {
    std::vector<uint8_t> frame1(40, 128);  // 10像素
    std::vector<uint8_t> frame2 = frame1;
    frame2[0] = 0;  // 只改变第一个像素的B通道

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), 40, ALGORITHM_DIFF);

    ASSERT_EQ(regions.size(), 1u);
    EXPECT_EQ(regions[0].offset, 0u);
    EXPECT_EQ(regions[0].length, 4u);  // 只有1个像素
}

TEST_F(DiffAlgorithmTest, LastPixelOnly_Changed) {
    std::vector<uint8_t> frame1(40, 128);  // 10像素
    std::vector<uint8_t> frame2 = frame1;
    frame2[36] = 0;  // 只改变最后一个像素的B通道

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), 40, ALGORITHM_DIFF);

    ASSERT_EQ(regions.size(), 1u);
    EXPECT_EQ(regions[0].offset, 36u);  // 第9个像素的偏移
    EXPECT_EQ(regions[0].length, 4u);
}

// ============================================
// 序列化测试
// ============================================

TEST_F(DiffAlgorithmTest, Serialize_SingleRegion) {
    std::vector<DiffRegion> regions;
    DiffRegion r;
    r.offset = 100;
    r.length = 16;
    r.data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    regions.push_back(r);

    std::vector<uint8_t> buffer(1024);
    size_t written = DiffAlgorithm::SerializeDiffRegions(
        regions, buffer.data(), buffer.size());

    EXPECT_EQ(written, 8u + 16u);  // offset(4) + length(4) + data(16)

    // 验证偏移
    uint32_t readOffset;
    memcpy(&readOffset, buffer.data(), 4);
    EXPECT_EQ(readOffset, 100u);

    // 验证长度
    uint32_t readLength;
    memcpy(&readLength, buffer.data() + 4, 4);
    EXPECT_EQ(readLength, 16u);
}

TEST_F(DiffAlgorithmTest, Serialize_MultipleRegions) {
    std::vector<DiffRegion> regions;

    DiffRegion r1;
    r1.offset = 0;
    r1.length = 4;
    r1.data = {1, 2, 3, 4};
    regions.push_back(r1);

    DiffRegion r2;
    r2.offset = 100;
    r2.length = 8;
    r2.data = {5, 6, 7, 8, 9, 10, 11, 12};
    regions.push_back(r2);

    std::vector<uint8_t> buffer(1024);
    size_t written = DiffAlgorithm::SerializeDiffRegions(
        regions, buffer.data(), buffer.size());

    EXPECT_EQ(written, (8u + 4u) + (8u + 8u));  // 两个区域
}

TEST_F(DiffAlgorithmTest, Serialize_BufferTooSmall) {
    std::vector<DiffRegion> regions;
    DiffRegion r;
    r.offset = 0;
    r.length = 100;
    r.data.resize(100, 0xFF);
    regions.push_back(r);

    std::vector<uint8_t> buffer(10);  // 太小
    size_t written = DiffAlgorithm::SerializeDiffRegions(
        regions, buffer.data(), buffer.size());

    EXPECT_EQ(written, 0u);  // 无法写入
}

// ============================================
// 应用差异测试
// ============================================

TEST_F(DiffAlgorithmTest, ApplyDiff_DIFF_Reconstructs) {
    auto frame1 = CreateGradientFrame(50, 50);
    auto frame2 = CreateFrameWithChanges(frame1, 50, 50, 10, 10, 20, 20, 255, 0, 0);

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_DIFF);

    // 应用差异到frame1的副本
    std::vector<uint8_t> reconstructed = frame1;
    DiffAlgorithm::ApplyDiff(reconstructed.data(), reconstructed.size(),
                             regions, ALGORITHM_DIFF);

    // 应该完全重建frame2
    EXPECT_EQ(reconstructed, frame2);
}

TEST_F(DiffAlgorithmTest, ApplyDiff_GRAY_ApproximateReconstruction) {
    auto frame1 = CreateSolidFrame(10, 10, 0, 0, 0);
    auto frame2 = CreateSolidFrame(10, 10, 200, 200, 200);  // 浅灰色

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_GRAY);

    std::vector<uint8_t> reconstructed = frame1;
    DiffAlgorithm::ApplyDiff(reconstructed.data(), reconstructed.size(),
                             regions, ALGORITHM_GRAY);

    // 灰度重建，R=G=B
    for (size_t i = 0; i < reconstructed.size(); i += 4) {
        EXPECT_EQ(reconstructed[i], reconstructed[i + 1]);  // B == G
        EXPECT_EQ(reconstructed[i + 1], reconstructed[i + 2]);  // G == R
    }
}

TEST_F(DiffAlgorithmTest, ApplyDiff_RGB565_ApproximateReconstruction) {
    auto frame1 = CreateSolidFrame(10, 10, 0, 0, 0);
    // RGB565有量化误差，使用能精确表示的颜色
    auto frame2 = CreateSolidFrame(10, 10, 248, 252, 248);  // 接近白色

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_RGB565);

    std::vector<uint8_t> reconstructed = frame1;
    DiffAlgorithm::ApplyDiff(reconstructed.data(), reconstructed.size(),
                             regions, ALGORITHM_RGB565);

    // 验证重建颜色接近原始（允许量化误差）
    for (size_t i = 0; i < reconstructed.size(); i += 4) {
        EXPECT_NEAR(reconstructed[i], frame2[i], 8);      // B
        EXPECT_NEAR(reconstructed[i + 1], frame2[i + 1], 4);  // G (6位精度)
        EXPECT_NEAR(reconstructed[i + 2], frame2[i + 2], 8);  // R
    }
}

// ============================================
// 性能相关测试（不计时，只验证正确性）
// ============================================

TEST_F(DiffAlgorithmTest, LargeFrame_1080p_Correctness) {
    const int WIDTH = 1920, HEIGHT = 1080;
    auto frame1 = CreateSolidFrame(WIDTH, HEIGHT, 128, 128, 128);
    auto frame2 = CreateFrameWithChanges(frame1, WIDTH, HEIGHT,
                                         100, 100, 200, 200, 255, 0, 0);

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_DIFF);

    // 应该有变化区域
    EXPECT_GT(regions.size(), 0u);

    // 重建验证
    std::vector<uint8_t> reconstructed = frame1;
    DiffAlgorithm::ApplyDiff(reconstructed.data(), reconstructed.size(),
                             regions, ALGORITHM_DIFF);
    EXPECT_EQ(reconstructed, frame2);
}

TEST_F(DiffAlgorithmTest, RandomChanges_AllAlgorithms) {
    const int WIDTH = 100, HEIGHT = 100;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);

    auto frame1 = CreateGradientFrame(WIDTH, HEIGHT);
    auto frame2 = frame1;

    // 随机修改10%的像素
    for (int i = 0; i < WIDTH * HEIGHT / 10; i++) {
        int idx = (rng() % (WIDTH * HEIGHT)) * 4;
        frame2[idx + 0] = dist(rng);
        frame2[idx + 1] = dist(rng);
        frame2[idx + 2] = dist(rng);
    }

    // 测试所有算法都能产生输出
    for (int algo : {ALGORITHM_GRAY, ALGORITHM_DIFF, ALGORITHM_RGB565}) {
        auto regions = DiffAlgorithm::CompareBitmap(
            frame2.data(), frame1.data(), frame1.size(), algo);
        EXPECT_GT(regions.size(), 0u) << "Algorithm " << algo << " failed";
    }
}

// ============================================
// 压缩效率测试
// ============================================

TEST_F(DiffAlgorithmTest, CompressionRatio_GRAY) {
    auto frame1 = CreateSolidFrame(100, 100, 0, 0, 0);
    auto frame2 = CreateSolidFrame(100, 100, 255, 255, 255);

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_GRAY);

    size_t originalSize = 100 * 100 * 4;  // 40000 字节
    size_t compressedSize = 8 + regions[0].data.size();  // offset + length + data

    // GRAY应该是 100*100*1 = 10000 字节数据
    EXPECT_EQ(regions[0].data.size(), 10000u);
    // 压缩比约 4:1
    EXPECT_LT(compressedSize, originalSize / 3);
}

TEST_F(DiffAlgorithmTest, CompressionRatio_RGB565) {
    auto frame1 = CreateSolidFrame(100, 100, 0, 0, 0);
    auto frame2 = CreateSolidFrame(100, 100, 255, 255, 255);

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_RGB565);

    // RGB565应该是 100*100*2 = 20000 字节数据
    EXPECT_EQ(regions[0].data.size(), 20000u);
}

TEST_F(DiffAlgorithmTest, NoChange_ZeroOverhead) {
    auto frame = CreateGradientFrame(100, 100);

    auto regions = DiffAlgorithm::CompareBitmap(
        frame.data(), frame.data(), frame.size(), ALGORITHM_DIFF);

    EXPECT_EQ(regions.size(), 0u);

    std::vector<uint8_t> buffer(1024);
    size_t written = DiffAlgorithm::SerializeDiffRegions(
        regions, buffer.data(), buffer.size());

    EXPECT_EQ(written, 0u);  // 无变化时零开销
}

// ============================================
// 参数化测试：不同分辨率
// ============================================
class DiffAlgorithmResolutionTest : public ::testing::TestWithParam<std::tuple<int, int>> {};

TEST_P(DiffAlgorithmResolutionTest, Resolution_Correctness) {
    auto [width, height] = GetParam();

    auto frame1 = std::vector<uint8_t>(width * height * 4, 0);
    auto frame2 = std::vector<uint8_t>(width * height * 4, 255);

    auto regions = DiffAlgorithm::CompareBitmap(
        frame2.data(), frame1.data(), frame1.size(), ALGORITHM_DIFF);

    EXPECT_EQ(regions.size(), 1u);
    EXPECT_EQ(regions[0].offset, 0u);
    EXPECT_EQ(regions[0].length, static_cast<uint32_t>(width * height * 4));
}

INSTANTIATE_TEST_SUITE_P(
    Resolutions,
    DiffAlgorithmResolutionTest,
    ::testing::Values(
        std::make_tuple(1, 1),      // 最小
        std::make_tuple(10, 10),    // 小
        std::make_tuple(100, 100),  // 中
        std::make_tuple(640, 480),  // VGA
        std::make_tuple(1280, 720), // 720p
        std::make_tuple(1920, 1080) // 1080p
    )
);
