#pragma once

#include "header.h"

// 鏁版嵁鍖呭崗璁皝瑁呮牸寮?
// Copy left: 962914132@qq.com & ChatGPT
enum PkgMaskType {
    MaskTypeUnknown = -1,
    MaskTypeNone,
    MaskTypeHTTP,
    MaskTypeNum,
};

#define DEFAULT_HOST "example.com"

inline ULONG UnMaskHttp(char* src, ULONG srcSize)
{
    const char* header_end_mark = "\r\n\r\n";
    const ULONG mark_len = 4;

    // 鏌ユ壘 HTTP 澶撮儴缁撴潫鏍囪
    for (ULONG i = 0; i + mark_len <= srcSize; ++i) {
        if (memcmp(src + i, header_end_mark, mark_len) == 0) {
            return i + mark_len;  // 杩斿洖 Body 璧峰浣嶇疆
        }
    }
    return 0;  // 鏃犳晥鏁版嵁
}

// TryUnMask 灏濊瘯鍘绘帀浼鐨勫崗璁ご.
inline ULONG TryUnMask(char* src, ULONG srcSize, PkgMaskType& maskHit)
{
    if (srcSize >= 5 && memcmp(src, "POST ", 5) == 0) {
        maskHit = MaskTypeHTTP;
        return UnMaskHttp(src, srcSize);
    }
    maskHit = MaskTypeNone;
    return 0;
}

// PkgMask 閽堝娑堟伅杩涗竴姝ュ姞瀵嗐€佹贩娣嗘垨浼.
class PkgMask
{
protected:
    virtual ~PkgMask() {}
public:
    virtual void Destroy()
    {
        delete this;
    }
    virtual void Mask(char*& dst, ULONG& dstSize, char* src, ULONG srcSize, int cmd = -1)
    {
        dst = src;
        dstSize = srcSize;
    }
    virtual ULONG UnMask(char* src, ULONG srcSize)
    {
        return 0;
    }
    virtual PkgMask* SetServer(const std::string& addr)
    {
        return this;
    }
    virtual PkgMaskType GetMaskType() const
    {
        return MaskTypeNone;
    }
};

class HttpMask : public PkgMask
{
public:
    virtual PkgMaskType GetMaskType() const override
    {
        return MaskTypeHTTP;
    }

    /**
     * @brief 鏋勯€犲嚱鏁?
     * @param host HTTP Host 澶村瓧娈?
     */
    explicit HttpMask(const std::string& host, const std::map<std::string, std::string>& headers = {}) :
        product_(GenerateRandomString()), host_(host)
    {
        // 鍒濆鍖栭殢鏈烘暟鐢熸垚鍣?
        srand(static_cast<unsigned>(time(nullptr)));
        char buf[32];
        sprintf_s(buf, "V%d.%d.%d", rand() % 10, rand() % 10, rand() % 10);
        version_ = buf;
        user_agent_ = GetEnhancedSystemUA(product_, version_);
        for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
            headers_ += it->first + ": " + it->second + "\r\n";
        }
    }

    /**
     * @brief 灏嗗師濮嬫暟鎹吉瑁呬负 HTTP 璇锋眰
     * @param dst      [杈撳嚭] 浼鍚庣殑鏁版嵁缂撳啿鍖猴紙闇€璋冪敤鑰呴噴鏀撅級
     * @param dstSize  [杈撳嚭] 浼鍚庢暟鎹暱搴?
     * @param src      鍘熷鏁版嵁鎸囬拡
     * @param srcSize  鍘熷鏁版嵁闀垮害
     * @param cmd      鍛戒护鍙?
     */
    void Mask(char*& dst, ULONG& dstSize, char* src, ULONG srcSize, int cmd = -1)
    {
        // 鐢熸垚鍔ㄦ€?HTTP 澶撮儴
        std::string http_header =
            "POST " + GeneratePath(cmd) + " HTTP/1.1\r\n"
            "Host: " + host_ + "\r\n"
            "User-Agent: " + user_agent_ + "\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: " + std::to_string(srcSize) + "\r\n" + headers_ +
            "Connection: keep-alive\r\n"
            "\r\n";  // 绌鸿鍒嗛殧澶撮儴鍜?Body

        // 鍒嗛厤杈撳嚭缂撳啿鍖?
        dstSize = static_cast<ULONG>(http_header.size()) + srcSize;
        dst = new char[dstSize];

        // 鎷疯礉鏁版嵁锛欻TTP 澶撮儴 + 鍘熷鏁版嵁
        memcpy(dst, http_header.data(), http_header.size());
        memcpy(dst + http_header.size(), src, srcSize);
    }

    /**
     * @brief 浠?HTTP 鏁版嵁涓彁鍙栧師濮嬫暟鎹捣濮嬩綅缃?
     * @param src      鏀跺埌鐨?HTTP 鏁版嵁
     * @param srcSize  鏁版嵁闀垮害
     * @return ULONG   鍘熷鏁版嵁鍦?src 涓殑璧峰鍋忕Щ閲忥紙澶辫触杩斿洖 0锛?
     */
    ULONG UnMask(char* src, ULONG srcSize)
    {
        return UnMaskHttp(src, srcSize);
    }

    PkgMask* SetServer(const std::string& addr) override
    {
        host_ = addr;
        return this;
    }
private:
    static std::string GetEnhancedSystemUA(const std::string& appName, const std::string& appVersion)
    {
#ifdef _WIN32
        OSVERSIONINFOEX osvi = {};
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        GetVersionEx((OSVERSIONINFO*)&osvi);

        // 鑾峰彇绯荤粺鏋舵瀯
        SYSTEM_INFO si;
        GetNativeSystemInfo(&si);
        std::string arch = (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) ? "Win64; x64" :
                           (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) ? "Win64; ARM64" :
                           "WOW64";

        return "Mozilla/5.0 (" +
               std::string("Windows NT ") +
               std::to_string(osvi.dwMajorVersion) + "." +
               std::to_string(osvi.dwMinorVersion) + "; " +
               arch + ") " +
               appName + "/" + appVersion;
#else
        return "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36";
#endif
    }

    std::string host_;      // 鐩爣涓绘満
    std::string product_;   // 浜у搧鍚嶇О
    std::string version_;   // 浜у搧鐗堟湰
    std::string user_agent_;// 浠ｇ悊鍚嶇О
    std::string headers_;   // 鑷畾涔夎姹傚ご

    /** 鐢熸垚闅忔満 URL 璺緞 */
    std::string GenerateRandomString(int size = 8) const
    {
        static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        char path[32];
        for (int i = 0; i < size; ++i) {
            path[i] = charset[rand() % (sizeof(charset) - 1)];
        }
        path[size] = 0;
        return path;
    }
    std::string GeneratePath(int cmd) const
    {
        static std::string root = "/" + product_ + "/" + GenerateRandomString() + "/";
        return root + (cmd == -1 ? GenerateRandomString() : std::to_string(cmd));
    }
};
