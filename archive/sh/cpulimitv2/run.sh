# 1. 安装 cpulimit 和 bc
sudo apt install -y cpulimit bc

# 2. 放脚本
sudo tee /usr/local/sbin/cpu_watchdog.sh << 'SCRIPT_EOF'
#!/bin/bash
# ============================================================
# CPU Watchdog - 每5秒检测CPU>10%的进程，自动cpulimit限制
# 记录到 /etc/cpu_watchdog/limited_procs.conf
# 开机自启后自动对记录过的进程名重新限制
# ============================================================

LIMIT_PCT=10
CHECK_INTERVAL=5
CONF_DIR="/etc/cpu_watchdog"
CONF_FILE="${CONF_DIR}/limited_procs.conf"
LOG_FILE="/var/log/cpu_watchdog.log"

# 白名单：这些不限制（内核线程/系统关键进程）
WHITELIST="systemd|kthreadd|kworker|migration|rcu|watchdog|idle|sshd|systemd-journal|rsyslog|journald|ssh|nginx|mysql"

mkdir -p "$CONF_DIR"
touch "$CONF_FILE"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >> "$LOG_FILE"
    echo "[$(date '+%H:%M:%S')] $*"
}

# ============================================================
# 函数：对指定PID施加限制
# ============================================================
limit_pid() {
    local pid="$1"
    local comm="$2"
    local pcpu="$3"

    # 已在限制中则跳过
    if pgrep -f "cpulimit.*-p ${pid}[^0-9]" > /dev/null 2>&1; then
        return
    fi

    # 白名单检查
    if echo "$comm" | grep -qiE "^(${WHITELIST})$"; then
        log "跳过白名单进程: $comm PID=$pid"
        return
    fi

    # 进程已退出
    if ! kill -0 "$pid" 2>/dev/null; then
        return
    fi

    log "限制 $comm PID=$pid CPU=${pcpu}% → ${LIMIT_PCT}%"
    cpulimit -p "$pid" -l "$LIMIT_PCT" -z >> "$LOG_FILE" 2>&1 &

    # 记录到配置文件（格式：comm|首次发现时间）
    if ! grep -q "^${comm}|" "$CONF_FILE" 2>/dev/null; then
        echo "${comm}|$(date '+%Y-%m-%d %H:%M:%S')" >> "$CONF_FILE"
        log "已记录 $comm 到黑名单配置"
    fi
}

# ============================================================
# 函数：开机自启时，对配置文件中记录的进程名重新限制
# ============================================================
enforce_saved() {
    log "开机恢复模式：检查已记录的进程..."
    while IFS='|' read -r comm first_seen; do
        [ -z "$comm" ] && continue
        # 跳过注释行
        echo "$comm" | grep -q '^#' && continue

        # 找当前运行的该名称进程
        for pid in $(pgrep -x "$comm" 2>/dev/null); do
            if kill -0 "$pid" 2>/dev/null; then
                pcpu=$(ps -p "$pid" -o pcpu= | tr -d ' ')
                log "恢复限制: $comm PID=$pid CPU=${pcpu}%"
                cpulimit -p "$pid" -l "$LIMIT_PCT" -z >> "$LOG_FILE" 2>&1 &
            fi
        done
    done < "$CONF_FILE"
}

# ============================================================
# 主循环：实时检测
# ============================================================
main_loop() {
    while true; do
        # ps 输出：pid comm pcpu
        ps -eo pid,comm,pcpu --no-headers | while read pid comm pcpu; do
            # 跳过自己
            [ "$pid" = "$$" ] && continue

            # 去掉 % 号（有些ps版本带）
            pcpu_num=$(echo "$pcpu" | tr -d '%')

            # 比较浮点
            if (( $(echo "$pcpu_num > $LIMIT_PCT" | bc -l 2>/dev/null || echo 0) )); then
                limit_pid "$pid" "$comm" "$pcpu_num"
            fi
        done

        sleep "$CHECK_INTERVAL"
    done
}

# ============================================================
# 入口
# ============================================================
case "${1:-loop}" in
    loop)
        log "CPU Watchdog 启动（实时循环模式）"
        main_loop
        ;;
    restore)
        enforce_saved
        ;;
    *)
        echo "用法: $0 [loop|restore]"
        exit 1
        ;;
esac
SCRIPT_EOF
# 或者你本地编辑好 scp 上去也行
sudo chmod +x /usr/local/sbin/cpu_watchdog.sh

# 3. 创建配置目录
sudo mkdir -p /etc/cpu_watchdog

# 4. 放 systemd service 文件
sudo tee /etc/systemd/system/cpu-watchdog.service << 'EOF'
[Unit]
Description=CPU Watchdog - Auto cpulimit processes over 10%
After=multi-user.target

[Service]
Type=simple
ExecStart=/usr/local/sbin/cpu_watchdog.sh loop
ExecStartPost=/bin/sleep 30
ExecStartPost=/usr/local/sbin/cpu_watchdog.sh restore
Restart=always
RestartSec=10

# 自身不受限
CPUQuota=

[Install]
WantedBy=multi-user.target
EOF

# 5. 重载并启动
sudo systemctl daemon-reload
sudo systemctl enable cpu-watchdog.service
sudo systemctl start cpu-watchdog.service

# 6. 查看状态
systemctl status cpu-watchdog
journalctl -u cpu-watchdog -f