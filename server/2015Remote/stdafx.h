
// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件

#pragma once

#include "common/commands.h"

// 使用压缩算法，算法需要和clien的stdafx.h匹配
#define USING_COMPRESS 1

// 是否使用ZLIB
#define USING_ZLIB 0

#if !USING_ZLIB
// 是否使用LZ4
#define USING_LZ4 0
#if !USING_LZ4
#define USING_ZSTD 1
#define USING_CTX 0
#endif
#endif

#ifndef _SECURE_ATL
#define _SECURE_ATL 1
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // 从 Windows 头中排除极少使用的资料
#endif

#ifdef _DEBUG
#define Mprintf(format, ...) printf(format, ##__VA_ARGS__)
#else
#define Mprintf(format, ...) 
#endif

// 移除对话框中MFC控件的支持，减小静态编译程序的大小
#define _AFX_NO_MFC_CONTROLS_IN_DIALOGS

// 关闭对话框时删除对话框
#define CLOSE_DELETE_DLG 0

// 检测内存泄漏，需安装VLD；否则请注释此行
// 如果安装了VLD, 请将安装路径添加到环境变量: 名称为"VLDPATH", 路径为"D:\Program Files (x86)\Visual Leak Detector"
// 请根据实际安装目录填写VLDPATH. 或者手动编辑每个项目文件的头文件目录和库目录. 有关下载VLD库的信息请参考下面链接.
// VS2017以前版本的VLD: https://kinddragon.github.io/vld
// VS2019使用的VLD（支持以往的VS版本, 推荐）: https://github.com/oneiric/vld/releases/tag/v2.7.0
// 如果要将受控端程序放到其他机器上面运行, 请使用Release模式生成的程序, 以解除对VLD的依赖; 否则你需要将VLD相关文件一同拷贝.
// 对于VLD提示内容泄露，但是追踪不了函数调用堆栈的情况，请启用使用符号服务器自动下载。这可能引起调试程序的时候变慢。
// 确保你的调试工具（如 Visual Studio 或 WinDbg）配置了符号服务器。
// 符号服务器会自动下载缺失的符号文件，包括 dbghelp.pdb，并将其缓存到本地符号路径。
// 配置符号服务器（以 Visual Studio 为例）：在 Visual Studio 中，打开 调试 > 选项 > 符号。
// 勾选 Microsoft Symbol Servers. 指定符号缓存目录，例如 "C:\Symbols"。
// 调试时，缺失的符号（如 dbghelp.pdb）会自动下载到缓存目录。
#include "vld.h"

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // 某些 CString 构造函数将是显式的

// 关闭 MFC 对某些常见但经常可放心忽略的警告消息的隐藏
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC 核心组件和标准组件
#include <afxext.h>         // MFC 扩展

#include <afxdisp.h>        // MFC 自动化类

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // MFC 对 Internet Explorer 4 公共控件的支持
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // MFC 对 Windows 公共控件的支持
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxcontrolbars.h>     // 功能区和控件条的 MFC 支持


#define WM_USERTOONLINELIST    WM_USER + 3000
#define WM_OPENSCREENSPYDIALOG WM_USER + 3001
#define WM_OPENFILEMANAGERDIALOG      WM_USER + 3002
#define WM_OPENTALKDIALOG              WM_USER+3003
#define WM_OPENSHELLDIALOG             WM_USER+3004
#define WM_OPENSYSTEMDIALOG            WM_USER+3005
#define WM_OPENAUDIODIALOG             WM_USER+3006
#define WM_OPENSERVICESDIALOG          WM_USER+3007
#define WM_OPENREGISTERDIALOG          WM_USER+3008
#define WM_OPENWEBCAMDIALOG            WM_USER+3009

#define WM_USEROFFLINEMSG				WM_USER+3010
#define WM_HANDLEMESSAGE				WM_USER+3011
#define WM_OPENKEYBOARDDIALOG			WM_USER+3012

enum
{
	FILEMANAGER_DLG = 1,
	SCREENSPY_DLG,
	VIDEO_DLG,
	AUDIO_DLG,
	KEYBOARD_DLG,
	SYSTEM_DLG,
	SHELL_DLG,
	SERVICES_DLG,
	REGISTER_DLG,
	TALK_DLG,
	MONITOR_DLG
};


#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif


#include <assert.h>
#include <MMSystem.h>
#pragma comment(lib, "winmm.lib")

// 高精度的睡眠函数
#define Sleep_m(ms) { timeBeginPeriod(1); Sleep(ms); timeEndPeriod(1); }

// 以步长n毫秒在条件C下等待T秒(n是步长，必须能整除1000)
#define WAIT_n(C, T, n) {assert(!(1000%(n)));int s=(1000*(T))/(n);do{Sleep(n);}while((C)&&(--s));}

// 在条件C成立时等待T秒(步长10ms)
#define WAIT(C, T) { timeBeginPeriod(1); WAIT_n(C, T, 10); timeEndPeriod(1); }

// 在条件C成立时等待T秒(步长1ms)
#define WAIT_1(C, T) { timeBeginPeriod(1); WAIT_n(C, T, 1); timeEndPeriod(1); }

// 智能计时器，计算函数的耗时
class auto_tick
{
private:
	const char *func;
	int span;
	clock_t tick;
	__inline clock_t now() const { return clock(); }
	__inline int time() const { return now() - tick; }

public:
	auto_tick(const char *func_name, int th = 5) : func(func_name), span(th), tick(now()) { }
	~auto_tick() { stop(); }

	__inline void stop() {
		if (span != 0) { int s(this->time()); if (s > span)TRACE("[%s]执行时间: [%d]ms.\n", func, s); span = 0; }
	}
};

#ifdef _DEBUG
// 智能计算当前函数的耗时，超时会打印
#define AUTO_TICK(thresh) auto_tick TICK(__FUNCTION__, thresh)
#define STOP_TICK TICK.stop()
#else
#define AUTO_TICK(thresh) 
#define STOP_TICK 
#endif
