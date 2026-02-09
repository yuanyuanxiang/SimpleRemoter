# SimpleRemoter Linux Client

SimpleRemoter 的 Linux 客户端，支持远程桌面、远程终端、文件管理和进程管理功能。

## 功能特性

- **远程桌面** - 实时屏幕截图传输，支持鼠标/键盘控制
- **远程终端** - 基于 PTY 的交互式 Shell
- **文件管理** - 远程文件浏览、上传、下载
- **进程管理** - 查看和管理远程进程
- **守护进程模式** - 支持后台运行 (`-d` 参数)

## 系统要求

### 显示服务器

| 类型 | 支持状态 | 说明 |
|------|---------|------|
| X11 / Xorg | 支持 | 完全支持 |
| XWayland | 部分支持 | X11 应用可用，原生 Wayland 应用不可见 |
| Wayland (纯) | 不支持 | 无法工作 |

> **重要**: 本客户端使用 X11 API 进行屏幕捕获，**不支持纯 Wayland 环境**。
> 如果你使用 GNOME/KDE 等桌面环境，请在登录时选择 "Xorg" 或 "X11" 会话。

### 依赖库

#### 必需 (远程桌面功能)

| 库 | 包名 (Debian/Ubuntu) | 包名 (RHEL/Fedora) | 用途 |
|----|---------------------|-------------------|------|
| libX11 | `libx11-6` | `libX11` | X11 核心库，屏幕捕获 |

#### 推荐 (完整远程控制)

| 库 | 包名 (Debian/Ubuntu) | 包名 (RHEL/Fedora) | 用途 |
|----|---------------------|-------------------|------|
| libXtst | `libxtst6` | `libXtst` | XTest 扩展，模拟鼠标/键盘输入 |

#### 可选

| 库 | 包名 (Debian/Ubuntu) | 包名 (RHEL/Fedora) | 用途 |
|----|---------------------|-------------------|------|
| libXss | `libxss1` | `libXScrnSaver` | 获取用户空闲时间 |

### 一键安装依赖

**Debian / Ubuntu:**
```bash
sudo apt update
sudo apt install libx11-6 libxtst6 libxss1
```

**RHEL / CentOS / Fedora:**
```bash
sudo dnf install libX11 libXtst libXScrnSaver
```

**Arch Linux:**
```bash
sudo pacman -S libx11 libxtst libxss
```

## 编译

### 编译依赖

```bash
# Debian/Ubuntu
sudo apt install build-essential cmake

# RHEL/Fedora
sudo dnf install gcc-c++ cmake make
```

### 编译步骤

```bash
cd linux
cmake .
make -j$(nproc)
```

编译成功后生成可执行文件 `ghost`。

## 使用方法

### 基本用法

```bash
./ghost [服务器IP] [端口]
```

### 守护进程模式

```bash
./ghost -d [服务器IP] [端口]
```

### 示例

```bash
# 前台运行，连接到 192.168.1.100:6543
./ghost 192.168.1.100 6543

# 后台守护进程模式
./ghost -d 192.168.1.100 6543

# 停止守护进程
kill $(cat ~/.config/ghost/ghost.pid)
```

## 常见问题

### Q: 远程桌面功能不工作

**检查项:**

1. **确认使用 X11 会话**
   ```bash
   echo $XDG_SESSION_TYPE
   # 应输出 "x11"，如果是 "wayland" 则不支持
   ```

2. **确认 DISPLAY 环境变量已设置**
   ```bash
   echo $DISPLAY
   # 应输出类似 ":0" 或 ":1"
   ```

3. **确认 X11 库已安装**
   ```bash
   ldconfig -p | grep libX11
   # 应输出 libX11.so.6 的路径
   ```

### Q: 鼠标/键盘控制不工作

安装 XTest 扩展库:
```bash
# Debian/Ubuntu
sudo apt install libxtst6

# RHEL/Fedora
sudo dnf install libXtst
```

### Q: 警告 "MIT-SCREEN-SAVER missing"

这是一个无害警告，表示无法获取用户空闲时间。可以忽略，或安装 libXss:
```bash
# Debian/Ubuntu
sudo apt install libxss1

# RHEL/Fedora
sudo dnf install libXScrnSaver
```

### Q: 如何在 Wayland 环境下使用?

目前不支持纯 Wayland。需要切换到 X11 会话。

**方法1：登录时选择 X11 会话**

- 在登录界面点击用户名后，右下角会出现齿轮图标 ⚙️
- 点击齿轮，选择 "Ubuntu on Xorg" 或 "GNOME on Xorg"

**方法2：强制禁用 Wayland（推荐）**

如果找不到齿轮图标，可以修改 GDM 配置强制使用 X11：

```bash
sudo nano /etc/gdm3/custom.conf
```

在 `[daemon]` 部分添加：

```ini
[daemon]
WaylandEnable=false
```

保存后重启系统：

```bash
sudo reboot
```

这样系统将始终使用 X11，无需每次手动选择。

**KDE 桌面环境：**
- 登录界面选择 "Plasma (X11)"
- 或编辑 `/etc/sddm.conf` 设置默认会话

### Q: 如何检查当前会话类型?

```bash
# 查看会话类型
echo $XDG_SESSION_TYPE

# 查看显示服务器信息
loginctl show-session $(loginctl | grep $(whoami) | awk '{print $1}') -p Type
```

## 配置文件

配置文件位于 `~/.config/ghost/config.ini`，存储以下信息:

- 首次安装时间
- 公网 IP 缓存
- 地理位置缓存

## 技术细节

### 屏幕捕获流程

1. 使用 `XCopyArea` 将 root window 拷贝到离屏 Pixmap
2. 使用 `XGetImage` 从 Pixmap 获取图像数据
3. 转换为 BGRA 格式并翻转行序 (BMP 格式要求)
4. 计算帧差异，仅传输变化区域

### 输入模拟

使用 XTest 扩展 (`libXtst`) 实现:
- `XTestFakeMotionEvent` - 鼠标移动
- `XTestFakeButtonEvent` - 鼠标点击
- `XTestFakeKeyEvent` - 键盘输入

### 为何不支持 Wayland?

Wayland 出于安全考虑，禁止应用程序:
- 捕获其他应用的屏幕内容
- 模拟全局输入事件

这些限制使得传统远程桌面方案无法在纯 Wayland 下工作。
