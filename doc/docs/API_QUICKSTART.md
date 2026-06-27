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
git clone <lv00-repo-url>
cd Lv-00
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 4. 安装（可选，需要管理员权限）
cmake --install . --prefix "C:/Program Files/lv00"
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
git clone <lv00-repo-url>
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
git clone <lv00-repo-url>
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

find_package(lv00 5.0.0 REQUIRED)

add_executable(myapp main.c)
target_link_libraries(myapp lv00::lv00)
```

**方法 2: pkg-config（安装后）**:

```bash
# 编译
gcc main.c -o myapp $(pkg-config --cflags --libs lv00)

# 或在 Makefile 中
CFLAGS += $(shell pkg-config --cflags lv00)
LDFLAGS += $(shell pkg-config --libs lv00)
```

**方法 3: 直接包含（未安装）**:

```cmake
# 将 Lv-00 源码放在项目的 third_party/ 目录下
add_subdirectory(third_party/Lv-00)
target_link_libraries(myapp lv00)
```

---

## 2. 最小的可工作示例

### 2.1 三角形验证

以下代码创建一个直角三角形 (0,0)-(3,0)-(0,4)，验证其面积为 6。

```c
#include "lv00/lv00.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* === 步骤 1: 初始化系统 === */
    if (!lv00_context_create()) {
        fprintf(stderr, "错误: Lv-00 初始化失败\n");
        return EXIT_FAILURE;
    }

    /* === 步骤 2: 检查版本兼容性 === */
    LV00VersionInfo ver;
    if (lv00_get_version_info(&ver)) {
        printf("Lv-00 v%d.%d.%d (%s, %s)\n\n",
               ver.major, ver.minor, ver.patch,
               ver.platform, ver.compiler);
    }

    /* === 步骤 3: 创建引擎 === */
    LV00Context *ctx = lv00_context_create();
    if (!ctx) {
        fprintf(stderr, "错误: 上下文创建失败\n");
        lv00_context_destroy(ctx);
        return EXIT_FAILURE;
    }

    /* === 步骤 4: 构建几何问题 === */
    /* 直角三角形: A(0,0), B(3,0), C(0,4) */
    int a = lv00_add_point(ctx, 0, 1, 0, 1);   /* (0, 0) */
    int b = lv00_add_point(ctx, 3, 1, 0, 1);   /* (3, 0) */
    int c = lv00_add_point(ctx, 0, 1, 4, 1);   /* (0, 4) */

    int ab = lv00_add_line_segment(ctx, a, b);
    int bc = lv00_add_line_segment(ctx, b, c);
    int ca = lv00_add_line_segment(ctx, c, a);

    /* 验证边存在 */
    if (a < 0 || b < 0 || c < 0 || ab < 0 || bc < 0 || ca < 0) {
        fprintf(stderr, "错误: 节点创建失败\n");
        lv00_context_destroy(ctx);
        lv00_context_destroy(ctx);
        return EXIT_FAILURE;
    }

    /* 关联约束: 点属于边 */
    lv00_add_constraint_incidence(ctx, a, ab);
    lv00_add_constraint_incidence(ctx, a, ca);
    lv00_add_constraint_incidence(ctx, b, ab);
    lv00_add_constraint_incidence(ctx, b, bc);
    lv00_add_constraint_incidence(ctx, c, bc);
    lv00_add_constraint_incidence(ctx, c, ca);

    /* === 步骤 5: 求解 === */
    lv00_normalize(ctx, true);
    EngineSolveResult result = lv00_solve(ctx);

    /* === 步骤 6: 获取结果 === */
    printf("计算结果:\n");
    printf("  状态: %s\n", result == LV00_SOLVE_SUCCESS ? "成功" : "失败");

    char info[1024];
    lv00_get_system_info(info, sizeof(info));
    printf("  系统信息: %s\n", info);

    /* === 步骤 7: 清理 === */
    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);

    return (result == LV00_SOLVE_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

编译并运行:

```bash
# Linux/macOS
gcc -o triangle triangle.c -llv00 -lgmp
./triangle

# Windows (MinGW)
gcc -o triangle.exe triangle.c -llv00 -lgmp
./triangle.exe
```

---

## 3. 高级示例

### 3.1 多问题批量处理

同时处理多个几何问题，复用初始化开销:

```c
#include "lv00/lv00.h"
#include <stdio.h>
#include <stdlib.h>

/** @brief 问题定义 */
typedef struct {
    const char *name;
    int         points[3][4];  /* 每点: x_num, x_den, y_num, y_den */
} GeometryProblem;

int main(void) {
    if (!lv00_context_create()) {
        fprintf(stderr, "初始化失败\n");
        return EXIT_FAILURE;
    }

    /* 设置内存上限，防止失控 */
    lv00_set_memory_limit_ex(256 * 1024 * 1024);  /* 256 MB */

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
        LV00Context *ctx = lv00_context_create();
        if (!ctx) {
            printf("上下文创建失败\n");
            continue;
        }

        /* 构建问题 */
        int p[3];
        int ok = 1;
        for (int j = 0; j < 3 && ok; j++) {
            p[j] = lv00_add_point(ctx,
                problems[i].points[j][0], problems[i].points[j][1],
                problems[i].points[j][2], problems[i].points[j][3]);
            if (p[j] < 0) ok = 0;
        }

        if (ok) {
            lv00_add_line_segment(ctx, p[0], p[1]);
            lv00_add_line_segment(ctx, p[1], p[2]);
            lv00_add_line_segment(ctx, p[2], p[0]);
            lv00_normalize(ctx, true);

            EngineSolveResult res = lv00_solve(ctx);
            if (res == LV00_SOLVE_SUCCESS) {
                printf("求解成功\n");
                success_count++;
            } else {
                printf("求解失败 (code=%d)\n", (int)res);
            }
        } else {
            printf("节点创建失败\n");
        }

        lv00_context_destroy(ctx);
    }

    printf("\n总计: %d/%d 个问题求解成功\n", success_count, num_problems);

    /* 检查内存统计 */
    MemoryStats stats;
    if (lv00_get_memory_stats_ex(&stats)) {
        printf("内存峰值: %zu 字节\n", stats.peak_bytes);
    }

    lv00_context_destroy(ctx);
    return EXIT_SUCCESS;
}
```

### 3.2 自定义公理加载

使用公理包系统定义和验证自定义数学理论:

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    if (!lv00_context_create()) return 1;

    LV00Context *ctx = lv00_context_create();
    if (!ctx) { lv00_context_destroy(ctx); return 1; }

    /* 加载预定义公理包 */
    /* 注意: 具体 API 请参考 axiom_pkg.h 和 module.h */

    /* --- 假设的公理包加载流程（概念示例） --- */
    /*
    AxiomPackage *pkg = lv00_axiom_pkg_load("euclidean_plane");
    if (!pkg) {
        fprintf(stderr, "公理包加载失败\n");
        lv00_context_destroy(ctx);
        lv00_context_destroy(ctx);
        return 1;
    }

    lv00_context_attach_axiom_pkg(ctx, pkg);
    */

    /* 构建并求解问题... */
    int a = lv00_add_point(ctx, 0, 1, 0, 1);
    int b = lv00_add_point(ctx, 1, 1, 0, 1);
    int ab = lv00_add_line_segment(ctx, a, b);

    lv00_normalize(ctx, true);
    lv00_solve(ctx);

    char info[1024];
    lv00_get_system_info(info, sizeof(info));
    printf("%s\n", info);

    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return 0;
}
```

### 3.3 系统信息与健康检查

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    /* 版本信息（无需初始化） */
    printf("=== Lv-00 版本信息 ===\n");
    printf("版本字符串: %s\n", lv00_get_version_string());
    printf("平台: %s\n", lv00_platform_name());
    printf("编译器: %s\n", lv00_compiler_name());
    printf("架构: %s\n", lv00_arch_name());

    /* 初始化后获取详细信息 */
    if (!lv00_context_create()) {
        fprintf(stderr, "初始化失败\n");
        return 1;
    }

    LV00VersionInfo ver;
    if (lv00_get_version_info(&ver)) {
        printf("\n=== 详细版本 ===\n");
        printf("v%d.%d.%d\n", ver.major, ver.minor, ver.patch);
        printf("构建日期: %s\n", ver.build_date);
        printf("构建时间: %s\n", ver.build_time);
    }

    /* 健康检查 */
    int health = lv00_health_check();
    printf("\n=== 健康状态: %d/100 ===\n", health);

    /* 系统信息 */
    char sysinfo[2048];
    int len = lv00_get_system_info(sysinfo, sizeof(sysinfo));
    printf("\n=== 系统信息 (%d 字符) ===\n%s\n", len, sysinfo);

    lv00_context_destroy(ctx);
    return 0;
}
```

### 3.4 配置优化示例

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_context_create();

    /* 查看默认配置 */
    printf("默认配置:\n");
    printf("  重写步数上限: %d\n",
           lv00_config_get_int("rewrite.step_limit", -1));
    printf("  日志级别: %d\n", lv00_get_log_level());

    /* 调整配置以适应大型问题 */
    lv00_config_set_int("rewrite.step_limit", 10000);
    lv00_config_set_bool("solver.allow_approximation", true);
    lv00_set_memory_limit_ex(1024 * 1024 * 1024);  /* 1 GB */
    lv00_set_log_level(2);  /* 仅错误和警告 */

    /* 创建引擎并求解... */
    LV00Context *ctx = lv00_context_create();
    if (ctx) {
        /* ... 构建问题 ... */
        lv00_context_destroy(ctx);
    }

    /* 已验证的配置 */
    printf("\n当前配置:\n");
    printf("  重写步数上限: %d\n",
           lv00_config_get_int("rewrite.step_limit", -1));
    printf("  内存上限: %zu 字节\n", lv00_get_memory_limit_ex());

    lv00_context_destroy(ctx);
    return 0;
}
```

---

## 4. 错误处理模式

### 4.1 推荐模式: 分层错误检查

```c
LV00Context *create_and_solve_safely(void) {
    /* 第 1 层: 系统级初始化 */
    if (!lv00_context_create()) {
        fprintf(stderr, "FATAL: lv00_context_create() 失败\n");
        return NULL;
    }

    /* 第 2 层: 资源分配 */
    LV00Context *ctx = lv00_context_create();
    if (!ctx) {
        fprintf(stderr, "ERROR: 上下文创建失败\n");
        lv00_context_destroy(ctx);
        return NULL;
    }

    /* 第 3 层: 问题构建（带错误恢复） */
    int a = lv00_add_point(ctx, 0, 1, 0, 1);
    if (a < 0) {
        fprintf(stderr, "ERROR: 点 A 创建失败\n");
        goto cleanup;
    }

    int b = lv00_add_point(ctx, 3, 1, 0, 1);
    if (b < 0) {
        fprintf(stderr, "ERROR: 点 B 创建失败\n");
        goto cleanup;
    }

    /* 第 4 层: 求解（可恢复错误） */
    lv00_normalize(ctx, true);
    EngineSolveResult result = lv00_solve(ctx);
    if (result != LV00_SOLVE_SUCCESS) {
        fprintf(stderr, "WARNING: 求解未完全成功 (code=%d)\n", (int)result);
        /* 即使求解失败，也可以获取部分结果 */
    }

    return ctx;  /* 调用者负责销毁 */

cleanup:
    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return NULL;
}
```

### 4.2 错误码速查

| 错误码 | 含义 | 处理建议 |
|--------|------|---------|
| `LV00_OK` | 成功 | 继续执行 |
| `LV00_ERR_PARSE` | 输入解析失败 | 检查输入格式 |
| `LV00_ERR_MEMORY` | 内存不足 | 减少问题规模或增加内存上限 |
| `LV00_ERR_INVALID_ARG` | 无效参数 | 检查传入参数（如分母为 0） |
| `LV00_ERR_TIMEOUT` | 计算超时 | 增大超时值或简化问题 |
| `LV00_ERR_STATE` | 状态机违规 | 检查 API 调用顺序 |
| `LV00_ERR_OVERFLOW` | 数值溢出 | 减小数值范围 |
| `LV00_ERR_NOT_INIT` | 系统未初始化 | 先调用 lv00_context_create() |

### 4.3 防御性编程示例

```c
/* 所有 API 调用前检查初始化状态 */
static void safe_solve(void) {
    if (!(ctx != NULL)) {
        fprintf(stderr, "Lv-00 未初始化\n");
        return;
    }

    LV00Context *ctx = lv00_context_create();
    if (!ctx) return;

    /* 健康检查（可选，适合长期运行的应用） */
    int health = lv00_health_check();
    if (health < 50) {
        fprintf(stderr, "系统健康评分过低: %d/100\n", health);
        /* 考虑重新初始化 */
    }

    /* ... 继续 ... */

    lv00_context_destroy(ctx);
}
```

---

## 5. 内存管理模式

### 5.1 所有权规则

Lv-00 遵循明确的所有权规则:

| 函数类型 | 所有权 | 示例 |
|----------|--------|------|
| `lv00_*_create()` | 调用者拥有，负责销毁 | `lv00_context_create()` -> `lv00_context_destroy()` |
| `lv00_*_get_*()` | 借出引用，不可释放 | `lv00_config_get_string()` |
| `lv00_normalize()` | 调用者拥有返回值 | 必须通过 `normalization_result_free()` 释放 |
| `lv00_context_destroy(ctx)` | 释放所有全局资源 | 最后调用 |

### 5.2 内存使用监控

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_context_create();

    /* 设置合理的内存上限 */
    lv00_set_memory_limit_ex(512 * 1024 * 1024);  /* 512 MB */

    /* 创建大量上下文，观察内存峰值 */
    for (int i = 0; i < 100; i++) {
        LV00Context *eng = lv00_context_create();
        if (!eng) {
            /* 内存上限触发！ */
            fprintf(stderr, "上下文 %d 创建失败（可能达到内存上限）\n", i);

            MemoryStats stats;
            lv00_get_memory_stats_ex(&stats);
            printf("当前: %zu / 峰值: %zu / 上限: %zu\n",
                   stats.current_bytes, stats.peak_bytes,
                   lv00_get_memory_limit_ex());
            break;
        }
        lv00_context_destroy(eng);
    }

    lv00_context_destroy(ctx);
    return 0;
}
```

### 5.3 RAII 风格的 C 包装（资源获取即初始化）

```c
/* 使用 goto cleanup 模式模拟 RAII */
typedef struct {
    LV00Context *ctx;
    /* 其他资源 */
} Lv00Session;

static bool session_init(Lv00Session *s) {
    memset(s, 0, sizeof(*s));
    if (!lv00_context_create()) return false;
    s->ctx = lv00_context_create();
    return s->ctx != NULL;
}

static void session_destroy(Lv00Session *s) {
    if (s->ctx) lv00_context_destroy(s->ctx);
    lv00_context_destroy(ctx);
    memset(s, 0, sizeof(*s));
}

/* 使用 */
int main(void) {
    Lv00Session session;
    if (!session_init(&session)) return 1;

    /* 所有退出路径统一清理 */
    int a = lv00_add_point(session.ctx, 0, 1, 0, 1);
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
| **引擎** | 每个 LV00Context 独立 | 不同上下文可并发使用 |
| **引擎内** | 单个上下文不可并发 | 同一引擎必须串行访问 |

### 6.2 安全的多线程模式

```c
#include "lv00/lv00.h"
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
    LV00Context *ctx = lv00_context_create();
    if (!ctx) {
        fprintf(stderr, "线程 %d: 上下文创建失败\n", td->thread_id);
        return NULL;
    }

    /* 构建并求解（线程本地操作） */
    int a = lv00_add_point(ctx, 0, 1, 0, 1);
    int b = lv00_add_point(ctx, 3, 1, 0, 1);
    int c = lv00_add_point(ctx, 0, 1, 4, 1);

    lv00_add_line_segment(ctx, a, b);
    lv00_add_line_segment(ctx, b, c);
    lv00_add_line_segment(ctx, c, a);

    lv00_normalize(ctx, true);
    td->result = lv00_solve(ctx);

    lv00_context_destroy(ctx);
    return NULL;
}

int main(void) {
    /* 全局初始化（主线程，创建任何工作线程之前） */
    if (!lv00_context_create()) return 1;

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
               tdata[i].result == LV00_SOLVE_SUCCESS ? "成功" : "失败");
    }

    /* 全局清理（主线程，所有工作线程结束后） */
    lv00_context_destroy(ctx);
    return 0;
}
```

### 6.3 不安全模式（避免！）

```c
/* ❌ 错误: 多个线程共享同一个引擎 */
LV00Context *shared_engine = lv00_context_create();

void *bad_worker(void *arg) {
    /* 多个线程同时调用 lv00_solve(shared_engine) 是未定义行为 */
    lv00_solve(shared_engine);  /* 数据竞争！ */
    return NULL;
}
```

### 6.4 线程安全最佳实践

1. **全局初始化在主线程中执行**，在任何工作线程创建之前
2. **每个线程使用独立的 LV00Context 实例**
3. **不要在线程间共享 LV00Context 指针**（除非有外部同步机制）
4. **全局清理在所有工作线程结束后执行**
5. **lv00_get_version_string() 是线程安全的**，可随时调用

---

## 7. API 速查表

### 7.1 版本 API

| 函数/宏 | 说明 |
|---------|------|
| `lv00_get_version_string()` | 获取版本字符串 "1.1.0" |
| `lv00_get_version_info(&info)` | 获取详细版本信息结构体 |
| `lv00_check_version_compat()` | 验证运行时/编译时版本兼容性 |
| `lv00_version_major()` | 编译期主版本号 (1) |
| `lv00_version_minor()` | 编译期次版本号 (1) |
| `lv00_version_patch()` | 编译期补丁版本号 (0) |

### 7.2 平台信息 API（来自 cross_platform.h）

| 函数 | 说明 |
|------|------|
| `lv00_platform_name()` | 平台名称 ("Windows"/"Linux"/"macOS") |
| `lv00_compiler_name()` | 编译器名称 ("GCC"/"Clang"/"MSVC") |
| `lv00_arch_name()` | 架构名称 ("64-bit"/"32-bit") |
| `lv00_endian_name()` | 字节序 ("little-endian"/"big-endian") |
| `lv00_stack_size()` | 估计线程栈大小（字节） |
| `lv00_cache_line_size()` | 缓存行大小（字节） |

### 7.3 生命周期 API

| 函数 | 说明 |
|------|------|
| `lv00_context_create()` | 全局初始化 |
| `lv00_context_destroy(ctx)` | 全局清理 |
| `(ctx != NULL)` | 检查初始化状态 |
| `lv00_context_create()` | 创建上下文实例 |
| `lv00_context_destroy(e)` | 销毁上下文实例 |
| `lv00_health_check()` | 健康评分 (0-100) |

### 7.4 几何构造 API

| 函数 | 说明 |
|------|------|
| `lv00_add_point(e, x_num, x_den, y_num, y_den)` | 创建有理数坐标点 |
| `lv00_add_line_segment(e, p1, p2)` | 创建线段 |
| `lv00_add_constraint_incidence(e, pt, ln)` | 关联约束 |

### 7.5 求解 API

| 函数 | 说明 |
|------|------|
| `lv00_normalize(e, scope_aware)` | 图归一化 |
| `lv00_solve(e)` | 执行求解流水线 |

### 7.6 配置 API

| 函数 | 说明 |
|------|------|
| `lv00_config_get_int(key, def)` | 获取整数配置 |
| `lv00_config_get_bool(key, def)` | 获取布尔配置 |
| `lv00_config_get_double(key, def)` | 获取浮点配置 |
| `lv00_config_get_string(key, def)` | 获取字符串配置 |
| `lv00_config_set_int(key, val)` | 设置整数配置 |
| `lv00_config_set_bool(key, val)` | 设置布尔配置 |
| `lv00_config_set_double(key, val)` | 设置浮点配置 |
| `lv00_config_set_string(key, val)` | 设置字符串配置 |

### 7.7 日志与调试 API

| 函数 | 说明 |
|------|------|
| `lv00_set_log_level(lvl)` | 设置日志级别 (0-4) |
| `lv00_get_log_level()` | 获取日志级别 |
| `lv00_set_assertions_enabled(v)` | 启用/禁用断言 |
| `lv00_are_assertions_enabled()` | 检查断言状态 |

### 7.8 内存管理 API

| 函数 | 说明 |
|------|------|
| `lv00_get_memory_stats_ex(&s)` | 内存统计 |
| `lv00_set_memory_limit_ex(n)` | 设置内存上限 |
| `lv00_get_memory_limit_ex()` | 获取内存上限 |

---

## 8. 常见问题排查

### Q1: 编译时找不到 lv00.h

```
fatal error: lv00/lv00.h: No such file or directory
```

**解决方案**:
- 确保已执行 `cmake --install` 或设置了正确的 include 路径
- 使用 `pkg-config --cflags lv00` 获取正确的编译标志
- 在 CMake 项目中使用 `find_package(lv00 REQUIRED)`

### Q2: 链接时找不到 lv00 符号

```
undefined reference to `lv00_context_create'
```

**解决方案**:
- 确保链接了 lv00 和 gmp: `-llv00 -lgmp`
- 链接顺序很重要: lv00 必须在前
- 使用 `pkg-config --libs lv00` 获取正确的链接标志

### Q3: 运行时崩溃在 lv00_solve()

**解决方案**:
- 确保先调用了 `lv00_context_create()`
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
- 确保每个 `lv00_context_create()` 都对应 `lv00_context_destroy()`
- 确保每个 `lv00_normalize()` 返回值都被 `normalization_result_free()` 释放
- 使用 `lv00_get_memory_stats_ex()` 监控内存
- 设置内存上限: `lv00_set_memory_limit_ex()`

---

## 更多资源

- [架构规范文档](ARCHITECTURE_v3.3.md)
- [变更日志](../CHANGELOG_v3.3.0.md)
- [配置参考](../include/lv00/config.h)
- [跨平台参考](../include/lv00/cross_platform.h)

---

> **提示**: 本指南涵盖最常用的 API。完整的 API 参考请阅读 `include/lv00/lv00.h` 中的 Doxygen 文档注释。
