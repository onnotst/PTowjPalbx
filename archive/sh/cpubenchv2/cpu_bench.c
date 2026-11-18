/*
 * cpu_bench.c —— 跨平台 CPU 跑分（Linux / macOS / Windows 统一单文件）
 *
 * 四个测试项: 整数累加 / 整数乘加+取模 / 标量浮点 / 内存顺序读
 * 输出: cpu_bench_<时间戳>_<arch>.log + .json
 * 上传: 跑完后把结果 JSON 做 base64 放进 message, POST 到 /api/allinone/save
 *
 * ---------------------------------------------------------------
 * 编译
 *   Linux :
 *     gcc -O2 -static -s -o cpu_bench cpu_bench.c
 *     gcc -O2 -s -o cpu_bench cpu_bench.c          (动态链接, 老 glibc 可加 -lrt)
 *   Windows (MinGW-w64, 需要 -lwinhttp, 系统自带, 无第三方依赖):
 *     x86_64-w64-mingw32-gcc -O2 -static -s cpu_bench.c -o cpu_bench_win64.exe -lwinhttp
 *     i686-w64-mingw32-gcc   -O2 -static -s cpu_bench.c -o cpu_bench_win32.exe -lwinhttp
 *
 * 用法
 *   ./cpu_bench              每项 20 秒, 跑完自动上传
 *   ./cpu_bench -t 10        每项 10 秒
 *   ./cpu_bench -m 32        内存测试用 32MB 缓冲区 (-m 0 跳过内存测试)
 *   ./cpu_bench -q           安静模式, 只输出分数(不建 log, 便于脚本解析)
 *   ./cpu_bench -n           不上传, 只本地生成 json
 *   ./cpu_bench -u <url>     指定上传地址, 覆盖默认值
 *
 * 上传地址优先级: -u 参数 > 环境变量 CPU_BENCH_API > DEFAULT_API_URL
 * 上传方式:
 *   Windows —— WinHTTP (系统组件, 原生 HTTPS)
 *   Linux   —— 调用 curl (缺失时退到 wget), 保证静态二进制不带 libcurl
 *   上传失败不影响跑分结果, 只会打印提示
 * ---------------------------------------------------------------
 */

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#else
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>

#if defined(_WIN32)
#  include <windows.h>
#  include <winhttp.h>
#else
#  include <unistd.h>
#  include <sys/time.h>
#  include <sys/utsname.h>
#  include <sys/wait.h>
#endif

/* ============ 上传配置 ============ */
#define DEFAULT_API_URL "https://api.mblue.ltd/api/allinone/save"
#define API_TYPE        2
#define API_TITLE       "设备测试记录"

/* ============ 防优化屏障 ============ */
static volatile uint64_t bench_sink = 0;

/* ============ 日志 / 输出 ============ */
static FILE *g_log = NULL;
static int   g_quiet = 0;

static void log_write(const char *s)
{
    if (!g_quiet) fputs(s, stdout);
    if (g_log) { fputs(s, g_log); fflush(g_log); }
}

#define LOG(...) do { \
    char _lb[1024]; \
    snprintf(_lb, sizeof(_lb), __VA_ARGS__); \
    log_write(_lb); \
} while (0)

/* ============ 高精度计时 ============ */
static double now_sec(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER freq;
    static int inited = 0;
    LARGE_INTEGER c;
    if (!inited) { QueryPerformanceFrequency(&freq); inited = 1; }
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

/* ============ 1. 整数累加 ============ */
static uint64_t bench_int_add(uint64_t seconds)
{
    uint64_t ops = 0;
    volatile uint64_t s = 0;
    double start = now_sec();

    while (1) {
        uint64_t i;
        for (i = 1; i <= 1000000ULL; i++) s += i;
        ops += 1000000ULL;
        if (now_sec() - start >= (double)seconds) break;
    }
    bench_sink = s;
    return ops;
}

/* ============ 2. 整数乘加 + 移位混淆 ============ */
static uint64_t bench_int_madd(uint64_t seconds)
{
    uint64_t ops = 0;
    volatile uint64_t h = 0x811C9DC5ULL;
    double start = now_sec();

    while (1) {
        uint64_t i;
        for (i = 0; i < 200000ULL; i++) {
            h = (h ^ (i * 0x01000193ULL)) * 0x100000001ULL;
            h ^= h >> 13;
            h = h * 0x5bd1e995ULL;
            h ^= h >> 15;
        }
        ops += 200000ULL;
        if (now_sec() - start >= (double)seconds) break;
    }
    bench_sink = h;
    return ops;
}

/* ============ 3. 标量浮点 ============ */
static uint64_t bench_float(uint64_t seconds)
{
    uint64_t ops = 0;
    volatile double x = 1.0;
    double start = now_sec();

    while (1) {
        uint64_t i;
        for (i = 0; i < 50000ULL; i++) {
            x = x * 1.0000001 + 0.0000003;
            x = x / 1.00000005 - 0.0000001;
            x = x * 0.9999999 + 0.5;
        }
        ops += 50000ULL * 3ULL;
        if (now_sec() - start >= (double)seconds) break;
    }
    bench_sink = (uint64_t)x;
    return ops;
}

/* ============ 4. 内存顺序读 ============ */
static uint64_t bench_mem_read(uint64_t seconds, uint64_t mem_mb)
{
    if (mem_mb == 0) return 0;

    uint64_t count = mem_mb * 1024ULL * 1024ULL / 4ULL;   /* uint32 元素个数 */
    uint32_t *buf = (uint32_t *)malloc((size_t)count * sizeof(uint32_t));
    if (!buf) return 0;

    uint64_t i;
    for (i = 0; i < count; i++) buf[i] = (uint32_t)i;

    uint64_t total_bytes = 0;
    volatile uint64_t sum = 0;
    double start = now_sec();

    while (1) {
        uint64_t j;
        for (j = 0; j + 7 < count; j += 8) {
            sum += buf[j] + buf[j+1] + buf[j+2] + buf[j+3]
                 + buf[j+4] + buf[j+5] + buf[j+6] + buf[j+7];
        }
        total_bytes += count * 4ULL;
        if (now_sec() - start >= (double)seconds) break;
    }
    bench_sink = sum;
    free(buf);
    return total_bytes;
}

/* ============ 系统信息 ============ */
typedef struct {
    char os[192];
    char kernel[192];
    char cpu[192];
    char loadavg[48];
    int  cores;
    int  bits;
    char arch[32];
    char platform[16];
    char timestamp[32];
} SysInfo;

static void local_time_str(char *buf, size_t outlen, const char *fmt)
{
    struct tm tmv;
    time_t t = time(NULL);
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    strftime(buf, outlen, fmt, &tmv);
}

#if defined(_WIN32)
/* 读注册表字符串/数值 */
static void reg_read(const char *subkey, const char *name, char *out, size_t outlen)
{
    HKEY  k;
    char  tmp[256];
    DWORD sz = (DWORD)sizeof(tmp), type = 0;

    out[0] = '\0';
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return;
    if (RegQueryValueExA(k, name, NULL, &type, (LPBYTE)tmp, &sz) == ERROR_SUCCESS) {
        if (type == REG_DWORD) {
            snprintf(out, outlen, "%lu", (unsigned long)(*(DWORD *)tmp));
        } else {
            tmp[sizeof(tmp) - 1] = '\0';
            snprintf(out, outlen, "%s", tmp);
        }
    }
    RegCloseKey(k);
}
#endif

#if !defined(_WIN32)
/* 从 /etc/os-release 或 /proc/cpuinfo 取字段（POSIX） */
static int proc_first_match(const char *path, const char *key, char *out, size_t outlen,
                            int strip_quote)
{
    FILE *f = fopen(path, "r");
    char  line[512];
    size_t kl;
    int   found = 0;

    if (!f) return 0;
    kl = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, kl) == 0) {
            char *p = strchr(line, '=');
            if (!p) p = strchr(line, ':');
            if (!p) continue;
            p++;
            while (*p == ' ' || *p == '\t') p++;
            if (strip_quote && *p == '"') p++;
            p[strcspn(p, "\"\r\n")] = '\0';
            if (*p == '\0') continue;
            snprintf(out, outlen, "%s", p);
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}
#endif /* !_WIN32 */

#if !defined(_WIN32)
/* 执行命令并取回 stdout（用于 macOS 的 sysctl / sw_vers） */
static int run_capture(const char *cmd, char *out, size_t outlen)
{
    FILE *p;
    size_t n = 0;
    int c;

    out[0] = '\0';
    p = popen(cmd, "r");
    if (!p) return -1;
    while ((c = fgetc(p)) != EOF && n + 1 < outlen) out[n++] = (char)c;
    /* 只去掉结尾的换行，保留正文中的换行（curl -w 的状态码标记在最后一行） */
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = '\0';
    out[n] = '\0';
    pclose(p);
    return n > 0 ? 0 : -1;
}
#endif

static void sysinfo_collect(SysInfo *si)
{
    memset(si, 0, sizeof(*si));

    /* 架构标签 + 平台 */
#if defined(_WIN32)
#  if defined(_WIN64)
    snprintf(si->arch, sizeof(si->arch), "win64");
#  else
    snprintf(si->arch, sizeof(si->arch), "win32");
#  endif
    snprintf(si->platform, sizeof(si->platform), "windows");
#elif defined(__APPLE__)
#  if defined(__aarch64__) || defined(__arm64__)
    snprintf(si->arch, sizeof(si->arch), "mac_arm64");
#  else
    snprintf(si->arch, sizeof(si->arch), "mac_x86_64");
#  endif
    snprintf(si->platform, sizeof(si->platform), "macos");
#else
#  if defined(__i386__)
    snprintf(si->arch, sizeof(si->arch), "i386");
#  elif defined(__x86_64__)
    snprintf(si->arch, sizeof(si->arch), "x86_64");
#  elif defined(__aarch64__)
    snprintf(si->arch, sizeof(si->arch), "aarch64");
#  elif defined(__arm__)
    snprintf(si->arch, sizeof(si->arch), "armhf");
#  else
    snprintf(si->arch, sizeof(si->arch), "unknown");
#  endif
    snprintf(si->platform, sizeof(si->platform), "linux");
#endif

    si->bits = (int)(sizeof(void *) * 8);
    local_time_str(si->timestamp, sizeof(si->timestamp), "%Y-%m-%d %H:%M:%S");

#if defined(_WIN32)
    /* ---- Windows ---- */
    {
        SYSTEM_INFO sinfo;
        char prod[128] = "", disp[64] = "", build[48] = "", curver[48] = "";
        const char *k = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

        reg_read(k, "ProductName", prod, sizeof(prod));
        reg_read(k, "DisplayVersion", disp, sizeof(disp));
        reg_read(k, "CurrentBuildNumber", build, sizeof(build));
        reg_read(k, "CurrentVersion", curver, sizeof(curver));

        snprintf(si->os, sizeof(si->os), "%s%s%s Build %s",
                 prod[0] ? prod : "Windows",
                 disp[0] ? " " : "", disp,
                 build[0] ? build : "?");
        snprintf(si->kernel, sizeof(si->kernel), "Windows NT %s Build %s",
                 curver[0] ? curver : "10.0", build[0] ? build : "?");

        reg_read("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                 "ProcessorNameString", si->cpu, sizeof(si->cpu));
        if (!si->cpu[0]) snprintf(si->cpu, sizeof(si->cpu), "Unknown");
        {
            char *p = si->cpu;
            while (*p) { if (*p == '\n' || *p == '\r' || *p == '\t') *p = ' '; p++; }
            /* 去掉首尾空白 */
            p = si->cpu;
            while (*p == ' ') p++;
            if (p != si->cpu) memmove(si->cpu, p, strlen(p) + 1);
        }

        GetSystemInfo(&sinfo);
        si->cores = (int)sinfo.dwNumberOfProcessors;
        snprintf(si->loadavg, sizeof(si->loadavg), "n/a");
    }
#elif defined(__APPLE__)
    /* ---- macOS ---- */
    {
        char v[64];
        if (run_capture("sysctl -n machdep.cpu.brand_string", si->cpu, sizeof(si->cpu)) != 0)
            snprintf(si->cpu, sizeof(si->cpu), "Apple Silicon / Unknown");
        if (run_capture("sw_vers -productVersion", v, sizeof(v)) == 0)
            snprintf(si->os, sizeof(si->os), "macOS %s", v);
        else
            snprintf(si->os, sizeof(si->os), "macOS");
        if (run_capture("uname -r", v, sizeof(v)) == 0)
            snprintf(si->kernel, sizeof(si->kernel), "Darwin %s", v);
        si->cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
        snprintf(si->loadavg, sizeof(si->loadavg), "n/a");
    }
#else
    /* ---- Linux ---- */
    {
        struct utsname un;
        if (proc_first_match("/etc/os-release", "PRETTY_NAME=", si->os, sizeof(si->os), 1) == 0) {
            if (uname(&un) == 0)
                snprintf(si->os, sizeof(si->os), "%s %s", un.sysname, un.release);
            else
                snprintf(si->os, sizeof(si->os), "Unknown");
        }

        if (uname(&un) == 0) {
            snprintf(si->kernel, sizeof(si->kernel), "%s %s", un.sysname, un.release);
            snprintf(si->platform, sizeof(si->platform), "%s", "linux");
        } else {
            snprintf(si->kernel, sizeof(si->kernel), "Unknown");
        }

        /* cpu 型号: x86 走 model name, ARM 走 Model / Hardware */
        if (proc_first_match("/proc/cpuinfo", "model name", si->cpu, sizeof(si->cpu), 0) == 0 &&
            proc_first_match("/proc/cpuinfo", "Model", si->cpu, sizeof(si->cpu), 0) == 0 &&
            proc_first_match("/proc/cpuinfo", "Hardware", si->cpu, sizeof(si->cpu), 0) == 0)
            snprintf(si->cpu, sizeof(si->cpu), "Unknown");

        si->cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
        {
            FILE *f = fopen("/proc/loadavg", "r");
            if (!f || fscanf(f, "%47s", si->loadavg) != 1)
                snprintf(si->loadavg, sizeof(si->loadavg), "n/a");
            if (f) fclose(f);
        }
    }
#endif
}

/* ============ JSON 工具 ============ */
static void json_escape(const char *in, char *out, size_t outlen)
{
    size_t o = 0;

    if (outlen == 0) return;
    if (!in) { out[0] = '\0'; return; }

    for (; *in && o + 1 < outlen; in++) {
        unsigned char c = (unsigned char)*in;
        if (c == '"' || c == '\\') {
            if (o + 2 >= outlen) break;
            out[o++] = '\\'; out[o++] = (char)c;
        } else if (c == '\n') {
            if (o + 2 >= outlen) break;
            out[o++] = '\\'; out[o++] = 'n';
        } else if (c == '\r') {
            if (o + 2 >= outlen) break;
            out[o++] = '\\'; out[o++] = 'r';
        } else if (c == '\t') {
            if (o + 2 >= outlen) break;
            out[o++] = '\\'; out[o++] = 't';
        } else if (c < 0x20) {
            if (o + 6 >= outlen) break;
            snprintf(out + o, 7, "\\u%04x", (unsigned)c);
            o += 6;
        } else {
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
}

/* ============ Base64 ============ */
static void base64_encode(const unsigned char *in, size_t inlen, char *out)
{
    static const char T[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i = 0, o = 0;

    while (i + 3 <= inlen) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | (uint32_t)in[i + 2];
        out[o++] = T[(v >> 18) & 0x3F];
        out[o++] = T[(v >> 12) & 0x3F];
        out[o++] = T[(v >> 6) & 0x3F];
        out[o++] = T[v & 0x3F];
        i += 3;
    }
    if (inlen - i == 1) {
        uint32_t v = (uint32_t)in[i] << 16;
        out[o++] = T[(v >> 18) & 0x3F];
        out[o++] = T[(v >> 12) & 0x3F];
        out[o++] = '=';
        out[o++] = '=';
    } else if (inlen - i == 2) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8);
        out[o++] = T[(v >> 18) & 0x3F];
        out[o++] = T[(v >> 12) & 0x3F];
        out[o++] = T[(v >> 6) & 0x3F];
        out[o++] = '=';
    }
    out[o] = '\0';
}

/* ============ 结果结构 ============ */
typedef struct {
    uint64_t duration_sec;
    uint64_t mem_mb;
    uint64_t int_add;
    uint64_t int_madd;
    uint64_t flt;
    uint64_t mem_bw_mib;
} Results;

static void write_json_file(const char *path, const SysInfo *si, const Results *r)
{
    char e_os[256], e_kernel[256], e_cpu[256], e_load[64];
    FILE *jf;

    json_escape(si->os, e_os, sizeof(e_os));
    json_escape(si->kernel, e_kernel, sizeof(e_kernel));
    json_escape(si->cpu, e_cpu, sizeof(e_cpu));
    json_escape(si->loadavg, e_load, sizeof(e_load));

    jf = fopen(path, "w");
    if (!jf) return;

    fprintf(jf, "{\n");
    fprintf(jf, "  \"timestamp\": \"%s\",\n", si->timestamp);
    fprintf(jf, "  \"source\": \"cli\",\n");
    fprintf(jf, "  \"platform\": \"%s\",\n", si->platform);
    fprintf(jf, "  \"arch\": \"%s\",\n", si->arch);
    fprintf(jf, "  \"os\": \"%s\",\n", e_os);
    fprintf(jf, "  \"kernel\": \"%s\",\n", e_kernel);
    fprintf(jf, "  \"cpu\": \"%s\",\n", e_cpu);
    fprintf(jf, "  \"cores\": %d,\n", si->cores);
    fprintf(jf, "  \"bits\": %d,\n", si->bits);
    fprintf(jf, "  \"loadavg\": \"%s\",\n", e_load);
    fprintf(jf, "  \"duration_sec\": %llu,\n", (unsigned long long)r->duration_sec);
    fprintf(jf, "  \"mem_mb\": %llu,\n", (unsigned long long)r->mem_mb);
    fprintf(jf, "  \"results\": {\n");
    fprintf(jf, "    \"int_add\": %llu,\n", (unsigned long long)r->int_add);
    fprintf(jf, "    \"int_madd\": %llu,\n", (unsigned long long)r->int_madd);
    fprintf(jf, "    \"float\": %llu,\n", (unsigned long long)r->flt);
    fprintf(jf, "    \"mem_bw_mib\": %llu\n", (unsigned long long)r->mem_bw_mib);
    fprintf(jf, "  }\n");
    fprintf(jf, "}\n");
    fclose(jf);
}

static void build_json_compact(const SysInfo *si, const Results *r, char *out, size_t outlen)
{
    char e_os[256], e_kernel[256], e_cpu[256], e_load[64];

    json_escape(si->os, e_os, sizeof(e_os));
    json_escape(si->kernel, e_kernel, sizeof(e_kernel));
    json_escape(si->cpu, e_cpu, sizeof(e_cpu));
    json_escape(si->loadavg, e_load, sizeof(e_load));

    snprintf(out, outlen,
        "{\"timestamp\":\"%s\",\"source\":\"cli\",\"platform\":\"%s\",\"arch\":\"%s\","
        "\"os\":\"%s\",\"kernel\":\"%s\",\"cpu\":\"%s\",\"cores\":%d,\"bits\":%d,"
        "\"loadavg\":\"%s\",\"duration_sec\":%llu,\"mem_mb\":%llu,"
        "\"results\":{\"int_add\":%llu,\"int_madd\":%llu,\"float\":%llu,\"mem_bw_mib\":%llu}}",
        si->timestamp, si->platform, si->arch,
        e_os, e_kernel, e_cpu, si->cores, si->bits,
        e_load,
        (unsigned long long)r->duration_sec, (unsigned long long)r->mem_mb,
        (unsigned long long)r->int_add, (unsigned long long)r->int_madd,
        (unsigned long long)r->flt, (unsigned long long)r->mem_bw_mib);
}

/* ============ 上传 ============ */
#if !defined(_WIN32)
/* 地址合法性检查：只允许 URL 安全字符，防止在 POSIX 下拼进 shell 造成注入 */
static int url_is_safe(const char *url)
{
    const char *p;
    size_t len;

    if (!url || !*url) return 0;
    len = strlen(url);
    if (len < 10 || len > 1024) return 0;
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) return 0;

    for (p = url; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c)) continue;
        if (strchr("-._~:/?#[]@!()*+,=%", (int)c)) continue;
        return 0;   /* 空格/引号/反引号/&/;/|/</>/\\/$ 一律拒绝 */
    }
    return 1;
}
#endif /* !_WIN32 */

#if defined(_WIN32)
static void to_wide(const char *s, wchar_t *w, size_t wcount)
{
    if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, w, (int)wcount) && wcount)
        w[0] = L'\0';
}

/* http(s)://host[:port]/path -> 拆分 */
static void url_split(const char *url, int *secure, char *host, size_t hostlen,
                      int *port, char *path, size_t pathlen)
{
    const char *p, *slash;
    size_t n;

    *secure = (strncmp(url, "https://", 8) == 0) ? 1 : 0;
    *port = 0;
    p = url + (*secure ? 8 : 7);

    slash = strchr(p, '/');
    n = slash ? (size_t)(slash - p) : strlen(p);
    if (n >= hostlen) n = hostlen - 1;
    memcpy(host, p, n);
    host[n] = '\0';

    /* 端口 */
    {
        char *colon = strrchr(host, ':');
        if (colon && *(colon + 1)) { *port = atoi(colon + 1); *colon = '\0'; }
    }

    if (slash) snprintf(path, pathlen, "%s", slash);
    else       snprintf(path, pathlen, "/");
}
#endif

/* 返回 0 表示 HTTP 2xx 成功；detail 带回状态说明 */
static int http_post_json(const char *url, const char *payload, char *detail, size_t detaillen)
{
#if defined(_WIN32)
    char    host[256], path[512], body[1024];
    int     secure = 0, port = 0, ret = -1;
    DWORD   status = 0;
    wchar_t whost[256], wpath[512];
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;

    url_split(url, &secure, host, sizeof(host), &port, path, sizeof(path));
    if (!host[0]) { snprintf(detail, detaillen, "地址无效"); return -1; }

    to_wide(host, whost, 256);
    to_wide(path, wpath, 512);

    hSession = WinHttpOpen(L"cpu_bench/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { snprintf(detail, detaillen, "WinHTTP 初始化失败"); goto done; }

    WinHttpSetTimeouts(hSession, 5000, 5000, 15000, 15000);

    hConnect = WinHttpConnect(hSession, whost,
                              (INTERNET_PORT)(port ? port : (secure ? 443 : 80)), 0);
    if (!hConnect) { snprintf(detail, detaillen, "连接失败"); goto done; }

    hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath, NULL,
                                  WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  secure ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { snprintf(detail, detaillen, "创建请求失败"); goto done; }

    if (!WinHttpSendRequest(hRequest,
            L"Content-Type: application/json; charset=utf-8\r\nAccept: application/json\r\n",
            (DWORD)-1L, (LPVOID)payload, (DWORD)strlen(payload),
            (DWORD)strlen(payload), 0)) {
        snprintf(detail, detaillen, "发送失败 (err %lu)", (unsigned long)GetLastError());
        goto done;
    }
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        snprintf(detail, detaillen, "无响应 (err %lu)", (unsigned long)GetLastError());
        goto done;
    }

    {
        DWORD sz = (DWORD)sizeof(status);
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz,
                            WINHTTP_NO_HEADER_INDEX);
    }

    /* 读回响应前 512 字节，便于排查 */
    body[0] = '\0';
    {
        DWORD avail = 0, got = 0, cap = (DWORD)sizeof(body) - 1;
        if (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
            if (avail < cap) cap = avail;
            if (WinHttpReadData(hRequest, body, cap, &got)) body[got] = '\0';
        }
    }

    ret = (status >= 200 && status < 300) ? 0 : -2;
    if (ret == 0)
        snprintf(detail, detaillen, "HTTP %lu", (unsigned long)status);
    else
        snprintf(detail, detaillen, "HTTP %lu %s", (unsigned long)status,
                 body[0] ? body : "");

done:
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    return ret;
#else
    char  tmp[512], cmd[2048], out[1024];
    const char *dir;
    int   fd, ret = -1;
    size_t plen;

    if (!url_is_safe(url)) { snprintf(detail, detaillen, "地址含不安全字符，已拒绝上传"); return -1; }

    dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";
    snprintf(tmp, sizeof(tmp), "%s/cpu_bench_body_XXXXXX", dir);
    fd = mkstemp(tmp);
    if (fd < 0) {   /* 退到当前目录 */
        snprintf(tmp, sizeof(tmp), "cpu_bench_body_XXXXXX");
        fd = mkstemp(tmp);
    }
    if (fd < 0) { snprintf(detail, detaillen, "无法创建临时文件"); return -1; }

    plen = strlen(payload);
    if (write(fd, payload, plen) != (ssize_t)plen) {
        close(fd); unlink(tmp);
        snprintf(detail, detaillen, "写临时文件失败");
        return -1;
    }
    close(fd);

    /* 首选 curl：-w 带回 HTTP 状态码 */
    snprintf(cmd, sizeof(cmd),
        "curl -sS -m 20 -X POST -H 'Content-Type: application/json' "
        "--data-binary @'%s' -w '\\n__HTTP_CODE__%%{http_code}' '%s' 2>&1", tmp, url);

    if (run_capture(cmd, out, sizeof(out)) == 0 || out[0] != '\0') {
        char *p = strstr(out, "__HTTP_CODE__");
        if (p) {
            int code = atoi(p + 13);
            if (code >= 200 && code < 300) {
                ret = 0;
                snprintf(detail, detaillen, "HTTP %d", code);
            } else if (code == 0) {
                ret = -1;
                snprintf(detail, detaillen, "无法连接或请求中断 (HTTP 000，检查网络/代理)" );
            } else {
                ret = -2;
                snprintf(detail, detaillen, "HTTP %d", code);
            }
            unlink(tmp);
            return ret;
        }
    }

    /* curl 不可用时退到 wget */
    snprintf(cmd, sizeof(cmd),
        "wget -q -T 20 -O - --header='Content-Type: application/json' "
        "--post-file='%s' '%s' 2>&1", tmp, url);

    if (run_capture(cmd, out, sizeof(out)) == 0) {
        ret = 0;
        snprintf(detail, detaillen, "HTTP 200 (wget)");
    } else {
        ret = -1;
        snprintf(detail, detaillen, "上传失败: curl/wget 均不可用或请求失败 (%.300s)",
                 out[0] ? out : "no output");
    }

    unlink(tmp);
    return ret;
#endif
}

/* ============ 帮助 ============ */
static void usage(void)
{
    printf("cpu_bench —— 跨平台 CPU 跑分\n\n");
    printf("  -t <秒>   每项测试时长, 默认 20\n");
    printf("  -m <MB>   内存测试缓冲区大小, 默认 16 (-m 0 跳过内存测试)\n");
    printf("  -q        安静模式 (不建 log, 只输出分数)\n");
    printf("  -n        不上传结果, 只本地生成 json\n");
    printf("  -u <url>  上传地址, 默认 %s\n", DEFAULT_API_URL);
    printf("\n上传地址也可用环境变量 CPU_BENCH_API 指定\n");
}

/* ============ 主函数 ============ */
int main(int argc, char *argv[])
{
    uint64_t    duration = 20;
    uint64_t    mem_mb   = 16;
    int         do_upload = 1;
    const char *api_url;
    SysInfo     si;
    Results     r;
    char        logname[512], jsonname[512];
    char        compact[1536];
    int         i;

#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    api_url = NULL;
    memset(&r, 0, sizeof(r));
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-t") && i + 1 < argc) {
            duration = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "-m") && i + 1 < argc) {
            mem_mb = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "-u") && i + 1 < argc) {
            api_url = argv[++i];
        } else if (!strcmp(argv[i], "-q")) {
            g_quiet = 1;
        } else if (!strcmp(argv[i], "-n")) {
            do_upload = 0;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage();
            return 0;
        }
    }
    if (duration == 0) duration = 1;
    if (!api_url) api_url = getenv("CPU_BENCH_API");
    if (!api_url || !*api_url) api_url = DEFAULT_API_URL;

    sysinfo_collect(&si);

    /* 文件名: cpu_bench_YYYYMMDD_HHMMSS_<arch>.log / .json */
    {
        char stamp[32];
        struct tm tmv;
        time_t t = time(NULL);
#if defined(_WIN32)
        localtime_s(&tmv, &t);
#else
        localtime_r(&t, &tmv);
#endif
        strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tmv);
        snprintf(logname, sizeof(logname), "cpu_bench_%s_%s.log", stamp, si.arch);
        snprintf(jsonname, sizeof(jsonname), "cpu_bench_%s_%s.json", stamp, si.arch);
    }

    if (!g_quiet) g_log = fopen(logname, "w");

    /* 头部 */
    LOG("=========================================\n");
    LOG(" CPU 性能测试 (%s)\n", si.platform);
    LOG(" 运行时间: %llu 秒/项\n", (unsigned long long)duration);
    LOG("=========================================\n\n");

    LOG("[系统信息]\n");
    LOG("  系统版本: %s\n", si.os);
    LOG("  内核版本: %s\n", si.kernel);
    LOG("  处理器:   %s\n", si.cpu);
    LOG("  逻辑核心: %d\n", si.cores);
    LOG("  架构位数: %d-bit (%s)\n", si.bits, si.arch);
    LOG("  负载均值: %s\n\n", si.loadavg);

    /* 1/4 */
    LOG("[1/4] 整数累加测试...\n");
    {
        uint64_t ops = bench_int_add(duration);
        r.int_add = ops / duration;
        LOG("  总操作数: %llu\n", (unsigned long long)ops);
        LOG("  >>> 整数累加: %llu ops/sec\n\n", (unsigned long long)r.int_add);
    }

    /* 2/4 */
    LOG("[2/4] 整数乘加+移位测试...\n");
    {
        uint64_t ops = bench_int_madd(duration);
        r.int_madd = ops / duration;
        LOG("  总操作数: %llu\n", (unsigned long long)ops);
        LOG("  >>> 整数乘加: %llu ops/sec\n\n", (unsigned long long)r.int_madd);
    }

    /* 3/4 */
    LOG("[3/4] 标量浮点测试...\n");
    {
        uint64_t ops = bench_float(duration);
        r.flt = ops / duration;
        LOG("  总操作数: %llu\n", (unsigned long long)ops);
        LOG("  >>> 浮点吞吐: %llu flops/sec\n\n", (unsigned long long)r.flt);
    }

    /* 4/4 */
    if (mem_mb > 0) {
        LOG("[4/4] 内存顺序读测试 (%lluMB)...\n", (unsigned long long)mem_mb);
        {
            uint64_t bytes = bench_mem_read(duration, mem_mb);
            r.mem_bw_mib = bytes / duration / 1024 / 1024;
            LOG("  总字节数: %llu\n", (unsigned long long)bytes);
            LOG("  >>> 内存带宽: %llu MiB/s\n\n", (unsigned long long)r.mem_bw_mib);
        }
    } else {
        LOG("[4/4] 内存测试已跳过 (-m 0)\n\n");
        r.mem_bw_mib = 0;
    }

    r.duration_sec = duration;
    r.mem_mb       = mem_mb;

    /* 汇总 */
    LOG("=========================================\n");
    LOG(" 汇总分数\n");
    LOG("=========================================\n");
    LOG("  整数累加: %llu ops/sec\n", (unsigned long long)r.int_add);
    LOG("  整数乘加: %llu ops/sec\n", (unsigned long long)r.int_madd);
    LOG("  标量浮点: %llu flops/sec\n", (unsigned long long)r.flt);
    LOG("  内存带宽: %llu MiB/s\n", (unsigned long long)r.mem_bw_mib);
    LOG("=========================================\n");

    /* 落盘 */
    write_json_file(jsonname, &si, &r);
    if (!g_quiet) printf("\n  结果文件: %s + %s\n", logname, jsonname);

    /* 上传 */
    if (do_upload) {
        char b64[4096];
        char payload[6144];
        char detail[512];

        build_json_compact(&si, &r, compact, sizeof(compact));
        base64_encode((const unsigned char *)compact, strlen(compact), b64);

        snprintf(payload, sizeof(payload),
            "{\"type\":%d,\"title\":\"%s\",\"message\":\"%s\",\"client_time\":\"%s\"}",
            API_TYPE, API_TITLE, b64, si.timestamp);

        LOG("\n[上传] %s\n", api_url);

        detail[0] = '\0';
        int rc = http_post_json(api_url, payload, detail, sizeof(detail));
        if (rc == 0) {
            LOG("[上传] 成功 (%s), message 为 base64 编码的结果 JSON\n", detail);
            if (!g_quiet) printf("  提示: message 字段为 base64, 解码后即为 %s 的内容\n", jsonname);
        } else {
            LOG("[上传] 失败: %s\n", detail);
            if (!g_quiet) printf("  结果已保存在 %s, 可稍后手动提交\n", jsonname);
        }
    }

    if (g_log) { fclose(g_log); g_log = NULL; }

    return 0;
}
