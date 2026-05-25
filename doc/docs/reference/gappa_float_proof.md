# Lv-00 参考落地设计文档：Gappa 浮点运算形式化证明工具

> **版本**: 1.0.0
> **日期**: 2026-05-25
> **参考**: [Gappa](https://gappa.gitlabpages.inria.fr/) -- INRIA（法国国家信息与自动化研究所）Guillaume Melquiond 开发
> **目标**: 借鉴 Gappa 的浮点证明 DSL、区间传播与重写规则自动证明机制、Coq 形式化证明脚本生成能力，为 Lv-00 提供浮点运算严格验证、舍入误差形式化证明和自动证明生成的基础设施

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴点](#2-核心借鉴点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 项目简介

Gappa（Génération Automatique de Preuves de Propriétés Arithmétiques，算术性质自动证明生成）是由法国国家信息与自动化研究所（INRIA）的 Guillaume Melquiond 开发的专门用于验证数值公式的形式化工具。Gappa 的核心能力在于对浮点运算和定点运算程序进行严格证明，能够自动生成 Coq 形式化证明脚本，使得验证结果可以被独立的形式化证明检查器（如 Rocq/Coq）所确认。

Gappa 的设计哲学是：用户以简洁的 DSL 描述数值性质（如"在给定条件下，某个浮点表达式的结果落在指定区间内"），工具自动完成区间传播、重写规则应用和证明生成。这种"声明式证明"的方式使得非形式化验证专家也能使用形式化方法的威力。

Gappa 的核心分析管线如下：

```
Gappa DSL 脚本（用户输入）
    |
    +--> 解析器 --> 逻辑公式内部表示
    |
    +--> 区间传播引擎
    |       +--> 基本区间算术（加减乘除、sqrt 等）
    |       +--> 舍入误差界计算（IEEE 754 定向舍入）
    |       +--> 谓词传播（BND, REL, LIN, FIX, FLT, NZR, EQL）
    |
    +--> 重写规则引擎
    |       +--> 实数算术重写（分配律、结合律变换）
    |       +--> 舍入算子重写（浮点/定点格式转换）
    |       +--> 用户自定义提示（approx, rewrite hints）
    |
    +--> 证明生成器
    |       +--> Coq/Rocq 证明脚本输出
    |       +--> Why3 后端集成
    |
    v
  形式化证明 + 误差界证书
```

关键数值特征：

| 指标 | 说明 |
|------|------|
| 分析精度 | 数学证明级别（严格上界） |
| 支持的浮点格式 | IEEE-754 单精度（binary32）、双精度（binary64）、四精度（binary128）、x86 扩展精度 |
| 舍入方向 | 11 种（ne, zr, dn, up, aw, od, no, nz, na, nd, nu） |
| 支持的运算 | +, -, *, /, sqrt, 以及定点/浮点格式转换 |
| 证明后端 | Coq/Rocq（gappalib-coq）、Why3 |
| 输入格式 | Gappa DSL（自定义领域特定语言） |
| 许可证 | CeCILL + GPL 双许可证 |

### 1.2 技术栈

| 维度 | 详情 |
|------|------|
| 语言 | C++（OCaml 用于早期版本，当前版本已迁移至 C++） |
| 依赖 | GMP（多精度整数）、MPFR（多精度浮点） |
| 构建系统 | Autotools（configure + make） |
| 代码仓库 | https://gitlab.inria.fr/gappa/gappa |
| 最新版本 | 1.8.0（截至 2026 年 5 月） |
| Coq 支持库 | gappalib-coq 1.10.0（支持 Coq >= 8.16） |
| 集成接口 | Why3 验证平台后端、Rocq/Coq 自动策略 |

### 1.3 社区活跃度

Gappa 是一个成熟且持续维护的学术项目，具有以下社区特征：

- **开发维护**：由 INRIA 的 Guillaume Melquiond 持续维护，作者同时是 MPFR 库的核心贡献者之一，在浮点形式化验证领域具有深厚的学术积累。
- **发布节奏**：从 1.0.0 到 1.8.0 经历了多年迭代，每次发布均包含功能增强和 bug 修复。Coq 支持库独立版本管理，紧跟 Coq/Rocq 的版本更新。
- **学术引用**：Gappa 在浮点形式化验证领域被广泛引用，代表性论文为 Melquiond 发表于 *Information and Computation*（2012 年）的 "Floating-point arithmetic in the Coq system"。Gappa 已被应用于 CompCert（经过验证的 C 编译器）等工业级验证项目。
- **下游集成**：作为 Why3 验证平台的内置后端证明器，Gappa 可被 SPARK Ada、Frama-C 等工具间接调用。同时作为 Coq/Rocq 的自动策略（tactic），可直接在交互式证明中使用。
- **通信渠道**：通过 Inria GitLab 的 Issue Tracker 进行用户与开发者交流。

### 1.4 许可证

Gappa 采用 **CeCILL + GPL 双许可证**。该许可证对 Lv-00 的影响如下：

- **CeCILL 许可证**：法国法律管辖的自由软件许可证，与 GPL 兼容。
- **GPL 许可证**：Gappa 核心工具以 GPL 发布，若直接链接或修改 Gappa 源码，需遵守 GPL 的传染性要求。
- **Coq 支持库**：以 LGPL 发布，可动态链接而不影响 Lv-00 自身的许可证。
- **借鉴策略**：Lv-00 不直接链接 Gappa 二进制，而是借鉴其设计模式和算法思路（区间传播、重写规则、谓词系统），自行实现 C 语言版本。这种方式完全不受 Gappa 许可证约束。

---

## 2. 核心借鉴点

### 2.1 Gappa 特性与 Lv-00 数值验证能力对照表

| 序号 | 借鉴点 | Gappa 特性 | Lv-00 现有对应 | 互补价值分析 |
|------|--------|-----------|---------------|-------------|
| a | **浮点证明 DSL** | 用户以声明式 DSL 描述数值性质（如 `float<ieee_64,ne>(a) in [0,1] -> ...`），工具自动完成证明 | Lv-00 有约束图和函数块系统，但缺乏面向浮点证明的专用 DSL | 引入浮点证明 DSL 可让用户以接近数学公式的语法描述浮点性质，降低使用门槛 |
| b | **区间传播引擎** | 基于 Moore 区间算术的自动传播，支持 BND/ABS/REL/LIN/FIX/FLT/NZR/EQL 八种谓词 | `float_error.h` 提供了 `FloatInterval` 类型和基本区间运算（加减乘除、sqrt、sin、cos、exp、log） | Gappa 的谓词系统更丰富（REL 相对误差、LIN 线性误差、FIX/FLT 格式属性），可显著增强误差分析能力 |
| c | **舍入误差形式化** | 11 种舍入方向（ne/zr/dn/up/aw/od/no/nz/na/nd/nu），支持 IEEE-754 全部标准舍入模式 | `exact_arithmetic.h` 有 `LV00_TOLERATED_FLOAT` 审计机制，但无舍入方向的系统化建模 | 引入舍入方向枚举和形式化模型，使 Lv-00 能精确模拟不同舍入模式下的误差行为 |
| d | **重写规则系统** | 内置大量重写规则（分配律变换、误差传播变换如 `mul_mars`、`add_xals` 等），支持用户自定义提示 | `rewrite.h` 有 VF2 子图同构匹配和规则热加载，但规则面向几何约束而非数值性质 | Gappa 的数值重写规则可直接移植到 Lv-00 的重写引擎，消减区间过估 |
| e | **Coq 证明生成** | 自动生成 Coq/Rocq 形式化证明脚本，证明可被独立检查 | Lv-00 的 `proof.h` 有证明步骤和自然语言输出，但无形式化证明脚本生成 | 借鉴 Gappa 的证明序列化格式，为 Lv-00 添加可机器检查的证明证书输出 |
| f | **后向推理** | 从目标出发，通过匹配定理的前提条件反向搜索证明路径 | Lv-00 的 `prop_verifier.h` 有 BHK 解释下的前向验证，缺乏后向推理 | 引入后向推理策略可显著提升复杂浮点性质的证明成功率 |
| g | **定点/浮点混合运算** | 同时支持 `float<>` 和 `fixed<>` 格式，可在同一表达式中混合使用 | Lv-00 的 `symbolic_coord.h` 有有理数（精确）和浮点（近似）两种路径，但缺乏定点格式 | 引入定点格式支持可扩展对嵌入式系统和硬件设计验证的适用范围 |
| h | **Why3 后端集成** | 作为 Why3 的自动证明器后端，可被 SPARK Ada、Frama-C 等工具间接调用 | Lv-00 的 `smt_backend.h` 和 `atp_backend.h` 有外部求解器接口，但未集成 Why3 | 借鉴 Gappa 的后端集成模式，为 Lv-00 添加 Why3 兼容的输出格式 |

### 2.2 详述：Gappa 谓词系统与区间传播

Gappa 的核心创新在于其谓词系统。不同于简单的区间算术（仅追踪值的范围），Gappa 同时追踪多种数值性质：

| 谓词 | 含义 | 用途 |
|------|------|------|
| `BND(x, I)` | x 的值在区间 I 内 | 基本值域约束 |
| `ABS(x, I)` | |x| 的值在区间 I 内 | 绝对值界（用于误差传播） |
| `REL(x, y, I)` | x = y * (1 + e)，e 在 I 内 | 相对误差界 |
| `LIN(x, y, I)` | x = y * e，e 在 I 内 | 线性误差界 |
| `FIX(x, k)` | x = m * 2^k，m 为整数 | 定点格式属性 |
| `FLT(x, k)` | x = m * 2^e，|m| < 2^k | 浮点格式属性 |
| `NZR(x)` | x 不为零 | 非零性（除法安全） |
| `EQL(x, y)` | x 和 y 值相等 | 等式约束（重写传播） |

**区间传播示例：** Gappa 处理 `{ x in [1, 2] -> float<ieee_64, ne>(x * x) - x * x in [-1b-52, 1b-52] }` 时的推理过程：

1. `BND(x, [1, 2])` -- 由输入假设得到
2. `BND(x * x, [1, 4])` -- 由区间乘法得到
3. `ABS(x * x, [1, 4])` -- 由 BND 推导
4. `REL(float<ieee_64,ne>(x*x), x*x, [-2^-53, 2^-53])` -- 由 IEEE-754 双精度相对误差界得到
5. `BND(float<ieee_64,ne>(x*x) - x*x, [-4*2^-53, 4*2^-53])` -- 由 REL 和 BND 组合得到

### 2.3 详述：Gappa 重写规则与误差过估消减

Gappa 的重写规则系统是其区别于纯区间算术工具的关键。直接使用区间算术分析 `a*b - c*d` 时，由于区间乘法的保守性，误差界往往过估数个数量级。Gappa 通过以下重写规则消减过估：

- **乘法差变换（mul_mars）**：`a * b - c * d --> (a - c) * d + c * (b - d)`。当 `a` 接近 `c` 且 `b` 接近 `d` 时，右侧两个因子的区间远小于原始因子，从而大幅收紧误差界。
- **加法差变换（add_mibs）**：`(a + b) - (c + d) --> (a - c) + (b - d)`。当 `a` 接近 `c` 且 `b` 接近 `d` 时，此变换可显著收紧结果区间。
- **平方根差变换（sqrt_mibs）**：`sqrt(a) - sqrt(b) --> (a - b) / (sqrt(a) + sqrt(b))`。利用有理化技巧，避免直接对 sqrt 做区间运算导致的过估。

这些重写规则与 Lv-00 现有的 `rewrite.h` 引擎高度兼容，可直接作为 `RewriteRule` 注册到约束图重写系统中。

---

## 3. Lv-00 映射方案

### 3.1 总体架构映射

| Gappa 组件 | Lv-00 对应层 | 映射方式 |
|-----------|-------------|---------|
| DSL 解析器 | 第 1 层（基础类）+ DSL 编译器 | 在 `dsl_compiler.h` 中扩展浮点证明 DSL 语法 |
| 区间传播引擎 | 第 1 层（FloatInterval）+ 第 3 层（求解器） | 扩展 `float_error.h` 的区间算术，增加谓词传播 |
| 重写规则引擎 | 第 3 层（rewrite.h） | 将 Gappa 的数值重写规则注册为 `RewriteRule` |
| 证明生成器 | 第 4 层（proof.h） | 在证明引擎中添加形式化证明证书生成 |
| Coq 后端 | 第 5-7 层（数据交换） | 通过 `interop.h` 输出 Coq 兼容格式 |

### 3.2 C 代码示例：Gappa 风格浮点证明基础设施

以下代码展示如何在 Lv-00 中实现 Gappa 风格的浮点证明核心类型和 API：

```c
/* gappa_dsl.h -- Gappa 风格浮点证明 DSL 核心类型 */

#include "float_error.h"
#include "rewrite.h"
#include "symbolic_coord.h"

/* 舍入方向枚举（对应 Gappa 的 11 种舍入模式） */
typedef enum {
    ROUND_NE,    /* 向最近舍入，平局向偶数（IEEE 754 默认） */
    ROUND_ZR,    /* 向零舍入 */
    ROUND_DN,    /* 向负无穷舍入 */
    ROUND_UP,    /* 向正无穷舍入 */
    ROUND_AW,    /* 远离零舍入 */
    ROUND_OD,    /* 向奇数尾数舍入（Gappa 扩展） */
    ROUND_NO,    /* 向最近舍入，平局向奇数 */
    ROUND_NZ,    /* 向最近舍入，平局向零 */
    ROUND_NA,    /* 向最近舍入，平局远离零 */
    ROUND_ND,    /* 向最近舍入，平局向负无穷 */
    ROUND_NU     /* 向最近舍入，平局向正无穷 */
} GappaRoundingDir;

/* 浮点格式描述（对应 Gappa 的 float<> 模板参数） */
typedef struct {
    int precision_bits;      /* 精度位数 p（IEEE 32: 24, IEEE 64: 53） */
    int min_exponent;        /* 最小指数 d（IEEE 32: -149, IEEE 64: -1074） */
    GappaRoundingDir rounding;
    char name[32];           /* 格式名称（如 "ieee_64"） */
} GappaFloatFormat;

/* Gappa 谓词类型（对应 Gappa 的八种内部谓词） */
typedef enum {
    GAPPA_PRED_BND,   /* BND(x, I): 值域约束 */
    GAPPA_PRED_ABS,   /* ABS(x, I): 绝对值约束 */
    GAPPA_PRED_REL,   /* REL(x, y, I): 相对误差约束 */
    GAPPA_PRED_LIN,   /* LIN(x, y, I): 线性误差约束 */
    GAPPA_PRED_FIX,   /* FIX(x, k): 定点格式属性 */
    GAPPA_PRED_FLT,   /* FLT(x, k): 浮点格式属性 */
    GAPPA_PRED_NZR,   /* NZR(x): 非零约束 */
    GAPPA_PRED_EQL    /* EQL(x, y): 等式约束 */
} GappaPredType;

/* Gappa 谓词 -- 单个数值性质断言 */
typedef struct {
    GappaPredType type;
    int expr_id;           /* 表达式 ID */
    int secondary_expr_id; /* 第二个表达式 ID（REL/LIN/EQL 用） */
    FloatInterval interval; /* 区间参数（BND/ABS/REL/LIN 用） */
    int int_param;         /* 整数参数（FIX/FLT 用） */
} GappaPredicate;

/* 证明目标 -- 对应 Gappa 脚本中的 { hypotheses -> goal } */
typedef struct {
    GappaPredicate *hypotheses;
    int hypothesis_count;
    GappaPredicate goal;
    GappaFloatFormat format;
    char **rewrite_hints;
    int hint_count;
} GappaProofGoal;

/* 证明结果 -- 包含证明状态和证书 */
typedef struct {
    bool proven;
    TrustColor trust_level;   /* 桥接到 Lv-00 信任颜色系统 */
    char *proof_certificate;  /* Coq 脚本或自定义格式 */
    char *proof_trace;        /* 人类可读的证明过程 */
    int steps_used;
} GappaProofResult;

/* 核心 API */
GappaFloatFormat gappa_format_predefined(const char *name,
    GappaRoundingDir rnd);
bool gappa_parse_dsl(const char *dsl_text,
    const GappaFloatFormat *fmt, GappaProofGoal *goal);
bool gappa_prove(const GappaProofGoal *goal,
    GappaProofResult *result);
void gappa_result_free(GappaProofResult *result);
void gappa_goal_free(GappaProofGoal *goal);
```

### 3.3 C 代码示例：谓词传播引擎

以下代码展示谓词传播引擎的核心接口，将 `float_error.h` 的区间算术扩展为完整的谓词传播系统：

```c
/* gappa_propagate.h -- Gappa 风格谓词传播引擎 */

#include "gappa_dsl.h"

#define GAPPA_MAX_PREDICATES 1024

typedef struct {
    GappaPredicate preds[GAPPA_MAX_PREDICATES];
    int count;
} GappaPredSet;

void   gappa_pred_set_init(GappaPredSet *set);
bool   gappa_pred_set_add(GappaPredSet *set,
         const GappaPredicate *pred);
const GappaPredicate *gappa_pred_set_find(
    const GappaPredSet *set, GappaPredType type, int expr_id);

typedef struct {
    int max_iterations;       /* 最大不动点迭代次数 */
    bool enable_rewrite;      /* 是否启用重写规则 */
    bool enable_backward;     /* 是否启用后向推理 */
    double timeout_ms;
} GappaPropagateConfig;

/*
 * 执行谓词传播（不动点迭代）。
 *
 * 传播规则（对应 Gappa 定理表）：
 *   1. 区间算术：BND(a)+BND(b)=>BND(a+b), BND(a)*BND(b)=>BND(a*b)
 *   2. 舍入误差：BND(a)=>REL(float(a),a,[-2^(-p+1),2^(-p+1)])
 *   3. 属性传播：NZR(a)+NZR(b)=>NZR(a*b)
 *   4. 重写规则：EQL(a,b)+BND(b)=>BND(a)
 *
 * 返回 true 表示目标谓词已被证明。
 */
bool gappa_propagate(const GappaPredSet *hypotheses,
    const GappaPredicate *goal,
    const GappaPropagateConfig *config,
    GappaPredSet *derived);

GappaPropagateConfig gappa_propagate_config_default(void);
```

### 3.4 C 代码示例：重写规则注册与信任颜色桥接

将 Gappa 的数值重写规则适配为 Lv-00 的 `RewriteRule`，并将证明结果映射到信任颜色系统：

```c
/* 将 Gappa 数值重写规则注册到 Lv-00 重写引擎 */

/* 乘法差变换：a*b - c*d => (a-c)*d + c*(b-d) */
void gappa_register_mul_diff_rule(ConstraintGraph *graph);

/* 加法差变换：(a+b)-(c+d) => (a-c)+(b-d) */
void gappa_register_add_diff_rule(ConstraintGraph *graph);

/* 平方根差变换：sqrt(a)-sqrt(b) => (a-b)/(sqrt(a)+sqrt(b)) */
void gappa_register_sqrt_diff_rule(ConstraintGraph *graph);

/* 一次性注册全部 Gappa 数值重写规则 */
void gappa_register_rewrite_rules(ConstraintGraph *graph);

/* 信任颜色桥接：Gappa 证明状态 -> Lv-00 TrustColor
 *
 *   完全证明（含 Coq 证书） -> TRUST_GREEN
 *   区间传播成功（无重写） -> TRUST_BLUE
 *   需要用户提示          -> TRUST_AMBER
 *   证明失败              -> TRUST_RED
 *   超时/内部错误         -> TRUST_YELLOW
 */
TrustColor gappa_result_to_trust_color(const GappaProofResult *r);
```

### 3.5 与 Lv-00 现有模块的集成点

Gappa 借鉴功能通过以下接口与 Lv-00 现有系统集成：

- **`float_error.h`**：复用 `FloatInterval` 类型和区间运算函数作为 BND 谓词传播的基础；通过 `fptaylor_verify_safety()` 的扩展版本实现信任颜色桥接。
- **`rewrite.h`**：将 Gappa 数值重写规则注册为 `RewriteRule`，利用 VF2 子图同构匹配实现自动规则应用。
- **`proof.h`**：将 Gappa 证明结果序列化为 `ProofStep`，纳入 Lv-00 的证明导航器管理。
- **`symbolic_coord.h`**：复用 `TrustColor` 枚举，确保浮点证明的信任颜色与几何证明的信任颜色在同一框架内一致传播。

---

## 4. 实现路线图

### 4.1 分阶段实施计划

| 阶段 | 时间 | 任务 | 交付物 | 优先级 |
|------|------|------|--------|--------|
| **短期 1** | 2-3 周 | 舍入方向枚举与浮点格式描述 | `GappaRoundingDir`、`GappaFloatFormat` 及预定义格式工厂函数 | 高 |
| **短期 2** | 2-3 周 | 谓词类型与集合操作 | `GappaPredicate`、`GappaPredSet` 及基本集合操作 | 高 |
| **短期 3** | 3-4 周 | 区间算术传播规则 | BND/ABS/NZR/EQL 谓词传播（基于 `FloatInterval`） | 高 |
| **中期 4** | 3-4 周 | 舍入误差谓词传播 | REL/LIN/FIX/FLT 谓词传播，IEEE-754 误差界计算 | 高 |
| **中期 5** | 2-3 周 | Gappa 数值重写规则注册 | mul_mars、add_mibs、sqrt_mibs 适配为 `RewriteRule` | 中 |
| **中期 6** | 3-4 周 | 不动点传播引擎 | 完整的 `gappa_propagate()`，含前向传播和后向推理 | 中 |
| **中期 7** | 2-3 周 | DSL 解析器（子集） | 值域约束、浮点舍入、简单逻辑连接的解析 | 中 |
| **长期 8** | 4-6 周 | 证明证书生成 | Coq 兼容的证明脚本输出（借鉴 gappalib-coq） | 低 |
| **长期 9** | 3-4 周 | Why3 后端集成 | 在 `smt_backend.h` 基础上添加 Why3 兼容输出 | 低 |
| **长期 10** | 4-6 周 | 定点格式支持 | `fixed<>` 格式扩展，定点运算误差分析 | 低 |

### 4.2 里程碑与验收标准

| 里程碑 | 阶段 | 验收标准 |
|--------|------|---------|
| M1: 基础类型就绪 | 1-2 | 预定义格式工厂函数返回正确的精度和指数参数 |
| M2: 区间传播可用 | 3-4 | `BND(x,[1,2])` + `BND(y,[3,4])` => `BND(x+y,[4,6])`；IEEE-64 格式 => `REL(float(a),a,[-2^-53,2^-53])` |
| M3: 重写规则生效 | 5-6 | `a*b - c*d` 在 a 接近 c、b 接近 d 时，重写后误差界收紧至少一个数量级 |
| M4: DSL 可用 | 7 | 能解析并证明 `{ x in [1, 2] -> x * x in [1, 4] }` |
| M5: 证明证书 | 8 | 生成的 Coq 脚本可通过 `coqc` 编译检查 |

### 4.3 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 区间过估导致证明失败 | 中 | 引入 Gappa 重写规则消减过估；必要时增加分支切割策略 |
| DSL 解析器复杂度高 | 中 | 首期仅实现 DSL 子集，后续迭代扩展 |
| Coq 证书格式兼容性 | 低 | 参照 gappalib-coq 最新文档；提供 Why3 兼容格式作为备选 |
| 与现有 `float_error.h` 接口冲突 | 低 | 新模块以 `gappa_` 前缀命名空间隔离 |

---

## 5. 附录

### 5.1 关键 API 列表

#### 新增 API（gappa_dsl.h）

| 函数 | 说明 |
|------|------|
| `gappa_format_predefined(name, rnd)` | 创建预定义 IEEE 754 浮点格式 |
| `gappa_parse_dsl(dsl_text, fmt, goal)` | 解析 Gappa DSL 脚本 |
| `gappa_prove(goal, result)` | 执行自动证明 |
| `gappa_result_free(result)` | 释放证明结果 |
| `gappa_goal_free(goal)` | 释放证明目标 |

#### 新增 API（gappa_propagate.h）

| 函数 | 说明 |
|------|------|
| `gappa_pred_set_init(set)` | 初始化谓词集合 |
| `gappa_pred_set_add(set, pred)` | 添加谓词（去重） |
| `gappa_pred_set_find(set, type, expr_id)` | 查找谓词 |
| `gappa_propagate(hypotheses, goal, config, derived)` | 执行谓词传播 |

#### 现有 API 复用

| 模块 | 函数 | 在 Gappa 借鉴中的角色 |
|------|------|---------------------|
| `float_error.h` | `interval_add/sub/mul/div` | BND 谓词传播的基础 |
| `float_error.h` | `interval_sqrt/sin/cos/exp/log` | 扩展 BND 传播到超越表达式 |
| `float_error.h` | `fptaylor_verify_safety()` | 信任颜色桥接的现有实现 |
| `rewrite.h` | `rewrite_add_rule()` | Gappa 数值重写规则的注册入口 |
| `rewrite.h` | `rewrite_apply_all()` | 自动尝试所有 Gappa 重写规则 |

### 5.2 Gappa DSL 语法速查（Lv-00 子集）

```
# 值域约束
x in [1, 2]                    # BND(x, [1, 2])

# 浮点舍入
float<ieee_64, ne>(x * x)      # IEEE-754 双精度，向最近舍入
float<53, ne>(a + b)           # 自定义精度 53 位
fixed<-16, dn>(x)              # 定点格式，LSB 权重 2^-16

# 逻辑连接
A /\ B                          # 合取（AND）
A -> B                          # 蕴涵（IMPLIES）

# 算术表达式
a + b, a - b, a * b, a / b     # 四则运算
sqrt(a)                         # 平方根

# 完整证明目标
{ x in [1, 2] /\ y in [3, 4] ->
  float<ieee_64, ne>(x * y) in [3, 8] }
```

### 5.3 参考文献

1. Melquiond, G. "Gappa: a tool for the static and formal analysis of numerical programs." *arXiv preprint arXiv:2111.03281*, 2021.

2. Melquiond, G. "Floating-point arithmetic in the Coq system." *Information and Computation*, 216:14-23, 2012.

3. Melquiond, G. and Rideau, L. "Formalization and automation of floating-point arithmetic: A Coq library for arbitrary precision." *Journal of Automated Reasoning*, 54(2):149-180, 2015.

4. Boldo, S. and Melquiond, G. "Computer Arithmetic and Formal Proofs: Verifying Floating-point Algorithms with the Coq System." *ISTE Ltd and John Wiley & Sons*, 2017.

5. IEEE Computer Society. "IEEE Standard for Floating-Point Arithmetic (IEEE 754-2019)." *IEEE Std 754-2019*, 2019.

6. Moore, R. E., Kearfott, R. B., and Cloud, M. J. "Introduction to Interval Analysis." *SIAM*, 2009.

7. Gappa 官方文档: https://gappa.gitlabpages.inria.fr/gappa/index.html

8. Gappa 源码仓库: https://gitlab.inria.fr/gappa/gappa

9. gappalib-coq 支持库: https://gitlab.inria.fr/gappa/coq

10. Daumas, M. and Melquiond, G. "Certification of bounds on expressions involving rounded operators." *ACM Transactions on Mathematical Software*, 37(1):1-21, 2010.

---

> **文档维护说明**: 本文档应随 Gappa 借鉴功能的实现进度同步更新。当新的 Gappa 版本发布时，需检查本文档中的特性描述是否仍然准确。
