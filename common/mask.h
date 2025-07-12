#pragma once

#include "commands.h"

// 数据包协议封装格式
// Copy left: 962914132@qq.com & ChatGPT
enum PkgMaskType {
	MaskTypeUnknown = -1,
	MaskTypeNone,
	MaskTypeHTTP,
	MaskTypeNum,
};

inline ULONG UnMaskHttp(char* src, ULONG srcSize) {
	const char* header_end_mark = "\r\n\r\n";
	const ULONG mark_len = 4;

	// 查找 HTTP 头部结束标记
	for (ULONG i = 0; i + mark_len <= srcSize; ++i) {
		if (memcmp(src + i, header_end_mark, mark_len) == 0) {
			return i + mark_len;  // 返回 Body 起始位置
		}
	}
	return 0;  // 无效数据
}

// TryUnMask 尝试去掉伪装的协议头.
inline ULONG TryUnMask(char* src, ULONG srcSize, PkgMaskType& maskHit) {
	if (srcSize >= 5 && memcmp(src, "POST ", 5) == 0) {
		maskHit = MaskTypeHTTP;
		return UnMaskHttp(src, srcSize);
	}
	maskHit = MaskTypeNone;
	return 0;
}

// PkgMask 针对消息进一步加密、混淆或伪装.
class PkgMask {
protected:
    virtual ~PkgMask() {}
public:
    virtual void Destroy() {
        delete this;
    }
    virtual void Mask(char*& dst, ULONG& dstSize, char* src, ULONG srcSize, int cmd = -1) {
        dst = src;
        dstSize = srcSize;
    }
    virtual ULONG UnMask(char* src, ULONG srcSize) {
        return 0;
    }
    virtual void SetServer(const char* addr) {}
    virtual PkgMaskType GetMaskType() const {
        return MaskTypeNone;
    }
};

class HttpMask : public PkgMask {
public:
	virtual PkgMaskType GetMaskType() const override {
		return MaskTypeHTTP;
	}

    /**
     * @brief 构造函数
     * @param host HTTP Host 头字段
     */
    explicit HttpMask(const std::string& host) : product_(GenerateRandomString()), host_(host) {
        // 初始化随机数生成器
        srand(static_cast<unsigned>(time(nullptr)));
        char buf[32];
        sprintf_s(buf, "V%d.%d.%d", rand() % 10, rand() % 10, rand() % 10);
        version_ = buf;
        user_agent_ = GetEnhancedSystemUA(product_, version_);
    }

    /**
     * @brief 将原始数据伪装为 HTTP 请求
     * @param dst      [输出] 伪装后的数据缓冲区（需调用者释放）
     * @param dstSize  [输出] 伪装后数据长度
     * @param src      原始数据指针
     * @param srcSize  原始数据长度
     * @param cmd      命令号
     */
    void Mask(char*& dst, ULONG& dstSize, char* src, ULONG srcSize, int cmd = -1) {
        // 生成动态 HTTP 头部
        std::string http_header =
            "POST " + GeneratePath(cmd) + " HTTP/1.1\r\n"
            "Host: " + host_ + "\r\n"
            "User-Agent: " + user_agent_ + "\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: " + std::to_string(srcSize) + "\r\n"
            "Connection: keep-alive\r\n"
            "\r\n";  // 空行分隔头部和 Body

        // 分配输出缓冲区
        dstSize = static_cast<ULONG>(http_header.size()) + srcSize;
        dst = new char[dstSize];

        // 拷贝数据：HTTP 头部 + 原始数据
        memcpy(dst, http_header.data(), http_header.size());
        memcpy(dst + http_header.size(), src, srcSize);
    }

    /**
     * @brief 从 HTTP 数据中提取原始数据起始位置
     * @param src      收到的 HTTP 数据
     * @param srcSize  数据长度
     * @return ULONG   原始数据在 src 中的起始偏移量（失败返回 0）
     */
    ULONG UnMask(char* src, ULONG srcSize) {
        return UnMaskHttp(src, srcSize);
    }

    void SetServer(const char* addr) {
        host_ = addr;
    }
private:
    static std::string GetEnhancedSystemUA(const std::string& appName, const std::string& appVersion) {
#ifdef _WIN32
        OSVERSIONINFOEX osvi = {};
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        GetVersionEx((OSVERSIONINFO*)&osvi);

        // 获取系统架构
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

    std::string host_;      // 目标主机
    std::string product_;   // 产品名称
    std::string version_;   // 产品版本
    std::string user_agent_;// 代理名称

    /** 生成随机 URL 路径 */
    std::string GenerateRandomString(int size = 8) const {
        static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        char path[32];
        for (int i = 0; i < size; ++i) {
            path[i] = charset[rand() % (sizeof(charset) - 1)];
        }
        path[size] = 0;
        return path;
    }
    std::string GeneratePath(int cmd) const {
        static std::string root = "/" + product_ + "/" + GenerateRandomString() + "/";
        return root + (cmd == -1 ? GenerateRandomString() : std::to_string(cmd));
    }
};
