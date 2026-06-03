# MPFI 多精度浮点区间算术参考文档

> 版本：1.0 | 日期：2026-05 | 语言：全部中文

---

## 1. 项目概述

### 1.1 项目简介

**MPFI**（Multiple Precision Floating-point Interval arithmetic）是一个基于 MPFR 库的多精度浮点区间算术库，由法国国家信息与自动化研究所（INRIA）的 Nathalie Revol 与 Fabrice Rouillier 共同开发。MPFI 的核心目标是同时提供**保证性结果**（通过区间计算）与**精确结果**（通过多精度算术），使得数值计算的每一步都附带可验证的误差边界。

MPFI 的基本原理是将每一个实数用包含该实数的闭区间来表示，区间的端点为 MPFR 多精度浮点数。所有算术运算与数学函数在区间操作数上进行扩展运算，保证精确结果一定落在计算所得的区间之内。这种"向外舍入"（outward rounding）机制由底层 MPFR 库的正确舍入功能提供支撑。

MPFI 的典型应用场景包括：严格数值验证（证明某个数值量确实落在指定范围内）、全局优化（在搜索空间中排除不可能包含最优解的区域）、方程求解（根隔离与验证）、以及计算机辅助证明（如几何定理的数值验证部分）。

### 1.2 技术栈

| 维度 | 详情 |
|------|------|
| 语言 | C（C99 标准），纯 C 实现，无 C++ 依赖 |
| 依赖 | GMP、MPFR（Multiple Precision Floating-point Reliable Library） |
| 构建系统 | Autotools（configure + make） |
| 精度模型 | 任意精度，运行时可动态调整（二进制位数，典型值 53 至数千位） |
| 核心数据结构 | `mpfi_t`（区间实数，由两个 `mpfr_t` 端点组成） |
| 运算覆盖 | 四则运算、幂运算、平方根、三角函数、指数函数、对数函数、集合运算 |
| 代码仓库 | https://gitlab.inria.fr/mpfi/mpfi |
| 最新版本 | 1.5.4（截至 2026 年 5 月） |

### 1.3 社区活跃度

MPFI 是一个成熟且稳定的项目，具有以下社区特征：

- **开发维护**：由 INRIA 的 Pascaline 项目团队持续维护。主要开发者 Nathalie Revol（ENS Lyon / LIP）和 Fabrice Rouillier（INRIA Paris-Rocquencourt）均为多精度算术与区间计算领域的资深研究者。
- **发布节奏**：采用低频稳定发布策略。从 1.0 版本（2002 年）到 1.5.4 版本，经历了约 20 余年的渐进式改进。每次发布均确保与最新版 MPFR/GMP 的兼容性。
- **学术引用**：在严格数值计算、计算机辅助证明、全局优化等领域被广泛引用。代表性论文为 Revol 和 Rouillier 发表于 *Reliable Computing*（2005 年）的 "Motivations for an arbitrary precision interval arithmetic and the MPFI library"。
- **下游项目**：被 SAGE 数学系统、CGAL 计算几何库、GNU Octave 区间工具包等多个知名项目间接或直接使用。MPFR 官方网站将 MPFI 列为其首要扩展库。
- **通信渠道**：通过 MPFR 邮件列表（mpfr at inria.fr）进行用户与开发者交流。

### 1.4 许可证

MPFI 采用 **LGPL（GNU Lesser General Public License）**。该许可证对 Lv-00 的影响如下：

- **动态链接**：将 MPFI 编译为动态库（`.dll`/`.so`）后链接至 Lv-00 的 `liblv`，Lv-00 可保留独立许可证，仅需在分发时附带 MPFI、MPFR、GMP 各自的许可证声明。
- **静态链接**：将 MPFI 编译为静态库（`.a`）后链接，需确保 Lv-00 的链接目标以 LGPL 兼容方式许可，或向用户提供目标文件以供重新链接。
- **代码修改**：若对 MPFI 源码进行修改并分发，修改后的版本必须同样以 LGPL 许可。

MPFI 的 LGPL 许可与 Lv-00 的纯 C 技术栈完全兼容，不引入额外的运行时依赖或许可证冲突。

---

## 2. 核心借鉴点

### 2.1 MPFI 特性与 Lv-00 现有精确坐标系统对照表

| 序号 | 借鉴点 | MPFI 特性 | Lv-00 现有对应 | 互补价值分析 |
|------|--------|-----------|---------------|-------------|
| a | **区间表示** | `mpfi_t` 以 `[lo, hi]` 闭区间表示实数，端点为任意精度 `mpfr_t`。语义为"真值一定在 [lo, hi] 内"。 | 有理数（`mpq_t`）精确但仅覆盖有理数；代数数精确但计算代价高；二次根式仅覆盖二次扩张；超越数仅支持符号判等。 | 填补"超越函数求值"与"非线性方程验证"的空白。对于无法符号化的计算（如 sin(pi/7)），提供严格数值边界。 |
| b | **向外舍入** | 所有运算的端点通过 MPFR 的定向舍入（`MPFR_RNDD`/`MPFR_RNDU`）保证结果区间包含真值。 | 数值路径使用 IEEE 754 双精度浮点，存在舍入误差累积风险，可能导致错误的符号判断。 | 为 Lv-00 提供"永不遗漏真值"的保证，可替代数值路径中不安全的浮点比较操作。 |
| c | **任意精度** | 运算精度可在运行时动态设置（`mpfr_prec_t`），从 2 位到数万位均可。 | 有理数和代数数天然支持任意精度，但超越函数无法在符号层面处理。 | 当区间过宽导致无法判定时，可动态提升精度重新计算，实现"精度自适应"策略。 |
| d | **超越函数覆盖** | 支持 `mpfi_sin`、`mpfi_cos`、`mpfi_exp`、`mpfi_log`、`mpfi_sqrt`、`mpfi_pow` 等，全部返回严格区间。 | 超越数类型仅支持 pi 和 e 的符号表示，不支持 sin(pi/3) 等超越函数的数值求值。 | 使 Lv-00 能够对包含超越函数的几何表达式进行严格数值验证，大幅扩展可验证的命题范围。 |
| e | **集合运算** | 提供 `mpfi_intersect`（交集）和 `mpfi_union`（并集），支持区间的合并与精化。 | 代数数隔离区间管理有类似需求，但当前实现为 ad-hoc 逻辑。 | 可借鉴 MPFI 的集合运算接口，统一 Lv-00 中所有涉及区间操作的逻辑。 |
| f | **空区间检测** | `mpfi_is_empty` 可检测区间是否为空集（lo > hi），用于判断"无解"情形。 | 约束求解器在无解时依赖返回码或异常，缺乏统一的"空集"语义。 | 引入空区间概念后，求解器可以更优雅地表达"该约束在给定范围内无解"的结论。 |
| g | **区间度量** | `mpfi_diam`（直径）和 `mpfi_mid`（中点）提供区间质量评估手段。 | 位数熔断系统监控有理数位数增长，但缺乏对区间宽度的度量。 | 区间直径可作为"精度不足"的信号，触发自动精度提升或符号化简。 |
| h | **C 语言原生接口** | 全部 API 遵循 GMP/MPFR 风格，无 C++ 运行时依赖。 | Lv-00 为纯 C 实现，技术栈完全匹配。 | 可直接通过 `#include <mpfi.h>` 绑定，编译时链接 `-lmpfi -lmpfr -lgmp`。 |

### 2.2 详述：MPFI 区间算术原理

MPFI 的区间表示基于经典的闭区间算术理论。每个 `mpfi_t` 对象在内部存储两个 `mpfr_t` 值：

```
typedef struct {
    mpfr_t left;   // 区间左端点（含）
    mpfr_t right;  // 区间右端点（含）
} mpfi_struct;
```

语义为：对于任意操作结果，真值 x 满足 `left <= x <= right`。基本运算的区间传播规则如下：

```
若 X = [a, b], Y = [c, d]，则
X + Y = [a + c, b + d]           （端点分别向下/向上舍入）
X - Y = [a - d, b - c]
X * Y = [min(ac,ad,bc,bd), max(ac,ad,bc,bd)]
1 / X = [1/b, 1/a]  （当 0 不在 X 内时）
sqrt(X) = [sqrt(a), sqrt(b)]      （当 a >= 0 时）
```

**依赖问题（Dependency Problem）**：当同一变量在表达式中多次出现时（如计算 X * X），区间算术会丢失变量间的相关性，导致结果区间过宽。MPFI 通过任意精度来缓解此问题——提升精度可使端点更接近真值，从而缩小过估计。

**与 FLINT/Arb 的对比**：MPFI 采用经典区间表示 `[lo, hi]`，而 FLINT/Arb 采用中点-半径表示 `[mid +/- rad]`。两者在数学上等价，但实现策略不同。MPFI 的优势在于与 MPFR 的紧密集成和成熟的定向舍入机制；Arb 的优势在于更快的特殊函数实现和更丰富的多项式/矩阵运算。对于 Lv-00 而言，MPFI 的 MPFR 兼容性是关键优势——Lv-00 已依赖 GMP，引入 MPFI 仅需额外安装 MPFR。

---

## 3. Lv-00 映射方案

### 3.1 新增 INTERVAL 精确坐标类型

在 Lv-00 第 1 层的 `symbolic_coord.h` 中扩展 `CoordType` 枚举，新增区间算术作为第 5 种精确坐标类型。

```c
/* === symbolic_coord.h 扩展片段 === */

#ifdef LV_USE_MPFI
#include <mpfi.h>
#endif

/* -- 扩展 CoordType 枚举，新增 INTERVAL 类型 -- */
typedef enum {
    RATIONAL,        /* 已有：有理数（GMP mpq_t） */
    ALGEBRAIC,       /* 已有：代数数（极小多项式 + 隔离区间） */
    QUADRATIC,       /* 已有：二次根式（a + b*sqrt(n)） */
    TRANSCENDENTAL,  /* 已有：超越常数（pi, e） */
    INTERVAL,        /* 新增：区间实数（MPFI mpfi_t） */
} CoordType;

/* -- 扩展 SymbolicCoord 联合体 -- */
typedef struct SymbolicCoord {
    CoordType type;
    union {
        Rational       *rational;
        Algebraic      *algebraic;
        Quadratic      *quadratic;
        Transcendental *transcendental;
#ifdef LV_USE_MPFI
        mpfi_t          interval_val;   /* INTERVAL */
#endif
    } data;
    TrustColor trust;
    mpfr_prec_t precision;  /* 区间精度，仅 INTERVAL 类型使用 */
} SymbolicCoord;

/* -- 区间坐标工厂函数 -- */
#ifdef LV_USE_MPFI
void symbolic_coord_init_interval(SymbolicCoord *c, mpfr_prec_t prec);
void symbolic_coord_set_interval_d(SymbolicCoord *c, double val, mpfr_prec_t prec);
void symbolic_coord_set_interval_mpq(SymbolicCoord *c, const mpq_t val, mpfr_prec_t prec);
int  symbolic_coord_parse_interval(SymbolicCoord *c, const char *str, mpfr_prec_t prec);
void symbolic_coord_clear_interval(SymbolicCoord *c);
#endif
```

### 3.2 区间算术运算包装

在 `symbolic_coord.c` 中实现区间坐标的四则运算与数学函数包装。

```c
/* === symbolic_coord.c 区间运算片段 === */

#ifdef LV_USE_MPFI

/* 区间加法：res = a + b，非区间类型自动提升 */
void symbolic_coord_add_interval(SymbolicCoord *res,
                                 const SymbolicCoord *a,
                                 const SymbolicCoord *b)
{
    SymbolicCoord tmp_a, tmp_b;
    if (a->type != INTERVAL) {
        symbolic_coord_promote_to_interval(&tmp_a, a); a = &tmp_a;
    }
    if (b->type != INTERVAL) {
        symbolic_coord_promote_to_interval(&tmp_b, b); b = &tmp_b;
    }
    mpfi_add(res->data.interval_val,
             a->data.interval_val, b->data.interval_val);
    if (a == &tmp_a) symbolic_coord_clear_interval(&tmp_a);
    if (b == &tmp_b) symbolic_coord_clear_interval(&tmp_b);
}

/* 区间除法：res = a / b，除数包含零时返回错误 */
int symbolic_coord_div_interval(SymbolicCoord *res,
                                const SymbolicCoord *a,
                                const SymbolicCoord *b)
{
    if (mpfi_has_zero(b->data.interval_val)) {
        mpfi_set_inf(res->data.interval_val);
        return -1;  /* 除零风险 */
    }
    mpfi_div(res->data.interval_val,
             a->data.interval_val, b->data.interval_val);
    return 0;
}

/* 区间正弦函数：res = sin(a) */
void symbolic_coord_sin_interval(SymbolicCoord *res, const SymbolicCoord *a)
{
    mpfi_sin(res->data.interval_val, a->data.interval_val);
}

/* 区间符号判断：1=正, -1=负, 0=不确定（包含零） */
int symbolic_coord_sign_interval(const SymbolicCoord *a)
{
    if (mpfi_is_positive(a->data.interval_val)) return 1;
    if (mpfi_is_neg(a->data.interval_val))      return -1;
    return 0;  /* 区间包含零 */
}

/* 区间直径：返回 hi - lo，用于评估精度质量 */
void symbolic_coord_diam_interval(mpfr_t diam, const SymbolicCoord *a)
{
    mpfi_diam(diam, a->data.interval_val);
}

#endif /* LV_USE_MPFI */
```

### 3.3 求解器区间验证路径

在 Lv-00 第 3 层的求解器中新增区间验证模式。求解器先用浮点路径快速求出候选解，再通过区间算术验证该解的合法性。

```c
/* === solver.h 区间验证扩展片段 === */

typedef enum {
    SOLVER_VERIFY_NONE,       /* 已有：不验证 */
    SOLVER_VERIFY_SYMBOLIC,   /* 已有：符号路径验证 */
    SOLVER_VERIFY_INTERVAL,   /* 新增：MPFI 区间算术验证 */
} SolverVerifyMode;

typedef enum {
    INTERVAL_VERIFY_VALID,       /* 解确实落在约束容差内 */
    INTERVAL_VERIFY_INVALID,     /* 解被证明落在约束容差外 */
    INTERVAL_VERIFY_UNCERTAIN,   /* 区间过宽，无法判断 */
} IntervalVerifyResult;

#ifdef LV_USE_MPFI

/**
 * 对候选解执行 MPFI 区间验证。
 * 工作流程：候选解转区间 -> 代入约束计算残差区间 -> 三态判定。
 * @return VALID / INVALID / UNCERTAIN
 */
IntervalVerifyResult solver_verify_interval(
    const SymbolicCoord soln[], int n_vars,
    const SolverConstraint constraints[], int n_cons,
    mpfr_prec_t prec, const mpfr_t tol);

/**
 * 自适应精度验证：UNCERTAIN 时自动翻倍精度重试。
 */
IntervalVerifyResult solver_verify_interval_adaptive(
    const SymbolicCoord soln[], int n_vars,
    const SolverConstraint constraints[], int n_cons,
    mpfr_prec_t initial_prec, const mpfr_t tol, int max_retries);

#endif /* LV_USE_MPFI */
```

### 3.4 精度自适应策略

区间算术的核心挑战在于结果区间可能因依赖问题而过宽。以下代码展示精度自适应求值框架。

```c
/* === interval_adaptive.c 精度自适应片段 === */

#ifdef LV_USE_MPFI

/**
 * 自适应精度求值：区间直径超过阈值时自动翻倍精度重试。
 * @param expr        表达式求值函数指针
 * @param expr_ctx    表达式上下文
 * @param initial_prec 初始精度（二进制位）
 * @param max_prec    最大精度硬上限
 * @param max_retries 最大重试次数
 * @param diam_thresh 直径阈值，低于此值时接受结果
 * @param result      输出：最终求值结果区间
 * @return            0=成功, -1=达到精度上限
 */
int interval_eval_adaptive(
    void (*expr)(mpfi_t result, const void *ctx, mpfr_prec_t prec),
    const void *expr_ctx,
    mpfr_prec_t initial_prec, mpfr_prec_t max_prec,
    int max_retries, const mpfr_t diam_thresh, mpfi_t result)
{
    mpfr_prec_t prec = initial_prec;
    mpfr_t diam;
    mpfr_init2(diam, initial_prec);

    for (int retry = 0; retry <= max_retries; retry++) {
        mpfi_set_prec(result, prec);
        expr(result, expr_ctx, prec);

        mpfr_set_prec(diam, prec);
        mpfi_diam(diam, result);

        if (mpfr_cmp(diam, diam_thresh) < 0) {
            mpfr_clear(diam); return 0;
        }
        prec *= 2;
        if (prec > max_prec) {
            mpfr_clear(diam); return -1;
        }
    }
    mpfr_clear(diam);
    return -1;
}

#endif /* LV_USE_MPFI */
```

### 3.5 编译配置

```cmake
# === CMakeLists.txt MPFI 集成片段 ===
option(LV_USE_MPFI "Enable MPFI interval arithmetic (requires MPFR)" OFF)

if(LV_USE_MPFI)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GMP  REQUIRED gmp>=6.0)
    pkg_check_modules(MPFR REQUIRED mpfr>=4.0)
    pkg_check_modules(MPFI REQUIRED mpfi>=1.5)
    target_compile_definitions(lv PRIVATE LV_USE_MPFI)
    target_include_directories(lv PRIVATE
        ${GMP_INCLUDE_DIRS} ${MPFR_INCLUDE_DIRS} ${MPFI_INCLUDE_DIRS})
    target_link_libraries(lv PRIVATE
        ${MPFI_LIBRARIES} ${MPFR_LIBRARIES} ${GMP_LIBRARIES})
    message(STATUS "MPFI interval arithmetic: ENABLED")
else()
    message(STATUS "MPFI interval arithmetic: DISABLED")
endif()
```

---

## 4. 实现路线图

| 阶段 | 内容 | 工期 | 依赖 |
|------|------|------|------|
| **Phase 1** | **基础设施集成** | 2-3 周 | GMP/MPFR/MPFI 编译安装成功 |
| 1.1 | 安装依赖链，验证编译链接，编写 Hello World 测试 | 2 天 | - |
| 1.2 | `CoordType` 新增 `INTERVAL`，`SymbolicCoord` 新增 `mpfi_t` 成员 | 1 天 | 1.1 |
| 1.3 | 实现生命周期函数：`init`、`set_d`、`set_mpq`、`parse`、`clear` | 3 天 | 1.2 |
| 1.4 | 实现类型提升函数 `promote_to_interval` | 3 天 | 1.3 |
| 1.5 | CMake 集成：`LV_USE_MPFI` 选项、条件编译 | 2 天 | 1.1 |
| 1.6 | 单元测试：区间坐标创建、类型提升、序列化 | 3 天 | 1.3-1.5 |
| **Phase 2** | **区间算术运算** | 2-3 周 | Phase 1 |
| 2.1 | 四则运算区间版本：`add`、`sub`、`mul`、`div`（含除零检测） | 4 天 | Phase 1 |
| 2.2 | 数学函数区间版本：`sqrt`、`sin`、`cos`、`exp`、`log`、`pow` | 5 天 | 2.1 |
| 2.3 | 区间比较与符号判断：`sign`、`is_positive`、`is_neg`、`contains_zero` | 2 天 | 2.1 |
| 2.4 | 集合运算：`intersect`、`union`、`is_empty` | 2 天 | 2.1 |
| 2.5 | 区间度量：`diam`、`mid`、`width_ratio` | 1 天 | 2.1 |
| 2.6 | 单元测试：临界情形（零区间、宽区间、包含零、除零） | 4 天 | 2.1-2.5 |
| **Phase 3** | **求解器区间验证** | 2-3 周 | Phase 2 |
| 3.1 | 新增 `SolverVerifyMode`、`IntervalVerifyResult` 枚举 | 1 天 | - |
| 3.2 | 实现 `solver_verify_interval`：候选解转区间、残差求值、三态判定 | 5 天 | 3.1 + Phase 2 |
| 3.3 | 实现自适应精度提升 `solver_verify_interval_adaptive` | 3 天 | 3.2 |
| 3.4 | 与位数熔断系统集成：区间直径超限触发熔断 | 2 天 | 3.3 |
| 3.5 | 集成测试：标准几何命题（三角形内角和、勾股定理）区间验证 | 5 天 | 3.4 |
| **Phase 4** | **多项式区间扩展** | 2-3 周 | Phase 2 |
| 4.1 | 新增 `mpz_poly_interval_t`，包装 MPFI 区间系数多项式 | 3 天 | Phase 2 |
| 4.2 | 实现区间多项式求值 `mpz_poly_eval_interval` | 3 天 | 4.1 |
| 4.3 | 实现区间多项式根隔离（二分法 + Sturm 序列） | 5 天 | 4.2 |
| 4.4 | 与渲染管线集成：函数图像绘制的区间求值模式 | 4 天 | 4.3 |
| **Phase 5** | **文档与发布** | 1-2 周 | Phase 1-4 |
| 5.1 | 开发者文档：区间算术模块架构与 API 参考 | 2 天 | - |
| 5.2 | 用户手册："Lv-00 的五种精确坐标类型" | 1 天 | - |
| 5.3 | 性能基准测试：符号/数值/区间三条路径对比 | 3 天 | Phase 3 |
| 5.4 | 发布说明、已知限制、与 FLINT/Arb 对比分析 | 2 天 | 5.3 |

**总工期估计**：约 9-14 周（视团队成员在 GMP/MPFR/MPFI 上的熟悉程度）。

---

## 5. 附录

### 5.1 关键 API 列表

**生命周期管理**

| 函数 | 签名 | 说明 |
|------|------|------|
| `mpfi_init` | `void mpfi_init(mpfi_t x)` | 初始化区间，默认精度 |
| `mpfi_init2` | `void mpfi_init2(mpfi_t x, mpfr_prec_t p)` | 初始化区间，指定精度 |
| `mpfi_clear` | `void mpfi_clear(mpfi_t x)` | 释放区间内部资源 |
| `mpfi_set_prec` | `void mpfi_set_prec(mpfi_t x, mpfr_prec_t p)` | 设置区间精度 |
| `mpfi_get_prec` | `mpfr_prec_t mpfi_get_prec(const mpfi_t x)` | 获取当前精度 |

**赋值与转换**

| 函数 | 签名 | 说明 |
|------|------|------|
| `mpfi_set` | `int mpfi_set(mpfi_t y, const mpfi_t x)` | 区间赋值 y = x |
| `mpfi_set_d` | `int mpfi_set_d(mpfi_t x, double d)` | 从 double 设置（点区间） |
| `mpfi_set_q` | `int mpfi_set_q(mpfi_t x, const mpq_t q)` | 从 GMP 有理数设置 |
| `mpfi_set_str` | `int mpfi_set_str(mpfi_t x, const char *s, int base)` | 从字符串解析 |
| `mpfi_get_d` | `double mpfi_get_d(const mpfi_t x)` | 获取中点的 double 近似 |

**算术运算**

| 函数 | 签名 | 说明 |
|------|------|------|
| `mpfi_add` | `int mpfi_add(mpfi_t z, const mpfi_t x, const mpfi_t y)` | z = x + y |
| `mpfi_sub` | `int mpfi_sub(mpfi_t z, const mpfi_t x, const mpfi_t y)` | z = x - y |
| `mpfi_mul` | `int mpfi_mul(mpfi_t z, const mpfi_t x, const mpfi_t y)` | z = x * y |
| `mpfi_div` | `int mpfi_div(mpfi_t z, const mpfi_t x, const mpfi_t y)` | z = x / y |
| `mpfi_neg` | `int mpfi_neg(mpfi_t y, const mpfi_t x)` | y = -x |
| `mpfi_sqrt` | `int mpfi_sqrt(mpfi_t y, const mpfi_t x)` | y = sqrt(x) |
| `mpfi_pow` | `int mpfi_pow(mpfi_t y, const mpfi_t x, unsigned long n)` | y = x^n |
| `mpfi_abs` | `int mpfi_abs(mpfi_t y, const mpfi_t x)` | y = |x| |

**数学函数**

| 函数 | 签名 | 说明 |
|------|------|------|
| `mpfi_sin` | `int mpfi_sin(mpfi_t y, const mpfi_t x)` | y = sin(x) |
| `mpfi_cos` | `int mpfi_cos(mpfi_t y, const mpfi_t x)` | y = cos(x) |
| `mpfi_exp` | `int mpfi_exp(mpfi_t y, const mpfi_t x)` | y = exp(x) |
| `mpfi_log` | `int mpfi_log(mpfi_t y, const mpfi_t x)` | y = log(x) |
| `mpfi_log2` | `int mpfi_log2(mpfi_t y, const mpfi_t x)` | y = log2(x) |
| `mpfi_log10` | `int mpfi_log10(mpfi_t y, const mpfi_t x)` | y = log10(x) |

**集合运算与查询**

| 函数 | 签名 | 说明 |
|------|------|------|
| `mpfi_intersect` | `int mpfi_intersect(mpfi_t y, const mpfi_t x, const mpfi_t z)` | 交集 |
| `mpfi_union` | `int mpfi_union(mpfi_t y, const mpfi_t x, const mpfi_t z)` | 并集（凸包） |
| `mpfi_is_empty` | `int mpfi_is_empty(const mpfi_t x)` | 是否为空集 |
| `mpfi_diam` | `int mpfi_diam(mpfr_t d, const mpfi_t x)` | 直径 d = hi - lo |
| `mpfi_mid` | `int mpfi_mid(mpfr_t m, const mpfi_t x)` | 中点 m = (lo+hi)/2 |
| `mpfi_is_positive` | `int mpfi_is_positive(const mpfi_t x)` | 是否严格为正 |
| `mpfi_is_neg` | `int mpfi_is_neg(const mpfi_t x)` | 是否严格为负 |
| `mpfi_has_zero` | `int mpfi_has_zero(const mpfi_t x)` | 是否包含零 |
| `mpfi_cmp` | `int mpfi_cmp(const mpfi_t x, const mpfi_t y)` | 区间比较（基于中点） |
| `mpfi_cmp_d` | `int mpfi_cmp_d(const mpfi_t x, double d)` | 区间与 double 比较 |

### 5.2 Lv-00 五种精确坐标类型对比

| 类型 | 存储表示 | 精确性 | 速度 | 适用场景 | 信任颜色 |
|------|---------|--------|------|---------|---------|
| **RATIONAL** | `mpq_t` | 完全精确（有理数域内） | 快 | 有理坐标几何、精确构造 | GREEN |
| **ALGEBRAIC** | 极小多项式 + 隔离区间 | 完全精确（代数数域内） | 中等 | 代数方程求解、根的精确表示 | GREEN |
| **QUADRATIC** | `a + b*sqrt(n)` | 完全精确（二次扩张域内） | 快 | 常见几何构造（中点、距离） | GREEN |
| **TRANSCENDENTAL** | 符号名（"pi", "e"） | 符号级精确 | 最快（仅判等） | 角度表示、符号推导 | BLUE |
| **INTERVAL** | `mpfi_t`（MPFI 区间） | 严格（附带误差界） | 中等偏慢 | 超越函数验证、临界判断、数值证明 | AMBER |

### 5.3 MPFI 与 FLINT/Arb 对比

| 维度 | MPFI | FLINT/Arb |
|------|------|-----------|
| 区间表示 | 经典区间 `[lo, hi]` | 中点-半径 `[mid +/- rad]` |
| 底层依赖 | GMP + MPFR | GMP + MPFR（可选） |
| 定向舍入 | 通过 MPFR 的 `MPFR_RNDD`/`MPFR_RNDU` | 自行实现 |
| 特殊函数 | 基础三角函数、指数、对数 | 丰富（Gamma、Bessel、超几何等） |
| 多项式/矩阵 | 无内置支持 | `arb_poly`、`arb_mat` |
| 成熟度 | 20+ 年，稳定 | 10+ 年，活跃开发 |
| 与 Lv-00 兼容性 | 直接兼容（GMP/MPFR 技术栈一致） | 需额外引入 FLINT 依赖 |
| 许可证 | LGPL | LGPL |

**建议**：Lv-00 优先集成 MPFI（依赖链最短、技术栈最匹配），未来如有更丰富的特殊函数需求，可考虑同时引入 FLINT/Arb 作为补充后端。

### 5.4 参考文献

| 序号 | 文献 | 来源 |
|------|------|------|
| 1 | N. Revol, F. Rouillier, "Motivations for an arbitrary precision interval arithmetic and the MPFI library", *Reliable Computing*, 11(4), 275-290, 2005 | [INRIA RR-05](http://perso.ens-lyon.fr/nathalie.revol/publis/RR05.pdf) |
| 2 | R. Moore, R. B. Kearfott, M. J. Cloud, *Introduction to Interval Analysis*, SIAM, 2009 | SIAM |
| 3 | G. Alefeld, J. Herzberger, *Introduction to Interval Analysis*, Academic Press, 1983 | Academic Press |
| 4 | A. Neumaier, *Interval Methods for Systems of Equations*, Cambridge Univ. Press, 1990 | CUP |
| 5 | E. Hansen, *Global Optimization Using Interval Analysis*, Marcel Dekker, 1992 | Marcel Dekker |
| 6 | R. B. Kearfott, *Rigorous Global Search: Continuous Problems*, Kluwer, 1996 | Kluwer |
| 7 | MPFR 官方网站 | https://www.mpfr.org/ |
| 8 | MPFI 代码仓库 | https://gitlab.inria.fr/mpfi/mpfi |
| 9 | Nathalie Revol 软件页面 | https://perso.ens-lyon.fr/nathalie.revol/software.html |
| 10 | IEEE 754-2019 浮点算术标准 | IEEE |

### 5.5 关键术语中英对照

| 英文 | 中文 |
|------|------|
| Interval Arithmetic | 区间算术 |
| Multiple Precision | 多精度 / 任意精度 |
| Outward Rounding | 向外舍入 |
| Guaranteed Result | 保证性结果 |
| Dependency Problem | 依赖问题 |
| Wrapping Effect | 包裹效应 |
| Interval Diameter | 区间直径 |
| Empty Interval | 空区间 |
| Root Isolation | 根隔离 |
| Correct Rounding | 正确舍入 |
| Rigorous Numerics | 严格数值 |

### 5.6 相关 Lv-00 文档

| 文档 | 路径 |
|------|------|
| 符号坐标系统设计 | `docs/01_symbolic_coord.md` |
| FLINT/Arb 精确数值参考 | `docs/reference/flint_arb_rigorous_numerics.md` |
| 约束图建模 | `docs/02_constraint_graph.md` |
| 求解器架构 | `docs/04_solver.md` |

---

> 本文档仅供 Lv-00 项目内部参考。MPFI 的代码和文档版权归 INRIA 及其原作者所有，LGPL 许可证条款适用于任何对 MPFI 代码的修改与分发。
