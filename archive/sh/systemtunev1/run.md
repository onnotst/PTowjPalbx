好，给这个 `system-tune.sh` 写一份完整说明文档。

---

## 📌 脚本是什么

`system-tune.sh` 是一个**交互式系统调优脚本**，覆盖 SSH / Nginx / MySQL / Swap / BBR / 内核参数 / 日志清理 / 磁盘分析等 10 个常见运维场景。菜单驱动，支持自定义参数传入，一条命令完成一台机器的核心加固和性能调优。

**它做的事情**：通过 systemd drop-in、sysctl、swap 配置、服务管理等方式，把一台裸机的系统参数拉到适合低配 VPS / 宝塔环境的合理值，同时清理 Oracle 云镜像自带的遥测垃圾。

---

## 📁 脚本不生成固定文件，但会修改/创建以下位置

| 路径 | 操作 | 对应选项 |
|---|---|---|
| `/etc/systemd/system/sshd*.d/tune.conf` | 新建（drop-in） | 1 |
| `/etc/systemd/system/ssh*.socket.d/tune.conf` | 新建（drop-in） | 1 |
| `/etc/systemd/system/nginx.service` 或 `nginx.service.d/tune.conf` | 新建/修改 | 2 |
| `/etc/systemd/system/mysqld.service.d/tune.conf` 或 `mariadb.service.d/` | 新建 | 3 |
| `/swapfile` 或 `/www/swap` | 创建/调整 swap | 4 |
| `/etc/sysctl.d/99-swap.conf` | 新建 | 4 |
| `/etc/sysctl.d/99-bbr.conf` | 新建 | 5 |
| `/etc/sysctl.d/99-system-tune.conf` | 新建 | 6 |
| `/var/oled/` | 清理 PCP 数据 | 7 |
| `/www/wwwlogs/*.log` | 截断 >50M 日志 | 8 |
| `/etc/fstab` | 追加 swap 条目 | 4 |

---

## 🎯 适用场景

- **Oracle 免费机首次初始化**——一键搞定 swap、BBR、PCP 清理、系统参数
- **宝塔面板服务器调优**——自动识别宝塔 nginx（init.d 脚本）并包装成 systemd service 加 cgroup 限制
- **低配 VPS 防 OOM**——sshd 设 `OOMScoreAdjust=-1000` 保证卡死时还能连上去
- **批量部署**——支持命令行参数 `1,5000,-500` 格式，可非交互调用

---

## 🖥️ 兼容范围

### 发行版

| 家族 | 覆盖 |
|---|---|
| RHEL 系 | Oracle Linux 7/8/9/10、Rocky、Alma、CentOS 7/8 |
| Debian 系 | Ubuntu 16.04+、Debian 8+ |

### 初始化系统

`systemd`（选项 1-3、6 依赖 systemd；选项 4-5、7-10 在非 systemd 下部分可用）

### 包管理器

`dnf` · `yum` · `apt`

---

## 📋 菜单选项说明

```
  ╔══════════════════════════════════════════════╗
  ║  1.  SSH 调优 (CPUWeight 10000 + OOM -1000) ║
  ║  2.  Nginx 调优 (CPUWeight 800 + Quota 80%) ║
  ║  3.  MySQL 调优 (CPUWeight 400 + Quota 60%) ║
  ║  4.  Swap 调优 (4G + swappiness 30)         ║
  ║  5.  BBR 拥塞控制启用                       ║
  ║  6.  系统参数调优 (TCP/文件描述符/内存)     ║
  ║  7.  清理 Oracle PCP / 遥测                 ║
  ║  8.  日志清理 (journald + 宝塔 + 包缓存)    ║
  ║  9.  磁盘空间分析                           ║
  ║  10. 一键全选 (1-8)                         ║
  ║  0.  退出                                   ║
  ╚══════════════════════════════════════════════╝
```

### 逐项详解

#### 1 — SSH 调优

| 参数 | 默认值 | 作用 |
|---|---|---|
| `CPUWeight` | 10000（最高） | sshd 在 CPU 争抢时优先获得算力 |
| `OOMScoreAdjust` | -1000 | OOM 时 sshd 最后被杀，保证你还能连上去 |
| `OOMPolicy` | continue | OOM 后不杀 sshd，继续运行 |

**检测逻辑**：自动扫描 `systemctl list-unit-files` 里所有 `ssh*` / `sshd*` 开头的 unit，分别处理 `.socket`、`@.service` 模板、普通 `.service`，全部写入 drop-in。

**自定义**：`1,5000,-500` → CPUWeight=5000, OOM=-500

#### 2 — Nginx 调优

| 参数 | 默认值 | 作用 |
|---|---|---|
| `CPUWeight` | 800 | nginx 高优先级 |
| `CPUQuota` | 80% | 最多用 80% 单核 CPU |
| `OOMScoreAdjust` | -500 | OOM 保护 |

**宝塔兼容**：检测到 `/www/server/nginx/sbin/nginx` 且无 systemd service 时，自动创建 `/etc/systemd/system/nginx.service` 包装 `/etc/init.d/nginx` 并写入 cgroup 参数。

**自定义**：`2,1000,90%,-300` → weight=1000, quota=90%, oom=-300

#### 3 — MySQL 调优

| 参数 | 默认值 | 作用 |
|---|---|---|
| `CPUWeight` | 400 | mysql 中等优先级 |
| `CPUQuota` | 60% | 防止慢查询拖垮整机 |

自动检测 `mysqld.service` / `mariadb.service` / `mysql.service`。

**自定义**：`3,500,70%`

#### 4 — Swap 调优

| 参数 | 默认值 | 作用 |
|---|---|---|
| swap 大小 | 4G | 创建或调整 swap 文件 |
| `swappiness` | 30 | 物理内存用到 70% 才开始 swap（默认 60 太激进） |
| `dirty_ratio` | 15 | 脏页回写阈值 |
| `dirty_background_ratio` | 5 | 后台回写阈值 |

**行为**：
- 无 swap → 创建 `/swapfile`（或 `/www/swap` 如果 `/www` 存在）
- 有 swap → 关闭、重建、重新启用
- 写入 `/etc/fstab` 保证重启生效

**自定义**：`4,2G,10` → 2G swap, swappiness=10

#### 5 — BBR

- 内核 ≥ 4.9 时写入 `net.ipv4.tcp_congestion_control=bbr` + `net.core.default_qdisc=fq`
- 自动验证 `sysctl` 返回值
- 内核 < 4.9 时报错退出

#### 6 — 系统参数

写入 `/etc/sysctl.d/99-system-tune.conf`：

| 类别 | 参数 | 值 |
|---|---|---|
| 文件描述符 | `fs.file-max` | 655350 |
| TCP | `tcp_fin_timeout` | 15s |
| TCP | `tcp_keepalive_time` | 300s |
| TCP | `tcp_max_syn_backlog` | 8192 |
| TCP | `tcp_tw_reuse` | 1 |
| TCP | `ip_local_port_range` | 1024-65535 |
| 网络 | `somaxconn` | 8192 |
| 网络 | `netdev_max_backlog` | 8192 |
| 内存 | `min_free_kbytes` | 65536 |
| 内存 | `overcommit_memory` | 1 |

#### 7 — 清理 Oracle PCP

- 停止并禁用 `pmcd` `pmie` `pmlogger` `pmproxy`
- 清理 `/var/oled/pcp/` 下的日志和压缩文件
- Oracle 云镜像默认装了 PCP 监控，吃 20G+ 磁盘（你之前踩过的坑）

#### 8 — 日志清理

- `journalctl --vacuum-size=200M`
- 宝塔 `/www/wwwlogs/` 下 >50M 的日志 truncate 为 0
- 包缓存清理（`dnf clean all` / `apt clean` 等）

#### 9 — 磁盘空间分析

- `df -h /` 根分区使用
- `du -h --max-depth=1 /` top 15 目录
- `find / -type f -size +100M` 大文件列表

#### 10 — 一键全选

按顺序执行选项 1-8（不含 9 磁盘分析）。适合新机器首次初始化。

---

## 🔧 自定义参数用法

菜单里输入非 0-10 的数字会进入自定义模式：

```
自定义参数? (格式: 选项,参数1,参数2... 或直接回车返回): 1,5000,-500
```

| 示例 | 含义 |
|---|---|
| `1,5000,-500` | SSH: CPUWeight=5000, OOM=-500 |
| `2,1000,90%,-300` | Nginx: weight=1000, quota=90%, oom=-300 |
| `3,600,80%` | MySQL: weight=600, quota=80% |
| `4,2G,10` | Swap: 2G, swappiness=10 |

也支持**非交互调用**（在脚本末尾直接传参）：

```bash
# 在脚本最后加一行：
# tune_ssh 5000 -500
# 或者改 main() 里读 $1 $2
```

---

## 🚀 快速开始

```bash
# 1. 保存脚本
nano system-tune.sh
# 粘贴内容，保存

# 2. 加执行权限
chmod +x system-tune.sh

# 3. 运行（需要 root）
sudo bash system-tune.sh

# 4. 按菜单选选项，或选 10 一键全选

# 5. 验证
systemctl status sshd        # 确认 sshd 正常
sysctl net.ipv4.tcp_congestion_control  # 确认 bbr
free -h                      # 确认 swap
```

---

## ⚠️ 注意事项 & 已知局限

| 项目 | 说明 |
|---|---|
| **SSH 检测** | OL10 上如果 ssh 只有 `sshd.socket` + `sshd@.service` 模板，脚本能覆盖但需确认 drop-in 路径正确 |
| **Swap 重建** | 选项 4 会 `swapoff` 再重建，如果正在用 swap 且内存不足会失败——确保物理内存够用再跑 |
| **BBR 内核** | Oracle Linux 默认内核 5.x 没问题；Ubuntu 16.04 默认 4.4 需先升级内核 |
| **Nginx 包装** | 宝塔 nginx 被包装成 systemd service 后，`/etc/init.d/nginx` 和 systemd 不要混用，统一用 `systemctl` |
| **全选顺序** | 选项 10 里 swap 在前面、PCP 清理在后面，如果 `/var/oled` 是独立 LV 且 xfs 不能缩小（你之前踩的坑），选项 7 只清理内容不缩 LV，安全 |
| **回滚** | drop-in 文件删掉 + `systemctl daemon-reload` 即可回滚 cgroup 改动；sysctl 文件删掉 + `sysctl --system` 回滚内核参数 |

---

## 🔄 回滚方法

```bash
# SSH 回滚
rm -rf /etc/systemd/system/sshd*.d /etc/systemd/system/ssh*.socket.d
systemctl daemon-reload && systemctl restart sshd

# Nginx 回滚
rm -f /etc/systemd/system/nginx.service.d/tune.conf
rm -f /etc/systemd/system/nginx.service  # 宝塔包装
systemctl daemon-reload

# MySQL 回滚
rm -rf /etc/systemd/system/mysqld.service.d /etc/systemd/system/mariadb.service.d
systemctl daemon-reload

# 内核参数回滚
rm -f /etc/sysctl.d/99-*.conf
sysctl --system

# Swap 回滚
swapoff /swapfile && rm -f /swapfile
sed -i '/swapfile/d' /etc/fstab
```

---

## 📝 一句话总结

> **`system-tune.sh` = 低配 VPS / 宝塔服务器的交互式初始化瑞士军刀**，10 个菜单选项覆盖从 SSH 保活到磁盘清理的全链路，支持自定义参数，systemd 环境下一键拉满。

---

**要不要我把 CPU Watchdog 部署也加进去做选项 11，合并成一个终极一键脚本？**