# 🌐 Language | 语言

**[🇺🇸 English](./ReadMe_EN.md) | [🇨🇳 中文](./ReadMe.md)**

---

# Project Overview

**Original Source:** [zibility](https://github.com/zibility/Remote)

**Feature Summary:**  
A remote controller based on Gh0st, implementing functionalities including terminal management, process management, window management, desktop control, file transfer, voice management, video monitoring, service management, registry viewer, keylogging, SOCKS proxy, virtual desktop, code execution, and more.

If you're interested in control programs and enjoy this project, please consider starring it. Forks, watches, issue submissions, and pull requests are all welcome.  
The author will fix reported issues as time permits.

[![Star History Chart](https://api.star-history.com/svg?repos=yuanyuanxiang/SimpleRemoter&type=Date)](https://star-history.com/#yuanyuanxiang/SimpleRemoter&Date)

<span style="color:#FF5722; font-weight:bold;">*This software is intended for learning and technical communication only. Users are responsible for any consequences resulting from its use.*</span>

**Project Start Date:** January 1, 2019

## Disclaimer

This project is a research-oriented implementation of remote control technology and is intended solely for educational purposes.  
Any use of this software for unauthorized access, surveillance, or control of other systems is **strictly prohibited**.  

This software is provided "as is" without any warranty. Use of this software is at your own risk.  
We are not responsible for any illegal or malicious use resulting from this software.  
Users should comply with relevant laws and regulations and use this software responsibly.  
The developer assumes no liability for any damage arising from the use of this software.

## Controller (Server)

The main controller is **YAMA.exe**, which functions as the server. It is based on IOCP communication and supports tens of thousands of concurrent connections. Only one instance can run per machine in the Release version.

Below are interface previews of the controller program. All features are stable and functional.  
Note: Some features require the client (controlled program) to run with administrator privileges.

![Main Interface](./images/Yama.jpg)

The main window displays a list of connected clients.  
Select a client to perform remote operations such as editing notes, setting up proxy mappings, or executing code.

![Terminal Management](./images/Console.jpg)

**Terminal Management** opens a command line interface to execute remote commands.  
A [minimal version](./linux/main.cpp) is available with Linux client support for research purposes.

![Process Management](./images/Process.jpg)

**Process Management** shows all running processes on the remote machine.  
You can start or stop regular processes (not high-privileged ones).

![Window Management](./images/Window.jpg)

**Window Management** displays currently open windows or programs on the remote machine, allowing you to hide or show them.

![Desktop Control](./images/Remote.jpg)  
![Desktop Settings](./images/RemoteSet.jpg)

**Desktop Control** functions as "Remote Desktop" for controlling the remote machine.  
You can configure screenshot capture methods (GDI, DXGI, or VIRTUAL) and compression algorithms (grayscale, screen-diff, H264).  
"VIRTUAL" enables a virtual desktop running in the background, improving smoothness.  
Additionally, it supports reporting the active window and detecting specific software.

![File Management](./images/FileManage.jpg)

**File Management** handles file transfer between the local and remote machine.

![Voice Management](./images/Voice.jpg)

**Voice Management** allows you to listen to the remote machine’s audio or send audio if a device is available.

![Video Management](./images/Video.jpg)

**Video Management** enables webcam access on the remote machine.  
If enabled in settings, the controller will show whether a webcam is present.

![Service Management](./images/Service.jpg)

**Service Management** lists services on the remote machine.  
If permitted, you can start, stop, or manage services.

![Registry Management](./images/Register.jpg)

**Registry Management** provides view-only access to the remote machine's registry.

## Linux Client

![LinuxClient](./images/LinuxClient.png)

A Linux client is available under the [v1.0.8](./Releases/v1.0.8/ghost) directory, currently supporting only terminal commands.

![Build Dialog](./images/BuildDlg.jpg)

Compile the client under a Linux environment, then use the server build dialog to select the file and set connection info for generating a Linux version.

## Licensing & Authorization

![Auth Dialog](./images/AuthDlg.jpg)  
![Password Generator](./images/PasswordGen.jpg)

Starting from v1.0.8, operating the controller requires authorization.  
Newly compiled programs have a 14-day trial period. After expiration, generating clients requires a **serial number** to obtain an **authorization token**.  

To bypass the authorization logic, refer to the `OnOnlineBuildClient` function and recompile the program. See issue:  
[#91](https://github.com/yuanyuanxiang/SimpleRemoter/issues/91)  
The token includes the authorization period and enforces one-machine-one-code rules.  
The logic also detects date tampering. Token generation requires a password.

## Controlled Program (Client)

![Main Interface](./images/TestRun.jpg)

The controlled program acts as the **Client**, with two available formats:

1. A standalone program `ghost.exe`  
2. A combo format `TestRun.exe + ServerDll.dll`

- Format (1) is self-contained with no external dependencies.  
- Format (2) runs the EXE, which invokes a core DLL.

Note: Since [v1.0.8](https://github.com/yuanyuanxiang/SimpleRemoter/releases/tag/v1.0.0.8), `TestRun.exe` loads DLLs into memory on demand from the controller, which supports hot code updates.

---

# Changelog

For changes before 2025, see: [history](./history.md)

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

---

# Other Projects

- [HoldingHands](https://github.com/yuanyuanxiang/HoldingHands): A remote control program with a fully English interface and a different architectural design.
- [BGW RAT](https://github.com/yuanyuanxiang/BGW_RAT): A fully featured remote access tool, also known as Big Grey Wolf 9.5.
- [Gh0st](https://github.com/yuanyuanxiang/Gh0st): Another remote controller based on the original Gh0st RAT.

---

# Feedback & Contact

QQ: 962914132

Contact: [Telegram](https://t.me/doge_grandfather), [Email](mailto:yuanyuanxiang163@gmail.com), [LinkedIn](https://www.linkedin.com/in/wishyuanqi)

Issue Reporting: [Issues](https://github.com/yuanyuanxiang/SimpleRemoter/issues)

Contributions welcome: [Merge requests](https://github.com/yuanyuanxiang/SimpleRemoter/pulls)

## Sponsorship

This project stems from technical exploration and personal interest. Updates are made on a non-regular basis, depending on available spare time.  
**If you find this project useful, please consider supporting it via the sponsor icon.**  
If you'd prefer to sponsor using other methods (e.g., WeChat, Alipay or PayPal), please click  
[here](https://github.com/yuanyuanxiang/yuanyuanxiang/blob/main/images/QR_Codes.jpg).
