# 命题与证明系统 (Proposition & Proof System)

## 模块概述

命题与证明系统是 Lv-00 证明引擎的核心模块（`proof.h`），提供完整的几何证明框架：命题管理（`Proposition` 的创建/销毁/端口/模式/前置/后置条件设置）、合一验证（构造图与命题模式匹配，`proof_unify`）、证明导航器（`ProofNavigator`，步骤添加/前进后退/断点）、依赖链与信任颜色（`ProofDependency`）、爆炸原理与反证法作用域（`lvProofScopeId`）、自然语言证明输出（AlphaGeometry 风格）、多策略证明引擎（JGEX 风格）、不可构造性检测以及六梯队参考项目 API（Agda 洞填充、Idris 2 QTT、Isabelle Sledgehammer、HOL Light 微内核、F\* 精化类型）。配套 `prop_verifier.h` 提供命题公式（`PropFormula`）的构造与验证器（含爆炸原理开关、直觉主义模式、BHK 解释、冒烟测试），`prop_formula_ops.h` 以 VTable 方式提供公式运算（相等、哈希、后代判定），`proof_trace.h` 提供证明轨迹记录、证明树与优化器（死步消除、步骤合并）。

## 核心设计原则

1. **命题即模式**：`Proposition` 以 `ConstraintGraph *pattern` 保存几何模式（虚线框内约束骨架），输入/输出端口（`input_port_ids`/`output_port_ids`）与前置/后置条件（`precondition_region_ids`/`postcondition_constraint_ids`）共同构成可复用的命题模板。
2. **合一驱动验证**：证明以 `proof_unify`/`proof_unify_detailed` 将构造图与命题模式合一，返回 `UnifyStatus` 并给出不匹配诊断信息。
3. **信任颜色传播**：`ProofColor`（绿/蓝/黄/橙/琥珀/红）沿依赖链 `ProofDependency`（`DEP_SOURCE_DIRECT/LEMMA/ORACLE/EX_FALSO/NUMERIC`）传播；公理包升级后内容哈希变化自动降级颜色（`proof_validate_dependencies`）。
4. **爆炸原理受控**：`BottomDefinition.allow_explosion` 显式开关；反证法/爆炸推演限定在假设作用域（`lvProofScopeId`）内，作用域关闭即回收，避免局部矛盾污染全局上下文（`proof_has_global_proposition` 校验）。
5. **引用计数生命周期**：`Proposition` 经 `proposition_ref`/`proposition_unref` 管理，计数归零自动销毁；`ProofNavigator` 为 `Proof` 的别名类型。
6. **多策略并存**：`ProofMultiStrategy` 注册多种策略（直接构造、面积法、Groebner 基、向量法、全角法、演绎数据库、坐标法、λ-演算、HOL Light、Oracle、数值验证等），支持回退、流水线与竞争模式；经典引擎经 `proof_multi_strategy_set_legacy_engine` 桥接挂载。
7. **双枚举体系隔离**：`prop_verifier.h` 的 `VerifyResult` 与 `proof.h` 的 `LvProofVerifyResult` 为两套独立枚举，避免 include 顺序导致的语义漂移。
8. **微内核验证**：`proof_minimal_verify` 仅用 HOL Light 10 条基本规则（ASSUME/REFL/BETA_CONV/MK_COMB/ABS/TRANS/SUBST/INST_TYPE/INST/DISCH）验证单步推导。

## 关键数据结构

```c
/* 命题类型与证明状态颜色 */
typedef enum { PROPOSITION_TYPE_ATOMIC, PROPOSITION_TYPE_CONJUNCTION,
               PROPOSITION_TYPE_DISJUNCTION, PROPOSITION_TYPE_IMPLICATION,
               PROPOSITION_TYPE_NEGATION, PROPOSITION_TYPE_UNIVERSAL,
               PROPOSITION_TYPE_EXISTENTIAL, PROPOSITION_TYPE_BOTTOM } PropositionType;

typedef enum { PROOF_COLOR_GREEN, PROOF_COLOR_BLUE_UNEXPLORED,
               PROOF_COLOR_BLUE_RESOURCE, PROOF_COLOR_BLUE_OUT_OF_RANGE,
               PROOF_COLOR_GREEN_VERIFIED, PROOF_COLOR_YELLOW,
               PROOF_COLOR_ORANGE_ORACLE, PROOF_COLOR_ORANGE_EX_FALSO,
               PROOF_COLOR_AMBER, PROOF_COLOR_DARK_ORANGE,
               PROOF_COLOR_GREEN_COMPLETE, PROOF_COLOR_RED_CONFLICT } ProofColor;

/* 命题：模式图 + 端口 + 前置/后置条件 + 子命题 */
struct Proposition {
    int id;
    PropositionType type;
    ProofColor color;
    char *label;
    int ref_count;
    int *input_port_ids;  int input_count;
    int *output_port_ids; int output_count;
    ConstraintGraph *pattern;              /* 命题模式图 */
    int *precondition_region_ids;  int precondition_count;
    int *postcondition_constraint_ids; int postcondition_count;
    Proposition **sub_props; int sub_prop_count, sub_prop_capacity;
    TypeRegion *prop_type;
    char *name, *description;
    time_t created_at, last_modified;
};

/* 证明步骤（PROOF_STEP_ADD_NODE/.../UNIFY/EX_FALSO/ORACLE） */
struct ProofStep {
    int id;
    ProofStepType type;
    ProofColor color;
    int node_id, constraint_id, rule_id, func_block_id;
    int *merged_node_ids; int merged_count, retained_node_id;
    int *dependency_step_ids, *dependent_step_ids;
    bool is_breakpoint, is_completed;
    char *note;
    int parent_step_id;  /* 证明树，-1=根 */
    int depth;
    struct ProofStepExt *ext;  /* HOL Light 结论 */
    int64_t timestamp;
};

/* 证明导航器（typedef ProofNavigator Proof） */
struct ProofNavigator {
    ProofStep **steps; int step_count, step_capacity, current_step;
    Proposition *target_prop;
    ConstraintGraph *construction;
    ProofDependency *dep_tree;
    bool is_complete;
    ProofColor final_color;
    ProofState proof_state;
    int *breakpoint_indices; int breakpoint_count, breakpoint_capacity;
    lvDArray equivalences;              /* PropositionEquivalence */
    BottomDefinition *bottom_def;
    lvDArray lemma_view_step_ids, lemma_view_states;
    lvEngine *engine;
    char *strategy_note;                 /* LeanGeo 总体策略 */
    lvProofScopeId *scope_ids; bool *scope_active;
    Proposition **scope_assumptions; int scope_count, scope_capacity;
    lvProofScopeId next_scope_id;
};

/* 命题公式（prop_verifier.h）与验证器配置 */
typedef struct PropFormula {
    PropFormulaType type;   /* PROP_ATOM/CONJUNCTION/DISJUNCTION/
                               IMPLICATION/NEGATION/BOTTOM/TRUE */
    union {
        struct { char name[64]; } atom;
        struct { struct PropFormula *left; struct PropFormula *right; } binary;
        struct { struct PropFormula *operand; } unary;
    } data;
} PropFormula;

typedef struct {
    bool enable_ex_falso;    /* 爆炸原理开关 */
    bool use_intuitionistic; /* 直觉主义模式（禁反证法） */
    int timeout_ms;          /* 验证超时（毫秒） */
    int max_steps;           /* 最大证明步数 */
} VerifierConfig;
#define VERIFIER_CONFIG_DEFAULT {true, false, 5000, 10000}

/* ⊥ 的定义：是否允许爆炸（prop_verifier.h 的 VerifyResult 与之并存） */
typedef struct BottomDefinition {
    bool has_input_ports;
    int input_port_count;
    bool allow_explosion;
} BottomDefinition;
```

## 主要接口

| 分组 | 函数 | 说明 |
| --- | --- | --- |
| 命题管理 | `proposition_create/ref/unref/destroy`、`proposition_set_input_ports/set_output_ports/set_pattern/set_preconditions/set_postconditions`、`proposition_add_sub_proposition`、`proposition_contradicts` | 命题生命周期、端口/模式/条件配置与互斥检查 |
| 合一验证 | `proof_unify`、`proof_unify_detailed` | 构造图↔命题模式合一，返回 `UnifyStatus` 与不匹配信息 |
| 证明步骤 | `proof_step_create/destroy/add_dependency/set_breakpoint/set_note/get_ancestors` | 步骤创建、依赖添加、断点与祖先链 |
| 导航器 | `proof_navigator_create/destroy/add_step/next/prev/goto/next_breakpoint/current_step/compute_final_color/set_strategy_note/get_strategy_note` | 步骤序列导航、断点跳转、最终颜色 |
| 依赖链 | `proof_dependency_create/destroy/add_sub/compute_color`、`proof_validate_dependencies` | 依赖树构建与升级后颜色降级 |
| 爆炸/作用域 | `proof_create_ex_falso_block`、`proof_apply_ex_falso`、`proof_begin_assumption_scope/close_assumption_scope/scope_is_active`、`proof_has_global_proposition`、`proof_set_bottom_definition/get_bottom_definition` | 受控爆炸原理与反证法局部作用域 |
| 等价与实例化 | `proof_declare_proposition_equivalence`、`proof_find_equivalent_proposition`、`proof_has_type_variables`、`proof_instantiate_proposition` | 命题等价表、多态实例化 |
| 导出 | `proof_export_html/latex/coq/natural_language`、`proof_step_get_natural_language`、`proof_export_isar` | HTML/LaTeX/Coq/自然语言/Isar 导出 |
| 不可构造性 | `proof_check_unconstructibility`、`proof_attempt_unconstructibility`、`unconstruct_info_destroy` | 三等分角/倍立方等检测与多策略归约 |
| 搜索树 | `proof_search_tree_create/destroy`、`backtrack_node_create`、`proof_search_tree_add_child`、`backtrack_node_mark_backtrack`、`proof_search_tree_register_strategy/set_strategy/export_json/export_dot` | Newclid 风格回溯搜索树可视化 |
| 多策略引擎 | `proof_multi_strategy_create/set_legacy_engine/destroy/register/activate/get_active/evaluate_applicability/execute/try_all/pipeline/set_fallback_order/switch/get_stats` | JGEX 风格多证明方法共存 |
| 简化搜索 | `proof_search_with_strategy`、`proof_mcts_execute`、`proof_bfs_execute`、`proof_best_first_execute` | DFS/BFS/最佳优先/MCTS 一键搜索 |
| 六梯队参考 API | `proof_guided_fill`、`proof_mark_ghost`/`proof_check_ghost_conflicts`、`proof_sledgehammer_dispatch`、`proof_minimal_verify`、`proof_refinement_check` | Agda 洞填充 / Idris2 QTT / Sledgehammer / HOL Light / F\* 精化类型 |
| 公式操作 | `prop_formula_create_atom/conjunction/disjunction/implication/negation/bottom/true/copy/destroy/to_string/to_latex`、`prop_formula_get_ops` | 命题公式构造与按类型 VTable 操作 |
| 公式验证 | `prop_verifier_verify`、`prop_verifier_bhk_verify`、`prop_verifier_analyze_inconstructibility`、`prop_verifier_check_equivalence`、`prop_verifier_check_tautology`、`prop_verifier_apply_trust_colors`、`prop_verifier_run_smoke_tests` | 可证性验证、BHK 构造解释、不可构成性分析、重言式检查、冒烟测试 |
| 证明轨迹 | `lv_proof_trace_add_step/get_step_count/get_rule/is_complete`、`lv_proof_tree_create/add_step/mark_contradiction/export_text`、`lv_proof_opt_create/add_step/dead_step_elimination/merge_steps` | 轨迹记录、证明树、死步消除优化 |

## 工作流程

1. **命题建模**：`proposition_create(id, type)` 创建命题，`proposition_set_pattern` 绑定模式图，`proposition_set_input_ports`/`set_preconditions` 配置端口与条件；复合命题经 `proposition_add_sub_proposition` 递归组装。
2. **导航器初始化**：`proof_navigator_create(target, engine)` 建立证明会话，设置总体策略 `proof_navigator_set_strategy_note`。
3. **逐步构造**：按 `ProofStepType`（添加节点/约束/重写/规范化/合一/爆炸/Orcale）创建 `ProofStep` 并 `proof_navigator_add_step`；每步可设断点（`proof_step_set_breakpoint`）或交互式推进（`proof_interactive_step`）。
4. **合一验证**：`proof_unify_detailed` 将当前构造图与目标命题模式合一；失败时从 `out_mismatch_info` 获取诊断，可借助回溯搜索树换策略重试。
5. **反证/爆炸**：`proof_begin_assumption_scope` 开启局部假设，推导矛盾后经 `proof_apply_ex_falso`（或受控的 `PROOF_STEP_EX_FALSO` 步骤）导出目标，再 `proof_close_assumption_scope` 回收并核验 `proof_has_global_proposition`。
6. **信任评估**：`proof_navigator_compute_final_color` 计算最终颜色；`proof_validate_dependencies` 在公理升级后重验内容哈希并自动降级颜色。
7. **多策略收尾**：`proof_multi_strategy_try_all`/`pipeline` 竞争或流水线补全；`proof_export_natural_language`/`export_html` 输出人类可读证明。

## 模块关系

| 模块 | 关系说明 |
| --- | --- |
| [01_symbolic_coord.md](01_symbolic_coord.md) | `Proposition`/`GeomNode` 的符号坐标由符号坐标系统提供；`lv_TOLERATED_FLOAT` 标记的精度阈值仅用于规则参数化，不参与代数计算 |
| [02_constraint_graph.md](02_constraint_graph.md) | `Proposition.pattern` 与导航器的 `construction` 均为 `ConstraintGraph`；模式匹配依赖图的节点/约束结构 |
| [03_normalization.md](03_normalization.md) | 合一前的图规范化遍（`proof_unify` 的 `normalize_first` 参数）调用规范化引擎 |
| [04_solver.md](04_solver.md) | `constraint_solver_get_proposition` 桥接求解器与命题；多策略引擎的数值验证依赖求解器 |
| [06_unify.md](06_unify.md) | `proof_unify`/`proof_unify_detailed` 复用合一系统的 `UnifyStatus` 与匹配算法 |
| [07_func_block.md](07_func_block.md) | 函数块经 `PROOF_STEP_PACK_FUNCTION` 打包为证明步骤；引理视图折叠管理 `lemma_view_step_ids` |
| [08_type_system.md](08_type_system.md) | `prop_type`/`TypeRegion` 关联类型系统；`proof_has_type_variables`/`proof_instantiate_proposition` 做多态实例化 |
| [14_solver_backends.md](14_solver_backends.md) | SMT/数值后端供多策略引擎的 `PROOF_STRATEGY_ORACLE`、精化检查（F\*）与数值验证策略调用 |
| [15_geometry_advanced.md](15_geometry_advanced.md) | 几何高级特征为命题模式（相似、共圆等）提供底层判定支持 |

## 版本历史

- v3.4-academic：爆炸原理与反证法作用域整改，引入 `lvProofScopeId` 与 `proof_has_global_proposition`；`VerifyResult`/`LvProofVerifyResult` 双枚举体系拆分。
- v3.2.0：多证明方法并存引擎（JGEX 风格 `ProofMultiStrategy`）、不可构造性检测、回溯搜索树（Newclid 风格）、自然语言输出（AlphaGeometry 风格）与策略注释（LeanGeo 风格）。
- 2026-05-24：落地第六梯队参考项目 API（Agda/Idris2/Isabelle/HOL Light/F\*）。
