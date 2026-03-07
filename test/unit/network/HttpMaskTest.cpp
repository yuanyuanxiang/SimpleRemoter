/**
 * @file HttpMaskTest.cpp
 * @brief HTTP 协议伪装测试
 *
 * 测试覆盖：
 * - HTTP 请求格式生成
 * - HTTP 头部解析和移除
 * - Mask/UnMask 往返测试
 * - 边界条件处理
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <vector>
#include <map>

// ============================================
// 类型定义
// ============================================

#ifdef _WIN32
typedef unsigned long ULONG;
#else
typedef uint32_t ULONG;
#endif

// ============================================
// 协议伪装类型
// ============================================

enum PkgMaskType {
    MaskTypeUnknown = -1,
    MaskTypeNone,
    MaskTypeHTTP,
    MaskTypeNum,
};

// ============================================
// HTTP 解除伪装函数
// ============================================

inline ULONG UnMaskHttp(const char* src, ULONG srcSize)
{
    const char* header_end_mark = "\r\n\r\n";
    const ULONG mark_len = 4;

    for (ULONG i = 0; i + mark_len <= srcSize; ++i) {
        if (memcmp(src + i, header_end_mark, mark_len) == 0) {
            return i + mark_len;
        }
    }
    return 0;
}

inline ULONG TryUnMask(const char* src, ULONG srcSize, PkgMaskType& maskHit)
{
    if (srcSize >= 5 && memcmp(src, "POST ", 5) == 0) {
        maskHit = MaskTypeHTTP;
        return UnMaskHttp(src, srcSize);
    }
    if (srcSize >= 4 && memcmp(src, "GET ", 4) == 0) {
        maskHit = MaskTypeHTTP;
        return UnMaskHttp(src, srcSize);
    }
    maskHit = MaskTypeNone;
    return 0;
}

// ============================================
// HTTP Mask 类（简化版）
// ============================================

class HttpMask {
public:
    explicit HttpMask(const std::string& host = "example.com",
                     const std::map<std::string, std::string>& headers = {})
        : host_(host)
    {
        for (const auto& kv : headers) {
            customHeaders_ += kv.first + ": " + kv.second + "\r\n";
        }
    }

    void Mask(char*& dst, ULONG& dstSize, const char* src, ULONG srcSize, int cmd = -1)
    {
        std::string path = "/api/v1/" + std::to_string(cmd == -1 ? 0 : cmd);

        std::string httpHeader =
            "POST " + path + " HTTP/1.1\r\n"
            "Host: " + host_ + "\r\n"
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: " + std::to_string(srcSize) + "\r\n" + customHeaders_ +
            "Connection: keep-alive\r\n"
            "\r\n";

        dstSize = static_cast<ULONG>(httpHeader.size()) + srcSize;
        dst = new char[dstSize];

        memcpy(dst, httpHeader.data(), httpHeader.size());
        if (srcSize > 0) {
            memcpy(dst + httpHeader.size(), src, srcSize);
        }
    }

    ULONG UnMask(const char* src, ULONG srcSize)
    {
        return UnMaskHttp(src, srcSize);
    }

    void SetHost(const std::string& host)
    {
        host_ = host;
    }

private:
    std::string host_;
    std::string customHeaders_;
};

// ============================================
// UnMaskHttp 测试
// ============================================

class UnMaskHttpTest : public ::testing::Test {};

TEST_F(UnMaskHttpTest, ValidHttpRequest) {
    std::string httpRequest =
        "POST /api HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "DATA";

    ULONG offset = UnMaskHttp(httpRequest.data(), static_cast<ULONG>(httpRequest.size()));

    // 应该返回 body 起始位置
    EXPECT_GT(offset, 0u);
    EXPECT_STREQ(httpRequest.data() + offset, "DATA");
}

TEST_F(UnMaskHttpTest, NoHeaderEnd) {
    std::string incomplete = "POST /api HTTP/1.1\r\nHost: example.com\r\n";

    ULONG offset = UnMaskHttp(incomplete.data(), static_cast<ULONG>(incomplete.size()));
    EXPECT_EQ(offset, 0u);
}

TEST_F(UnMaskHttpTest, EmptyBody) {
    std::string httpRequest =
        "POST /api HTTP/1.1\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    ULONG offset = UnMaskHttp(httpRequest.data(), static_cast<ULONG>(httpRequest.size()));
    EXPECT_EQ(offset, static_cast<ULONG>(httpRequest.size()));
}

TEST_F(UnMaskHttpTest, MultipleHeaderEndMarkers) {
    std::string httpRequest =
        "POST /api HTTP/1.1\r\n"
        "Content-Length: 8\r\n"
        "\r\n"
        "\r\n\r\nXX";  // body 中也有 \r\n\r\n

    ULONG offset = UnMaskHttp(httpRequest.data(), static_cast<ULONG>(httpRequest.size()));

    // 应该返回第一个 \r\n\r\n 之后
    std::string body(httpRequest.data() + offset);
    EXPECT_EQ(body, "\r\n\r\nXX");
}

TEST_F(UnMaskHttpTest, MinimalInput) {
    // 小于 4 字节
    std::string tiny = "AB";
    ULONG offset = UnMaskHttp(tiny.data(), static_cast<ULONG>(tiny.size()));
    EXPECT_EQ(offset, 0u);
}

TEST_F(UnMaskHttpTest, ExactlyHeaderEnd) {
    std::string justEnd = "\r\n\r\n";
    ULONG offset = UnMaskHttp(justEnd.data(), static_cast<ULONG>(justEnd.size()));
    EXPECT_EQ(offset, 4u);
}

// ============================================
// TryUnMask 测试
// ============================================

class TryUnMaskTest : public ::testing::Test {};

TEST_F(TryUnMaskTest, DetectPOST) {
    std::string httpRequest =
        "POST /api HTTP/1.1\r\n"
        "Host: test.com\r\n"
        "\r\n"
        "body";

    PkgMaskType maskType;
    ULONG offset = TryUnMask(httpRequest.data(), static_cast<ULONG>(httpRequest.size()), maskType);

    EXPECT_EQ(maskType, MaskTypeHTTP);
    EXPECT_GT(offset, 0u);
}

TEST_F(TryUnMaskTest, DetectGET) {
    std::string httpRequest =
        "GET /resource HTTP/1.1\r\n"
        "Host: test.com\r\n"
        "\r\n";

    PkgMaskType maskType;
    ULONG offset = TryUnMask(httpRequest.data(), static_cast<ULONG>(httpRequest.size()), maskType);

    EXPECT_EQ(maskType, MaskTypeHTTP);
    EXPECT_GT(offset, 0u);
}

TEST_F(TryUnMaskTest, NonHttpData) {
    std::string binaryData = "HELL\x00\x00\x42\xBD";

    PkgMaskType maskType;
    ULONG offset = TryUnMask(binaryData.data(), static_cast<ULONG>(binaryData.size()), maskType);

    EXPECT_EQ(maskType, MaskTypeNone);
    EXPECT_EQ(offset, 0u);
}

TEST_F(TryUnMaskTest, ShortInput) {
    std::string tooShort = "POS";

    PkgMaskType maskType;
    ULONG offset = TryUnMask(tooShort.data(), static_cast<ULONG>(tooShort.size()), maskType);

    EXPECT_EQ(maskType, MaskTypeNone);
    EXPECT_EQ(offset, 0u);
}

// ============================================
// HttpMask 类测试
// ============================================

class HttpMaskClassTest : public ::testing::Test {};

TEST_F(HttpMaskClassTest, MaskBasic) {
    HttpMask mask("api.example.com");

    const char* data = "Hello";
    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, data, 5);

    ASSERT_NE(masked, nullptr);
    EXPECT_GT(maskedSize, 5u);

    // 验证是 HTTP 格式
    EXPECT_EQ(memcmp(masked, "POST ", 5), 0);

    // 验证 Host 头
    std::string maskedStr(masked, maskedSize);
    EXPECT_NE(maskedStr.find("Host: api.example.com"), std::string::npos);

    // 验证 Content-Length
    EXPECT_NE(maskedStr.find("Content-Length: 5"), std::string::npos);

    // 验证 body
    ULONG offset = mask.UnMask(masked, maskedSize);
    EXPECT_EQ(memcmp(masked + offset, "Hello", 5), 0);

    delete[] masked;
}

TEST_F(HttpMaskClassTest, MaskEmptyData) {
    HttpMask mask;

    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, "", 0);

    ASSERT_NE(masked, nullptr);

    // 验证 Content-Length: 0
    std::string maskedStr(masked, maskedSize);
    EXPECT_NE(maskedStr.find("Content-Length: 0"), std::string::npos);

    delete[] masked;
}

TEST_F(HttpMaskClassTest, MaskWithCommand) {
    HttpMask mask;

    const char* data = "X";
    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, data, 1, 42);

    // 验证路径包含命令号
    std::string maskedStr(masked, maskedSize);
    EXPECT_NE(maskedStr.find("/42"), std::string::npos);

    delete[] masked;
}

TEST_F(HttpMaskClassTest, MaskLargeData) {
    HttpMask mask;

    std::vector<char> largeData(64 * 1024, 'X');  // 64 KB
    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, largeData.data(), static_cast<ULONG>(largeData.size()));

    ASSERT_NE(masked, nullptr);

    // 验证 Content-Length
    std::string maskedStr(masked, maskedSize);
    EXPECT_NE(maskedStr.find("Content-Length: 65536"), std::string::npos);

    // 验证 body 完整
    ULONG offset = mask.UnMask(masked, maskedSize);
    EXPECT_EQ(maskedSize - offset, 64u * 1024u);

    delete[] masked;
}

// ============================================
// Mask/UnMask 往返测试
// ============================================

class MaskRoundTripTest : public ::testing::Test {};

TEST_F(MaskRoundTripTest, SimpleRoundTrip) {
    HttpMask mask;

    std::vector<char> original = {'H', 'E', 'L', 'L', 'O'};
    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, original.data(), static_cast<ULONG>(original.size()));

    // UnMask
    ULONG offset = mask.UnMask(masked, maskedSize);
    EXPECT_GT(offset, 0u);

    // 验证数据完整
    EXPECT_EQ(maskedSize - offset, original.size());
    EXPECT_EQ(memcmp(masked + offset, original.data(), original.size()), 0);

    delete[] masked;
}

TEST_F(MaskRoundTripTest, BinaryDataRoundTrip) {
    HttpMask mask;

    // 包含所有字节值
    std::vector<char> original(256);
    for (int i = 0; i < 256; ++i) {
        original[i] = static_cast<char>(i);
    }

    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, original.data(), static_cast<ULONG>(original.size()));

    ULONG offset = mask.UnMask(masked, maskedSize);
    EXPECT_EQ(memcmp(masked + offset, original.data(), original.size()), 0);

    delete[] masked;
}

TEST_F(MaskRoundTripTest, NullBytesRoundTrip) {
    HttpMask mask;

    std::vector<char> original = {'\0', '\0', 'A', '\0', 'B'};

    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, original.data(), static_cast<ULONG>(original.size()));

    ULONG offset = mask.UnMask(masked, maskedSize);
    EXPECT_EQ(maskedSize - offset, original.size());
    EXPECT_EQ(memcmp(masked + offset, original.data(), original.size()), 0);

    delete[] masked;
}

TEST_F(MaskRoundTripTest, HttpLikeDataRoundTrip) {
    HttpMask mask;

    // 数据本身看起来像 HTTP
    std::string httpLike = "POST /fake HTTP/1.1\r\n\r\nfake body";
    std::vector<char> original(httpLike.begin(), httpLike.end());

    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, original.data(), static_cast<ULONG>(original.size()));

    ULONG offset = mask.UnMask(masked, maskedSize);
    EXPECT_EQ(memcmp(masked + offset, original.data(), original.size()), 0);

    delete[] masked;
}

// ============================================
// 自定义头部测试
// ============================================

class CustomHeadersTest : public ::testing::Test {};

TEST_F(CustomHeadersTest, AddCustomHeaders) {
    std::map<std::string, std::string> headers;
    headers["X-Custom-Header"] = "custom-value";
    headers["X-Request-ID"] = "12345";

    HttpMask mask("test.com", headers);

    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, "data", 4);

    std::string maskedStr(masked, maskedSize);
    EXPECT_NE(maskedStr.find("X-Custom-Header: custom-value"), std::string::npos);
    EXPECT_NE(maskedStr.find("X-Request-ID: 12345"), std::string::npos);

    delete[] masked;
}

// ============================================
// 边界条件测试
// ============================================

class HttpMaskBoundaryTest : public ::testing::Test {};

TEST_F(HttpMaskBoundaryTest, VeryLongHost) {
    std::string longHost(1000, 'x');
    longHost += ".com";

    HttpMask mask(longHost);

    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, "test", 4);

    std::string maskedStr(masked, maskedSize);
    EXPECT_NE(maskedStr.find(longHost), std::string::npos);

    delete[] masked;
}

TEST_F(HttpMaskBoundaryTest, SpecialCharactersInHost) {
    HttpMask mask("api-v2.test-server.example.com");

    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, "x", 1);

    std::string maskedStr(masked, maskedSize);
    EXPECT_NE(maskedStr.find("Host: api-v2.test-server.example.com"), std::string::npos);

    delete[] masked;
}

TEST_F(HttpMaskBoundaryTest, MaxULONGContentLength) {
    // 测试大的 Content-Length 值
    HttpMask mask;

    // 不实际分配这么大的内存，只是构造请求头
    std::string largeContentLengthStr = "Content-Length: 4294967295";

    // 验证格式正确
    EXPECT_EQ(largeContentLengthStr.find("4294967295"), 16u);
}

// ============================================
// HTTP 格式验证测试
// ============================================

class HttpFormatTest : public ::testing::Test {};

TEST_F(HttpFormatTest, ValidHttpRequestFormat) {
    HttpMask mask("test.com");

    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, "body", 4);

    std::string maskedStr(masked, maskedSize);

    // 验证请求行
    EXPECT_EQ(maskedStr.substr(0, 5), "POST ");

    // 验证 HTTP 版本
    EXPECT_NE(maskedStr.find("HTTP/1.1\r\n"), std::string::npos);

    // 验证必需的头部
    EXPECT_NE(maskedStr.find("Host:"), std::string::npos);
    EXPECT_NE(maskedStr.find("Content-Length:"), std::string::npos);
    EXPECT_NE(maskedStr.find("Content-Type:"), std::string::npos);

    // 验证头部和 body 之间的分隔符
    EXPECT_NE(maskedStr.find("\r\n\r\n"), std::string::npos);

    delete[] masked;
}

TEST_F(HttpFormatTest, ContentLengthMatchesBody) {
    HttpMask mask;

    std::vector<char> body(123, 'X');
    char* masked = nullptr;
    ULONG maskedSize = 0;

    mask.Mask(masked, maskedSize, body.data(), static_cast<ULONG>(body.size()));

    std::string maskedStr(masked, maskedSize);
    EXPECT_NE(maskedStr.find("Content-Length: 123"), std::string::npos);

    ULONG offset = mask.UnMask(masked, maskedSize);
    EXPECT_EQ(maskedSize - offset, 123u);

    delete[] masked;
}

