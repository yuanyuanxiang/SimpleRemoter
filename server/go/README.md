# SimpleRemoter Go TCP Server Framework

基于 Go 语言实现的高性能 TCP 服务端框架，用于替代原有的 C++ IOCP 服务端。

## 项目结构

```
server/go/
├── go.mod                    # Go 模块定义
├── auth/
│   └── auth.go              # 授权验证模块 (TOKEN_AUTH + Heartbeat HMAC)
├── buffer/
│   └── buffer.go            # 线程安全的动态缓冲区
├── connection/
│   ├── context.go           # 连接上下文
│   ├── errors.go            # 错误定义
│   └── manager.go           # 连接管理器
├── protocol/
│   ├── parser.go            # 协议解析器
│   ├── codec.go             # 编解码和压缩 (ZSTD)
│   ├── header.go            # 协议头解密 (8种加密方式)
│   └── commands.go          # 命令常量和LOGIN_INFOR解析
├── server/
│   ├── server.go            # TCP 服务器核心
│   └── pool.go              # Goroutine 工作池
├── logger/
│   └── logger.go            # 日志模块 (基于 zerolog)
└── cmd/
    └── main.go              # 程序入口
```

## 核心特性

- **高并发**: 基于 Goroutine 池管理并发连接
- **协议兼容**: 支持原有 C++ 客户端的多种协议标识 (Hell/Hello/Shine/Fuck)
- **协议头解密**: 支持8种协议头加密方式 (V0-V6 + Default)
- **授权验证**: 支持 TOKEN_AUTH 和 Heartbeat HMAC-SHA256 双重授权验证
- **XOR编码**: 支持 XOREncoder16 数据编码/解码
- **ZSTD 压缩**: 使用高效的 ZSTD 算法进行数据压缩
- **GBK编码**: 自动将 Windows 客户端的 GBK 编码转换为 UTF-8
- **线程安全**: Buffer、连接管理器和 LastActive 均为线程安全设计
- **优雅关闭**: 支持信号处理和优雅停机，自动释放资源
- **可配置**: 支持自定义端口、最大连接数、超时时间等
- **日志系统**: 基于 zerolog，支持文件输出、日志轮转、客户端上下线记录

## 支持的命令

当前已实现以下命令处理：

| 命令 | 值 | 说明 |
|------|-----|------|
| TOKEN_AUTH | 100 | 授权请求 (验证 SN + Passcode + HMAC) |
| TOKEN_HEARTBEAT | 101 | 心跳包 (支持 HMAC 授权验证，返回 Authorized 状态) |
| TOKEN_LOGIN | 102 | 客户端登录 |
| CMD_HEARTBEAT_ACK | 216 | 心跳响应 (包含 Authorized 字段) |

其他命令会被记录为 Debug 日志，可按需扩展。

## 快速开始

### 安装依赖

```bash
cd server/go
go mod tidy
```

### 编译

```bash
go build -o simpleremoter-server ./cmd
```

### 运行

```bash
./simpleremoter-server
```

服务器默认监听 6543 端口，日志输出到 `logs/server.log`。

### 环境变量

| 变量 | 说明 | 示例 |
|------|------|------|
| `YAMA_PWDHASH` | 密码的 SHA256 哈希值 (64位十六进制) | `61f04dd6...` |
| `YAMA_PWD` | 超级密码，用于 HMAC 签名验证 | `your_super_password` |

```bash
# Linux/macOS
export YAMA_PWDHASH="61f04dd637a74ee34493fc1025de2c131022536da751c29e3ff4e9024d8eec43"
export YAMA_PWD="your_super_password"
./simpleremoter-server

# Windows PowerShell
$env:YAMA_PWDHASH="61f04dd637a74ee34493fc1025de2c131022536da751c29e3ff4e9024d8eec43"
$env:YAMA_PWD="your_super_password"
.\simpleremoter-server.exe
```

## 使用示例

```go
package main

import (
    "os"
    "os/signal"
    "syscall"

    "github.com/yuanyuanxiang/SimpleRemoter/server/go/connection"
    "github.com/yuanyuanxiang/SimpleRemoter/server/go/logger"
    "github.com/yuanyuanxiang/SimpleRemoter/server/go/protocol"
    "github.com/yuanyuanxiang/SimpleRemoter/server/go/server"
)

// 实现 Handler 接口
type MyHandler struct {
    log *logger.Logger
}

func (h *MyHandler) OnConnect(ctx *connection.Context) {
    h.log.ClientEvent("online", ctx.ID, ctx.GetPeerIP())
}

func (h *MyHandler) OnDisconnect(ctx *connection.Context) {
    h.log.ClientEvent("offline", ctx.ID, ctx.GetPeerIP())
}

func (h *MyHandler) OnReceive(ctx *connection.Context, data []byte) {
    if len(data) == 0 {
        return
    }
    cmd := data[0]
    switch cmd {
    case protocol.TokenLogin:
        info, _ := protocol.ParseLoginInfo(data)
        h.log.Info("Client login: %s (%s)", info.PCName, info.OsVerInfo)
    case protocol.TokenHeartbeat:
        h.log.Debug("Heartbeat from client %d", ctx.ID)
    }
}

func main() {
    // 配置日志 (控制台 + 文件)
    logCfg := logger.DefaultConfig()
    logCfg.File = "logs/server.log"
    log := logger.New(logCfg)

    // 配置服务器
    config := server.DefaultConfig()
    config.Port = 6543

    // 创建并启动服务器
    srv := server.New(config)
    srv.SetLogger(log.WithPrefix("Server"))
    srv.SetHandler(&MyHandler{log: log})

    if err := srv.Start(); err != nil {
        log.Fatal("启动失败: %v", err)
    }

    // 等待退出信号
    sigChan := make(chan os.Signal, 1)
    signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
    <-sigChan

    srv.Stop()
}
```

## 配置选项

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| Port | 8080 | 监听端口 |
| MaxConnections | 10000 | 最大连接数 |
| MinWorkers | 4 | 最小工作协程数 |
| MaxWorkers | 100 | 最大工作协程数 |
| ReadBufferSize | 8192 | 读缓冲区大小 |
| WriteBufferSize | 8192 | 写缓冲区大小 |
| KeepAliveTime | 5min | 连接保活时间 |
| ReadTimeout | 2min | 读超时时间 |
| WriteTimeout | 30s | 写超时时间 |

## 日志配置

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| Level | Info | 日志级别 (Debug/Info/Warn/Error/Fatal) |
| Console | true | 是否输出到控制台 |
| File | "" | 日志文件路径 (空则不写文件) |
| MaxSize | 100 | 单个日志文件最大 MB |
| MaxBackups | 3 | 保留的旧日志文件数量 |
| MaxAge | 30 | 旧日志保留天数 |
| Compress | true | 是否压缩轮转的日志 |

日志示例输出：
```json
{"level":"info","module":"Server","time":"2025-12-19T13:17:32+01:00","message":"Server started on port 6543"}
{"level":"info","module":"Handler","event":"login","client_id":1,"ip":"192.168.0.92","computer":"DESKTOP-BI6RGEJ","os":"Windows 10","version":"Dec 19 2025","time":"2025-12-19T13:17:32+01:00"}
{"level":"debug","module":"Handler","time":"2025-12-19T13:17:47+01:00","message":"Heartbeat from client 1 (DESKTOP-BI6RGEJ)"}
```

## 协议格式

数据包格式与 C++ 版本兼容：

```
+----------+------------+------------+------------------+
|  Flag    | TotalLen   | OrigLen    |  Compressed Data |
| (N bytes)| (4 bytes)  | (4 bytes)  |  (variable)      |
+----------+------------+------------+------------------+
```

### 协议标识

| 标识 | Flag长度 | 压缩方式 | 说明 |
|------|----------|----------|------|
| HELL | 8 bytes | ZSTD | 主要协议 |
| Hello? | 8 bytes | None | 无压缩协议 |
| Shine | 5 bytes | ZSTD | 备用协议 |
| <<FUCK>> | 11 bytes | ZSTD | 备用协议 |

### 协议头加密

支持8种加密方式，服务端自动检测并解密：
- V0 (Default): 动态密钥，4种操作
- V1: 交替加减
- V2: 带旋转的异或
- V3: 带位置的动态密钥
- V4: 对称的伪随机异或
- V5: 带位移的动态密钥
- V6: 带位置的伪随机
- V7: 纯异或

### LOGIN_INFOR 结构

客户端登录信息结构体 (考虑 C++ 内存对齐)：

| 字段 | 偏移 | 大小 | 说明 |
|------|------|------|------|
| bToken | 0 | 1 | 命令标识 (102) |
| OsVerInfoEx | 1 | 156 | 操作系统版本 |
| (padding) | 157 | 3 | 对齐填充 |
| dwCPUMHz | 160 | 4 | CPU 频率 |
| moduleVersion | 164 | 24 | 模块版本 |
| szPCName | 188 | 240 | 计算机名 |
| szMasterID | 428 | 20 | 主控 ID |
| bWebCamExist | 448 | 4 | 是否有摄像头 |
| dwSpeed | 452 | 4 | 网速 |
| szStartTime | 456 | 20 | 启动时间 |
| szReserved | 476 | 512 | 扩展字段 (用`|`分隔) |

### Heartbeat 结构

客户端心跳包结构 (1024 字节)：

| 字段 | 偏移 | 大小 | 说明 |
|------|------|------|------|
| Time | 0 | 8 | 时间戳 (uint64) |
| ActiveWnd | 8 | 512 | 当前活动窗口 |
| Ping | 520 | 4 | 延迟 (int) |
| HasSoftware | 524 | 4 | 软件标识 (int) |
| SN | 528 | 20 | 序列号 (用于授权验证) |
| Passcode | 548 | 44 | 授权码 (格式: v0-v1-v2-v3-v4-v5) |
| PwdHmac | 592 | 8 | HMAC 签名 (uint64) |
| Reserved | 600 | 424 | 保留字段 |

### HeartbeatACK 结构

服务端心跳响应结构 (32 字节)：

| 字段 | 偏移 | 大小 | 说明 |
|------|------|------|------|
| Time | 0 | 8 | 原始时间戳 (uint64) |
| Authorized | 8 | 1 | 授权状态 (1=已授权, 0=未授权) |
| Reserved | 9 | 23 | 保留字段 |

### 授权验证流程

```
客户端 Heartbeat                     服务端
    │                                   │
    │  SN + Passcode + PwdHmac          │
    │ ────────────────────────────────► │
    │                                   │ 1. 验证 Passcode 格式
    │                                   │ 2. 验证 Passcode 哈希
    │                                   │ 3. 验证 HMAC 签名
    │          HeartbeatACK             │
    │ ◄──────────────────────────────── │
    │       (Authorized=1 或 0)         │
```

## API 参考

### Server

```go
// 创建服务器
srv := server.New(config)

// 设置日志
srv.SetLogger(log)

// 设置事件处理器
srv.SetHandler(handler)

// 启动服务器
srv.Start()

// 停止服务器
srv.Stop()

// 发送数据到指定连接
srv.Send(ctx, data)

// 广播数据到所有连接
srv.Broadcast(data)

// 获取当前连接数
count := srv.ConnectionCount()
```

### Connection Context

```go
// 发送数据
ctx.Send(data)

// 关闭连接
ctx.Close()

// 获取客户端 IP
ip := ctx.GetPeerIP()

// 检查连接状态
closed := ctx.IsClosed()

// 获取/更新最后活跃时间 (线程安全)
lastActive := ctx.LastActive()
ctx.UpdateLastActive()
duration := ctx.TimeSinceLastActive()

// 设置/获取客户端信息
ctx.SetInfo(clientInfo)
info := ctx.GetInfo()

// 设置/获取用户数据
ctx.SetUserData(myData)
data := ctx.GetUserData()
```

### Protocol

```go
// 解析登录信息
info, err := protocol.ParseLoginInfo(data)
if err == nil {
    fmt.Println(info.PCName)      // 计算机名
    fmt.Println(info.OsVerInfo)   // 操作系统
    fmt.Println(info.ModuleVersion) // 版本
    fmt.Println(info.WebCamExist) // 是否有摄像头
}

// 获取扩展字段
reserved := info.ParseReserved()  // 返回 []string
clientType := info.GetReservedField(0)  // 客户端类型
cpuCores := info.GetReservedField(2)    // CPU 核数
filePath := info.GetReservedField(4)    // 文件路径
publicIP := info.GetReservedField(11)   // 公网 IP
```

## 与 C++ 版本对比

| 特性 | C++ (IOCP) | Go |
|------|------------|-----|
| 并发模型 | IOCP + 线程池 | Goroutine 池 |
| 压缩算法 | ZSTD | ZSTD |
| 跨平台 | Windows | 全平台 |
| 内存管理 | 手动 | GC |
| 代码复杂度 | 高 | 低 |
| 协议头解密 | 8种方式 | 8种方式 |
| XOR编码 | XOREncoder16 | XOREncoder16 |
| 字符编码 | GBK | GBK -> UTF-8 |

## 依赖

- [github.com/klauspost/compress/zstd](https://github.com/klauspost/compress) - ZSTD 压缩
- [github.com/rs/zerolog](https://github.com/rs/zerolog) - 高性能日志
- [gopkg.in/natefinch/lumberjack.v2](https://github.com/natefinch/lumberjack) - 日志轮转
- [golang.org/x/text](https://pkg.go.dev/golang.org/x/text) - GBK 编码转换

## License

MIT License
