这是一份针对你这个 **CPU Watchdog 方案**的完整说明文档，方便你理解每个环节在干什么、为什么这么设计，以及后续怎么维护。

---

## 📌 CPU Watchdog 是什么

一个**后台守护脚本**，每 N 秒扫描一次系统所有进程，把 **CPU 使用率超过阈值的进程** 自动用 `cpulimit` 限到指定百分比。同时记录"黑名单"，开机后自动对历史记录过的进程重新施加限制。

**适用场景**：Oracle 免费机 / 低配 VPS，防止某个进程跑飞把整台机器卡死，同时保护 sshd / nginx / mysql 等关键服务不被误杀。

---

## 📁 文件结构

| 文件 | 作用 |
|---|---|
| `/usr/local/sbin/cpu_watchdog.sh` | 主脚本（核心逻辑） |
| `/etc/cpu_watchdog/limited_procs.conf` | 被限制过的进程记录（黑名单） |
| `/var/log/cpu_watchdog.log` | 运行日志 |
| `/etc/systemd/system/cpu-watchdog.service` | systemd 自启服务 |

---

## ⚙️ 工作原理

```
┌─────────────────────────────────────────────────────────┐
│  systemd 启动 cpu-watchdog.service                      │
│       │                                                 │
│       ├── ExecStart: 主循环 (loop)                       │
│       │     每 5 秒:                                    │
│       │     ps -eo pid,comm,pcpu                        │
│       │       │                                         │
│       │       ├── CPU > 10% ?                           │
│       │       │     ├── 在白名单? → 跳过                 │
│       │       │     ├── 已被 cpulimit? → 跳过            │
│       │       │     └── 否则 → cpulimit -p PID -l 10%   │
│       │       │           记录到 limited_procs.conf      │
│       │       └── CPU ≤ 10% → 不管                      │
│       │                                                 │
│       └── ExecStartPost (延迟30秒): 恢复模式 (restore)    │
│             读取 limited_procs.conf                      │
│             对历史记录过的进程名重新施加 cpulimit          │
└─────────────────────────────────────────────────────────┘
```

---

## 🛡️ 白名单机制

脚本里硬编码了 `WHITELIST` 变量，以下进程**永远不会被限制**：

| 类别 | 进程名 |
|---|---|
| 内核线程 | systemd, kthreadd, kworker, migration, rcu, watchdog, idle |
| SSH | sshd, ssh |
| 日志 | systemd-journal, rsyslog, journald |
| Web/DB | nginx, mysql |
| 其他 | （可按需追加） |

匹配方式是 **`/proc/PID/comm` 的进程名**（截断到 15 字符），用正则精确匹配。

> ⚠️ **当前白名单偏短**，在宝塔/Oracle Linux 环境下可能误杀 dnf、python3（宝塔面板自身）、NetworkManager 等。建议替换为上一轮给的 v3 扩展版白名单。

---

## 📋 配置参数说明

| 参数 | 默认值 | 含义 |
|---|---|---|
| `LIMIT_PCT` | `10` | CPU 超过此百分比就触发限制，限制到该值 |
| `CHECK_INTERVAL` | `5` | 检测间隔（秒） |
| `CONF_DIR` | `/etc/cpu_watchdog` | 配置目录 |
| `LOG_FILE` | `/var/log/cpu_watchdog.log` | 日志路径 |

---

## 🔧 日常维护命令

### 查看状态
```bash
systemctl status cpu-watchdog
```

### 实时日志
```bash
tail -f /var/log/cpu_watchdog.log
# 或
journalctl -u cpu-watchdog -f
```

### 看哪些进程被限制过
```bash
cat /etc/cpu_watchdog/limited_procs.conf
```

### 手动解除某个进程的限制
```bash
# 1. 从黑名单里删掉
sed -i '/^进程名|/d' /etc/cpu_watchdog/limited_procs.conf

# 2. 杀掉对应的 cpulimit 进程
pkill -f "cpulimit.*进程名"
```

### 停止 / 启动 / 重启
```bash
systemctl stop cpu-watchdog      # 停止
systemctl start cpu-watchdog     # 启动
systemctl restart cpu-watchdog   # 重启
systemctl disable cpu-watchdog   # 禁止开机自启
```

### 卸载
```bash
systemctl disable --now cpu-watchdog
rm -f /usr/local/sbin/cpu_watchdog.sh
rm -f /etc/systemd/system/cpu-watchdog.service
rm -rf /etc/cpu_watchdog
rm -f /var/log/cpu_watchdog.log
systemctl daemon-reload
```

---

## ⚠️ 已知局限 & 注意事项

| 问题 | 说明 | 建议 |
|---|---|---|
| **白名单短** | 只保护了 14 个进程名，宝塔/python3/系统服务可能误杀 | 替换为 v3 扩展白名单 |
| **comm 截断** | `/proc/PID/comm` 只有 15 字符，长进程名截断后可能匹配不上 | 关键进程加完整命令行匹配 |
| **cpulimit 精度** | cpulimit 基于 SIGSTOP/SIGCONT，精度 ±2%，不适合硬实时场景 | 免费机够用 |
| **进程重启** | 新 fork 的进程要等下一个检测周期才会被限制 | restore 模式兜底，但有 30 秒窗口 |
| **bc 依赖** | 浮点比较用 `bc -l`，没装会报错 | 脚本已处理 fallback（默认返回 0） |

---

## 🚀 快速部署（复制粘贴版）

你贴的那段命令本身就是一键部署，按顺序执行就行：

```bash
# 1. 装依赖
sudo apt install -y cpulimit bc

# 2-4. 写脚本 + 配目录 + 写 service（你贴的那段全部复制执行）

# 5. 启动
sudo systemctl daemon-reload
sudo systemctl enable --now cpu-watchdog.service

# 6. 验证
systemctl status cpu-watchdog
```

---

## 📝 总结一句话

> **CPU Watchdog = 穷人版 cgroup CPUQuota**，用 cpulimit 软限制野进程，保护关键服务不被拖死。适合没有 systemd service 管理的临时脚本/野进程，配合 systemd 原生 `CPUQuota` 一起用效果最好。

**建议下一步**：把白名单换成 v3 扩展版（覆盖宝塔 + OL10 + Ubuntu），避免误杀系统关键进程。需要我直接给你一个替换版的完整脚本吗？