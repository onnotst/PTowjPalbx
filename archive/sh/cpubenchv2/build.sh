#!/bin/bash
# build.sh — cpu_bench 多架构静态编译
#
# 源码只有一个: cpu_bench.c（Linux / macOS / Windows 统一单文件）
# 产物目录: ./bin/
#   Linux:   cpu_bench_x86  cpu_bench_amd64  cpu_bench_armhf  cpu_bench_aarch64
#   Windows: cpu_bench_win32.exe  cpu_bench_win64.exe   (链接系统自带 winhttp, 无第三方依赖)
#
# 依赖 (Debian/Ubuntu):
#   本机:   apt install -y gcc build-essential
#   Linux交叉: apt install -y gcc-i686-linux-gnu gcc-x86-64-linux-gnu \
#                              gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu
#   Windows: apt install -y mingw-w64
#
# 用法: bash build.sh            # 编译所有可用架构
#       bash build.sh -j 4       # 并行编译
#       bash build.sh clean      # 清理产物
#
# 说明:
#   - Windows 版必须带 -lwinhttp（MinGW 自带的导入库，静态链接也无需额外文件）
#   - Linux 版上传结果依赖运行时的 curl（缺失时自动退到 wget），
#     所以静态二进制本身不需要链接 libcurl，保持零依赖
#   - 上传地址默认 https://api.mblue.ltd/api/allinone/save
#     可用 -u <url> 或环境变量 CPU_BENCH_API 覆盖

set -e

JOBS=1
if [ "$1" = "-j" ] && [ -n "$2" ]; then JOBS="$2"; shift 2; fi
if [ "$1" = "clean" ]; then
    rm -rf bin
    echo "[*] 已清理 bin/ 目录"
    exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
mkdir -p bin

CFLAGS="-O2 -s -Wall"
COMMON="-static -s"
WINLIB="-lwinhttp"

# 收集每个目标的: 编译器 源码 输出名 附加链接参数
declare -a TARGETS=()

# ---------- Linux 本机 ----------
if command -v gcc >/dev/null 2>&1; then
    TARGETS+=( "gcc|cpu_bench.c|cpu_bench_$(uname -m)|" )
fi

# ---------- Linux 交叉编译 ----------
if command -v i686-linux-gnu-gcc >/dev/null 2>&1; then
    TARGETS+=( "i686-linux-gnu-gcc|cpu_bench.c|cpu_bench_x86|" )
fi
if command -v x86_64-linux-gnu-gcc >/dev/null 2>&1; then
    TARGETS+=( "x86_64-linux-gnu-gcc|cpu_bench.c|cpu_bench_amd64|" )
fi
if command -v arm-linux-gnueabihf-gcc >/dev/null 2>&1; then
    TARGETS+=( "arm-linux-gnueabihf-gcc|cpu_bench.c|cpu_bench_armhf|" )
fi
if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
    TARGETS+=( "aarch64-linux-gnu-gcc|cpu_bench.c|cpu_bench_aarch64|" )
fi

# ---------- Windows (MinGW-w64) ----------
if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
    TARGETS+=( "x86_64-w64-mingw32-gcc|cpu_bench.c|cpu_bench_win64.exe|$WINLIB" )
fi
if command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
    TARGETS+=( "i686-w64-mingw32-gcc|cpu_bench.c|cpu_bench_win32.exe|$WINLIB" )
fi

if [ ${#TARGETS[@]} -eq 0 ]; then
    echo "[!] 未找到任何 C 编译器 (gcc / 交叉工具链 / mingw-w64)"
    echo "    Debian/Ubuntu 安装示例:"
    echo "    apt install -y gcc build-essential"
    echo "    apt install -y gcc-i686-linux-gnu gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu"
    echo "    apt install -y mingw-w64"
    exit 1
fi

echo "[*] 开始编译 (并行度: $JOBS)..."
echo "[*] 共 ${#TARGETS[@]} 个目标"
echo

# 用临时文件收集编译结果, 支持并行
TMPDIR_BUILD="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_BUILD"' EXIT

compile_one() {
    local cc="$1" src="$2" out="$3" extra="$4" log="$5"
    if $cc $CFLAGS $COMMON "$src" -o "bin/$out" $extra >"$log" 2>&1; then
        echo "OK:$out" >> "$TMPDIR_BUILD/results"
    else
        echo "FAIL:$out" >> "$TMPDIR_BUILD/results"
        echo "----- $out -----" >> "$TMPDIR_BUILD/failures"
        cat "$log" >> "$TMPDIR_BUILD/failures"
    fi
}

JOB=0
PIDS=()
for t in "${TARGETS[@]}"; do
    IFS='|' read -r CC SRC OUT EXTRA <<< "$t"
    compile_one "$CC" "$SRC" "$OUT" "$EXTRA" "$TMPDIR_BUILD/$OUT.log" &
    PIDS+=($!)
    JOB=$((JOB + 1))
    if [ "$JOB" -ge "$JOBS" ]; then
        wait
        JOB=0
    fi
done
wait

# 汇总
echo "[*] 编译结果:"
PASS=0; FAIL=0
if [ -f "$TMPDIR_BUILD/results" ]; then
    while IFS=':' read -r status out; do
        if [ "$status" = "OK" ]; then
            printf "  \033[32m✅ %-24s\033[0m\n" "$out"
            PASS=$((PASS + 1))
        else
            printf "  \033[31m❌ %-24s\033[0m\n" "$out"
            FAIL=$((FAIL + 1))
        fi
    done < "$TMPDIR_BUILD/results"
fi

if [ -f "$TMPDIR_BUILD/failures" ]; then
    echo
    echo "[!] 失败详情:"
    cat "$TMPDIR_BUILD/failures"
fi

echo
echo "[*] 产物目录: $SCRIPT_DIR/bin/"
ls -lh bin/ 2>/dev/null | tail -n +2 | awk '{printf "    %-28s %6s\n", $9, $5}'

echo
echo "[*] 完成: 成功 $PASS, 失败 $FAIL"
[ "$FAIL" -eq 0 ] || exit 1
