# 42 增强证明引擎（Enhanced Proof Engine）

## 模块概述

本文档描述 Lv-00 增强证明引擎。该引擎在基础证明系统（`proof.h`）之上提供可交互、可度量的证明编排能力：证明会话（`proof_session.h`）管理目标-步骤生命周期；评分（`proof_score.h`）将证明质量量化为分数与等级；优先级队列（`proof_priority.h`）驱动节点展开顺序；策略调度（`proof_step_strategy.h`）以 vtable 分发步骤验证与 Coq 导出；元验证器（`meta_verify.h`）对会话/证明执行 6 项质量检查；剪枝元证明（`meta_proof.h`）为 WFC 状态空间削减提供数学合法性证明；证明追踪树（`proof_trace.h`）与交互组件（`proof_widget.h`）提供过程可视与策略回传。

**覆盖头文件**：

- `proof_session.h` / `proof_session_internal.h` —— 证明会话与状态管理
- `proof_score.h` —— 证明评分与等级评定
- `proof_priority.h` —— 证明搜索优先级队列
- `proof_step_strategy.h` —— 步骤策略 vtable（验证 + Coq 导出）
- `meta_verify.h` —— Layer 8 元验证器（6 项质量检查）
- `meta_proof.h` —— 剪枝合法性元证明（L1/L2/L3 三层策略）
- `proof_trace.h` —— 证明追踪、证明树与步骤优化
- `proof_widget.h` —— 证明交互可视化组件
- `proof_compiler.h` —— 底层证明对象与证明跟踪（`lvProofObject`）

---

## 核心设计原则

1. **会话即契约**：一次证明尝试对应一个 `lvProofSession`，`proof_session_submit_step` 统一裁决步骤（`lvStepResult`：接受/拒绝/目标变化/得证/错误），任何中间状态可经 `proof_session_get_state_json` 快照并恢复。
2. **质量可量化**：评分函数 `lv_proof_score_evaluate` 返回连续分数，`lv_proof_score_grade` 映射为等级；分数同时作为优先级队列的调度键，实现"高质量先展开"的启发式。
3. **策略即数据**：`ProofStepStrategy` vtable 将步骤类型的 `switch` 分发替换为数据驱动分发；`rewrite_strategy_apply` 提供 Maude 风格组合子（顺序/回退/重复/规范化），策略树本身可序列化。
4. **元验证分层**：`lv_meta_verify_session` / `lv_meta_verify_proof` 执行结构完整性、类型一致性、完备性、可靠性、非平凡性、往返验证六项检查；剪枝元证明（`meta_prove_pruning`）按 L1 直接矛盾 → L2 传播矛盾 → L3 代数排除自动选择策略，保证被剪枝状态确实不含合法解。
5. **全程可追踪**：`ProofTrace` 记录规则应用序列，`lvProofTree` 维护证明树拓扑，`ProofOptimizer` 执行死步消除与步骤合并，`proof_widget_*` 将上述状态以 JSON 契约同步到 Web GUI。

---

## 关键数据结构

```c
/* —— 证明会话（proof_session_internal.h） —— */
typedef struct lvProofSession {
    char session_id[lv_SESSION_ID_MAX]; /* 会话 ID（64） */
    uint64_t created_at;                /* 创建时间戳 */
    char *target_proposition;           /* 目标命题 */
    lvProofState *state;                /* 证明状态（目标栈/假设/规则记录） */
    lvRuleEngine *engine;               /* 规则引擎 */
    int step_count;
    bool is_complete;
    lvSessionStatus status;             /* ACTIVE / COMPLETE / ABANDONED / ERROR */
} lvProofSession;

/* —— 优先级队列（proof_priority.h，不透明类型） —— */
typedef struct lvProofPriority lvProofPriority; /* (node_id, score) 优先队列 */

/* —— 元验证器与报告（meta_verify.h） —— */
typedef enum {
    lv_CHECK_STRUCTURAL = 0, /* 1 结构完整性 */
    lv_CHECK_TYPE,           /* 2 类型一致性 */
    lv_CHECK_COMPLETE,       /* 3 完备性 */
    lv_CHECK_SOUND,          /* 4 可靠性 */
    lv_CHECK_NONTRIVIAL,     /* 5 非平凡性 */
    lv_CHECK_ROUNDTRIP,      /* 6 往返验证 */
    lv_CHECK_COUNT
} lvVerifyCheck;

typedef struct {
    lvVerifyCheck check;
    int passed;              /* 1=通过 0=失败 -1=跳过 */
    char description[512];
} lvMetaVerifyResult;

typedef struct {
    int total_checks, passed_checks, failed_checks, skipped_checks;
    lvMetaVerifyResult results[lv_CHECK_COUNT];
    char summary[1024];
} lvVerifyReport;

typedef struct lvMetaVerifier {
    unsigned int check_mask; /* 启用的检查位掩码 */
    int strict_mode;         /* 严格模式 */
} lvMetaVerifier;

/* —— 步骤策略 vtable（proof_step_strategy.h） —— */
typedef struct {
    ProofStepValidateFn validate;     /* bool (*)(ProofStep *, const void *) */
    ProofStepExportCoqFn export_coq;  /* void (*)(const ProofStep *, FILE *) */
} ProofStepStrategy;

/* —— 剪枝元证明上下文（meta_proof.h） —— */
typedef struct MetaProofContext {
    ConstraintGraph *graph;
    PropagationContext *prop_ctx;   /* L2 传播矛盾需要 */
    EquivClassManager *equiv_mgr;
    ProofNavigator *navigator;
    PruningRecord *record;
    int max_propagation_steps;
    int timeout_ms;                 /* 单次证明超时 */
    bool enable_l1, enable_l2, enable_l3;
    int64_t l1_proofs, l2_proofs, l3_proofs, inconclusive_count;
    StreamContext *stream_ctx;
} MetaProofContext;

/* —— 证明树（proof_trace.h） —— */
typedef struct lvProofTree {
    char name[256];
    char strategy[128];
    char *theorem_name;
    char *proof_strategy;
    lvProofTreeNode *root;
    lvDArray all_nodes;   /* 指针数组 */
    int next_id;
    int total_steps;
    int max_depth;
    bool is_complete;
} lvProofTree;
```

---

## 主要接口

### 证明会话（proof_session.h / proof_session_internal.h）

| 接口 | 说明 |
| --- | --- |
| `proof_session_create(target, engine)` / `proof_session_create_with_id` | 创建会话（自动/指定 ID） |
| `proof_session_destroy` / `reset` / `abandon` | 销毁、重置、放弃会话 |
| `proof_session_submit_step(session, step, &result)` | 提交步骤，返回 `lvStepResult` 裁决 |
| `proof_session_get_id` / `get_target` / `get_step_count` / `is_complete` / `get_status` | 会话查询 |
| `proof_session_get_state_json` | 会话状态 JSON 快照 |
| `proof_state_create` / `pop_goal` / `record_rule` / `add_hypothesis` / `current_goal` | 目标栈与假设管理 |
| `session_status_to_string` / `step_result_to_string` | 枚举字符串化 |

### 评分与优先级（proof_score.h / proof_priority.h）

| 接口 | 说明 |
| --- | --- |
| `lv_proof_score_evaluate(proof_id, engine)` | 返回 `double` 证明质量分数 |
| `lv_proof_score_grade(score)` | 分数 → 等级字符串 |
| `lv_proof_priority_create(capacity)` / `destroy` | 优先级队列生命周期 |
| `lv_proof_priority_push(pq, node_id, score)` / `pop(&node_id, &score)` / `empty` | 入队/出队/判空（按分数调度） |

### 策略调度（proof_step_strategy.h）

| 接口 | 说明 |
| --- | --- |
| `proof_step_get_strategy(type)` | 按步骤类型获取 `ProofStepStrategy` 实例（无效类型返回 NULL） |
| `ProofStepValidateFn validate` | 步骤验证回调（回调可 NULL） |
| `ProofStepExportCoqFn export_coq` | Coq 导出回调（回调可 NULL） |
| `rewrite_strategy_apply(graph, strategy, rules, n, &out, &steps)` | Maude 风格策略树执行（策略调度下游） |

### 元证明验证（meta_verify.h）

| 接口 | 说明 |
| --- | --- |
| `lv_meta_verifier_create` / `destroy` | 元验证器生命周期 |
| `lv_meta_verifier_enable_check` / `disable_check` / `set_strict` | 检查项位掩码与严格模式 |
| `lv_meta_verify_session(verifier, session)` | 对 `lvSession` 执行 6 项检查 |
| `lv_meta_verify_proof(verifier, proof)` | 对证明对象执行 6 项检查 |
| `lv_verify_report_passed` / `summary` / `result` | 报告查询 |

### 剪枝元证明（meta_proof.h）

| 接口 | 说明 |
| --- | --- |
| `meta_proof_context_create(graph, prop_ctx)` / `destroy` | 元证明上下文（prop_ctx 可为 NULL） |
| `meta_prove_direct_contradiction` / `propagation_contradiction` / `algebraic_exclusion` | L1/L2/L3 单策略证明 |
| `meta_prove_pruning(ctx, node_id, candidate)` | 自动选择策略（L1→L2→L3） |
| `meta_prove_completeness(ctx)` | 聚合剪枝记录生成完备性报告 |
| `meta_proof_record_pruning` / `meta_proof_get_record` | 剪枝记录写入与只读访问 |
| `meta_proof_set_strategy_enabled` / `set_max_propagation_steps` / `set_timeout` | 策略与超时配置 |
| `meta_proof_set_navigator` / `set_equiv_manager` / `set_stream_context` | 关联外部组件 |
| `meta_proof_get_statistics` | L1/L2/L3 计数与无法确定计数 |

### 追踪与可视化（proof_trace.h / proof_widget.h）

| 接口 | 说明 |
| --- | --- |
| `lv_proof_trace_add_step(t, rule, state)` / `get_step_count` / `get_rule` / `is_complete` | 规则应用追踪 |
| `lv_proof_tree_create` / `add_step` / `mark_contradiction` / `export_text` | 证明树构建与文本导出 |
| `lv_proof_opt_create` / `add_step` / `dead_step_elimination` / `merge_steps` / `active_count` | 死步消除与步骤合并 |
| `proof_widget_init` / `register` / `update` / `set_layout_type` | 组件布局与注册 |
| `proof_widget_get_goal` / `get_hypotheses` | 目标与假设面板数据 |
| `proof_widget_suggest_tactic` / `apply_tactic` | 智能战术推荐与策略回传 |
| `proof_widget_get_step_highlights` / `get_search_tree` / `get_dependency_graph` | 步骤高亮、搜索树、依赖图 JSON |

---

## 工作流程

1. **会话建立**：`proof_session_create` 创建会话并绑定 `lvRuleEngine` 与目标命题，`proof_state_create` 初始化目标栈。
2. **步骤提交与评分**：用户/引擎提交步骤 → `proof_session_submit_step` 裁决（ACCEPTED/REJECTED/GOAL_CHANGED/PROVED/ERROR）；对每个候选结果调用 `lv_proof_score_evaluate` 评分，`lv_proof_score_grade` 给出等级。
3. **优先级调度**：候选节点以 (node_id, score) 压入 `lvProofPriority`，`pop` 按分数取出最高优先节点展开，形成最佳优先搜索。
4. **策略分发**：`proof_step_get_strategy(type)` 取得步骤类型对应的 vtable，先 `validate` 后执行；复杂证明流程可用 `rewrite_strategy_apply` 组合子编排（顺序/回退/重复/规范化）。
5. **元验证**：会话收敛后 `lv_meta_verify_session` 执行 6 项检查生成 `lvVerifyReport`，严格模式下任一检查失败即整体不通过。
6. **剪枝元证明**：WFC 求解路径上的每次状态移除经 `meta_prove_pruning`（L1→L2→L3）证明合法性并写入 `PruningRecord`，`meta_prove_completeness` 汇总为完备性报告与总体信任颜色。
7. **追踪与可视化**：`lv_proof_trace_add_step` 记录规则序列，`lvProofTree` 组织证明树，`ProofOptimizer` 压缩冗余步骤；`proof_widget_*` 将目标/假设/高亮/依赖图以 JSON 同步至 Web GUI，`apply_tactic` 支持交互回传。

---

## 模块关系

| 编号文档 | 关系说明 |
| --- | --- |
| [09_proof.md](09_proof.md) | 基础命题与证明系统（`Proposition`、`ProofStep`、`ProofNavigator`），本引擎的全部证明语义源于此 |
| [22_proof_export_trace_widget.md](22_proof_export_trace_widget.md) | 定义导出/追踪/交互组件层；`proof_trace.h` 与 `proof_widget.h` 为其核心实现 |
| [34_meta_proof_cache.md](34_meta_proof_cache.md) | 元证明与推理缓存总文档；`meta_proof.h` 剪枝元证明为本文档元证明子系统 |
| [24_constraint_propagation.md](24_constraint_propagation.md) | 约束传播与等价类系统；`MetaProofContext.prop_ctx` / `equiv_mgr` 关联传播引擎 |
| [11_wfc_paradigm.md](11_wfc_paradigm.md) | WFC 范式；剪枝元证明（L1/L2/L3）是其数学严格化基础 |
| [14_solver_backends.md](14_solver_backends.md) | 多后端求解体系；`proof_priority` 与 `rewrite_search_backward` 协同完成证明搜索 |
| [25_engine_scheduler.md](25_engine_scheduler.md) | 引擎核心与调度系统；本引擎的会话/步骤调度可挂接为调度策略之一 |
| [31_stream_interop.md](31_stream_interop.md) | 流处理与互操作系统；`MetaProofContext.stream_ctx` 与 `interop` 流式事件对接 |
| [41_axiom_rewrite_export.md](41_axiom_rewrite_export.md) | 公理包重写与导出；`proof_step_get_strategy` 的 `export_coq` 回调经其互操作通道输出 |

---

## 版本历史

| 版本 | 日期 | 变更 |
| --- | --- | --- |
| 1.0 | 2026-08-10 | 初稿：证明会话、评分、优先级、策略调度、元验证与可视化全流程 |
