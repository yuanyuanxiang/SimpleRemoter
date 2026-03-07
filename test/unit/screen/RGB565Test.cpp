// RGB565Test.cpp - Phase 4: RGB565压缩单元测试
// 测试 BGRA <-> RGB565 颜色空间转换

#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <cmath>

// ============================================
// RGB565 颜色空间转换实现
// 来源: client/ScreenCapture.h, server/ScreenSpyDlg.cpp
// ============================================

// RGB565 格式说明:
// 16位: RRRRRGGG GGGBBBBB
// R: 5位 (0-31)
// G: 6位 (0-63)
// B: 5位 (0-31)

class RGB565Converter {
public:
    // ============================================
    // BGRA -> RGB565 转换 (标量版本)
    // ============================================
    static void ConvertBGRAtoRGB565_Scalar(
        const uint8_t* src,
        uint16_t* dst,
        size_t pixelCount
    ) {
        for (size_t i = 0; i < pixelCount; i++) {
            // BGRA 格式: B=0, G=1, R=2, A=3
            uint8_t b = src[i * 4 + 0];
            uint8_t g = src[i * 4 + 1];
            uint8_t r = src[i * 4 + 2];
            // A 通道被忽略

            // RGB565: RRRRRGGG GGGBBBBB
            dst[i] = static_cast<uint16_t>(
                ((r >> 3) << 11) |  // R: 高5位 -> 位11-15
                ((g >> 2) << 5) |   // G: 高6位 -> 位5-10
                (b >> 3)            // B: 高5位 -> 位0-4
            );
        }
    }

    // ============================================
    // RGB565 -> BGRA 转换
    // 位复制填充低位以提高精度
    // ============================================
    static void ConvertRGB565ToBGRA(
        const uint16_t* src,
        uint8_t* dst,
        size_t pixelCount
    ) {
        for (size_t i = 0; i < pixelCount; i++) {
            uint16_t c = src[i];

            // 提取各通道
            uint8_t r5 = (c >> 11) & 0x1F;  // 5位红色
            uint8_t g6 = (c >> 5) & 0x3F;   // 6位绿色
            uint8_t b5 = c & 0x1F;          // 5位蓝色

            // 扩展到8位（使用位复制填充低位）
            // 例如: 5位 11111 -> 8位 11111111 (通过 (x << 3) | (x >> 2))
            dst[i * 4 + 0] = (b5 << 3) | (b5 >> 2);  // B: 5->8位
            dst[i * 4 + 1] = (g6 << 2) | (g6 >> 4);  // G: 6->8位
            dst[i * 4 + 2] = (r5 << 3) | (r5 >> 2);  // R: 5->8位
            dst[i * 4 + 3] = 0xFF;                    // A: 不透明
        }
    }

    // ============================================
    // 单像素转换辅助函数
    // ============================================
    static uint16_t BGRAToRGB565(uint8_t b, uint8_t g, uint8_t r) {
        return static_cast<uint16_t>(
            ((r >> 3) << 11) |
            ((g >> 2) << 5) |
            (b >> 3)
        );
    }

    static void RGB565ToBGRA(uint16_t c, uint8_t& b, uint8_t& g, uint8_t& r, uint8_t& a) {
        uint8_t r5 = (c >> 11) & 0x1F;
        uint8_t g6 = (c >> 5) & 0x3F;
        uint8_t b5 = c & 0x1F;

        b = (b5 << 3) | (b5 >> 2);
        g = (g6 << 2) | (g6 >> 4);
        r = (r5 << 3) | (r5 >> 2);
        a = 0xFF;
    }

    // ============================================
    // 计算量化误差
    // ============================================
    static int CalculateError(uint8_t original, uint8_t converted) {
        return std::abs(static_cast<int>(original) - static_cast<int>(converted));
    }

    // 计算最大理论误差
    // 5位通道: 量化+位填充可能产生最大误差 7
    // 6位通道: 量化+位填充可能产生最大误差 3
    static int MaxError5Bit() { return 7; }
    static int MaxError6Bit() { return 3; }
};

// ============================================
// 测试夹具
// ============================================
class RGB565Test : public ::testing::Test {
protected:
    // 创建BGRA像素数组
    static std::vector<uint8_t> CreateBGRAPixels(size_t count, uint8_t b, uint8_t g, uint8_t r, uint8_t a = 0xFF) {
        std::vector<uint8_t> pixels(count * 4);
        for (size_t i = 0; i < count; i++) {
            pixels[i * 4 + 0] = b;
            pixels[i * 4 + 1] = g;
            pixels[i * 4 + 2] = r;
            pixels[i * 4 + 3] = a;
        }
        return pixels;
    }
};

// ============================================
// 基础转换测试
// ============================================

TEST_F(RGB565Test, SinglePixel_Black) {
    uint16_t result = RGB565Converter::BGRAToRGB565(0, 0, 0);
    EXPECT_EQ(result, 0x0000);
}

TEST_F(RGB565Test, SinglePixel_White) {
    uint16_t result = RGB565Converter::BGRAToRGB565(255, 255, 255);
    // R=31<<11=0xF800, G=63<<5=0x07E0, B=31=0x001F
    // 合计: 0xFFFF
    EXPECT_EQ(result, 0xFFFF);
}

TEST_F(RGB565Test, SinglePixel_Red) {
    uint16_t result = RGB565Converter::BGRAToRGB565(0, 0, 255);
    // R=31<<11=0xF800, G=0, B=0
    EXPECT_EQ(result, 0xF800);
}

TEST_F(RGB565Test, SinglePixel_Green) {
    uint16_t result = RGB565Converter::BGRAToRGB565(0, 255, 0);
    // R=0, G=63<<5=0x07E0, B=0
    EXPECT_EQ(result, 0x07E0);
}

TEST_F(RGB565Test, SinglePixel_Blue) {
    uint16_t result = RGB565Converter::BGRAToRGB565(255, 0, 0);
    // R=0, G=0, B=31=0x001F
    EXPECT_EQ(result, 0x001F);
}

// ============================================
// 反向转换测试
// ============================================

TEST_F(RGB565Test, Reverse_Black) {
    uint8_t b, g, r, a;
    RGB565Converter::RGB565ToBGRA(0x0000, b, g, r, a);
    EXPECT_EQ(b, 0);
    EXPECT_EQ(g, 0);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(a, 255);
}

TEST_F(RGB565Test, Reverse_White) {
    uint8_t b, g, r, a;
    RGB565Converter::RGB565ToBGRA(0xFFFF, b, g, r, a);
    EXPECT_EQ(b, 255);
    EXPECT_EQ(g, 255);
    EXPECT_EQ(r, 255);
    EXPECT_EQ(a, 255);
}

TEST_F(RGB565Test, Reverse_Red) {
    uint8_t b, g, r, a;
    RGB565Converter::RGB565ToBGRA(0xF800, b, g, r, a);
    EXPECT_EQ(b, 0);
    EXPECT_EQ(g, 0);
    EXPECT_EQ(r, 255);
    EXPECT_EQ(a, 255);
}

TEST_F(RGB565Test, Reverse_Green) {
    uint8_t b, g, r, a;
    RGB565Converter::RGB565ToBGRA(0x07E0, b, g, r, a);
    EXPECT_EQ(b, 0);
    EXPECT_EQ(g, 255);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(a, 255);
}

TEST_F(RGB565Test, Reverse_Blue) {
    uint8_t b, g, r, a;
    RGB565Converter::RGB565ToBGRA(0x001F, b, g, r, a);
    EXPECT_EQ(b, 255);
    EXPECT_EQ(g, 0);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(a, 255);
}

// ============================================
// 往返转换测试
// ============================================

TEST_F(RGB565Test, RoundTrip_ExactColors) {
    // 测试能精确表示的颜色 (只有 0 和 255 能完美往返)
    // 位填充公式: (x << 3) | (x >> 2) 只有 x=0 -> 0, x=31 -> 255 是精确的
    struct TestCase {
        uint8_t b, g, r;
    } testCases[] = {
        {0, 0, 0},       // 黑色 - 精确
        {255, 255, 255}, // 白色 - 精确
        {0, 0, 255},     // 红色 - 精确
        {0, 255, 0},     // 绿色 - 精确
        {255, 0, 0},     // 蓝色 - 精确
        {255, 255, 0},   // 青色 - 精确
        {0, 255, 255},   // 黄色 - 精确
        {255, 0, 255},   // 品红 - 精确
    };

    for (const auto& tc : testCases) {
        uint16_t rgb565 = RGB565Converter::BGRAToRGB565(tc.b, tc.g, tc.r);
        uint8_t b, g, r, a;
        RGB565Converter::RGB565ToBGRA(rgb565, b, g, r, a);

        // 能精确表示的颜色应该完美还原
        EXPECT_EQ(b, tc.b) << "B channel mismatch for (" << (int)tc.r << "," << (int)tc.g << "," << (int)tc.b << ")";
        EXPECT_EQ(g, tc.g) << "G channel mismatch for (" << (int)tc.r << "," << (int)tc.g << "," << (int)tc.b << ")";
        EXPECT_EQ(r, tc.r) << "R channel mismatch for (" << (int)tc.r << "," << (int)tc.g << "," << (int)tc.b << ")";
    }
}

TEST_F(RGB565Test, RoundTrip_QuantizationError) {
    // 测试所有颜色的量化误差在允许范围内
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, 255);

    for (int i = 0; i < 1000; i++) {
        uint8_t origB = dist(rng);
        uint8_t origG = dist(rng);
        uint8_t origR = dist(rng);

        uint16_t rgb565 = RGB565Converter::BGRAToRGB565(origB, origG, origR);
        uint8_t b, g, r, a;
        RGB565Converter::RGB565ToBGRA(rgb565, b, g, r, a);

        // 验证误差在允许范围内
        EXPECT_LE(RGB565Converter::CalculateError(origB, b), RGB565Converter::MaxError5Bit());
        EXPECT_LE(RGB565Converter::CalculateError(origG, g), RGB565Converter::MaxError6Bit());
        EXPECT_LE(RGB565Converter::CalculateError(origR, r), RGB565Converter::MaxError5Bit());
    }
}

// ============================================
// 批量转换测试
// ============================================

TEST_F(RGB565Test, BatchConvert_SinglePixel) {
    auto bgra = CreateBGRAPixels(1, 128, 64, 192);
    std::vector<uint16_t> rgb565(1);

    RGB565Converter::ConvertBGRAtoRGB565_Scalar(bgra.data(), rgb565.data(), 1);

    // 验证转换结果
    uint16_t expected = RGB565Converter::BGRAToRGB565(128, 64, 192);
    EXPECT_EQ(rgb565[0], expected);
}

TEST_F(RGB565Test, BatchConvert_MultiplePixels) {
    const size_t COUNT = 100;
    auto bgra = CreateBGRAPixels(COUNT, 100, 150, 200);
    std::vector<uint16_t> rgb565(COUNT);

    RGB565Converter::ConvertBGRAtoRGB565_Scalar(bgra.data(), rgb565.data(), COUNT);

    uint16_t expected = RGB565Converter::BGRAToRGB565(100, 150, 200);
    for (size_t i = 0; i < COUNT; i++) {
        EXPECT_EQ(rgb565[i], expected);
    }
}

TEST_F(RGB565Test, BatchReverse_MultiplePixels) {
    const size_t COUNT = 100;
    std::vector<uint16_t> rgb565(COUNT, 0xF800);  // 红色
    std::vector<uint8_t> bgra(COUNT * 4);

    RGB565Converter::ConvertRGB565ToBGRA(rgb565.data(), bgra.data(), COUNT);

    for (size_t i = 0; i < COUNT; i++) {
        EXPECT_EQ(bgra[i * 4 + 0], 0);    // B
        EXPECT_EQ(bgra[i * 4 + 1], 0);    // G
        EXPECT_EQ(bgra[i * 4 + 2], 255);  // R
        EXPECT_EQ(bgra[i * 4 + 3], 255);  // A
    }
}

TEST_F(RGB565Test, BatchRoundTrip) {
    const size_t COUNT = 1000;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);

    // 创建随机BGRA数据
    std::vector<uint8_t> original(COUNT * 4);
    for (size_t i = 0; i < COUNT * 4; i++) {
        original[i] = (i % 4 == 3) ? 255 : dist(rng);
    }

    // 转换到 RGB565
    std::vector<uint16_t> rgb565(COUNT);
    RGB565Converter::ConvertBGRAtoRGB565_Scalar(original.data(), rgb565.data(), COUNT);

    // 转换回 BGRA
    std::vector<uint8_t> reconstructed(COUNT * 4);
    RGB565Converter::ConvertRGB565ToBGRA(rgb565.data(), reconstructed.data(), COUNT);

    // 验证所有像素误差在允许范围内
    for (size_t i = 0; i < COUNT; i++) {
        EXPECT_LE(RGB565Converter::CalculateError(original[i * 4 + 0], reconstructed[i * 4 + 0]),
                  RGB565Converter::MaxError5Bit()) << "Pixel " << i << " B";
        EXPECT_LE(RGB565Converter::CalculateError(original[i * 4 + 1], reconstructed[i * 4 + 1]),
                  RGB565Converter::MaxError6Bit()) << "Pixel " << i << " G";
        EXPECT_LE(RGB565Converter::CalculateError(original[i * 4 + 2], reconstructed[i * 4 + 2]),
                  RGB565Converter::MaxError5Bit()) << "Pixel " << i << " R";
        EXPECT_EQ(reconstructed[i * 4 + 3], 255) << "Pixel " << i << " A";
    }
}

// ============================================
// 边界值测试
// ============================================

TEST_F(RGB565Test, Boundary_AllZeros) {
    uint16_t result = RGB565Converter::BGRAToRGB565(0, 0, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(RGB565Test, Boundary_AllOnes) {
    uint16_t result = RGB565Converter::BGRAToRGB565(255, 255, 255);
    EXPECT_EQ(result, 0xFFFF);
}

TEST_F(RGB565Test, Boundary_ChannelMax) {
    // 单通道最大值
    EXPECT_EQ(RGB565Converter::BGRAToRGB565(255, 0, 0), 0x001F);    // B
    EXPECT_EQ(RGB565Converter::BGRAToRGB565(0, 255, 0), 0x07E0);    // G
    EXPECT_EQ(RGB565Converter::BGRAToRGB565(0, 0, 255), 0xF800);    // R
}

TEST_F(RGB565Test, Boundary_ChannelMin) {
    // 单通道最小值（其他为最大）
    EXPECT_EQ(RGB565Converter::BGRAToRGB565(0, 255, 255), 0xFFE0);    // 无B
    EXPECT_EQ(RGB565Converter::BGRAToRGB565(255, 0, 255), 0xF81F);    // 无G
    EXPECT_EQ(RGB565Converter::BGRAToRGB565(255, 255, 0), 0x07FF);    // 无R
}

// ============================================
// 位填充测试
// ============================================

TEST_F(RGB565Test, BitFilling_5BitExpansion) {
    // 测试5位扩展到8位的位填充策略
    // 5位值 11111 (31) -> 8位值 11111111 (255)
    // 公式: (x << 3) | (x >> 2)

    // 最大值: 31 -> 255
    uint8_t expanded = (31 << 3) | (31 >> 2);
    EXPECT_EQ(expanded, 255);

    // 最小值: 0 -> 0
    expanded = (0 << 3) | (0 >> 2);
    EXPECT_EQ(expanded, 0);

    // 中间值: 16 -> 132 (10000 -> 10000100)
    expanded = (16 << 3) | (16 >> 2);
    EXPECT_EQ(expanded, 132);
}

TEST_F(RGB565Test, BitFilling_6BitExpansion) {
    // 测试6位扩展到8位的位填充策略
    // 6位值 111111 (63) -> 8位值 11111111 (255)
    // 公式: (x << 2) | (x >> 4)

    // 最大值: 63 -> 255
    uint8_t expanded = (63 << 2) | (63 >> 4);
    EXPECT_EQ(expanded, 255);

    // 最小值: 0 -> 0
    expanded = (0 << 2) | (0 >> 4);
    EXPECT_EQ(expanded, 0);

    // 中间值: 32 -> 130 (100000 -> 10000010)
    expanded = (32 << 2) | (32 >> 4);
    EXPECT_EQ(expanded, 130);
}

// ============================================
// Alpha通道测试
// ============================================

TEST_F(RGB565Test, Alpha_Ignored) {
    // 不同alpha值应该产生相同的RGB565
    auto bgra1 = CreateBGRAPixels(1, 100, 100, 100, 255);
    auto bgra2 = CreateBGRAPixels(1, 100, 100, 100, 128);
    auto bgra3 = CreateBGRAPixels(1, 100, 100, 100, 0);

    std::vector<uint16_t> rgb565_1(1), rgb565_2(1), rgb565_3(1);

    RGB565Converter::ConvertBGRAtoRGB565_Scalar(bgra1.data(), rgb565_1.data(), 1);
    RGB565Converter::ConvertBGRAtoRGB565_Scalar(bgra2.data(), rgb565_2.data(), 1);
    RGB565Converter::ConvertBGRAtoRGB565_Scalar(bgra3.data(), rgb565_3.data(), 1);

    EXPECT_EQ(rgb565_1[0], rgb565_2[0]);
    EXPECT_EQ(rgb565_2[0], rgb565_3[0]);
}

TEST_F(RGB565Test, Alpha_RestoredToOpaque) {
    // 反向转换时Alpha应该恢复为255
    std::vector<uint16_t> rgb565 = {0x1234, 0x5678, 0x9ABC};
    std::vector<uint8_t> bgra(3 * 4);

    RGB565Converter::ConvertRGB565ToBGRA(rgb565.data(), bgra.data(), 3);

    for (size_t i = 0; i < 3; i++) {
        EXPECT_EQ(bgra[i * 4 + 3], 255);
    }
}

// ============================================
// 压缩率测试
// ============================================

TEST_F(RGB565Test, CompressionRatio) {
    // BGRA: 4字节/像素
    // RGB565: 2字节/像素
    // 压缩率: 50%

    const size_t PIXEL_COUNT = 1920 * 1080;
    size_t bgraSize = PIXEL_COUNT * 4;
    size_t rgb565Size = PIXEL_COUNT * 2;

    EXPECT_EQ(rgb565Size, bgraSize / 2);
    EXPECT_DOUBLE_EQ(static_cast<double>(rgb565Size) / bgraSize, 0.5);
}

// ============================================
// 特殊颜色测试
// ============================================

TEST_F(RGB565Test, CommonColors) {
    struct ColorTest {
        const char* name;
        uint8_t r, g, b;
        uint16_t expectedRGB565;
    } colors[] = {
        {"Black",   0,   0,   0,   0x0000},
        {"White",   255, 255, 255, 0xFFFF},
        {"Red",     255, 0,   0,   0xF800},
        {"Green",   0,   255, 0,   0x07E0},
        {"Blue",    0,   0,   255, 0x001F},
        {"Yellow",  255, 255, 0,   0xFFE0},
        {"Cyan",    0,   255, 255, 0x07FF},
        {"Magenta", 255, 0,   255, 0xF81F},
    };

    for (const auto& c : colors) {
        uint16_t result = RGB565Converter::BGRAToRGB565(c.b, c.g, c.r);
        EXPECT_EQ(result, c.expectedRGB565) << "Color: " << c.name;
    }
}

TEST_F(RGB565Test, GrayScales) {
    // 测试灰度值转换
    for (int gray = 0; gray <= 255; gray += 17) {
        uint8_t g = static_cast<uint8_t>(gray);
        uint16_t rgb565 = RGB565Converter::BGRAToRGB565(g, g, g);

        uint8_t b, gr, r, a;
        RGB565Converter::RGB565ToBGRA(rgb565, b, gr, r, a);

        // 灰度值在量化误差范围内应该保持一致
        EXPECT_NEAR(b, g, 8);
        EXPECT_NEAR(gr, g, 4);
        EXPECT_NEAR(r, g, 8);
    }
}

// ============================================
// 参数化测试
// ============================================

class RGB565ChannelTest : public ::testing::TestWithParam<int> {};

TEST_P(RGB565ChannelTest, ChannelValueRange) {
    int value = GetParam();
    uint8_t v = static_cast<uint8_t>(value);

    // 测试R通道
    {
        uint16_t rgb565 = RGB565Converter::BGRAToRGB565(0, 0, v);
        uint8_t b, g, r, a;
        RGB565Converter::RGB565ToBGRA(rgb565, b, g, r, a);
        EXPECT_LE(RGB565Converter::CalculateError(v, r), RGB565Converter::MaxError5Bit());
    }

    // 测试G通道
    {
        uint16_t rgb565 = RGB565Converter::BGRAToRGB565(0, v, 0);
        uint8_t b, g, r, a;
        RGB565Converter::RGB565ToBGRA(rgb565, b, g, r, a);
        EXPECT_LE(RGB565Converter::CalculateError(v, g), RGB565Converter::MaxError6Bit());
    }

    // 测试B通道
    {
        uint16_t rgb565 = RGB565Converter::BGRAToRGB565(v, 0, 0);
        uint8_t b, g, r, a;
        RGB565Converter::RGB565ToBGRA(rgb565, b, g, r, a);
        EXPECT_LE(RGB565Converter::CalculateError(v, b), RGB565Converter::MaxError5Bit());
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllValues,
    RGB565ChannelTest,
    ::testing::Range(0, 256, 1)  // 测试0-255所有值
);

// ============================================
// 大数据量测试
// ============================================

TEST_F(RGB565Test, LargeFrame_1080p) {
    const size_t WIDTH = 1920, HEIGHT = 1080;
    const size_t COUNT = WIDTH * HEIGHT;

    // 创建渐变图像
    std::vector<uint8_t> bgra(COUNT * 4);
    for (size_t y = 0; y < HEIGHT; y++) {
        for (size_t x = 0; x < WIDTH; x++) {
            size_t idx = (y * WIDTH + x) * 4;
            bgra[idx + 0] = static_cast<uint8_t>(x * 255 / WIDTH);
            bgra[idx + 1] = static_cast<uint8_t>(y * 255 / HEIGHT);
            bgra[idx + 2] = static_cast<uint8_t>((x + y) * 127 / (WIDTH + HEIGHT));
            bgra[idx + 3] = 255;
        }
    }

    // 转换
    std::vector<uint16_t> rgb565(COUNT);
    RGB565Converter::ConvertBGRAtoRGB565_Scalar(bgra.data(), rgb565.data(), COUNT);

    // 验证大小
    EXPECT_EQ(rgb565.size() * sizeof(uint16_t), COUNT * 2);

    // 抽样验证
    for (size_t i = 0; i < COUNT; i += COUNT / 100) {
        uint16_t expected = RGB565Converter::BGRAToRGB565(bgra[i * 4 + 0], bgra[i * 4 + 1], bgra[i * 4 + 2]);
        EXPECT_EQ(rgb565[i], expected) << "Mismatch at pixel " << i;
    }
}

// ============================================
// 端序测试
// ============================================

TEST_F(RGB565Test, Endianness_LittleEndian) {
    // RGB565 应该以小端存储
    uint16_t rgb565 = 0x1234;
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&rgb565);

    // 在小端系统上: bytes[0] = 0x34, bytes[1] = 0x12
    EXPECT_EQ(bytes[0], 0x34);
    EXPECT_EQ(bytes[1], 0x12);
}

// ============================================
// 错误处理测试
// ============================================

TEST_F(RGB565Test, ZeroPixelCount) {
    std::vector<uint8_t> bgra(4);
    std::vector<uint16_t> rgb565(1);

    // 零像素不应崩溃
    RGB565Converter::ConvertBGRAtoRGB565_Scalar(bgra.data(), rgb565.data(), 0);
    RGB565Converter::ConvertRGB565ToBGRA(rgb565.data(), bgra.data(), 0);
}
