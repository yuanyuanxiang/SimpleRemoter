# 🌐 Language | 语言

**[🇺🇸 English](./ReadMe_EN.md) | [🇨🇳 中文](./ReadMe.md)**

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

# 📚 Table of Contents

- [1. Project Overview](#1-project-overview)
- [2. Legal Disclaimer](#2-legal-disclaimer)
- [3. System Architecture](#3-system-architecture)
  - [3.1 Master Controller](#31-master-controller)
  - [3.2 Controlled Client](#32-controlled-client)
- [4. Deployment Methods](#4-deployment-methods)
  - [4.1 Intranet Deployment](#41-intranet-deployment)
  - [4.2 Internet Deployment](#42-internet-deployment)
- [5. Changelog](#5-changelog)
- [6. Related Projects](#6-related-projects)
- [7. Feedback & Contact](#7-feedback--contact)

---

# 1. Project Overview

**Original Source:** [zibility](https://github.com/zibility/Remote)

**Feature Summary:** A remote control system based on Gh0st, providing the following core features:
- Terminal management, process management, window management
- Desktop management, file management, audio management, video management
- Service management, registry management, keylogging
- SOCKS proxy, virtual desktop, remote code execution

If you are interested in remote control technology and find this project useful, feel free to add a Star. Fork, Watch, submit Issues, or open Pull Requests are all welcome. The author will address issues and maintain the project during spare time.

[![Star History Chart](https://api.star-history.com/svg?repos=yuanyuanxiang/SimpleRemoter&type=Date)](https://star-history.com/#yuanyuanxiang/SimpleRemoter&Date)

<span style="color:#FF5722; font-weight:bold;">*This program is for educational and technical research purposes only. Users are fully responsible for any consequences arising from its usage.*</span>

**Project Purpose:** This project is suitable for remote work, remote device management, remote assistance, and other legitimate use cases.

**Initial Release Date:** January 1, 2019

**Compilation Method:** Please use **git clone** to get the project code. Downloading as a ZIP file may result in UNIX-style line endings, which can cause unexpected compilation issues.

This project is developed and maintained using **VS2019**, and is also compatible with newer compilers such as **VS2022/VS2026**. For details, see [#171](https://github.com/yuanyuanxiang/SimpleRemoter/issues/171).

When using newer compilers, you may encounter dependency library conflicts. For solutions, refer to [#269](https://github.com/yuanyuanxiang/SimpleRemoter/issues/269).

The master controller may display garbled text on non-Chinese systems. For solutions, refer to [#157](https://github.com/yuanyuanxiang/SimpleRemoter/issues/157).

**Code Style:**

```cmd
for /R %F in (*.cpp *.h *.c) do astyle --style=linux "%F"
```

# 2. Legal Disclaimer

This project is a research-oriented implementation of remote control technology and is intended solely for legal and educational use. **Strictly prohibited**: any illegal access, control, or monitoring of others' devices.

This software is provided "as is" without any express or implied warranties. Use of this software is at your own risk. The developer is not liable for any illegal or malicious use of this software. Users must comply with applicable laws in their jurisdiction and use the software responsibly. The developer assumes no responsibility for any direct or indirect damages caused by use of this software.

# 3. System Architecture

![Architecture](https://github.com/yuanyuanxiang/SimpleRemoter/wiki/res/Architecture.jpg)

This program (since v1.1.1) adopts a two-tier architecture design:

1. Superusers are responsible for distributing and managing master controllers
2. Each master controller manages its own set of controlled clients

This architecture has the following characteristics:

- **Hierarchical Control**: Using subordinate master controllers as relays, superusers can control any computer in the entire system
- **Isolation Mechanism**: Computers managed by different master controllers (Master 1, 2, 3, etc.) are isolated from each other; each master can only control clients generated by itself
- **Centralized Authorization**: Authorization for all master controllers is managed uniformly by superusers

**Important Warning: Unauthorized control of other users' devices is strictly prohibited.**

## 3.1 Master Controller

The master controller executable is **YAMA.exe**. It serves as the Server side, based on IOCP communication model, supporting tens of thousands of clients online concurrently. Thanks to the layered architecture, the supported number of hosts can increase by an order of magnitude. For example, if one superuser manages 10 masters, and each master handles 10,000 clients, the superuser can control up to 100,000 clients.

The following showcases the master controller's interface. All features are available and stable. Some operations require the client to run with administrator privileges.

![Main UI](./images/Yama.jpg)

The main UI displays all connected clients in a list format. After selecting a host, you can perform remote control operations such as modifying notes, proxy mapping, or executing code.

![Terminal Management](./images/Console.jpg)

**Terminal Management**: Opens a command-line window to execute remote commands. A [minimal version](./linux/main.cpp) is also available that supports Linux clients for Linux developers to research.

![Process Management](./images/Process.jpg)

**Process Management**: Displays running processes on the controlled machine. You can start/stop normal processes or inject code (cannot operate on high-privilege processes).

![Window Management](./images/Window.jpg)

**Window Management**: Displays open windows or programs on the controlled machine. Supports maximize, minimize, hide, or show window operations.

![Desktop Management](./images/Remote.jpg)
![Desktop Settings](./images/RemoteSet.jpg)

**Desktop Management**: The "Remote Desktop" feature for controlling remote machines. You can configure remote desktop parameters via the menu:
- **Screen Capture Method**: Supports GDI, DXGI, or VIRTUAL (virtual desktop)
- **Image Compression Method**: Supports grayscale image transmission, screen diff algorithm, and H.264 compression

The VIRTUAL (virtual desktop) mode can run the remote desktop program in the background on the controlled computer, optimized for smooth operation. Additionally, it supports reporting active windows and detecting specified software. Screen transmission is deeply optimized with SSE2 and multi-threading, achieving up to 30 FPS across networks. Remote desktop control supports multiple monitors and video wall display, as well as privacy screen. If the client runs as a Windows service, it supports remote control during lock screen and password input to unlock. Supports using Ctrl+C and Ctrl+V to transfer files between master and client.

![File Management](./images/FileManage.jpg)

**File Management**: Transfer files between local machine and controlled machine.

![Voice Management](./images/Voice.jpg)

**Voice Management**: Monitor sound from the controlled machine, or send voice to the controlled computer (requires audio devices on the controlled machine).

![Video Management](./images/Video.jpg)

**Video Management**: Opens the controlled machine's webcam. Camera detection is enabled by default in settings, and the host list will display camera status.

![Service Management](./images/Service.jpg)

**Service Management**: View the service list on the controlled machine. If permissions allow, you can start, stop, or perform other operations on services.

![Registry Viewer](./images/Register.jpg)

**Registry Management**: View the registry on the controlled machine (read-only, modification not supported).

**Other Features**

- **Machine Management**: Execute logout, shutdown, or restart operations on controlled hosts
- **Client Management**: Transfer or share clients, supports setting expiration dates with automatic recovery after expiration
- **Client Proxy**: Enable proxy mapping for SOCKS proxy via client; or use FRP to proxy specific ports (e.g., 3389) on the client, enabling remote desktop control via mstsc.exe
- **Execute Program**: Client downloads and executes EXE programs from remote, or upload EXE programs from master to client for execution
- **Execute Code**: Write a DLL that conforms to specifications, and it can be transmitted to the client for execution with high flexibility. The client caches this DLL to avoid repeated transmission

**About Licensing:**

Since v1.0.8, operating the master controller requires authorization. Newly compiled programs have a 14-day trial period, after which generating clients requires a "serial number" to apply for an unlock code. To disable this authorization logic, refer to the `OnOnlineBuildClient` function and recompile the program. See: [#91](https://github.com/yuanyuanxiang/SimpleRemoter/issues/91). The "unlock code" includes an authorization date range, ensuring one code per machine; the authorization logic checks whether the computer date has been tampered with. Generating an unlock code requires a password.

![Authorization Dialog](./images/AuthDlg.jpg)

![Password Generator](./images/PasswordGen.jpg)

Since v1.1.1, authorization for newly compiled programs has been revoked. Anyone using this program must compile it themselves, otherwise a dialog will appear after 10 minutes of use requiring an unlock code. The introduction of this authorization mechanism can keep users without programming experience out. If you just want to experience this remote control program, we recommend using v1.0.7 or earlier versions, as their core functionality is essentially the same as later versions. If you are interested in the technology, we believe you have sufficient ability to compile the program yourself.

Since v1.2.0, the master controller must obtain authorization, otherwise functionality will be limited. Authorization methods support binding by computer or by domain name. Attempts to bypass authorization may cause the master controller to stop working. Master controllers that have obtained authorization will automatically disconnect from the authorization server. If authorization is unsuccessful, the connection to the authorization server will be maintained along with necessary data exchange—this mechanism may be defined as a "backdoor", but it is necessary and cannot be bypassed.

## 3.2 Controlled Client

![Client UI](./images/TestRun.jpg)

The controlled client is the Client side, supporting two running forms:

1. **Standalone Program Form**: ghost.exe runs as a single program without depending on other dynamic link libraries
2. **Separated Loading Form**: TestRun.exe + ServerDll.dll, where the EXE program calls the core dynamic link library

Note: Since [v1.0.8](https://github.com/yuanyuanxiang/SimpleRemoter/releases/tag/v1.0.0.8), `TestRun.exe` uses memory-loaded DLL execution, requesting the DLL from the master controller and executing it in memory, which facilitates hot code updates.

---

# 4. Deployment Methods

## 4.1 Intranet Deployment

Intranet deployment means the master controller and controlled devices are within the same local network, where controlled devices can directly connect to the master's address. This deployment method is very simple—just fill in the master's IP and port when generating the client.

## 4.2 Internet Deployment

Internet deployment means the master controller and controlled programs are on different networks, where the master device has no public IP and controlled devices cannot directly connect to the master's address. In this case, a "middleman" is needed to tunnel the master's internal IP. One method is using [Peanuthull](./使用花生壳.txt), which won't be covered here. For cross-border/international remote control, we strongly recommend using a VPS instead of Peanuthull.

This document introduces the second method, which works similarly:

```
Client ──> VPS ──> Master
```

Use a VPS (Virtual Private Server) as the "middleman" to control remote devices. You can also use a physical server, but VPS is more cost-effective. Typically you need to purchase VPS service. Communication between VPS and the program is based on [FRP](https://github.com/fatedier/frp).

In this deployment mode, when generating the client program, fill in the VPS IP (some VPS providers offer domain names, which can also be used). Typically, run the FRP server on the VPS and the FRP client locally. When a host connects to the VPS, the VPS forwards the request to the local computer; similarly, control requests are also relayed through the VPS to the controlled host.

For cross-network, cross-border, or international remote control system deployment, see: [Reverse Proxy Deployment Guide](./反向代理.md), which is the method used by the author in practice.

---

# 5. Changelog

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

---

# 6. Related Projects

- [HoldingHands](https://github.com/yuanyuanxiang/HoldingHands): A remote control program with a fully English interface, using a different architectural design
- [BGW RAT](https://github.com/yuanyuanxiang/BGW_RAT): A fully featured remote control program (Big Grey Wolf 9.5)
- [Gh0st](https://github.com/yuanyuanxiang/Gh0st): Another remote control program based on Gh0st

---

# 7. Feedback & Contact

**QQ:** 962914132

**Contact:** [Telegram](https://t.me/doge_grandfather) | [Email](mailto:yuanyuanxiang163@gmail.com) | [LinkedIn](https://www.linkedin.com/in/wishyuanqi)

**Issue Reporting:** [Issues](https://github.com/yuanyuanxiang/SimpleRemoter/issues)

**Contributions Welcome:** [Pull Requests](https://github.com/yuanyuanxiang/SimpleRemoter/pulls)

**Sponsorship:** This project stems from technical learning and personal interest. The author will update irregularly based on spare time. **If this project has been helpful to you, please consider supporting it via the sponsor icon.** If you prefer to sponsor through other methods (e.g., WeChat, Alipay, PayPal), please click [here](https://github.com/yuanyuanxiang/yuanyuanxiang/blob/main/images/QR_Codes.jpg).
