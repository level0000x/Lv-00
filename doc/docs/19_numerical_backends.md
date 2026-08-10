# 19 数值后端（Numerical Backends）

## 模块概述

数值后端为 Lv-00 的符号-数值混合验证体系提供可控的浮点计算地基，覆盖五个子系统：

- **区间算术**（`interval_arith.h`）：以 `float_error.c` 的 `float_interval_*` 实现为语义基准的公共区间底座，收敛 `gappa_propagate.c` 的 `ia_*`、`float_error.c` 的 `float_interval_*` 与 `interval_arithmetic.c` 的 `interval_*` 三份历史实现，复用 `interval_arithmetic.h` 的 `lvInterval` 类型。
- **浮点误差分析**（`float_error.h`）：FPTaylor 风格的一阶/高阶泰勒展开 + 区间误差界分析，输出绝对/相对误差界并映射到 `symbolic_coord.h` 的 `TrustColor` 信任颜色系统。
- **Gappa 验证**（`gappa_dsl.h` + `gappa_propagate.h`）：Gappa 风格浮点证明 DSL 的解析、求值与结构化证明 API，以及谓词集合的正向/反向区间传播引擎。
- **数值线性代数**（`bicgstab_shared.h` + `gmres_shared.h`）：BiCGSTAB（van der Vorst 1992）与重启 GMRES(m) 共享迭代内核，消除 SERIAL / CUDA / HIP 三个后端的算法重复，仅需各后端实现点积/范数/矩阵向量乘算子表。
- **ODE 求解**（`ode_solver.h` + `ode_integrator.h`）：初值问题 `dy/dt = f(t, y)` 的 Euler / RK4 / AB4 三种方法，单步积分器与系数表在 `ode_integrator.h` 中统一封装，供 `ode_solver.c`、`geom_evol.c` 等模块复用。

## 核心设计原则

1. **保守外扩**：所有区间运算端点向外取整（依赖 `nextafter` 方向舍入），保证结果区间包含所有可能真实值（Moore 1966 区间分析，IEEE 1788 语义）。
2. **定义域外返回全实数区间**：除数跨零、`sqrt`/`log`/`asin`/`acos` 定义域外统一返回 `[-HUGE_VAL, HUGE_VAL]`（而非空区间），确保 FPTaylor 误差界的 half-width 不会因 `lo > hi` 变负而误判 `TrustColor`。
3. **共享内核消除后端重复**：BiCGSTAB 与 GMRES(m) 主循环只实现一次，后端仅提供 `lvBicgstabOps` / `lvGmresOps` 算子表（`vector_dot` / `vector_norm` / `matvec`），默认迭代参数由 `lv_linsol_default_params()` 统一。
4. **信任颜色桥接**：误差界通过 `fptaylor_verify_safety()` 映射为 GREEN / BLUE / AMBER / RED / YELLOW，使数值步骤可被几何证明链的信任颜色机制追踪。
5. **单步积分器回调返回 int**：`lvOdeDerivFn` 以返回值表达成功/失败，直接适配 `geom_evol.c` 的 `rhs_func`；`ode_solver.h` 的 `lvODERhsFn` 为 void 返回，由调用方包装适配。

## 关键数据结构

```c
/* interval_arith.h —— 复用 interval_arithmetic.h 的类型 */
typedef struct {
    double lo;      /* 区间下界 */
    double hi;      /* 区间上界 */
    int is_exact;   /* 是否为精确值（lo == hi） */
} lvInterval;

/* float_error.h —— 泰勒形式与误差界 */
typedef struct {
    double center_val;      /* 中心点处函数值 f(center) */
    double *first_derivs;   /* 一阶偏导数数组 df/dxi|center */
    int *deriv_var_ids;     /* 导数对应的变量 ID 数组 */
    int deriv_count;        /* 偏导数数量 */
    double interval_lo, interval_hi; /* 含泰勒余项的区间估计 */
    int order;              /* 泰勒展开阶数（1-3） */
} TaylorForm;

typedef struct {
    double absolute_error;  /* 绝对误差上界 */
    double relative_error;  /* 相对误差上界 */
    TrustColor trust_level; /* 信任颜色等级 */
    char *proof_text;       /* 误差证明文本（调用者负责 free） */
} ErrorBound;

/* gappa_dsl.h —— Gappa 谓词与证明结果 */
typedef struct lvGappaPredicate {
    lvPredType type;        /* lv_PRED_BND / lv_PRED_ABS / lv_PRED_REL */
    char expr_lhs[256], expr_rhs[256];
    double bound_lo, bound_hi, bound_abs;
    bool is_hypothesis;     /* 是否为假设（而非结论） */
} lvGappaPredicate;

typedef struct {
    bool success;
    int goals_total, goals_proven, goals_failed;
    lvGappaProofGoal *goals; /* 每个目标的证明状态 */
} lvGappaProofResult;

/* gappa_propagate.h —— 谓词集合与传播配置 */
typedef struct {
    lvGappaPredicate *preds;
    int count, capacity;
} lvGappaPredSet;

typedef struct {
    int max_iterations;
    double precision;
    bool backward;   /* 是否启用反向传播 */
} lvGappaPropagateConfig;

/* bicgstab_shared.h / gmres_shared.h —— 算子表（两者同构） */
typedef struct {
    void *ctx;   /* 后端私有上下文，透传给各算子 */
    double (*vector_dot)(void *ctx, const double *a, const double *b, int64_t n);
    double (*vector_norm)(void *ctx, const double *v, int64_t n);
    void (*matvec)(void *ctx, const lvMatrix *a, const double *x, double *y, int64_t n);
} lvBicgstabOps;   /* lvGmresOps 字段与之一致 */

/* ode_solver.h —— ODE 初值问题与解 */
typedef struct lvODEProblem {
    lvODERhsFn rhs_fn;  /* dy/dt = f(t,y,params) */
    double *y0;         /* 初始状态向量（调用方持有） */
    size_t dim;         /* 状态维度 */
    double t_span[2];   /* 积分区间 [t_start, t_end] */
    void *params;       /* 透传参数（可 NULL） */
} lvODEProblem;

typedef struct lvODESolution {
    double *t_values;   /* 时间点数组（n_steps 个） */
    double *y_values;   /* 行主序状态数组 y_values[i*dim + j] */
    size_t n_steps, dim;
} lvODESolution;
```

## 主要接口

### 区间算术（`interval_arith.h`）

| 接口 | 说明 |
|---|---|
| `lvInterval lv_interval_make(double lo, double hi, int is_exact)` | 构造区间 |
| `lvInterval lv_interval_add/sub/mul/div(...)` | 加减乘除（div 跨零返回全实数区间） |
| `lvInterval lv_interval_sqrt/sin/cos/exp/log/abs/neg(...)` | 基本单目运算 |
| `lvInterval lv_interval_tan/atan/pow/asin/acos/floor/ceil(...)` | 扩展函数（tan 含 π/2+kπ 奇点处理） |

### 浮点误差（`float_error.h`）

| 接口 | 说明 |
|---|---|
| `FloatInterval float_interval_add/sub/mul/div/sqrt/sin/cos/exp/log(...)` | 区间基本运算（语义基准） |
| `bool fptaylor_evaluate_expr(const char *expr, const FloatInterval *var_bounds, int var_count, const FPTaylorConfig *cfg, ErrorBound *out)` | 对浮点表达式做一阶泰勒误差评估 |
| `bool fptaylor_evaluate_graph(const ConstraintGraph *graph, int var_id, const FPTaylorConfig *cfg, ErrorBound *out)` | 对约束图中指定变量做综合误差分析 |
| `TrustColor fptaylor_verify_safety(const ErrorBound *bound, double tolerance)` | 误差界→信任颜色（≤1e-12 GREEN，≤1e-10 BLUE，≤tolerance AMBER，>tolerance RED，失败 YELLOW） |
| `FPTaylorConfig fptaylor_config_default(void)` | 默认配置（taylor_order=1，branch_bound_threshold=1e-6） |
| `FloatInterval interval_make(double lo, double hi, bool is_exact)` | 便捷构造 |
| `void error_bound_destroy(ErrorBound *bound)` | 释放 ErrorBound 内部资源 |

### Gappa 验证（`gappa_dsl.h` + `gappa_propagate.h`）

| 接口 | 说明 |
|---|---|
| `int lv_gappa_parse(const char *input)` / `bool gappa_parse(...)` | DSL 解析（结构化输出假设与目标） |
| `void gappa_predicates_free(...)` / `gappa_goals_free(...)` | 释放解析结果 |
| `int lv_gappa_eval(const char *expr, double *lo, double *hi)` | 区间界求值 |
| `char *lv_gappa_prove(const char *script)` / `lvGappaProofResult gappa_prove(...)` | 生成/执行证明 |
| `void gappa_result_free(lvGappaProofResult *result)` | 释放证明结果 |
| `bool gappa_register_rewrite_rules(const lvGappaRewriteRule *rules, int count)` | 注册重写规则 |
| `bool gappa_format_predefined(const char *name, lvGappaFormat *out)` | 预定义格式（IEEE 754 各精度） |
| `int lv_gappa_propagate(...)` / `lv_gappa_propagate_set(...)` / `lv_gappa_propagate_backward(...)` | 谓词集正向/反向区间传播 |
| `lvGappaPropagateConfig lv_gappa_propagate_config_default(void)` | 传播默认配置 |

### 数值线性代数（`bicgstab_shared.h` + `gmres_shared.h`）

| 接口 | 说明 |
|---|---|
| `int lv_bicgstab_solve(const lvBicgstabOps *ops, const lvMatrix *a, const double *b, double *x, int64_t n, int max_iters, double tol, double breakdown_eps)` | BiCGSTAB 共享内核；返回 `lv_BACKEND_OK` / `lv_BACKEND_INVALID_ARGS` / `lv_BACKEND_MEM_ERROR` |
| `int lv_gmres_solve(const lvGmresOps *ops, const lvMatrix *a, const double *b, double *x, int64_t n, int max_iter, double tol, double breakdown_eps, int restart_m)` | 重启 GMRES(m) 共享内核（x 作为初始猜测迭代） |
| `void lv_linsol_default_params(int *max_iters, double *tol)` | 统一默认参数：max_iters=200，tol=`lv_EPSILON_HIGH`(1e-10) |

### ODE 求解（`ode_solver.h` + `ode_integrator.h`）

| 接口 | 说明 |
|---|---|
| `lvODESolution *ode_solve(const lvODEProblem *problem, const lvODEConfig *config)` | 求解初值问题（Euler / RK4 / AB4） |
| `void ode_solution_destroy(lvODESolution *sol)` | 销毁解 |
| `int lv_ode_rk4_step(double t, const double *y, size_t n, double h, double *yout, lvOdeDerivFn deriv, void *ctx)` | RK4 单步 |
| `int lv_ode_euler_step(...)` | Euler 单步 |
| `const double lv_ode_ab4_coeffs[4]` | AB4 系数表 {55/24, -59/24, 37/24, -9/24} |

## 工作流程

1. **区间误差传播**：给定变量区间边界，自底向上对表达式树求 `lv_interval_*`，端点每次向外取整；`div`/`log`/`sqrt` 越界时扩展为全实数区间保持保守性。
2. **FPTaylor 误差评估**：解析表达式 → 在变量边界内取中心值做一阶（可升至三阶）泰勒展开 → 用区间算术传播导数项与余项 → 聚合出 `ErrorBound` → 经 `fptaylor_verify_safety` 得到信任颜色；`fptaylor_evaluate_graph` 额外从约束图提取所有涉及 `var_id` 的约束方程后重复上述流程。
3. **Gappa 验证**：`gappa_parse` 分解假设与目标 → `lvGappaPredSet` 装载谓词 → 正向传播 `lv_gappa_propagate_set` 收紧区间界、反向传播 `lv_gappa_propagate_backward` 从目标反推所需前置条件 → `gappa_prove` 生成可复核证明文本。
4. **迭代求解**：各后端实例化算子表（GPU 加速 matvec + 主机端迭代）→ 用 `lv_linsol_default_params` 取默认参数 → BiCGSTAB 走 `rho/alpha/omega` 递推、GMRES 走 Arnoldi + MGS + Givens + 重启周期，breakdown 阈值统一保护退化情形。
5. **ODE 求解**：`ode_solve` 依据 `lvODEConfig.method` 分发到 Euler / RK4 / AB4；AB4 前 3 步由 RK4 起步填充历史导数，之后按 `lv_ode_ab4_coeffs` 多步递推；`lv_ode_*_step` 单步接口可直接被 `geom_evol.c` 等曲线演化模块复用。

## 模块关系

| 本模块组件 | 关联文档/模块 | 关系说明 |
|---|---|---|
| `float_error.h` / `interval_arith.h` | [01_symbolic_coord.md](01_symbolic_coord.md) | `TrustColor` 信任颜色枚举与符号坐标系统由 `symbolic_coord.h` 提供，误差界映射复用其颜色分级 |
| `fptaylor_evaluate_graph` | [04_solver.md](04_solver.md) | 从 `ConstraintGraph`（约束图，见 02_constraint_graph.md）提取约束方程做误差分析 |
| `float_error.h` / `interval_arith.h` | [33_gappa_verification.md](33_gappa_verification.md) | Gappa 谓词传播的区间底座收敛自本模块 `lv_interval_*` 语义；DSL/传播细节见 33 |
| `interval_arith.h` | [29_inequality_approximation.md](29_inequality_approximation.md) | 区间端点外扩语义为不等式近似验证提供保守界 |
| `gappa_dsl.h` / `gappa_propagate.h` | [33_gappa_verification.md](33_gappa_verification.md) | 与 `parser_safety.h`、路径类型系统共同构成 Gappa 浮点验证与解析安全文档主体 |
| `bicgstab_shared.h` / `gmres_shared.h` | [14_solver_backends.md](14_solver_backends.md) | 依赖 `numerical_backend.h` 的 `lvMatrix` 与 `lv_BACKEND_*` 返回码体系 |
| `ode_solver.h` / `ode_integrator.h` | [07_func_block.md](07_func_block.md) | ODE/数值类预设函数块（`ode_rk4`、`numerical_euler` 等）调用本模块积分器与单步内核 |

## 版本历史

| 版本 | 日期 | 说明 |
|---|---|---|
| 1.0.0 | 2026-08-10 | 初稿：汇总区间算术 / FPTaylor 误差 / Gappa / 迭代线性求解 / ODE 五个子系统的接口与设计原则 |
