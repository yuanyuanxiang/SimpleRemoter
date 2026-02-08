#pragma once

// IniParser.h - 轻量级 INI 文件解析器（header-only）
// 特点：
//   - 不 trim key/value，保留原始空格（适用于多语言 key 精确匹配）
//   - 无文件大小限制（不依赖 GetPrivateProfileSection）
//   - 支持 ; 和 # 注释
//   - 支持多 section
//   - 支持转义序列：\n \r \t \\ \" （key 和 value 均支持）
//   - 纯 C++ 标准库，不依赖 MFC / Windows API

#include <cstdio>
#include <cstring>
#include <string>
#include <map>

class CIniParser
{
public:
    typedef std::map<std::string, std::string> TKeyVal;
    typedef std::map<std::string, TKeyVal>     TSections;

    CIniParser() {}

    void Clear()
    {
        m_sections.clear();
    }

    // 加载 INI 文件，返回是否成功
    // 文件不存在返回 false，空文件返回 true
    bool LoadFile(const char* filePath)
    {
        Clear();

        if (!filePath || !filePath[0])
            return false;

        FILE* f = nullptr;
#ifdef _MSC_VER
        if (fopen_s(&f, filePath, "r") != 0 || !f)
            return false;
#else
        f = fopen(filePath, "r");
        if (!f)
            return false;
#endif

        std::string currentSection;
        char line[4096];

        while (fgets(line, sizeof(line), f)) {
            // 去除行尾换行符
            size_t len = strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
                line[--len] = '\0';

            if (len == 0)
                continue;

            // 跳过注释
            if (line[0] == ';' || line[0] == '#')
                continue;

            // 检测 section 头: [SectionName]
            // 真正的 section 头：']' 后面没有 '='（否则是 key=value）
            if (line[0] == '[') {
                char* end = strchr(line, ']');
                if (end) {
                    char* eqAfter = strchr(end + 1, '=');
                    if (!eqAfter) {
                        // 纯 section 头，如 [Strings]
                        *end = '\0';
                        currentSection = line + 1;
                        continue;
                    }
                    // ']' 后有 '='，如 [使用FRP]=[Using FRP]，当作 key=value 处理
                }
            }

            // 不在任何 section 内则跳过
            if (currentSection.empty())
                continue;

            // 解析 key=value（只按第一个 '=' 分割，不 trim）
            // key 和 value 均做反转义（\n \r \t \\ \"）
            char* eq = strchr(line, '=');
            if (eq && eq != line) {
                *eq = '\0';
                std::string key   = Unescape(std::string(line));
                std::string value = Unescape(std::string(eq + 1));
                m_sections[currentSection][key] = value;
            }
        }

        fclose(f);
        return true;
    }

    // 获取指定 section 下的 key 对应的 value
    // 未找到时返回 defaultVal
    const char* GetValue(const char* section, const char* key,
                         const char* defaultVal = "") const
    {
        auto itSec = m_sections.find(section ? section : "");
        if (itSec == m_sections.end())
            return defaultVal;

        auto itKey = itSec->second.find(key ? key : "");
        if (itKey == itSec->second.end())
            return defaultVal;

        return itKey->second.c_str();
    }

    // 获取整个 section 的所有键值对，不存在返回 nullptr
    const TKeyVal* GetSection(const char* section) const
    {
        auto it = m_sections.find(section ? section : "");
        if (it == m_sections.end())
            return nullptr;
        return &it->second;
    }

    // 获取 section 中的键值对数量
    size_t GetSectionSize(const char* section) const
    {
        const TKeyVal* p = GetSection(section);
        return p ? p->size() : 0;
    }

    // 获取所有 section
    const TSections& GetAllSections() const
    {
        return m_sections;
    }

private:
    TSections m_sections;

    // 反转义：将字面量 \n \r \t \\ \" 转为对应的控制字符
    static std::string Unescape(const std::string& s)
    {
        std::string result;
        result.reserve(s.size());

        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                switch (s[i + 1]) {
                case 'n':
                    result += '\n';
                    i++;
                    break;
                case 'r':
                    result += '\r';
                    i++;
                    break;
                case 't':
                    result += '\t';
                    i++;
                    break;
                case '\\':
                    result += '\\';
                    i++;
                    break;
                case '"':
                    result += '"';
                    i++;
                    break;
                default:
                    result += s[i];
                    break;  // 未知转义保留原样
                }
            } else {
                result += s[i];
            }
        }

        return result;
    }
};
