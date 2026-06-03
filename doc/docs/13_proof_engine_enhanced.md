# 增强证明引擎与证明辅助系统 (Enhanced Proof Engine and Proof Assistance System)

## 模块概述

增强证明引擎与证明辅助系统是对核心 `proof.h` 命题与证明系统的功能扩展，提供更强大的自动化证明搜索、交互式证明构造、证明质量评估和版本管理能力。本组模块涵盖以下七个头文件：

| 模块 | 头文件 | 职责 |
|------|--------|------|
| 增强证明引擎 | `proof_engine_enhanced.h` | 溯源树、反证法完善、策略调度、证明验证/优化/导出 |
| 证明会话 | `proof_session.h` | REPL 风格的交互式证明构造 |
| 证明评分 | `proof_score.h` | 多维度证明有效性评分与证明比较 |
| 推理优先级 | `proof_priority.h` | 四级优先级规则调度器 |
| 证明规则引擎 | `proof_rule_engine.h` | 加权优先最佳优先搜索与深度限制 |
| 证明版本控制 | `proof_version.h` | SHA-256 内容寻址与分支管理 |

### 与核心 proof.h 的关系

核心 `proof.h`（文档 09）提供了命题模式定义、合一检查、证明导航器和不可构造性证明的基础框架。本组模块在此基础上进行扩展：

```
proof.h（核心）
  ├── proof_engine_enhanced.h  ← 自动化证明搜索 + 溯源树 + 反证法完善
  ├── proof_session.h          ← 交互式 REPL 风格证明构造
  ├── proof_score.h            ← 证明质量评估与比较
  ├── proof_priority.h         ← 规则优先级调度
  ├── proof_rule_engine.h      ← 规则搜索引擎（被 proof_session.h 使用）
  └── proof_version.h          ← 证明版本控制
```

依赖关系：`proof_engine_enhanced.h` 依赖 `proof.h`、`constraint_graph.h`、`axiom_rule_engine.h`；`proof_session.h` 依赖 `proof_rule_engine.h`；`proof_score.h` 依赖 `proof.h`。

## 核心设计原则

1. **策略多样性**：支持 10 种证明策略的自动调度与手动选择
2. **可溯源性**：完整的逻辑溯源树记录证明的每一步依赖链
3. **可评估性**：多维度评分系统量化证明质量
4. **可交互性**：REPL 风格会话支持逐步证明构造
5. **可持久化**：SHA-256 内容寻址的证明版本控制

---

## 1. proof_engine_enhanced.h：增强证明引擎

`proof_engine_enhanced.h` 提供增强的证明引擎功能，包括逻辑溯源树、反证法完善、策略调度、证明验证、证明优化和多格式导出。

### 1.1 配置常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `LV00_PROOF_MAX_DEPTH` | 100 | 最大证明深度 |
| `LV00_PROOF_MAX_BRANCHES` | 64 | 最大分支数 |
| `LV00_PROOF_MAX_STRATEGIES` | 16 | 最大策略数 |
| `LV00_TRACE_TREE_MAX_DEPTH` | 50 | 溯源树最大深度 |

### 1.2 枚举类型定义

**溯源节点类型（8 种）**：

```c
typedef enum {
    TRACE_NODE_AXIOM,           /* 公理 */
    TRACE_NODE_DEFINITION,      /* 定义 */
    TRACE_NODE_THEOREM,         /* 定理 */
    TRACE_NODE_LEMMA,           /* 引理 */
    TRACE_NODE_HYPOTHESIS,      /* 假设 */
    TRACE_NODE_DERIVATION,      /* 推导 */
    TRACE_NODE_CONTRADICTION,   /* 矛盾 */
    TRACE_NODE_GOAL             /* 目标 */
} Lv00TraceNodeType;
```

**溯源节点状态**：

```c
typedef enum {
    TRACE_STATUS_UNEXPLORED,    /* 未探索 */
    TRACE_STATUS_EXPLORING,     /* 探索中 */
    TRACE_STATUS_PROVED,        /* 已证明 */
    TRACE_STATUS_DISPROVED,     /* 已证伪 */
    TRACE_STATUS_BLOCKED        /* 阻塞 */
} Lv00TraceNodeStatus;
```

**矛盾类型（6 种）**：

```c
typedef enum {
    CONTRADICTION_TYPE_P_AND_NOT_P,     /* P ∧ ¬P */
    CONTRADICTION_TYPE_FALSE_DERIVED,   /* 推导出假 */
    CONTRADICTION_TYPE_CYCLE,           /* 循环依赖 */
    CONTRADICTION_TYPE_TYPE_MISMATCH,   /* 类型不匹配 */
    CONTRADICTION_TYPE_ARITHMETIC,      /* 算术矛盾 */
    CONTRADICTION_TYPE_GEOMETRIC        /* 几何矛盾 */
} Lv00ContradictionType;
```

**证明策略类型（10 种）**：

```c
typedef enum {
    STRATEGY_DIRECT,            /* 直接证明 */
    STRATEGY_CONTRADICTION,     /* 反证法 */
    STRATEGY_CONTRAPOSITIVE,    /* 逆否证明 */
    STRATEGY_INDUCTION,         /* 数学归纳法 */
    STRATEGY_CASES,             /* 分情况讨论 */
    STRATEGY_CONSTRUCTION,      /* 构造性证明 */
    STRATEGY_UNFOLDING,         /* 定义展开 */
    STRATEGY_BACKWARD,          /* 逆向推理 */
    STRATEGY_FORWARD,           /* 正向推理 */
    STRATEGY_HYBRID             /* 混合策略 */
} Lv00StrategyType;
```

**策略状态**：

```c
typedef enum {
    STRATEGY_STATUS_PENDING,    /* 待执行 */
    STRATEGY_STATUS_RUNNING,    /* 执行中 */
    STRATEGY_STATUS_SUCCESS,    /* 成功 */
    STRATEGY_STATUS_FAILED,     /* 失败 */
    STRATEGY_STATUS_TIMEOUT     /* 超时 */
} Lv00StrategyStatus;
```

**验证结果**：

```c
typedef enum {
    LV00_VERIFY_VALID,          /* 有效 */
    LV00_VERIFY_INVALID,        /* 无效 */
    LV00_VERIFY_INCOMPLETE,     /* 不完整 */
    LV00_VERIFY_ERROR           /* 验证错误 */
} Lv00VerifyResult;
```

### 1.3 核心数据结构

**溯源树节点**：

```c
struct Lv00ProofTraceNode {
    /* 基本信息 */
    uint32_t id;                        /* 节点 ID */
    Lv00TraceNodeType type;             /* 节点类型 */
    Lv00TraceNodeStatus status;         /* 节点状态 */
    char label[256];                    /* 节点标签 */
    char description[512];              /* 详细描述 */

    /* 证明内容 */
    Proposition *proposition;           /* 关联命题 */
    ProofStep *step;                    /* 关联证明步骤 */
    Lv00Rule *rule;                     /* 使用的规则 */

    /* 信任颜色 */
    TrustColor trust_color;             /* 信任颜色 */

    /* 树结构 */
    Lv00ProofTraceNode *parent;         /* 父节点 */
    Lv00ProofTraceNode **children;      /* 子节点数组 */
    uint32_t child_count;               /* 子节点数量 */
    uint32_t child_capacity;            /* 子节点容量 */

    /* 依赖关系 */
    uint32_t *dependency_ids;           /* 依赖节点 ID */
    uint32_t dependency_count;          /* 依赖数量 */

    /* 元数据 */
    int depth;                          /* 树深度 */
    int64_t create_time_ns;             /* 创建时间 */
    int64_t complete_time_ns;           /* 完成时间 */
    double elapsed_ms;                  /* 耗时（毫秒） */
};
```

**溯源树**：

```c
struct Lv00ProofTraceTree {
    Lv00ProofTraceNode *root;           /* 根节点 */
    Lv00ProofTraceNode **all_nodes;     /* 所有节点（用于遍历） */
    uint32_t node_count;                /* 节点总数 */
    uint32_t node_capacity;             /* 节点容量 */

    /* 统计信息 */
    uint32_t proved_count;              /* 已证明节点数 */
    uint32_t disproved_count;           /* 已证伪节点数 */
    uint32_t max_depth;                 /* 最大深度 */

    /* 状态 */
    bool is_complete;                   /* 是否完成 */
    TrustColor final_color;             /* 最终信任颜色 */
};
```

**反证法路径**：

```c
struct Lv00ContradictionPath {
    Lv00ContradictionPathNode *nodes;   /* 节点数组 */
    uint32_t node_count;                /* 节点数量 */
    uint32_t node_capacity;             /* 节点容量 */
    Lv00ContradictionType type;         /* 矛盾类型 */
    char contradiction_desc[512];       /* 矛盾描述 */
    Lv00ProofTraceTree *trace_tree;     /* 完整溯源树 */
    bool is_valid;                      /* 是否有效 */
};
```

**证明策略**：

```c
struct Lv00ProofStrategy {
    Lv00StrategyType type;              /* 策略类型 */
    char name[64];                      /* 策略名称 */
    char description[256];              /* 策略描述 */
    Lv00StrategyStatus status;          /* 策略状态 */
    double priority;                    /* 优先级 */

    /* 执行信息 */
    int64_t start_time_ns;              /* 开始时间 */
    int64_t end_time_ns;                /* 结束时间 */
    double elapsed_ms;                  /* 耗时 */

    /* 结果 */
    Lv00ProofTraceTree *trace_tree;     /* 生成的溯源树 */
    uint32_t step_count;                /* 步骤数 */
    char error_message[512];            /* 错误消息 */

    /* 适用性检查函数指针 */
    bool (*is_applicable)(const Proposition *, const ConstraintGraph *);
    bool (*execute)(Lv00ProofEngine *, const Proposition *);
};
```

**证明引擎**：

```c
struct Lv00ProofEngine {
    Lv00ProofEngineConfig config;       /* 引擎配置 */
    Lv00RuleLibrary *rule_library;      /* 规则库 */
    Lv00ProofStrategy strategies[LV00_PROOF_MAX_STRATEGIES]; /* 策略数组 */
    uint32_t strategy_count;            /* 已注册策略数 */
    ConstraintGraph *graph;             /* 当前约束图 */
    ProofNavigator *navigator;          /* 证明导航器 */
    Lv00ProofTraceTree *current_trace;  /* 当前溯源树 */

    /* 统计 */
    uint64_t total_proofs;              /* 总证明次数 */
    uint64_t success_proofs;            /* 成功次数 */
    double avg_proof_time_ms;           /* 平均证明时间 */

    void *proof_cache;                  /* 证明缓存 */
};
```

**证明引擎配置**：

```c
typedef struct {
    uint32_t max_depth;                 /* 最大证明深度 */
    uint32_t max_branches;              /* 最大分支数 */
    uint32_t timeout_ms;                /* 超时时间（毫秒） */
    bool enable_parallel;               /* 启用并行证明 */
    bool enable_cache;                  /* 启用结果缓存 */
    bool verify_proofs;                 /* 验证证明 */
    bool optimize_proofs;               /* 优化证明 */
} Lv00ProofEngineConfig;
```

### 1.4 主要 API

#### 溯源树操作（7 个）

| 函数 | 说明 |
|------|------|
| `lv00_trace_tree_create(root_prop)` | 创建溯源树 |
| `lv00_trace_tree_destroy(tree)` | 销毁溯源树 |
| `lv00_trace_node_create(type, label)` | 创建溯源节点 |
| `lv00_trace_node_destroy(node)` | 销毁溯源节点 |
| `lv00_trace_node_add_child(parent, child)` | 添加子节点 |
| `lv00_trace_node_set_status(node, status)` | 设置节点状态 |
| `lv00_trace_node_compute_color(node)` | 计算节点信任颜色 |

#### 溯源树查询与导出（3 个）

| 函数 | 说明 |
|------|------|
| `lv00_trace_tree_find_path(tree, from_id, to_id, out_path, max_length)` | 查找两节点间的路径 |
| `lv00_trace_tree_export_dot(tree, path)` | 导出为 DOT 格式（Graphviz） |
| `lv00_trace_tree_to_json(tree)` | 导出为 JSON 字符串 |

#### 反证法（5 个）

| 函数 | 说明 |
|------|------|
| `lv00_engine_proof_by_contradiction(engine, goal, max_steps, out_path)` | 执行反证法证明 |
| `lv00_contradiction_path_create()` | 创建矛盾路径 |
| `lv00_contradiction_path_destroy(path)` | 销毁矛盾路径 |
| `lv00_contradiction_path_add_node(path, statement, justification, is_assumption)` | 添加节点到矛盾路径 |
| `lv00_detect_contradiction(graph, nav, out_type, out_desc)` | 检测矛盾 |
| `lv00_contradiction_path_validate(path)` | 验证反证法证明有效性 |

#### 证明引擎（6 个）

| 函数 | 说明 |
|------|------|
| `lv00_proof_engine_create(config)` | 创建证明引擎 |
| `lv00_proof_engine_destroy(engine)` | 销毁证明引擎 |
| `lv00_proof_engine_set_rule_library(engine, library)` | 设置规则库 |
| `lv00_proof_engine_register_strategy(engine, strategy)` | 注册证明策略 |
| `lv00_proof_engine_prove(engine, goal, graph, out_trace)` | 执行证明 |
| `lv00_proof_engine_auto_prove(engine, goal, graph, out_trace, out_strategy)` | 自动选择策略并证明 |
| `lv00_proof_engine_prove_with_strategy(engine, goal, graph, strategy_type, out_trace)` | 使用指定策略证明 |
| `lv00_proof_engine_get_stats(engine, out_total, out_success, out_avg_time)` | 获取引擎统计信息 |

#### 证明验证（2 个）

| 函数 | 说明 |
|------|------|
| `lv00_verify_proof(trace, out_error)` | 验证完整证明 |
| `lv00_verify_proof_step(step, graph, out_error)` | 验证单个证明步骤 |

#### 证明优化（3 个）

| 函数 | 说明 |
|------|------|
| `lv00_optimize_proof(trace, out_optimized)` | 优化证明 |
| `lv00_compute_proof_complexity(trace)` | 计算证明复杂度 |
| `lv00_simplify_proof(trace)` | 简化证明 |

#### 证明导出（4 个）

| 函数 | 说明 |
|------|------|
| `lv00_proof_to_natural_language(trace, lang)` | 导出为自然语言 |
| `lv00_proof_to_latex(trace)` | 导出为 LaTeX |
| `lv00_proof_to_coq(trace)` | 导出为 Coq 脚本 |
| `lv00_proof_to_isar(trace)` | 导出为 Isar 脚本 |

---

## 2. proof_session.h：REPL 风格证明会话

`proof_session.h` 提供会话式的证明构造接口，灵感来源于 Coq 的交互式证明模式和 Lean 的 tactic 框架。每个会话追踪一个目标命题，维护证明状态，支持增量证明步骤提交。

### 2.1 配置常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `LV00_SESSION_ID_MAX` | 64 | 会话 ID 最大长度 |
| `LV00_PROPOSITION_MAX` | 1024 | 命题最大长度 |
| `LV00_SESSION_JSON_MAX` | 8192 | JSON 输出缓冲区大小 |
| `LV00_SESSION_MAX_STEPS` | 4096 | 每个会话最大证明步骤数 |

### 2.2 枚举类型

**会话状态**：

```c
typedef enum {
    SESSION_STATUS_ACTIVE,     /* 会话活跃，接受步骤 */
    SESSION_STATUS_COMPLETE,   /* 证明成功完成 */
    SESSION_STATUS_ABANDONED,  /* 证明被放弃 */
    SESSION_STATUS_ERROR       /* 会话遇到错误 */
} Lv00SessionStatus;
```

**步骤结果**：

```c
typedef enum {
    STEP_RESULT_ACCEPTED,      /* 步骤被接受并应用 */
    STEP_RESULT_REJECTED,      /* 步骤被拒绝（无效 tactic） */
    STEP_RESULT_GOAL_CHANGED,  /* 步骤被接受，目标已改变 */
    STEP_RESULT_PROVED,        /* 步骤被接受，当前目标已证明 */
    STEP_RESULT_ERROR          /* 处理步骤时内部错误 */
} Lv00StepResult;
```

### 2.3 会话结构体

```c
struct Lv00ProofSession {
    char session_id[LV00_SESSION_ID_MAX]; /* 唯一会话标识 */
    uint64_t created_at;                   /* 创建时间戳 */
    char *target_proposition;              /* 目标命题 */
    Lv00ProofState *state;                 /* 当前证明状态 */
    Lv00RuleEngine *engine;                /* 规则引擎（可选） */
    int step_count;                        /* 已提交步骤数 */
    bool is_complete;                      /* 证明是否完成 */
    Lv00SessionStatus status;              /* 当前会话状态 */
};
```

### 2.4 核心 API

| 函数 | 说明 |
|------|------|
| `proof_session_create(target_proposition, engine)` | 创建新证明会话 |
| `proof_session_create_with_id(session_id, target_proposition, engine)` | 创建带自定义 ID 的证明会话 |
| `proof_session_destroy(session)` | 销毁证明会话 |
| `proof_session_submit_step(session, tactic, result)` | 提交证明步骤（tactic 应用） |
| `proof_session_get_state_json(session)` | 获取会话状态的 JSON 表示 |
| `proof_session_get_status(session)` | 获取当前会话状态 |
| `proof_session_is_complete(session)` | 检查证明是否完成 |
| `proof_session_get_id(session)` | 获取会话 ID |
| `proof_session_get_target(session)` | 获取目标命题 |
| `proof_session_get_step_count(session)` | 获取步骤计数 |
| `proof_session_abandon(session)` | 放弃当前证明会话 |
| `proof_session_reset(session)` | 重置证明会话到初始状态 |
| `session_status_to_string(status)` | 会话状态转可读字符串 |
| `step_result_to_string(result)` | 步骤结果转可读字符串 |

---

## 3. proof_score.h：证明有效性评分

`proof_score.h` 为证明提供多维度的自动评分系统，支持证明质量评估、证明比较和批量评分排序。

### 3.1 评分维度

| 维度 | 说明 | 评分公式 |
|------|------|---------|
| 简洁性 (Brevity) | 步骤越少越好 | `1.0 - (steps / max_allowed)` |
| 优雅性 (Elegance) | 公理使用多样性 | `使用公理种类 / 总可用公理种类` |
| 完备性 (Completeness) | 所有断言有据 | `有据步骤数 / 总步骤数` |
| 深度 (Depth) | 逻辑深度适中为佳 | 基于理想深度的偏好函数 |
| 反冗余 (Anti-Redundancy) | 冗余步骤比例 | `1.0 - (冗余步骤数 / 总步骤数)` |

综合评分 = w1 * 简洁性 + w2 * 优雅性 + w3 * 完备性 + w4 * 深度 + w5 * 反冗余

### 3.2 评分结构体

```c
struct Lv00ProofScore {
    /* 基础统计 */
    int steps_count;               /* 总步骤数 */
    int axiom_usage_diversity;     /* 使用的不同公理种类数 */
    int max_dependency_depth;      /* 最大依赖深度 */
    int total_axiom_applications;  /* 公理应用总次数 */
    int redundant_steps;           /* 冗余步骤数 */

    /* 维度评分（0.0 ~ 1.0） */
    double brevity_score;          /* 简洁性评分 */
    double elegance_score;         /* 优雅性评分 */
    double completeness_score;     /* 完备性评分 */
    double depth_preference_score; /* 深度偏好评分 */
    double anti_redundancy_score;  /* 反冗余评分 */

    /* 综合评分 */
    double composite_score;        /* 综合加权评分 */
    double redundancy_ratio;       /* 冗余度 */

    /* 附加属性 */
    bool has_counterexample_checks;  /* 是否包含反例检查 */
    bool uses_natural_constructions; /* 是否使用自然辅助构造 */
    int natural_construction_count;  /* 自然构造数量 */
    ProofColor trust_color;        /* 信任颜色 */
    ProofStrategyType used_strategy; /* 使用的策略 */
    char *summary;                 /* 评分摘要文本 */
};
```

### 3.3 评分配置

```c
struct Lv00ProofScoreConfig {
    double weight_brevity;          /* 简洁性权重（默认 0.25） */
    double weight_elegance;         /* 优雅性权重（默认 0.20） */
    double weight_completeness;     /* 完备性权重（默认 0.30） */
    double weight_depth;            /* 深度权重（默认 0.10） */
    double weight_anti_redundancy;  /* 反冗余权重（默认 0.15） */
    int max_allowed_steps;          /* 最大允许步骤数（默认 100） */
    double ideal_depth;             /* 理想深度（默认 10.0） */
    int max_axiom_diversity;        /* 公理多样性分母（默认 30） */
    bool prefer_shorter;            /* 偏好更短证明（默认 true） */
    bool penalize_oracle_use;       /* 惩罚 Oracle 依赖（默认 true） */
    bool count_lemma_steps;         /* 计入引理步骤（默认 true） */
};
```

### 3.4 核心 API

#### 评分

| 函数 | 说明 |
|------|------|
| `lv00_proof_evaluate(nav, config)` | 对证明进行多维评分评估 |
| `lv00_proof_evaluate_tree(tree, config)` | 对证明树进行多维评分评估 |
| `lv00_proof_score_destroy(score)` | 销毁评分对象 |

#### 比较

| 函数 | 说明 |
|------|------|
| `lv00_proof_compare(s1, s2)` | 比较两个证明评分（信任颜色 > 综合评分 > 简洁性 > 完备性） |
| `lv00_proof_select_best(scores, count)` | 在多个评分中选出最优者 |

#### 报告

| 函数 | 说明 |
|------|------|
| `lv00_proof_score_report(score)` | 生成人类可读的评分报告（含 ASCII 条形图） |
| `lv00_proof_score_summary(score)` | 生成简短评分摘要（单行格式） |

#### 批量操作

| 函数 | 说明 |
|------|------|
| `lv00_proof_evaluate_batch(navs, count, config, out_scores)` | 批量评分 |
| `lv00_proof_rank(navs, count, config, out_ranked_indices)` | 批量评分并排序 |
| `lv00_proof_score_recompute(score, config)` | 重新计算评分（修改权重后使用） |

---

## 4. proof_priority.h：推理优先级系统

`proof_priority.h` 提供定理和推理规则的优先级标记机制，用于在自动证明搜索中指导调度器优先使用高优先级规则。

### 4.1 四级优先级

```c
typedef enum {
    PRIORITY_LOW    = 0,  /* 低优先级 —— 延迟执行（如规范化规则） */
    PRIORITY_NORMAL = 1,  /* 普通优先级 —— 默认等级 */
    PRIORITY_HIGH   = 2,  /* 高优先级 —— 优先执行（如化简规则、强约束） */
    PRIORITY_URGENT = 3   /* 紧急优先级 —— 立即执行（如矛盾检测规则） */
} Lv00TheoremPriority;
```

### 4.2 优先级标记

```c
typedef struct {
    int                rule_id;          /* 规则唯一标识 */
    char               rule_name[128];   /* 规则名称 */
    Lv00TheoremPriority priority;        /* 优先级等级 */
    int                weight;           /* 权重系数（0-100） */
    bool               is_exhausted;     /* 是否已耗尽 */
    int                apply_count;      /* 已应用次数 */
    int                max_applications; /* 最大应用次数（0=无限制） */
    char              *scheduler_hint;   /* 调度器提示字符串 */
} Lv00PriorityTag;
```

### 4.3 优先级调度器

```c
typedef struct {
    Lv00PriorityTag **urgent_queue;  /* 紧急优先级规则队列 */
    Lv00PriorityTag **high_queue;    /* 高优先级规则队列 */
    Lv00PriorityTag **normal_queue;  /* 普通优先级规则队列 */
    Lv00PriorityTag **low_queue;     /* 低优先级规则队列 */
    int  current_priority_level;     /* 当前处理级别 */
    int  total_rules;                /* 注册规则总数 */
    bool sort_needed;                /* 是否需要重新排序 */
} Lv00PriorityScheduler;
```

调度策略：URGENT > HIGH > NORMAL > LOW，同级内部按 weight 降序。

### 4.4 核心 API

| 函数 | 说明 |
|------|------|
| `lv00_priority_tag_create(rule_name, rule_id, priority, weight, max_applications, scheduler_hint)` | 创建优先级标记 |
| `lv00_priority_tag_destroy(tag)` | 销毁优先级标记 |
| `lv00_priority_scheduler_create()` | 创建优先级调度器 |
| `lv00_priority_scheduler_destroy(scheduler)` | 销毁优先级调度器 |
| `lv00_priority_scheduler_register(scheduler, tag)` | 注册优先级规则 |
| `lv00_priority_scheduler_next(scheduler)` | 获取下一个应执行的规则 |
| `lv00_priority_tag_exhaust(tag)` | 标记规则为已耗尽 |
| `lv00_priority_scheduler_reprioritize(scheduler, rule_id, new_priority)` | 动态调整规则优先级 |
| `lv00_priority_tag_record_application(tag)` | 记录规则被应用 |
| `lv00_priority_scheduler_reset(scheduler)` | 重置所有规则状态 |
| `lv00_priority_scheduler_get_stats(scheduler, out_total, out_remaining, out_current_level)` | 获取调度器统计 |
| `lv00_priority_to_string(priority)` | 优先级等级转中文字符串 |

---

## 5. proof_rule_engine.h：证明规则搜索引擎

`proof_rule_engine.h` 提供可配置的基于规则的证明搜索引擎，灵感来源于 Aesop（tactic 搜索）、Seed-Prover（神经辅助构造）和 MiniF2F（神经定理证明）。

### 5.1 配置常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `LV00_PROOF_RULE_NAME_MAX` | 128 | 规则名称最大长度 |
| `LV00_GOAL_STACK_MAX` | 64 | 目标栈最大深度 |
| `LV00_HYPOTHESIS_MAX` | 128 | 最大假设数量 |
| `LV00_APPLIED_RULES_MAX` | 256 | 已应用规则历史最大长度 |
| `LV00_DEFAULT_MAX_DEPTH` | 32 | 默认最大搜索深度 |
| `LV00_DEFAULT_SEARCH_TIMEOUT_MS` | 0 | 默认搜索超时（0=无限制） |
| `LV00_RULE_SET_CAPACITY` | 64 | 默认规则集容量 |

### 5.2 枚举类型

**规则类型（10 种）**：

```c
typedef enum {
    RULE_INTRO,            /* 引入规则（如 and-intro, exists-intro） */
    RULE_ELIM,             /* 消去规则（如 and-elim, exists-elim） */
    RULE_REWRITE,          /* 重写规则（等式或定义重写） */
    RULE_INDUCTION,        /* 归纳法原理应用 */
    RULE_CONTRADICTION,    /* 矛盾 / ex falso 规则 */
    RULE_CASE_SPLIT,       /* 分情况讨论 / 析取分拆 */
    RULE_GENERALIZE,       /* 泛化（引入全称量词） */
    RULE_SPECIALIZE,       /* 特化（实例化全称量词） */
    RULE_NEURAL_SUGGEST,   /* 神经网络建议 tactic（MiniF2F 风格） */
    RULE_AUX_CONSTRUCT     /* 辅助构造（Seed-Prover 风格） */
} Lv00ProofRuleType;
```

**搜索策略（4 种）**：

```c
typedef enum {
    SEARCH_BEST_FIRST,           /* 最佳优先：按规则权重排序 */
    SEARCH_DEPTH_FIRST,          /* 深度优先 */
    SEARCH_BREADTH_FIRST,        /* 广度优先 */
    SEARCH_ITERATIVE_DEEPENING   /* 迭代加深 */
} Lv00SearchStrategy;
```

**搜索结果状态**：

```c
typedef enum {
    SEARCH_RESULT_FOUND,       /* 成功找到证明 */
    SEARCH_RESULT_TIMEOUT,     /* 搜索超时 */
    SEARCH_RESULT_DEPTH_LIMIT, /* 达到深度限制 */
    SEARCH_RESULT_EXHAUSTED,   /* 所有可能性已穷尽 */
    SEARCH_RESULT_ERROR        /* 搜索过程中内部错误 */
} Lv00SearchResultStatus;
```

### 5.3 核心数据结构

**证明规则**：

```c
struct Lv00ProofRule {
    Lv00ProofRuleType type;              /* 规则类型 */
    char name[LV00_PROOF_RULE_NAME_MAX]; /* 规则名称 */
    int priority;                        /* 静态优先级 */
    double weight;                       /* 动态权重（最佳优先排序用） */
    Lv00RuleApplicabilityCheckFn applicability_check_fn; /* 适用性检查 */
    Lv00RuleApplyFn apply_fn;            /* 规则应用函数 */
};
```

**证明状态**：

```c
struct Lv00ProofState {
    char *goal_stack[LV00_GOAL_STACK_MAX]; /* 目标栈 */
    int goal_stack_top;                     /* 当前目标索引 */
    char *current_goal;                     /* 当前目标指针 */
    char *hypotheses[LV00_HYPOTHESIS_MAX]; /* 假设集合 */
    int hypothesis_count;                   /* 假设数量 */
    char *applied_rules[LV00_APPLIED_RULES_MAX]; /* 已应用规则历史 */
    int applied_rule_count;                        /* 已应用规则数 */
    int current_depth;                              /* 当前搜索深度 */
};
```

**规则引擎**：

```c
struct Lv00RuleEngine {
    Lv00ProofRule **rule_set;    /* 已注册规则数组 */
    int rule_count;              /* 规则数量 */
    int rule_capacity;           /* 规则数组容量 */
    Lv00SearchStrategy search_strategy; /* 搜索策略 */
    int max_depth;               /* 最大搜索深度 */
    uint64_t timeout_ms;         /* 搜索超时（毫秒） */
};
```

### 5.4 核心 API

#### 规则引擎管理

| 函数 | 说明 |
|------|------|
| `rule_engine_create()` | 创建规则引擎（默认配置） |
| `rule_engine_create_ex(strategy, max_depth, timeout_ms)` | 创建规则引擎（自定义配置） |
| `rule_engine_destroy(engine)` | 销毁规则引擎 |
| `rule_engine_add_rule(engine, rule)` | 添加规则（所有权转移） |
| `rule_engine_remove_rule(engine, name)` | 按名称移除规则 |
| `rule_engine_find_rule(engine, name)` | 按名称查找规则 |
| `rule_engine_search(engine, state)` | 执行证明搜索 |
| `rule_engine_rule_count(engine)` | 获取已注册规则数量 |

#### 证明状态管理

| 函数 | 说明 |
|------|------|
| `proof_state_create(initial_goal)` | 创建证明状态 |
| `proof_state_destroy(state)` | 销毁证明状态 |
| `proof_state_push_goal(state, goal)` | 压入子目标 |
| `proof_state_pop_goal(state)` | 弹出当前目标 |
| `proof_state_add_hypothesis(state, hypothesis)` | 添加假设 |
| `proof_state_record_rule(state, name)` | 记录已应用规则 |
| `proof_state_is_complete(state)` | 检查证明是否完成（目标栈为空） |
| `proof_state_current_goal(state)` | 获取当前目标字符串 |

#### 工具函数

| 函数 | 说明 |
|------|------|
| `proof_rule_type_to_string(type)` | 规则类型转可读字符串 |
| `search_strategy_to_string(strategy)` | 搜索策略转可读字符串 |
| `search_result_status_to_string(status)` | 搜索结果状态转可读字符串 |

---

## 6. proof_version.h：证明版本控制

`proof_version.h` 提供基于文件系统的轻量级证明版本控制系统，使用 SHA-256 哈希进行内容寻址和提交标识。灵感来源于 libgit2 的 API 设计，但不依赖外部库。

### 6.1 配置常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `LV00_OID_LENGTH` | 65 | 提交 OID 最大长度（SHA-256 hex + null） |
| `LV00_COMMIT_MSG_MAX` | 512 | 提交消息最大长度 |
| `LV00_MAX_BRANCHES` | 64 | 每个仓库最大分支数 |
| `LV00_BRANCH_NAME_MAX` | 128 | 分支名称最大长度 |
| `LV00_LOG_MAX_ENTRIES` | 256 | 日志查询最大条目数 |

### 6.2 核心数据结构

**证明提交**：

```c
typedef struct Lv00ProofCommit {
    char     oid[LV00_OID_LENGTH];          /* SHA-256 哈希（hex） */
    char     message[LV00_COMMIT_MSG_MAX];  /* 提交消息 */
    char     parent_oid[LV00_OID_LENGTH];   /* 父提交 OID（根提交为空字符串） */
    int64_t  timestamp;                     /* Unix 时间戳 */
} Lv00ProofCommit;
```

**差异条目**：

```c
typedef struct Lv00ProofDiffEntry {
    char path[256];               /* 变更文件路径 */
    char old_hash[LV00_OID_LENGTH]; /* 旧内容 SHA-256 */
    char new_hash[LV00_OID_LENGTH]; /* 新内容 SHA-256 */
    int  change_type;             /* 0=新增, 1=修改, 2=删除 */
} Lv00ProofDiffEntry;
```

**差异结果**：

```c
typedef struct Lv00ProofDiff {
    Lv00ProofDiffEntry *entries;   /* 差异条目数组 */
    size_t              count;     /* 条目数量 */
} Lv00ProofDiff;
```

**证明仓库**：

```c
typedef struct Lv00ProofRepo {
    char     path[512];                  /* 仓库根路径 */
    char     head_commit[LV00_OID_LENGTH]; /* HEAD 提交 OID */
    int      branch_count;               /* 分支数量 */
    char     branches[LV00_MAX_BRANCHES][LV00_BRANCH_NAME_MAX]; /* 分支名称 */
    char     branch_heads[LV00_MAX_BRANCHES][LV00_OID_LENGTH];  /* 分支头 OID */
} Lv00ProofRepo;
```

### 6.3 核心 API

#### 仓库生命周期

| 函数 | 说明 |
|------|------|
| `proof_repo_init(path)` | 初始化新证明仓库 |
| `proof_repo_open(path)` | 打开已有证明仓库 |
| `proof_repo_destroy(repo)` | 销毁仓库对象（不删除磁盘数据） |

#### 提交操作

| 函数 | 说明 |
|------|------|
| `proof_repo_commit(repo, message, files, contents, file_count)` | 创建新提交（SHA-256 内容寻址） |

#### 历史与差异

| 函数 | 说明 |
|------|------|
| `proof_repo_log(repo, commits, max_count)` | 获取从 HEAD 开始的提交日志 |
| `proof_repo_diff(repo, oid_a, oid_b, diff)` | 计算两个提交之间的差异 |
| `proof_repo_diff_destroy(diff)` | 释放差异结果资源 |

#### 分支管理

| 函数 | 说明 |
|------|------|
| `proof_repo_branch(repo, name)` | 创建新分支（指向当前 HEAD） |
| `proof_repo_checkout(repo, name)` | 切换分支（更新 HEAD） |

---

## 与核心 proof.h 的关系说明

核心 `proof.h`（文档 09）定义了 Lv-00 证明系统的基本框架：

| 核心能力 | 所在模块 | 说明 |
|---------|---------|------|
| 命题模式定义 | `proof.h` | `Proposition` 结构体、端口声明、几何模式 |
| 证明状态 | `proof.h` | `ProofStatus` 枚举（5 种状态） |
| 证明步骤 | `proof.h` | `ProofStep` 结构体、`StepType` 枚举 |
| 证明记录 | `proof.h` | `Proof` 结构体、信任颜色传播 |
| 证明导航器 | `proof.h` | `ProofNavigator`、步骤回放 |
| 不可构造性证明 | `proof.h` | 不可构造性判定与标记 |

增强模块在核心框架上的扩展：

| 扩展能力 | 所在模块 | 与核心的关系 |
|---------|---------|-------------|
| 溯源树 | `proof_engine_enhanced.h` | 扩展 `Proof` 的步骤记录为树形依赖图 |
| 反证法完善 | `proof_engine_enhanced.h` | 扩展核心的反证法为完整矛盾路径追踪 |
| 策略调度 | `proof_engine_enhanced.h` | 扩展核心的单一证明路径为多策略自动选择 |
| 证明验证 | `proof_engine_enhanced.h` | 独立验证核心 `Proof` 的正确性 |
| 证明优化 | `proof_engine_enhanced.h` | 简化核心 `Proof` 的步骤序列 |
| 交互式构造 | `proof_session.h` | 封装核心 `ProofNavigator` 为 REPL 接口 |
| 质量评估 | `proof_score.h` | 对核心 `Proof` 进行多维度评分 |
| 规则调度 | `proof_priority.h` | 为规则搜索提供优先级元数据 |
| 规则搜索 | `proof_rule_engine.h` | 实现自动化的规则应用搜索 |
| 版本管理 | `proof_version.h` | 对证明产物进行持久化版本控制 |
