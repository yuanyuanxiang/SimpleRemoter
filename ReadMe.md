# SimpleRemoter

**[简体中文](./ReadMe.md) | [繁體中文](./ReadMe_TW.md) | [English](./ReadMe_EN.md)**

<p align="center">
  <a href="https://github.com/yuanyuanxiang/SimpleRemoter/stargazers">
    <img src="https://img.shields.io/github/stars/yuanyuanxiang/SimpleRemoter?style=flat-square&logo=github" alt="GitHub Stars">
  </a>
  <a href="https://github.com/yuanyuanxiang/SimpleRemoter/network/members">
    <img src="https://img.shields.io/github/forks/yuanyuanxiang/SimpleRemoter?style=flat-square&logo=github" alt="GitHub Forks">
  </a>
  <a href="https://github.com/yuanyuanxiang/SimpleRemoter/releases">
    <img src="https://img.shields.io/github/v/release/yuanyuanxiang/SimpleRemoter?style=flat-square" alt="GitHub Release">
  </a>
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/language-C%2B%2B17-orange?style=flat-square&logo=cplusplus" alt="Language">
  <img src="https://img.shields.io/badge/IDE-VS2019%2B-purple?style=flat-square&logo=visualstudio" alt="IDE">
  <img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" alt="License">
</p>

<p align="center">
  <a href="https://github.com/yuanyuanxiang/SimpleRemoter/releases/latest">
    <img src="https://img.shields.io/badge/Download-最新版本-2ea44f?style=for-the-badge&logo=github" alt="Download Latest">
  </a>
</p>

---

> [!WARNING]
> **重要法律声明**
>
> 本软件**仅供教育目的及授权使用场景**，包括：
> - 在您的组织内进行远程 IT 管理
> - 经授权的渗透测试和安全研究
> - 个人设备管理和技术学习
>
> **未经授权访问计算机系统属违法行为。** 使用者须对遵守所有适用法律承担全部责任。开发者对任何滥用行为概不负责。

---

## 目录

- [项目简介](#项目简介)
- [免责声明](#免责声明)
- [功能特性](#功能特性)
- [技术亮点](#技术亮点)
- [系统架构](#系统架构)
- [快速开始](#快速开始)
- [客户端支持](#客户端支持)
- [更新日志](#更新日志)
- [相关项目](#相关项目)
- [联系方式](#联系方式)

---

## 项目简介

**SimpleRemoter** 是一个功能完整的远程控制解决方案，基于经典的 Gh0st 框架重构，采用现代 C++17 开发。项目始于 2019 年，经过持续迭代已发展为支持 **Windows + Linux** 双平台的企业级远程管理工具。

### 核心能力

| 类别 | 功能 |
|------|------|
| **远程桌面** | 实时屏幕控制、多显示器支持、H.264 编码、自适应质量 |
| **文件管理** | 双向传输、断点续传、C2C 传输、SHA-256 校验 |
| **终端管理** | 交互式 Shell、ConPTY/PTY 支持、现代 Web 终端 |
| **系统管理** | 进程/服务/窗口管理、注册表浏览、会话控制 |
| **媒体采集** | 摄像头监控、音频监听、键盘记录 |
| **网络功能** | SOCKS 代理、FRP 穿透、端口映射 |

### 适用场景

- **企业 IT 运维**：批量管理内网设备，远程故障排查
- **远程办公**：安全访问办公电脑，文件同步传输
- **安全研究**：渗透测试、红队演练、安全审计
- **技术学习**：网络编程、IOCP 模型、加密传输实践

**原始来源：** [zibility/Remote](https://github.com/zibility/Remote) | **起始日期：** 2019.1.1

[![Star History Chart](https://api.star-history.com/svg?repos=yuanyuanxiang/SimpleRemoter&type=Date)](https://star-history.com/#yuanyuanxiang/SimpleRemoter&Date)

---

## 免责声明

**请在使用本软件前仔细阅读以下声明：**

1. **合法用途**：本项目仅供合法的技术研究、学习交流和授权的远程管理使用。严禁将本软件用于未经授权访问他人计算机系统、窃取数据、监控他人隐私等任何违法行为。

2. **使用者责任**：使用者必须遵守所在国家/地区的法律法规。因使用本软件而产生的任何法律责任，由使用者自行承担。

3. **无担保声明**：本软件按"现状"提供，不附带任何明示或暗示的担保，包括但不限于适销性、特定用途适用性的担保。

4. **免责条款**：开发者不对因使用、误用或无法使用本软件而造成的任何直接、间接、偶然、特殊或后果性损害承担责任。

5. **版权声明**：本项目采用 MIT 协议开源，允许自由使用、修改和分发，但必须保留原始版权声明。

**继续使用本软件即表示您已阅读、理解并同意上述所有条款。**

---

## 功能特性

### 远程桌面

![远程桌面](./images/Remote.jpg)

- **多种截图方式**：GDI（兼容性强）、DXGI（高性能）、虚拟桌面（后台运行）
- **智能压缩算法**：
  - DIFF 差分算法 - SSE2 优化，仅传输变化区域
  - RGB565 算法 - 节省 50% 带宽
  - H.264 编码 - 视频级压缩，适合高帧率场景
  - 灰度模式 - 极低带宽消耗
- **自适应质量**：根据网络 RTT 自动调整帧率（5-30 FPS）、分辨率和压缩算法
- **多显示器**：支持多屏切换和多屏上墙显示
- **隐私屏幕**：被控端屏幕可隐藏，支持锁屏状态下控制
- **文件拖拽**：Ctrl+C/V 跨设备复制粘贴文件

### 文件管理

![文件管理](./images/FileManage.jpg)

- **V2 传输协议**：全新设计，支持大文件（>4GB）
- **断点续传**：网络中断后自动恢复，状态持久化
- **C2C 传输**：客户端之间直接传输，无需经过主控
- **完整性校验**：SHA-256 哈希验证，确保文件完整
- **批量操作**：支持文件搜索、压缩、批量传输

### 终端管理

![终端管理](./images/Console.jpg)

- **交互式 Shell**：完整的命令行体验，支持 Tab 补全
- **ConPTY 技术**：Windows 10+ 原生伪终端支持
- **现代 Web 终端**：基于 WebView2 + xterm.js（v1.2.7+）
- **终端尺寸调整**：自适应窗口大小

### 进程与窗口管理

| 进程管理 | 窗口管理 |
|---------|---------|
| ![进程](./images/Process.jpg) | ![窗口](./images/Window.jpg) |

- **进程管理**：查看进程列表、CPU/内存占用、启动/终止进程
- **代码注入**：向目标进程注入 DLL（需管理员权限）
- **窗口控制**：最大化/最小化/隐藏/关闭窗口

### 媒体功能

| 视频管理 | 语音管理 |
|---------|---------|
| ![视频](./images/Video.jpg) | ![语音](./images/Voice.jpg) |

- **摄像头监控**：实时视频流，支持分辨率调整
- **音频监听**：远程声音采集，支持双向语音
- **键盘记录**：在线/离线记录模式

### 其他功能

- **服务管理**：查看和控制 Windows 服务
- **注册表浏览**：只读方式浏览注册表内容
- **会话控制**：远程注销/关机/重启
- **SOCKS 代理**：通过客户端建立代理隧道
- **FRP 穿透**：内置 FRP 支持，轻松穿透内网
- **代码执行**：远程执行 DLL，支持热更新

---

## 技术亮点

### 高性能网络架构

```
┌─────────────────────────────────────────────────────────┐
│                    IOCP 通信模型                          │
├─────────────────────────────────────────────────────────┤
│  • I/O 完成端口：Windows 最高效的异步 I/O 模型            │
│  • 单主控支持 10,000+ 并发连接                            │
│  • 支持 TCP / UDP / KCP 三种传输协议                      │
│  • 自动分块处理大数据包（最大 128KB 发送缓冲）             │
└─────────────────────────────────────────────────────────┘
```

### 自适应质量控制

基于 RTT（Round-Trip Time）的智能质量调整系统：

| RTT 延迟 | 质量等级 | 帧率 | 分辨率 | 压缩算法 | 适用场景 |
|---------|---------|------|--------|---------|---------|
| < 30ms | Ultra | 25 FPS | 原始 | DIFF | 局域网办公 |
| 30-80ms | High | 20 FPS | 原始 | RGB565 | 一般办公 |
| 80-150ms | Good | 20 FPS | ≤1080p | H.264 | 跨网/视频 |
| 150-250ms | Medium | 15 FPS | ≤900p | H.264 | 跨网办公 |
| 250-400ms | Low | 12 FPS | ≤720p | H.264 | 较差网络 |
| > 400ms | Minimal | 8 FPS | ≤540p | H.264 | 极差网络 |

- **零额外开销**：复用心跳包计算 RTT
- **快速降级**：2 次检测即触发，响应网络波动
- **谨慎升级**：5 次稳定后才提升质量
- **冷却机制**：防止频繁切换

### V2 文件传输协议

```cpp
// 77 字节协议头 + 文件名 + 数据载荷
struct FileChunkPacketV2 {
    uint8_t   cmd;            // COMMAND_SEND_FILE_V2 = 85
    uint64_t  transferID;     // 传输会话 ID
    uint64_t  srcClientID;    // 源客户端 ID (0=主控端)
    uint64_t  dstClientID;    // 目标客户端 ID (0=主控端, C2C)
    uint32_t  fileIndex;      // 文件编号 (0-based)
    uint32_t  totalFiles;     // 总文件数
    uint64_t  fileSize;       // 文件大小（支持 >4GB）
    uint64_t  offset;         // 当前块偏移
    uint64_t  dataLength;     // 本块数据长度
    uint64_t  nameLength;     // 文件名长度
    uint16_t  flags;          // 标志位 (FFV2_LAST_CHUNK 等)
    uint16_t  checksum;       // CRC16 校验（可选）
    uint8_t   reserved[8];    // 预留扩展
    // char filename[nameLength];  // UTF-8 相对路径
    // uint8_t data[dataLength];   // 文件数据
};
```

**特性**：
- 大文件支持（uint64_t 突破 4GB 限制）
- 断点续传（状态持久化到 `%TEMP%\FileTransfer\`）
- SHA-256 完整性校验
- C2C 直传（客户端到客户端）
- V1/V2 协议兼容

### 屏幕传输优化

- **SSE2 指令集**：像素差分计算硬件加速
- **多线程并行**：线程池分块处理屏幕数据
- **滚动检测**：识别滚动场景，减少 50-80% 带宽
- **H.264 编码**：基于 x264，GOP 控制，视频级压缩

### 安全机制

| 层级 | 措施 |
|------|------|
| **传输加密** | AES-256 数据加密，可配置 IV |
| **身份验证** | 签名验证 + HMAC 认证 |
| **授权控制** | 序列号绑定（IP/域名），多级授权 |
| **文件校验** | SHA-256 完整性验证 |
| **会话隔离** | Session 0 独立处理 |

### 依赖库

| 库 | 版本 | 用途 |
|----|------|------|
| zlib | 1.3.1 | 通用压缩 |
| zstd | 1.5.7 | 高速压缩 |
| x264 | 0.164 | H.264 编码 |
| libyuv | 190 | YUV 转换 |
| HPSocket | 6.0.3 | 网络 I/O |
| jsoncpp | 1.9.6 | JSON 解析 |

---

## 系统架构

![架构图](https://github.com/yuanyuanxiang/SimpleRemoter/wiki/res/Architecture.jpg)

### 两层控制架构（v1.1.1+）

```
超级用户
    │
    ├── Master 1 ──> 客户端群组 A（最多 10,000+）
    ├── Master 2 ──> 客户端群组 B
    └── Master 3 ──> 客户端群组 C
```

**设计优势**：
- **层级控制**：超级用户可管理任意主控程序
- **隔离机制**：不同主控管理的客户端相互隔离
- **水平扩展**：10 个 Master × 10,000 客户端 = 100,000 设备

### 主控程序（Server）

主控程序 **YAMA.exe** 提供图形化管理界面：

![主界面](./images/Yama.jpg)

- 基于 IOCP 的高性能服务端
- 客户端分组管理
- 实时状态监控（RTT、地理位置、活动窗口）
- 一键生成客户端

### 受控程序（Client）

![客户端生成](./images/TestRun.jpg)

**运行形式**：

| 类型 | 说明 |
|------|------|
| `ghost.exe` | 独立可执行文件，无外部依赖 |
| `TestRun.exe` + `ServerDll.dll` | 分离加载，支持内存加载 DLL |
| Windows 服务 | 后台运行，支持锁屏控制 |
| Linux 客户端 | 跨平台支持（v1.2.5+） |

---

## 快速开始

### 5 分钟快速体验

无需编译，下载即用：

1. **下载发布版** - 从 [Releases](https://github.com/yuanyuanxiang/SimpleRemoter/releases/latest) 下载最新版本
2. **启动主控** - 运行 `YAMA.exe`，输入授权信息（见下方试用口令）
3. **生成客户端** - 点击工具栏「生成」按钮，配置服务器 IP 和端口
4. **部署客户端** - 将生成的客户端复制到目标机器并运行
5. **开始控制** - 客户端上线后，双击即可打开远程桌面

> [!TIP]
> 首次测试建议在同一台机器上运行主控和客户端，使用 `127.0.0.1` 作为服务器地址。

### 编译要求

- **操作系统**：Windows 10/11 或 Windows Server 2016+
- **开发环境**：Visual Studio 2019 / 2022 / 2026
- **SDK**：Windows 10 SDK (10.0.19041.0+)

### 编译步骤

```bash
# 1. 克隆代码（必须使用 git clone，不要下载 zip）
git clone https://github.com/yuanyuanxiang/SimpleRemoter.git

# 2. 打开解决方案
#    使用 VS2019+ 打开 SimpleRemoter.sln

# 3. 选择配置
#    Release | x86 或 Release | x64

# 4. 编译
#    生成 -> 生成解决方案
```

**常见问题**：
- 依赖库冲突：[#269](https://github.com/yuanyuanxiang/SimpleRemoter/issues/269)
- 非中文系统乱码：[#157](https://github.com/yuanyuanxiang/SimpleRemoter/issues/157)
- 编译器兼容性：[#171](https://github.com/yuanyuanxiang/SimpleRemoter/issues/171)

### 部署方式

#### 内网部署

主控与客户端在同一局域网，客户端直连主控 IP:Port。

#### 外网部署（FRP 穿透）

```
客户端 ──> VPS (FRP Server) ──> 本地主控 (FRP Client)
```

详细配置请参考：[反向代理部署说明](./反向代理.md)

### 授权说明

自 v1.2.4 起提供试用口令（2 年有效期，20 并发连接，仅限内网）：

```
授权方式：按计算机 IP 绑定
主控 IP：127.0.0.1
序列号：12ca-17b4-9af2-2894
密码：20260201-20280201-0020-be94-120d-20f9-919a
验证码：6015188620429852704
有效期：2026-02-01 至 2028-02-01
```

> [!NOTE]
> **多层授权方案**
>
> SimpleRemoter 采用企业级多层授权架构，支持代理商/开发者独立运营：
> - **离线验证**：第一层用户获得授权后可完全离线使用
> - **独立控制**：您的下级用户只连接到您的服务器，数据完全由您掌控
> - **自由定制**：支持二次开发，打造您的专属版本
>
> 📖 **[查看完整授权方案说明](./docs/MultiLayerLicense.md)**

---

## 客户端支持

### Windows 客户端

**系统要求**：Windows 7 SP1 及以上

**功能完整性**：✅ 全部功能支持

### Linux 客户端（v1.2.5+）

**系统要求**：
- 显示服务器：X11/Xorg（暂不支持 Wayland）
- 必需库：libX11
- 推荐库：libXtst（XTest 扩展）、libXss（空闲检测）

**功能支持**：

| 功能 | 状态 | 实现 |
|------|------|------|
| 远程桌面 | ✅ | X11 屏幕捕获，鼠标/键盘控制 |
| 远程终端 | ✅ | PTY 交互式 Shell |
| 文件管理 | ✅ | 双向传输，大文件支持 |
| 进程管理 | ✅ | 进程列表、终止进程 |
| 心跳/RTT | ✅ | RFC 6298 RTT 估算 |
| 守护进程 | ✅ | 双 fork 守护化 |
| 剪贴板 | ⏳ | 开发中 |
| 会话管理 | ⏳ | 开发中 |

**编译方式**：

```bash
cd linux
cmake .
make
```

---

## 更新日志

### v1.2.8 (2026.3.11)

**邮件通知 & 远程音频**

- 主机上线邮件通知（SMTP 配置、关键词匹配、右键快捷添加）
- 远程音频播放（WASAPI Loopback）+ Opus 压缩（24:1）
- 多 FRPS 服务器同时连接支持
- 自定义光标显示和追踪
- V2 授权协议（ECDSA 签名）
- 修复非中文 Windows 系统乱码问题
- Linux 客户端屏幕压缩算法优化

### v1.2.7 (2026.2.28)

**V2 文件传输协议**

- 支持 C2C（客户端到客户端）直接传输
- 断点续传和大文件支持（>4GB）
- SHA-256 文件完整性校验
- WebView2 + xterm.js 现代终端
- Linux 文件管理支持
- 主机列表批量更新优化，减少 UI 闪烁

### v1.2.6 (2026.2.16)

**远程桌面工具栏重写**

- 状态窗口显示 RTT、帧率、分辨率
- 全屏工具栏支持 4 个位置和多显示器
- H.264 带宽优化
- 授权管理 UI 完善

### v1.2.5 (2026.2.11)

**自适应质量控制 & Linux 客户端**

- 基于 RTT 的智能质量调整
- RGB565 算法（节省 50% 带宽）
- 滚动检测优化（节省 50-80% 带宽）
- Linux 客户端初版发布

完整更新历史请查看：[history.md](./history.md)

---

## 相关项目

- [HoldingHands](https://github.com/yuanyuanxiang/HoldingHands) - 全英文界面远程控制
- [BGW RAT](https://github.com/yuanyuanxiang/BGW_RAT) - 大灰狼 9.5
- [Gh0st](https://github.com/yuanyuanxiang/Gh0st) - 经典 Gh0st 实现

---

## 联系方式

| 渠道 | 链接 |
|------|------|
| **QQ** | 962914132 |
| **Telegram** | [@doge_grandfather](https://t.me/doge_grandfather) |
| **Email** | [yuanyuanxiang163@gmail.com](mailto:yuanyuanxiang163@gmail.com) |
| **LinkedIn** | [wishyuanqi](https://www.linkedin.com/in/wishyuanqi) |
| **Issues** | [问题反馈](https://github.com/yuanyuanxiang/SimpleRemoter/issues) |
| **PR** | [贡献代码](https://github.com/yuanyuanxiang/SimpleRemoter/pulls) |

### 赞助支持

本项目源于技术学习与兴趣爱好，作者将根据业余时间不定期更新。如果本项目对您有所帮助，欢迎赞助支持：

[![Sponsor](https://img.shields.io/badge/Sponsor-Support%20This%20Project-ff69b4?style=for-the-badge)](https://github.com/yuanyuanxiang/yuanyuanxiang/blob/main/images/QR_Codes.jpg)

---

<p align="center">
  <sub>如果您喜欢这个项目，请给它一个 ⭐ Star！</sub>
</p>
