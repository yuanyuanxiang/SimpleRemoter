# 历史变更记录

[Chinese, since 2019]

这篇文档记录本项目的一些历史变更记录。
This document records the history changes of this project.

2019.1.5

1、整理垃圾排版，优化上线下线处理逻辑。
2、修复部分内存泄漏问题，改善线程处理逻辑。
3、修复客户端不停断线重连的缺陷。解决部分内存泄漏缺陷。
4、解决几处缺陷。【遗留问题】文件管理对话框释放资源导致第2次打开崩溃。

2019.1.6

1、改用EnumDisplaySettings获取屏幕大小，原方法获取屏幕大小不准。

2、将FileManagerDlg、InputDlg、FileTransferModeDlg、TrueColorToolBar还原到gh0st最初版本。

3、新增项目"ghost"，不通过TestRun调用dll，而是直接生成可执行文件。

4、修复开启视频，客户端产生的一处内存泄漏缺陷，m_pCapture需要释放。

2019.1.7

1、ghost单台电脑只允许启动唯一的实例。

2、远程桌面反应迟钝，改用每秒传送8帧屏幕，后续有待优化。

2019.1.8

1、发现传屏的瓶颈在zlib压缩数据，更新zlib到版本V1.2.11，提高传送屏幕速度到每秒10帧。

2、ghost的类CBuffer不需要临界区。

2019.1.9

1、服务端IOCPServer类的工作线程改为计算机核心个数的2倍。

2、解决服务端主动退出的内存泄漏问题，泄漏源在OVERLAPPEDPLUS。

2019.1.10

1、服务端远程控制增加全屏（系统右键菜单）、退出全屏（F11）的功能。

2、修复客户端机器屏幕缩放时远程桌面鼠标光标位置不准确的问题。（跟踪光标受影响）

3、发现服务端需要采用默认英文输入法，才能在远程桌面输入中文（怀疑本地输入法截获消息）。

4、添加崩溃时写dump文件的代码。

2019.1.11

1、修复文件管理对话框多次打开崩溃的问题（【遗留问题】）。

2、遗留问题：远程cmd窗口总是将输入命令输出2次、文件对话框的菜单操作可能已失效。

2019.1.12

1、还原客户端的文件管理模块代码为gh0st的源码3.6版本.

2、修复上述"cmd窗口总是将输入命令输出2次"遗留问题。

3、打开注册表关闭后崩溃，参照按对文件管理窗口的修改进行处理。遗留问题：
	并无内存泄漏，但退出时报"HEAP: Free Heap modified after it was freed"问题。

4、退出时睡眠一会，等待服务端清理，发现这样可以避免退出时崩溃的概率。

5、发布稍微稳定的版本V1.0.0.1。

2019.1.13

1、在主对话框清理子窗口的资源（原先在各自的OnClose函数），通过CLOSE_DELETE_DLG控制。

2、修正CFileManagerDlg的构造函数调用SHGetFileInfo和FromHandle方法，解决多次打开崩溃。

3、更新服务端zlib版本为V1.2.11。（与客户端不同，因inflate_fast 崩溃，没有采用汇编）

2019.1.15

1、修复主控端CTalkDlg的内存泄漏问题，被控端即时消息对话框置于顶层。

2、SAFE_DELETE(ContextObject->olps)有崩溃概率。改为主控端退出时先令被控端退出，就没有内存泄漏。

3、开关音频时偶有内存泄漏，waveInCallBack线程不能正常退出。

2019.1.16

1、智能计时宏AUTO_TICK有问题，不应该用无名的局部变量auto_tick。

2、采用由Facebook所开发的速度更快的压缩库zstd，提高程序运行效率。
	参看：https://github.com/facebook/zstd

2019.1.17

1、添加比zstd更快的压缩库（压缩率不如zstd和zlib）lz4 1.8.3，参看
	https://github.com/lz4/lz4

2、修复被控端屏幕被缩放显示时远程桌面跟踪鼠标的位置不准的问题。

3、修复语音监听的问题，2个事件CAudio修改为非"Manual Reset"。

2019.1.18

1、整理部分垃圾代码。

2、发布V1.0.0.2。

2018.1.19

1、发现使用lz4压缩库时监控端程序进行远程桌面操作时容易崩溃，原因不明。

2、修复内存泄漏缺陷，在throw "Bad Buffer"的情况需要释放申请的内存。

2019.1.20

1、发现不管是采用zstd还是zlib，主控端在进行桌面控制时均有崩溃的几率（zlib较小）。

2、改用zlib压缩解压库。

3、完善追踪鼠标时鼠标形态变化时的展现效果。

4、当退出远程桌面窗口全屏状态时，不再向远程被控端发送F11。

5、发现在有线网络条件下主控端崩溃几率较小。

6、禁用主控端输入法，解决使用远程桌面在被控端输入时的麻烦问题。

2019.1.21

减少远程桌面new缓冲区的频率，将部分从堆上new固定内存的操作改用从栈上分配内存。

2019.1.22

减少音频视频捕获过程中频繁申请内存。

2019.1.25

1、修复被控端消息提示对话框在消息换行时显示不完整的问题。

2、添加/完善录制远程被控端视频的功能。

3、修复语音监听对话框显示已收到数据不更新状态的问题。

4、发现"发送本地语音"会导致主控端容易崩溃的问题，现象类似于操作远程桌面时的随机崩溃。

5、设置视频监控对话框为可调整大小，为其设置图标。

2019.1.26

1、发布V1.0.0.3。

2、修复Release模式打不开远程视频，或打开视频时画面卡住的问题，问题出在CCaptureVideo GetDIB。

2019.2.4

清理垃圾注释、整理不良排版，对代码略有改动。

遗留问题：文件管理功能无效、主控端随机崩溃。因此有必要将文件管理的功能屏蔽。

发布V1.0.0.4。

2019.3.24

1、将"2015Remote.rc"的一个光标文件"4.cur"的路径由绝对路径改为相对路径。

2、新增Release模式编译后控制台运行时不可见，新增TestRun向注册表写入开机自启动项。

2019.3.29

1、主控端和受控端同时修改LOGIN_INFOR结构，修复了受控端上报的操作系统信息不准确的问题。

2、发布V1.0.0.5。

注意：此次更新后的主控端需要和受控端匹配使用，否则可能出现问题。

2019.4.4

ghost项目采用VS2012 xp模式编译，以便支持在XP系统上运行。

2019.4.14

在2015RemoteDlg.h添加宏CLIENT_EXIT_WITH_SERVER，用于控制ghost是否随Yama退出。

2019.4.15

明确区分开退出被控端和退出主控端2个消息，只有发送退出被控端消息才会停止Socket客户端。

2019.4.19
1、TestRun读取配置文件改为setting.ini，配置项为 [settings] localIp 和 ghost。
2、CAudio的线程waveInCallBack在while循环有一处return，已改为break.

2019.4.20
TestRun在写入开机自启动项时先提升权限，以防止因权限不足而写注册表失败。

2019.4.30
升级全部项目采用Visual Studio Community 2015编译。

2019.5.6
当TestRun、ClientDemo运行时若未成功加载ServerDll.dll，则给出提示。
所有项目均采用平台工具集"Visual Studio 2012 - Windows XP (v110_xp)"，以支持在XP上运行。

2019.5.7
1、添加对远程IP使用域名时的支持，若IP为域名，先将域名进行解析后再连接。
2、添加文档“使用花生壳.txt”，介绍了如何使用花生壳软件搭建远程监控系统。

2019.5.8
优化左键点击Yama托盘图标的效果。

2019.5.11
优化远程桌面发送屏幕的功能，可动态调整发送屏幕的速率。

2019.8.25
调整项目设置，解决采用VS2015编译时某些项目不通过的问题。

2021.3.14
修复了若干个问题。

2024.9.6
1.新增"2019Remote.sln"支持使用Visual Studio 2019编译项目。
2.增加了使用VLD的操作方法，详见"server\2015Remote\stdafx.h"。
注意：自VS2019开始，不支持XP系统了（微软已经声明这个变更）。如果有需要在XP系统进行监控的需求，推荐使用"2015Remote.sln"。
如果使用VS2015编译，需将WindowsTargetPlatformVersion修改为8.1，将PlatformToolset修改为v140_xp。

2024.12.26
解决主控程序概率性崩溃的问题，增强主控程序运行的稳定性。本人未进行广泛测试，不保证彻底根治，但稳定性有明显改观。
fix: client threads number excceeding bug
fix: #19 the CBuffer causing server crash
fix: showing the wrong host quantity in status bar

2024.12.27
solve some issues according to code analysis result
reorg: Move commands to common/commands.h
此次提交的重点是将重复代码移动到公共目录，减少代码的冗余。

2024.12.28
1.修改了注册指令内容，新生成的主控程序和被控程序不能和以往的程序混用!! 预留了字段，以便未来之需。
2.解决客户端接收大数据包的问题! 主控程序增加显示被控端版本信息，以便实现针对老版本在线更新(仅限基于TestRun的服务)的能力。
在主控程序上面增加了显示被控端启动时间的功能，以便掌握被控端程序的稳定性。
3.完善生成服务程序的功能。

2024.12.29
增加显示被控程序"类型"的功能：如果被控程序为单个EXE则显示为"EXE"，如果被控程序为EXE调用动态库形式，则显示为"DLL".
当前，只有类型为DLL的服务支持在线升级。本次提交借机对前一个更新中的"预留字段"进行了验证。

在动态链接库中增加导出函数Run，以便通过rundll32.exe调用动态链接库。这种形式也是支持在线对DLL进行升级的。

2024.12.31
生成服务时增加加密选项，当前支持XOR加密。配合使用解密程序来加载加密后的服务。

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

**2025.08.08**

发布版本 v1.1.5：

此版本重点提升了远程控制体验（尤其是多屏支持和鼠标/全屏行为）、增强了集成性与权限管理，并解决了若干关键性 Bug。

* 新功能：新增以管理员身份运行客户端程序的功能  
* 新功能：将 frp 客户端集成至主程序中  
* 改进：显示非活跃锁定客户端的状态  
* 清理：移除旧版本历史记录  
* 修复：#176 #177 桌面控制功能异常的问题  
* 改进：在当前显示器上进入全屏模式  
* 修复：窗口中显示正确的鼠标光标状态  
* 改进：支持多屏桌面监控  
* 修复：虚拟桌面控制支持多显示器  
* 修复：按下 F1 键时避免自动打开网页

**2025.09.11**

发布版本 v1.1.6：

本次更新新增了客户端压缩和构建配置功能，支持多屏幕显示（视频墙）；优化了性能（如多线程压缩和 RTT 机制）；修复了通信协议和注入相关问题。

* 功能：构建客户端时支持压缩选项  
* 修复：#182 第一次命令使用 HTTP 协议  
* 优化：为客户端启用 zstd 多线程压缩  
* 优化：Master 使用 ZSTD_DCtx，并启用新的 RTT  
* 修复：改进注册表创建和 shellcode 注入  
* 优化：getPublicIP 可能失败并阻塞主程序的问题  
* 功能：支持设置客户端构建标志  
* 功能：新增客户端 shellcode 构建选项  
* 功能：支持多个远程显示器（视频墙）  

**2025.10.12**

发布版本 v1.1.7：

增加新工具、自动删除客户端、修复 IP 问题，并加入私有远程桌面代码。

* 功能：添加数字货币地址劫持功能（仅供研究）
* 功能：#193 首次运行后自动删除客户端
* 功能：添加用于更改 exe 文件图标的工具菜单
* 修复：#195 客户端获取公网 IP 失败
* 功能：添加卸载客户端程序的菜单
* 功能：添加远程桌面隐私屏幕源代码

**2025.11.15**

发布版本 v1.1.8：

本次更新修复多项稳定性与安全性问题、增强远程控制中的剪贴板与文件操作功能，并加入插件示例。

* 修复: #204 将套接字连接改为非阻塞模式  
* 风格: 格式化源代码并支持客户端分组  
* 修复: 在注册表中保存 shellcode 并在可能时使用  
* 新功能: 添加机器注销、关机和重启命令  
* 修复: UpdateClientClipboard 可能会丢失最后一个字母  
* 新功能: 支持使用 Ctrl+V 从远程复制文本  
* 修复: #210 保存解码缓冲区时的栈溢出  
* 修复: #212 printf 的未定义行为  
* 修复: #185 和 #214  
* 新功能: 远程控制中支持文件复制/粘贴  
* 新功能 & 修复: 在主程序中显示用户名  
* 改进: 在生成通行码时生成 HMAC  
* 新功能: 添加加载 bin 文件以测试 shellcode 的菜单  
* 修复: 更新钱包地址时无需重启客户端  
* 新功能: 添加构建和测试 AES 加密 shellcode 的菜单  
* 新功能: 支持使用 pe_to_shellcode 转换 PE  
* 插件: 添加示例插件项目供参考  
* 新功能: 为进程管理添加 shellcode 注入功能

**2025.12.14**

发布版本 v1.1.9：

本次更新重点提升客户端稳定性与运行模式、优化远程桌面性能，并新增多项实用功能。

* 改进: 修改 client/SimpleSCLoader.c
* 功能: 在进程管理中支持反黑屏功能
* 改进: 为 `SCLoader` 添加调试代码
* 功能: 在客户端构建选项中添加 `TinyRun.dll`
* 修复: 查看注册表导致主控程序崩溃的问题
* 修复: 打开密码生成对话框会修改最大连接数
* 功能: 在远程桌面控制中支持录制视频
* 功能: 支持客户端以 Windows 服务方式运行
* 功能: 为主控程序添加参数设置菜单
* 功能: 在远程控制中添加切换屏幕的菜单
* 修复: 注册表错误，并使用 [MT] 重新编译 zlib、x264 和 libyuv
* 功能: 为程序添加启动进度显示
* 改进: 将多线程压缩设置为远程控制的可选项
* 改进: 使用 SSE2 提升位图比较速度
* 改进: 代码风格调整，并使用优化选项重新编译 zstd
* 修复: 客户端死机问题，改进大数据包发送
* 改进: 减少 IOCPServer 中 new/delete 内存的频率
* 修复: 某些情况下 "std::runtime_error" 导致崩溃
* 修复: TestRun (MDLL) 配置不生效的问题
* 功能: 支持将 TestRun 构建为 Windows 服务
* 改进: 通过异步消息处理提升主控端效率
* 改进: 请求以管理员身份运行主控程序
* 功能: 为在线主机添加 shellcode 注入菜单
* 修复: `Ghost` 以 Windows 服务方式运行失败
* 日志: 为 FileUpload 库和服务安装添加日志
* 修复: 使用自定义结构体替代 char 缓冲区
* 修复: 使用 DXGI 时禁用 SSE2（会导致崩溃）
* 修复 (Windows 服务): 移除 AI 产生的冗余代码
* 改进: 将客户端以管理员运行改为可选项
* 修复: AudioManager 错误，移除 struct dlgInfo
* 修复: 注册计划任务失败的问题，并添加日志
* 修复: 主控和客户端之间复制文本需要延迟
* 改进: 在客户端构建选项中添加 `runasAdmin`
* 修复: 客户端离线问题和虚拟桌面打开问题
* 改进: 为客户端程序计算唯一 ID

**2025.12.25**

发布版本 v1.2.0：

本次更新主要涉及远程桌面优化、Go 语言服务端框架、授权系统改进以及多项 Bug 修复。

* 功能: 新增 Go 语言 TCP 服务端框架
* 功能: 使用 `frpc.dll` 代理客户端 TCP 端口
* 功能: 支持上传/下载并运行可执行文件
* 功能: 为远程桌面控制添加重连逻辑
* 功能: 完善客户端重新分组逻辑并添加日志控制
* 功能: 添加客户端向主控端发送消息的命令
* 功能: 支持生成绑定域名的口令
* 改进: 4K 远程桌面缩放至 1080P 显示 (#267)
* 改进: 使用弹出对话框退出全屏（移除 F11 快捷键）
* 改进: 支持向远程桌面发送 WIN 键
* 改进: 更改客户端程序的注册表/互斥锁名称
* 改进: 主机上线通知改为异步消息处理
* 改进: 添加 HMAC 验证主控端口令
* 修复: 远程桌面相关的小问题
* 修复: 使用 PowerShell 获取硬件信息（Windows 7 及以上）
* 修复: 窗口未使用 DWM 渲染时远程屏幕黑屏问题
* 修复: 非默认分组客户端显示在默认列表的问题
* 修复: 日期验证导致主控程序 UI 阻塞的问题
* 修复: 操作 `CharMsg` 时的栈溢出问题
* 修复: 重写 FRPC 设置文件前先删除旧文件
* 修复: 撤销在 `ProgramData` 中复制并运行客户端程序的更改
* 服务端: Go 语言授权客户端验证成功后自动退出

**2026.01.01**

发布版本 v1.2.1：

本版本主要增强远程桌面功能（新增 FPS 控制、屏幕切换、文件拖拽支持），改进 Windows 服务运行模式，并修复多个稳定性问题。

* 改进: 客户端构建时将 shellcode 追加到文件末尾
* 修复: `TestRun` 以 Windows 服务运行时无法解锁屏幕
* 功能: 为远程桌面添加 FPS 控制菜单
* 修复: `AddList` 中的存在性检查包含客户端 IP 检查
* 修复: 回退 #242 并改进向客户端发送文件时的安全性
* 修复: 切换屏幕并支持拖拽文件到远程
* 修复: #266 CloseHandle 关闭无效句柄
* 改进: 添加更多系统热键转发
* 功能: 添加控制主控端是否以服务运行的菜单
* 改进: `ToolbarDlg` 滑入/滑出性能

**2026.01.11**

发布版本 v1.2.2：

本版本主要增强远程桌面设置持久化、文件管理功能，并改进授权和键盘转发。

* 改进: 将远程桌面屏幕设置保存到注册表
* 修复: 日期超出范围时授权返回失败
* 改进: 当 `GetClipboardFiles` 失败时使用 `GetForegroundSelectedFiles`
* 功能: 在弹出窗口中显示屏幕分辨率和客户端 ID
* 功能: 支持自定义客户端名称和安装目录
* 功能: 添加设置远程控制屏幕策略的菜单
* 改进: 显示文件传输进度对话框
* 功能: 文件管理对话框支持压缩文件
* 改进: 添加 F10、WM_SYSKEYDOWN、WM_SYSKEYUP 的转发

---

[English, since 2025]

**2025.01.12**  
Fixed two potential remote desktop issues (#28, #29).  
Added sorting for the controller’s list windows (#26, #27), making it easier to locate windows, services, or processes.

Released version **v1.0.6**, noted for high stability.  
This version does **not** support older Windows XP systems  
(note: VS2019 and later have dropped support for XP toolchains; use older Visual Studio versions if needed).  
Download the latest release from GitHub or clone the repo.  
If antivirus software flags it as a virus, this is expected due to the nature of the software — feel free to compile it yourself for verification.

**2025.02.01**  
Inspired by [Gh0st](https://github.com/yuanyuanxiang/Gh0st/pull/2), added **keylogging** support, implemented by copying four core files:

*KeyboardManager.h、KeyboardManager.cpp、KeyBoardDlg.h、KeyBoardDlg.cpp*

**2025.04.05**

Release v1.0.7, mainly fix or add new feature:
 
- Update third-party libraries and switch zlib to zstd, old version v1.0.6 is compatible;
- Support compile with Win64;
- Fix buges and make the program more stable;
- Improve remote control efficiency and support more bitmap compressing method;
- Some code reorganization.

**2025.04.12**

Since v1.0.7 released in April 5:

- Improvement: Make sure that the input command is always at the end of `Shelldlg`, and build a **simple Linux client**;
- Fix bugs: #62, #74, #75 ;
- Change the flag 0x1234567 to a more readable string; and improve building service and allow chosing other files to build;
- Showing the user's activities and monitoring specified software;
- Clean up global variables and make it easy to create multiple clients in one program, which is useful for testing the master's capacity;
- Implement loading DLL in memory, make it easy to update client program;

**2025.04.21**

Release v1.0.8:

- Support to share online host with other masters;
- Implement service-generated authorization capability, and add a serial number generation menu;
- Add `HPSocket` libraries which may be used in the future, and add static ffmpeg libraries to build Win64 master;
- Implement a memory DLL runner: the `TestRun` program request DLL from master and execute in memory.

**2025.04.30**

Release v1.0.9：

- Update client building feature / All in one; The master will only accept the clients built by itself.
- Improve authorization feature.

**2025.06.01**

Release v1.1.0:

* fix: IOCPClient clear buffer when disconnect
* Implement SOCKS proxy feature
* Add menus and modify list style, add log
* feature: Add a C program to execute shell code
* feature: Encrypt for server address
* feat: Support virtual remote desktop monitoring
* feature: Add command to execute DLL

**2025.06.21**

Release v1.1.1:

*Starting from this version, the controller program requires authorization and 
will automatically connect to the authorization server. 
You may contact the author to request authorization. If you have concerns about this, 
please use an earlier version (prior to v1.0.8). 
Modifying and compiling the program yourself can also resolve this issue.*

* fix: remote desktop algorithm doesn't take effort
* Add some menus for operating online client
* Plugin: Add remote chat feature
* Plugin: Add browser decryption feature
* Plugin: Add host management feature
* Plugin: Add virtual desktop feature
* Improve: #48 Support sorting in file management dialog
* Feature: Support WinOS RAT client
* Improve authorization logic: Support authorize master online
* feature: Support random or multi connection
* Improvement: Add a popup window to show details
* Improve client stability


**2025.07.07**

Release v1.1.2:

* Fix the problem with the keylogging feature
* Security: Improve the authorization of plugins
* fix: Memory leak when release `CMachineDlg` object
* fix: Showing wrong IP while using `Reverse Proxy` (#147)
* Add a menu redirects to the authorization guide page
* Plugin: Add another file management module
* Improve: Reduce master program redundant code
* fix: Prevent crash when closing window during background data processing
* Improve: Remove redundant code for reading the registry
* layout: Reorganize TCP client/server code; Refactor the socket server code
* feature: Support listening on multiple ports simultaneously
* feature: Support client connections over UDP; Add client protocol option TCP/UDP
* Plugin: #145 Support remote drawing board
* Plugin: Add remote desktop privacy screen feature

**2025.07.19**

Release v1.1.3

* Add encrypt and decrypt functions
* Modify the popup message to be triggered by `NM_DBLCLK`
* Improve: Save DLL data to registry
* Feature: Support HTTP protocol and add building option
* Feature: Add encryption option for client building
* Improvement: Reduce transmit mouse move message
* fix: Lost control when operating high permission windows
* Improve client stability by handling exceptions
* feature: Remote desktop support multi monitor
* Improve: Support authorizing the online host quantity
* fix：#159 Authorization doesn't work under TestRun injection

**2025.07.29**

Release v1.1.4

* fix: Limit the online host number which uses UDP
* Feature: Implement KCP protocol - based on UDP
* Improve: Add random protocol option for building client
* Feature: Support remote desktop adaptive to window size
* Feature: Add a menu item to build shellcode
* Feature: support assigning client to another master
* Feature: Support adding client to watch list
* Improve: Avoid client computer going to sleep
* fix: #170 Remove the dependency of `VCOMP140.dll`
* Improve: Showing the client application version
* fix: Refresh client public IP every one week

**2025.08.08**

Release v1.1.5

This version focuses on improving the remote control experience (especially multi-monitor support and UI behavior), enhancing integration and permission handling, and resolving several critical bugs.

* Feature: Add run client program as admin feature
* Feature: Integrate frp client with master program
* Improve: Showing inactive locked client status
* Clean up: Remove old history releases
* fix: #176 #177 Desktop control does not work properly
* Improve: Enter full screen on the current monitor
* fix: Showing the correct cursor status on window
* Improve: Support multiple screen desktop monitoring
* fix: Virtual desktop control support multiple monitor
* fix: Avoid opening w web page when press F1

**2025.09.11**

Release v1.1.6

This update adds client compression and build options, supports multiple displays (video wall), improves performance (e.g., multi-threaded compression and RTT), and fixes protocol and injection issues.

* Feature: Support compression option when building client
* fix: #182 First command using HTTP protocol
* Improve: Enable zstd multi-thread compression for client
* Improve: Master using ZSTD_DCtx and using new RTT
* fix: Improve creating registry and injecting shellcode
* Improve: getPublicIP may fail and block mater program
* Feature: Support setting the client building flag
* Feature: Add client shellcode building option
* Feature: Supports multiple remote displays (video wall)

**2025.10.12** 

Release v1.1.7 

This version adds new tools, auto client deletion, IP fix, and private remote desktop code.

* Feature: Add digital coin hack feature (research only) 
* Feature: #193 Automatically delete client after first running 
* Feature: Add tool menu for changing exe file icon 
* fix: #195 Client get public IP failed 
* Feature: Add a menu to uninstall client program 
* Feature: Add private remote desktop source code

**2025.11.15**

Release v1.1.8

This update fixes several stability and security issues, enhances clipboard and file operations in remote control, 
and adds a plugin example.

* fix: #204 Change socket connecting to non-blocking mode
* style: Format source code and support grouping client
* fix: Save shellcode in registry and use it when possible
* Feature: Add machine logout, shutdown and reboot cmd
* fix: UpdateClientClipboard may lost the last letter
* Feature: Support copy text from remote with Ctrl+V
* fix: #210 Stack for saving decoded buffer overflow
* fix: #212 Undefined behavior on printf
* fix #185 and fix #214
* Feature: File copy/paste support in remote control
* Feature&fix: Show username on master program
* Improve: Generate HMAC while generating pass code
* feature: Add menu to load bin file to test shellcode
* fix: No need to restart client to update wallet address
* Feature: Add menu to build and test AES encrypted shellcode
* Feature: Support converting PE using pe_to_shellcode
* plugin: Add an example plugin project for reference
* Feature: Add shellcode injection feature for process management

**2025.12.14**

Release v1.1.9

This update focuses on improving client stability and running modes, optimizing remote desktop performance, and adding several practical features.

* Improve: Modify client/SimpleSCLoader.c
* Feature: Support anti black-screen in process management
* Improve: Add debug code for `SCLoader`
* Feature: Add `TinyRun.dll` to client building option
* fix: Viewing registry causing master program crash
* fix: Open password gen dialog will modify max connection
* Feature: Support recording video in remote desktop control
* Feature: Support client running as windows service
* Feature: Add parameters setting menu for master program
* Feature: Add menu to switch screen for remote control
* fix: Registry error and use [MT] to rebuild zlib, x264 and libyuv
* Feature: Add a startup progress display to the program
* Improve: Set multi-thread compression as a option for remote control
* Improve: Using SSE2 to improve bitmap compare speed
* Improve: Code style change and rebuild zstd with optimization options
* fix: Client dead issue and improve sending large packet
* Improve: Reduce new / delete memory frequency in IOCPServer
* fix: "std::runtime_error" causing crashes in some cases
* fix: TestRun (MDLL) configuration doesn't take effort
* Feature: Support build TestRun as windows service
* Improve: Master efficiency by using asynchronous message processing
* Improve: Ask for running master with administrator
* Feature: Add menu (online host) for injecting shellcode
* fix: `Ghost` run as windows service failed
* logs: Add log for FileUpload libraries and service installing
* fix: Use self-defined struct to replace char buffer
* fix: Disable SSE2 (which causes crash) while using DXGI
* fix (Windows Service): Remove the shit dropped by AI
* Improve: Change running client as admin to an option
* fix: AudioManager bug and remove struct dlgInfo
* fix: Register schedule task failed issue and add logs
* fix: Copy text between master and client need a delay
* Improve: Add `runasAdmin` to client building options
* fix: Client offline issue and virtual desktop opening issue
* Improve: Calculate unique ID for client program


**2025.12.25**

Release v1.2.0

This update focuses on remote desktop optimization, Go language server framework, authorization system improvements, and various bug fixes.

* Feature: Add Go TCP server framework
* Feature: Use `frpc.dll` to proxy client's TCP port
* Feature: Support upload/download executable file and run it
* Feature: Add reconnect logic for remote desktop control
* Feature: Complete re-group logic and add log control
* Feature: Add command for client sending msg to master
* Feature: Support gen pass code binding with domain
* Improve: Scale 4K desktop screen to 1080P (#267)
* Improve: Remove F11 to leave full screen (Use popup dialog)
* Improve: Send `WIN` key press action to remote desktop
* Improve: Change registry/mutex name of client program
* Improve: Move host online notification to `PostMessage`
* Improve: Add HMAC to verify master's Passcode
* fix: Small issues related to remote desktop control
* fix: Use PowerShell to get hardware info (>=Win7)
* fix: Remote screen black if the window doesn't use DWM to render
* fix: Non default group client showing in default list
* fix: DateVerify causes master program UI blocked
* fix: Stack overflow when operating `CharMsg`
* fix: Remove FRPC settings file before re-write it
* fix: Revert copy and run client program in `ProgramData`
* Server/go: Authorization client automatically exit if verify succeed

**2026.01.01**

Release v1.2.1

This release enhances remote desktop features (FPS control, screen switching, file drag-and-drop support), improves Windows service mode, and fixes several stability issues.

* Improve: Append shellcode to file end when building client
* Fix: `TestRun` run as Windows service can't unlock screen
* Feature: Add FPS control menu for remote desktop
* Fix: Exist-check include checking client IP in `AddList`
* Fix: Revert #242 and improve security when sending files to client
* Fix: Switch screen and support dragging files to remote
* Fix: #266 CloseHandle close an invalid handle
* Improve: Add more system hot key to forward
* Feature: Add menu to control if run master as service
* Improve: `ToolbarDlg` slide in / slide out performance

**2026.01.11**

Release v1.2.2

This release enhances remote desktop settings persistence, file management features, and improves authorization and keyboard forwarding.

* Improve: Save remote desktop screen settings in registry
* Fix: Authorization return failure if date is out of range
* Improve: `GetForegroundSelectedFiles` if `GetClipboardFiles` failed
* Feature: Show screen resolution and client id in popup window
* Feature: Support customizing client name and install directory
* Feature: Add menu to set screen strategy for remote control
* Improve: Showing file transmit progress dialog
* Feature: Support compress files in file management dialog
* Improve: Add F10, WM_SYSKEYDOWN, WM_SYSKEYUP to forward
