#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

// Linux 剪贴板操作封装
// 使用外部工具 xclip 或 xsel 实现，简单可靠
// 支持 X11 环境，Wayland 需要 wl-clipboard

class ClipboardHandler
{
public:
    // 检测可用的剪贴板工具
    static const char* GetClipboardTool()
    {
        static const char* tool = nullptr;
        static bool checked = false;

        if (!checked) {
            checked = true;
            // 优先使用 xclip，其次 xsel
            if (system("which xclip > /dev/null 2>&1") == 0) {
                tool = "xclip";
            } else if (system("which xsel > /dev/null 2>&1") == 0) {
                tool = "xsel";
            }
        }
        return tool;
    }

    // 检查剪贴板功能是否可用
    static bool IsAvailable()
    {
        // 需要有剪贴板工具且 DISPLAY 环境变量存在
        return GetClipboardTool() != nullptr && getenv("DISPLAY") != nullptr;
    }

    // 获取剪贴板中的文件列表
    // 返回文件的完整路径列表（UTF-8），失败返回空列表
    // X11 使用 text/uri-list MIME 类型存储文件路径
    static std::vector<std::string> GetFiles()
    {
        std::vector<std::string> files;

        const char* tool = GetClipboardTool();
        if (!tool || strcmp(tool, "xclip") != 0) {
            // xsel 不支持指定目标类型，只能用 xclip
            return files;
        }

        // 获取 text/uri-list 类型的剪贴板内容
        std::string cmd = "xclip -selection clipboard -t text/uri-list -o 2>/dev/null";

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return files;

        std::string result;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }

        int status = pclose(pipe);
        if (WEXITSTATUS(status) != 0 || result.empty()) {
            return files;
        }

        // 解析 URI 列表，每行一个 file:// URI
        // 格式: file:///path/to/file 或 file://hostname/path
        size_t pos = 0;
        while (pos < result.size()) {
            size_t lineEnd = result.find('\n', pos);
            if (lineEnd == std::string::npos) lineEnd = result.size();

            std::string line = result.substr(pos, lineEnd - pos);
            pos = lineEnd + 1;

            // 去除回车
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
                line.pop_back();
            }

            // 跳过空行和注释
            if (line.empty() || line[0] == '#') continue;

            // 解析 file:// URI
            if (line.compare(0, 7, "file://") == 0) {
                std::string path;
                if (line.compare(0, 8, "file:///") == 0) {
                    // file:///path/to/file -> /path/to/file
                    path = line.substr(7);
                } else {
                    // file://hostname/path -> /path (忽略 hostname)
                    size_t slash = line.find('/', 7);
                    if (slash != std::string::npos) {
                        path = line.substr(slash);
                    }
                }

                // URL 解码 (处理 %20 等转义字符)
                path = UrlDecode(path);

                if (!path.empty() && path[0] == '/') {
                    files.push_back(path);
                }
            }
        }

        return files;
    }

    // 获取剪贴板文本
    // 返回 UTF-8 编码的文本，失败返回空字符串
    static std::string GetText()
    {
        const char* tool = GetClipboardTool();
        if (!tool) return "";

        std::string cmd;
        if (strcmp(tool, "xclip") == 0) {
            cmd = "xclip -selection clipboard -o 2>/dev/null";
        } else {
            cmd = "xsel --clipboard --output 2>/dev/null";
        }

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";

        std::string result;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }

        int status = pclose(pipe);
        if (WEXITSTATUS(status) != 0) {
            return "";  // 命令执行失败
        }

        return result;
    }

    // 设置剪贴板文本
    // text: UTF-8 编码的文本
    // 返回是否成功
    static bool SetText(const std::string& text)
    {
        if (text.empty()) return true;

        const char* tool = GetClipboardTool();
        if (!tool) return false;

        std::string cmd;
        if (strcmp(tool, "xclip") == 0) {
            cmd = "xclip -selection clipboard 2>/dev/null";
        } else {
            cmd = "xsel --clipboard --input 2>/dev/null";
        }

        FILE* pipe = popen(cmd.c_str(), "w");
        if (!pipe) return false;

        size_t written = fwrite(text.c_str(), 1, text.size(), pipe);
        int status = pclose(pipe);

        return written == text.size() && WEXITSTATUS(status) == 0;
    }

    // 设置剪贴板文本（从原始字节）
    // data: 文本数据（可能是 GBK 或 UTF-8）
    // len: 数据长度
    static bool SetTextRaw(const char* data, size_t len)
    {
        if (!data || len == 0) return true;

        // 服务端发来的文本可能是 GBK 编码，尝试转换为 UTF-8
        // 如果已经是 UTF-8 则直接使用
        std::string text = ConvertToUtf8(data, len);
        return SetText(text);
    }

private:
    // URL 解码 (处理 %XX 转义字符)
    static std::string UrlDecode(const std::string& str)
    {
        std::string result;
        result.reserve(str.size());

        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '%' && i + 2 < str.size()) {
                // 解析两位十六进制数
                char hex[3] = { str[i + 1], str[i + 2], 0 };
                char* end;
                long val = strtol(hex, &end, 16);
                if (end == hex + 2 && val >= 0 && val <= 255) {
                    result += (char)val;
                    i += 2;
                    continue;
                }
            }
            result += str[i];
        }

        return result;
    }

    // 尝试将 GBK 转换为 UTF-8
    // 如果转换失败或已经是 UTF-8，返回原始数据
    static std::string ConvertToUtf8(const char* data, size_t len)
    {
        // 检查是否已经是有效的 UTF-8
        if (IsValidUtf8(data, len)) {
            return std::string(data, len);
        }

        // 使用临时文件进行 iconv 转换
        char tmpIn[] = "/tmp/clip_in_XXXXXX";

        int fdIn = mkstemp(tmpIn);
        if (fdIn < 0) {
            return std::string(data, len);
        }

        // 写入输入数据
        write(fdIn, data, len);
        close(fdIn);

        // 使用 iconv 转换
        std::string cmd = "iconv -f GBK -t UTF-8 ";
        cmd += tmpIn;
        cmd += " 2>/dev/null";

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            unlink(tmpIn);
            return std::string(data, len);
        }

        std::string result;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }

        int status = pclose(pipe);
        unlink(tmpIn);

        // 如果转换成功且结果非空，返回转换后的文本
        if (WEXITSTATUS(status) == 0 && !result.empty()) {
            return result;
        }

        // 转换失败，返回原始数据
        return std::string(data, len);
    }

    // 检查是否是有效的 UTF-8 序列
    static bool IsValidUtf8(const char* data, size_t len)
    {
        const unsigned char* bytes = (const unsigned char*)data;
        size_t i = 0;

        while (i < len) {
            if (bytes[i] <= 0x7F) {
                // ASCII
                i++;
            } else if ((bytes[i] & 0xE0) == 0xC0) {
                // 2-byte sequence
                if (i + 1 >= len || (bytes[i + 1] & 0xC0) != 0x80) return false;
                i += 2;
            } else if ((bytes[i] & 0xF0) == 0xE0) {
                // 3-byte sequence
                if (i + 2 >= len || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80) return false;
                i += 3;
            } else if ((bytes[i] & 0xF8) == 0xF0) {
                // 4-byte sequence
                if (i + 3 >= len || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80 || (bytes[i + 3] & 0xC0) != 0x80) return false;
                i += 4;
            } else {
                return false;
            }
        }
        return true;
    }
};
