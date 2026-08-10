# 17. 推理层（Layer 4）：求解、重写、证明与多后端调度

## 模块概述

推理层（Layer 4，`lv_LAYER_REASONING`）是 Lv-00 的推理中枢，负责将几何拓扑层（Layer 3）产出的约束图转化为代数方程并求解，进而执行重写、证明与量词推理。本层以 `engine.h` 主引擎为中央调度器，协调以下推理子模块：

- **求解器**（`solver.h`）：Groebner 基求解（度数 ≤ 2）、全类型约束方程提取、增量求解（仅重解脏变量子图）、冲突检测、自由度计算、代数数结式运算、多解分支、Solvespace 风格交互反馈、稀疏矩阵后端。
- **重写引擎**（`rewrite.h`）：VF2 子图同构匹配、规则热加载/卸载、图快照事务回滚、WL 图核哈希循环检测、多非重叠匹配批量应用、Maude 风格策略组合子、Herbie 风格数值精度优化。
- **证明系统**（`proof.h`）：命题管理、合一检查、证明导航器、依赖链信任颜色、爆炸原理与反证作用域、多策略证明引擎（JGEX 风格）、回溯搜索树、不可构造性证明、Sledgehammer 调度、HOL Light 微内核验证、精化类型检查。
- **后端抽象**（`smt_backend.h` / `atp_backend.h` / `sat_encoding.h` / `bdd_encoding.h`）：SMT（Z3/cvc5/Singular）、FOL ATP（Vampire/E Prover/iProver）、SAT 编码与 DIMACS 导出、BDD/ADD 符号化表示。
- **引擎调度器**（`engine_scheduler.h`）：约束图特征分析 → 路由规则匹配 → 自动后端选择与回退。
- **量词系统**（`quantifier.h`）：∀ / ∃ / ∃! 的构造性（BHK）处理、实例化/泛化、有限域量词消去、三值逻辑真值评估。

## 核心设计原则

1. **代数精确优先**：默认使用内置 Groebner 基求解（`GROEBNER`），仅处理总度数 ≤ 2 的多项式系统，保证结果精确可复核。
2. **增量求解**：仅重解"脏变量"的最小依赖子图（`solver_incremental_solve`），配合图快照与冻结点实现事务式回滚。
3. **重写-求解协作**：先重写化简约束、遇停顿再求解、冲突即时暴露（`engine_rewrite_and_solve`）。
4. **多后端统一抽象**：所有后端经"工厂 + 注册表"模式接入（`SMTBackendRegistry` / `ATPBackendRegistry` / `SchedulerBackendEntry`），上层代码与具体求解器解耦。
5. **自动路由与回退**：调度器基于 `GraphFeatures` 路由（含量词 → ATP、非线性算术 → SMT、默认 Groebner），失败按回退链降级。
6. **证明可信度可追溯**：`ProofColor` 信任颜色贯穿依赖链，爆炸原理与 oracle 步骤被显式标记并受假设作用域约束。
7. **构造性量词语义**：量词遵循 BHK 解释，有限域上枚举消去，无限域返回 `lv_UNKNOWN` 而非臆测。
8. **层级边界受控**：本层经 `lv_ALLOW_LAYER` 编译时断言与运行时层级验证标志约束，只允许向下调用低层。

## 关键数据结构

### 求解结果与状态（solver.h）

```c
typedef struct GroebnerResult {
    SymbolicCoord **solutions; /**< 解数组 */
    int solution_count;        int solution_capacity;
    bool unique;               /**< 是否唯一解 */
    bool overdetermined;       /**< 是否过度约束 */
} GroebnerResult;

typedef enum {
    SOLVER_STATUS_OK, SOLVER_STATUS_UNIQUE, SOLVER_STATUS_MULTIPLE,
    SOLVER_STATUS_NO_SOLUTION, SOLVER_STATUS_OVERCONSTRAINED,
    SOLVER_STATUS_OUT_OF_SCOPE, SOLVER_STATUS_TIMEOUT, SOLVER_STATUS_OUT_OF_MEMORY
} SolverStatus;
```

### 重写规则（rewrite.h）

```c
typedef struct RewriteRule {
    RewritePattern *pattern;         /**< 匹配模式（VF2 子图同构） */
    RewriteReplacement *replacement; /**< 替换内容 */
    int reduction_measure;           /**< 归约度量（循环检测用） */
    char *name;
    RewritePrecondition condition_func; /**< 前置条件回调 */
    void *condition_data;
} RewriteRule;
```

### 证明步骤与导航（proof.h）

```c
typedef struct ProofStep {
    int id;  ProofStepType type;  ProofColor color;
    int node_id, constraint_id, rule_id, func_block_id;
    int *dependency_step_ids;  int dependency_count; /* 前驱依赖 */
    int parent_step_id;  int depth;                  /* 证明树结构 */
    bool is_breakpoint;  bool is_completed;  char *note;
} ProofStep;
```

### 调度特征与路由规则（engine_scheduler.h）

```c
typedef struct GraphFeatures {
    int total_nodes, total_constraints;
    int nonlinear_constraints;  double nonlinear_ratio;
    bool has_quantifier_like;   bool has_boolean_variables;
    int estimated_equation_count;  int estimated_degree_max;
    int64_t analysis_time_us;
} GraphFeatures;

typedef struct RoutingRule {
    char name[64];  int priority;  bool enabled;
    RouteCondition conditions[4];  int condition_count;
    RouteCombineMode combine_mode;
    SolverBackendType target_backend;
} RoutingRule;
```

### 量词表达式（quantifier.h）

```c
struct lvQuantifiedExpr {
    int id;  lvQuantifier quantifier;
    char *variable_name;  int variable_node_id;
    lvDomain *domain;  struct Proposition *body_proposition;
    int *instantiated_ids;  int instantiated_count;
    lvTruthValue cached_truth;  bool truth_cache_valid;
};
```

### 后端配置与 SAT 编码（smt_backend.h / sat_encoding.h）

```c
typedef struct SMTSolverConfig {
    int timeout_ms;  int memory_limit_mb;  SMTLogic logic;
    bool produce_models, produce_unsat_cores, produce_proofs, incremental;
    int random_seed;  int verbosity;  void *custom_config;
} SMTSolverConfig;

struct SatEncoding {
    lvDArray var_map;  int next_var_id;
    int **clauses;  int *clause_sizes;  int clause_count;  int clause_capacity;
    int total_vars;  int total_clauses;  double encode_time_ms;
    ConstraintGraph *graph;  const RelModel *rel_model;
};
```

## 主要接口

| 模块 | 头文件 | 关键接口 | 说明 |
|------|--------|----------|------|
| 求解器 | `solver.h` | `solve_algebraic_system` / `solver_incremental_solve` / `groebner_basis_compute` / `solver_extract_equations_full` / `count_degrees_of_freedom` / `check_conflict_equations` / `compute_algebraic_resultant` / `solver_handle_multiple_solutions` / `solver_sparse_solve` / `solver_feedback_solve` | Groebner 求解、方程提取、增量求解、自由度、冲突、代数数、多解分支、稀疏后端 |
| 重写引擎 | `rewrite.h` | `find_rewrite_match` / `vf2_find_match` / `apply_rewrite` / `rewrite_with_rules` / `rewrite_compute_wl_hash` / `detect_rewrite_loop_wl` / `graph_snapshot_create\|restore` / `rewrite_strategy_apply` / `rewrite_search_backward` / `rewrite_num_optimize` | VF2 匹配、批量应用、循环检测、事务回滚、Maude 策略、Herbie 数值优化 |
| 证明引擎 | `proof.h` | `proof_unify(_detailed)` / `proof_navigator_add_step\|next\|prev` / `proof_begin_assumption_scope` / `proof_multi_strategy_execute\|try_all` / `proof_sledgehammer_dispatch` / `proof_minimal_verify` / `proof_search_with_strategy` / `proof_check_unconstructibility` / `proof_export_natural_language` | 合一、导航、作用域、多策略、搜索、不可构造性、自然语言导出 |
| SMT 后端 | `smt_backend.h` | `smtsolver_create` / `smtsolver_solve` / `smtsolver_check` / `smtencode_constraint_graph_to_smtlib2` / `smtsolver_decode_result` / `smtsolver_is_backend_available` / `smt_register_all_plugins` | SMT-LIB2 编码、求解、模型/unsat core 解码、插件注册 |
| ATP 后端 | `atp_backend.h` | `atp_encode_constraint_graph` / `atp_solver_solve_graph` / `atp_proof_to_lv` / `atp_register_all_to_scheduler` / `atp_auto_solve` | TPTP 编码、Vampire/E/iProver 求解、TSTP 证明转 ProofNavigator |
| SAT 编码 | `sat_encoding.h` | `constraint_graph_to_sat` / `relation_model_to_sat` / `sat_solve_and_decode` / `sat_solve_incremental` / `sat_get_unsat_core` / `sat_encoding_export_dimacs` / `sat_model_to_graph` | 关系模型