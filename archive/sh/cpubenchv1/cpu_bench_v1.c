// cpu_bench.c — 跨 Linux 发行版 CPU 基准测试 (静态编译, 无依赖)
//
// 编译:
//   本机: gcc -O2 -static -s -o cpu_bench cpu_bench.c
//   交叉:
//     gcc-i686-linux-gnu-gcc      -O2 -static -s cpu_bench.c -o cpu_bench_x86
//     gcc-x86-64-linux-gnu-gcc    -O2 -static -s cpu_bench.c -o cpu_bench_amd64
//     gcc-arm-linux-gnueabihf-gcc -O2 -static -s cpu_bench.c -o cpu_bench_armhf
//     aarch64-linux-gnu-gcc       -O2 -static -s cpu_bench.c -o cpu_bench_aarch64
//
// 运行: ./cpu_bench [-t 秒数] [-m 内存MB] [-q]
//   -t 秒数   每项测试时长 (默认 20)
//   -m 内存MB 内存测试缓冲区大小 (默认 16, 0=跳过)
//   -q        安静模式

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

#if defined(__linux__)
#  include <sys/sysinfo.h>
#endif

/* ============ 防优化屏障 ============ */
static volatile uint64_t bench_sink = 0;

/* ============ 高精度计时 ============ */
static double now_sec(void) {
#if defined(__linux__) && defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#else
    return (double)time(NULL);
#endif
}

/* ============ 1. 整数累加 ============ */
static uint64_t bench_int_add(uint64_t seconds) {
    uint64_t ops = 0;
    volatile uint64_t s = 0;
    double t0 = now_sec();

    while (1) {
        uint64_t i;
        for (i = 1; i <= 1000000; i++) s += i;
        ops += 1000000;
        if (now_sec() - t0 >= (double)seconds) break;
    }
    bench_sink = s;
    return ops;
}

/* ============ 2. 整数乘加+取模 (FNV变体) ============ */
static uint64_t bench_int_madd(uint64_t seconds) {
    uint64_t ops = 0;
    volatile uint64_t h = 0x811C9DC5;
    double t0 = now_sec();

    while (1) {
        uint64_t i;
        for (i = 0; i < 200000; i++) {
            h = (h ^ (i * 0x01000193)) * 0x100000001ULL;
            h ^= h >> 13;
            h = h * 0x5bd1e995;
            h ^= h >> 15;
        }
        ops += 200000;
        if (now_sec() - t0 >= (double)seconds) break;
    }
    bench_sink = h;
    return ops;
}

/* ============ 3. 标量浮点 ============ */
static uint64_t bench_float(uint64_t seconds) {
    uint64_t ops = 0;
    volatile double x = 1.0;
    double t0 = now_sec();

    while (1) {
        uint64_t i;
        for (i = 0; i < 50000; i++) {
            x = x * 1.0000001 + 0.0000003;
            x = x / 1.00000005 - 0.0000001;
            x = x * 0.9999999 + 0.5;
        }
        ops += 50000 * 3;
        if (now_sec() - t0 >= (double)seconds) break;
    }
    bench_sink = (uint64_t)(x * 1.0);
    return ops;
}

/* ============ 4. 内存顺序读 ============ */
static uint64_t bench_mem_read(uint64_t seconds, uint32_t mb) {
    size_t n = (size_t)mb * 1024 * 1024 / sizeof(uint32_t);
    uint32_t *buf = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!buf) {
        fprintf(stderr, "  内存分配失败 (%u MB)\n", mb);
        return 0;
    }
    for (uint32_t i = 0; i < n; i++) buf[i] = i;

    uint64_t ops = 0;
    volatile uint64_t sum = 0;
    double t0 = now_sec();

    while (1) {
        uint64_t local_sum = 0;
        for (uint32_t i = 0; i < n; i += 8) {
            local_sum += buf[i] + buf[i+1] + buf[i+2] + buf[i+3]
                       + buf[i+4] + buf[i+5] + buf[i+6] + buf[i+7];
        }
        sum = local_sum;
        ops += n;
        if (now_sec() - t0 >= (double)seconds) break;
    }
    free(buf);
    bench_sink = sum;
    return ops * sizeof(uint32_t);
}

/* ============ 系统信息 ============ */
static void print_sysinfo(void) {
    /* OS 版本 */
#if defined(__linux__)
    const char *pretty = "Unknown";
    FILE *f = fopen("/etc/os-release", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                pretty = line + 12;
                char *q = strchr(pretty, '\n'); if (q) *q = 0;
                if (*pretty == '"') pretty++;
                q = strrchr((char *)pretty, '"'); if (q) *q = 0;
                break;
            }
        }
        fclose(f);
    }
    printf("  系统版本: %s\n", pretty);
    printf("  内核版本: ");
    fflush(stdout);
    char kver[128] = "";
    FILE *kf = fopen("/proc/version", "r");
    if (kf) {
        if (fgets(kver, sizeof(kver), kf)) {
            char *p = strchr(kver, '\n'); if (p) *p = 0;
        }
        fclose(kf);
    }
    printf("%s\n", kver);
#else
    printf("  系统版本: 非 Linux 构建\n");
#endif

    /* 处理器型号 */
    char cpu_brand[256] = "Unknown";
#if defined(__linux__)
    FILE *cf = fopen("/proc/cpuinfo", "r");
    if (cf) {
        char line[256];
        while (fgets(line, sizeof(line), cf)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *p = strchr(line, ':');
                if (p) {
                    p++; while (*p == ' ') p++;
                    char *q = strchr(p, '\n'); if (q) *q = 0;
                    snprintf(cpu_brand, sizeof(cpu_brand), "%s", p);
                    break;
                }
            }
        }
        fclose(cf);
    }
#endif
    printf("  处理器:   %s\n", cpu_brand);

    /* 核心数 */
    long cores = 0;
#if defined(__linux__)
    cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (cores <= 0) cores = 1;
    printf("  逻辑核心: %ld\n", cores);

    /* 架构 */
    printf("  架构位数: %d-bit\n", (int)(sizeof(void *) * 8));

    /* 负载 */
#if defined(__linux__)
    double load[3] = {0, 0, 0};
    FILE *lf = fopen("/proc/loadavg", "r");
    if (lf) {
        if (fscanf(lf, "%lf %lf %lf", &load[0], &load[1], &load[2]) == 3) {
            printf("  负载均值: %.2f, %.2f, %.2f\n", load[0], load[1], load[2]);
        }
        fclose(lf);
    }
#endif
}

/* ============ 主函数 ============ */
int main(int argc, char *argv[]) {
    uint64_t timeout = 20;
    uint32_t mem_mb = 16;
    int quiet = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc)
            timeout = (uint64_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
            mem_mb = (uint32_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0)
            quiet = 1;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("用法: %s [-t 秒数] [-m 内存MB] [-q]\n", argv[0]);
            printf("  -t 秒数   每项测试时长 (默认 20)\n");
            printf("  -m 内存MB 内存测试缓冲区, 0=跳过 (默认 16)\n");
            printf("  -q        安静模式, 只输出分数\n");
            return 0;
        }
    }

    if (!quiet) {
        printf("=========================================\n");
        printf(" CPU 性能测试 (Linux Static)\n");
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
    uint64_t mem_bytes = 0;
    if (mem_mb > 0) {
        if (!quiet) printf("\n[4/4] 内存顺序读测试 (%uMB)...\n", mem_mb);
        mem_bytes = bench_mem_read(timeout, mem_mb);
        if (mem_bytes > 0) {
            if (!quiet) {
                printf("  总字节数: %" PRIu64 "\n", mem_bytes);
                printf("  耗时: %" PRIu64 " s\n", timeout);
            }
            printf("  >>> 内存带宽: %" PRIu64 " MiB/s\n",
                   mem_bytes / timeout / 1024 / 1024);
        }
    } else if (!quiet) {
        printf("\n[4/4] 内存顺序读测试: 已跳过 (-m 0)\n");
    }

    /* 汇总 */
    if (!quiet) {
        printf("\n=========================================\n");
        printf(" 汇总分数\n");
        printf("=========================================\n");
        printf("  整数累加: %" PRIu64 " ops/sec\n", int_ops / timeout);
        printf("  整数乘加: %" PRIu64 " ops/sec\n", madd_ops / timeout);
        printf("  标量浮点: %" PRIu64 " flops/sec\n", flt_ops / timeout);
        if (mem_bytes > 0)
            printf("  内存带宽: %" PRIu64 " MiB/s\n",
                   mem_bytes / timeout / 1024 / 1024);
        printf("=========================================\n");
    }

    return 0;
}
