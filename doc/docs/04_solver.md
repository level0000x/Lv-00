# 04 符号代数求解器（Symbolic Algebraic Solver）

## 模块概述

符号代数求解器将几何约束问题转化为多项式方程组，通过 Gröbner 基消去理论求解。它是 Lv-00 的核心计算引擎，覆盖从"约束 → 代数方程"到"方程 → 解集/诊断"的完整链路，并针对交互式构造场景提供增量求解、多解分支与实时反馈。

本模块跨越一组头文件，各司其职：

- `solver.h`：公共入口与聚合层。Groebner 基求解（`solve_algebraic_system`）、全类型约束方程提取（`solver_extract_equations_full`）、增量求解（`solver_incremental_solve`）、自由度计算（`count_degrees_of_freedom`）、冲突检测（`check_conflict_equations`）、代数数运算（`compute_algebraic_resultant`）、多解分支（`solver_handle_multiple_solutions`）、交互反馈（`solver_feedback_*`）与稀疏求解（`solver_sparse_solve`）。
- `solver_types.h`：共享数据结构 `PolyEquation`/`EquationSystem` 与二次求根工具，作为 solver 各实现文件的单一事实来源。
- `solver_dirty_set.h`：脏变量追踪，支撑"仅重解脏变量子图"的增量求解。
- `groebner_engine.h`：借鉴 Singular/Macaulay2 的多项式环、理想、Gröbner 基与代数簇计算引擎。
- `solver_core.h`：CDCL SAT 求解器核心（`lvSolver`），提供 SAT/代数并行的另一条求解路径。
- `critical_pair.h`：关键对计算引擎，以规范化 + 合一判定重写规则汇合性，属于验证层。
- `bicgstab_shared.h` / `gmres_shared.h`：BiCGSTAB / GMRES(m) 共享迭代内核，统一 SERIAL/CUDA/HIP 后端的数值线性求解。

## 核心设计原则

1. **约束 → 理想 → 簇**：把几何约束编码为多项式并生成理想 `I = <f_1, ..., f_k>`，求解等价于求代数簇 `V(I)`；零维簇给出有限离散解，正维簇对应连续解空间（自由参数），与 GroebnerResult 的 `unique`/`overdetermined` 标记直接对应。
2. **精确优先**：方程系数使用 GMP 整数缩放（`mpz_poly_t`，缩放因子 `lv_SOLVER_SCALE_FACTOR = 1000`），代数路径避免浮点误差；浮点仅用于根判定的相对容差（`lv_rel_tol_scale` 与 `lv_EPSILON_DOUBLE`）。
3. **增量求解**：`DirtyVariableSet` 记录变化的变量，`filter_equations_for_dirty()` 仅筛选涉及脏变量的方程子集，避免每次交互后全图重解。
4. **多解显式化**：`SolverStatus` 枚举完整刻画解空间形态（`UNIQUE/MULTIPLE/NO_SOLUTION/OVERCONSTRAINED`）；二次方程的双根通过 `solver_handle_multiple_solutions()` 展开为分支，过滤与主方程矛盾的组合。
5. **共享工具单一来源**：二次求根（`solver_quadratic_roots_double`）、不同实根判定（`solver_quadratic_distinct_roots`）与计数（`solver_count_positive_disc_quadratics`）收敛于 solver_types.h，杜绝多文件语义漂移。
6. **环归属显式化**（借鉴 Singular）：每个 `lvPolynomial` 携带 `ring_id`，运算前校验环一致性，防止跨环混算。
7. **代数/数值双路径**：精确 Gröbner 路径与稀疏数值路径（`solver_sparse_solve` + Krylov 迭代内核）共存，前者保真、后者提速，按约束规模与精度需求选择。
8. **可观测反馈**：Solvespace 风格的 `SolverFeedback` 在每次交互后返回变量状态（唯一/自由/过约束）与自由度，供画布即时刷新。

## 关键数据结构（C 代码块）

```c
/* solver_types.h —— 方程系统 */
typedef struct {
    mpz_poly_t poly; /* 一元多项式系数 */
    int var_node_id; /* 关联的变量节点 ID */
    int coord_index; /* 坐标索引（0=x, 1=y） */
} PolyEquation;

typedef struct EquationSystem {
    lvDArray eqs;    /* PolyEquation 的动态数组 */
} EquationSystem;

/* solver_dirty_set.h —— 脏变量追踪 */
typedef struct {
    lvDArray dirty_ids; /* 脏变量 ID 列表 */
} DirtyVariableSet;

/* solver.h —— 求解结果与状态 */
typedef struct GroebnerResult {
    SymbolicCoord **solutions; /* 解数组 */
    int solution_count;
    int solution_capacity;     /* 倍增扩容 */
    bool unique;               /* 唯一解 */
    bool overdetermined;       /* 过度约束 */
} GroebnerResult;

typedef enum {
    SOLVER_STATUS_OK, SOLVER_STATUS_UNIQUE, SOLVER_STATUS_MULTIPLE,
    SOLVER_STATUS_NO_SOLUTION, SOLVER_STATUS_OVERCONSTRAINED,
    SOLVER_STATUS_OUT_OF_SCOPE, SOLVER_STATUS_TIMEOUT, SOLVER_STATUS_OUT_OF_MEMORY
} SolverStatus;

/* groebner_engine.h —— 环 / 理想 / 基 / 簇 */
typedef struct lvPolynomialRing {
    int ring_id;
    char **var_names;
    int var_count;
    lvRingFieldType field;   /* Q / R / C / GF(p) / Z */
    lvMonomialOrder order;   /* LEX / GRLEX / GREVLEX / ELIM / WEIGHT */
    int *elim_vars;
    int elim_var_count;
    double *weights;
    int finite_field_char;
    char *label;
    bool is_commutative;
} lvPolynomialRing;

typedef struct lvIdeal {
    int ideal_id;
    int ring_id;
    lvPolynomial **generators;
    int generator_count;
    int generator_capacity;
    lvGroebnerBasis *cached_basis; /* 惰性缓存 */
    bool basis_valid;
    char *label;
} lvIdeal;

typedef struct lvVariety {
    int variety_id;
    int ideal_id;
    double **solution_points;
    int solution_count;
    int solution_capacity;
    int variety_dimension;      /* Krull 维数 */
    int degree_of_freedom;
    bool is_zero_dimensional;
    char *label;
} lvVariety;

/* bicgstab_shared.h / gmres_shared.h —— 数值迭代内核算子表 */
typedef struct {
    void *ctx;
    double (*vector_dot)(void *ctx, const double *a, const double *b, int64_t n);
    double (*vector_norm)(void *ctx, const double *v, int64_t n);
    void (*matvec)(void *ctx, const lvMatrix *a, const double *x, double *y, int64_t n);
} lvBicgstabOps; /* lvGmresOps 与之同构 */
```

## 主要接口（表格）

| 分组 | 接口 | 说明 |
| --- | --- | --- |
| 方程提取 | `solver_extract_equations_full` | 从全部约束类型提取方程：INCIDENCE→叉积=0（线性）、INTERSECTION→参数化线性系统、CONTAINMENT→卷绕数（标记超范围）、BETWEENNESS→无独立方程（用于解选择） |
| 方程系统 | `equation_system_create/destroy/count/get_poly/get_var_id/get_coord_index` | 方程系统生命周期与查询；`equation_system_init/push/clear` 为栈上变体，`lv_equation_push_checked` 携带 OOM 错误 |
| Gröbner 求解 | `solve_algebraic_system` | 主入口：按脏变量集（可为 NULL）求解，返回 `GroebnerResult`；失败时自动释放并置 NULL 旧结果 |
| Gröbner 计算 | `groebner_basis_compute` | 简化 Buchberger 算法，仅处理总次数 ≤ 2；超范围返回 `SOLVER_OUT_OF_SCOPE`，超步数返回 `SOLVER_TIMEOUT` |
| 增量求解 | `solver_incremental_solve` | 仅重解与脏变量相关的最小依赖子图 |
| 多解分支 | `solver_handle_multiple_solutions` | 对 k 个二次方程展开 2^k 个笛卡尔积分支并过滤无效组合 |
| 诊断 | `count_degrees_of_freedom` | 返回自由变量数并输出自由变量 ID 数组（0 自由度 ≠ 出错 -1） |
| 诊断 | `check_conflict_equations` | 检测约束系统矛盾方程 |
| 诊断 | `analyze_out_of_scope` | 分析超范围变量并生成建议 |
| 消除 | `eliminate_geometry` | 对目标变量消去指定变量集合 |
| 代数数 | `compute_algebraic_resultant` | 结式法计算两个代数数和/积的最小多项式（度数 ≤ 4） |
| 交互反馈 | `solver_feedback_create/destroy/solve` | Solvespace 风格：增量求解并返回结构化反馈（动态字段由 `solver_feedback_destroy` 统一释放） |
| 稀疏求解 | `solver_sparse_solve` | 稀疏矩阵后端（SuiteSparse/GraphBLAS 风格）替代密集 GMP 路径 |
| 环管理 | `ring_registry_create/destroy`、`ring_create/destroy/register/find` | 多项式环注册表与环生命周期 |
| 多项式 | `poly_create/destroy/add/multiply/substitute/get` | 环上的多项式运算 |
| 理想/基 | `ideal_create/destroy/add_generator`、`groebner_compute`、`groebner_compute_incremental` | 理想构造、Gröbner 基计算与增量更新 |
| 理想运算 | `ideal_membership`、`ideal_intersection`、`ideal_quotient` | 成员判定、交、商 |
| 簇 | `constraint_graph_to_ideal`、`variety_compute`、`variety_is_zero_dimensional`、`variety_dimension`、`variety_get_solution_point` | 图→理想→簇的解算链路 |
| 数值内核 | `lv_bicgstab_solve`、`lv_gmres_solve`、`lv_linsol_default_params` | BiCGSTAB / GMRES(m) 共享迭代内核与统一默认参数（max_iters=200, tol=1e-10） |
| 汇合验证 | `critical_pair_compute_all/compare/compare_all/export_text/get_statistics` | 重写规则关键对计算与汇合性判定（`export_text` 输出可被 nauty/Traces 解析的邻接表） |
| CDCL SAT | `lv_solver_create/create_with_config/destroy`、`lv_solver_new_var(s)`、`lv_solver_add_constraint`、`lv_solver_solve`、`lv_solver_solve_under_assumptions`、`lv_solver_solve_algebraic` | CDCL 求解器另一条求解路径；`lv_solver_get_coord` 从赋值解码符号坐标 |

## 工作流程

1. **脏变量标记**：交互编辑后以 `dirty_set_init/add/contains` 维护 `DirtyVariableSet`；首次全量求解时脏集为空或全部变量。
2. **方程提取**：`solver_extract_equations_full()` 遍历图中约束生成 `EquationSystem`；约束提取语义与 `ConstraintType`（INCIDENCE/INTERSECTION/CONTAINMENT/BETWEENNESS）一一对应。
3. **增量过滤**：`filter_equations_for_dirty()` 按 `(var_node_id, coord_index)` 键（`poly_eq_same_key`）筛选子集，仅重解受影响方程。
4. **Gröbner 基**：`groebner_basis_compute()` 原地将系统替换为约化基；次数 > 2 返回 `SOLVER_OUT_OF_SCOPE` 并转入稀疏数值路径。
5. **求解与解码**：`solve_algebraic_system()` 生成 `GroebnerResult`，解以 `SymbolicCoord**` 表示；`groebner_result_destroy()` 释放。
6. **多解处理**：当存在二次方程时 `solver_handle_multiple_solutions()` 展开 2^k 分支、逐支回代过滤矛盾组合；返回 `SOLVER_UNIQUE`（实际单解）或 `SOLVER_NO_SOLUTION`（全支无效）等。
7. **诊断**：`count_degrees_of_freedom()` 输出自由变量；`check_conflict_equations()` 定位矛盾；`analyze_out_of_scope()` 处理超范围变量。
8. **数值路径**：线性/二次大规模系统经 `solver_sparse_solve()` 装配稀疏矩阵，再以 `lv_bicgstab_solve()`/`lv_gmres_solve()` 迭代收敛（`lv_linsol_default_params()` 统一参数）。
9. **交互反馈**：`solver_feedback_solve()` 汇总本轮脏变量的唯一/自由/过约束状态，推送画布即时刷新；高级验证可经 critical_pair 对重写规则做汇合性检查。

## 模块关系（表格）

| 相关模块 | 文档 | 关系说明 |
| --- | --- | --- |
| 符号坐标 | 01_symbolic_coord.md | 解以 `SymbolicCoord**` 表示，`solver_extract_equations_full` 从坐标构造多项式 |
| 约束图 | 02_constraint_graph.md | 方程提取的语义来源：节点 ID、坐标索引与 `ConstraintType` 均由约束图提供 |
| 图规范化 | 03_normalization.md | 求解前先经规范化消除冗余对象，减少方程规模；拓扑排序为求解定序 |
| 合一系统 | 06_unify.md | 多解分支与构造等价性最终经合一判定收敛；critical_pair 的汇合性比较依赖规范化 + 合一 |
| 函数块 | 07_func_block.md | 函数块内部约束子系统经求解器独立求解后按端口对外暴露结果 |
| 求解后端 | 14_solver_backends.md | 稀疏求解（`solver_sparse_solve`）与 BiCGSTAB/GMRES 迭代内核的详细后端设计 |
| 约束传播 | 24_constraint_propagation.md | 传播层先行推断的部分确定值作为方程提取的已知量，缩小求解空间 |
| CDCL 核心 | solver_core.h（头文件） | `lvSolver` CDCL 引擎与代数求解器构成双路径，`lv_solver_solve_algebraic` 桥接代数能力 |
| 关键对引擎 | critical_pair.h（头文件） | 验证层：以 `graph_normalize` + `unify_construction_with_proposition_detailed` 判定重写汇合性，间接依赖本模块的规范化输入 |

## 版本历史

| 版本 | 变更说明 |
| --- | --- |
| v0.1 | 初稿：整合 solver.h / solver_types.h / solver_dirty_set.h / groebner_engine.h / solver_core.h / critical_pair.h / bicgstab_shared.h / gmres_shared.h 的真实接口，按"方程提取 → Gröbner 求解 → 多解处理 → 诊断 → 数值后端"组织工作流 |
