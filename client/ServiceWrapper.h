#ifndef SERVICE_WRAPPER_H
#define SERVICE_WRAPPER_H

#include <windows.h>

typedef struct MyService {
    char Name[256];
    char Display[256];
    char Description[512];
} MyService;

typedef void (*ServiceLogFunc)(const char* message);

#ifdef __cplusplus
extern "C" {
#endif

/*
# 停止服务
net stop RemoteControlService

# 查看状态（应该显示 STOPPED）
sc query RemoteControlService

# 启动服务
net start RemoteControlService

# 再次查看状态（应该显示 RUNNING）
sc query RemoteControlService
*/

// 自定义服务信息
void InitWindowsService(MyService info, ServiceLogFunc log);

// 以Windows服务模式运行程序
BOOL RunAsWindowsService(int argc, const char* argv[]);

// 检查服务状态
// 参数:
//   registered - 输出参数，服务是否已注册
//   running - 输出参数，服务是否正在运行
//   exePath - 输出参数，服务可执行文件路径（可为NULL）
//   exePathSize - exePath缓冲区大小
// 返回: 成功返回TRUE
BOOL ServiceWrapper_CheckStatus(BOOL* registered, BOOL* running,
                                char* exePath, size_t exePathSize);

// 简单启动服务
// 返回: ERROR_SUCCESS 或错误码
int ServiceWrapper_StartSimple(void);

// 运行服务（作为服务主入口）
// 返回: ERROR_SUCCESS 或错误码
int ServiceWrapper_Run(void);

// 安装服务
BOOL ServiceWrapper_Install(void);

// 卸载服务
BOOL ServiceWrapper_Uninstall(void);

// 服务工作线程
DWORD WINAPI ServiceWrapper_WorkerThread(LPVOID lpParam);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_WRAPPER_H */
