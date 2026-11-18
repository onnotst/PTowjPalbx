// cpu_bench_win.c — Windows 原生 CPU 基准测试
// 在 Linux 上用 MinGW-w64 交叉编译:
//   x86_64-w64-mingw32-gcc -O2 -static -s cpu_bench_win.c -o cpu_bench_win64.exe
//   i686-w64-mingw32-gcc  -O2 -static -s cpu_bench_win.c -o cpu_bench_win32.exe
//
// 编译需求: mingw-w64 (Debian/Ubuntu: apt install mingw-w64)
// 运行需求: 无依赖, 直接运行

#define _WIN32_WINNT 0x0601
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <windows.h>

/* ============ 防优化屏障 ============ */
static volatile uint64_t bench_sink = 0;

/* ============ 1. 整数累加 ============ */
static uint64_t bench_int_add(uint64_t seconds) {
    uint64_t ops = 0;
    volatile uint64_t s = 0;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    while (1) {
        uint64_t i;
        for (i = 1; i <= 1000000; i++) s += i;
        ops += 1000000;
        QueryPerformanceCounter(&t1);
        double elapsed = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
        if (elapsed >= (double)seconds) break;
    }
    bench_sink = s;
    return ops;
}

/* ============ 2. 整数乘加+取模 (FNV变体, 贴近真实整数负载) ============ */
static uint64_t bench_int_madd(uint64_t seconds) {
    uint64_t ops = 0;
    volatile uint64_t h = 0x811C9DC5;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    while (1) {
        uint64_t i;
        for (i = 0; i < 200000; i++) {
            h = (h ^ (i * 0x01000193)) * 0x100000001ULL;
            h ^= h >> 13;
            h = h * 0x5bd1e995;
            h ^= h >> 15;
        }
        ops += 200000;
        QueryPerformanceCounter(&t1);
        double elapsed = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
        if (elapsed >= (double)seconds) break;
    }
    bench_sink = h;
    return ops;
}

/* ============ 3. 标量浮点 (纯算术, 不依赖 libm) ============ */
static uint64_t bench_float(uint64_t seconds) {
    uint64_t ops = 0;
    volatile double x = 1.0;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    while (1) {
        uint64_t i;
        for (i = 0; i < 50000; i++) {
            x = x * 1.0000001 + 0.0000003;
            x = x / 1.00000005 - 0.0000001;
            x = x * 0.9999999 + 0.5;
        }
        ops += 50000 * 3;
        QueryPerformanceCounter(&t1);
        double elapsed = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
        if (elapsed >= (double)seconds) break;
    }
    /* 用 sink 吸收 x, 防止整段被优化掉 */
    bench_sink = (uint64_t)(x * 1.0);
    return ops;
}

/* ============ 4. 内存顺序读带宽 ============ */
#define MEM_BUF_SIZE (16u * 1024u * 1024u)   /* 16MB */

static uint64_t bench_mem_read(uint64_t seconds) {
    static uint32_t buf[MEM_BUF_SIZE];   /* 放在静态区, 避免巨大栈 */
    uint32_t *p = buf;
    for (uint32_t i = 0; i < MEM_BUF_SIZE; i++) p[i] = i;

    uint64_t ops = 0;
    volatile uint64_t sum = 0;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    while (1) {
        uint64_t local_sum = 0;
        for (uint32_t i = 0; i < MEM_BUF_SIZE; i += 8) {
            local_sum += p[i] + p[i+1] + p[i+2] + p[i+3]
                       + p[i+4] + p[i+5] + p[i+6] + p[i+7];
        }
        sum = local_sum;
        ops += MEM_BUF_SIZE;
        QueryPerformanceCounter(&t1);
        double elapsed = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
        if (elapsed >= (double)seconds) break;
    }
    bench_sink = sum;
    return ops * 4ULL;   /* 每次读 4 字节 */
}

/* ============ 系统信息 (Windows) ============ */
static void print_sysinfo(void) {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    DWORD cores = si.dwNumberOfProcessors;

    char cpu_brand[256] = "Unknown";
    DWORD brand_len = sizeof(cpu_brand);
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL,
                         (LPBYTE)cpu_brand, &brand_len);
        RegCloseKey(hKey);
    }

    char os_ver[128] = "Unknown";
    OSVERSIONINFOEXA osi;
    ZeroMemory(&osi, sizeof(OSVERSIONINFOEXA));
    osi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
    if (GetVersionExA((OSVERSIONINFOA *)&osi)) {
        /* Windows 10/11 只报告 10.0, 用产品名补充 */
        snprintf(os_ver, sizeof(os_ver), "Windows %lu.%lu (Build %lu)",
                 osi.dwMajorVersion, osi.dwMinorVersion, osi.dwBuildNumber);
    }

    char bits[16];
    snprintf(bits, sizeof(bits), "%d-bit",
             (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
              si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) ? 64 : 32);

    printf("  系统版本: %s\n", os_ver);
    printf("  处理器:   %s\n", cpu_brand);
    printf("  逻辑核心: %lu\n", cores);
    printf("  架构位数: %s\n", bits);
}

/* ============ 主函数 ============ */
int main(int argc, char *argv[]) {
    uint64_t timeout = 20;
    int quiet = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc)
            timeout = (uint64_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            i++;   /* 兼容 Linux 版参数, Windows 暂未用 */
        }
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0)
            quiet = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("用法: %s [-t 秒数] [-q]\n", argv[0]);
            printf("  -t 秒数   每项测试时长 (默认 20)\n");
            printf("  -q        安静模式, 只输出分数\n");
            return 0;
        }
    }

    if (!quiet) {
        printf("=========================================\n");
        printf(" CPU 性能测试 (Windows Native)\n");
        printf(" 运行时间: %" PRIu64 " 秒\n", timeout);
        printf("=========================================\n");
        printf("\n[系统信息]\n");
        print_sysinfo();
    }

    /* 1. 整数累加 */
    if (!quiet) printf("\n[1/4] 整数累加测试...\n");
    uint64_t int_ops = bench_int_add(timeout);
    if (!quiet) {
        printf("  总操作数: %" PRIu64 "\n", int_ops);
        printf("  耗时: %" PRIu64 " s\n", timeout);
    }
    printf("  >>> 整数累加: %" PRIu64 " ops/sec\n", int_ops / timeout);

    /* 2. 整数乘加 */
    if (!quiet) printf("\n[2/4] 整数乘加+取模测试...\n");
    uint64_t madd_ops = bench_int_madd(timeout);
    if (!quiet) {
        printf("  总操作数: %" PRIu64 "\n", madd_ops);
        printf("  耗时: %" PRIu64 " s\n", timeout);
    }
    printf("  >>> 整数乘加: %" PRIu64 " ops/sec\n", madd_ops / timeout);

    /* 3. 标量浮点 */
    if (!quiet) printf("\n[3/4] 标量浮点测试...\n");
    uint64_t flt_ops = bench_float(timeout);
    if (!quiet) {
        printf("  总操作数: %" PRIu64 "\n", flt_ops);
        printf("  耗时: %" PRIu64 " s\n", timeout);
    }
    printf("  >>> 浮点吞吐: %" PRIu64 " flops/sec\n", flt_ops / timeout);

    /* 4. 内存带宽 */
    if (!quiet) printf("\n[4/4] 内存顺序读测试 (16MB)...\n");
    uint64_t mem_bytes = bench_mem_read(timeout);
    if (!quiet) {
        printf("  总字节数: %" PRIu64 "\n", mem_bytes);
        printf("  耗时: %" PRIu64 " s\n", timeout);
    }
    printf("  >>> 内存带宽: %" PRIu64 " MiB/s\n",
           mem_bytes / timeout / 1024 / 1024);

    /* 汇总 */
    if (!quiet) {
        printf("\n=========================================\n");
        printf(" 汇总分数\n");
        printf("=========================================\n");
        printf("  整数累加: %" PRIu64 " ops/sec\n", int_ops / timeout);
        printf("  整数乘加: %" PRIu64 " ops/sec\n", madd_ops / timeout);
        printf("  标量浮点: %" PRIu64 " flops/sec\n", flt_ops / timeout);
        printf("  内存带宽: %" PRIu64 " MiB/s\n",
               mem_bytes / timeout / 1024 / 1024);
        printf("=========================================\n");
    }

    return 0;
}
