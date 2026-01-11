# 🌐 语言 | Language

**[🇨🇳 中文](./ReadMe.md) | [🇺🇸 English](./ReadMe_EN.md)**

---

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
  <img src="https://img.shields.io/badge/platform-Windows-blue?style=flat-square&logo=windows" alt="Platform">
  <img src="https://img.shields.io/badge/language-C%2B%2B-orange?style=flat-square&logo=cplusplus" alt="Language">
  <img src="https://img.shields.io/badge/IDE-VS2019%2B-purple?style=flat-square&logo=visualstudio" alt="IDE">
</p>

---

# 📚 导航目录

- [1. 项目简介](#1-project-overview)
- [2. 免责声明](#2-legal-disclaimer)
- [3. 系统架构](#3-system-architecture)
  - [3.1 主控程序](#31-master-controller)
  - [3.2 受控程序](#32-controlled-client)
- [4. 部署方式](#4-deployment-methods)
  - [4.1 内网部署](#41-intranet-deployment)
  - [4.2 外网部署](#42-internet-deployment)
- [5. 更新日志](#5-changelog)
- [6. 其他项目](#6-other-projects)
- [7. 沟通反馈](#7-feedback-and-contact)

---

# 1. 项目简介 <a id="1-project-overview"></a>

**原始来源：** [zibility](https://github.com/zibility/Remote)

**功能概述：** 基于 Gh0st 的远程控制器，提供以下核心功能：
- 终端管理、进程管理、窗口管理
- 桌面管理、文件管理、语音管理、视频管理
- 服务管理、注册表管理、键盘记录
- SOCKS 代理、虚拟桌面、远程代码执行

如果您对远程控制技术感兴趣并喜欢本项目，欢迎为本项目添加 Star。同时，Fork、Watch、提交 Issues 或发起 Pull Request 均受欢迎。作者将在业余时间对所提问题进行修复与维护。

[![Star History Chart](https://api.star-history.com/svg?repos=yuanyuanxiang/SimpleRemoter&type=Date)](https://star-history.com/#yuanyuanxiang/SimpleRemoter&Date)

<span style="color:#FF5722; font-weight:bold;">*本程序仅限于学习和技术交流用途，使用者需对自身使用该软件所产生的一切后果承担责任。*</span>

**项目用途：** 本项目适用于远程办公、远程设备管理、远程协助等合法合规场景。

**起始日期：** 2019.1.1

**编译方法：** 请用**git clone**本项目代码，直接下载为zip文件，代码格式默认是UNIX风格，将会出现奇怪的编译问题。
本项目采用 VS2019 进行开发和维护，同时兼容 VS2022/VS2026 等较新版本的编译器，详见：[#171](https://github.com/yuanyuanxiang/SimpleRemoter/issues/171)。

使用新版编译器时可能遇到依赖库冲突问题，解决方案请参考：[#269](https://github.com/yuanyuanxiang/SimpleRemoter/issues/269)。

主控程序在非中文系统上可能出现乱码，解决方法请参阅：[#157](https://github.com/yuanyuanxiang/SimpleRemoter/issues/157)。

**代码风格：**

```cmd
for /R %F in (*.cpp *.h *.c) do astyle --style=linux "%F"
```

# 2. 免责声明 <a id="2-legal-disclaimer"></a>

本项目为远程控制技术的研究性实现，仅供合法学习用途。**严禁**用于非法侵入、控制、监听他人设备等违法行为。

本软件按"现状"提供，不附带任何明示或暗示的保证。使用本软件的风险由用户自行承担。开发者不对任何因使用本软件而引发的非法或恶意用途承担责任。用户应遵守所在地区的法律法规，并以负责任的方式使用本软件。开发者对因使用本软件所产生的任何直接或间接损害不承担责任。

# 3. 系统架构 <a id="3-system-architecture"></a>

![Architecture](https://github.com/yuanyuanxiang/SimpleRemoter/wiki/res/Architecture.jpg)

本程序（自 v1.1.1 起）采用两层架构设计：

1. 超级用户负责分发并管理各主控程序
2. 各主控程序分别控制其下属的受控计算机

该架构具有以下特点：

- **层级控制**：借助下层主控程序作为跳板，超级用户可对整个系统中的任意计算机进行控制
- **隔离机制**：不同主控程序（Master 1、2、3 等）所管理的计算机相互隔离，主控程序仅能控制由其自身生成的客户端
- **集中授权**：由超级用户统一管理各主控程序的授权

**郑重提示：严禁用于非法控制他人设备。**

## 3.1 主控程序 <a id="31-master-controller"></a>

主控程序为 **YAMA.exe**，作为 Server 端，基于 IOCP 通信模型，支持上万主机同时在线。得益于分层控制架构，系统支持的主机数量可提升一个数量级。例如，一个超级用户管理 10 个 Master，每个 Master 控制 1 万台主机，则超级用户可控制的主机总数达到 10 万台。

以下展示主控程序的运行界面，所有功能均可正常使用，程序运行稳定。部分功能要求受控程序以管理员权限运行。

![主界面](./images/Yama.jpg)

主界面以列表形式展示已连接到本机的受控程序。选中某一主机后，可进行远程控制操作，如修改备注、代理映射、执行代码等。

![终端管理](./images/Console.jpg)

**终端管理**：打开命令行窗口，执行远程命令。另有一个[极简版本](./linux/main.cpp)，已支持 Linux 客户端，供 Linux 开发者研究使用。

![进程管理](./images/Process.jpg)

**进程管理**：显示受控机器上正在运行的进程，可对普通进程进行启停操作或代码注入（无法操作高权限进程）。

![窗口管理](./images/Window.jpg)

**窗口管理**：显示受控机器上已打开的窗口或程序，支持最大化、最小化、隐藏或显示窗口等操作。

![桌面管理](./images/Remote.jpg)
![桌面管理](./images/RemoteSet.jpg)

**桌面管理**：即"远程桌面"功能，用于控制远程机器。可通过菜单设置远程桌面参数：
- **屏幕截图方式**：支持 GDI、DXGI 或 VIRTUAL（虚拟桌面）
- **图像压缩方式**：支持灰度图像传输、屏幕差异算法和 H.264 压缩

其中 VIRTUAL（虚拟桌面）模式可在被控计算机后台运行远程桌面程序，已针对操作流畅度进行优化。此外，支持上报活动窗口和检测指定软件。屏幕传输经 SSE2 和多线程深度优化，跨网情况下可达 30 FPS。远程桌面控制支持多显示器和多屏上墙，支持隐私屏幕。若客户端以 Windows 服务方式运行，则支持在锁屏状态下进行远程控制并输入密码解锁。支持使用 Ctrl+C 和 Ctrl+V 在主控与被控之间传输文件。

![文件管理](./images/FileManage.jpg)

**文件管理**：在本机与受控机器之间传输文件。

![语音管理](./images/Voice.jpg)

**语音管理**：监听受控机器的声音，也可向受控计算机发送语音（需受控机器配备音频设备）。

![视频管理](./images/Video.jpg)

**视频管理**：打开受控机器的摄像头。设置中默认勾选检测摄像头选项，主机列表将显示摄像头状态。

![服务管理](./images/Service.jpg)

**服务管理**：查看受控机器上的服务列表，在权限允许的情况下可对服务进行启动、停止等操作。

![注册表管理](./images/Register.jpg)

**注册表管理**：查看受控机器上的注册表（仅支持查看，不支持修改）。

**其他功能**

- **机器管理**：对被控主机执行注销、关机或重启操作
- **客户管理**：转移或分享客户端，支持设置有效期，到期后自动恢复
- **客户代理**：开启代理映射，借助客户端进行 SOCKS 代理；或利用 FRP 代理客户端的指定端口（如 3389），即可使用 mstsc.exe 工具进行远程桌面控制
- **执行程序**：客户端从远程下载 EXE 程序执行，或从主控端上传 EXE 程序到客户端执行
- **执行代码**：编写符合规范的 DLL，即可将该 DLL 传输到客户端执行，具有极高的灵活性。客户端会缓存此 DLL，避免每次执行代码时重复传输

**关于授权：**

自 v1.0.8 起，操作主控程序需要获得授权。新编译的程序享有 14 天试用期，过期后生成服务端需凭借"序列号"申请口令。如需屏蔽该授权逻辑，请参考 `OnOnlineBuildClient` 函数并重新编译程序，详见：[#91](https://github.com/yuanyuanxiang/SimpleRemoter/issues/91)。"口令"包含授权日期范围，确保一机一码；授权逻辑会检测计算机日期是否被篡改。生成口令需使用密码。

![AuthDlg](./images/AuthDlg.jpg)

![PasswordGen](./images/PasswordGen.jpg)

自 v1.1.1 起，撤销对新编译程序的授权。任何人使用本程序均需自行编译，否则程序将在运行 10 分钟后弹出对话框，要求输入口令。授权机制的引入可将缺乏编程经验的用户阻挡在外。若仅想体验此远程控制程序，建议使用 v1.0.7 及更早版本，其核心功能与后续版本无本质差异。若您对技术有兴趣，相信您有足够能力自行编译程序。

自 v1.2.0 起，主控程序必须取得授权，否则功能将受限。授权方式支持按计算机绑定或按域名绑定。尝试绕过授权可能导致主控程序无法正常工作。已取得授权的主控程序将自动断开与授权服务器的连接。若授权未成功，则继续保持与授权服务器的连接并进行必要的数据交互——这一机制可能被定义为"后门"，但这是必需的，且无法被绕过。

## 3.2 受控程序 <a id="32-controlled-client"></a>

![主界面](./images/TestRun.jpg)

受控程序为 Client 端，支持两种运行形式：

1. **独立程序形式**：ghost.exe 作为单个程序运行，不依赖其他动态链接库
2. **分离加载形式**：TestRun.exe + ServerDll.dll，由 EXE 程序调用核心动态链接库

注意：自 [v1.0.8](https://github.com/yuanyuanxiang/SimpleRemoter/releases/tag/v1.0.0.8) 起，`TestRun.exe` 采用内存加载 DLL 的运行方式，向主控程序请求 DLL 并在内存中执行，有利于代码热更新。

---

# 4. 部署方式 <a id="4-deployment-methods"></a>

## 4.1 内网部署 <a id="41-intranet-deployment"></a>

内网部署是指主控程序与受控设备位于同一局域网内，受控设备能够直接连接主控的地址。这种部署方式非常简单，在生成服务端时填写主控的 IP 和端口即可。

## 4.2 外网部署 <a id="42-internet-deployment"></a>

外网部署是指主控程序与受控程序位于不同网络，主控设备没有公网 IP，受控设备无法直接连接主控地址。此时需要一个"中间人"将主控设备的内网 IP 穿透出去。一种方式是[使用花生壳](./使用花生壳.txt)，此处不再赘述。若需进行跨境/跨国远程控制，强烈建议使用 VPS 而非花生壳。

本文介绍第二种方法，其原理与使用花生壳类似：

```
受控 ──> VPS ──> 主控
```

使用 VPS（Virtual Private Server，虚拟专用服务器）作为"中间人"，实现对远程设备的控制。当然也可使用物理服务器，但 VPS 更具性价比。通常您需要购买 VPS 服务。VPS 与程序之间的通信基于 [FRP](https://github.com/fatedier/frp)。

在这种部署模式下，生成服务端程序时 IP 填写 VPS 的 IP（部分 VPS 供应商提供域名，也可填写域名）。通常在 VPS 上运行 FRP 服务端程序，在本地运行 FRP 客户端程序。当主机连接 VPS 时，VPS 会将请求转发到本地计算机；同样，控制请求也将经由 VPS 中转到受控主机。

有关跨网、跨境或跨国远程控制系统的部署方式，详见：[反向代理部署说明](./反向代理.md)，这是作者实践所采用的方式。

# 5. 更新日志 <a id="5-changelog"></a>

更早的变更记录参看：[history](./history.md)

**发布 v1.2.2（2026.1.11）：**

本版本主要增强远程桌面设置持久化、文件管理功能，并改进授权和键盘转发。

- 改进：将远程桌面屏幕设置保存到注册表
- 修复：日期超出范围时授权返回失败
- 改进：当 `GetClipboardFiles` 失败时使用 `GetForegroundSelectedFiles`
- 新功能：在弹出窗口中显示屏幕分辨率和客户端 ID
- 新功能：支持自定义客户端名称和安装目录
- 新功能：添加设置远程控制屏幕策略的菜单
- 改进：显示文件传输进度对话框
- 新功能：文件管理对话框支持压缩文件
- 改进：添加 F10、WM_SYSKEYDOWN、WM_SYSKEYUP 的转发

**发布 v1.2.1（2026.1.1）：**

本版本主要增强远程桌面功能（新增 FPS 控制、屏幕切换、文件拖拽支持），改进 Windows 服务运行模式，并修复多个稳定性问题。

- 改进客户端构建：将 shellcode 追加到文件末尾
- 修复：`TestRun` 以 Windows 服务运行时无法解锁屏幕
- 新功能：为远程桌面添加 FPS 控制菜单
- 修复：`AddList` 中的存在性检查包含客户端 IP 检查
- 回退 #242 并改进向客户端发送文件时的安全性
- 修复切换屏幕并支持拖拽文件到远程
- 修复 #266：CloseHandle 关闭无效句柄
- 改进：添加更多系统热键转发
- 新功能：添加控制主控端是否以服务运行的菜单
- 改进 `ToolbarDlg` 滑入/滑出性能



---

# 6. 其他项目 <a id="6-other-projects"></a>

- [HoldingHands](https://github.com/yuanyuanxiang/HoldingHands)：全英文界面的远程控制程序，采用不同的架构设计
- [BGW RAT](https://github.com/yuanyuanxiang/BGW_RAT)：功能全面的远程控制程序（大灰狼 9.5）
- [Gh0st](https://github.com/yuanyuanxiang/Gh0st)：另一款基于 Gh0st 的远程控制程序

---

# 7. 沟通反馈 <a id="7-feedback-and-contact"></a>

**作者QQ：** 请通过 962914132 联系

**联系方式：** [Telegram](https://t.me/doge_grandfather) | [Email](mailto:yuanyuanxiang163@gmail.com) | [LinkedIn](https://www.linkedin.com/in/wishyuanqi)

**问题报告：** [Issues](https://github.com/yuanyuanxiang/SimpleRemoter/issues)

**欢迎提交：** [Pull Requests](https://github.com/yuanyuanxiang/SimpleRemoter/pulls)

**赞助方式 / Sponsor：** 本项目源于技术学习与兴趣爱好，作者将根据业余时间不定期更新。**如果本项目对您有所帮助，欢迎通过赞助图标支持本项目。** 若希望通过其他方式（如微信、支付宝、PayPal）进行赞助，请点击[这里](https://github.com/yuanyuanxiang/yuanyuanxiang/blob/main/images/QR_Codes.jpg)。
