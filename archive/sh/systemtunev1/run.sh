#!/bin/bash
# ============================================================
# system-tune.sh — 系统关键参数一键调优（交互式菜单）
# 支持: Oracle Linux, Rocky, Alma, CentOS, Ubuntu, Debian
# ============================================================

set -euo pipefail

# ---------- 颜色 ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# ---------- 工具函数 ----------
info()    { echo -e "${GREEN}[INFO]${NC} $1"; }
warn()    { echo -e "${YELLOW}[WARN]${NC} $1"; }
error()   { echo -e "${RED}[ERROR]${NC} $1"; }
section(){ echo -e "\n${CYAN}====== $1 ======${NC}\n"; }

# ---------- 检测发行版 ----------
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO="$ID"
        VERSION="$VERSION_ID"
    else
        DISTRO="unknown"
        VERSION="unknown"
    fi
}

# ---------- 检测 init 系统 ----------
detect_init() {
    if command -v systemctl &>/dev/null && systemctl --version &>/dev/null 2>&1; then
        INIT="systemd"
    elif command -v initctl &>/dev/null; then
        INIT="upstart"
    else
        INIT="sysv"
    fi
}

# ---------- 检测包管理器 ----------
detect_pkg() {
    if command -v dnf &>/dev/null; then
        PKG_MGR="dnf"; PKG_INSTALL="dnf install -y"
    elif command -v yum &>/dev/null; then
        PKG_MGR="yum"; PKG_INSTALL="yum install -y"
    elif command -v apt &>/dev/null; then
        PKG_MGR="apt"; PKG_INSTALL="apt install -y"
    else
        PKG_MGR="unknown"; PKG_INSTALL="echo 'Cannot install:'"
    fi
}

# ---------- 检测文件系统类型 ----------
detect_fstype() {
    local mp="${1:-/}"
    if command -v df &>/dev/null; then
        df -T "$mp" 2>/dev/null | awk 'NR==2 {print $2}'
    else
        echo "unknown"
    fi
}

# ---------- 检测 swap ----------
detect_swap() {
    if [ -f /proc/swaps ]; then
        grep -v "Filename" /proc/swaps | awk '{print $1, $3}'
    fi
}

# ---------- 选项 1: SSH 调优 ----------
tune_ssh() {
    section "SSH 调优 (CPUWeight + OOM 保护)"

    if [ "$INIT" != "systemd" ]; then
        warn "非 systemd 系统，跳过 cgroup 调优"
        return
    fi

    local weight="${1:-10000}"
    local oom="${2:--1000}"

    info "设置 ssh CPUWeight=$weight, OOMScoreAdjust=$oom"

    # 检测 ssh 相关 unit（覆盖所有变体）
    local ssh_units=""
    ssh_units=$(systemctl list-unit-files 2>/dev/null | grep -iE '^(ssh|sshd)' | awk '{print $1}')

    if [ -z "$ssh_units" ]; then
        error "找不到任何 ssh/sshd service，尝试手动指定..."
        # 兜底：直接找 sshd 进程
        local sshd_pid=$(pgrep -x sshd 2>/dev/null | head -1)
        if [ -n "$sshd_pid" ]; then
            info "找到 sshd 进程 (PID $sshd_pid)，通过 proc cgroup 路径推断"
            local cgroup_path=$(cat /proc/$sshd_pid/cgroup 2>/dev/null | grep -o '[^/]*\.service' | head -1)
            if [ -n "$cgroup_path" ]; then
                info "sshd 在 cgroup: $cgroup_path"
                mkdir -p /etc/systemd/system/$(dirname $cgroup_path)/
            fi
        fi
        error "无法自动检测 ssh service 名，请手动确认后重试"
        return
    fi

    info "检测到 ssh 相关 unit:"
    echo "$ssh_units" | while read -r unit; do
        echo "  - $unit"
    done

    # 优先处理 socket 激活
    echo "$ssh_units" | grep -E '\.socket$' | while read -r sock; do
        local sock_dir="/etc/systemd/system/$(basename $sock .socket).socket.d"
        mkdir -p "$sock_dir"
        cat > "$sock_dir/tune.conf" << EOF
[Service]
CPUWeight=$weight
OOMScoreAdjust=$oom
OOMPolicy=continue
EOF
        info "已配置 $sock → $sock_dir/tune.conf"
    done

    # 处理 @.service 模板
    echo "$ssh_units" | grep '@\.' | while read -r tmpl; do
        local tmpl_dir="/etc/systemd/system/$(basename $tmpl).d"
        mkdir -p "$tmpl_dir"
        cat > "$tmpl_dir/tune.conf" << EOF
[Service]
CPUWeight=$weight
OOMScoreAdjust=$oom
OOMPolicy=continue
EOF
        info "已配置模板 $tmpl → $tmpl_dir/tune.conf"
    done

    # 处理普通 .service
    echo "$ssh_units" | grep '\.service$' | grep -v '@' | while read -r svc; do
        local svc_dir="/etc/systemd/system/${svc}.d"
        mkdir -p "$svc_dir"
        cat > "$svc_dir/tune.conf" << EOF
[Service]
CPUWeight=$weight
OOMScoreAdjust=$oom
OOMPolicy=continue
EOF
        info "已配置 $svc → $svc_dir/tune.conf"
    done

    systemctl daemon-reload
    info "SSH 调优完成 ✓ (重启 ssh 后生效: systemctl restart sshd)"
}

# ---------- 选项 2: Nginx 调优 ----------
tune_nginx() {
    section "Nginx 调优 (CPUWeight + CPUQuota + OOM)"

    if [ "$INIT" != "systemd" ]; then
        warn "非 systemd 系统，跳过"
        return
    fi

    local weight="${1:-800}"
    local quota="${2:-80%}"
    local oom="${3:--500}"

    # 检测 nginx 是否已安装
    if ! command -v nginx &>/dev/null && ! [ -f /www/server/nginx/sbin/nginx ]; then
        warn "nginx 未检测到，跳过"
        return
    fi

    # 检测 nginx service 名
    local nginx_svc=""
    for s in nginx.service nginx; do
        if systemctl list-unit-files 2>/dev/null | grep -q "^${s}"; then
            nginx_svc="$s"
            break
        fi
    done

    if [ -z "$nginx_svc" ]; then
        warn "nginx service 未找到（可能是宝塔安装的 init.d 脚本）"
        # 宝塔 nginx 走 /etc/init.d/nginx
        if [ -f /etc/init.d/nginx ]; then
            info "检测到宝塔 nginx (/etc/init.d/nginx)，创建 systemd 包装 service"
            cat > /etc/systemd/system/nginx.service << 'EOF'
[Unit]
Description=Nginx (BT)
After=network.target

[Service]
Type=forking
PIDFile=/www/server/nginx/logs/nginx.pid
ExecStart=/etc/init.d/nginx start
ExecReload=/etc/init.d/nginx reload
ExecStop=/etc/init.d/nginx stop
CPUWeight=800
CPUQuota=80%
OOMScoreAdjust=-500
Restart=always

[Install]
WantedBy=multi-user.target
EOF
            systemctl daemon-reload
            systemctl enable nginx 2>/dev/null || true
            info "已创建 nginx systemd service 包装"
        fi
    else
        mkdir -p /etc/systemd/system/${nginx_svc}.d
        cat > /etc/systemd/system/${nginx_svc}.d/tune.conf << EOF
[Service]
CPUWeight=$weight
CPUQuota=$quota
OOMScoreAdjust=$oom
EOF
        info "写入 /etc/systemd/system/${nginx_svc}.d/tune.conf"
    fi

    systemctl daemon-reload
    info "Nginx 调优完成 ✓"
}

# ---------- 选项 3: MySQL 调优 ----------
tune_mysql() {
    section "MySQL 调优 (CPUWeight + CPUQuota)"

    if [ "$INIT" != "systemd" ]; then
        warn "非 systemd 系统，跳过"
        return
    fi

    local weight="${1:-400}"
    local quota="${2:-60%}"

    # 检测 mysql service
    local mysql_svc=""
    for s in mysqld.service mariadb.service mysql.service; do
        if systemctl list-unit-files 2>/dev/null | grep -q "^${s}"; then
            mysql_svc="$s"
            break
        fi
    done

    if [ -z "$mysql_svc" ]; then
        warn "MySQL service 未找到"
        return
    fi

    mkdir -p /etc/systemd/system/${mysql_svc}.d
    cat > /etc/systemd/system/${mysql_svc}.d/tune.conf << EOF
[Service]
CPUWeight=$weight
CPUQuota=$quota
EOF

    systemctl daemon-reload
    info "MySQL 调优完成 ($mysql_svc, weight=$weight, quota=$quota) ✓"
}

# ---------- 选项 4: Swap 调优 ----------
tune_swap() {
    section "Swap 调优"

    local swap_size="${1:-4G}"
    local swappiness="${2:-30}"

    info "当前 swap 状态:"
    detect_swap || echo "  无 swap"

    # 找现有 swap 文件
    local swap_file=""
    if grep -q "swap" /proc/swaps 2>/dev/null; then
        swap_file=$(grep -v Filename /proc/swaps | awk '{print $1}' | head -1)
    fi

    if [ -z "$swap_file" ] || [ "$swap_file" = "none" ]; then
        # 创建新 swap
        if [ -f /www/swap ]; then
            swap_file="/www/swap"
        else
            swap_file="/swapfile"
        fi
        info "创建 swap 文件: $swap_file ($swap_size)"
        swapoff "$swap_file" 2>/dev/null || true
        rm -f "$swap_file"
        fallocate -l "$swap_size" "$swap_file" 2>/dev/null || dd if=/dev/zero of="$swap_file" bs=1M count=${swap_size%G}000 status=progress
        chmod 600 "$swap_file"
        mkswap "$swap_file"
        swapon "$swap_file"
    else
        info "调整现有 swap: $swap_file"
        swapoff "$swap_file"
        fallocate -l "$swap_size" "$swap_file" 2>/dev/null || dd if=/dev/zero of="$swap_file" bs=1M count=${swap_size%G}000 status=progress
        chmod 600 "$swap_file"
        mkswap "$swap_file"
        swapon "$swap_file"
    fi

    # 写 fstab
    if ! grep -q "$swap_file" /etc/fstab; then
        echo "$swap_file none swap sw 0 0" >> /etc/fstab
        info "fstab 已更新"
    fi

    # sysctl 参数
    cat > /etc/sysctl.d/99-swap.conf << EOF
vm.swappiness=$swappiness
vm.dirty_ratio=15
vm.dirty_background_ratio=5
EOF
    sysctl -p /etc/sysctl.d/99-swap.conf
    info "Swap 调优完成 ✓"
}

# ---------- 选项 5: BBR 开启 ----------
tune_bbr() {
    section "BBR 拥塞控制"

    info "检测内核版本..."
    local kernel_ver=$(uname -r | cut -d. -f1)
    if [ "$kernel_ver" -lt 4 ]; then
        error "内核版本过低 ($(uname -r))，BBR 需要 4.9+"
        return
    fi

    cat > /etc/sysctl.d/99-bbr.conf << EOF
net.core.default_qdisc=fq
net.ipv4.tcp_congestion_control=bbr
EOF
    sysctl -p /etc/sysctl.d/99-bbr.conf

    # 验证
    local bbr_check=$(sysctl net.ipv4.tcp_congestion_control | awk '{print $3}')
    if [ "$bbr_check" = "bbr" ]; then
        info "BBR 已启用 ✓ ($(uname -r))"
    else
        warn "BBR 启用失败，当前: $bbr_check"
    fi
}

# ---------- 选项 6: 系统参数调优 ----------
tune_sysctl() {
    section "通用系统参数调优"

    cat > /etc/sysctl.d/99-system-tune.conf << EOF
# 文件描述符
fs.file-max=655350

# TCP 优化
net.ipv4.tcp_fin_timeout=15
net.ipv4.tcp_keepalive_time=300
net.ipv4.tcp_keepalive_probes=5
net.ipv4.tcp_keepalive_intvl=15
net.ipv4.tcp_max_syn_backlog=8192
net.ipv4.tcp_syncookies=1
net.ipv4.tcp_tw_reuse=1
net.ipv4.ip_local_port_range=1024 65535
net.core.somaxconn=8192
net.core.netdev_max_backlog=8192

# 内存管理
vm.min_free_kbytes=65536
vm.overcommit_memory=1
EOF

    sysctl -p /etc/sysctl.d/99-system-tune.conf
    info "系统参数调优完成 ✓"
}

# ---------- 选项 7: 清理 PCP / 遥测 ----------
clean_pcp() {
    section "清理 Oracle PCP / 遥测"

    systemctl stop pmcd pmie pmlogger pmproxy 2>/dev/null || true
    systemctl disable pmcd pmie pmlogger pmproxy 2>/dev/null || true

    if [ -d /var/oled ]; then
        rm -rf /var/oled/pcp/pmlogger/* /var/oled/pcp/pmie/*
        find /var/oled -name "*.xz" -delete 2>/dev/null || true
        find /var/oled -name "*.prev" -delete 2>/dev/null || true
        info "/var/oled 已清理"
    fi

    info "PCP 清理完成 ✓"
}

# ---------- 选项 8: 日志清理 ----------
clean_logs() {
    section "日志清理"

    # journald
    journalctl --vacuum-size=200M 2>/dev/null || true

    # 宝塔日志
    if [ -d /www/wwwlogs ]; then
        find /www/wwwlogs -name "*.log" -size +50M -exec truncate -s 0 {} \; 2>/dev/null || true
        info "宝塔日志已截断 (>50M)"
    fi

    # 包缓存
    if [ "$PKG_MGR" = "dnf" ]; then
        dnf clean all 2>/dev/null || true
    elif [ "$PKG_MGR" = "yum" ]; then
        yum clean all 2>/dev/null || true
    elif [ "$PKG_MGR" = "apt" ]; then
        apt clean 2>/dev/null || true
    fi

    info "日志清理完成 ✓"
}

# ---------- 选项 9: 磁盘空间分析 ----------
analyze_disk() {
    section "磁盘空间分析"

    info "根分区使用情况:"
    df -h /

    echo ""
    info "根目录下各目录大小 (top 15):"
    du -h --max-depth=1 / 2>/dev/null | sort -rh | head -15

    echo ""
    info "大文件 (>100M):"
    find / -type f -size +100M -exec ls -lh {} \; 2>/dev/null | awk '{print $5, $9}' | head -10
}

# ---------- 选项 10: 一键全选 ----------
tune_all() {
    section "一键全选模式"
    tune_ssh 10000 -1000
    tune_nginx 800 80% -500
    tune_mysql 400 60%
    tune_swap 4G 30
    tune_bbr
    tune_sysctl
    clean_pcp
    clean_logs
    info "一键全选完成 ✓"
}

# ---------- 显示菜单 ----------
show_menu() {
    clear
    echo -e "${CYAN}"
    echo "  ╔══════════════════════════════════════════════╗"
    echo "  ║        System Tune v1.0 — 交互式调优        ║"
    echo "  ╠══════════════════════════════════════════════╣"
    echo "  ║  1.  SSH 调优 (CPUWeight 10000 + OOM -1000) ║"
    echo "  ║  2.  Nginx 调优 (CPUWeight 800 + Quota 80%) ║"
    echo "  ║  3.  MySQL 调优 (CPUWeight 400 + Quota 60%) ║"
    echo "  ║  4.  Swap 调优 (4G + swappiness 30)         ║"
    echo "  ║  5.  BBR 拥塞控制启用                       ║"
    echo "  ║  6.  系统参数调优 (TCP/文件描述符/内存)     ║"
    echo "  ║  7.  清理 Oracle PCP / 遥测                 ║"
    echo "  ║  8.  日志清理 (journald + 宝塔 + 包缓存)    ║"
    echo "  ║  9.  磁盘空间分析                           ║"
    echo "  ║  10. 一键全选 (1-8)                         ║"
    echo "  ║  0.  退出                                   ║"
    echo "  ╚══════════════════════════════════════════════╝"
    echo -e "${NC}"
    echo -e "  系统: ${GREEN}$DISTRO $VERSION${NC} | Init: ${GREEN}$INIT${NC} | 包管理: ${GREEN}$PKG_MGR${NC}"
    echo ""
}

# ---------- 主循环 ----------
main() {
    detect_distro
    detect_init
    detect_pkg

    while true; do
        show_menu
        read -rp "  请选择 [0-10]: " choice

        case "$choice" in
            1) tune_ssh 10000 -1000 ;;
            2) tune_nginx 800 80% -500 ;;
            3) tune_mysql 400 60% ;;
            4) tune_swap 4G 30 ;;
            5) tune_bbr ;;
            6) tune_sysctl ;;
            7) clean_pcp ;;
            8) clean_logs ;;
            9) analyze_disk ;;
            10) tune_all ;;
            0) echo -e "\n${GREEN}退出${NC}"; exit 0 ;;
            *)
                read -rp "  自定义参数? (格式: 选项,参数1,参数2... 或 直接回车返回): " custom
                if [ -z "$custom" ]; then
                    continue
                fi
                IFS=',' read -ra PARTS <<< "$custom"
                case "${PARTS[0]}" in
                    1) tune_ssh "${PARTS[1]:-10000}" "${PARTS[2]:--1000}" ;;
                    2) tune_nginx "${PARTS[1]:-800}" "${PARTS[2]:-80%}" "${PARTS[3]:--500}" ;;
                    3) tune_mysql "${PARTS[1]:-400}" "${PARTS[2]:-60%}" ;;
                    4) tune_swap "${PARTS[1]:-4G}" "${PARTS[2]:-30}" ;;
                    *) error "未知选项" ;;
                esac
                ;;
        esac

        echo ""
        read -rp "  按回车返回菜单..."
    done
}

main "$@"