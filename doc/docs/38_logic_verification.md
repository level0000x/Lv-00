# 38. 逻辑验证：命题验证器、三值逻辑与 SAT 编码

## 模块概述

本文档描述 Lv-00 几何元语言系统的逻辑验证层。该层覆盖从命题公式构造与构造性验证（BHK 解释）、Kleene 强三值逻辑真值传播，到约束图/关系模型的 SAT 编码求解、量词消去与模态算子（Kripke 语义）验证的完整链条，是证明系统正确性判定的计算基础。

**覆盖头文件**：
- `prop_verifier.h` —— 命题验证器（构造性/BHK 验证、真值表、冒烟测试、不可构成性分析）
- `prop_formula_ops.h` —— 命题公式类型 VTable（相等/哈希/后代判定）
- `logic_check.h` —— 字符串公式的重言式/矛盾式/等价性检查
- `three_valued_logic.h` —— Kleene 强三值逻辑（真/伪/未知）
- `sat_encoding.h` —— SAT 文字/子句/编码上下文与求解解码
- `relation_model.h` —— 关系模型层（Alloy 风格"关系即一切"）
- `quantifier.h` —— 量词系统（∀/∃/∃!）
- `modal_operators.h` —— 模态逻辑扩展（□/◇，Kripke 框架）

---

## 核心设计原则

1. **构造性/BHK 语义**：命题验证遵循构造主义解释——`prop_verifier.h` 默认启用爆炸原理（`enable_ex_falso`），并支持直觉主义模式（`use_intuitionistic`，禁止反证法）。
2. **验证可配置与可终止**：`VerifierConfig` 提供 `timeout_ms` 超时与 `max_steps` 步数上限，验证器必须可判定地返回"可证/失败/超时/证伪"。
3. **结果枚举体系隔离**：本层 `VerifyResult`（VERIFY_PROVEN/FAILED/TIMEOUT/DISPROVEN/...）与 `proof.h` 的 `LvProofVerifyResult` 是两套独立枚举，避免 include 顺序导致语义漂移；`TrustColor` 信任着色随 `VerifyDetail` 输出。
4. **三值逻辑承接不确定**：未解决子目标标注 `lv_UNKNOWN`，Kleene 强真值表保证 UNKNOWN 不会"证出"矛盾；提供保守（`to_bool_conservative`）与乐观（`to_bool_optimistic`）两种布尔投影。
5. **小范围推理（small scope）**：几何约束与关系公式编码为 CNF 交由 SAT 求解，在有限范围内判定可满足性并解码回约束图/关系实例；支持增量求解与 unsat core 提取。
6. **量词优先有限域消去**：`quantifier.h` 对有限域执行枚举消去（∀→∧、∃→∨、∃!→恰好一个），对无限域返回 `lv_UNKNOWN`，并支持 ∀E/∀I/∃I/∃E 构造性规则。
7. **Kripke 模态语义**：`modal_operators.h` 基于框架 `<W, R>` 定义 □（所有可达世界成立）与 ◇（某可达世界成立），支持对偶转换 ◇A ↔ ¬□¬A，可达关系按几何变换类型分层（恒等/刚体/相似/仿射/射影/约束继承）。

---

## 关键数据结构

```c
/* 命题公式（prop_verifier.h） */
typedef enum {
    PROP_ATOM = 0, PROP_CONJUNCTION, PROP_DISJUNCTION,
    PROP_IMPLICATION, PROP_NEGATION, PROP_BOTTOM, PROP_TRUE
} PropFormulaType;

typedef struct PropFormula {
    PropFormulaType type;
    union {
        struct { char name[64]; } atom;                    /* PROP_ATOM */
        struct { struct PropFormula *left; struct PropFormula *right; } binary;
        struct { struct PropFormula *operand; } unary;     /* PROP_NEGATION */
    } data;
} PropFormula;

/* 验证器配置与结果（prop_verifier.h） */
typedef struct {
    bool enable_ex_falso;    /* 启用爆炸原理 */
    bool use_intuitionistic; /* 直觉主义模式（禁止反证法） */
    int timeout_ms;          /* 超时毫秒，0 = 无限制 */
    int max_steps;           /* 最大证明步数，0 = 默认 */
} VerifierConfig;
#define VERIFIER_CONFIG_DEFAULT {true, false, 5000, 10000}

typedef struct {
    VerifyResult result;               /* 见 VERIFY_* 枚举 */
    char error_message[256];
    char construction_summary[256];    /* 构造摘要 */
    int steps_used;  int max_steps;
    bool proven;
    TrustColor trust_color;            /* 信任着色 */
} VerifyDetail;

/* 三值真值（three_valued_logic.h） */
typedef enum { lv_TRUE = 0, lv_FALSE = 1, lv_UNKNOWN = 2 } lvTruthValue;

/* SAT 编码上下文（sat_encoding.h，字段采用 lvDArray 变长表） */
typedef struct SatEncoding {
    lvDArray var_map;          /* lvDArray of SatVarEntry（arity + atom_ids[]） */
    int next_var_id;
    int **clauses;             /* CNF 子句 */
    int *clause_sizes;
    int clause_count;  int clause_capacity;
    int total_vars;    int total_clauses;
    double encode_time_ms;     /* 编码耗时统计 */
    ConstraintGraph *graph;    /* 关联约束图（可为 NULL） */
    const RelModel *rel_model; /* 关联关系模型（可为 NULL） */
} SatEncoding;

/* 量化表达式（quantifier.h） */
struct lvQuantifiedExpr {
    int id;
    lvQuantifier quantifier;   /* lv_FORALL / lv_EXISTS / lv_EXISTS_UNIQUE */
    char *variable_name;
    int variable_node_id;
    lvDomain *domain;          /* 量化域 */
    struct Proposition *body_proposition;
    int *instantiated_ids;     /* 已实例化变量列表 */
    int instantiated_count;
    int instantiated_capacity;
    lvTruthValue cached_truth; /* 真值缓存 */
    bool truth_cache_valid;
};

/* 模态框架与公式（modal_operators.h） */
struct lvModalFrame {
    lvDArray worlds;               /* lvDArray of lvModalWorld* */
    int current_world_id;
    lvReachabilityType **reach_matrix; /* reachability[w_from][w_to] */
    int reach_dimension;
};

struct lvModalFormula {
    lvModalOperator op;               /* □ / ◇ */
    struct Proposition *inner_prop;   /* 内层命题 */
    struct lvModalFormula *sub;       /* 嵌套子公式（如 □◇P 的 ◇P） */
};
```

---

## 主要接口

### 命题公式与验证（prop_verifier.h / prop_formula_ops.h / logic_check.h）

| 接口 | 签名 | 说明 |
|------|------|------|
| 公式构造 | `prop_formula_create_atom/conjunction/disjunction/implication/negation/bottom/true/copy/destroy/to_string/to_latex` | 7 种节点构造 + 复制/销毁/输出 |
| 核心验证 | `VerifyDetail prop_verifier_verify(const PropFormula **premises, int premise_count, const PropFormula *goal, const VerifierConfig *config)` | 前提列表 → 目标的可证性判定 |
| 冒烟测试 | `prop_verifier_builtin_smoke_test_count` / `run_builtin_smoke_tests` / `run_smoke_tests` | 内置/自定义 `SmokeTest` 批量回归 |
| BHK 验证 | `BHKVerificationResult prop_verifier_bhk_verify(...)`；`prop_verifier_free_bhk_result` | 构造性解释 + 几何映射 + 缺失构造统计 |
| 不可构成性 | `InconstructibilityAnalysis prop_verifier_analyze_inconstructibility(...)`；`prop_verifier_free_analysis` | 分析目标为何不可构造，列出失败子目标 |
| 辅助判定 | `prop_verifier_apply_trust_colors` / `prop_verifier_check_equivalence` / `prop_verifier_check_tautology` | 信任着色、等价、重言判定 |
| 类型 VTable | `const PropFormulaOps *prop_formula_get_ops(PropFormulaType type)` | `equal/hash/is_descendant` 三回调 |
| 字符串检查 | `int lv_logic_check_tautology(const char *f)` / `lv_logic_check_contradiction(const char *f)` / `lv_logic_check_equivalence(const char *a, const char *b)` | 解析式快捷判定（返回 1/0/-1 语义） |

### 三值逻辑（three_valued_logic.h）

| 接口 | 签名 | 说明 |
|------|------|------|
| 真值表 | `lv_tvl_and / or / not / implies / equiv(lvTruthValue, lvTruthValue)` | Kleene 强三值运算 |
| 批量归约 | `lv_tvl_and_all / or_all(const lvTruthValue *, int)` | 短路归约（遇 FALSE/TRUE 提前返回） |
| 判定/投影 | `lv_tvl_is_known / is_true / is_false`；`lv_tvl_to_bool_conservative / optimistic`；`lv_tvl_from_bool` | UNKNOWN 处理的保守/乐观策略 |
| 字符串 | `lv_tvl_to_string` / `lv_tvl_to_string_zh` | "TRUE/FALSE/UNKNOWN" 或 "真/伪/未知" |

### SAT 编码与关系模型（sat_encoding.h / relation_model.h）

| 接口 | 签名 | 说明 |
|------|------|------|
| 编码上下文 | `sat_encoding_create(initial_var_capacity, initial_clause_capacity)` / `sat_encoding_destroy` | 生命周期管理 |
| 变量注册 | `sat_encoding_register_var(enc, arity, atom_ids)` / `sat_encoding_lookup_var` | 元组 ↔ SAT 变量（≥1，失败 -1） |
| 子句 | `sat_encoding_add_clause(enc, literals, count)` / `sat_encoding_add_assumption(enc, literal)` | CNF 子句与假设 |
| 几何谓词编码 | `sat_encode_collinearity / parallelism / perpendicularity / distance_eq / angle_eq / containment / constraint` | 共线/平行/垂直/距离/角/包含/通用约束 |
| 模型编码 | `SatResult constraint_graph_to_sat(const ConstraintGraph *, SatEncoding *)`；`SatResult relation_model_to_sat(const RelModel *, const SmallScopeConfig *, SatEncoding *)` | 两种源模型编码 |
| 求解解码 | `sat_solve_and_decode`；`sat_solve_incremental(enc, literals, count, out)`；`sat_model_to_graph`；`sat_model_to_instance`；`sat_model_destroy`；`relation_instance_destroy` | 全量/增量求解与解码 |
| 分析导出 | `int *sat_get_unsat_core(enc, out_count)`；`bool sat_encoding_export_dimacs(enc, filepath)`；`sat_encoding_get_stats(enc, out_vars, out_clauses)` | unsat core、DIMACS 导出、统计 |
| 关系运算 | `rel_union / intersection / difference / join / product / transpose / transitive_closure / reflexive_transitive_closure` | 8 种公开关系运算符（共 13 种 RelOp） |
| 模型构建 | `relation_model_from_graph(graph)` / `relation_model_destroy` / `add_fact` / `add_assertion`；`relation_check_satisfiability` / `relation_find_instance` / `relation_evaluate_expr` / `relation_evaluate_formula`；`relation_model_export_alloy` / `relation_instance_export_xml` | Alloy 风格模型全生命周期 |

### 量词与模态（quantifier.h / modal_operators.h）

| 接口 | 签名 | 说明 |
|------|------|------|
| 域管理 | `lv_quant_domain_create / _create_finite / add_element / add_elements / contains / size / destroy` | 命名域/有限枚举域/子图域 |
| 量化表达式 | `lv_quant_expr_create(id, quantifier, var_name, node_id, domain, body_prop)` / `lv_quant_expr_destroy` / `lv_quant_expr_evaluate` | 三值评估（无限域返回 UNKNOWN） |
| 构造性规则 | `lv_quantifier_instantiate`（∀E）/ `lv_quantifier_generalize`（∀I）/ `lv_quant_exists_introduce`（∃I）/ `lv_quant_exists_eliminate`（∃E） | 实例化与泛化 |
| 有限域消去 | `lv_quant_eliminate_forall_finite / exists_finite / exists_unique_finite` | 展开为 ∧ / ∨ / 恰好一个 |
| 判定辅助 | `lv_quant_is_eliminable` / `lv_quant_count_satisfying` / `lv_quant_result_destroy` / `lv_quant_to_string` / `lv_quant_result_to_string` | 可消去性、满足计数、字符串化 |
| 世界/框架 | `lv_modal_world_create / destroy / assert / holds`；`lv_modal_frame_create / destroy / add_world / set_reachability / is_reachable / get_reachable_worlds` | Kripke 框架 `<W, R>` 管理 |
| 公式与评估 | `lv_modal_formula_create / create_nested / destroy`；`int lv_modal_evaluate(frame, formula, world_id, result)`；`lv_modal_check_validity` | □/◇ 语义评估与有效性检查 |
| 对偶与几何辅助 | `lv_modal_possible_to_necessary_not` / `lv_modal_necessary_to_not_possible`；`lv_modal_frame_create_geometric_default`；`lv_modal_assert_point_must_on_line` / `_can_on_line`；`lv_modal_eval_result_destroy`；`lv_modal_op_to_string` / `lv_reachability_type_to_string` / `lv_modal_formula_to_string` | 模态对偶、几何默认框架、字符串化 |

---

## 工作流程

**命题验证**：构造前提与目标 `PropFormula`（二叉树）→ 组装 `VerifierConfig`（超时/步数/直觉主义开关）→ `prop_verifier_verify` 在 `timeout_ms`/`max_steps` 约束下搜索证明，输出 `VerifyDetail`（PROVEN/FAILED/TIMEOUT/DISPROVEN/ERROR + 构造摘要 + `TrustColor`）；可选 `prop_verifier_bhk_verify` 获取 BHK 解释与缺失构造清单，或 `prop_verifier_analyze_inconstructibility` 定位失败子目标。

**三值传播**：子目标未解决时以 `lv_UNKNOWN` 标注，沿公式树用 `lv_tvl_and/or/not/implies` 真值表传播；`lv_tvl_and_all/or_all` 对前提集合短路归约，最后按保守/乐观策略投影为布尔值驱动上层决策。

**SAT 编码-求解-解码**：`constraint_graph_to_sat`/`relation_model_to_sat` 将约束或关系模型（受 `SmallScopeConfig` 限制）编码为 CNF（几何谓词经 `sat_encode_*` 产生子句，元组经 `register_var` 映射变量）→ `sat_solve_and_decode` 求解并解码为 `SatModel` → `sat_model_to_graph`/`sat_model_to_instance` 还原；不可满足时可调用 `sat_solve_incremental` 追加假设或 `sat_get_unsat_core` 提取冲突核。

**量词验证**：`lv_quant_expr_evaluate` 先查真值缓存；有限域走 `lv_quant_eliminate_forall_finite`（∧）/`exists_finite`（∨）/`exists_unique_finite`（恰好一个）枚举展开；证明过程按需用 `instantiate`（∀E）、`generalize`（∀I）、`exists_introduce`（∃I）、`exists_eliminate`（∃E）推进；`lv_quant_count_satisfying` 统计满足元素数。

**模态评估**：构造几何世界（`lv_modal_world_create`）与框架（`lv_modal_frame_create_geometric_default` 默认刚体变换可达）→ 设置 `reach_matrix` 可达关系 → 对 `lvModalFormula`（支持嵌套）调用 `lv_modal_evaluate`：□ 检查全部可达世界、◇ 搜索目击世界（`witness_world_id`）；`lv_modal_check_validity` 遍历所有世界判有效式，对偶转换接口负责 □↔◇ 等价改写。

---

## 模块关系

| 相关模块 | 关系说明 |
|----------|----------|
| [25_engine_scheduler.md](25_engine_scheduler.md) | 验证任务经引擎调度器编排，验证结果反馈求解/重写流程 |
| [27_quantifier_logic.md](27_quantifier_logic.md) | 量词系统与关系模型的独立设计文档，与本文 SAT 编码/小范围推理互补 |
| [09_proof.md](09_proof.md) | `VerifyResult` 与 `LvProofVerifyResult` 枚举分离；`TrustColor` 信任着色贯穿验证与证明导出 |
| [32_runtime_monitoring.md](32_runtime_monitoring.md) | `VerifierConfig.timeout_ms` 与运行时防护协同，防止验证失控；性能会话可追踪验证耗时 |
| [34_meta_proof_cache.md](34_meta_proof_cache.md) | 验证结果/证明可缓存复用，`HashHistory` 或缓存表按公式哈希判重 |
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图是 `constraint_graph_to_sat` 与 `sat_model_to_graph` 的图源/解码目标 |
| [33_gappa_verification.md](33_gappa_verification.md) | Gappa 负责数值/区间侧验证，本文档负责逻辑结构侧验证，二者构成双重证明 |

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0.0 | 2026-08-10 | 初稿：整理 `prop_verifier.h`/`prop_formula_ops.h`/`logic_check.h`/`three_valued_logic.h`/`sat_encoding.h`/`relation_model.h`/`quantifier.h`/`modal_operators.h` 设计 |
