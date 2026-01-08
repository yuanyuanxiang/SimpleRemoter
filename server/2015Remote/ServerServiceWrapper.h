#ifndef SERVER_SERVICE_WRAPPER_H
#define SERVER_SERVICE_WRAPPER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

    // 服务配置：服务端使用不同的服务名
#define SERVER_SERVICE_NAME        "YamaControlService"
#define SERVER_SERVICE_DISPLAY     "Yama Control Service"
#define SERVER_SERVICE_DESC        "Provides remote desktop control server functionality."

/*
# 停止服务
net stop YamaControlService

# 查看状态（应该显示 STOPPED）
sc query YamaControlService

# 启动服务
net start YamaControlService

# 再次查看状态（应该显示 RUNNING）
sc query YamaControlService
*/

// 检查服务状态
// 参数:
//   registered - 输出参数，服务是否已注册
//   running - 输出参数，服务是否正在运行
//   exePath - 输出参数，服务可执行文件路径（可为NULL）
//   exePathSize - exePath缓冲区大小
// 返回: 成功返回TRUE
    BOOL ServerService_CheckStatus(BOOL* registered, BOOL* running,
        char* exePath, size_t exePathSize);

    // 简单启动服务
    // 返回: ERROR_SUCCESS 或错误码
    int ServerService_StartSimple(void);

    // 运行服务（作为服务主入口）
    // 返回: ERROR_SUCCESS 或错误码
    int ServerService_Run(void);

    // 停止服务
    // 返回: ERROR_SUCCESS 或错误码
    int ServerService_Stop(void);

    // 安装服务
    BOOL ServerService_Install(void);

    // 卸载服务
    BOOL ServerService_Uninstall(void);

    // 服务工作线程
    DWORD WINAPI ServerService_WorkerThread(LPVOID lpParam);

#ifdef __cplusplus
}
#endif

#endif /* SERVER_SERVICE_WRAPPER_H */
