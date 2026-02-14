#pragma once
#include "common/commands.h"
#include "client/IOCPClient.h"
#include <dlfcn.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <cstring>
#include <stdexcept>

// Linux 端 BITMAPINFOHEADER 定义，与 Windows 完全一致
#pragma pack(push, 1)
struct BITMAPINFOHEADER_LNX {
    uint32_t biSize;            // 40
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;          // 1
    uint16_t biBitCount;        // 32
    uint32_t biCompression;     // 0 (BI_RGB)
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;   // 0
    int32_t  biYPelsPerMeter;   // 0
    uint32_t biClrUsed;         // 0
    uint32_t biClrImportant;    // 0
};
#pragma pack(pop)

// Linux 本地 MSG64 定义（与 Windows MSG64 内存布局完全一致）
// 用于解析服务端发来的鼠标/键盘控制命令
#pragma pack(push, 1)
struct MSG64_LNX {
    uint64_t hwnd;
    uint64_t message;
    uint64_t wParam;
    uint64_t lParam;
    uint64_t time;
    int32_t pt_x;
    int32_t pt_y;
};
#pragma pack(pop)

// X11 类型前向声明（避免 #include <X11/Xlib.h>）
typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;
typedef XID Pixmap;
typedef struct _XGC *GC;

struct XImage {
    int width, height;
    int xoffset;
    int format;
    char *data;
    int byte_order;
    int bitmap_unit;
    int bitmap_bit_order;
    int bitmap_pad;
    int depth;
    int bytes_per_line;
    int bits_per_pixel;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    // 后续字段省略，XDestroyImage 通过函数指针释放
    void *obdata;
    struct funcs {
        void *p[8]; // create_image, destroy_image, ...
    } f;
};

// X11 错误处理（防止 BadMatch 等错误导致进程退出）
static int x11_error_handler(Display* dpy, void* evt)
{
    // 忽略所有 X11 错误，由调用方检查返回值
    return 0;
}

// XCreateGC 需要的 XGCValues 结构体（布局与 Xlib 一致）
struct XGCValues_LNX {
    int function;
    unsigned long plane_mask;
    unsigned long foreground;
    unsigned long background;
    int line_width;
    int line_style;
    int cap_style;
    int join_style;
    int fill_style;
    int fill_rule;
    int arc_mode;
    unsigned long tile;     // Pixmap = XID
    unsigned long stipple;  // Pixmap = XID
    int ts_x_origin;
    int ts_y_origin;
    unsigned long font;     // Font = XID
    int subwindow_mode;
};

// X11 GC 常量
#define GCSubwindowMode (1L<<15)
#define IncludeInferiors 1

// ============== Windows 消息常量（用于解析服务端控制命令）==============
#define WM_MOUSEMOVE      0x0200
#define WM_LBUTTONDOWN    0x0201
#define WM_LBUTTONUP      0x0202
#define WM_LBUTTONDBLCLK  0x0203
#define WM_RBUTTONDOWN    0x0204
#define WM_RBUTTONUP      0x0205
#define WM_RBUTTONDBLCLK  0x0206
#define WM_MBUTTONDOWN    0x0207
#define WM_MBUTTONUP      0x0208
#define WM_MBUTTONDBLCLK  0x0209
#define WM_MOUSEWHEEL     0x020A

#define WM_KEYDOWN        0x0100
#define WM_KEYUP          0x0101
#define WM_SYSKEYDOWN     0x0104
#define WM_SYSKEYUP       0x0105

// Windows 滚轮增量宏
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)((wParam) >> 16))

// X11 Bool 常量
#ifndef True
#define True 1
#endif
#ifndef False
#define False 0
#endif

// X11 按钮常量
#define Button1 1  // 左键
#define Button2 2  // 中键
#define Button3 3  // 右键
#define Button4 4  // 滚轮上
#define Button5 5  // 滚轮下

// X11 KeySym 常量（常用键）
#define XK_BackSpace    0xff08
#define XK_Tab          0xff09
#define XK_Return       0xff0d
#define XK_Escape       0xff1b
#define XK_Delete       0xffff
#define XK_Home         0xff50
#define XK_Left         0xff51
#define XK_Up           0xff52
#define XK_Right        0xff53
#define XK_Down         0xff54
#define XK_Page_Up      0xff55
#define XK_Page_Down    0xff56
#define XK_End          0xff57
#define XK_Insert       0xff63
#define XK_Shift_L      0xffe1
#define XK_Shift_R      0xffe2
#define XK_Control_L    0xffe3
#define XK_Control_R    0xffe4
#define XK_Caps_Lock    0xffe5
#define XK_Alt_L        0xffe9
#define XK_Alt_R        0xffea
#define XK_Super_L      0xffeb  // Win键
#define XK_Super_R      0xffec
#define XK_F1           0xffbe
#define XK_F2           0xffbf
#define XK_F3           0xffc0
#define XK_F4           0xffc1
#define XK_F5           0xffc2
#define XK_F6           0xffc3
#define XK_F7           0xffc4
#define XK_F8           0xffc5
#define XK_F9           0xffc6
#define XK_F10          0xffc7
#define XK_F11          0xffc8
#define XK_F12          0xffc9
#define XK_Num_Lock     0xff7f
#define XK_Scroll_Lock  0xff14
#define XK_KP_0         0xffb0
#define XK_KP_1         0xffb1
#define XK_KP_2         0xffb2
#define XK_KP_3         0xffb3
#define XK_KP_4         0xffb4
#define XK_KP_5         0xffb5
#define XK_KP_6         0xffb6
#define XK_KP_7         0xffb7
#define XK_KP_8         0xffb8
#define XK_KP_9         0xffb9
#define XK_KP_Multiply  0xffaa
#define XK_KP_Add       0xffab
#define XK_KP_Subtract  0xffad
#define XK_KP_Decimal   0xffae
#define XK_KP_Divide    0xffaf
#define XK_KP_Enter     0xff8d
#define XK_Print        0xff61
#define XK_Pause        0xff13
#define XK_space        0x0020

// Windows VK 码到 X11 KeySym 的映射表
static unsigned long VKtoKeySym(unsigned int vk)
{
    // 字母键 A-Z (VK 0x41-0x5A)
    if (vk >= 0x41 && vk <= 0x5A)
        return vk + 0x20;  // 'a'-'z' 的 ASCII/KeySym

    // 数字键 0-9 (VK 0x30-0x39)
    if (vk >= 0x30 && vk <= 0x39)
        return vk;  // '0'-'9' 的 ASCII/KeySym

    // 小键盘数字 VK_NUMPAD0-9 (0x60-0x69)
    if (vk >= 0x60 && vk <= 0x69)
        return XK_KP_0 + (vk - 0x60);

    // F1-F12 (VK 0x70-0x7B)
    if (vk >= 0x70 && vk <= 0x7B)
        return XK_F1 + (vk - 0x70);

    // 特殊键映射
    switch (vk) {
    case 0x08:
        return XK_BackSpace;     // VK_BACK
    case 0x09:
        return XK_Tab;           // VK_TAB
    case 0x0D:
        return XK_Return;        // VK_RETURN
    case 0x10:
        return XK_Shift_L;       // VK_SHIFT
    case 0x11:
        return XK_Control_L;     // VK_CONTROL
    case 0x12:
        return XK_Alt_L;         // VK_MENU (Alt)
    case 0x13:
        return XK_Pause;         // VK_PAUSE
    case 0x14:
        return XK_Caps_Lock;     // VK_CAPITAL
    case 0x1B:
        return XK_Escape;        // VK_ESCAPE
    case 0x20:
        return XK_space;         // VK_SPACE
    case 0x21:
        return XK_Page_Up;       // VK_PRIOR
    case 0x22:
        return XK_Page_Down;     // VK_NEXT
    case 0x23:
        return XK_End;           // VK_END
    case 0x24:
        return XK_Home;          // VK_HOME
    case 0x25:
        return XK_Left;          // VK_LEFT
    case 0x26:
        return XK_Up;            // VK_UP
    case 0x27:
        return XK_Right;         // VK_RIGHT
    case 0x28:
        return XK_Down;          // VK_DOWN
    case 0x2C:
        return XK_Print;         // VK_SNAPSHOT
    case 0x2D:
        return XK_Insert;        // VK_INSERT
    case 0x2E:
        return XK_Delete;        // VK_DELETE
    case 0x5B:
        return XK_Super_L;       // VK_LWIN
    case 0x5C:
        return XK_Super_R;       // VK_RWIN
    case 0x6A:
        return XK_KP_Multiply;   // VK_MULTIPLY
    case 0x6B:
        return XK_KP_Add;        // VK_ADD
    case 0x6D:
        return XK_KP_Subtract;   // VK_SUBTRACT
    case 0x6E:
        return XK_KP_Decimal;    // VK_DECIMAL
    case 0x6F:
        return XK_KP_Divide;     // VK_DIVIDE
    case 0x90:
        return XK_Num_Lock;      // VK_NUMLOCK
    case 0x91:
        return XK_Scroll_Lock;   // VK_SCROLL
    case 0xA0:
        return XK_Shift_L;       // VK_LSHIFT
    case 0xA1:
        return XK_Shift_R;       // VK_RSHIFT
    case 0xA2:
        return XK_Control_L;     // VK_LCONTROL
    case 0xA3:
        return XK_Control_R;     // VK_RCONTROL
    case 0xA4:
        return XK_Alt_L;         // VK_LMENU
    case 0xA5:
        return XK_Alt_R;         // VK_RMENU
    // 符号键（美式键盘布局）
    case 0xBA:
        return 0x003b;           // VK_OEM_1 (;:)
    case 0xBB:
        return 0x003d;           // VK_OEM_PLUS (=+)
    case 0xBC:
        return 0x002c;           // VK_OEM_COMMA (,<)
    case 0xBD:
        return 0x002d;           // VK_OEM_MINUS (-_)
    case 0xBE:
        return 0x002e;           // VK_OEM_PERIOD (.>)
    case 0xBF:
        return 0x002f;           // VK_OEM_2 (/?)
    case 0xC0:
        return 0x0060;           // VK_OEM_3 (`~)
    case 0xDB:
        return 0x005b;           // VK_OEM_4 ([{)
    case 0xDC:
        return 0x005c;           // VK_OEM_5 (\|)
    case 0xDD:
        return 0x005d;           // VK_OEM_6 (]})
    case 0xDE:
        return 0x0027;           // VK_OEM_7 ('")
    default:
        return 0;                // 未知键
    }
}

// X11 函数指针类型
typedef Display* (*fn_XOpenDisplay)(const char*);
typedef int (*fn_XCloseDisplay)(Display*);
typedef XImage* (*fn_XGetImage)(Display*, Drawable, int, int, unsigned int, unsigned int, unsigned long, int);
typedef int (*fn_XDestroyImage)(XImage*);
typedef int (*fn_XSetErrorHandler)(int (*)(Display*, void*));
typedef Pixmap (*fn_XCreatePixmap)(Display*, Drawable, unsigned int, unsigned int, unsigned int);
typedef int (*fn_XFreePixmap)(Display*, Pixmap);
typedef GC (*fn_XCreateGC)(Display*, Drawable, unsigned long, void*);
typedef int (*fn_XFreeGC)(Display*, GC);
typedef int (*fn_XCopyArea)(Display*, Drawable, Drawable, GC, int, int, unsigned int, unsigned int, int, int);
typedef int (*fn_XDefaultDepth)(Display*, int);
typedef int (*fn_XSync)(Display*, int);
typedef unsigned long (*fn_XKeysymToKeycode)(Display*, unsigned long);
typedef int (*fn_XFlush)(Display*);
typedef int (*fn_XClearArea)(Display*, Window, int, int, unsigned int, unsigned int, int);

// XTest 扩展函数指针类型（用于模拟鼠标/键盘输入）
typedef int (*fn_XTestFakeMotionEvent)(Display*, int, int, int, unsigned long);
typedef int (*fn_XTestFakeButtonEvent)(Display*, unsigned int, int, unsigned long);
typedef int (*fn_XTestFakeKeyEvent)(Display*, unsigned int, int, unsigned long);

// X11 动态加载包装
class X11Loader
{
public:
    fn_XOpenDisplay   pXOpenDisplay;
    fn_XCloseDisplay  pXCloseDisplay;
    fn_XGetImage      pXGetImage;
    fn_XDestroyImage  pXDestroyImage;

    // Xlib 宏的替代：通过偏移读取 Display 内部结构
    // 这些函数通过 dlsym 获取
    typedef int (*fn_XDefaultScreen)(Display*);
    typedef int (*fn_XDisplayWidth)(Display*, int);
    typedef int (*fn_XDisplayHeight)(Display*, int);
    typedef Window (*fn_XRootWindow)(Display*, int);

    fn_XDefaultScreen  pXDefaultScreen;
    fn_XDisplayWidth   pXDisplayWidth;
    fn_XDisplayHeight  pXDisplayHeight;
    fn_XRootWindow     pXRootWindow;

    // Pixmap 相关（解决合成窗口管理器下 XGetImage BadMatch 问题）
    fn_XSetErrorHandler pXSetErrorHandler;
    fn_XCreatePixmap   pXCreatePixmap;
    fn_XFreePixmap     pXFreePixmap;
    fn_XCreateGC       pXCreateGC;
    fn_XFreeGC         pXFreeGC;
    fn_XCopyArea       pXCopyArea;
    fn_XDefaultDepth   pXDefaultDepth;
    fn_XSync           pXSync;
    fn_XKeysymToKeycode pXKeysymToKeycode;
    fn_XFlush          pXFlush;
    fn_XClearArea      pXClearArea;

    // XTest 扩展（用于模拟输入）
    fn_XTestFakeMotionEvent pXTestFakeMotionEvent;
    fn_XTestFakeButtonEvent pXTestFakeButtonEvent;
    fn_XTestFakeKeyEvent    pXTestFakeKeyEvent;

    X11Loader() : m_handle(nullptr), m_xtst_handle(nullptr)
    {
        pXOpenDisplay = nullptr;
        pXCloseDisplay = nullptr;
        pXGetImage = nullptr;
        pXDestroyImage = nullptr;
        pXDefaultScreen = nullptr;
        pXDisplayWidth = nullptr;
        pXDisplayHeight = nullptr;
        pXRootWindow = nullptr;
        pXSetErrorHandler = nullptr;
        pXCreatePixmap = nullptr;
        pXFreePixmap = nullptr;
        pXCreateGC = nullptr;
        pXFreeGC = nullptr;
        pXCopyArea = nullptr;
        pXDefaultDepth = nullptr;
        pXSync = nullptr;
        pXKeysymToKeycode = nullptr;
        pXFlush = nullptr;
        pXClearArea = nullptr;
        pXTestFakeMotionEvent = nullptr;
        pXTestFakeButtonEvent = nullptr;
        pXTestFakeKeyEvent = nullptr;
    }

    bool Load()
    {
        m_handle = dlopen("libX11.so.6", RTLD_LAZY);
        if (!m_handle) m_handle = dlopen("libX11.so", RTLD_LAZY);
        if (!m_handle) return false;

        pXOpenDisplay   = (fn_XOpenDisplay)dlsym(m_handle, "XOpenDisplay");
        pXCloseDisplay  = (fn_XCloseDisplay)dlsym(m_handle, "XCloseDisplay");
        pXGetImage      = (fn_XGetImage)dlsym(m_handle, "XGetImage");
        pXDestroyImage  = (fn_XDestroyImage)dlsym(m_handle, "XDestroyImage");
        pXDefaultScreen = (fn_XDefaultScreen)dlsym(m_handle, "XDefaultScreen");
        pXDisplayWidth  = (fn_XDisplayWidth)dlsym(m_handle, "XDisplayWidth");
        pXDisplayHeight = (fn_XDisplayHeight)dlsym(m_handle, "XDisplayHeight");
        pXRootWindow    = (fn_XRootWindow)dlsym(m_handle, "XRootWindow");

        // Pixmap 相关函数
        pXSetErrorHandler = (fn_XSetErrorHandler)dlsym(m_handle, "XSetErrorHandler");
        pXCreatePixmap  = (fn_XCreatePixmap)dlsym(m_handle, "XCreatePixmap");
        pXFreePixmap    = (fn_XFreePixmap)dlsym(m_handle, "XFreePixmap");
        pXCreateGC      = (fn_XCreateGC)dlsym(m_handle, "XCreateGC");
        pXFreeGC        = (fn_XFreeGC)dlsym(m_handle, "XFreeGC");
        pXCopyArea      = (fn_XCopyArea)dlsym(m_handle, "XCopyArea");
        pXDefaultDepth  = (fn_XDefaultDepth)dlsym(m_handle, "XDefaultDepth");
        pXSync          = (fn_XSync)dlsym(m_handle, "XSync");
        pXKeysymToKeycode = (fn_XKeysymToKeycode)dlsym(m_handle, "XKeysymToKeycode");
        pXFlush         = (fn_XFlush)dlsym(m_handle, "XFlush");
        pXClearArea     = (fn_XClearArea)dlsym(m_handle, "XClearArea");

        // 加载 XTest 扩展库（用于模拟鼠标/键盘输入）
        m_xtst_handle = dlopen("libXtst.so.6", RTLD_LAZY);
        if (!m_xtst_handle) m_xtst_handle = dlopen("libXtst.so", RTLD_LAZY);
        if (m_xtst_handle) {
            pXTestFakeMotionEvent = (fn_XTestFakeMotionEvent)dlsym(m_xtst_handle, "XTestFakeMotionEvent");
            pXTestFakeButtonEvent = (fn_XTestFakeButtonEvent)dlsym(m_xtst_handle, "XTestFakeButtonEvent");
            pXTestFakeKeyEvent    = (fn_XTestFakeKeyEvent)dlsym(m_xtst_handle, "XTestFakeKeyEvent");
        }

        // 基本 X11 函数必须全部存在；XTest 函数可选（没有时无法控制输入）
        return pXOpenDisplay && pXCloseDisplay && pXGetImage && pXDestroyImage &&
               pXDefaultScreen && pXDisplayWidth && pXDisplayHeight && pXRootWindow &&
               pXSetErrorHandler && pXCreatePixmap && pXFreePixmap &&
               pXCreateGC && pXFreeGC && pXCopyArea && pXDefaultDepth && pXSync &&
               pXKeysymToKeycode && pXFlush;
    }

    // 检查 XTest 扩展是否可用
    bool HasXTest() const
    {
        return pXTestFakeMotionEvent && pXTestFakeButtonEvent && pXTestFakeKeyEvent;
    }

    ~X11Loader()
    {
        if (m_xtst_handle) {
            dlclose(m_xtst_handle);
            m_xtst_handle = nullptr;
        }
        if (m_handle) {
            dlclose(m_handle);
            m_handle = nullptr;
        }
    }

private:
    void* m_handle;
    void* m_xtst_handle;
};

class ScreenHandler : public IOCPManager
{
public:
    ScreenHandler(IOCPClient* client)
        : m_client(client), m_running(false), m_destroyed(false), m_display(nullptr),
          m_inputDisplay(nullptr),
          m_width(0), m_height(0),
          m_pixmap(0), m_gc(nullptr), m_xtestWarned(false)
    {
        if (!client) {
            throw std::invalid_argument("IOCPClient pointer cannot be null");
        }

        // 动态加载 X11
        if (!m_x11.Load()) {
            throw std::runtime_error("Failed to load libX11.so (X11 not installed)");
        }

        // 打开 X11 Display（截屏专用）
        m_display = m_x11.pXOpenDisplay(nullptr);
        if (!m_display) {
            throw std::runtime_error("Failed to open X11 display (no desktop environment?)");
        }

        // 打开独立的 X11 Display（输入控制专用）
        // X11 Display 不是线程安全的，截屏线程和回调线程必须使用各自独立的连接
        if (m_x11.HasXTest()) {
            m_inputDisplay = m_x11.pXOpenDisplay(nullptr);
        }

        // 设置自定义错误处理器，防止 BadMatch 等错误导致进程退出
        m_x11.pXSetErrorHandler(x11_error_handler);

        // 获取默认屏幕信息
        int screen = m_x11.pXDefaultScreen(m_display);
        m_width = m_x11.pXDisplayWidth(m_display, screen);
        m_height = m_x11.pXDisplayHeight(m_display, screen);
        m_root = m_x11.pXRootWindow(m_display, screen);

        // 创建离屏 Pixmap 和 GC（解决合成窗口管理器下 XGetImage 的 BadMatch 问题）
        int depth = m_x11.pXDefaultDepth(m_display, screen);
        m_pixmap = m_x11.pXCreatePixmap(m_display, m_root, m_width, m_height, depth);

        // GC 必须设置 subwindow_mode = IncludeInferiors，
        // 否则 XCopyArea 会裁剪掉子窗口（应用程序窗口），只拷贝 root 背景
        XGCValues_LNX gcv;
        memset(&gcv, 0, sizeof(gcv));
        gcv.subwindow_mode = IncludeInferiors;
        m_gc = m_x11.pXCreateGC(m_display, m_root, GCSubwindowMode, &gcv);
        if (!m_pixmap || !m_gc) {
            throw std::runtime_error("Failed to create X11 Pixmap/GC for screen capture");
        }

        // 初始化 BITMAPINFOHEADER
        memset(&m_bmpHeader, 0, sizeof(m_bmpHeader));
        m_bmpHeader.biSize = sizeof(BITMAPINFOHEADER_LNX);
        m_bmpHeader.biWidth = m_width;
        m_bmpHeader.biHeight = m_height;
        m_bmpHeader.biPlanes = 1;
        m_bmpHeader.biBitCount = 32;
        m_bmpHeader.biCompression = 0; // BI_RGB
        m_bmpHeader.biSizeImage = m_width * m_height * 4;

        // 分配帧缓冲
        m_prevFrame.resize(m_bmpHeader.biSizeImage, 0);
        m_currFrame.resize(m_bmpHeader.biSizeImage, 0);

        // 差异帧缓冲: token(1) + algo(1) + cursorXY(8) + cursorType(1) + 最大差异数据
        m_diffBuffer.resize(1 + 1 + 8 + 1 + m_bmpHeader.biSizeImage * 2);
    }

    ~ScreenHandler()
    {
        // Mark as destroyed to prevent Start() from creating new thread
        m_destroyed = true;
        m_running = false;

        // Lock to ensure Start() is not in the middle of creating thread
        {
            std::lock_guard<std::mutex> lock(m_threadMutex);
            if (m_captureThread.joinable()) {
                m_captureThread.join();
            }
        }
        if (m_inputDisplay && m_x11.pXCloseDisplay) {
            m_x11.pXCloseDisplay(m_inputDisplay);
            m_inputDisplay = nullptr;
        }
        if (m_display) {
            if (m_gc && m_x11.pXFreeGC) m_x11.pXFreeGC(m_display, m_gc);
            if (m_pixmap && m_x11.pXFreePixmap) m_x11.pXFreePixmap(m_display, m_pixmap);
            m_pixmap = 0;
            m_gc = nullptr;
            // 强制全屏重绘，恢复 VMware SVGA 等虚拟显卡驱动的显示状态
            // XClearArea(display, window, x, y, w, h, exposures)
            // w=0, h=0 表示整个窗口；exposures=True 触发 Expose 事件强制所有窗口重绘
            if (m_x11.pXClearArea) {
                m_x11.pXClearArea(m_display, m_root, 0, 0, 0, 0, True);
            }
            m_x11.pXSync(m_display, 0);
            if (m_x11.pXCloseDisplay) m_x11.pXCloseDisplay(m_display);
            m_display = nullptr;
        }
        Mprintf(">>> ScreenHandler destroyed, display refreshed\n");
    }

    void Start()
    {
        // Prevent starting if destructor has begun
        if (m_destroyed) return;

        // Lock to prevent race with destructor
        std::lock_guard<std::mutex> lock(m_threadMutex);

        // Double-check after acquiring lock
        if (m_destroyed) return;

        // Prevent starting if thread is already running or joinable
        if (m_captureThread.joinable()) return;

        bool expected = false;
        if (!m_running.compare_exchange_strong(expected, true)) return;

        m_captureThread = std::thread(&ScreenHandler::CaptureLoop, this);
    }

    // 发送 BITMAPINFOHEADER + ScreenSettings（需在连接后、等待 COMMAND_NEXT 前调用）
    void SendBitmapInfo()
    {
        const uint32_t ulLength = 1 + sizeof(BITMAPINFOHEADER_LNX) + 2 * sizeof(uint64_t) + sizeof(ScreenSettings);
        std::vector<uint8_t> buf(ulLength, 0);
        buf[0] = TOKEN_BITMAPINFO;
        memcpy(&buf[1], &m_bmpHeader, sizeof(BITMAPINFOHEADER_LNX));
        uint64_t zero = 0;
        memcpy(&buf[1 + sizeof(BITMAPINFOHEADER_LNX)], &zero, sizeof(uint64_t));
        memcpy(&buf[1 + sizeof(BITMAPINFOHEADER_LNX) + sizeof(uint64_t)], &zero, sizeof(uint64_t));
        ScreenSettings settings = {};
        settings.MaxFPS = 10;
        memcpy(&buf[1 + sizeof(BITMAPINFOHEADER_LNX) + 2 * sizeof(uint64_t)], &settings, sizeof(ScreenSettings));
        m_client->Send2Server((char*)buf.data(), ulLength);
    }

    virtual VOID OnReceive(PBYTE data, ULONG size)
    {
        if (!size) return;

        switch (data[0]) {
        case COMMAND_NEXT:
            Start();
            break;

        case COMMAND_SCREEN_CONTROL:
            // 处理鼠标/键盘控制命令
            if (size >= 1 + sizeof(MSG64_LNX)) {
                HandleInputEvent((MSG64_LNX*)(data + 1));
            }
            break;
        }
    }

    // 处理来自服务端的鼠标/键盘输入事件
    // 使用独立的 m_inputDisplay，避免与截屏线程的 m_display 产生竞争
    void HandleInputEvent(const MSG64_LNX* msg)
    {
        if (!m_inputDisplay || !m_x11.HasXTest()) {
            if (!m_xtestWarned) {
                Mprintf("*** XTest not available, cannot handle input ***\n");
                m_xtestWarned = true;
            }
            return;
        }

        unsigned int message = (unsigned int)msg->message;
        // 从 lParam 提取坐标 (MAKELPARAM 格式: low 16 bits = x, high 16 bits = y)
        // 注意：不使用 pt_x/pt_y，因为 Windows MSG64 结构可能有填充导致偏移不一致
        int x = (int)(msg->lParam & 0xFFFF);
        int y = (int)((msg->lParam >> 16) & 0xFFFF);

        switch (message) {
        // ================== 鼠标事件 ==================
        case WM_MOUSEMOVE:
            m_x11.pXTestFakeMotionEvent(m_inputDisplay, -1, x, y, 0);
            m_x11.pXFlush(m_inputDisplay);
            break;

        case WM_LBUTTONDOWN:
            m_x11.pXTestFakeMotionEvent(m_inputDisplay, -1, x, y, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button1, True, 0);
            m_x11.pXFlush(m_inputDisplay);
            break;

        case WM_LBUTTONUP:
            m_x11.pXTestFakeMotionEvent(m_inputDisplay, -1, x, y, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button1, False, 0);
            m_x11.pXFlush(m_inputDisplay);
            break;

        case WM_LBUTTONDBLCLK:
            // 双击：快速按下释放两次
            m_x11.pXTestFakeMotionEvent(m_inputDisplay, -1, x, y, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button1, True, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button1, False, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button1, True, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button1, False, 0);
            m_x11.pXFlush(m_inputDisplay);
            break;

        case WM_RBUTTONDOWN:
            m_x11.pXTestFakeMotionEvent(m_inputDisplay, -1, x, y, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button3, True, 0);
            m_x11.pXFlush(m_inputDisplay);
            break;

        case WM_RBUTTONUP:
            m_x11.pXTestFakeMotionEvent(m_inputDisplay, -1, x, y, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button3, False, 0);
            m_x11.pXFlush(m_inputDisplay);
            break;

        case WM_RBUTTONDBLCLK:
            m_x11.pXTestFakeMotionEvent(m_inputDisplay, -1, x, y, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button3, True, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button3, False, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button3, True, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button3, False, 0);
            m_x11.pXFlush(m_inputDisplay);
            break;

        case WM_MBUTTONDOWN:
            m_x11.pXTestFakeMotionEvent(m_inputDisplay, -1, x, y, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button2, True, 0);
            m_x11.pXFlush(m_inputDisplay);
            break;

        case WM_MBUTTONUP:
            m_x11.pXTestFakeMotionEvent(m_inputDisplay, -1, x, y, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button2, False, 0);
            m_x11.pXFlush(m_inputDisplay);
            break;

        case WM_MBUTTONDBLCLK:
            m_x11.pXTestFakeMotionEvent(m_inputDisplay, -1, x, y, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button2, True, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button2, False, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button2, True, 0);
            m_x11.pXTestFakeButtonEvent(m_inputDisplay, Button2, False, 0);
            m_x11.pXFlush(m_inputDisplay);
            break;

        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(msg->wParam);
            // 滚轮：正值向上(Button4)，负值向下(Button5)
            unsigned int button = (delta > 0) ? Button4 : Button5;
            int clicks = abs(delta) / 120;  // 标准滚轮增量是120
            if (clicks < 1) clicks = 1;
            for (int i = 0; i < clicks; i++) {
                m_x11.pXTestFakeButtonEvent(m_inputDisplay, button, True, 0);
                m_x11.pXTestFakeButtonEvent(m_inputDisplay, button, False, 0);
            }
            m_x11.pXFlush(m_inputDisplay);
            break;
        }

        // ================== 键盘事件 ==================
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            unsigned int vk = (unsigned int)msg->wParam;
            unsigned long keysym = VKtoKeySym(vk);
            if (keysym) {
                unsigned int keycode = m_x11.pXKeysymToKeycode(m_inputDisplay, keysym);
                if (keycode) {
                    m_x11.pXTestFakeKeyEvent(m_inputDisplay, keycode, True, 0);
                    m_x11.pXFlush(m_inputDisplay);
                }
            }
            break;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP: {
            unsigned int vk = (unsigned int)msg->wParam;
            unsigned long keysym = VKtoKeySym(vk);
            if (keysym) {
                unsigned int keycode = m_x11.pXKeysymToKeycode(m_inputDisplay, keysym);
                if (keycode) {
                    m_x11.pXTestFakeKeyEvent(m_inputDisplay, keycode, False, 0);
                    m_x11.pXFlush(m_inputDisplay);
                }
            }
            break;
        }
        }
    }

private:
    IOCPClient* m_client;
    std::atomic<bool> m_running;
    std::atomic<bool> m_destroyed;  // Flag to prevent Start() after destruction begins
    std::mutex m_threadMutex;       // Protects m_captureThread lifecycle
    std::thread m_captureThread;
    bool m_xtestWarned;

    // X11 动态加载
    X11Loader m_x11;
    Display* m_display;        // 截屏线程专用
    Display* m_inputDisplay;   // 输入控制专用（OnReceive 回调线程）
    Window m_root;
    Pixmap m_pixmap;
    GC m_gc;
    int m_width;
    int m_height;

    // 协议
    BITMAPINFOHEADER_LNX m_bmpHeader;
    std::vector<uint8_t> m_prevFrame;
    std::vector<uint8_t> m_currFrame;
    std::vector<uint8_t> m_diffBuffer;

    // X11 截屏，输出 BGRA 格式（自底向上，与 BMP 一致）
    // 使用 XCopyArea 将 root window 拷贝到离屏 Pixmap，再对 Pixmap 调用 XGetImage
    // 这样可以避免合成窗口管理器（Mutter 等）导致的 BadMatch 错误
    bool CaptureScreen(std::vector<uint8_t>& buffer)
    {
        // 先将 root window 内容拷贝到离屏 Pixmap
        m_x11.pXCopyArea(m_display, m_root, m_pixmap, m_gc, 0, 0, m_width, m_height, 0, 0);
        m_x11.pXSync(m_display, 0); // 等待拷贝完成

        // AllPlanes = ~0UL, ZPixmap = 2
        XImage* img = m_x11.pXGetImage(m_display, m_pixmap, 0, 0, m_width, m_height, ~0UL, 2);
        if (!img) return false;

        // X11 ZPixmap 通常是 BGRA (bits_per_pixel=32)，但行序是自顶向下
        // BMP 期望自底向上，需要翻转
        int rowBytes = m_width * 4;
        for (int y = 0; y < m_height; y++) {
            int srcRow = y;
            int dstRow = m_height - 1 - y; // 翻转
            uint8_t* src = (uint8_t*)img->data + srcRow * img->bytes_per_line;
            uint8_t* dst = buffer.data() + dstRow * rowBytes;

            if (img->bits_per_pixel == 32) {
                // X11 通常是 BGRX，需要确保 alpha 通道
                for (int x = 0; x < m_width; x++) {
                    dst[x * 4 + 0] = src[x * 4 + 0]; // B
                    dst[x * 4 + 1] = src[x * 4 + 1]; // G
                    dst[x * 4 + 2] = src[x * 4 + 2]; // R
                    dst[x * 4 + 3] = 0xFF;            // A
                }
            } else {
                // 不支持非 32 位格式
                m_x11.pXDestroyImage(img);
                return false;
            }
        }

        m_x11.pXDestroyImage(img);
        return true;
    }

    // 发送第一帧完整截图
    void SendFirstScreen()
    {
        if (!CaptureScreen(m_currFrame)) return;

        uint32_t imgSize = m_bmpHeader.biSizeImage;
        std::vector<uint8_t> buf(1 + imgSize);
        buf[0] = TOKEN_FIRSTSCREEN;
        memcpy(&buf[1], m_currFrame.data(), imgSize);

        m_client->Send2Server((char*)buf.data(), buf.size());

        // 保存为上一帧
        m_prevFrame = m_currFrame;
    }

    // 计算差异并发送 TOKEN_NEXTSCREEN
    // 差异格式: 每个变化区域 = offset(4字节) + pixelCount(4字节) + BGRA 像素数据
    void SendDiffFrame()
    {
        if (!CaptureScreen(m_currFrame)) return;

        uint8_t* out = m_diffBuffer.data();
        out[0] = TOKEN_NEXTSCREEN;
        uint8_t* data = out + 1;

        // 写入算法类型
        uint8_t algo = 1; // ALGORITHM_DIFF (服务端定义为 1)
        memcpy(data, &algo, sizeof(uint8_t));

        // 写入光标位置 (Linux 端简单置 0)
        int32_t cursorX = 0, cursorY = 0;
        memcpy(data + 1, &cursorX, sizeof(int32_t));
        memcpy(data + 1 + sizeof(int32_t), &cursorY, sizeof(int32_t));

        // 写入光标类型
        uint8_t cursorType = 0;
        memcpy(data + 1 + 2 * sizeof(int32_t), &cursorType, sizeof(uint8_t));

        uint32_t headerSize = 1 + 2 * sizeof(int32_t) + 1; // algo + cursor + cursorType
        uint8_t* diffData = data + headerSize;
        uint32_t diffLen = CompareBitmap(m_currFrame.data(), m_prevFrame.data(),
                                         diffData, m_bmpHeader.biSizeImage);

        uint32_t totalLen = 1 + headerSize + diffLen;
        m_client->Send2Server((char*)out, totalLen);

        // 更新上一帧
        std::swap(m_prevFrame, m_currFrame);
    }

    // 简化版差异比较算法
    // 输出格式: [byteOffset(4) + byteCount(4) + pixel data] ...
    // 注意：offset 和 count 都是字节数，与服务端 memcpy(dst + offset, data, count) 对应
    uint32_t CompareBitmap(const uint8_t* curr, const uint8_t* prev,
                           uint8_t* outBuf, uint32_t totalBytes)
    {
        const uint32_t bytesPerPixel = 4;
        const uint32_t totalPixels = totalBytes / bytesPerPixel;
        const uint32_t gapThreshold = 8; // 8 像素间隙容忍

        uint32_t outOffset = 0;
        uint32_t i = 0;

        while (i < totalPixels) {
            // 跳过相同像素
            while (i < totalPixels &&
                   *(uint32_t*)(curr + i * 4) == *(uint32_t*)(prev + i * 4)) {
                i++;
            }
            if (i >= totalPixels) break;

            // 找到变化区域的起始
            uint32_t start = i;
            uint32_t lastDiff = i;

            // 扫描直到连续 gapThreshold 个像素相同
            while (i < totalPixels) {
                if (*(uint32_t*)(curr + i * 4) != *(uint32_t*)(prev + i * 4)) {
                    lastDiff = i;
                } else if (i - lastDiff > gapThreshold) {
                    break;
                }
                i++;
            }

            uint32_t end = lastDiff + 1;
            uint32_t count = end - start;
            uint32_t byteOffset = start * bytesPerPixel; // 字节偏移
            uint32_t byteCount = count * bytesPerPixel;  // 字节数

            // 写入 byteOffset + byteCount + pixel data
            memcpy(outBuf + outOffset, &byteOffset, sizeof(uint32_t));
            outOffset += sizeof(uint32_t);
            memcpy(outBuf + outOffset, &byteCount, sizeof(uint32_t));
            outOffset += sizeof(uint32_t);
            memcpy(outBuf + outOffset, curr + byteOffset, byteCount);
            outOffset += byteCount;
        }

        return outOffset;
    }


    // 截屏主循环
    void CaptureLoop()
    {
        try {
            Mprintf(">>> ScreenHandler CaptureLoop started (%dx%d)\n", m_width, m_height);

            // 发送第一帧
            SendFirstScreen();

            const int fps = 10;
            const int sleepMs = 1000 / fps;

            while (m_running) {
                clock_t start = clock();

                SendDiffFrame();

                int elapsed = (clock() - start) * 1000 / CLOCKS_PER_SEC;
                int wait = sleepMs - elapsed;
                if (wait > 0) {
                    usleep(wait * 1000);
                }
            }

            Mprintf(">>> ScreenHandler CaptureLoop stopped\n");
        } catch (const std::exception& e) {
            Mprintf("*** CaptureLoop exception: %s ***\n", e.what());
        } catch (...) {
            Mprintf("*** CaptureLoop unknown exception ***\n");
        }
    }
};
