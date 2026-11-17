好，给这个 `cpu_watchdog_deploy.sh` 写一份完整说明文档。

---

## 📌 脚本是什么

`cpu_watchdog_deploy.sh` 是一个**一键部署脚本**，自动安装依赖、生成 CPU Watchdog 守护程序、配置 systemd 自启服务。整个过程交互式选择档位，一条命令完成部署。

**它做的事情**：在后台持续监控所有进程的 CPU 使用率，超过设定阈值就自动用 `cpulimit` 限到该阈值，防止单个进程跑飞拖垮整台机器。

---

## 📁 部署后生成的文件

| 文件 | 作用 |
|---|---|
| `/usr/local/sbin/cpu_watchdog.sh` | 主脚本（watchdog 核心逻辑，由部署脚本自动生成） |
| `/etc/cpu_watchdog/limited_procs.conf` | 被限制过的进程记录（格式：`进程名\|首次发现时间\|命令行摘要`） |
| `/var/log/cpu_watchdog.log` | 运行日志 |
| `/etc/systemd/system/cpu-watchdog.service` | systemd 自启服务单元 |

---

## 🎯 适用场景

- **Oracle 免费机 / 低配 VPS**（1G 内存、1 核）——宿主机超卖严重，st 高，一个进程跑飞直接卡死连不上
- **没有 systemd service 管理的临时脚本/野进程**——cgroup 管不到的，用 cpulimit 兜底
- **宝塔面板环境**——php-fpm 卡死、python3 任务失控时自动限流，不影响 nginx/mysql/sshd
- **配合 systemd `CPUQuota` 使用**——有 service 的走 cgroup 硬限制，没 service 的走 watchdog 软限制，双层防护

---

## 🖥️ 兼容范围

### 发行版

| 家族 | 覆盖 |
|---|---|
| RHEL 系 | Oracle Linux 7/8/9/10、Rocky、Alma、CentOS 7/8、Fedora、Amazon Linux |
| Debian 系 | Ubuntu 18.04+、Debian 9+ |
| 其他 | Alpine（apk）、openSUSE（zypper）、Arch（pacman） |

### 包管理器

`dnf` · `yum` · `apt` · `apk` · `zypper` · `pacman`

### 初始化系统

仅支持 **systemd**（脚本会检测并拒绝非 systemd 系统）。

---

## 📋 档位选择说明

运行脚本后会显示交互菜单：

```
当前机器: 1G 内存 / 1 核 CPU

1. 差机器 (1G/1核)  → 限 5%   间隔 3s
2. 中机器 (2-4G)    → 限 10%  间隔 5s
3. 好机器 (8G+/4核+)→ 限 20%  间隔 8s
4. 自定义配置
0. 退出
```

| 档位 | 限制阈值 | 检测间隔 | 适合场景 |
|---|---|---|---|
| **1 - 差机器** | 5% | 3s | Oracle 免费机、1G/1核，宁可慢不能卡死 |
| **2 - 中机器** | 10% | 5s | 2-4G 内存，一般 VPS |
| **3 - 好机器** | 20% | 8s | 8G+/4核+，只防跑飞 |
| **4 - 自定义** | 你输入 | 你输入 | 特殊需求 |

> **怎么选**：你的两台 Oracle 免费机都选 **1**（5%/3s）。阈值越低，正常进程越容易被限但机器越不容易卡死；阈值越高，限制越宽松但突发时反应慢。

---

## 🛡️ 防误杀机制（4 层）

```
第1层: comm 精确匹配白名单（100+ 进程名，按发行版动态生成）
第2层: 完整命令行关键字匹配（宝塔路径、/usr/sbin、/lib/systemd 等）
第3层: systemd cgroup 检查（在 .service 里的进程默认不杀，除非在排除列表）
第4层: 不杀自己（SCRIPT_PID）
```

### 白名单覆盖范围

| 类别 | 示例 |
|---|---|
| 内核线程 | systemd, kthreadd, kworker, migration, rcu, watchdog… |
| 网络/SSH | sshd, nginx, mysqld, postgresql, dnsmasq, firewalld… |
| 系统服务 | systemd-journald, NetworkManager, chronyd, rsyslogd… |
| 宝塔全家桶 | BT-Panel, BT-Task, php-fpm, redis-server, pure-ftpd… |
| 监控/容器 | node_exporter, dockerd, kubelet, prometheus… |
| 发行版特有 | OL: cloud-init, waagent, dnf, oci-utils / Ubuntu: snapd, unattended-upgr, fwupd |

---

## ⚙️ 工作原理

```
systemd 启动 cpu-watchdog.service
  │
  ├── ExecStart: 主循环 (loop)
  │     每 N 秒:
  │     ps -eo pid,comm,pcpu
  │       ├── CPU > 阈值?
  │       │     ├── 4 层白名单检查 → 命中则跳过
  │       │     ├── 已被 cpulimit? → 跳过
  │       │     └── 否则 → cpulimit -p PID -l 阈值%
  │       │           记录到 limited_procs.conf
  │       └── CPU ≤ 阈值 → 不管
  │
  └── ExecStartPost (延迟20秒): 恢复模式 (restore)
        读取 limited_procs.conf
        对历史记录过的进程名重新施加 cpulimit
```

---

## 🔧 日常维护

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
# 1. 从黑名单删掉
sed -i '/^进程名|/d' /etc/cpu_watchdog/limited_procs.conf

# 2. 杀掉对应的 cpulimit 进程
pkill -f "cpulimit.*进程名"
```

### 调整阈值（需重新部署）
```bash
# 重新跑部署脚本选新档位，或手动编辑：
nano /usr/local/sbin/cpu_watchdog.sh
# 改 LIMIT_PCT= 和 CHECK_INTERVAL=
systemctl restart cpu-watchdog
```

### 停止 / 启动 / 重启
```bash
systemctl stop cpu-watchdog
systemctl start cpu-watchdog
systemctl restart cpu-watchdog
```

### 完全卸载
```bash
systemctl disable --now cpu-watchdog
rm -f /usr/local/sbin/cpu_watchdog.sh
rm -f /etc/systemd/system/cpu-watchdog.service
rm -rf /etc/cpu_watchdog
rm -f /var/log/cpu_watchdog.log
systemctl daemon-reload
```

---

## ⚠️ 已知局限

| 项目 | 说明 |
|---|---|
| **cpulimit 精度** | 基于 SIGSTOP/SIGCONT，精度约 ±2%，不适合硬实时 |
| **进程重启窗口** | 新 fork 的进程要等下一个检测周期才被限制（3-8s 窗口） |
| **bc 依赖** | 浮点比较用 `bc -l`，没装时 fallback 为不限制（安全侧） |
| **comm 截断** | `/proc/PID/comm` 只有 15 字符，靠第 2、3 层白名单兜底 |
| **systemd only** | 不支持 sysvinit / openrc（Alpine 上需自行配启动脚本） |

---

## 🚀 快速开始

```bash
# 1. 保存脚本
nano cpu_watchdog_deploy.sh
# 粘贴内容，保存

# 2. 加执行权限
chmod +x cpu_watchdog_deploy.sh

# 3. 运行（需要 root）
sudo bash cpu_watchdog_deploy.sh

# 4. 按菜单选档位，等部署完成

# 5. 验证
systemctl status cpu-watchdog
tail -5 /var/log/cpu_watchdog.log
```

---

## 📝 一句话总结

> **`cpu_watchdog_deploy.sh` = 交互式一键部署穷人版 cgroup CPUQuota**，自动适配发行版、自动生成扩展白名单、自动配 systemd 自启，选个档位就能用，适合所有低配 VPS 保命。

---

要不要我把这个也整合进之前那个 `system-tune.sh` 菜单里加个选项？这样一台机器跑一个脚本全搞定。

cpulimit 如果新系统找不到 见0tools目录，cpulimit.md