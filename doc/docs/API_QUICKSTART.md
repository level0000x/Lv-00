# Lv-00 API 快速入门指南

> **版本**: 1.1.0
> **最后更新**: 2026-06-27
> **适用范围**: Lv-00 公共 API 使用者

---

## 目录

1. [安装与构建](#1-安装与构建)
2. [最小的可工作示例](#2-最小的可工作示例)
3. [高级示例](#3-高级示例)
4. [错误处理模式](#4-错误处理模式)
5. [内存管理模式](#5-内存管理模式)
6. [线程安全说明](#6-线程安全说明)
7. [API 速查表](#7-api-速查表)
8. [常见问题排查](#8-常见问题排查)

---

## 1. 安装与构建

### 1.1 前提条件

| 依赖 | 最低版本 | 说明 |
|------|---------|------|
| CMake | 3.15+ | 构建系统 |
| GMP (libgmp) | 6.0+ | 大整数运算库 |
| C11 编译器 | GCC 7+, Clang 5+, MSVC 2019+ | 编译器 |

### 1.2 Windows 安装

**使用 MSYS2 (推荐)**:

```powershell
# 1. 安装 MSYS2 (https://www.msys2.org/)
# 2. 在 MSYS2 MinGW64 终端中安装依赖
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-gmp mingw-w64-x86_64-make

# 3. 克隆并构建
git clone <lv-repo-url>
cd Lv-00
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 4. 安装（可选，需要管理员权限）
cmake --install . --prefix "C:/Program Files/lv"
```

**使用 MSVC (Visual Studio)**:

```powershell
# 1. 安装 Visual Studio 2019+ 和 vcpkg
# 2. 通过 vcpkg 安装 GMP
vcpkg install gmp:x64-windows

# 3. 构建
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### 1.3 Linux 安装

```bash
# Ubuntu / Debian
sudo apt-get install build-essential cmake libgmp-dev

# Fedora
sudo dnf install gcc cmake gmp-devel make

# Arch Linux
sudo pacman -S base-devel cmake gmp

# 构建
git clone <lv-repo-url>
cd Lv-00
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 安装（可选）
sudo cmake --install .
```

### 1.4 macOS 安装

```bash
# 使用 Homebrew
brew install cmake gmp

# 构建
git clone <lv-repo-url>
cd Lv-00
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.logicalcpu)

# 安装（可选）
sudo cmake --install .
```

### 1.5 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_TESTS` | ON | 构建测试程序 |
| `BUILD_EXAMPLES` | ON | 构建示例程序 |
| `BUILD_SHARED_LIBS` | OFF | 构建共享库（DLL/SO） |
| `BUILD_FUZZERS` | OFF | 构建模糊测试目标（需 Clang） |
| `ENABLE_COVERAGE` | OFF | 启用代码覆盖率 |
| `ENABLE_SANITIZERS` | OFF | 启用 ASan + UBSan 消毒器 |
| `ENABLE_LAYER_VALIDATION` | OFF | 启用编译时层级边界检查 |

```bash
# 示例：开发者构建（带消毒器）
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON -DBUILD_TESTS=ON
```

### 1.6 在项目中使用

**方法 1: CMake `find_package`（安装后）**:

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyApp LANGUAGES C)

find_package(lv 5.0.0 REQUIRED)

add_executable(myapp main.c)
target_link_libraries(myapp lv::lv)
```

**方法 2: pkg-config（安装后）**:

```bash
# 编译
gcc main.c -o myapp $(pkg-config --cflags --libs lv)

# 或在 Makefile 中
CFLAGS += $(shell pkg-config --cflags lv)
LDFLAGS += $(shell pkg-config --libs lv)
```

**方法 3: 直接包含（未安装）**:

```cmake
# 将 Lv-00 源码放在项目的 third_party/ 目录下
add_subdirectory(third_party/Lv-00)
target_link_libraries(myapp lv)
```

---

## 2. 最小的可工作示例

### 2.1 三角形验证

以下代码创建一个直角三角形 (0,0)-(3,0)-(0,4)，验证其面积为 6。

```c
#include "lv/lv.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* === 步骤 1: 初始化系统 === */
    if (!lv_init()) {
        fprintf(stderr, "错误: Lv-00 初始化失败\n");
        return EXIT_FAILURE;
    }

    /* === 步骤 2: 检查版本 === */
    printf("Lv-00 %s\n", lv_get_version_string());

    /* === 步骤 3: 创建引擎 === */
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        fprintf(stderr, "错误: 引擎创建失败\n");
        lv_cleanup();
        return EXIT_FAILURE;
    }

    /* === 步骤 4: 构建几何问题 === */
    /* 直角三角形: A(0,0), B(3,0), C(0,4) */
    int a = lv_add_point(engine, 0, 1, 0, 1);   /* (0, 0) */
    int b = lv_add_point(engine, 3, 1, 0, 1);   /* (3, 0) */
    int c = lv_add_point(engine, 0, 1, 4, 1);   /* (0, 4) */

    int ab = lv_add_line_segment(engine, a, b);
    int bc = lv_add_line_segment(engine, b, c);
    int ca = lv_add_line_segment(engine, c, a);

    /* 验证边存在 */
    if (a < 0 || b < 0 || c < 0 || ab < 0 || bc < 0 || ca < 0) {
        fprintf(stderr, "错误: 节点创建失败\n");
        lv_engine_destroy(engine);
        lv_cleanup();
        return EXIT_FAILURE;
    }

    /* 关联约束: 点属于边 */
    lv_add_constraint_incidence(engine, a, ab);
    lv_add_constraint_incidence(engine, a, ca);
    lv_add_constraint_incidence(engine, b, ab);
    lv_add_constraint_incidence(engine, b, bc);
    lv_add_constraint_incidence(engine, c, bc);
    lv_add_constraint_incidence(engine, c, ca);

    /* === 步骤 5: 求解 === */
    lv_normalize(engine, true);
    EngineSolveResult result = lv_solve(engine);

    /* === 步骤 6: 获取结果 === */
    printf("计算结果:\n");
    printf("  状态: %s\n", result == ENGINE_SOLVE_OK ? "成功" : "失败");

    char info[1024];
    lv_get_system_info(info, sizeof(info));
    printf("  系统信息: %s\n", info);

    /* === 步骤 7: 清理 === */
    lv_engine_destroy(engine);
    lv_cleanup();

    return (result == ENGINE_SOLVE_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

编译并运行:

```bash
# Linux/macOS
gcc -o triangle triangle.c -llv -lgmp
./triangle

# Windows (MinGW)
gcc -o triangle.exe triangle.c -llv -lgmp
./triangle.exe
```

---

## 3. 高级示例

### 3.1 多问题批量处理

同时处理多个几何问题，复用初始化开销:

```c
#include "lv/lv.h"
#include <stdio.h>
#include <stdlib.h>

/** @brief 问题定义 */
typedef struct {
    const char *name;
    int         points[3][4];  /* 每点: x_num, x_den, y_num, y_den */
} GeometryProblem;

int main(void) {
    if (!lv_init()) {
        fprintf(stderr, "初始化失败\n");
        return EXIT_FAILURE;
    }

    /* 设置内存上限，防止失控 */
    lv_set_memory_limit(256 * 1024 * 1024);  /* 256 MB */

    /* 问题列表 */
    GeometryProblem problems[] = {
        {"直角三角形 (3-4-5)",   {{0,1,0,1}, {3,1,0,1}, {0,1,4,1}}},
        {"等边三角形",            {{0,1,0,1}, {1,1,0,1}, {1,2,1,1}}},
        {"等腰直角三角形",        {{0,1,0,1}, {2,1,0,1}, {0,1,2,1}}},
        {"钝角三角形",            {{0,1,0,1}, {5,1,0,1}, {1,1,1,1}}},
        {"退化的线",              {{0,1,0,1}, {1,1,0,1}, {2,1,0,1}}},
    };
    int num_problems = sizeof(problems) / sizeof(problems[0]);

    int success_count = 0;

    for (int i = 0; i < num_problems; i++) {
        printf("[%d/%d] %s ... ", i + 1, num_problems, problems[i].name);

        /* 每个问题使用独立的引擎 */
        lvEngine *engine = lv_engine_create();
        if (!ctx) {
            printf("上下文创建失败\n");
            continue;
        }

        /* 构建问题 */
        int p[3];
        int ok = 1;
        for (int j = 0; j < 3 && ok; j++) {
            p[j] = lv_add_point(engine,
                problems[i].points[j][0], problems[i].points[j][1],
                problems[i].points[j][2], problems[i].points[j][3]);
            if (p[j] < 0) ok = 0;
        }

        if (ok) {
            lv_add_line_segment(engine, p[0], p[1]);
            lv_add_line_segment(engine, p[1], p[2]);
            lv_add_line_segment(engine, p[2], p[0]);
            lv_normalize(engine, true);

            EngineSolveResult res = lv_solve(engine);
            if (res == ENGINE_SOLVE_OK) {
                printf("求解成功\n");
                success_count++;
            } else {
                printf("求解失败 (code=%d)\n", (int)res);
            }
        } else {
            printf("节点创建失败\n");
        }

        lv_engine_destroy(engine);
    }

    printf("\n总计: %d/%d 个问题求解成功\n", success_count, num_problems);

    /* 检查内存统计 */
    MemoryStats stats;
    if (lv_get_memory_stats(&stats)) {
        printf("内存峰值: %zu 字节\n", stats.peak_bytes);
    }

    lv_engine_destroy(engine);
    return EXIT_SUCCESS;
}
```

### 3.2 自定义公理加载

使用公理包系统定义和验证自定义数学理论:

```c
#include "lv/lv.h"
#include <stdio.h>

int main(void) {
    if (!lv_init()) return 1;

    lvEngine *engine = lv_engine_create();
    if (!ctx) { lv_engine_destroy(engine); return 1; }

    /* 加载预定义公理包 */
    /* 注意: 具体 API 请参考 axiom_pkg.h 和 module.h */

    /* --- 假设的公理包加载流程（概念示例，K5 标注：以下 API 未实现，
     * 真实公理包 API 为 axiom_package_create / axiom_package_load，见 axiom_pkg.h） --- */
    /*
    AxiomPackage *pkg = lv_axiom_pkg_load("euclidean_plane");
    if (!pkg) {
        fprintf(stderr, "公理包加载失败\n");
        lv_engine_destroy(engine);
        lv_engine_destroy(engine);
        return 1;
    }

    lv_context_attach_axiom_pkg(ctx, pkg);
    */

    /* 构建并求解问题... */
    int a = lv_add_point(engine, 0, 1, 0, 1);
    int b = lv_add_point(engine, 1, 1, 0, 1);
    int ab = lv_add_line_segment(engine, a, b);

    lv_normalize(engine, true);
    lv_solve(engine);

    char info[1024];
    lv_get_system_info(info, sizeof(info));
    printf("%s\n", info);

    lv_engine_destroy(engine);
    lv_engine_destroy(engine);
    return 0;
}
```

### 3.3 系统信息与健康检查

```c
#include "lv/lv.h"
#include <stdio.h>

int main(void) {
    /* 版本信息（无需初始化） */
    printf("=== Lv-00 版本信息 ===\n");
    printf("版本字符串: %s\n", lv_get_version_string());
    printf("平台: %s\n", lv_platform_name());
    printf("编译器: %s\n", lv_compiler_name());
    printf("架构: %s\n", lv_arch_name());

    /* 初始化后获取详细信息 */
    if (!lv_init()) {
        fprintf(stderr, "初始化失败\n");
        return 1;
    }

    lvVersionInfo ver;
    if (lv_get_version_string(&ver)) {
        printf("\n=== 详细版本 ===\n");
        printf("v%d.%d.%d\n", ver.major, ver.minor, ver.patch);
        printf("构建日期: %s\n", ver.build_date);
        printf("构建时间: %s\n", ver.build_time);
    }

    /* 健康检查 */
    int health = lv_health_check();
    printf("\n=== 健康状态: %d/100 ===\n", health);

    /* 系统信息 */
    char sysinfo[2048];
    int len = lv_get_system_info(sysinfo, sizeof(sysinfo));
    printf("\n=== 系统信息 (%d 字符) ===\n%s\n", len, sysinfo);

    lv_engine_destroy(engine);
    return 0;
}
```

### 3.4 配置优化示例

```c
#include "lv/lv.h"
#include <stdio.h>

int main(void) {
    lv_init();

    /* 查看默认配置 */
    printf("默认配置:\n");
    printf("  重写步数上限: %d\n",
           lv_config_get_int("rewrite.step_limit", -1));
    printf("  日志级别: %d\n", lv_get_log_level());

    /* 调整配置以适应大型问题 */
    lv_config_set_int("rewrite.step_limit", 10000);
    lv_config_set_bool("solver.allow_approximation", true);
    lv_set_memory_limit(1024 * 1024 * 1024);  /* 1 GB */
    lv_set_log_level(2);  /* 仅错误和警告 */

    /* 创建引擎并求解... */
    lvEngine *engine = lv_engine_create();
    if (ctx) {
        /* ... 构建问题 ... */
        lv_engine_destroy(engine);
    }

    /* 已验证的配置 */
    printf("\n当前配置:\n");
    printf("  重写步数上限: %d\n",
           lv_config_get_int("rewrite.step_limit", -1));
    printf("  内存上限: %zu 字节\n", lv_get_memory_limit());

    lv_engine_destroy(engine);
    return 0;
}
```

---

## 4. 错误处理模式

### 4.1 推荐模式: 分层错误检查

```c
lvContext *create_and_solve_safely(void) {
    /* 第 1 层: 系统级初始化 */
    if (!lv_init()) {
        fprintf(stderr, "FATAL: lv_init() 失败\n");
        return NULL;
    }

    /* 第 2 层: 资源分配 */
    lvEngine *engine = lv_engine_create();
    if (!ctx) {
        fprintf(stderr, "ERROR: 上下文创建失败\n");
        lv_engine_destroy(engine);
        return NULL;
    }

    /* 第 3 层: 问题构建（带错误恢复） */
    int a = lv_add_point(engine, 0, 1, 0, 1);
    if (a < 0) {
        fprintf(stderr, "ERROR: 点 A 创建失败\n");
        goto cleanup;
    }

    int b = lv_add_point(engine, 3, 1, 0, 1);
    if (b < 0) {
        fprintf(stderr, "ERROR: 点 B 创建失败\n");
        goto cleanup;
    }

    /* 第 4 层: 求解（可恢复错误） */
    lv_normalize(engine, true);
    EngineSolveResult result = lv_solve(engine);
    if (result != ENGINE_SOLVE_OK) {
        fprintf(stderr, "WARNING: 求解未完全成功 (code=%d)\n", (int)result);
        /* 即使求解失败，也可以获取部分结果 */
    }

    return ctx;  /* 调用者负责销毁 */

cleanup:
    lv_engine_destroy(engine);
    lv_engine_destroy(engine);
    return NULL;
}
```

### 4.2 错误码速查

| 错误码 | 含义 | 处理建议 |
|--------|------|---------|
| `lv_OK` | 成功 | 继续执行 |
| `lv_ERR_PARSE` | 输入解析失败 | 检查输入格式 |
| `lv_ERR_MEMORY` | 内存不足 | 减少问题规模或增加内存上限 |
| `lv_ERR_INVALID_ARG` | 无效参数 | 检查传入参数（如分母为 0） |
| `lv_ERR_TIMEOUT` | 计算超时 | 增大超时值或简化问题 |
| `lv_ERR_STATE` | 状态机违规 | 检查 API 调用顺序 |
| `lv_ERR_OVERFLOW` | 数值溢出 | 减小数值范围 |
| `lv_ERR_NOT_INIT` | 系统未初始化 | 先调用 lv_init() |

### 4.3 防御性编程示例

```c
/* 所有 API 调用前检查初始化状态 */
static void safe_solve(void) {
    if (!(ctx != NULL)) {
        fprintf(stderr, "Lv-00 未初始化\n");
        return;
    }

    lvEngine *engine = lv_engine_create();
    if (!ctx) return;

    /* 健康检查（可选，适合长期运行的应用） */
    int health = lv_health_check();
    if (health < 50) {
        fprintf(stderr, "系统健康评分过低: %d/100\n", health);
        /* 考虑重新初始化 */
    }

    /* ... 继续 ... */

    lv_engine_destroy(engine);
}
```

---

## 5. 内存管理模式

### 5.1 所有权规则

Lv-00 遵循明确的所有权规则:

| 函数类型 | 所有权 | 示例 |
|----------|--------|------|
| `lv_*_create()` | 调用者拥有，负责销毁 | `lv_engine_create()` -> `lv_engine_destroy()` |
| `lv_*_get_*()` | 借出引用，不可释放 | `lv_config_get_string()` |
| `lv_normalize()` | 调用者拥有返回值 | 必须通过 `normalization_result_destroy()` 释放 |
| `lv_engine_destroy(engine)` | 释放所有全局资源 | 最后调用 |

### 5.2 内存使用监控

```c
#include "lv/lv.h"
#include <stdio.h>

int main(void) {
    lv_init();

    /* 设置合理的内存上限 */
    lv_set_memory_limit(512 * 1024 * 1024);  /* 512 MB */

    /* 创建大量上下文，观察内存峰值 */
    for (int i = 0; i < 100; i++) {
        lvContext *eng = lv_init();
        if (!eng) {
            /* 内存上限触发！ */
            fprintf(stderr, "上下文 %d 创建失败（可能达到内存上限）\n", i);

            MemoryStats stats;
            lv_get_memory_stats(&stats);
            printf("当前: %zu / 峰值: %zu / 上限: %zu\n",
                   stats.current_bytes, stats.peak_bytes,
                   lv_get_memory_limit());
            break;
        }
        lv_engine_destroy(eng);
    }

    lv_engine_destroy(engine);
    return 0;
}
```

### 5.3 RAII 风格的 C 包装（资源获取即初始化）

```c
/* 使用 goto cleanup 模式模拟 RAII */
typedef struct {
    lvContext *ctx;
    /* 其他资源 */
} lvSession;

static bool session_init(lvSession *s) {
    memset(s, 0, sizeof(*s));
    if (!lv_init()) return false;
    s->ctx = lv_init();
    return s->ctx != NULL;
}

static void session_destroy(lvSession *s) {
    if (s->engine) lv_engine_destroy(s->engine);
    lv_engine_destroy(engine);
    memset(s, 0, sizeof(*s));
}

/* 使用 */
int main(void) {
    lvSession session;
    if (!session_init(&session)) return 1;

    /* 所有退出路径统一清理 */
    int a = lv_add_point(session.ctx, 0, 1, 0, 1);
    if (a < 0) { session_destroy(&session); return 1; }

    /* ... 工作 ... */

    session_destroy(&session);
    return 0;
}
```

---

## 6. 线程安全说明

### 6.1 线程模型

Lv-00 的线程安全分为三个级别:

| 级别 | 说明 | 操作 |
|------|------|------|
| **全局** | 全局初始化/清理 | 单线程调用 |
| **引擎** | 每个 lvContext 独立 | 不同上下文可并发使用 |
| **引擎内** | 单个上下文不可并发 | 同一引擎必须串行访问 |

### 6.2 安全的多线程模式

```c
#include "lv/lv.h"
#include <pthread.h>
#include <stdio.h>

#define NUM_THREADS 4

typedef struct {
    int              thread_id;
    GeometryProblem *problem;
    EngineSolveResult result;
} ThreadData;

static void *worker_thread(void *arg) {
    ThreadData *td = (ThreadData *)arg;

    /* 每个线程创建自己的引擎 */
    lvEngine *engine = lv_engine_create();
    if (!ctx) {
        fprintf(stderr, "线程 %d: 上下文创建失败\n", td->thread_id);
        return NULL;
    }

    /* 构建并求解（线程本地操作） */
    int a = lv_add_point(engine, 0, 1, 0, 1);
    int b = lv_add_point(engine, 3, 1, 0, 1);
    int c = lv_add_point(engine, 0, 1, 4, 1);

    lv_add_line_segment(engine, a, b);
    lv_add_line_segment(engine, b, c);
    lv_add_line_segment(engine, c, a);

    lv_normalize(engine, true);
    td->result = lv_solve(engine);

    lv_engine_destroy(engine);
    return NULL;
}

int main(void) {
    /* 全局初始化（主线程，创建任何工作线程之前） */
    if (!lv_init()) return 1;

    pthread_t    threads[NUM_THREADS];
    ThreadData   tdata[NUM_THREADS];

    /* 启动工作线程 */
    for (int i = 0; i < NUM_THREADS; i++) {
        tdata[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker_thread, &tdata[i]);
    }

    /* 等待完成 */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        printf("线程 %d: %s\n", i,
               tdata[i].result == ENGINE_SOLVE_OK ? "成功" : "失败");
    }

    /* 全局清理（主线程，所有工作线程结束后） */
    lv_engine_destroy(engine);
    return 0;
}
```

### 6.3 不安全模式（避免！）

```c
/* ❌ 错误: 多个线程共享同一个引擎 */
lvContext *shared_engine = lv_init();

void *bad_worker(void *arg) {
    /* 多个线程同时调用 lv_solve(shared_engine) 是未定义行为 */
    lv_solve(shared_engine);  /* 数据竞争！ */
    return NULL;
}
```

### 6.4 线程安全最佳实践

1. **全局初始化在主线程中执行**，在任何工作线程创建之前
2. **每个线程使用独立的 lvContext 实例**
3. **不要在线程间共享 lvContext 指针**（除非有外部同步机制）
4. **全局清理在所有工作线程结束后执行**
5. **lv_get_version_string() 是线程安全的**，可随时调用

---

## 7. API 速查表

### 7.1 版本 API

| 函数/宏 | 说明 |
|---------|------|
| `lv_get_version_string()` | 获取版本字符串 "1.1.0" |
| `lv_get_version_string(&info)` | 获取详细版本信息结构体 |
| `lv_check_version_compat()` | 验证运行时/编译时版本兼容性 |
| `lv_version_major()` | 编译期主版本号 (1) |
| `lv_version_minor()` | 编译期次版本号 (1) |
| `lv_version_patch()` | 编译期补丁版本号 (0) |

### 7.2 平台信息 API（来自 cross_platform.h）

| 函数 | 说明 |
|------|------|
| `lv_platform_name()` | 平台名称 ("Windows"/"Linux"/"macOS") |
| `lv_compiler_name()` | 编译器名称 ("GCC"/"Clang"/"MSVC") |
| `lv_arch_name()` | 架构名称 ("64-bit"/"32-bit") |
| `lv_endian_name()` | 字节序 ("little-endian"/"big-endian") |
| `lv_stack_size()` | 估计线程栈大小（字节） |
| `lv_cache_line_size()` | 缓存行大小（字节） |

### 7.3 生命周期 API

| 函数 | 说明 |
|------|------|
| `lv_init()` | 全局初始化 |
| `lv_engine_destroy(engine)` | 全局清理 |
| `(ctx != NULL)` | 检查初始化状态 |
| `lv_engine_create()` | 创建引擎实例 |
| `lv_engine_destroy(e)` | 销毁引擎实例 |
| `lv_health_check()` | 健康评分 (0-100) |

### 7.4 几何构造 API

| 函数 | 说明 |
|------|------|
| `lv_add_point(e, x_num, x_den, y_num, y_den)` | 创建有理数坐标点 |
| `lv_add_line_segment(e, p1, p2)` | 创建线段 |
| `lv_add_constraint_incidence(e, pt, ln)` | 关联约束 |

### 7.5 求解 API

| 函数 | 说明 |
|------|------|
| `lv_normalize(e, scope_aware)` | 图归一化 |
| `lv_solve(e)` | 执行求解流水线 |

### 7.6 配置 API

| 函数 | 说明 |
|------|------|
| `lv_config_get_int(key, def)` | 获取整数配置 |
| `lv_config_get_bool(key, def)` | 获取布尔配置 |
| `lv_config_get_double(key, def)` | 获取浮点配置 |
| `lv_config_get_string(key, def)` | 获取字符串配置 |
| `lv_config_set_int(key, val)` | 设置整数配置 |
| `lv_config_set_bool(key, val)` | 设置布尔配置 |
| `lv_config_set_double(key, val)` | 设置浮点配置 |
| `lv_config_set_string(key, val)` | 设置字符串配置 |

### 7.7 日志与调试 API

| 函数 | 说明 |
|------|------|
| `lv_set_log_level(lvl)` | 设置日志级别 (0-4) |
| `lv_get_log_level()` | 获取日志级别 |
| `lv_set_assertions_enabled(v)` | 启用/禁用断言 |
| `lv_are_assertions_enabled()` | 检查断言状态 |

### 7.8 内存管理 API

| 函数 | 说明 |
|------|------|
| `lv_get_memory_stats(&s)` | 内存统计 |
| `lv_set_memory_limit(n)` | 设置内存上限 |
| `lv_get_memory_limit()` | 获取内存上限 |

---

## 8. 常见问题排查

### Q1: 编译时找不到 lv.h

```
fatal error: lv/lv.h: No such file or directory
```

**解决方案**:
- 确保已执行 `cmake --install` 或设置了正确的 include 路径
- 使用 `pkg-config --cflags lv` 获取正确的编译标志
- 在 CMake 项目中使用 `find_package(lv REQUIRED)`

### Q2: 链接时找不到 lv 符号

```
undefined reference to `lv_init'
```

**解决方案**:
- 确保链接了 lv 和 gmp: `-llv -lgmp`
- 链接顺序很重要: lv 必须在前
- 使用 `pkg-config --libs lv` 获取正确的链接标志

### Q3: 运行时崩溃在 lv_solve()

**解决方案**:
- 确保先调用了 `lv_init()`
- 确保上下文不是 NULL
- 检查内存上限是否过低
- 尝试启用消毒器构建: `-DENABLE_SANITIZERS=ON`

### Q4: Windows 上找不到 gmp.h

**解决方案**:
- 使用 MSYS2: `pacman -S mingw-w64-x86_64-gmp`
- 使用 vcpkg: `vcpkg install gmp:x64-windows`
- 手动设置 GMP_ROOT: `-DGMP_ROOT=C:/path/to/gmp`

### Q5: 内存使用持续增长

**解决方案**:
- 确保每个 `lv_engine_create()` 都对应 `lv_engine_destroy()`
- 确保每个 `lv_normalize()` 返回值都被 `normalization_result_destroy()` 释放
- 使用 `lv_get_memory_stats()` 监控内存
- 设置内存上限: `lv_set_memory_limit()`

---

## 更多资源

- [架构规范文档](../../README.md)
- [变更日志](../../CHANGELOG.md)
- [配置参考](../../core/include/lv/config.h)
- [跨平台参考](../../core/include/lv/cross_platform.h)

---

> **提示**: 本指南涵盖最常用的 API。完整的 API 参考请阅读 `include/lv/lv.h` 中的 Doxygen 文档注释。
