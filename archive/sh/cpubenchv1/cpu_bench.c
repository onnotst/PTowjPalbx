#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdint.h>
#include <signal.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/utsname.h>
#include <ctype.h>

/* ============ 防优化屏障 ============ */
static volatile uint64_t bench_sink = 0;

/* ============ 1. 整数累加 ============ */
static uint64_t bench_int_add(uint64_t seconds) {
    uint64_t ops = 0;
    volatile uint64_t s = 0;
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    double start = t0.tv_sec + t0.tv_usec / 1e6;

    while (1) {
        uint64_t i;
        for (i = 1; i <= 1000000; i++) s += i;
        ops += 1000000;
        gettimeofday(&t1, NULL);
        double now = t1.tv_sec + t1.tv_usec / 1e6;
        if (now - start >= (double)seconds) break;
    }
    bench_sink = s;
    return ops;
}

/* ============ 2. 整数乘加+取模 ============ */
static uint64_t bench_int_madd(uint64_t seconds) {
    uint64_t ops = 0;
    volatile uint64_t h = 0x811C9DC5;
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    double start = t0.tv_sec + t0.tv_usec / 1e6;

    while (1) {
        uint64_t i;
        for (i = 0; i < 200000; i++) {
            h = (h ^ (i * 0x01000193)) * 0x100000001;
            h ^= h >> 13;
            h = h * 0x5bd1e995;
            h ^= h >> 15;
        }
        ops += 200000;
        gettimeofday(&t1, NULL);
        double now = t1.tv_sec + t1.tv_usec / 1e6;
        if (now - start >= (double)seconds) break;
    }
    bench_sink = h;
    return ops;
}

/* ============ 3. 标量浮点 ============ */
static uint64_t bench_float(uint64_t seconds) {
    uint64_t ops = 0;
    volatile double x = 1.0;
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    double start = t0.tv_sec + t0.tv_usec / 1e6;

    while (1) {
        uint64_t i;
        for (i = 0; i < 50000; i++) {
            x = x * 1.0000001 + 0.0000003;
            x = x / 1.00000005 - 0.0000001;
            x = x * 0.9999999 + 0.5;
        }
        ops += 50000 * 3;
        gettimeofday(&t1, NULL);
        double now = t1.tv_sec + t1.tv_usec / 1e6;
        if (now - start >= (double)seconds) break;
    }
    bench_sink = (uint64_t)x;
    return ops;
}

/* ============ 4. 内存顺序读 ============ */
static uint64_t bench_mem_read(uint64_t seconds) {
    const uint64_t SIZE = 16 * 1024 * 1024;
    uint32_t *buf = malloc(SIZE * sizeof(uint32_t));
    if (!buf) return 0;
    uint64_t i;
    for (i = 0; i < SIZE; i++) buf[i] = (uint32_t)i;
    uint64_t total_bytes = 0;
    volatile uint64_t sum = 0;
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    double start = t0.tv_sec + t0.tv_usec / 1e6;

    while (1) {
        uint64_t j;
        for (j = 0; j < SIZE; j += 8) {
            sum += buf[j] + buf[j+1] + buf[j+2] + buf[j+3]
                 + buf[j+4] + buf[j+5] + buf[j+6] + buf[j+7];
        }
        total_bytes += SIZE * 4;
        gettimeofday(&t1, NULL);
        double now = t1.tv_sec + t1.tv_usec / 1e6;
        if (now - start >= (double)seconds) break;
    }
    bench_sink = sum;
    free(buf);
    return total_bytes;
}

/* ============ 系统信息 ============ */
static void get_os_version(char *buf, size_t buflen) {
    FILE *f = fopen("/etc/os-release", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                char *p = line + 12;
                if (*p == '"') p++;
                p[strcspn(p, "\"\n")] = '\0';
                snprintf(buf, buflen, "%s", p);
                fclose(f);
                return;
            }
        }
        fclose(f);
    }
    struct utsname u;
    if (uname(&u) == 0) snprintf(buf, buflen, "%s %s", u.sysname, u.release);
    else snprintf(buf, buflen, "Unknown");
}

static void get_cpu_model(char *buf, size_t buflen) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *p = strchr(line, ':');
                if (p) {
                    p++; while (*p == ' ') p++;
                    p[strcspn(p, "\n")] = '\0';
                    snprintf(buf, buflen, "%s", p);
                    fclose(f);
                    return;
                }
            }
        }
        fclose(f);
    }
    snprintf(buf, buflen, "Unknown");
}

static int get_core_count(void) {
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
}

static int get_arch_bits(void) {
    return sizeof(void*) * 8;
}

static void get_loadavg(char *buf, size_t buflen) {
    FILE *f = fopen("/proc/loadavg", "r");
    if (f) {
        if (fgets(buf, buflen, f) == NULL) buf[0] = '\0';
        buf[strcspn(buf, " \n")] = '\0';
        fclose(f);
    } else snprintf(buf, buflen, "unknown");
}

/* ============ 日志文件句柄 ============ */
static FILE *log_file = NULL;

static void open_log(const char *arch_tag) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char filename[256];
    snprintf(filename, sizeof(filename),
        "cpu_bench_%04d%02d%02d_%02d%02d%02d_%s.log",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec, arch_tag);

    log_file = fopen(filename, "w");
    if (!log_file) {
        perror("无法创建日志文件");
        return;
    }

    /* 终端 + 文件同时输出 */
    /* 用 tee 方式：把 stdout 复制一份 */
    /* 简单方案：后续所有 printf 之后手动 fputs 到 log_file */
}

static void log_line(const char *line) {
    if (log_file) {
        fputs(line, log_file);
        fflush(log_file);
    }
}

/* printf 包装：同时写终端和文件 */
#define LOG(fmt, ...) do { \
    char _log_buf[2048]; \
    snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__); \
    printf("%s", _log_buf); \
    if (log_file) { fputs(_log_buf, log_file); fflush(log_file); } \
} while(0)

/* ============ 主函数 ============ */
int main(int argc, char *argv[]) {
    uint64_t timeout = 20;
    int quiet = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc)
            timeout = (uint64_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "-q") == 0)
            quiet = 1;
    }

    /* 架构标签 */
    char arch_tag[32] = "unknown";
#if defined(__i386__)
    snprintf(arch_tag, sizeof(arch_tag), "i386");
#elif defined(__x86_64__)
    snprintf(arch_tag, sizeof(arch_tag), "x86_64");
#elif defined(__arm__)
    snprintf(arch_tag, sizeof(arch_tag), "armhf");
#elif defined(__aarch64__)
    snprintf(arch_tag, sizeof(arch_tag), "aarch64");
#endif

    /* 打开日志 */
    if (!quiet) open_log(arch_tag);

    char os_ver[128], cpu_model[256], load[64];
    int cores = get_core_count();
    int bits = get_arch_bits();
    get_os_version(os_ver, sizeof(os_ver));
    get_cpu_model(cpu_model, sizeof(cpu_model));
    get_loadavg(load, sizeof(load));

    struct utsname un;
    char kernel_ver[256] = "";
    if (uname(&un) == 0) snprintf(kernel_ver, sizeof(kernel_ver), "Linux version %s", un.release);

    LOG("=========================================\n");
    LOG(" CPU 性能测试 (Linux Static)\n");
    LOG(" 运行时间: %lu 秒\n", (unsigned long)timeout);
    LOG("=========================================\n\n");

    LOG("[系统信息]\n");
    LOG("  系统版本: %s\n", os_ver);
    LOG("  内核版本: %s\n", kernel_ver);
    LOG("  处理器:   %s\n", cpu_model);
    LOG("  逻辑核心: %d\n", cores);
    LOG("  架构位数: %d-bit\n", bits);
    LOG("  负载均值: %s\n\n", load);

    /* 1/4 */
    LOG("[1/4] 整数累加测试...\n");
    uint64_t int_ops = bench_int_add(timeout);
    LOG("  总操作数: %" PRIu64 "\n", int_ops);
    LOG("  耗时: %lu s\n", (unsigned long)timeout);
    LOG("  >>> 整数累加: %" PRIu64 " ops/sec\n\n", int_ops / timeout);

    /* 2/4 */
    LOG("[2/4] 整数乘加+取模测试...\n");
    uint64_t madd_ops = bench_int_madd(timeout);
    LOG("  总操作数: %" PRIu64 "\n", madd_ops);
    LOG("  耗时: %lu s\n", (unsigned long)timeout);
    LOG("  >>> 整数乘加: %" PRIu64 " ops/sec\n\n", madd_ops / timeout);

    /* 3/4 */
    LOG("[3/4] 标量浮点测试...\n");
    uint64_t flt_ops = bench_float(timeout);
    LOG("  总操作数: %" PRIu64 "\n", flt_ops);
    LOG("  耗时: %lu s\n", (unsigned long)timeout);
    LOG("  >>> 浮点吞吐: %" PRIu64 " flops/sec\n\n", flt_ops / timeout);

    /* 4/4 */
    LOG("[4/4] 内存顺序读测试 (16MB)...\n");
    uint64_t mem_bytes = bench_mem_read(timeout);
    LOG("  总字节数: %" PRIu64 "\n", mem_bytes);
    LOG("  耗时: %lu s\n", (unsigned long)timeout);
    LOG("  >>> 内存带宽: %" PRIu64 " MiB/s\n\n", mem_bytes / timeout / 1024 / 1024);

    /* 汇总 */
    LOG("=========================================\n");
    LOG(" 汇总分数\n");
    LOG("=========================================\n");
    LOG("  整数累加: %" PRIu64 " ops/sec\n", int_ops / timeout);
    LOG("  整数乘加: %" PRIu64 " ops/sec\n", madd_ops / timeout);
    LOG("  标量浮点: %" PRIu64 " flops/sec\n", flt_ops / timeout);
    LOG("  内存带宽: %" PRIu64 " MiB/s\n", mem_bytes / timeout / 1024 / 1024);
    LOG("=========================================\n");

    /* JSON 结果文件 */
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }

    /* 写 JSON */
    time_t now = time(NULL);
    struct tm tm2;
    localtime_r(&now, &tm2);
    char json_filename[256];
    snprintf(json_filename, sizeof(json_filename),
        "cpu_bench_%04d%02d%02d_%02d%02d%02d_%s.json",
        tm2.tm_year + 1900, tm2.tm_mon + 1, tm2.tm_mday,
        tm2.tm_hour, tm2.tm_min, tm2.tm_sec, arch_tag);

    FILE *jf = fopen(json_filename, "w");
    if (jf) {
        fprintf(jf, "{\n");
        fprintf(jf, "  \"timestamp\": \"%04d-%02d-%02d %02d:%02d:%02d\",\n",
            tm2.tm_year + 1900, tm2.tm_mon + 1, tm2.tm_mday,
            tm2.tm_hour, tm2.tm_min, tm2.tm_sec);
        fprintf(jf, "  \"arch\": \"%s\",\n", arch_tag);
        fprintf(jf, "  \"os\": \"%s\",\n", os_ver);
        fprintf(jf, "  \"kernel\": \"%s\",\n", kernel_ver);
        fprintf(jf, "  \"cpu\": \"%s\",\n", cpu_model);
        fprintf(jf, "  \"cores\": %d,\n", cores);
        fprintf(jf, "  \"bits\": %d,\n", bits);
        fprintf(jf, "  \"loadavg\": \"%s\",\n", load);
        fprintf(jf, "  \"duration_sec\": %lu,\n", (unsigned long)timeout);
        fprintf(jf, "  \"results\": {\n");
        fprintf(jf, "    \"int_add\": %" PRIu64 ",\n", int_ops / timeout);
        fprintf(jf, "    \"int_madd\": %" PRIu64 ",\n", madd_ops / timeout);
        fprintf(jf, "    \"float\": %" PRIu64 ",\n", flt_ops / timeout);
        fprintf(jf, "    \"mem_bw_mib\": %" PRIu64 "\n", mem_bytes / timeout / 1024 / 1024);
        fprintf(jf, "  }\n");
        fprintf(jf, "}\n");
        fclose(jf);
        if (!quiet) printf("  结果文件: cpu_bench_*.log + cpu_bench_*.json\n");
    }

    return 0;
}
