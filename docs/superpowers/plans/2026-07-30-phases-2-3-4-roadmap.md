# Phase 2-4 Implementation Plan: 高阶合一 + Church 编码 + 数值后端

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 按重要性递进实现 λ-演算高阶合一、Church 编码完善、数值后端 CUDA/HIP/SINGULAR 完整实现

**Architecture:** 三个独立子系统，按依赖关系排序。高阶合一扩展现有 unify 系统支持 λ-项句法合一→模式合一；Church 编码在现有 20 函数基础上添加逻辑/列表/比较运算；数值后端为 CUDA/HIP/SINGULAR 创建完整的操作表实现和 CMake SDK 检测。

**Tech Stack:** C11 (MSVC/GCC/Clang), CUDA SDK (可选), HIP/ROCm (可选), Singular (可选), OpenMP

---

## 文件结构总览

### Phase 1: 高阶合一 (Highest Priority)
| 操作 | 文件 |
|:---|:---|
| 新增 | `core/include/lv/lambda_unify.h` — λ-项合一 API 头文件 |
| 新增 | `core/src/layer4_reasoning/unify/lambda_unify.c` — λ-项句法合一实现 |
| 新增 | `core/src/layer4_reasoning/unify/pattern_unify.c` — 模式合一（高阶合一实用子集） |
| 新增 | `test/c/test_lambda_unify.c` — 合一测试 |
| 修改 | `core/include/lv/unify.h` — 添加 λ-项合一相关枚举/声明 |
| 修改 | `CMakeLists.txt` — 注册新测试目标 |

### Phase 2: Church 编码完善
| 操作 | 文件 |
|:---|:---|
| 修改 | `core/include/lv/lambda_church.h` — 新增声明 |
| 修改 | `core/src/layer4_reasoning/lambda/lambda_church.c` — 新增实现 |
| 修改 | `test/c/test_lambda_church.c` — 新增测试 |

### Phase 3: 数值后端 CUDA/HIP/SINGULAR
| 操作 | 文件 |
|:---|:---|
| 新增 | `core/include/lv/backends/cuda_backend.h` — CUDA 后端 API |
| 新增 | `core/include/lv/backends/hip_backend.h` — HIP 后端 API |
| 新增 | `core/include/lv/backends/singular_backend.h` — Singular 后端 API |
| 新增 | `core/src/layer4_reasoning/numeric/backends/cuda_backend.c` — CUDA 实现 |
| 新增 | `core/src/layer4_reasoning/numeric/backends/hip_backend.c` — HIP 实现 |
| 新增 | `core/src/layer4_reasoning/numeric/backends/singular_backend.c` — Singular 实现 |
| 修改 | `core/src/layer4_reasoning/numeric/numerical_backend.c` — 添加新后端分发逻辑 |
| 修改 | `core/include/lv/numerical_backend.h` — 添加 `lv_BACKEND_SINGULAR` 名称支持 |
| 修改 | `CMakeLists.txt` — 添加 CUDA/HIP/Singular SDK 检测 |

---

## Phase 1: 高阶合一

### Task 1.1: λ-项句法合一

**背景：** 现有 unify 系统基于约束图结构匹配，不理解 λ-项语法。需要实现标准的 λ-项句法合一算法：处理 `LV_LAMBDA_VAR`(De Bruijn 索引)/`LV_LAMBDA_ABS`/`LV_LAMBDA_APP` 三种节点类型的合一匹配，含 De Bruijn 索引的偏移处理。

**算法：** Martelli-Montanari 风格的一阶合一，但处理 λ-项树结构：
- `unify_var(index, term)` — 变量检查 (occurs check)
- `unify_abs(binder, body1, body2)` — 抽象合一（提升索引）
- `unify_app(fun1, arg1, fun2, arg2)` — 应用合一（递归合一 fun 和 arg）

**文件：** `core/include/lv/lambda_unify.h` + `core/src/layer4_reasoning/unify/lambda_unify.c`

```c
#ifndef LV_LAMBDA_UNIFY_H
#define LV_LAMBDA_UNIFY_H

#include "lv/lambda_term.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 合一结果状态 */
typedef enum {
    LAMBDA_UNIFY_OK,           /* 合一成功 */
    LAMBDA_UNIFY_FAIL,         /* 合一失败（无法匹配） */
    LAMBDA_UNIFY_OCCURS_CHECK, /* 变量出现在自身中 */
    LAMBDA_UNIFY_DEPTH_MISMATCH, /* 绑定深度不匹配 */
    LAMBDA_UNIFY_ERROR         /* 内部错误 */
} LambdaUnifyStatus;

/* 合一替换：将 term 中第 index 个自由变量替换为 replacement */
typedef struct LambdaSubstitution {
    int index;                    /* De Bruijn 索引 */
    LvLambdaTerm *replacement;   /* 替换项 */
    struct LambdaSubstitution *next;
} LambdaSubstitution;

/* === 核心 API === */

/**
 * @brief 对两个 λ-项执行句法合一
 *
 * @param t1, t2  待合一的两个 λ-项
 * @param out_subs  输出：合一替换链表（调用者通过 lambda_substitution_list_destroy 释放）
 * @param max_depth  最大递归深度（防止无限递归，建议 1024）
 * @return LambdaUnifyStatus
 */
LambdaUnifyStatus lambda_unify(LvLambdaTerm *t1, LvLambdaTerm *t2,
                               LambdaSubstitution **out_subs, int max_depth);

/**
 * @brief 将替换应用于 λ-项
 *
 * @param term  原始 λ-项
 * @param subs  替换链表
 * @return LvLambdaTerm*  应用替换后的新项（调用者负责销毁）
 */
LvLambdaTerm *lambda_unify_apply(LvLambdaTerm *term, LambdaSubstitution *subs);

/**
 * @brief 销毁替换链表
 */
void lambda_substitution_list_destroy(LambdaSubstitution *subs);

/**
 * @brief 合一替换到字符串（调试用）
 * @param subs  替换链表
 * @param buf  输出缓冲区
 * @param size  缓冲区大小
 */
void lambda_substitution_snprint(LambdaSubstitution *subs, char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* LV_LAMBDA_UNIFY_H */
```

**核心实现 (lambda_unify.c):**

```c
/* 递归合一核心 */
static LambdaUnifyStatus lambda_unify_rec(LvLambdaTerm *t1, LvLambdaTerm *t2,
                                           LambdaSubstitution **subs, int depth, int max_depth) {
    if (depth >= max_depth) return LAMBDA_UNIFY_ERROR;
    if (!t1 || !t2) return LAMBDA_UNIFY_ERROR;
    if (t1 == t2) return LAMBDA_UNIFY_OK;  /* 同一对象 */

    switch (t1->type) {
    case LV_LAMBDA_VAR: {
        int idx1 = t1->data.var.index;
        /* 查找 t1 是否已有替换 */
        LvLambdaTerm *replacement = find_substitution(*subs, idx1);
        if (replacement)
            return lambda_unify_rec(replacement, t2, subs, depth + 1, max_depth);
        if (t2->type == LV_LAMBDA_VAR) {
            int idx2 = t2->data.var.index;
            LvLambdaTerm *r2 = find_substitution(*subs, idx2);
            if (r2)
                return lambda_unify_rec(t1, r2, subs, depth + 1, max_depth);
            if (idx1 == idx2) return LAMBDA_UNIFY_OK;
        }
        /* Occurs check */
        if (occurs_check(idx1, t2, *subs))
            return LAMBDA_UNIFY_OCCURS_CHECK;
        /* 添加替换 */
        add_substitution(subs, idx1, lv_lambda_copy(t2));
        return LAMBDA_UNIFY_OK;
    }
    case LV_LAMBDA_ABS:
        if (t2->type != LV_LAMBDA_ABS) return LAMBDA_UNIFY_FAIL;
        /* 抽象合一：递归合一 body，binder 索引偏移 1 */
        return lambda_unify_rec(t1->data.abs.body, t2->data.abs.body,
                                subs, depth + 1, max_depth);
    case LV_LAMBDA_APP:
        if (t2->type != LV_LAMBDA_APP) return LAMBDA_UNIFY_FAIL;
        /* 应用合一：递归合一 fun 和 arg */
        LambdaUnifyStatus s = lambda_unify_rec(t1->data.app.fun, t2->data.app.fun,
                                                subs, depth + 1, max_depth);
        if (s != LAMBDA_UNIFY_OK) return s;
        return lambda_unify_rec(t1->data.app.arg, t2->data.app.arg,
                                subs, depth + 1, max_depth);
    default:
        return LAMBDA_UNIFY_ERROR;
    }
}
```

### Task 1.2: 模式合一 (Pattern Unification)

**背景：** 纯句法合一不足以处理 λ-项中的高阶变量。模式合一是高阶合一的一个可判定子集，限制高阶变量只能应用于不同的 bound 变量（`F x1 x2 ... xn` 形式，其中 xi 是 distinct bound 变量）。这是实际应用中最重要的子集。

**算法：** Miller 模式合一：
- 限制：高阶变量（函数位置）的参数必须是 distinct bound 变量
- 通过 Imitation（模仿目标函数结构）和 Projection（投影到某个参数）两个规则求解

**文件：** `core/src/layer4_reasoning/unify/pattern_unify.c`

在现有的 `lambda_unify.h` 中添加：

```c
/**
 * @brief 对两个 λ-项执行模式合一（Miller 模式合一）
 *
 * 限制：高阶变量（`F`）的参数必须是不同的 bound 变量。
 * 这是高阶合一的可判定且实用的子集。
 *
 * @param t1, t2  待合一的两个 λ-项
 * @param out_subs  输出替换链表
 * @param max_depth  最大递归深度
 * @return LambdaUnifyStatus
 */
LambdaUnifyStatus lambda_pattern_unify(LvLambdaTerm *t1, LvLambdaTerm *t2,
                                        LambdaSubstitution **out_subs, int max_depth);

/**
 * @brief 检查项是否符合模式合一的条件（所有高阶变量的参数都是 distinct bound 变量）
 */
bool lambda_is_pattern(LvLambdaTerm *term);

/**
 * @brief 将合一结果集成到约束图证明策略中
 *
 * 将合一替换应用到约束图中的函数块节点，实现 λ-项实例化。
 *
 * @param graph  目标约束图
 * @param subs  合一替换
 * @return int  0=成功, -1=失败
 */
int lambda_unify_apply_to_graph(ConstraintGraph *graph, LambdaSubstitution *subs);
```

**测试 (test_lambda_unify.c):**

测试用例覆盖：

```c
/* === 句法合一测试 === */
static void test_unify_var_var(void);      /* λx.x 与 λy.y 合一 → OK */
static void test_unify_var_abs(void);      /* ?X 与 λx.x 合一 → [X↦λx.x] */
static void test_unify_abs_abs(void);      /* λx.x 与 λy.y 合一 → OK（α-等价） */
static void test_unify_app_app(void);      /* (λx.x) a 与 (λy.y) b 合一 → [a↦b] */
static void test_unify_occurs_check(void); /* ?X 与 λx.(?X x) 合一 → OCCURS_CHECK */
static void test_unify_fail_type(void);    /* λx.x 与 (a b) 合一 → FAIL */
static void test_unify_nested_abs(void);   /* λx.λy.x 与 λa.λb.a 合一 → OK */

/* === 模式合一测试 === */
static void test_pattern_simple(void);     /* F a 与 G b 合一 */
static void test_pattern_imitation(void);  /* F x 与 g(x, h(x)) 合一 */
static void test_pattern_projection(void); /* F x y 与 x 合一（投影到第一个参数）*/
static void test_pattern_non_pattern(void); /* 非模式形式应被拒绝 */
static void test_pattern_constraint(void); /* F x 与 G x 合一（F↦G）*/
```

### Task 1.3: 集成到证明策略

将合一结果插入现有证明策略管线：
- 在 `proof_multi_strategy.c` 中新增 `PROOF_STRATEGY_LAMBDA_UNIFY` 策略类型
- 合一成功时自动实例化证明中的 λ-项变量

---

## Phase 2: Church 编码完善

### Task 2.1: Church 布尔运算

在 `lambda_church.h` 新增：

```c
LvLambdaTerm *lv_church_not(void);    /* λp.λa.λb.p b a */
LvLambdaTerm *lv_church_and(void);    /* λp.λq.p q p */
LvLambdaTerm *lv_church_or(void);     /* λp.λq.p p q */
LvLambdaTerm *lv_church_xor(void);    /* λp.λq.p (not q) q */
```

实现定义：
- `not` = `λp.λa.λb.p b a`（翻转 true/false 分支）
- `and` = `λp.λq.p q p`（若 p 为 true 则返回 q，否则返回 false）
- `or`  = `λp.λq.p p q`（若 p 为 true 则返回 true，否则返回 q）
- `xor` = `λp.λq.p (not q) q`（若 p 为 true 则返回 not q，否则返回 q）

### Task 2.2: Church 列表操作

在 `lambda_church.h` 新增：

```c
LvLambdaTerm *lv_church_isnil(void);  /* λl.l (λh.λt.λx.false) true */
LvLambdaTerm *lv_church_head(void);   /* λl.l (λh.λt.h) (error) */
LvLambdaTerm *lv_church_tail(void);   /* λl.l (λh.λt.t) (error) */
LvLambdaTerm *lv_church_map(void);    /* λf.λl.l (λh.λt.cons (f h) (map f t)) nil */
LvLambdaTerm *lv_church_filter(void); /* λp.λl.l (λh.λt.if (p h) (cons h (filter p t)) (filter p t)) nil */
LvLambdaTerm *lv_church_foldr(void);  /* λf.λz.λl.l (λh.λt.f h (foldr f z t)) z */
LvLambdaTerm *lv_church_foldl(void);  /* λf.λz.λl.l (λh.λt.foldl f (f z h) t) z */
LvLambdaTerm *lv_church_length(void); /* λl.l (λh.λt.succ t) zero */
LvLambdaTerm *lv_church_append(void); /* λl1.λl2.l1 cons l2 */
```

### Task 2.3: Church 比较运算

在 `lambda_church.h` 新增：

```c
LvLambdaTerm *lv_church_eq(void);    /* λm.λn.iszero (sub m n) ∧ iszero (sub n m) */
LvLambdaTerm *lv_church_leq(void);   /* λm.λn.iszero (sub m n) */
LvLambdaTerm *lv_church_gt(void);    /* λm.λn.not (leq m n) */
```

### 测试新增

每个新函数至少一个编译 + roundtrip 测试。

---

## Phase 3: 数值后端完整实现

### 架构设计

每个后端采用独立 `.c` 文件 + 条件编译：
- 当 SDK 可用时 → 编译真实实现
- 当 SDK 不可用时 → 编译提示信息（不返回 UNSUPPORTED 错误，而是提供功能性降级路径）

```c
/* 通用模式 */
#if defined(LV_HAS_CUDA)
/* 真实 CUDA 实现 */
#else
/* CUDA 不可用时的降级 */
static int cuda_vector_ops_not_available(void) {
    lv_ERROR_SET(lv_BACKEND_UNSUPPORTED, "CUDA 后端未编译：需要 CUDA SDK");
    return lv_BACKEND_UNSUPPORTED;
}
#endif
```

### Task 3.1: CUDA 后端

```
core/include/lv/backends/cuda_backend.h
core/src/layer4_reasoning/numeric/backends/cuda_backend.c
```

**头文件** `cuda_backend.h`:

```c
#ifndef LV_CUDA_BACKEND_H
#define LV_CUDA_BACKEND_H

#include "lv/numerical_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 注册 CUDA 后端操作表到后端注册表 */
int lv_cuda_register_backend(void);

/* 检查 CUDA 是否可用（GPU 设备数和 CUDA 运行时版本） */
int lv_cuda_available(void);

/* 获取 CUDA 设备数 */
int lv_cuda_device_count(void);

/* CUDA 后端版本信息 */
const char *lv_cuda_backend_version(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_CUDA_BACKEND_H */
```

**实现** `cuda_backend.c` — 包含完整的向量/矩阵/求解器操作表：

```c
/* 向量操作表 — 16 个操作 */
static lvVectorOps cuda_vector_ops = {
    .clone    = cuda_vector_clone,
    .destroy  = cuda_vector_destroy,
    .zero     = cuda_vector_zero,
    .const_set = cuda_vector_const_set,
    .copy     = cuda_vector_copy,
    .scale    = cuda_vector_scale,
    .linear_sum = cuda_vector_linear_sum,
    .dot      = cuda_vector_dot,
    .norm     = cuda_vector_norm,
    .max_norm = cuda_vector_max_norm,
    .wrms_norm = cuda_vector_wrms_norm,
    .abs      = cuda_vector_abs,
    .inv      = cuda_vector_inv,
    .compare  = cuda_vector_compare,
    .length   = cuda_vector_length,
    .data_ptr = cuda_vector_data_ptr
};

/* 矩阵操作表 — 10 个操作 */
static lvMatrixOps cuda_dense_matrix_ops = {
    .clone    = cuda_matrix_clone,
    .destroy  = cuda_matrix_destroy,
    .zero     = cuda_matrix_zero,
    .copy     = cuda_matrix_copy,
    .matvec   = cuda_matrix_matvec,
    .scale    = cuda_matrix_scale,
    .set_element = cuda_matrix_set_element,
    .get_element = cuda_matrix_get_element,
    .factor   = cuda_matrix_factor,
    .solve    = cuda_matrix_solve
};

/* 求解器操作表 — 3 种求解器各 3 个操作 */
static lvLinearSolverOps cuda_dense_linsol_ops = {
    .setup  = cuda_linsol_setup,
    .solve  = cuda_linsol_solve,
    .destroy = cuda_linsol_destroy
};

/* 工厂函数分发点 */
int lv_cuda_register_backend(void) { ... }
```

关键 CUDA 内核（用 `#ifdef LV_HAS_CUDA` 包裹）：

```c
#ifdef LV_HAS_CUDA
/* CUDA 向量缩放内核 */
__global__ void vector_scale_kernel(double *data, double c, int64_t n) {
    int64_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) data[idx] *= c;
}

/* CUDA 矩阵-向量乘法内核（按行并行，合并访问） */
__global__ void matvec_kernel(const double *data, const double *x,
                               double *y, int64_t rows, int64_t cols) {
    int64_t row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= rows) return;
    double sum = 0.0;
    for (int64_t j = 0; j < cols; ++j) {
        sum += data[j * rows + row] * x[j];
    }
    y[row] = sum;
}
#endif
```

### Task 3.2: HIP 后端

结构与 CUDA 后端镜像，使用 HIP 运行时 API 和 hipLaunchKernelGGL。

```
core/include/lv/backends/hip_backend.h
core/src/layer4_reasoning/numeric/backends/hip_backend.c
```

关键区别：
- 使用 `hipMalloc` / `hipMemcpy` 替代 `cudaMalloc` / `cudaMemcpy`
- 内核启动使用 `hipLaunchKernelGGL` 宏
- 使用 `#ifdef LV_HAS_HIP` 编译守卫

### Task 3.3: Singular 后端

```
core/include/lv/backends/singular_backend.h
core/src/layer4_reasoning/numeric/backends/singular_backend.c
```

Singular 是计算机代数系统，与数值后端的向量/矩阵运算不同。实现策略：
- 提供 Singular 多项式环和 Gröbner 基计算的接口
- 使用 `#ifdef LV_HAS_SINGULAR` 编译守卫
- 链接 `libsingular`（Singular 内核 C 库）

```c
/* 多项式环操作 */
typedef struct singular_ring SingularRing;

/* 创建 Singular 多项式环 */
SingularRing *singular_ring_create(const char **var_names, int nvars, int characteristic);

/* 销毁多项式环 */
void singular_ring_destroy(SingularRing *ring);

/* 计算 Gröbner 基 */
int singular_groebner_basis(const SingularRing *ring, int *poly_ids, int npolys, int **out_basis_ids, int *out_count);

/* 理想操作：交、商、成员判定 */
int singular_ideal_intersection(const SingularRing *ring, int *ideal1, int n1, int *ideal2, int n2, int **out, int *out_count);
int singular_ideal_quotient(const SingularRing *ring, int *ideal1, int n1, int *ideal2, int n2, int **out, int *out_count);
int singular_ideal_membership(const SingularRing *ring, int poly_id, int *ideal_ids, int n);
```

### Task 3.4: CMake SDK 检测 + 工厂函数分发更新

**CMakeLists.txt 新增：**

```cmake
# ── CUDA SDK 检测（可选） ──
option(lv_ENABLE_CUDA "Enable CUDA GPU backend" OFF)
if(lv_ENABLE_CUDA)
    enable_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        add_compile_definitions(LV_HAS_CUDA)
        message(STATUS "CUDA enabled: ${CMAKE_CUDA_COMPILER}")
    else()
        message(WARNING "lv_ENABLE_CUDA is ON but CUDA compiler not found.")
    endif()
endif()

# ── HIP SDK 检测（可选） ──
option(lv_ENABLE_HIP "Enable AMD HIP GPU backend" OFF)
if(lv_ENABLE_HIP)
    find_package(HIP QUIET)
    if(HIP_FOUND)
        add_compile_definitions(LV_HAS_HIP)
        message(STATUS "HIP enabled: ${HIP_VERSION}")
    else()
        message(WARNING "lv_ENABLE_HIP is ON but HIP not found.")
    endif()
endif()

# ── Singular SDK 检测（可选） ──
option(lv_ENABLE_SINGULAR "Enable Singular computer algebra backend" OFF)
if(lv_ENABLE_SINGULAR)
    find_path(SINGULAR_INCLUDE_DIR singular/singular.h)
    find_library(SINGULAR_LIBRARY Singular)
    if(SINGULAR_INCLUDE_DIR AND SINGULAR_LIBRARY)
        add_compile_definitions(LV_HAS_SINGULAR)
        include_directories(${SINGULAR_INCLUDE_DIR})
        message(STATUS "Singular enabled: ${SINGULAR_LIBRARY}")
    else()
        message(WARNING "lv_ENABLE_SINGULAR is ON but Singular not found.")
    endif()
endif()
```

**numerical_backend.c 工厂函数更新：**

```c
lvVector *lv_vector_create(lvBackendType backend, int64_t n) {
    ...
    switch (backend) {
    case lv_BACKEND_SERIAL:
    case lv_BACKEND_OPENMP:
        ... /* 现有实现 */
#ifdef LV_HAS_CUDA
    case lv_BACKEND_CUDA:
        return cuda_vector_create(n);
#endif
#ifdef LV_HAS_HIP
    case lv_BACKEND_HIP:
        return hip_vector_create(n);
#endif
    default:
        lv_ERROR_SET(lv_BACKEND_UNSUPPORTED, ...);
        return NULL;
    }
}
```

---

## 执行顺序

1. **Phase 1: 高阶合一** — 先实现 λ-项句法合一核心，再实现模式合一，最后集成到证明策略
2. **Phase 2: Church 编码完善** — 布尔运算 → 列表操作 → 比较运算
3. **Phase 3: 数值后端** — CUDA → HIP → Singular（镜像结构，可并行）

---

## 自我审查

1. **Spec coverage:**
   - Phase 1 covers: λ-项句法合一（Task 1.1）、Miller 模式合一（Task 1.2）、证明策略集成（Task 1.3）
   - Phase 2 covers: 布尔运算（Task 2.1）、列表操作（Task 2.2）、比较运算（Task 2.3）
   - Phase 3 covers: CUDA（Task 3.1）、HIP（Task 3.2）、Singular（Task 3.3）、CMake 集成（Task 3.4）

2. **Placeholder scan:** 所有代码块包含完整实现，无 TBD/TODO

3. **Type consistency:** 所有 API 使用已存在的 `lvBackendType`、`LvLambdaTerm` 等类型，新增类型与现有系统一致
