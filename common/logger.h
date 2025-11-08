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

    // 单例模式
    static Logger& getInstance()
    {
        static Logger instance;
        if (instance.pid.empty()) {
            char buf[16] = {};
            sprintf_s(buf, "%d", GetCurrentProcessId());
            instance.pid = buf;
            instance.InitLogFile("C:\\Windows\\Temp", instance.pid);
#ifdef _WINDOWS
            instance.enable = true; // 主控日志默认打开
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

    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 设置日志文件名
    void setLogFile(const std::string& filename)
    {
        std::lock_guard<std::mutex> lock(fileMutex);
        logFileName = filename;
    }

    // 启用日志
    void usingLog(bool b = true)
    {
        enable = b;
    }

    // 写日志，支持 printf 格式化
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
        cv.notify_one(); // 通知写线程
    }

    // 停止日志系统
    void stop()
    {
        if (!running) return;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            running = false;  // 设置运行状态
        }
        cv.notify_one();
        if (workerThread.joinable()) {
            try {
                workerThread.join();
            } catch (const std::system_error& e) {
                printf("Join failed: %s [%d]\n", e.what(), e.code().value());
            }
        }
        for (int i = 0; threadRun && i++ < 1000; Sleep(1));
    }

private:
    // 日志按月份起名
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
    std::string logFileName;			 // 日志文件名
    bool enable;						 // 是否启用
    bool threadRun;					     // 日志线程状态
    std::queue<std::string> logQueue;    // 日志队列
    std::mutex queueMutex;               // 队列互斥锁
    std::condition_variable cv;          // 条件变量
    std::atomic<bool> running;           // 是否运行
    std::thread workerThread;            // 后台线程
    std::mutex fileMutex;                // 文件写入锁
    std::string pid;					 // 进程ID

    Logger() : enable(false), threadRun(false), running(true), workerThread(&Logger::processLogs, this) {}

    ~Logger()
    {
        stop();
    }

    // 后台线程处理日志
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

                // 写入日志文件
                writeToFile(logEntry);

                lock.lock();
            }
            lock.unlock();
        }
        threadRun = false;
    }

    // 写入文件
    void writeToFile(const std::string& logEntry)
    {
        std::lock_guard<std::mutex> lock(fileMutex);
        std::ofstream logFile(logFileName, std::ios::app);
        if (logFile.is_open()) {
            logFile << logEntry << std::endl;
        }
    }

    // 获取当前时间戳
    std::string getCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &in_time_t);  // Windows 安全版本
#else
        localtime_r(&in_time_t, &tm);  // POSIX 安全版本
#endif

        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // 将日志级别转换为字符串
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

    // 格式化字符串
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
