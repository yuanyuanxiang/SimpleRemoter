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

For earlier changes, see: [history](./history.md)

**Release v1.2.3 (2026.1.21):**

This release enhances remote desktop control experience, optimizes client update logic, and fixes several stability issues.

- Feature: Support download payload from http(s) server
- Feature: Refactor ClientList and add dialog to show it
- Feature: Support using remote cursor in screen control
- Improve: Use FRP to proxy payload download request
- Improve: `ExpandDirectories` after `GetForegroundSelectedFiles`
- Improve: Change zstd compression options for some dialog
- Improve: Update zlib to version `1.3.1.2` and use context in decompression
- Improve: Use old shellcode+AES loader to build client for Windows Server
- Improve: Add more features/buttons for remote desktop toolbar
- Improve: Improve remote control `ScreenSpyDlg` reconnect logic
- Improve: Show all windows status in system dialog
- Improve: Add client update logic for client type EXE
- Fix: `GetPort` issue causing not show host offline log
- Fix: OnOnlineUpdate caused by commit 2fb77d5
- Fix: `GetProcessList` can't get some process full path
- Fix: Check time to make reassigned client restore immediately
- Fix: #288 Command line issues
- Fix: Copy payload file to target directory when installing
- Fix: #281 Check if CPU has SSE2 to avoid client crash
- Fix: Remote desktop window restore size issue

**Release v1.2.2 (2026.1.11):**

This release enhances remote desktop settings persistence, file management features, and improves authorization and keyboard forwarding.

- Improve: Save remote desktop screen settings in registry
- Fix: Authorization return failure if date is out of range
- Improve: `GetForegroundSelectedFiles` if `GetClipboardFiles` failed
- Feature: Show screen resolution and client id in popup window
- Feature: Support customizing client name and install directory
- Feature: Add menu to set screen strategy for remote control
- Improve: Showing file transmit progress dialog
- Feature: Support compress files in file management dialog
- Improve: Add F10, WM_SYSKEYDOWN, WM_SYSKEYUP to forward

**Release v1.2.1 (2026.1.1):**

This release enhances remote desktop features (FPS control, screen switching, file drag-and-drop support), improves Windows service mode, and fixes several stability issues.

- Improve building client - append shellcode to file end
- Fix: `TestRun` run as Windows service can't unlock screen
- Feature: Add FPS control menu for remote desktop
- Fix: Exist-check include checking client IP in `AddList`
- Revert #242 and improve security when sending files to client
- Fix switch screen and support dragging files to remote
- Fix #266: CloseHandle close an invalid handle
- Improve: Add more system hot key to forward
- Feature: Add menu to control if run master as service
- Improve `ToolbarDlg` slide in / slide out performance


---

# 6. Related Projects

- [HoldingHands](https://github.com/yuanyuanxiang/HoldingHands): A remote control program with a fully English interface, using a different architectural design
- [BGW RAT](https://github.com/yuanyuanxiang/BGW_RAT): A fully featured remote control program (Big Grey Wolf 9.5)
- [Gh0st](https://github.com/yuanyuanxiang/Gh0st): Another remote control program based on Gh0st

---

# 7. Feedback & Contact

**Tencent QQ:** Please contact me via 962914132

**Contact:** [Telegram](https://t.me/doge_grandfather) | [Email](mailto:yuanyuanxiang163@gmail.com) | [LinkedIn](https://www.linkedin.com/in/wishyuanqi)

**Issue Reporting:** [Issues](https://github.com/yuanyuanxiang/SimpleRemoter/issues)

**Contributions Welcome:** [Pull Requests](https://github.com/yuanyuanxiang/SimpleRemoter/pulls)

**Sponsorship:** This project stems from technical learning and personal interest. The author will update irregularly based on spare time. **If this project has been helpful to you, please consider supporting it via the sponsor icon.** If you prefer to sponsor through other methods (e.g., WeChat, Alipay, PayPal), please click [here](https://github.com/yuanyuanxiang/yuanyuanxiang/blob/main/images/QR_Codes.jpg).
