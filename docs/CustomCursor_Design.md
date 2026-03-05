# 自定义光标支持设计方案

**状态：已实现** ✓

## 概述

扩展远程桌面的光标支持，使其能够显示应用程序自定义光标和动画光标，而不仅仅是 16 种标准系统光标。

## 现有实现

### 数据流

```
客户端 (ScreenCapture.h):
  getCurrentCursorIndex() → 0-15 或 -1
  帧数据: [算法(1)] [光标位置(8)] [光标索引(1)] [图像数据...]

服务端 (ScreenSpyDlg.cpp):
  cursorIndex == -1 → 回退到标准箭头
  cursorIndex 0-15  → 使用 m_CursorInfo.getCursorHandle(index)
```

### 限制

- 仅支持 16 种标准系统光标 (IDC_ARROW, IDC_HAND, IDC_IBEAM 等)
- 自定义光标显示为箭头
- 动画光标不支持

## 设计方案

### 方案概述

采用**独立命令传输 + 哈希缓存 + 节流机制**：

1. 新增 `CMD_CURSOR_IMAGE` 命令传输光标位图
2. 使用位图哈希避免重复传输
3. 节流机制防止动画光标过载
4. 光标索引 `-2` 表示使用已缓存的自定义光标

### 光标索引定义

| 索引值 | 含义 |
|-------|------|
| 0-15 | 标准系统光标（现有） |
| -1 (255) | 不支持，显示箭头（现有，向后兼容） |
| -2 (254) | 使用缓存的自定义光标（新增） |

### CMD_CURSOR_IMAGE 数据格式

```
字节偏移  大小    字段
────────────────────────────
0         1      CMD_CURSOR_IMAGE (命令字节)
1         4      hash (位图哈希，用于去重)
5         2      hotspotX (热点X坐标)
7         2      hotspotY (热点Y坐标)
9         1      width (光标宽度，最大255)
10        1      height (光标高度，最大255)
11        N      BGRA位图数据 (N = width * height * 4)

典型大小: 32x32光标 = 11 + 4096 = 4107 字节
```

### 客户端逻辑 (ScreenCapture.h / ScreenManager.cpp)

```cpp
// 新增状态变量
DWORD m_lastCursorHash = 0;       // 上次发送的光标哈希
DWORD m_lastCursorSendTime = 0;   // 上次发送时间
const DWORD CURSOR_THROTTLE = 50; // 最小发送间隔 (ms)

// 每帧处理逻辑
int cursorIndex = getCurrentCursorIndex();
if (cursorIndex == -1) {
    // 非标准光标，获取位图
    CursorBitmapInfo info;
    if (GetCurrentCursorBitmap(&info)) {
        DWORD hash = CalculateBitmapHash(info);
        DWORD now = GetTickCount();

        // 哈希变化且超过节流时间 → 发送光标图像
        if (hash != m_lastCursorHash &&
            (now - m_lastCursorSendTime) > CURSOR_THROTTLE) {
            SendCursorImage(info, hash);
            m_lastCursorHash = hash;
            m_lastCursorSendTime = now;
        }
        cursorIndex = -2;  // 使用自定义光标
    }
}
// 帧数据中写入 cursorIndex
```

### 服务端逻辑 (ScreenSpyDlg.cpp)

```cpp
// 新增成员变量
HCURSOR m_hCustomCursor = NULL;   // 缓存的自定义光标
DWORD m_customCursorHash = 0;     // 当前光标哈希

// 处理 CMD_CURSOR_IMAGE
case CMD_CURSOR_IMAGE: {
    DWORD hash = *(DWORD*)(buffer + 1);
    if (hash != m_customCursorHash) {
        WORD hotX = *(WORD*)(buffer + 5);
        WORD hotY = *(WORD*)(buffer + 7);
        BYTE width = buffer[9];
        BYTE height = buffer[10];
        LPBYTE bitmapData = buffer + 11;

        // 销毁旧光标
        if (m_hCustomCursor) {
            DestroyCursor(m_hCustomCursor);
        }

        // 创建新光标
        m_hCustomCursor = CreateCursorFromBitmap(
            hotX, hotY, width, height, bitmapData);
        m_customCursorHash = hash;
    }
    break;
}

// 处理帧数据中的光标索引
BYTE cursorIndex = buffer[2 + sizeof(POINT)];
HCURSOR cursor;
if (cursorIndex == 254) {  // -2
    cursor = m_hCustomCursor ? m_hCustomCursor : LoadCursor(NULL, IDC_ARROW);
} else if (cursorIndex == 255) {  // -1
    cursor = LoadCursor(NULL, IDC_ARROW);
} else {
    cursor = m_CursorInfo.getCursorHandle(cursorIndex);
}
```

### 获取光标位图实现

```cpp
struct CursorBitmapInfo {
    WORD hotspotX;
    WORD hotspotY;
    BYTE width;
    BYTE height;
    std::vector<BYTE> bgraData;  // width * height * 4
};

bool GetCurrentCursorBitmap(CursorBitmapInfo* info) {
    CURSORINFO ci = { sizeof(CURSORINFO) };
    if (!GetCursorInfo(&ci) || ci.flags != CURSOR_SHOWING)
        return false;

    ICONINFO iconInfo;
    if (!GetIconInfo(ci.hCursor, &iconInfo))
        return false;

    info->hotspotX = (WORD)iconInfo.xHotspot;
    info->hotspotY = (WORD)iconInfo.yHotspot;

    BITMAP bm;
    GetObject(iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask,
              sizeof(BITMAP), &bm);

    info->width = (BYTE)min(bm.bmWidth, 255);
    info->height = (BYTE)min(bm.bmHeight, 255);

    // 获取 BGRA 位图数据
    // ... (使用 GetDIBits)

    // 清理
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);

    return true;
}
```

### 哈希计算

```cpp
DWORD CalculateBitmapHash(const CursorBitmapInfo& info) {
    // 使用 FNV-1a 或简单的累加哈希
    DWORD hash = 2166136261;  // FNV offset basis
    const BYTE* data = info.bgraData.data();
    size_t len = info.bgraData.size();

    for (size_t i = 0; i < len; i += 16) {  // 每16字节采样一次，加速
        hash ^= data[i];
        hash *= 16777619;  // FNV prime
    }
    return hash;
}
```

## 数据流图

```
┌─────────────────────────────────────────────────────────────────┐
│                          客户端                                  │
├─────────────────────────────────────────────────────────────────┤
│  getCurrentCursorIndex()                                        │
│    │                                                            │
│    ├─ 0-15 ──→ 帧数据: cursorIndex = 0-15                       │
│    │                                                            │
│    └─ -1 ──→ GetCurrentCursorBitmap()                          │
│               │                                                 │
│               ├─ 计算哈希                                        │
│               │                                                 │
│               ├─ 哈希变化 && 超过节流?                           │
│               │   │                                             │
│               │   ├─ Yes ──→ 发送 CMD_CURSOR_IMAGE              │
│               │   │          更新 lastHash, lastTime            │
│               │   │                                             │
│               │   └─ No ──→ 跳过发送                            │
│               │                                                 │
│               └─ 帧数据: cursorIndex = -2                       │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                          服务端                                  │
├─────────────────────────────────────────────────────────────────┤
│  收到 CMD_CURSOR_IMAGE:                                         │
│    └─ hash != m_customCursorHash?                               │
│        ├─ Yes ──→ CreateCursorFromBitmap() → m_hCustomCursor    │
│        └─ No ──→ 忽略 (已缓存)                                   │
│                                                                 │
│  收到帧数据:                                                     │
│    └─ cursorIndex:                                              │
│        ├─ 0-15 ──→ m_CursorInfo.getCursorHandle(index)          │
│        ├─ -1 ──→ LoadCursor(IDC_ARROW) (向后兼容)               │
│        └─ -2 ──→ m_hCustomCursor                                │
└─────────────────────────────────────────────────────────────────┘
```

## 动画光标支持

本方案自然支持动画光标：

| 机制 | 说明 |
|-----|------|
| **自动检测** | 动画每帧位图哈希不同，自动触发传输 |
| **节流保护** | 50ms 节流 ≈ 最高 20fps，防止过载 |
| **带宽控制** | 32x32 动画 @ 10fps ≈ 40 KB/s，可接受 |

## 修改文件清单

| 文件 | 修改内容 |
|-----|---------|
| `common/commands.h` | 新增 `CMD_CURSOR_IMAGE = 75` |
| `client/CursorInfo.h` | 新增 `GetCurrentCursorBitmap()`, `CalculateBitmapHash()` |
| `client/ScreenCapture.h` | 发送逻辑：检测、节流、发送 CMD_CURSOR_IMAGE |
| `client/ScreenManager.cpp` | 处理 CMD_CURSOR_IMAGE（如果服务端发给客户端需要） |
| `server/2015Remote/ScreenSpyDlg.h` | 新增 `m_hCustomCursor`, `m_customCursorHash` |
| `server/2015Remote/ScreenSpyDlg.cpp` | 处理 CMD_CURSOR_IMAGE，更新光标显示逻辑 |

## 向后兼容性

- 旧客户端：继续发送 `-1`，服务端显示箭头（现有行为）
- 新客户端 + 旧服务端：发送 `-2` 和 CMD_CURSOR_IMAGE，旧服务端忽略新命令，`-2` 当作 `-1` 处理
- 新客户端 + 新服务端：完整自定义光标支持

## 可调参数

```cpp
// client/ScreenCapture.h
const DWORD CURSOR_THROTTLE = 50;   // 节流间隔 (ms)
// 50ms  → 最高 20fps，动画流畅
// 100ms → 最高 10fps，省带宽

const BYTE MAX_CURSOR_SIZE = 64;    // 最大光标尺寸
// 超过此尺寸的光标将被缩放或回退到标准光标
```

## 测试用例

1. **标准光标** - 箭头、手形、I形等，应使用索引 0-15
2. **自定义静态光标** - 应用程序自定义光标，只传输一次
3. **动画光标** - Windows 忙碌动画，应持续更新（受节流限制）
4. **快速切换** - 频繁切换光标，节流应生效
5. **大尺寸光标** - 超过 64x64 的光标，应正确处理或回退
