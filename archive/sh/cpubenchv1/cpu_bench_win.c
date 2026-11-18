#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>

/* ============ 防优化屏障 ============ */
static volatile uint64_t bench_sink = 0;

/* ============ 日志文件 ============ */
static FILE *g_log = NULL;

#define LOG(fmt, ...) do { \
    char _b[8192]; \
    _snprintf(_b, sizeof(_b), fmt, ##__VA_ARGS__); \
    printf("%s", _b); \
    if (g_log) { fputs(_b, g_log); fflush(g_log); } \
} while (0)

#define LOG_Q(fmt, ...) do { \
    char _b[8192]; \
    _snprintf(_b, sizeof(_b), fmt, ##__VA_ARGS__); \
    if (g_log) { fputs(_b, g_log); fflush(g_log); } \
} while (0)

/* ============ 1. 整数累加 ============ */
static uint64_t bench_int_add(uint64_t seconds) {
    uint64_t ops = 0;
    volatile uint64_t s = 0;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    double start = (double)t0.QuadPart / (double)freq.QuadPart;

    while (1) {
        uint64_t i;
        for (i = 1; i <= 1000000; i++) s += i;
        ops += 1000000;
        QueryPerformanceCounter(&t1);
        double now = (double)t1.QuadPart / (double)freq.QuadPart;
        if (now - start >= (double)seconds) break;
    }
    bench_sink = s;
    return ops;
}

/* ============ 2. 整数乘加+取模 ============ */
static uint64_t bench_int_madd(uint64_t seconds) {
    uint64_t ops = 0;
    volatile uint64_t h = 0x811C9DC5;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    double start = (double)t0.QuadPart / (double)freq.QuadPart;

    while (1) {
        uint64_t i;
        for (i = 0; i < 200000; i++) {
            h = (h ^ (i * 0x01000193)) * 0x100000001;
            h ^= h >> 13;
            h = h * 0x5bd1e995;
            h ^= h >> 15;
        }
        ops += 200000;
        QueryPerformanceCounter(&t1);
        double now = (double)t1.QuadPart / (double)freq.QuadPart;
        if (now - start >= (double)seconds) break;
    }
    bench_sink = h;
    return ops;
}

/* ============ 3. 标量浮点 ============ */
static uint64_t bench_float(uint64_t seconds) {
    uint64_t ops = 0;
    volatile double x = 1.0;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    double start = (double)t0.QuadPart / (double)freq.QuadPart;

    while (1) {
        uint64_t i;
        for (i = 0; i < 50000; i++) {
            x = x * 1.0000001 + 0.0000003;
            x = x / 1.00000005 - 0.0000001;
            x = x * 0.9999999 + 0.5;
        }
        ops += 50000 * 3;
        QueryPerformanceCounter(&t1);
        double now = (double)t1.QuadPart / (double)freq.QuadPart;
        if (now - start >= (double)seconds) break;
    }
    bench_sink = (uint64_t)x;
    return ops;
}

/* ============ 4. 内存顺序读 ============ */
static uint64_t bench_mem_read(uint64_t seconds) {
    const uint64_t SIZE = 16 * 1024 * 1024;
    uint32_t *buf = (uint32_t *)malloc(SIZE * sizeof(uint32_t));
    if (!buf) return 0;
    uint64_t i;
    for (i = 0; i < SIZE; i++) buf[i] = (uint32_t)i;
    uint64_t total_bytes = 0;
    volatile uint64_t sum = 0;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    double start = (double)t0.QuadPart / (double)freq.QuadPart;

    while (1) {
        uint64_t j;
        for (j = 0; j < SIZE; j += 8) {
            sum += buf[j] + buf[j+1] + buf[j+2] + buf[j+3]
                 + buf[j+4] + buf[j+5] + buf[j+6] + buf[j+7];
        }
        total_bytes += SIZE * 4;
        QueryPerformanceCounter(&t1);
        double now = (double)t1.QuadPart / (double)freq.QuadPart;
        if (now - start >= (double)seconds) break;
    }
    bench_sink = sum;
    free(buf);
    return total_bytes;
}

/* ============ 系统信息 ============ */
static void get_cpu_model(char *buf, size_t buflen) {
    HKEY hKey;
    DWORD type = REG_SZ;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, &type,
            (LPBYTE)buf, (LPDWORD)&buflen) == ERROR_SUCCESS) {
            /* 去掉多余空格 */
            char *p = buf;
            while (*p) { if (*p == '\n' || *p == '\r') *p = ' '; p++; }
            RegCloseKey(hKey);
            return;
        }
        RegCloseKey(hKey);
    }
    strncpy(buf, "Unknown", buflen);
}

static int get_core_count(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
}

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
#if defined(_WIN64)
    const char *arch_tag = "win64";
#elif defined(_WIN32)
    const char *arch_tag = "win32";
#else
    const char *arch_tag = "win";
#endif

    /* 时间戳 */
    SYSTEMTIME st;
    GetLocalTime(&st);

    char logname[256], jsonname[256];
    _snprintf(logname, sizeof(logname),
        "cpu_bench_%04d%02d%02d_%02d%02d%02d_%s.log",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, arch_tag);
    _snprintf(jsonname, sizeof(jsonname),
        "cpu_bench_%04d%02d%02d_%02d%02d%02d_%s.json",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, arch_tag);

    if (!quiet) g_log = fopen(logname, "w");

    char cpu_model[256];
    get_cpu_model(cpu_model, sizeof(cpu_model));
    int cores = get_core_count();

    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    GetVersionEx((OSVERSIONINFO *)&osvi);
    char os_ver[128];
    _snprintf(os_ver, sizeof(os_ver), "Windows %d.%d Build %d",
        osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);

    if (!quiet) {
        LOG("=========================================\n");
        LOG(" CPU 性能测试 (Windows %s)\n", arch_tag);
        LOG(" 运行时间: %llu 秒\n", (unsigned long long)timeout);
        LOG("=========================================\n\n");

        LOG("[系统信息]\n");
        LOG("  系统版本: %s\n", os_ver);
        LOG("  处理器:   %s\n", cpu_model);
        LOG("  逻辑核心: %d\n", cores);
        LOG("  架构:     %s\n\n", arch_tag);
    } else {
        LOG_Q("=========================================\n");
        LOG_Q(" CPU 性能测试 (Windows %s)\n", arch_tag);
        LOG_Q(" 运行时间: %llu 秒\n", (unsigned long long)timeout);
        LOG_Q("=========================================\n\n");
        LOG_Q("[系统信息]\n");
        LOG_Q("  系统版本: %s\n", os_ver);
        LOG_Q("  处理器:   %s\n", cpu_model);
        LOG_Q("  逻辑核心: %d\n", cores);
        LOG_Q("  架构:     %s\n\n", arch_tag);
    }

    /* 1/4 */
    if (!quiet) LOG("[1/4] 整数累加测试...\n");
    else LOG_Q("[1/4] 整数累加测试...\n");
    uint64_t int_ops = bench_int_add(timeout);
    if (!quiet) {
        LOG("  总操作数: %llu\n", (unsigned long long)int_ops);
        LOG("  耗时: %llu s\n", (unsigned long long)timeout);
        LOG("  >>> 整数累加: %llu ops/sec\n\n", (unsigned long long)(int_ops / timeout));
    } else {
        LOG_Q("  总操作数: %llu\n", (unsigned long long)int_ops);
        LOG_Q("  耗时: %llu s\n", (unsigned long long)timeout);
        LOG_Q("  >>> 整数累加: %llu ops/sec\n\n", (unsigned long long)(int_ops / timeout));
    }

    /* 2/4 */
    if (!quiet) LOG("[2/4] 整数乘加+取模测试...\n");
    else LOG_Q("[2/4] 整数乘加+取模测试...\n");
    uint64_t madd_ops = bench_int_madd(timeout);
    if (!quiet) {
        LOG("  总操作数: %llu\n", (unsigned long long)madd_ops);
        LOG("  耗时: %llu s\n", (unsigned long long)timeout);
        LOG("  >>> 整数乘加: %llu ops/sec\n\n", (unsigned long long)(madd_ops / timeout));
    } else {
        LOG_Q("  总操作数: %llu\n", (unsigned long long)madd_ops);
        LOG_Q("  耗时: %llu s\n", (unsigned long long)timeout);
        LOG_Q("  >>> 整数乘加: %llu ops/sec\n\n", (unsigned long long)(madd_ops / timeout));
    }

    /* 3/4 */
    if (!quiet) LOG("[3/4] 标量浮点测试...\n");
    else LOG_Q("[3/4] 标量浮点测试...\n");
    uint64_t flt_ops = bench_float(timeout);
    if (!quiet) {
        LOG("  总操作数: %llu\n", (unsigned long long)flt_ops);
        LOG("  耗时: %llu s\n", (unsigned long long)timeout);
        LOG("  >>> 浮点吞吐: %llu flops/sec\n\n", (unsigned long long)(flt_ops / timeout));
    } else {
        LOG_Q("  总操作数: %llu\n", (unsigned long long)flt_ops);
        LOG_Q("  耗时: %llu s\n", (unsigned long long)timeout);
        LOG_Q("  >>> 浮点吞吐: %llu flops/sec\n\n", (unsigned long long)(flt_ops / timeout));
    }

    /* 4/4 */
    if (!quiet) LOG("[4/4] 内存顺序读测试 (16MB)...\n");
    else LOG_Q("[4/4] 内存顺序读测试 (16MB)...\n");
    uint64_t mem_bytes = bench_mem_read(timeout);
    if (!quiet) {
        LOG("  总字节数: %llu\n", (unsigned long long)mem_bytes);
        LOG("  耗时: %llu s\n", (unsigned long long)timeout);
        LOG("  >>> 内存带宽: %llu MiB/s\n\n", (unsigned long long)(mem_bytes / timeout / 1024 / 1024));
    } else {
        LOG_Q("  总字节数: %llu\n", (unsigned long long)mem_bytes);
        LOG_Q("  耗时: %llu s\n", (unsigned long long)timeout);
        LOG_Q("  >>> 内存带宽: %llu MiB/s\n\n", (unsigned long long)(mem_bytes / timeout / 1024 / 1024));
    }

    /* 汇总 */
    if (!quiet) {
        LOG("=========================================\n");
        LOG(" 汇总分数\n");
        LOG("=========================================\n");
        LOG("  整数累加: %llu ops/sec\n", (unsigned long long)(int_ops / timeout));
        LOG("  整数乘加: %llu ops/sec\n", (unsigned long long)(madd_ops / timeout));
        LOG("  标量浮点: %llu flops/sec\n", (unsigned long long)(flt_ops / timeout));
        LOG("  内存带宽: %llu MiB/s\n", (unsigned long long)(mem_bytes / timeout / 1024 / 1024));
        LOG("=========================================\n");
    } else {
        LOG_Q("=========================================\n");
        LOG_Q(" 汇总分数\n");
        LOG_Q("=========================================\n");
        LOG_Q("  整数累加: %llu ops/sec\n", (unsigned long long)(int_ops / timeout));
        LOG_Q("  整数乘加: %llu ops/sec\n", (unsigned long long)(madd_ops / timeout));
        LOG_Q("  标量浮点: %llu flops/sec\n", (unsigned long long)(flt_ops / timeout));
        LOG_Q("  内存带宽: %llu MiB/s\n", (unsigned long long)(mem_bytes / timeout / 1024 / 1024));
        LOG_Q("=========================================\n");
    }

    /* JSON */
    FILE *jf = fopen(jsonname, "w");
    if (jf) {
        fprintf(jf, "{\n");
        fprintf(jf, "  \"timestamp\": \"%04d-%02d-%02d %02d:%02d:%02d\",\n",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        fprintf(jf, "  \"arch\": \"%s\",\n", arch_tag);
        fprintf(jf, "  \"os\": \"%s\",\n", os_ver);
        fprintf(jf, "  \"cpu\": \"%s\",\n", cpu_model);
        fprintf(jf, "  \"cores\": %d,\n", cores);
        fprintf(jf, "  \"duration_sec\": %llu,\n", (unsigned long long)timeout);
        fprintf(jf, "  \"results\": {\n");
        fprintf(jf, "    \"int_add\": %llu,\n", (unsigned long long)(int_ops / timeout));
        fprintf(jf, "    \"int_madd\": %llu,\n", (unsigned long long)(madd_ops / timeout));
        fprintf(jf, "    \"float\": %llu,\n", (unsigned long long)(flt_ops / timeout));
        fprintf(jf, "    \"mem_bw_mib\": %llu\n", (unsigned long long)(mem_bytes / timeout / 1024 / 1024));
        fprintf(jf, "  }\n");
        fprintf(jf, "}\n");
        fclose(jf);
    }

    if (g_log) fclose(g_log);

    if (!quiet) {
        printf("\n  结果文件: %s + %s\n", logname, jsonname);
    }

    return 0;
}