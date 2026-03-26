# 崩溃保护与 MTBF 统计设计文档

## 概述

本文档描述服务端代理进程的崩溃保护机制和可靠性统计功能。

## 功能目标

1. **崩溃保护**：防止代理进程连续崩溃导致的无限重启循环
2. **崩溃统计**：记录崩溃次数、时间、退出代码等信息
3. **MTBF 计算**：收集数据用于计算平均故障间隔时间（Mean Time Between Failures）

---

## 一、崩溃保护机制

### 1.1 触发条件

| 参数 | 值 | 说明 |
|------|-----|------|
| 窗口期 | 5 分钟 | `CRASH_WINDOW_MS = 300000` |
| 阈值 | 3 次 | `CRASH_THRESHOLD = 3` |
| 快速崩溃定义 | 30 秒内退出 | `FAST_CRASH_TIME_MS = 30000` |

**触发逻辑**：在 5 分钟窗口期内，代理进程发生 3 次快速崩溃（启动后 30 秒内异常退出），则触发崩溃保护。

### 1.2 判定标准

只有同时满足以下条件才计入崩溃：
- 退出代码 ≠ 0（正常退出不计入）
- 运行时间 < 30 秒（长时间运行后退出不计入）

**设计原则**：程序正常退出必须返回 0，任何非 0 退出代码都视为异常退出。包括：
- Windows 异常（如 `0xC0000005` ACCESS_VIOLATION）
- 自定义错误码（如 `1001` CONFIG_ERROR）
- 其他非零退出码

### 1.3 保护行为

触发崩溃保护后：
1. 服务停止自动重启代理进程
2. 写入 `AgentCrashProtected = 1` 到配置
3. 下次程序启动时检测到此标志，切换到普通运行模式
4. 弹出提示框告知用户

### 1.4 状态持久化

崩溃窗口状态持久化到注册表，确保服务重启后状态不丢失：

配置 section: `[crash]`

| 配置键 | 类型 | 说明 |
|--------|------|------|
| `winCount` | int | 窗口期内崩溃次数 |
| `winStart` | string (uint64) | 窗口起始时间 (GetTickCount64) |

**服务重启时的恢复逻辑**：

```
加载保存的状态
↓
检查 savedFirstCrashTime <= 当前时间？
├─ 否 → 系统重启过，清除状态
└─ 是 → 检查是否仍在 5 分钟窗口内？
         ├─ 否 → 窗口已过期，清除状态
         └─ 是 → 恢复状态，继续计数
```

**注意**：`GetTickCount64()` 在系统重启后会重置为 0，因此如果保存的时间戳大于当前时间，说明系统重启过，需要清除状态重新开始。

---

## 二、崩溃统计

### 2.1 记录内容

每次崩溃时记录以下信息（section: `[crash]`）：

| 配置键 | 类型 | 说明 | 示例 |
|--------|------|------|------|
| `count` | int | 总崩溃次数 | `15` |
| `lastTime` | string | 最后崩溃时间 | `"2024-01-15 10:30:45"` |
| `lastCode` | string | 最后退出代码 | `"0xC0000005 (ACCESS_VIOLATION)"` |
| `lastRunMs` | string (uint64) | 最后运行时间(ms) | `"25000"` |

### 2.2 退出代码描述

常见的 Windows 异常代码：

| 代码 | 描述 |
|------|------|
| `0xC0000005` | ACCESS_VIOLATION（访问违规） |
| `0xC0000094` | INTEGER_DIVIDE_BY_ZERO（整数除零） |
| `0xC00000FD` | STACK_OVERFLOW（栈溢出） |
| `0xC0000409` | STACK_BUFFER_OVERRUN（栈缓冲区溢出） |
| `0xC000001D` | ILLEGAL_INSTRUCTION（非法指令） |
| `0x80000003` | BREAKPOINT（断点） |

正常退出：

| 代码 | 描述 |
|------|------|
| `0` | NORMAL（正常退出） |

自定义错误代码（1000-1999）：

| 代码 | 描述 |
|------|------|
| `1001` | CONFIG_ERROR（配置错误） |
| `1002` | NETWORK_ERROR（网络错误） |
| `1003` | AUTH_FAILED（认证失败） |
| `1004` | RESTART_REQUEST（请求重启） |
| `1005` | MANUAL_STOP（手动停止） |

---

## 三、MTBF 统计

### 3.1 数据收集

| 配置键 | 类型 | 说明 |
|--------|------|------|
| `starts` | int | 启动次数 |
| `totalRunMs` | string (uint64) | 累计运行时间(ms) |
| `count` | int | 崩溃次数 |

**注意**：`totalRunMs` 和 `lastRunMs` 使用字符串存储 64 位整数，避免 32 位整数的限制（最大约 49 天）。

### 3.2 计算公式

**MTBF（平均故障间隔时间）**：
```
MTBF = 累计运行时间 / 崩溃次数
```

**失败率（Failure Rate）**：
```
失败率 = 崩溃次数 / 启动次数
```

### 3.3 示例

假设：
- 启动次数：100 次
- 崩溃次数：5 次
- 累计运行时间：30 天

计算结果：
- MTBF = 30 天 / 5 = 6 天（平均每 6 天发生一次崩溃）
- 失败率 = 5 / 100 = 5%（每次启动有 5% 概率崩溃）

---

## 四、回调函数设计

### 4.1 回调类型

| 回调 | 触发时机 | 参数 |
|------|---------|------|
| `onAgentStart` | 代理进程启动成功 | processId, sessionId |
| `onAgentExit` | 代理进程退出（任何原因） | exitCode, runtimeMs |
| `onCrash` | 检测到快速崩溃 | exitCode, runtimeMs |
| `onCrashWindowChange` | 崩溃窗口状态变化 | crashCount, firstCrashTime |
| `onCrashProtection` | 触发崩溃保护 | 无 |

### 4.2 调用顺序

正常退出：
```
onAgentExit(exitCode=0, runtime)
```

快速崩溃：
```
onAgentExit(exitCode, runtime)
    ↓
onCrash(exitCode, runtime)
    ↓
[如果 count >= 3]
onCrashProtection()
    ↓
onCrashWindowChange(count, startTime)
```

---

## 五、配置键汇总

Section: `[crash]`

| 配置键 | 类型 | 用途 | 持久性 |
|--------|------|------|--------|
| `count` | int | 总崩溃次数 | 永久 |
| `lastTime` | string | 最后崩溃时间 | 永久 |
| `lastCode` | string | 最后退出代码 | 永久 |
| `lastRunMs` | string | 最后运行时间 | 永久 |
| `totalRunMs` | string | 累计运行时间 | 永久 |
| `starts` | int | 启动次数 | 永久 |
| `winCount` | int | 窗口期崩溃次数 | 临时* |
| `winStart` | string | 窗口起始时间 | 临时* |
| `protected` | int | 崩溃保护标志 | 一次性** |

\* 临时：窗口期过期或系统重启后自动清除
\** 一次性：程序启动时读取并清除

---

## 六、文件改动

| 文件 | 改动内容 |
|------|---------|
| `CrashReport.h` | 配置键定义、退出代码常量、MTBF 计算函数 |
| `ServerSessionMonitor.h` | 回调类型定义、监控器结构体 |
| `ServerSessionMonitor.cpp` | 崩溃检测逻辑、回调调用 |
| `ServerServiceWrapper.cpp` | 回调实现、状态持久化/恢复 |
| `2015Remote.cpp` | 启动时检查崩溃保护标志 |

---

## 七、使用场景示例

### 场景 1：连续崩溃

```
T=00:00  代理启动 (startCount=1)
T=00:05  代理崩溃 (crashCount=1, 窗口开始)
T=00:10  代理重启 (startCount=2)
T=00:15  代理崩溃 (crashCount=2)
T=00:20  代理重启 (startCount=3)
T=00:25  代理崩溃 (crashCount=3, 触发保护!)
         → 服务停止重启代理
         → 写入 AgentCrashProtected=1
T=00:30  用户手动启动程序
         → 检测到保护标志，切换到普通模式
         → 弹出提示框
```

### 场景 2：服务重启后恢复

```
T=00:00  代理崩溃 (crashCount=1)
T=00:30  代理崩溃 (crashCount=2, 保存到注册表)
T=01:00  服务重启（非系统重启）
         → 加载状态: count=2, elapsed=1分钟
         → 仍在5分钟窗口内，恢复状态
T=01:30  代理崩溃 (crashCount=3, 触发保护!)
```

### 场景 3：窗口过期

```
T=00:00  代理崩溃 (crashCount=1)
T=02:00  代理崩溃 (crashCount=2)
T=08:00  代理崩溃
         → 距离第一次崩溃已超过5分钟
         → 重置窗口: crashCount=1, 新窗口开始
```

---

## 八、设计决策记录

| 问题 | 决策 | 理由 |
|------|------|------|
| 窗口期时长 | 5 分钟，不可配置 | 足够检测连续崩溃，避免误判 |
| 崩溃阈值 | 3 次，不可配置 | 平衡敏感度和容错 |
| 快速崩溃定义 | 30 秒 | 覆盖大多数初始化场景 |
| 系统重启处理 | 清除窗口状态 | 系统重启后问题可能已修复 |
| 统计数据重置 | 暂不实现 | 需求不明确，后续按需添加 |
| 日志记录 | 使用 Mprintf | 与现有日志系统一致 |
| 非零退出码 | 都算崩溃 | 正常退出必须返回 0 |
| lastRunMs 类型 | string (uint64) | 支持超过 24 天的运行时间 |
