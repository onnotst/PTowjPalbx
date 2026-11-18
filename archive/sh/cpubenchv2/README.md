# cpu_bench — 跨平台 CPU 基准测试

一套**零依赖**的 CPU 跑分工具，覆盖命令行（主力）与浏览器（补充）两种形态。

## 目录结构

```
cpu_bench/
├── cpu_bench.c        # 唯一源码: Linux / macOS / Windows 统一单文件
├── cpu_bench.html     # 浏览器版 (纯前端, 结果可回传)
├── build.sh           # 多架构编译脚本
├── bin/               # 编译产物 (运行 build.sh 后生成)
└── README.md
```

## 一、命令行版（主力）

### 编译

```bash
# 本机编译
gcc -O2 -static -s -o cpu_bench cpu_bench.c

# 一键多架构
bash build.sh           # 自动检测工具链, 编出所有可用架构
bash build.sh -j 4      # 并行编译
bash build.sh clean     # 清理产物
```

#### 依赖安装

| 系统 | 本机 | Linux 交叉 | Windows |
|------|------|-----------|---------|
| Debian/Ubuntu | `apt install gcc build-essential` | `apt install gcc-i686-linux-gnu gcc-x86-64-linux-gnu gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu` | `apt install mingw-w64` |
| Alpine | `apk add build-base` | `apk add gcc-armhf gcc-aarch64` | `apk add mingw-w64-gcc` |
| Arch | `pacman -S gcc` | `pacman -S gcc-multilib gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu` | `pacman -S mingw-w64` |

### 运行

```bash
./cpu_bench            # 每项 20 秒 (默认)
./cpu_bench -t 10      # 每项 10 秒 (快速验证)
./cpu_bench -m 32      # 内存测试用 32MB 缓冲区
./cpu_bench -m 0       # 跳过内存测试
./cpu_bench -q         # 安静模式, 只输出分数 (便于脚本解析)
```

### 产物矩阵

| 文件 | 平台 | 说明 |
|------|------|------|
| `cpu_bench_x86` | Linux x86 (32位) | |
| `cpu_bench_amd64` | Linux x86_64 | |
| `cpu_bench_armhf` | Linux ARM 32位 | |
| `cpu_bench_aarch64` | Linux ARM 64位 | |
| `cpu_bench_win32.exe` | Windows 32位 | MinGW-w64 编译 |
| `cpu_bench_win64.exe` | Windows 64位 | MinGW-w64 编译 |

所有二进制均为**静态链接**，拷到任何同架构机器直接运行，无需安装运行时。

## 二、浏览器版（补充）

适用于无命令行的环境：手机、平板、Windows/Mac 无开发环境、临时给别人跑分。

```bash
# 本地直接打开
双击 cpu_bench.html

# 局域网共享 (推荐, 手机可访问)
python3 -m http.server 8080
# 手机浏览器打开 http://<电脑IP>:8080/cpu_bench.html
```

测四项：整数累加、整数乘加+取模、标量浮点、内存顺序读。

## 三、为什么命令行是主力，浏览器是补充

| 维度 | 命令行 (C 静态二进制) | 浏览器 (JS) |
|------|---------------------|-------------|
| 依赖 | 零（静态编译） | 零（打开即跑） |
| 精度 | 最高（直接机器码） | 高（JIT 优化后接近原生） |
| 服务器/SSH 环境 | ✅ 必需 | ❌ 无桌面 |
| 手机/临时跑分 | 需 Termux | ✅ 直接浏览器 |
| 跨平台一致性 | 同架构即可比 | 同浏览器引擎即可比 |

**结论**：命令行是硬通货，浏览器是便利补充。

## 四、结果回传（可选但默认开启）

跑分结束后，程序会把结果 JSON 用 **Base64** 编码后放进 `message` 字段，POST 到：

```
POST https://api.mblue.ltd/api/allinone/save
Content-Type: application/json

{
  "type": 2,
  "title": "设备测试记录",
  "message": "<Base64(结果JSON)>",
  "client_time": "2026-11-18 18:30:00"
}
```

- `client_time` 为运行机器的本地时间，格式 `YYYY-MM-DD HH:MM:SS`
- `message` 解码（Base64）后即为与 `.json` 文件完全一致的内容
- 上传失败不影响跑分结果，本地 `.json` 依然生成，只打印一行提示

### 关闭 / 改地址

```bash
./cpu_bench -n                              # 不上传
./cpu_bench -u http://1.2.3.4:8080/api/allinone/save
CPU_BENCH_API=https://xxx/api/allinone/save ./cpu_bench
```

优先级：`-u` > 环境变量 `CPU_BENCH_API` > 内置默认地址。

### 各平台实现

| 平台 | 传输方式 | 依赖 |
|------|---------|------|
| Windows | WinHTTP（系统组件，原生 TLS） | 编译加 `-lwinhttp`，无需第三方库 |
| Linux / macOS | 调用 `curl`（缺失时退到 `wget`） | 运行时需 `curl`；二进制本身仍零依赖 |
| 浏览器版 | `fetch()` | 若页面与接口不同源，服务端需放行 CORS |

> Linux 版地址若含空格、引号、`&`、`;`、`|` 等字符会直接拒绝上传（防 shell 注入）。

## 五、横向对比注意事项

1. **同架构、同系统**下的数字才具备可比性
2. 对比时**保持相同的 `-t` 时长**，避免热启动差异
3. 跑分前关闭其他高负载程序
4. 手机端请保持屏幕常亮，后台会降频导致分数偏低
5. Alpine vs glibc 系统：静态链接分别用 musl 和 glibc 编，不要混用
