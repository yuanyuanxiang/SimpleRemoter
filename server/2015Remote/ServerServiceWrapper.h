#ifndef SERVER_SERVICE_WRAPPER_H
#define SERVER_SERVICE_WRAPPER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// 鏈嶅姟閰嶇疆锛氭湇鍔＄浣跨敤涓嶅悓鐨勬湇鍔″悕
#define SERVER_SERVICE_NAME        "YamaControlService"
#define SERVER_SERVICE_DISPLAY     "Yama Control Service"
#define SERVER_SERVICE_DESC        "Provides remote desktop control server functionality."

/*
# 鍋滄鏈嶅姟
net stop YamaControlService

# 鏌ョ湅鐘舵€侊紙搴旇鏄剧ず STOPPED锛?
sc query YamaControlService

# 鍚姩鏈嶅姟
net start YamaControlService

# 鍐嶆鏌ョ湅鐘舵€侊紙搴旇鏄剧ず RUNNING锛?
sc query YamaControlService
*/

// 妫€鏌ユ湇鍔＄姸鎬?
// 鍙傛暟:
//   registered - 杈撳嚭鍙傛暟锛屾湇鍔℃槸鍚﹀凡娉ㄥ唽
//   running - 杈撳嚭鍙傛暟锛屾湇鍔℃槸鍚︽鍦ㄨ繍琛?
//   exePath - 杈撳嚭鍙傛暟锛屾湇鍔″彲鎵ц鏂囦欢璺緞锛堝彲涓篘ULL锛?
//   exePathSize - exePath缂撳啿鍖哄ぇ灏?
// 杩斿洖: 鎴愬姛杩斿洖TRUE
BOOL ServerService_CheckStatus(BOOL* registered, BOOL* running,
                               char* exePath, size_t exePathSize);

// 绠€鍗曞惎鍔ㄦ湇鍔?
// 杩斿洖: ERROR_SUCCESS 鎴栭敊璇爜
int ServerService_StartSimple(void);

// 杩愯鏈嶅姟锛堜綔涓烘湇鍔′富鍏ュ彛锛?
// 杩斿洖: ERROR_SUCCESS 鎴栭敊璇爜
int ServerService_Run(void);

// 鍋滄鏈嶅姟
// 杩斿洖: ERROR_SUCCESS 鎴栭敊璇爜
int ServerService_Stop(void);

// 瀹夎鏈嶅姟
BOOL ServerService_Install(void);

// 鍗歌浇鏈嶅姟
BOOL ServerService_Uninstall(void);

// 鏈嶅姟宸ヤ綔绾跨▼
DWORD WINAPI ServerService_WorkerThread(LPVOID lpParam);

#ifdef __cplusplus
}
#endif

#endif /* SERVER_SERVICE_WRAPPER_H */
