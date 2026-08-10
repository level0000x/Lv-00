# 39. 数值分析

## 模块概述

数值分析模块为 Lv-00 提供面向浮点计算的工具集，覆盖自动微分、常微分方程（ODE）数值求解、区间算术、误差计数与精度控制四条主线。自动微分（`autodiff.h`）借鉴 Enzyme 与 PyTorch autograd，提供正向/反向两种模式；ODE 求解（`ode_solver.h` 与 `ode_integrator.h`）借鉴 DifferentialEquations.jl 与 SUNDIALS/CVODE，提供 Euler、RK4 与 AB4 方法及共享单步积分器；区间算术（`interval_arith.h`）以 `float_error.c` 的 `float_interval_*` 为语义基准，收敛多份历史区间实现；误差验证（`float_error.h`）借鉴 FPTaylor 的一阶泰勒展开误差界分析；近似模型计数（`approx_counter.h`）借鉴 ApproxMC 的 XOR 哈希框架，提供 PAC 保证的约束图模型计数。

**覆盖头文件**：
- `autodiff.h` —— 前向/反向自动微分引擎（常量、变量、加乘、sin/cos/pow 表达式）
- `ode_solver.h` —— ODE 初值问题定义、配置与求解入口（Euler/RK4/AB4）
- `ode_integrator.h` —— 共享单步积分器（RK4/Euler）与 AB4 系数表
- `interval_arith.h` —— 公共区间算术库，复用 `interval_arithmetic.h` 的 `lvInterval`
- `float_error.h` —— FPTaylor 风格泰勒展开 + 区间算术误差界分析
- `approx_counter.h` —— ApproxMC 风格近似模型计数（PAC 保证）

## 核心设计原则

1. **符号精确优先，数值仅作支撑**：精确算术（`exact_arithmetic.h` 的 `lv_TOLERATED_FLOAT`）与符号推理优先，数值模块只负责误差可控的求值与验证。
2. **区间保守性**：所有区间运算端点向外取整，保证结果区间包含全部可能真实值；定义域外（除零、`sqrt/log/asin/acos` 越界）返回保守全实数区间，避免空区间导致的误判。
3. **单步积分器共享**：`ode_integrator.h` 统一 RK4/Euler 单步与 AB4 系数，供 `ode_solver.c` 与 `geom_evol.c` 复用，消除跨文件重复实现。
4. **误差显式记账**：每次近似都记录绝对/相对误差上界与信任颜色（`ErrorBound`），并由 `fptaylor_verify_safety` 映射到 TrustColor。
5. **PAC 保证**：模型计数以 `epsilon/delta` 参数提供概率近似正确性，`is_approximately_constructible` 基于计数结果做构造性判断。
6. **接入以数值行为安全为先**：autodiff 因表达式种类不含 DIV 与条件分支，未接入几何约束雅可比构建，保持手写差分。

## 关键数据结构

自动微分表达式树（叶节点为常量/变量，一元节点为 NEG/SIN/COS，二元节点为 ADD/MUL/POW）：

```c
typedef enum lvADExprKind {
    AD_CONST = 0, AD_VAR = 1, AD_ADD = 2, AD_MUL = 3,
    AD_NEG = 4, AD_SIN = 5, AD_COS = 6, AD_POW = 7
} lvADExprKind;

typedef struct lvADExpr {
    lvADExprKind kind;
    double value;
    int var_index;
    struct lvADExpr **children;
    size_t child_count;
    double gradient;   /* 反向模式梯度累积 */
} lvADExpr;
```

ODE 问题、配置与解轨迹：

```c
typedef void (*lvODERhsFn)(double t, const double *y, void *params, double *dydt);

typedef struct lvODEProblem {
    lvODERhsFn rhs_fn;
    double *y0;        /* 初始状态（调用方持有） */
    size_t dim;
    double t_span[2];  /* [t_start, t_end] */
    void *params;
} lvODEProblem;

typedef struct lvODESolution {
    double *t_values;  /* 时间点，size = n_steps */
    double *y_values;  /* 行主序状态，y_values[i*dim+j] */
    size_t n_steps;
    size_t dim;
} lvODESolution;
```

区间与误差界（`lvInterval` 复用 `interval_arithmetic.h` 定义）：

```c
typedef struct { double lo, hi; int is_exact; } lvInterval;

typedef struct {
    double lv_TOLERATED_FLOAT(absolute_error);
    double lv_TOLERATED_FLOAT(relative_error);
    TrustColor trust_level;
    char *proof_text;
} ErrorBound;
```

PAC 计数配置与结果：

```c
typedef struct {
    double epsilon;   /* 允许相对误差，如 0.1 表示 ±10% */
    double delta;     /* 成功概率下界，如 0.99 */
    int seed;
    bool sparse_xor;
    int num_hashes;   /* 0 = 自动选择 */
} PacConfig;

typedef struct {
    uint64_t cell_sol_count;
    int hash_count;
    uint64_t total_count;   /* cell_sol_count * 2^hash_count */
    double confidence;
    char *status_msg;
} ApproxCountResult;
```

## 主要接口

| 头文件 | 接口 | 说明 |
|--------|------|------|
| autodiff.h | `lv_ad_engine_create(mode)` / `lv_ad_engine_destroy` | 引擎生命周期，`mode` 取 `AD_FORWARD`/`AD_REVERSE` |
| autodiff.h | `lv_ad_expr_create_const` / `lv_ad_expr_create_var` | 叶子节点构造 |
| autodiff.h | `lv_ad_expr_add` / `lv_ad_expr_mul` / `lv_ad_expr_sin` / `lv_ad_expr_cos` / `lv_ad_expr_pow` | 表达式组合构造 |
| autodiff.h | `lv_ad_forward_diff(expr, var_index, var_value, value, derivative)` | 单变量前向导数 |
| autodiff.h | `lv_ad_reverse_diff(expr, var_values, var_count, value, gradients)` | 反向梯度（多输入单输出高效） |
| autodiff.h | `lv_ad_eval` / `lv_ad_grad` | 求值与反向后梯度查询 |
| ode_solver.h | `ode_solve(problem, config)` | 求解 `dy/dt = f(t,y)`，返回 `lvODESolution*` |
| ode_solver.h | `ode_solution_destroy(sol)` | 释放解轨迹 |
| ode_integrator.h | `lv_ode_rk4_step(t, y, n, h, yout, deriv, ctx)` | RK4 单步（回调返回 int，0 成功） |
| ode_integrator.h | `lv_ode_euler_step(t, y, n, h, yout, deriv, ctx)` | 显式 Euler 单步 |
| ode_integrator.h | `lv_ode_ab4_coeffs[4]` | AB4 系数表 `{55/24, -59/24, 37/24, -9/24}` |
| interval_arith.h | `lv_interval_make` | 构造 `[lo, hi]` |
| interval_arith.h | `lv_interval_add/sub/mul/div/sqrt/sin/cos/exp/log/abs/neg` | 基础区间运算（端点向外取整） |
| interval_arith.h | `lv_interval_tan/atan/pow/asin/acos/floor/ceil` | 扩展函数（奇点/定义域保守处理） |
| float_error.h | `fptaylor_evaluate_expr(expr, var_bounds, var_count, cfg, out)` | 表达式误差评估 |
| float_error.h | `fptaylor_evaluate_graph(graph, var_id, cfg, out)` | 约束图中指定变量的误差分析 |
| float_error.h | `fptaylor_verify_safety(bound, tolerance)` | 误差界 → TrustColor（GREEN/BLUE/AMBER/RED/YELLOW） |
| float_error.h | `fptaylor_config_default` / `error_bound_destroy` | 默认配置与资源释放 |
| approx_counter.h | `approx_count_solutions(graph, cfg, out)` | 约束图近似模型计数 |
| approx_counter.h | `approx_count_projected(graph, proj_vars, proj_count, cfg, out)` | 投影模型计数 |
| approx_counter.h | `approx_count_to_sat(graph, out_cnf_vars)` | 约束图编码为 DIMACS CNF |
| approx_counter.h | `approx_count_get_pac_bound` / `is_approximately_constructible` | PAC 置信度与近似构造性判断 |

## 工作流程

1. **自动微分**：构造表达式树（常量/变量/算子）→ 正向模式经 `lv_ad_forward_diff` 单趟计算 `f` 与 `f'`；反向模式经 `lv_ad_reverse_diff` 构建计算图并反向传播梯度，`lv_ad_grad` 查询单变量梯度。
2. **ODE 求解**：定义 `lvODEProblem`（右端函数、初值、区间、维度）与 `lvODEConfig`（方法、固定步长 `dt`、`max_steps`、`rtol/atol`）→ `ode_solve` 按 `ODE_EULER`/`ODE_RK4`/`ODE_ADAMS` 迭代 → 结果以行主序存入 `lvODESolution`。几何演化（`geom_evol`）复用 `lv_ode_rk4_step`/`lv_ode_euler_step` 与 AB4 系数。
3. **区间验证**：以 `lv_interval_*` 对表达式做保守区间传播；`fptaylor_evaluate_expr`/`fptaylor_evaluate_graph` 输出泰勒形式与误差界；`fptaylor_verify_safety` 将绝对误差与容差比较映射为信任颜色（≤1e-12 GREEN、≤1e-10 BLUE、≤容差 AMBER、超容差 RED、无法判定 YELLOW）。
4. **误差计数与精度控制**：`approx_count_solutions` 以 XOR 哈希分层采样，按 `cell_sol_count * 2^hash_count` 估计模型总数并给出置信度；`approx_count_get_pac_bound` 依 Chernoff-Hoeffding 界计算置信度下界；`is_approximately_constructible` 在 `total_count > 0` 时判定近似可构造。

## 模块关系

| 文档 | 关联内容 |
|------|----------|
| [29_inequality_approximation.md](29_inequality_approximation.md) | 不等式推理、近似模型计数与浮点误差评估的符号侧对应 |
| [33_gappa_verification.md](33_gappa_verification.md) | Gappa DSL 与谓词传播，区间语义与 `interval_arith.h` 互补 |
| [19_numerical_backends.md](19_numerical_backends.md) | 数值后端统一层，ODE/AD/区间库的工程接入 |
| [28_number_theory.md](28_number_theory.md) | 精确算术（有理数/GMP），与浮点误差链路的符号对照 |
| [14_solver_backends.md](14_solver_backends.md) | 求解后端，数值误差与约束求解的接口边界 |
| [15_geometry_advanced.md](15_geometry_advanced.md) | 几何演化（`geom_evol`）复用单步积分器与事件检测 |
| [01_symbolic_coord.md](01_symbolic_coord.md) | TrustColor 权威定义，`fptaylor_verify_safety` 的输出目标 |
| [32_runtime_monitoring.md](32_runtime_monitoring.md) | 运行时监控，误差计数与精度控制的观测侧 |

## 版本历史

- **v5.0.0**
  - 补全数值分析文档：自动微分、ODE 求解、区间算术、误差计数与精度控制。

- **v1.1.0**（autodiff / ode_solver / float_error / approx_counter）
  - autodiff 明确"已实现待接入"状态及候选接入点评估结论（不接入，保持手写差分）。
  - ODE 求解支持 Euler / RK4 / AB4，配置 `rtol/atol` 预留自适应扩展。

- **v1.0.0**（interval_arith / ode_integrator）
  - 公共区间算术库收敛 `gappa_propagate.c`、`float_error.c` 与 `interval_arithmetic.c` 多份实现。
  - 共享单步积分器（RK4/Euler）与 AB4 系数表，供 ode_solver 与 geom_evol 复用。
