# 🌐 语言 | Language

**[🇨🇳 中文](./ReadMe.md) | [🇺🇸 English](./ReadMe_EN.md)**

---

# 📚 导航目录

- [1. 项目简介](#1-project-overview)
- [2. 免责声明](#2-legal-disclaimer)
- [3. 系统架构](#3-system-architecture)
  - [3.1 主控程序](#31-master-controller)
  - [3.2 受控程序](#32-controlled-client)
  - [3.3 Linux客户端](#33-linux-client)
- [4. 部署方式](#4-deployment-methods)
  - [4.1 内网部署](#41-intranet-deployment)
  - [4.2 外网部署](#42-internet-deployment)
- [5. 更新日志](#5-changelog)
- [6. 其他项目](#6-other-projects)
- [7. 沟通反馈](#7-feedback-and-contact)

---

# 1.项目简介 <a id="1-project-overview"></a>

**原始来源：** [zibility](https://github.com/zibility/Remote)

**功能概述：** 基于gh0st的远程控制器：实现了终端管理、进程管理、窗口管理、桌面管理、文件管理、语音管理、视频管理、服务管理、
注册表管理、键盘记录、SOCKS代理、虚拟桌面和执行代码等功能。
如果您热爱研究控制程序，喜欢本项目，请您对该项目添加星标。Fork、Watch此项目，提交Issues，发起Pull Request都是受欢迎的。
作者视业余空闲情况，将对所提问题进行修复。

[![Star History Chart](https://api.star-history.com/svg?repos=yuanyuanxiang/SimpleRemoter&type=Date)](https://star-history.com/#yuanyuanxiang/SimpleRemoter&Date)

<span style="color:#FF5722; font-weight:bold;">*此程序仅限于学习和技术交流用途，使用者本人需对自己使用该软件产生的结果进行负责。* </span>

**起始日期**：2019.1.1

**编译方法：** 此项目采用VS2019进行开发和维护，使用最新的VS2022编译器也可以成功编译，详见：[#171](https://github.com/yuanyuanxiang/SimpleRemoter/issues/171).

主控程序在非中文系统可能显示乱码，有关解决方法参看 [#157](https://github.com/yuanyuanxiang/SimpleRemoter/issues/157).


# 2.免责声明 <a id="2-legal-disclaimer"></a>

本项目为远程控制技术的研究性实现，仅供合法学习用途。**严禁**用于非法侵入、控制、监听他人设备等违法行为。

本软件以“现状”提供，不附带任何保证。使用本软件的风险由用户自行承担。我们不对任何因使用本软件而引发的非法或恶意用途负责。
用户应遵守相关法律法规，并负责任地使用本软件。开发者对任何因使用本软件产生的损害不承担责任。

# 3.系统架构 <a id="3-system-architecture"></a>

![Architecture](https://github.com/yuanyuanxiang/SimpleRemoter/wiki/res/Architecture.jpg)

此程序（自v1.1.1）采用2层架构：
- （1）.由超级用户分发并管理主控程序；

- （2）.各个主控程序分别控制一些计算机。

这个架构具有下述特点：
- 借助于下层的主控程序作跳板，超级用户可以对整个系统中任何计算机进行控制；
- 不同的主控程序（Master 1,2,3等）所管理的计算机之间不能乱连，即主控程序只能控制由其本身生成的控制端；
- 由超级用户管理主控程序的授权。

**郑重提示：严禁用于非法控制他人的设备。**

## 3.1主控程序 <a id="31-master-controller"></a>
主控程序为**YAMA.exe**，作为Server端，基于IOCP通讯，支持上万主机同时在线；并且得益于分层控制架构，
系统支持的主机数量上升一个数量级。例如，一个超级用户管理10个Master，每个Master控制1万台主机，那么对超级管理员而言
可以控制10万台主机。
下面展示主控程序运行界面，所有功能均可用，程序运行稳定。
某些功能要求受控程序以管理员权限运行。

![主界面](./images/Yama.jpg)

主界面以列表形式展示连接到本机的受控程序。
选中某个主机以便进行远程控制，例如修改备注、代理映射、执行代码等。

![终端管理](./images/Console.jpg)

终端管理打开命令行窗口，可以执行远程命令。有一个[极简版本](./linux/main.cpp)，已经支持Linux客户端，供Linux开发者研究使用。

![进程管理](./images/Process.jpg)

进程管理显示受控机器上面正在运行的进程，可对普通进程进行启停操作（无法操作高权限进程）。

![窗口管理](./images/Window.jpg)

窗口管理显示受控机器上面打开的窗口或程序，可对其进行操作，隐藏或显示窗口。

![桌面管理](./images/Remote.jpg)
![桌面管理](./images/RemoteSet.jpg)

桌面管理即"远程桌面"，控制远程机器。可以通过菜单设置远程桌面的参数：
屏幕截图方法支持GDI、DXGI或VIRTUAL，图像压缩方法支持灰度图像传输、屏幕差异算法和H264压缩。
值得一提的是，VIRTUAL即虚拟桌面，可以在被控计算机后台运行远程桌面程序，对操作的流畅度进行了优化。
另外，支持上报活动窗口和检测指定软件。

![文件管理](./images/FileManage.jpg)

文件管理即在本机和受控机器之间传输文件。

![语音管理](./images/Voice.jpg)

语音管理即监听受控机器的声音，也可以向受控计算机发送语音，需受控机器有音频设备。

![视频管理](./images/Video.jpg)

视频管理即打开受控机器的摄像头。在设置中默认勾选了检测摄像头，主机列表会显示是否有摄像头。

![服务管理](./images/Service.jpg)

服务管理即打开受控机器上面的服务列表，如果有权限，亦可对服务进行启动、停止等操作。

![注册表管理](./images/Register.jpg)

注册表管理即打开受控机器上面的注册表，只能查看注册表，不支持修改。

**关于授权：**
自v1.0.8起，操作主控程序需要获得授权。给新编译的程序14天试用期，过期之后生成服务端需要凭借"序列号"申请口令；
如果要屏蔽该授权逻辑，请参考`OnOnlineBuildClient`函数，重新编译程序，参看：
[#91](https://github.com/yuanyuanxiang/SimpleRemoter/issues/91)。
“口令”包含授权日期范围，确保一机一码；授权逻辑会检测计算机日期未被篡改。生成口令需使用密码。

![AuthDlg](./images/AuthDlg.jpg)

![PasswordGen](./images/PasswordGen.jpg)

自v1.1.1起，撤销对新编译程序的授权，任何人使用本程序，需要自行编译。否则程序将在使用10分钟后弹出对话框，请求输入口令。
授权逻辑的引入可以将大部分毫无编程经验的小白阻挡在外。
如果只想体验此远程控制程序，作者认为使用v1.0.7及以前版本即可，它们和后续版本在核心功能上没有本质不同。 
如果您对技术有兴趣，则作者认为您有足够的能力亲自编译程序。

## 3.2受控程序 <a id="32-controlled-client"></a>
![主界面](./images/TestRun.jpg)

受控程序是Client端，分为2种运行形式（"类型"）：单个程序 **（1）** ghost.exe和 **（2）** TestRun.exe+ServerDll.dll形式。
（1）单个程序运行时，不依赖其他动态链接库，而第（2）种情况运行时，由EXE程序调用核心动态链接库。

注意：自[v1.0.8](https://github.com/yuanyuanxiang/SimpleRemoter/releases/tag/v1.0.0.8)起，
`TestRun.exe`将采取内存加载DLL运行方式，向主控程序请求DLL并在内存中执行，这有利于代码的热更新。

## 3.3Linux客户端 <a id="33-linux-client"></a>

![LinuxClient](./images/LinuxClient.png)

在[v1.0.8](./Releases/v1.0.8/ghost)目录下实现了一个Linux端受控程序，当前只支持远程终端窗口。

![BuildDlg](./images/BuildDlg.jpg)

请在Linux环境编译得到客户端，然后在生成服务端对话框，选择该文件，填写上线地址生成Linux端程序。

---

# 4.部署方式 <a id="4-deployment-methods"></a>

## 4.1内网部署 <a id="41-intranet-deployment"></a>

指的是主控程序和受控设备在同一个局域网，受控设备能直接连接主控的地址。这种部署方式非常简单，
在生成服务时填写主控的IP和端口即可。

## 4.2外网部署 <a id="42-internet-deployment"></a>

指的是主控程序和受控程序在不同的网络，主控设备没有公网IP，受控设备不能直接连接主控的地址。
为此，需要一个“中间人”，能将主控设备的内网IP穿透出去。一种方式是[使用花生壳](./使用花生壳.txt)，此处不再赘述。

本文讲述第二种方法，其原理和使用花生壳无异：

*受控 ——> VPS ——> 主控*

我们用VPS作为“中间人”，实现对远程设备的控制。VPS的全称是“Virtual Private Server”，使用物理机也是可以的，不过VPS更具有性价比。
通常，您需要购买VPS。VPS和程序之间的通讯基于[FRP](https://github.com/fatedier/frp).

这种情况，在生成服务端程序时，IP填写的是VPS的IP，有些VPS供应商甚至提供域名，则填写域名也可以。通常，在VPS上运行FRP服务程序，
在本地运行FRP客户端程序。当有主机连接VPS时，则VPS这个中间人会把请求转发到本地计算机，而我们发起的控制请求也将经这个中间人打到受控主机。

有关跨网、跨境或跨国远程控制系统的部署方式，详见：[反向代理部署说明](./反向代理.md)，这是作者实践和所使用的方式。

# 5.更新日志 <a id="5-changelog"></a>

2025年以前的变更记录参看：[history](./history.md)

2025.01.12
修复被控程序关于远程桌面相关可能的2处问题（#28 #29）。增加对主控端列表窗口的排序功能（#26 #27），以便快速定位窗口、服务或进程。

发布一个运行**非常稳定**的版本v1.0.6，该版本不支持在较老的Windows XP系统运行（注：VS2019及以后版本已不支持XP工具集，为此需要更早的VS）。
您可以从GitHub下载最新的Release，也可以clone该项目在相关目录找到。如果杀毒软件报告病毒，这是正常现象，请信任即可，或者您可以亲自编译。

2025.02.01

参考[Gh0st](https://github.com/yuanyuanxiang/Gh0st/pull/2)，增加键盘记录功能。实质上就是拷贝如下四个文件：

*KeyboardManager.h、KeyboardManager.cpp、KeyBoardDlg.h、KeyBoardDlg.cpp*

**2025.04.05**

发布 v1.0.7，主要修复或新增以下功能：

- 更新第三方库，将压缩算法从 zlib 更换为 zstd，旧版本 v1.0.6 仍兼容；
- 支持编译为 Win64；
- 修复若干 Bug，提高程序稳定性；
- 提升远程控制效率，新增更多位图压缩方式；
- 对部分代码结构进行了重构。

**2025.04.12**

自 v1.0.7 于 4 月 5 日发布以来：

- 功能改进：确保 `Shelldlg` 输入命令始终定位在末尾，并构建了一个**简易的 Linux 客户端**；
- 修复 Bug：#62、#74、#75；
- 将原标志位 0x1234567 更改为更具可读性的字符串；改进构建服务功能，允许选择其他文件进行构建；
- 增加展示用户活动和监控指定软件的功能；
- 清理全局变量，使得一个程序中可以轻松创建多个客户端，便于测试主控端的负载能力；
- 实现内存加载 DLL，便于客户端程序热更新。

**2025.04.21**

发布 v1.0.8：

- 支持与其他主控共享在线主机；
- 实现服务端生成授权的能力，增加序列号生成菜单；
- 引入 `HPSocket` 库，为未来使用做准备，并引入静态 ffmpeg 库以支持构建 Win64 主控端；
- 实现内存中运行 DLL：`TestRun` 程序从主控请求 DLL 并在内存中执行。

**2025.04.30**

发布 v1.0.9：

- 更新客户端构建功能 / 一体化生成；主控仅接受由自身构建的客户端连接；
- 优化授权功能。

**2025.06.01**

发布 v1.1.0：

- 修复：IOCPClient 断开连接时清空缓冲区；
- 实现 SOCKS 代理功能；
- 增加菜单项，修改列表样式，添加日志记录；
- 新增功能：增加一个用于执行 Shellcode 的 C 程序；
- 新增功能：对服务器地址进行加密；
- 新增特性：支持虚拟远程桌面监控；
- 新增命令：支持执行代码（64位 DLL）。

**2025.06.21**

发布 v1.1.1:

*自该版本开始，主控程序需要授权，并且会自动连接到授权服务器，您可以联系作者请求授权。
如果对这个有意见，请使用早期版本（<v1.0.8）。自行修改和编译程序，也可以解决该问题。*

- 修复：远程桌面算法不生效的问题
- 新增：添加用于操作在线客户端的菜单项
- 插件：新增远程聊天功能
- 插件：新增浏览器数据解密功能
- 插件：新增主机管理功能
- 插件：新增虚拟桌面功能
- 改进：#48 文件管理对话框支持排序
- 新功能：主控支持 WinOS(银狐) 远控客户端（RAT）
- 改进授权逻辑：支持在线授权主控端
- 新功能：支持随机或多路连接，即多个域名随机上线或并发上线
- 改进：新增弹窗以显示主机的详细信息
- 改进客户端稳定性

**2025.07.07**

发布 v1.1.2:

* 修复：键盘记录功能的问题  
* 安全：增强插件的授权机制  
* 修复：释放 `CMachineDlg` 对象时的内存泄漏问题  
* 修复：使用 `Reverse Proxy` 时显示错误的 IP 地址 (#147)  
* 新增：添加跳转到授权指南页面的菜单项  
* 插件：新增一个文件管理模块  
* 优化：减少主程序中的冗余代码  
* 修复：在后台数据处理过程中关闭窗口导致的崩溃问题  
* 优化：移除读取注册表时的冗余代码  
* 架构调整：重构 TCP 客户端/服务器代码；重写套接字服务器逻辑  
* 新功能：支持同时监听多个端口  
* 新功能：支持客户端通过 UDP 连接；新增客户端协议选择（TCP/UDP）  
* 插件：#145 支持远程画板功能  
* 插件：增加远程桌面隐私屏幕功能  

**2025.07.19**

Release v1.1.3

- 添加加密和解密函数  
- 修改弹出消息为通过 `NM_DBLCLK` 触发  
- 改进：将 DLL 数据保存到注册表中  
- 新功能：支持 HTTP 协议并添加构建选项  
- 新功能：为客户端构建添加加密选项  
- 改进：减少鼠标移动消息的传输  
- 修复：在操作高权限窗口时失去控制的问题  
- 改进：通过异常处理提升客户端稳定性  
- 新功能：远程桌面支持多显示器  
- 改进：支持授权在线主机数量  
- 修复：#159 在 TestRun 注入模式下授权无效的问题  

**2025.07.29**

Release v1.1.4

* 修复：限制使用 UDP 的在线主机数量  
* 新功能：实现基于 UDP 的 KCP 协议  
* 改进：为构建客户端添加随机协议选项  
* 新功能：支持远程桌面自适应窗口大小  
* 新功能：添加菜单项用于生成 shellcode  
* 新功能：支持将客户端分配给其他主控端  
* 新功能：支持将客户端添加到监视列表  
* 改进：避免客户端计算机进入睡眠状态  
* 修复：#170 移除对 `VCOMP140.dll` 的依赖  
* 改进：显示客户端应用程序版本  
* 修复：每周刷新一次客户端公网 IP  


---

# 6.其他项目 <a id="6-other-projects"></a>

- [HoldingHands](https://github.com/yuanyuanxiang/HoldingHands)：此远控程序界面为全英文，采用不同的架构设计。
- [BGW RAT](https://github.com/yuanyuanxiang/BGW_RAT): 一款功能全面的远程控制程序，即大灰狼9.5.
- [Gh0st](https://github.com/yuanyuanxiang/Gh0st): 也是一款基于Gh0st的远程控制程序。

---

# 7.沟通反馈 <a id="7-feedback-and-contact"></a>

QQ：962914132

联系方式： [Telegram](https://t.me/doge_grandfather), [Email](mailto:yuanyuanxiang163@gmail.com), [LinkedIn](https://www.linkedin.com/in/wishyuanqi)

问题报告： [Issues](https://github.com/yuanyuanxiang/SimpleRemoter/issues) 

欢迎提交： [Merge requests](https://github.com/yuanyuanxiang/SimpleRemoter/pulls)

赞助方式 / Sponsor：该项目的研究出自技术学习和兴趣爱好，本人视业余情况不定期更新项目。
**如果该项目对你有益，请通过赞助图标对本项目进行支持。**
如果你希望采用其他方式（如微信、支付宝、PayPal）对本项目进行赞助，请点击
[这里](https://github.com/yuanyuanxiang/yuanyuanxiang/blob/main/images/QR_Codes.jpg)。
