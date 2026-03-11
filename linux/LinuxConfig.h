#pragma once
#include <string>
#include <map>
#include <fstream>
#include <sys/stat.h>
#include <cstdlib>

// ============== 轻量 INI 配置文件读写（Linux）==============
// 配置文件路径: ~/.config/ghost/*.conf
// 格式: key=value（按行存储，不分 section）
class LinuxConfig
{
public:
    // 使用指定配置文件名（如 "screen" -> ~/.config/ghost/screen.conf）
    explicit LinuxConfig(const std::string& name = "config")
    {
        const char* xdg = getenv("XDG_CONFIG_HOME");
        if (xdg && xdg[0]) {
            m_dir = std::string(xdg) + "/ghost";
        } else {
            const char* home = getenv("HOME");
            if (!home) home = "/tmp";
            m_dir = std::string(home) + "/.config/ghost";
        }
        m_path = m_dir + "/" + name + ".conf";
        Load();
    }

    std::string GetStr(const std::string& key, const std::string& def = "") const
    {
        auto it = m_data.find(key);
        return it != m_data.end() ? it->second : def;
    }

    void SetStr(const std::string& key, const std::string& value)
    {
        m_data[key] = value;
        Save();
    }

    int GetInt(const std::string& key, int def = 0) const
    {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            try { return std::stoi(it->second); } catch (...) {}
        }
        return def;
    }

    void SetInt(const std::string& key, int value)
    {
        m_data[key] = std::to_string(value);
        Save();
    }

    // 重新加载配置
    void Reload() { Load(); }

private:
    std::string m_dir, m_path;
    std::map<std::string, std::string> m_data;

    void Load()
    {
        m_data.clear();
        std::ifstream f(m_path);
        std::string line;
        while (std::getline(f, line)) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                m_data[line.substr(0, eq)] = line.substr(eq + 1);
            }
        }
    }

    void Save()
    {
        // 递归创建目录（mkdir -p 效果）
        MkdirRecursive(m_dir);
        std::ofstream f(m_path, std::ios::trunc);
        if (!f.is_open()) {
            return;  // 打开失败
        }
        for (auto& kv : m_data) {
            f << kv.first << "=" << kv.second << "\n";
        }
    }

    // 递归创建目录
    static void MkdirRecursive(const std::string& path)
    {
        size_t pos = 0;
        while ((pos = path.find('/', pos + 1)) != std::string::npos) {
            mkdir(path.substr(0, pos).c_str(), 0755);
        }
        mkdir(path.c_str(), 0755);
    }
};
