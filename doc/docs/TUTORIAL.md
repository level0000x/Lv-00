# Lv-00 入门教程

> **版本**: 1.1.0  
> **最后更新**: 2026-07-23  
> **难度**: 初级到中级

---

## 目录

1. [环境搭建](#1-环境搭建)
2. [第一个程序](#2-第一个程序)
3. [基础几何构造](#3-基础几何构造)
4. [约束与求解](#4-约束与求解)
5. [证明系统](#5-证明系统)
6. [预设模块](#6-预设模块)
7. [进阶主题](#7-进阶主题)
8. [调试与优化](#8-调试与优化)

---

## 1. 环境搭建

### 1.1 安装依赖

**Ubuntu/Debian**:
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libgmp-dev
```

**macOS**:
```bash
brew install cmake gmp
```

**Windows (MSYS2)**:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-gmp
```

### 1.2 构建 Lv-00

```bash
# 克隆仓库
git clone https://github.com/level0000x/Lv-00.git
cd Lv-00

# 创建构建目录
mkdir build && cd build

# 配置
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build . -j$(nproc)

# 运行测试
ctest --output-on-failure
```

### 1.3 安装（可选）

```bash
# Linux/macOS
sudo cmake --install .

# Windows (管理员 PowerShell)
cmake --install . --prefix "C:/Program Files/lv"
```

---

## 2. 第一个程序

### 2.1 Hello Lv-00

创建 `hello_lv.c`:

```c
#include "lv/lv.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* 初始化系统 */
    if (!lv_engine_create()) {
        fprintf(stderr, "错误: Lv-00 初始化失败\n");
        return EXIT_FAILURE;
    }

    /* 打印版本信息 */
    printf("Lv-00 版本: %s\n", lv_get_version_string());

    /* 创建引擎 */
    lvEngine *ctx = lv_engine_create();
    if (!ctx) {
        fprintf(stderr, "错误: 上下文创建失败\n");
        lv_engine_destroy(ctx);
        return EXIT_FAILURE;
    }

    printf("上下文创建成功!\n");

    /* 获取系统信息 */
    char info[1024];
    lv_get_system_info(info, sizeof(info));
    printf("系统信息:\n%s\n", info);

    /* 清理 */
    lv_engine_destroy(ctx);

    return EXIT_SUCCESS;
}
```

### 2.2 编译运行

```bash
# 使用 pkg-config
gcc -o hello_lv hello_lv.c $(pkg-config --cflags --libs lv)

# 或手动指定
gcc -o hello_lv hello_lv.c -I/usr/local/include -L/usr/local/lib -llv -lgmp

# 运行
./hello_lv
```

**预期输出**:
```
Lv-00 版本: 1.1.0
上下文创建成功!
系统信息:
  版本: 1.1.0
  平台: Linux
  编译器: GCC 11.4.0
  ...
```

---

## 3. 基础几何构造

### 3.1 创建点

```c
#include "lv/lv.h"
#include <stdio.h>

int main(void) {
    lv_engine_create();
    lvEngine *ctx = lv_engine_create();

    /* 创建三个点 */
    int A = lv_add_point(ctx, 0, 1, 0, 1);    /* (0, 0) */
    int B = lv_add_point(ctx, 3, 1, 0, 1);    /* (3, 0) */
    int C = lv_add_point(ctx, 0, 1, 4, 1);    /* (0, 4) */

    printf("创建了点: A=%d, B=%d, C=%d\n", A, B, C);

    /* 使用整数坐标便捷函数 */
    int D = lv_add_point_i(ctx, 5, 5);        /* (5, 5) */
    printf("创建了整数点 D=%d\n", D);

    lv_engine_destroy(ctx);
    return 0;
}
```

### 3.2 创建线段和约束

```c
#include "lv/lv.h"
#include <stdio.h>

int main(void) {
    lv_engine_create();
    lvEngine *ctx = lv_engine_create();

    /* 创建三角形的三个顶点 */
    int A = lv_add_point_i(ctx, 0, 0);
    int B = lv_add_point_i(ctx, 3, 0);
    int C = lv_add_point_i(ctx, 0, 4);

    /* 创建三条边 */
    int AB = lv_add_line_segment(ctx, A, B);
    int BC = lv_add_line_segment(ctx, B, C);
    int CA = lv_add_line_segment(ctx, C, A);

    printf("三角形边: AB=%d, BC=%d, CA=%d\n", AB, BC, CA);

    /* 添加关联约束（点在边上） */
    lv_add_constraint_incidence(ctx, A, AB);
    lv_add_constraint_incidence(ctx, A, CA);
    lv_add_constraint_incidence(ctx, B, AB);
    lv_add_constraint_incidence(ctx, B, BC);
    lv_add_constraint_incidence(ctx, C, BC);
    lv_add_constraint_incidence(ctx, C, CA);

    printf("约束添加完成\n");

    lv_engine_destroy(ctx);
    return 0;
}
```

### 3.3 符号坐标运算

```c
#include "lv/lv.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    lv_engine_create();

    /* 创建有理数坐标 */
    SymbolicCoord *x = symbolic_coord_create_rational(3, 4);   /* 3/4 */
    SymbolicCoord *y = symbolic_coord_create_rational(5, 2);   /* 5/2 */

    /* 加法 */
    SymbolicCoord *sum = symbolic_coord_add(x, y);
    char *s = symbolic_coord_serialize(sum);
    printf("3/4 + 5/2 = %s\n", s);   /* 输出: 13/4 */
    free(s);

    /* 创建 sqrt(2) */
    Rational *a = rational_create(1, 1);
    Rational *b = rational_create(1, 1);
    SymbolicCoord *sqrt2 = symbolic_coord_create_quadratic(a, b, 2);

    /* 乘法 */
    SymbolicCoord *product = symbolic_coord_multiply(sqrt2, sqrt2);
    s = symbolic_coord_serialize(product);
    printf("sqrt(2) * sqrt(2) = %s\n", s);   /* 输出: 2 */
    free(s);

    /* 清理 */
    symbolic_coord_destroy(x);
    symbolic_coord_destroy(y);
    symbolic_coord_destroy(sum);
    symbolic_coord_destroy(sqrt2);
    symbolic_coord_destroy(product);
    rational_destroy(a);
    rational_destroy(b);

    lv_engine_destroy(ctx);
    return 0;
}
```

---

## 4. 约束与求解

### 4.1 基本求解流程

```c
#include "lv/lv.h"
#include <stdio.h>

int main(void) {
    lv_engine_create();
    lvEngine *ctx = lv_engine_create();

    /* 创建直角三角形 (3-4-5) */
    int A = lv_add_point_i(ctx, 0, 0);
    int B = lv_add_point_i(ctx, 3, 0);
    int C = lv_add_point_i(ctx, 0, 4);

    lv_add_line_segment(ctx, A, B);
    lv_add_line_segment(ctx, B, C);
    lv_add_line_segment(ctx, C, A);

    /* 步骤 1: 归一化 */
    printf("执行归一化...\n");
    NormalizationResult *nr = lv_normalize(ctx, true);
    if (nr) {
        printf("归一化完成，迭代次数: %d\n", nr->iterations);
        normalization_result_destroy(nr);
    }

    /* 步骤 2: 求解 */
    printf("执行求解...\n");
    EngineSolveResult result = lv_solve(ctx);

    /* 处理结果 */
    switch (result) {
        case ENGINE_SOLVE_OK:
            printf("求解成功!\n");
            break;
        case ENGINE_SOLVE_TIMEOUT:
            printf("求解超时\n");
            break;
        case ENGINE_SOLVE_CONFLICT:
            printf("约束矛盾\n");
            break;
        default:
            printf("求解失败，错误码: %d\n", result);
    }

    lv_engine_destroy(ctx);
    return 0;
}
```

### 4.2 带选项的求解

```c
#include "lv/lv.h"
#include <stdio.h>

int main(void) {
    lv_engine_create();

    /* 设置配置 */
    lv_config_set_int("rewrite.step_limit", 5000);
    lv_config_set_int("solver.timeout_ms", 60000);
    lv_set_log_level(3);  /* 启用 INFO 日志 */

    lvEngine *ctx = lv_engine_create();

    /* 构建复杂问题... */

    /* 求解（引擎内部按配置执行重写-求解协作流水线） */
    EngineSolveResult result = lv_solve(ctx);
    printf("求解结果: %d\n", result);

    lv_engine_destroy(ctx);
    return 0;
}
```

---

## 5. 证明系统

### 5.1 创建和证明命题

```c
#include "lv/lv.h"
#include "lv/lv_convenience.h"
#include <stdio.h>

int main(void) {
    lvContext *ctx = lv_context_create();

    /* 解析目标：等边三角形的三条边相等 */
    /* 目标使用 DSL 文本描述，由 lv_prove 解析并调用引擎证明 */

    /* 执行证明 */
    printf("开始证明...\n");
    int rc = lv_prove(ctx, "triangle ABC is equilateral");

    if (rc == 0) {
        printf("证明成功!\n");
    } else {
        printf("证明失败\n");
    }

    lv_context_destroy(ctx);
    return 0;
}
```

### 5.2 使用特定证明策略

Lv-00 的引擎会在重写-求解协作流水线中自动选择合适的推理路径。
需要针对反证法、Groebner 基等具体策略时，可通过预设（preset）与
DSL 目标描述组合实现：

```c
#include "lv/lv.h"
#include "lv/lv_convenience.h"
#include <stdio.h>

int main(void) {
    lvContext *ctx = lv_context_create();

    /* 构建问题（DSL 目标文本）... */

    /* 反证法风格目标 */
    int rc = lv_prove(ctx, "by contradiction: triangle ABC is isosceles");

    if (rc == 0) {
        printf("证明成功!\n");
    } else {
        printf("证明失败\n");
    }

    lv_context_destroy(ctx);
    return 0;
}
```

---

## 6. 预设模块

### 6.1 使用欧氏几何预设

```c
#include "lv/lv.h"
#include "lv/lv_convenience.h"
#include <stdio.h>

int main(void) {
    lvContext *ctx = lv_context_create();

    /* 加载欧氏几何预设 */
    if (lv_preset_load(ctx, "euclidean_geometry") != 0) {
        fprintf(stderr, "加载预设失败\n");
        lv_context_destroy(ctx);
        return 1;
    }
    printf("欧氏几何预设加载成功\n");

    /* 应用勾股定理预设 */
    lv_preset_apply(ctx, "pythagorean_theorem");

    /* 证明 */
    int rc = lv_prove(ctx, "triangle ABC is right-angled at A");

    if (rc == 0) {
        printf("勾股定理验证成功!\n");
    }

    lv_preset_unload(ctx, "euclidean_geometry");
    lv_context_destroy(ctx);
    return 0;
}
```

### 6.2 预设的加载与应用流程

预设库由内部管理器维护；对外可通过 `lv_preset_load()`、
`lv_preset_apply()` 与 `lv_preset_unload()` 操作预设：

```c
#include "lv/lv.h"
#include "lv/lv_convenience.h"
#include <stdio.h>

int main(void) {
    lvContext *ctx = lv_context_create();

    /* 加载预设 */
    if (lv_preset_load(ctx, "midpoint") != 0) {
        fprintf(stderr, "预设加载失败\n");
        lv_context_destroy(ctx);
        return 1;
    }

    /* 应用预设 */
    if (lv_preset_apply(ctx, "midpoint") != 0) {
        fprintf(stderr, "预设应用失败\n");
    } else {
        printf("预设应用成功\n");
    }

    /* 卸载预设 */
    lv_preset_unload(ctx, "midpoint");

    lv_context_destroy(ctx);
    return 0;
}
```

---

## 7. 进阶主题

### 7.1 批量处理问题

```c
#include "lv/lv.h"
#include <stdio.h>

/* 定义问题结构 */
typedef struct {
    const char *name;
    int points[3][2];  /* 三个整数坐标点 */
} Problem;

int main(void) {
    lv_engine_create();

    /* 设置内存上限 */
    lv_set_memory_limit(256 * 1024 * 1024);  /* 256 MB */

    Problem problems[] = {
        {"直角三角形",     {{0,0}, {3,0}, {0,4}}},
        {"等边三角形",     {{0,0}, {2,0}, {1,2}}},
        {"等腰三角形",     {{0,0}, {4,0}, {2,3}}},
    };
    int n = sizeof(problems) / sizeof(problems[0]);

    int success = 0;
    for (int i = 0; i < n; i++) {
        printf("[%d/%d] 处理 %s... ", i+1, n, problems[i].name);

        lvEngine *ctx = lv_engine_create();

        /* 构建问题 */
        int p[3];
        for (int j = 0; j < 3; j++) {
            p[j] = lv_add_point_i(ctx,
                problems[i].points[j][0],
                problems[i].points[j][1]
            );
        }

        lv_add_line_segment(ctx, p[0], p[1]);
        lv_add_line_segment(ctx, p[1], p[2]);
        lv_add_line_segment(ctx, p[2], p[0]);

        /* 求解 */
        lv_normalize(ctx, true);
        if (lv_solve(ctx) == ENGINE_SOLVE_OK) {
            printf("成功\n");
            success++;
        } else {
            printf("失败\n");
        }

        lv_engine_destroy(ctx);
    }

    printf("\n总计: %d/%d 成功\n", success, n);

    /* 查看内存统计 */
    MemoryStats stats;
    lv_get_memory_stats(&stats);
    printf("内存峰值: %zu 字节\n", stats.peak_used);

    return 0;
}
```

### 7.2 多线程使用

```c
#include "lv/lv.h"
#include <pthread.h>
#include <stdio.h>

#define NUM_THREADS 4

typedef struct {
    int thread_id;
    int result;
} ThreadData;

void *worker(void *arg) {
    ThreadData *data = (ThreadData *)arg;

    /* 每个线程创建独立引擎 */
    lvEngine *ctx = lv_engine_create();
    if (!ctx) {
        data->result = -1;
        return NULL;
    }

    /* 构建简单问题 */
    int A = lv_add_point_i(ctx, 0, 0);
    int B = lv_add_point_i(ctx, 3, 0);
    int C = lv_add_point_i(ctx, 0, 4);

    lv_add_line_segment(ctx, A, B);
    lv_add_line_segment(ctx, B, C);
    lv_add_line_segment(ctx, C, A);

    lv_normalize(ctx, true);
    data->result = lv_solve(ctx);

    lv_engine_destroy(ctx);
    return NULL;
}

int main(void) {
    /* 主线程初始化 */
    if (!lv_engine_create()) {
        fprintf(stderr, "初始化失败\n");
        return 1;
    }

    pthread_t threads[NUM_THREADS];
    ThreadData tdata[NUM_THREADS];

    /* 启动工作线程 */
    for (int i = 0; i < NUM_THREADS; i++) {
        tdata[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker, &tdata[i]);
    }

    /* 等待完成 */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        printf("线程 %d: %s\n", i,
               tdata[i].result == ENGINE_SOLVE_OK ? "成功" : "失败");
    }

    return 0;
}
```

---

## 8. 调试与优化

### 8.1 启用调试日志

```c
#include "lv/lv.h"
#include <stdio.h>

int main(void) {
    lv_engine_create();

    /* 设置日志级别 */
    lv_set_log_level(4);  /* DEBUG 级别 */

    lvEngine *ctx = lv_engine_create();

    /* 你的代码... */

    /* 查看健康状态 */
    int health = lv_health_check();
    printf("系统健康评分: %d/100\n", health);

    lv_engine_destroy(ctx);
    return 0;
}
```

### 8.2 内存调试

```c
#include "lv/lv.h"
#include <stdio.h>

int main(void) {
    lv_engine_create();

    /* 设置内存限制 */
    lv_set_memory_limit(100 * 1024 * 1024);  /* 100 MB */

    /* 获取初始内存状态 */
    MemoryStats stats_before;
    lv_get_memory_stats(&stats_before);

    /* 执行操作... */
    lvEngine *ctx = lv_engine_create();
    /* ... */

    /* 获取最终内存状态 */
    MemoryStats stats_after;
    lv_get_memory_stats(&stats_after);

    printf("内存使用: %zu 字节\n",
           stats_after.current_used - stats_before.current_used);
    printf("内存峰值: %zu 字节\n", stats_after.peak_used);

    lv_engine_destroy(ctx);
    return 0;
}
```

### 8.3 错误处理模式

```c
#include "lv/lv.h"
#include <stdio.h>
#include <stdlib.h>

/* 推荐的分层错误处理 */
int solve_problem_safely(void) {
    /* 第 1 层: 系统初始化 */
    if (!lv_engine_create()) {
        fprintf(stderr, "FATAL: lv_engine_create() 失败\n");
        return -1;
    }

    /* 第 2 层: 资源分配 */
    lvEngine *ctx = lv_engine_create();
    if (!ctx) {
        fprintf(stderr, "ERROR: 上下文创建失败: %s\n",
                lv_get_last_error_message());
        lv_engine_destroy(ctx);
        return -1;
    }

    /* 第 3 层: 问题构建 */
    int A = lv_add_point_i(ctx, 0, 0);
    if (A < 0) {
        fprintf(stderr, "ERROR: 点 A 创建失败\n");
        goto cleanup;
    }

    int B = lv_add_point_i(ctx, 3, 0);
    if (B < 0) {
        fprintf(stderr, "ERROR: 点 B 创建失败\n");
        goto cleanup;
    }

    /* 第 4 层: 求解 */
    lv_normalize(ctx, true);
    EngineSolveResult result = lv_solve(ctx);

    if (result != ENGINE_SOLVE_OK) {
        fprintf(stderr, "WARNING: 求解未完全成功: %s\n",
                lv_get_last_error_message());
        /* 即使失败也可能有部分结果 */
    }

    lv_engine_destroy(ctx);
    return (result == ENGINE_SOLVE_OK) ? 0 : 1;

cleanup:
    lv_engine_destroy(ctx);
    return -1;
}

int main(void) {
    int ret = solve_problem_safely();
    return (ret == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

---

## 下一步

- 阅读 [API 完整参考](API_REFERENCE.md) 了解所有 API
- 阅读 [架构手册](archived/ARCHITECTURE_MANUAL.md) 深入理解系统设计
- 查看 [应用场景](USE_CASES.md) 了解更多使用示例
- 参与项目贡献，查看 [贡献指南](../../CONTRIBUTING.md)
