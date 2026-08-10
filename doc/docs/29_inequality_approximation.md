# 29. 不等式推理与近似计算

## 29.1 模块概述

本文档描述 Lv-00 几何元语言系统中的不等式推理、近似模型计数与浮点误差评估模块。该组模块连接符号证明、概率性构造判断与数值可信度评估，用于处理几何系统中的大小关系、误差边界、近似可构造性和浮点表达式稳定性。

**覆盖头文件**：
- `inequality_reasoning.h` —— 纯符号不等式证明系统
- `approx_counter.h` —— ApproxMC 风格近似模型计数
- `herbie_eval.h` —— Herbie 风格浮点精度评估
- `fptaylor_eval.h` —— FPTaylor 风格浮点误差上界分析
- `adaptive_threshold.h` —— 自适应阈值、误差界动态调整与数值判定阈值管理

---

## 29.2 理论定位

Lv-00 的几何元语言以构造和证明为核心，但几何对象之间常常不仅需要判等，也需要判定大小、边界、可行区间和误差范围。因此本模块承担四类职责：

1. **符号不等式证明**：在不使用浮点近似的前提下证明代数与几何不等式。
2. **构造空间规模估计**：通过近似模型计数判断约束系统是否存在大量可行构造。
3. **浮点表达式稳定性评估**：评估同一表达式不同等价形式的数值精度。
4. **严格误差上界分析**：通过 Taylor 展开与区间算术推导浮点计算误差上界。

该模块既支持绿色精确证明路径，也支持 AMBER/近似可信路径。所有近似结果必须显式记录误差界、置信度或可信等级。

---

## 29.3 inequality_reasoning.h —— 符号不等式证明

### 29.3.1 设计原则

`inequality_reasoning.h` 的核心原则是：

- 所有数值计算使用 GMP 多精度有理数；
- 禁止在证明核心中使用浮点运算；
- 不等式证明必须形成可追溯步骤；
- 支持经典不等式、几何不等式和代数不等式的统一表达。

### 29.3.2 不等式关系类型

```c
typedef enum {
    INEQ_LESS_THAN,         // <
    INEQ_LESS_EQUAL,        // ≤
    INEQ_GREATER_THAN,      // >
    INEQ_GREATER_EQUAL,     // ≥
    INEQ_NOT_EQUAL          // ≠
} lvInequalityType;
```

这些关系作用于规范表达式 `lvExpr`，可表示长度、面积、角度、代数坐标差值等对象之间的大小关系。

### 29.3.3 证明状态

```c
typedef enum {
    INEQ_STATUS_UNPROVED,
    INEQ_STATUS_PROVED,
    INEQ_STATUS_DISPROVED,
    INEQ_STATUS_CONDITIONAL,
    INEQ_STATUS_UNKNOWN
} lvInequalityStatus;
```

- `PROVED`：不等式已在给定前提下证明；
- `DISPROVED`：发现反例或可推出相反结论；
- `CONDITIONAL`：仅在附加条件下成立；
- `UNKNOWN`：当前策略无法判定。

### 29.3.4 证明方法

```c
typedef enum {
    INEQ_METHOD_DIRECT,
    INEQ_METHOD_AM_GM,
    INEQ_METHOD_CAUCHY,
    INEQ_METHOD_REARRANGEMENT,
    INEQ_METHOD_SCHUR,
    INEQ_METHOD_JENSEN,
    INEQ_METHOD_HOLDER,
    INEQ_METHOD_MINKOWSKI,
    INEQ_METHOD_TRIANGLE,
    INEQ_METHOD_SUBSTITUTION,
    INEQ_METHOD_INDUCTION,
    INEQ_METHOD_CONTRADICTION,
    INEQ_METHOD_SMART_SUM,
    INEQ_METHOD_SOS
} lvInequalityMethod;
```

这些方法对应经典数学证明策略：

| 方法 | 理论对应 | 典型用途 |
|------|----------|----------|
| AM-GM | 算术—几何平均不等式 | 正数乘积/和约束 |
| Cauchy | Cauchy-Schwarz | 向量、距离、内积 |
| Schur | Schur 不等式 | 对称多项式 |
| Jensen | 凸函数不等式 | 函数型表达式 |
| Hölder | Hölder 不等式 | 多项乘积估计 |
| Minkowski | Minkowski 不等式 | 距离与范数 |
| Triangle | 三角形不等式 | 几何长度约束 |
| SOS | 平方和分解 | 多项式非负证明 |

### 29.3.5 核心结构

```c
struct lvInequality {
    lvExpr *left;
    lvExpr *right;
    lvInequalityType type;
    lvInequalityStatus status;
    char *label;
};
```

```c
typedef struct {
    lvInequalityMethod method;
    lvInequality *ineq;
    char *justification;
    int *premise_ids;
    int premise_count;
} lvInequalityStep;
```

```c
struct lvInequalityProof {
    lvInequality *target;
    lvInequalityStep *steps;
    int step_count;
    int step_capacity;
    lvInequalityStatus status;
    char *error_message;
};
```

不等式证明不是单个布尔值，而是带有步骤、理由和前提引用的证明对象，可接入 `proof_export_enhanced.h` 和 `proof_trace.h`。

### 29.3.6 核心 API

```c
lvInequality *lv_ineq_create(lvExpr *left,
                                 lvInequalityType type,
                                 lvExpr *right);
void lv_ineq_destroy(lvInequality *ineq);
lvInequality *lv_ineq_copy(const lvInequality *ineq);
```

```c
lvInequalitySystem *lv_ineq_system_create(void);
void lv_ineq_system_destroy(lvInequalitySystem *sys);
bool lv_ineq_system_add(lvInequalitySystem *sys, lvInequality *ineq);
```

```c
lvInequalityStatus lv_ineq_prove(lvInequality *ineq,
                                      const lvInequalitySystem *sys,
                                      lvInequalityProof **proof);

lvInequalityStatus lv_ineq_prove_with_method(
    lvInequality *ineq,
    lvInequalityMethod method,
    const lvInequalitySystem *sys,
    lvInequalityProof **proof);
```

---

## 29.4 approx_counter.h —— 近似模型计数

### 29.4.1 设计来源

`approx_counter.h` 借鉴 ApproxMC 与 UniGen 的思想：

- 将约束图编码为 SAT/CNF；
- 添加随机 XOR 哈希约束；
- 估计单个哈希桶中的解数量；
- 使用 `cell_sol_count * 2^hash_count` 估计模型总数；
- 返回 PAC（Probably Approximately Correct）保证。

### 29.4.2 PAC 配置

```c
typedef struct {
    double epsilon;
    double delta;
    int seed;
    bool sparse_xor;
    int num_hashes;
} PacConfig;
```

PAC 语义：

```text
Pr[ |估计值 - 真实值| <= epsilon * 真实值 ] >= 1 - delta
```

其中：
- `epsilon` 控制相对误差；
- `delta` 控制失败概率；
- `sparse_xor` 用于大变量数时降低哈希约束密度。

### 29.4.3 计数结果

```c
typedef struct {
    uint64_t cell_sol_count;
    int hash_count;
    uint64_t total_count;
    double confidence;
    char *status_msg;
} ApproxCountResult;
```

其中：

```text
total_count = cell_sol_count * 2^hash_count
```

### 29.4.4 核心 API

```c
bool approx_count_solutions(const ConstraintGraph *graph,
                            const PacConfig *cfg,
                            ApproxCountResult *out);
```

用于估计整个约束图的可满足赋值数量。

```c
bool approx_count_projected(const ConstraintGraph *graph,
                            int *proj_vars,
                            int proj_count,
                            const PacConfig *cfg,
                            ApproxCountResult *out);
```

用于只统计指定变量集合上的不同赋值，忽略非投影变量的取值差异。

```c
char *approx_count_to_sat(const ConstraintGraph *graph, int *out_cnf_vars);
```

将约束图转换为 DIMACS CNF 格式。

```c
double approx_count_get_pac_bound(const PacConfig *cfg,
                                  const ApproxCountResult *res);
void approx_count_result_destroy(ApproxCountResult *res);
```

### 29.4.5 近似构造性判断

```c
bool is_approximately_constructible(const ConstraintGraph *graph,
                                    double min_prob);
```

当近似模型计数大于 0 且置信度满足 `min_prob` 时，系统可判定约束图“近似可构造”。该结论不能替代严格证明，但可作为搜索策略、交互式构造反馈和大规模模型筛选依据。

---

## 29.5 herbie_eval.h —— Herbie 风格精度评估

### 29.5.1 设计来源

`herbie_eval.h` 借鉴 Herbie、FPBench 与 Rosa：

- 对表达式进行采样评估；
- 计算 bit error 与 relative error；
- 根据 AMBER 评分评估表达式稳定性；
- 将输入域划分为不同 regime；
- 为每个 regime 选择最稳定的表达式等价形式。

### 29.5.2 结果结构

```c
typedef struct {
    char expression[256];
    double max_bit_error;
    double avg_bit_error;
    double max_relative_error;
    double avg_relative_error;
    double amber_score;
    int sample_count;
    int valid_samples;
} lvHerbieResult;
```

AMBER 分数范围 `[0,1]`，越接近 1 表示数值精度越好。

### 29.5.3 Regime 分区

```c
typedef struct {
    lvInterval bounds[lv_TAYLOR_MAX_VARS];
    int var_count;
    double weight;
    char description[128];
} lvHerbieRegime;
```

```c
typedef struct {
    lvHerbieRegime regimes[lv_HERBIE_MAX_REGIMES];
    int regime_count;
    double total_weight;
} lvHerbiePartitionResult;
```

不同输入区间可能具有不同的稳定表达式形式，regime 分区用于记录这种局部稳定性差异。

### 29.5.4 配置与 API

```c
typedef struct {
    int sample_count;
    unsigned int random_seed;
    double amber_alpha;
    double amber_beta;
    int enable_regime_detection;
    double regime_threshold;
} lvHerbieConfig;
```

```c
bool herbie_evaluate(const char *expr,
                     const char **var_names,
                     const lvInterval *var_bounds,
                     int var_count,
                     const lvHerbieConfig *config,
                     lvHerbieResult *out);
```

```c
bool herbie_compare(const char **exprs,
                    int expr_count,
                    const char **var_names,
                    const lvInterval *var_bounds,
                    int var_count,
                    const lvHerbieConfig *config,
                    lvHerbieResult *results,
                    int *best_index);
```

```c
bool herbie_partition_regimes(const char *expr,
                              const char **var_names,
                              const lvInterval *var_bounds,
                              int var_count,
                              const lvHerbieConfig *config,
                              lvHerbiePartitionResult *out);

bool herbie_select_path(const char **exprs,
                        int expr_count,
                        const char **var_names,
                        const lvHerbiePartitionResult *partition,
                        int var_count,
                        const lvHerbieConfig *config,
                        int *best_indices);
```

---

## 29.6 fptaylor_eval.h —— FPTaylor 风格误差上界

### 29.6.1 设计来源

`fptaylor_eval.h` 借鉴 FPTaylor、FPBench 与 Gappa，通过 Taylor 展开与区间算术推导浮点表达式误差上界。与 Herbie 偏向采样评估不同，FPTaylor 路径偏向严格误差界。

### 29.6.2 Taylor 形式

```c
typedef struct {
    double center;
    double vars_center[lv_TAYLOR_MAX_VARS];
    double derivs[lv_TAYLOR_MAX_VARS];
    int var_count;
    double rem_lo;
    double rem_hi;
    int order;
} lvTaylorForm;
```

形式上表示：

```text
f(x) = center + Σ_i deriv[i] * (x_i - x_i_center) + remainder
```

其中余项被限制在 `[rem_lo, rem_hi]`。

### 29.6.3 误差界结果

```c
typedef struct {
    double absolute_error;
    double relative_error;
    double roundoff_error;
    double truncation_error;
    int is_valid;
    char proof_text[1024];
} lvErrorBound;
```

该结构区分：
- `roundoff_error`：浮点舍入误差；
- `truncation_error`：Taylor 余项误差；
- `absolute_error` 与 `relative_error`：最终误差上界。

### 29.6.4 配置与 API

```c
typedef struct {
    int taylor_order;
    double branch_threshold;
    int max_bisections;
    int enable_optimization;
    double rounding_unit;
} lvFPTaylorConfig;
```

```c
bool fptaylor_evaluate(const char *expr,
                       const char **var_names,
                       const lvInterval *var_bounds,
                       int var_count,
                       const lvFPTaylorConfig *config,
                       lvErrorBound *out);
```

```c
bool fptaylor_analyze_expression(const char *expr,
                                 const char **var_names,
                                 const lvInterval *var_bounds,
                                 int var_count,
                                 const lvFPTaylorConfig *config,
                                 lvErrorBound *out,
                                 lvTaylorForm *taylor_out);
```

```c
bool fptaylor_taylor_form(const char *expr,
                          const char **var_names,
                          const double *var_centers,
                          int var_count,
                          int order,
                          lvTaylorForm *out);
```

---

## 29.7 理论—代码对应关系

| 代码概念 | 理论对应 | 说明 |
|----------|----------|------|
| `lvInequality` | 形式不等式命题 | 左右表达式与关系符构成判断 |
| `lvInequalityProof` | 可追踪证明对象 | 保存方法、步骤、前提引用 |
| `INEQ_METHOD_SOS` | 平方和分解证明 | 用于多项式非负性证明 |
| `PacConfig` | PAC 学习/估计参数 | epsilon 控制误差，delta 控制失败概率 |
| `ApproxCountResult` | 近似模型计数结果 | `cell_sol_count * 2^hash_count` |
| `lvHerbieResult` | 采样精度评估 | bit error、relative error、AMBER 分数 |
| `lvHerbieRegime` | 输入域分区 | 局部选择最佳表达式路径 |
| `lvTaylorForm` | Taylor 展开 | 中心值、一阶导、余项界 |
| `lvErrorBound` | 浮点误差上界 | 绝对误差、相对误差、舍入误差 |

---

## 29.8 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [39_numerical_analysis.md](39_numerical_analysis.md) | 区间算术、浮点误差与数值后端 |
| [28_number_theory.md](28_number_theory.md) | 精确有理数、多项式和数论基础 |
| [38_logic_verification.md](38_logic_verification.md) | 逻辑验证与三值逻辑 |
| [14_solver_backends.md](14_solver_backends.md) | SAT/SMT/BDD 后端 |
| [33_gappa_verification.md](33_gappa_verification.md) | Gappa 浮点证明与误差传播 |

---

## 29.9 版本历史

- **v5.0.0**
  - 补全文档化：不等式推理、PAC 近似模型计数、Herbie 风格精度评估、FPTaylor 风格误差上界。
  - 明确符号证明路径与近似可信路径的边界。

- **v3.4.0**
  - 引入 Herbie 与 FPTaylor 风格数值评估接口。

- **v3.3.0**
  - 引入纯符号不等式证明与 ApproxMC 风格模型计数。
