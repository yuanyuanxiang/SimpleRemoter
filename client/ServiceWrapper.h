#ifndef SERVICE_WRAPPER_H
#define SERVICE_WRAPPER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// 服务配置：根据需要可修改这些参数
#define SERVICE_NAME        "RemoteControlService"
#define SERVICE_DISPLAY     "Remote Control Service"
#define SERVICE_DESC        "Provides remote desktop control functionality"

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

// 直接模式标志
extern BOOL g_ServiceDirectMode;

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
void ServiceWrapper_Install(void);

// 卸载服务
void ServiceWrapper_Uninstall(void);

// 服务工作线程
DWORD WINAPI ServiceWrapper_WorkerThread(LPVOID lpParam);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_WRAPPER_H */
