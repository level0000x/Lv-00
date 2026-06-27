# Lv-00 入门教程

> **版本**: 1.1.0  
> **最后更新**: 2026-06-27  
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
cmake --install . --prefix "C:/Program Files/lv00"
```

---

## 2. 第一个程序

### 2.1 Hello Lv-00

创建 `hello_lv00.c`:

```c
#include "lv00/lv00.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* 初始化系统 */
    if (!lv00_context_create()) {
        fprintf(stderr, "错误: Lv-00 初始化失败\n");
        return EXIT_FAILURE;
    }

    /* 打印版本信息 */
    printf("Lv-00 版本: %s\n", lv00_get_version_string());

    /* 创建引擎 */
    LV00Context *ctx = lv00_context_create();
    if (!ctx) {
        fprintf(stderr, "错误: 上下文创建失败\n");
        lv00_context_destroy(ctx);
        return EXIT_FAILURE;
    }

    printf("上下文创建成功!\n");

    /* 获取系统信息 */
    char info[1024];
    lv00_get_system_info(info, sizeof(info));
    printf("系统信息:\n%s\n", info);

    /* 清理 */
    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);

    return EXIT_SUCCESS;
}
```

### 2.2 编译运行

```bash
# 使用 pkg-config
gcc -o hello_lv00 hello_lv00.c $(pkg-config --cflags --libs lv00)

# 或手动指定
gcc -o hello_lv00 hello_lv00.c -I/usr/local/include -L/usr/local/lib -llv00 -lgmp

# 运行
./hello_lv00
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
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_context_create();
    LV00Context *ctx = lv00_context_create();

    /* 创建三个点 */
    int A = lv00_add_point(ctx, 0, 1, 0, 1);    /* (0, 0) */
    int B = lv00_add_point(ctx, 3, 1, 0, 1);    /* (3, 0) */
    int C = lv00_add_point(ctx, 0, 1, 4, 1);    /* (0, 4) */

    printf("创建了点: A=%d, B=%d, C=%d\n", A, B, C);

    /* 使用整数坐标便捷函数 */
    int D = lv00_add_point_i(ctx, 5, 5);        /* (5, 5) */
    printf("创建了整数点 D=%d\n", D);

    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return 0;
}
```

### 3.2 创建线段和约束

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_context_create();
    LV00Context *ctx = lv00_context_create();

    /* 创建三角形的三个顶点 */
    int A = lv00_add_point_i(ctx, 0, 0);
    int B = lv00_add_point_i(ctx, 3, 0);
    int C = lv00_add_point_i(ctx, 0, 4);

    /* 创建三条边 */
    int AB = lv00_add_line_segment(ctx, A, B);
    int BC = lv00_add_line_segment(ctx, B, C);
    int CA = lv00_add_line_segment(ctx, C, A);

    printf("三角形边: AB=%d, BC=%d, CA=%d\n", AB, BC, CA);

    /* 添加关联约束（点在边上） */
    lv00_add_constraint_incidence(ctx, A, AB);
    lv00_add_constraint_incidence(ctx, A, CA);
    lv00_add_constraint_incidence(ctx, B, AB);
    lv00_add_constraint_incidence(ctx, B, BC);
    lv00_add_constraint_incidence(ctx, C, BC);
    lv00_add_constraint_incidence(ctx, C, CA);

    printf("约束添加完成\n");

    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return 0;
}
```

### 3.3 符号坐标运算

```c
#include "lv00/lv00.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    lv00_context_create();

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

    lv00_context_destroy(ctx);
    return 0;
}
```

---

## 4. 约束与求解

### 4.1 基本求解流程

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_context_create();
    LV00Context *ctx = lv00_context_create();

    /* 创建直角三角形 (3-4-5) */
    int A = lv00_add_point_i(ctx, 0, 0);
    int B = lv00_add_point_i(ctx, 3, 0);
    int C = lv00_add_point_i(ctx, 0, 4);

    lv00_add_line_segment(ctx, A, B);
    lv00_add_line_segment(ctx, B, C);
    lv00_add_line_segment(ctx, C, A);

    /* 步骤 1: 归一化 */
    printf("执行归一化...\n");
    NormalizationResult *nr = lv00_normalize(ctx, true);
    if (nr) {
        printf("归一化完成，迭代次数: %d\n", nr->iterations);
        normalization_result_destroy(nr);
    }

    /* 步骤 2: 求解 */
    printf("执行求解...\n");
    EngineSolveResult result = lv00_solve(ctx);

    /* 处理结果 */
    switch (result) {
        case LV00_SOLVE_SUCCESS:
            printf("求解成功!\n");
            break;
        case LV00_SOLVE_TIMEOUT:
            printf("求解超时\n");
            break;
        case LV00_SOLVE_INCONSISTENT:
            printf("约束矛盾\n");
            break;
        default:
            printf("求解失败，错误码: %d\n", result);
    }

    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return 0;
}
```

### 4.2 带选项的求解

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_context_create();

    /* 设置配置 */
    lv00_config_set_int("rewrite.step_limit", 5000);
    lv00_config_set_int("solver.timeout_ms", 60000);
    lv00_set_log_level(3);  /* 启用 INFO 日志 */

    LV00Context *ctx = lv00_context_create();

    /* 构建复杂问题... */

    /* 使用自定义选项求解 */
    SolverOptions options = {
        .timeout_ms = 30000,
        .max_iterations = 1000,
        .enable_groebner = true,
        .enable_smt = true,
        .enable_atp = false,
        .log_level = 2
    };

    EngineSolveResult result = lv00_solve_with_options(ctx, &options);
    printf("求解结果: %d\n", result);

    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return 0;
}
```

---

## 5. 证明系统

### 5.1 创建和证明命题

```c
#include "lv00/lv00.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    lv00_context_create();
    LV00Context *ctx = lv00_context_create();

    /* 创建等边三角形 */
    int A = lv00_add_point_i(ctx, 0, 0);
    int B = lv00_add_point_i(ctx, 2, 0);

    /* 构造等边三角形第三点 */
    /* 使用圆交点构造 */
    /* ... 构造代码 ... */

    /* 创建命题：三边相等 */
    /* 假设我们已经计算出边长 */
    Expr *ab = /* 边 AB 长度 */;
    Expr *bc = /* 边 BC 长度 */;
    Expr *ca = /* 边 CA 长度 */;

    Proposition *eq1 = lv00_proposition_eq(ab, bc);
    Proposition *eq2 = lv00_proposition_eq(bc, ca);
    Proposition *goal = lv00_proposition_and(eq1, eq2);

    /* 执行证明 */
    printf("开始证明...\n");
    Proof *proof = lv00_prove(ctx, goal);

    if (proof && lv00_proof_valid(proof)) {
        printf("证明成功!\n");
        printf("证明步骤数: %zu\n", lv00_proof_get_step_count(proof));

        /* 导出证明 */
        lv00_proof_export_lean(proof, "equilateral.lean");
        lv00_proof_export_latex(proof, "equilateral.tex");
        printf("证明已导出到 equilateral.lean 和 equilateral.tex\n");
    } else {
        printf("证明失败\n");
    }

    /* 清理 */
    /* ... 销毁命题和证明 ... */

    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return 0;
}
```

### 5.2 使用特定证明策略

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_context_create();
    LV00Context *ctx = lv00_context_create();

    /* 构建问题... */

    /* 创建命题 */
    Proposition *goal = /* ... */;

    /* 使用反证法 */
    Proof *proof = lv00_prove_with_strategy(
        ctx, goal, PROOF_STRATEGY_CONTRADICTION
    );

    if (proof && lv00_proof_valid(proof)) {
        printf("反证成功!\n");
    }

    /* 使用 Groebner 基 */
    proof = lv00_prove_with_strategy(
        ctx, goal, PROOF_STRATEGY_GROEBNER
    );

    if (proof && lv00_proof_valid(proof)) {
        printf("代数证明成功!\n");
    }

    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return 0;
}
```

---

## 6. 预设模块

### 6.1 使用欧氏几何预设

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_context_create();
    LV00Context *ctx = lv00_context_create();

    /* 加载欧氏几何预设 */
    if (!lv00_preset_load(ctx, "euclidean_geometry")) {
        fprintf(stderr, "加载预设失败\n");
        lv00_context_destroy(ctx);
        lv00_context_destroy(ctx);
        return 1;
    }
    printf("欧氏几何预设加载成功\n");

    /* 创建 3-4-5 直角三角形 */
    int A = lv00_add_point_i(ctx, 0, 0);
    int B = lv00_add_point_i(ctx, 3, 0);
    int C = lv00_add_point_i(ctx, 0, 4);

    /* 应用勾股定理预设 */
    Proposition *prop = lv00_preset_apply(
        ctx, "pythagorean_theorem", A, B, C
    );

    /* 证明 */
    Proof *proof = lv00_prove(ctx, prop);

    if (proof && lv00_proof_valid(proof)) {
        printf("勾股定理验证成功!\n");
        lv00_proof_export_tikz(proof, "pythagorean.tex");
    }

    lv00_preset_unload(ctx, "euclidean_geometry");
    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return 0;
}
```

### 6.2 列出可用预设

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_context_create();

    size_t count;
    char **presets = lv00_preset_list(&count);

    printf("可用预设模块 (%zu 个):\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  - %s\n", presets[i]);

        /* 获取预设信息 */
        PresetInfo *info = lv00_preset_get_info(presets[i]);
        if (info) {
            printf("    描述: %s\n", info->description);
            printf("    版本: %s\n", info->version);
            free(info);
        }

        free(presets[i]);
    }
    free(presets);

    lv00_context_destroy(ctx);
    return 0;
}
```

---

## 7. 进阶主题

### 7.1 批量处理问题

```c
#include "lv00/lv00.h"
#include <stdio.h>

/* 定义问题结构 */
typedef struct {
    const char *name;
    int points[3][2];  /* 三个整数坐标点 */
} Problem;

int main(void) {
    lv00_context_create();

    /* 设置内存上限 */
    lv00_set_memory_limit_ex(256 * 1024 * 1024);  /* 256 MB */

    Problem problems[] = {
        {"直角三角形",     {{0,0}, {3,0}, {0,4}}},
        {"等边三角形",     {{0,0}, {2,0}, {1,2}}},
        {"等腰三角形",     {{0,0}, {4,0}, {2,3}}},
    };
    int n = sizeof(problems) / sizeof(problems[0]);

    int success = 0;
    for (int i = 0; i < n; i++) {
        printf("[%d/%d] 处理 %s... ", i+1, n, problems[i].name);

        LV00Context *ctx = lv00_context_create();

        /* 构建问题 */
        int p[3];
        for (int j = 0; j < 3; j++) {
            p[j] = lv00_add_point_i(ctx,
                problems[i].points[j][0],
                problems[i].points[j][1]
            );
        }

        lv00_add_line_segment(ctx, p[0], p[1]);
        lv00_add_line_segment(ctx, p[1], p[2]);
        lv00_add_line_segment(ctx, p[2], p[0]);

        /* 求解 */
        lv00_normalize(ctx, true);
        if (lv00_solve(ctx) == LV00_SOLVE_SUCCESS) {
            printf("成功\n");
            success++;
        } else {
            printf("失败\n");
        }

        lv00_context_destroy(ctx);
    }

    printf("\n总计: %d/%d 成功\n", success, n);

    /* 查看内存统计 */
    Lv00MemoryStats stats;
    if (lv00_get_memory_stats_ex(&stats)) {
        printf("内存峰值: %zu 字节\n", stats.peak_bytes);
    }

    lv00_context_destroy(ctx);
    return 0;
}
```

### 7.2 多线程使用

```c
#include "lv00/lv00.h"
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
    LV00Context *ctx = lv00_context_create();
    if (!ctx) {
        data->result = -1;
        return NULL;
    }

    /* 构建简单问题 */
    int A = lv00_add_point_i(ctx, 0, 0);
    int B = lv00_add_point_i(ctx, 3, 0);
    int C = lv00_add_point_i(ctx, 0, 4);

    lv00_add_line_segment(ctx, A, B);
    lv00_add_line_segment(ctx, B, C);
    lv00_add_line_segment(ctx, C, A);

    lv00_normalize(ctx, true);
    data->result = lv00_solve(ctx);

    lv00_context_destroy(ctx);
    return NULL;
}

int main(void) {
    /* 主线程初始化 */
    if (!lv00_context_create()) {
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
               tdata[i].result == LV00_SOLVE_SUCCESS ? "成功" : "失败");
    }

    /* 主线程清理 */
    lv00_context_destroy(ctx);
    return 0;
}
```

---

## 8. 调试与优化

### 8.1 启用调试日志

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_context_create();

    /* 设置日志级别 */
    lv00_set_log_level(4);  /* DEBUG 级别 */

    LV00Context *ctx = lv00_context_create();

    /* 你的代码... */

    /* 查看健康状态 */
    int health = lv00_health_check();
    printf("系统健康评分: %d/100\n", health);

    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return 0;
}
```

### 8.2 内存调试

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_context_create();

    /* 设置内存限制 */
    lv00_set_memory_limit_ex(100 * 1024 * 1024);  /* 100 MB */

    /* 获取初始内存状态 */
    Lv00MemoryStats stats_before;
    lv00_get_memory_stats_ex(&stats_before);

    /* 执行操作... */
    LV00Context *ctx = lv00_context_create();
    /* ... */

    /* 获取最终内存状态 */
    Lv00MemoryStats stats_after;
    lv00_get_memory_stats_ex(&stats_after);

    printf("内存使用: %zu 字节\n",
           stats_after.current_bytes - stats_before.current_bytes);
    printf("内存峰值: %zu 字节\n", stats_after.peak_bytes);

    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return 0;
}
```

### 8.3 错误处理模式

```c
#include "lv00/lv00.h"
#include <stdio.h>
#include <stdlib.h>

/* 推荐的分层错误处理 */
int solve_problem_safely(void) {
    /* 第 1 层: 系统初始化 */
    if (!lv00_context_create()) {
        fprintf(stderr, "FATAL: lv00_context_create() 失败\n");
        return -1;
    }

    /* 第 2 层: 资源分配 */
    LV00Context *ctx = lv00_context_create();
    if (!ctx) {
        fprintf(stderr, "ERROR: 上下文创建失败: %s\n",
                lv00_get_last_error_string());
        lv00_context_destroy(ctx);
        return -1;
    }

    /* 第 3 层: 问题构建 */
    int A = lv00_add_point_i(ctx, 0, 0);
    if (A < 0) {
        fprintf(stderr, "ERROR: 点 A 创建失败\n");
        goto cleanup;
    }

    int B = lv00_add_point_i(ctx, 3, 0);
    if (B < 0) {
        fprintf(stderr, "ERROR: 点 B 创建失败\n");
        goto cleanup;
    }

    /* 第 4 层: 求解 */
    lv00_normalize(ctx, true);
    EngineSolveResult result = lv00_solve(ctx);

    if (result != LV00_SOLVE_SUCCESS) {
        fprintf(stderr, "WARNING: 求解未完全成功: %s\n",
                lv00_get_last_error_string());
        /* 即使失败也可能有部分结果 */
    }

    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
    return (result == LV00_SOLVE_SUCCESS) ? 0 : 1;

cleanup:
    lv00_context_destroy(ctx);
    lv00_context_destroy(ctx);
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
- 阅读 [架构手册](ARCHITECTURE_MANUAL.md) 深入理解系统设计
- 查看 [应用场景](USE_CASES.md) 了解更多使用示例
- 参与项目贡献，查看 [贡献指南](../../CONTRIBUTING.md)
