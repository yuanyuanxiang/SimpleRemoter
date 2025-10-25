#pragma once
#pragma warning(disable: 4996)
#ifdef _WIN32
#ifdef _WINDOWS
#include <afxwin.h>
#else
#include <windows.h>
#endif
#include <iostream>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <sstream>
#include <cstdarg>
#include "skCrypter.h"
#include <iomanip>


inline bool stringToBool(const std::string& str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return (lower == "true" || lower == "1");
}

class Logger
{
public:
    enum LogLevel {
        InfoLevel, WarningLevel, ErrorLevel
    };

    // ����ģʽ
    static Logger& getInstance()
    {
        static Logger instance;
        if (instance.pid.empty()) {
            char buf[16] = {};
            sprintf_s(buf, "%d", GetCurrentProcessId());
            instance.pid = buf;
            instance.InitLogFile("C:\\Windows\\Temp", instance.pid);
#ifdef _WINDOWS
            instance.enable = true; // ������־Ĭ�ϴ�
#else
            char var[32] = {};
            const char* name = skCrypt("ENABLE_LOG");
            DWORD size = GetEnvironmentVariableA(name, var, sizeof(var));
            instance.enable = stringToBool(var);
            instance.log("logger.h", __LINE__, "GetEnvironmentVariable: %s=%s\n", name, var);
#endif
        }
        return instance;
    }

    // ��ֹ�����͸�ֵ
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // ������־�ļ���
    void setLogFile(const std::string& filename)
    {
        std::lock_guard<std::mutex> lock(fileMutex);
        logFileName = filename;
    }

    // ������־
    void usingLog(bool b = true)
    {
        enable = b;
    }

    // д��־��֧�� printf ��ʽ��
    void log(const char* file, int line, const char* format, ...)
    {
        va_list args;
        va_start(args, format);

        std::string message = formatString(format, args);

        va_end(args);

        auto timestamp = getCurrentTimestamp();
        std::string id = pid.empty() ? "" : "[" + pid + "]";

        std::string logEntry = id + "[" + timestamp + "] [" + file + ":" + std::to_string(line) + "] " + message;
        if (enable) {
            if (running) {
                std::lock_guard<std::mutex> lock(queueMutex);
                logQueue.push(logEntry);
            } else {
                writeToFile(logEntry);
            }
        }
#ifndef _WINDOWS
#ifdef _DEBUG
        printf("%s", logEntry.c_str());
#endif
#endif
        cv.notify_one(); // ֪ͨд�߳�
    }

    // ֹͣ��־ϵͳ
    void stop()
    {
        if (!running) return;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            running = false;  // ��������״̬
        }
        cv.notify_one();
        if (workerThread.joinable()) {
            workerThread.join();
        }
        for (int i = 0; threadRun && i++ < 1000; Sleep(1));
    }

private:
    // ��־���·�����
    void InitLogFile(const std::string & dir, const std::string& pid)
    {
        time_t currentTime = time(nullptr);
        tm* localTime = localtime(&currentTime);
        char timeString[32];
        strftime(timeString, sizeof(timeString), "%Y-%m", localTime);
        char fileName[100];
#ifdef _WINDOWS
        sprintf_s(fileName, "\\YAMA_%s_%s.txt", timeString, pid.c_str());
#else
        sprintf_s(fileName, "\\log_%s_%s.txt", timeString, pid.c_str());
#endif
        logFileName = dir + fileName;
    }
    std::string logFileName;			 // ��־�ļ���
    bool enable;						 // �Ƿ�����
    bool threadRun;					     // ��־�߳�״̬
    std::queue<std::string> logQueue;    // ��־����
    std::mutex queueMutex;               // ���л�����
    std::condition_variable cv;          // ��������
    std::atomic<bool> running;           // �Ƿ�����
    std::thread workerThread;            // ��̨�߳�
    std::mutex fileMutex;                // �ļ�д����
    std::string pid;					 // ����ID

    Logger() : enable(false), threadRun(false), running(true), workerThread(&Logger::processLogs, this) {}

    ~Logger()
    {
        stop();
    }

    // ��̨�̴߳�����־
    void processLogs()
    {
        threadRun = true;
        while (running) {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [this]() {
                return !running || !logQueue.empty();
            });

            while (running && !logQueue.empty()) {
                std::string logEntry = logQueue.front();
                logQueue.pop();
                lock.unlock();

                // д����־�ļ�
                writeToFile(logEntry);

                lock.lock();
            }
            lock.unlock();
        }
        threadRun = false;
    }

    // д���ļ�
    void writeToFile(const std::string& logEntry)
    {
        std::lock_guard<std::mutex> lock(fileMutex);
        std::ofstream logFile(logFileName, std::ios::app);
        if (logFile.is_open()) {
            logFile << logEntry << std::endl;
        }
    }

    // ��ȡ��ǰʱ���
    std::string getCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &in_time_t);  // Windows ��ȫ�汾
#else
        localtime_r(&in_time_t, &tm);  // POSIX ��ȫ�汾
#endif

        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // ����־����ת��Ϊ�ַ���
    std::string logLevelToString(LogLevel level)
    {
        switch (level) {
        case InfoLevel:
            return "INFO";
        case WarningLevel:
            return "WARNING";
        case ErrorLevel:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

    // ��ʽ���ַ���
    std::string formatString(const char* format, va_list args)
    {
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), format, args);
        return std::string(buffer);
    }
};

inline const char* getFileName(const char* path)
{
    const char* fileName = strrchr(path, '\\');
    if (!fileName) {
        fileName = strrchr(path, '/');
    }
    return fileName ? fileName + 1 : path;
}

#ifdef _WINDOWS
#ifdef _DEBUG
#define Mprintf(format, ...) TRACE(format, __VA_ARGS__)
#else
#define Mprintf(format, ...) Logger::getInstance().log(getFileName(__FILE__), __LINE__, format, __VA_ARGS__)
#endif
#else
#define Mprintf(format, ...) Logger::getInstance().log(getFileName(skCrypt(__FILE__)), __LINE__, skCrypt(format), __VA_ARGS__)
#endif

#endif // _WIN32
