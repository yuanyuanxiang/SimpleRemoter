# 项目简介

**原始来源：** [zibility](https://github.com/zibility/Remote)

**功能概述：** 基于gh0st的远程控制器：实现了终端管理、进程管理、窗口管理、桌面管理、文件管理、语音管理、视频管理、服务管理、注册表管理等功能。
如果您热爱研究控制程序，喜欢本项目，请您对该项目添加星标。Fork、Watch此项目，提交Issues，发起Pull Request都是受欢迎的。

根据本人空闲情况，此项目会不定期更新。若您想对该项目了解更多技术细节，喜欢讨论软件的各方面，学习和交流请通过适当的方式联系。

此程序仅限于学习和技术交流用途，使用者本人需对自己使用该软件产生的结果进行负责。

**起始日期**：2019.1.1

## 主控程序
主控程序为**YAMA.exe**是Server端，Release发布版本在单台电脑只能运行一个示例。
下面展示主控程序运行界面，所有功能均可用，程序运行稳定。
某些功能要求受控程序以管理员权限运行。

![主界面](./images/Yama.jpg)

主界面以列表形式展示连接到本机的受控程序。
选中某个主机以便进行远程控制。

![终端管理](./images/Console.jpg)

终端管理打开命令行窗口，可以执行远程命令。有一个[极简版本](./linux/main.cpp)，已经支持Linux客户端，供Linux开发者研究使用。

![进程管理](./images/Process.jpg)

进程管理显示受控机器上面正在运行的进程，可对进程进行启停操作。

![窗口管理](./images/Window.jpg)

窗口管理显示受控机器上面打开的窗口或程序，可对其进行操作。

![桌面管理](./images/Remote.jpg)
![桌面管理](./images/RemoteSet.jpg)

桌面管理即"远程桌面"，控制远程机器。可以通过菜单设置远程桌面的参数：
屏幕截图方法支持GDI或DXGI，图像压缩方法支持灰度图像传输、屏幕差异算法和H264压缩。虚拟桌面尚未开发，但也是能支持的。

![文件管理](./images/FileManage.jpg)

文件管理即在本机和受控机器之间传输文件。

![语音管理](./images/Voice.jpg)

语音管理即监听受控机器的声音，需受控机器有声音输入设备。

![视频管理](./images/Video.jpg)

视频管理即打印受控机器的摄像头，需受控机器有摄像头。

![服务管理](./images/Service.jpg)

服务管理即打开受控机器上面的服务列表。

![注册表管理](./images/Register.jpg)

注册表管理即打开受控机器上面的注册表。

## Linux 客户端

![LinuxClient](./images/LinuxClient.png)

在[v1.0.8](./Releases/v1.0.8/ghost)目录下实现了一个Linux端受控程序，当前只支持远程终端窗口。

![BuildDlg](./images/BuildDlg.jpg)

请在Linux环境编译得到客户端，然后在生成服务端对话框，选择该文件，填写上线地址生成Linux端程序。

## 关于授权

![AuthDlg](./images/AuthDlg.jpg)

![PasswordGen](./images/PasswordGen.jpg)

当前对生成服务功能进行了限制，需要取得口令方可操作。给新编译的程序14天试用期，过期之后生成服务端需要申请"序列号"；
如果要对其他功能乃至整个程序启动授权逻辑，或者屏蔽该授权逻辑，请参考`OnOnlineBuildClient`函数。
序列号包含授权日期范围，确保一机一码；授权逻辑会检测计算机日期未被篡改。

## 受控程序
![主界面](./images/TestRun.jpg)

受控程序是Client端，分为2种运行形式（"类型"）：单个程序 **（1）** ghost.exe和 **（2）** TestRun.exe+ServerDll.dll形式。
（1）单个程序运行时，不依赖其他动态链接库，而第（2）种情况运行时，由EXE程序调用核心动态链接库。

注意：自[v1.0.8](https://github.com/yuanyuanxiang/SimpleRemoter/releases/tag/v1.0.0.8)起，
`TestRun.exe`将采取内存加载DLL运行方式，向主控程序请求DLL并在内存中执行，这有利于代码的热更新。

# 更新日志

2025年以前的变更记录参看：[history](./history.md)

2025.01.12
修复被控程序关于远程桌面相关可能的2处问题（#28 #29）。增加对主控端列表窗口的排序功能（#26 #27），以便快速定位窗口、服务或进程。

发布一个运行**非常稳定**的版本v1.0.6，该版本不支持在较老的Windows XP系统运行（注：VS2019及以后版本已不支持XP工具集，为此需要更早的VS）。
您可以从GitHub下载最新的Release，也可以clone该项目在相关目录找到。如果杀毒软件报告病毒，这是正常现象，请信任即可，或者您可以亲自编译。

2025.02.01

参考[Gh0st](https://github.com/yuanyuanxiang/Gh0st/pull/2)，增加键盘记录功能。实质上就是拷贝如下四个文件：

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

# 沟通反馈

QQ：962914132

联系方式： [Telegram](https://t.me/doge_grandfather), [Email](mailto:yuanyuanxiang163@gmail.com), [LinkedIn](https://www.linkedin.com/in/wishyuanqi)

问题报告： [Issues](https://github.com/yuanyuanxiang/SimpleRemoter/issues) 

欢迎提交： [Merge requests](https://github.com/yuanyuanxiang/SimpleRemoter/pulls)

赞助方式 / Sponsor：

![Sponsor](https://github.com/yuanyuanxiang/yuanyuanxiang/blob/main/images/QR_Codes.jpg)
