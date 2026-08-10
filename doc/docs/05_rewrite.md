# 05. 图重写引擎（Graph Rewrite Engine）

## 模块概述

图重写引擎是 Lv-00 推理层的变换设施，以约束图（`ConstraintGraph`）为操作对象，提供规则匹配、结构替换与循环检测能力。引擎借鉴 Maude 的策略组合子与 Herbie 的数值精度优化，形成一条从"基础子图匹配"到"逆向证明搜索"的完整重写管线。

- **匹配算法**：VF2 子图同构匹配，支持局部等价容错与"最佳匹配/全部非重叠匹配"两种检索模式；
- **替换操作**：规则替换以图快照（`GraphSnapshot`）为事务边界，冲突时回滚；
- **循环检测**：Weisfeiler-Lehman（WL）图核哈希历史 + 归约度量双重保障终止性；
- **重写策略**：Maude 风格策略树（idle / fail / sequence / orelse / repeat / normalize / try / β-归约）与 8 种方向性策略（`lv_RWS_*`）。

**覆盖头文件**：
- `rewrite.h` —— 核心引擎：规则、匹配、替换、WL 循环检测、策略组合子、Herbie 数值规则
- `rewrite_strategy.h` —— 扩展策略枚举与规则引擎（条件规则、`lv_RWS_*` 八策略）

## 核心设计原则

1. **模式-替换分离**：`RewritePattern` 描述"长什么样"，`RewriteReplacement` 描述"替换成什么"，二者经 `RewriteRule` 组合，规则可热加载/卸载（`.lvz` 文件）。
2. **同构即匹配**：匹配采用子图同构语义而非语法相等；`local_equivalence_tolerant` 开关控制局部等价容错。
3. **事务性替换**：每次替换前创建 `GraphSnapshot`（深拷贝节点、约束、端口/区域/函数块交叉引用），冲突或度量验证失败时恢复。
4. **归约度量强制终止**：每条规则携带 `reduction_measure`，应用后由 `rewrite_validate_measure` 验证严格归约，从源头阻断循环。
5. **策略显式编排**：规则应用顺序由策略树决定，`REPEAT`/`NORMALIZE` 反复应用直至不动点，步数受 `step_limit`/`max_iterations` 约束。
6. **前条件可判定**：规则可挂接 `RewritePrecondition` 回调，匹配命中后仍需前条件满足才可替换。

## 关键数据结构

### 规则、模式与匹配

```c
typedef struct RewritePattern {
    int kind;                        /* type kind (TypeKind) */
    int *variable_node_ids;
    int var_count;
    Constraint **pattern_constraints;
    int pattern_constraint_count;
} RewritePattern;

typedef struct RewriteReplacement {
    int **node_bindings;
    int binding_count;
    Constraint **replacement_constraints;
    int replacement_constraint_count;
    int *new_nodes;
    int new_node_count;
    GeomType *new_node_types;        /* 新节点几何类型，与 new_nodes 一一对应 */
} RewriteReplacement;

typedef struct RewriteMatch {
    int *node_bindings;
    int *constraint_bindings;
    int binding_count;
} RewriteMatch;

typedef bool (*RewritePrecondition)(ConstraintGraph *graph, RewriteMatch *match, void *user_data);

typedef struct RewriteRule {
    RewritePattern *pattern;
    RewriteReplacement *replacement;
    int reduction_measure;           /* 归约度量，用于循环检测 */
    char *name;
    RewritePrecondition condition_func;   /* 前置条件回调 */
    void *condition_data;
} RewriteRule;
```

### 状态枚举与 WL 循环检测

```c
typedef enum {
    REWRITE_STATUS_OK,               /* 重写成功（无操作） */
    REWRITE_STATUS_NO_MATCH,         /* 未找到匹配 */
    REWRITE_STATUS_APPLIED,          /* 规则已应用 */
    REWRITE_STATUS_CONFLUENCE_ISSUE, /* 汇流性问题 */
    REWRITE_STATUS_TERMINATED        /* 重写终止 */
} RewriteStatus;

#define WL_ITERATIONS 3              /* WL 哈希迭代轮数 */
#define WL_HISTORY_SIZE 64           /* 哈希历史环形缓冲容量 */

typedef struct {
    uint64_t *hash_history;          /* 完整 WL 哈希环形缓冲 */
    int history_count;
    int history_pos;
    uint32_t *light_hash_history;    /* 轻量哈希（快速预筛选） */
    int light_history_count;
    int light_history_pos;
} WLHashHistory;
```

### 图快照（事务回滚载体）

```c
typedef struct GraphSnapshot {
    GeomNode **nodes;                /* 节点深拷贝 */
    int node_count;
    Constraint **constraints;        /* 约束深拷贝 */
    int constraint_count;
    int next_node_id;
    int next_constraint_id;
    PortRef *port_refs;              /* PORT connected_to 交叉引用（存 ID） */
    RegionRef *region_refs;          /* REGION boundary_segments ID 信息 */
    FBRef *fb_refs;                  /* FUNCTION_BLOCK internal_nodes ID 信息 */
} GraphSnapshot;
```

### Maude 风格策略树与 Herbie 数值规则

```c
typedef enum {
    REWRITE_STRATEGY_KIND_IDLE, REWRITE_STRATEGY_KIND_FAIL,
    REWRITE_STRATEGY_KIND_APPLY_RULE, REWRITE_STRATEGY_KIND_MATCH_PATTERN,
    REWRITE_STRATEGY_KIND_TEST_COND, REWRITE_STRATEGY_KIND_SEQUENCE,
    REWRITE_STRATEGY_KIND_ORELSE, REWRITE_STRATEGY_KIND_REPEAT,
    REWRITE_STRATEGY_KIND_NORMALIZE, REWRITE_STRATEGY_KIND_TRY,
    REWRITE_STRATEGY_KIND_BETA_REDUCE   /* 对函数块执行一次 β-归约 */
} RewriteStrategyKind;

typedef struct RewriteStrategy {
    RewriteStrategyKind kind;
    int rule_id;                     /* APPLY_RULE: 规则索引 */
    char *pattern_expr;              /* MATCH_PATTERN */
    int (*test_func)(void *);        /* TEST_COND */
    void *test_ctx;
    struct RewriteStrategy *left;    /* 组合子左子树 */
    struct RewriteStrategy *right;   /* 组合子右子树 */
    int max_iterations;              /* REPEAT: 0 = 不限 */
} RewriteStrategy;

typedef enum {
    REWRITE_NUM_CRITICAL = 0,        /* 消除灾难性抵消 */
    REWRITE_NUM_HIGH = 1,            /* 改善条件数 */
    REWRITE_NUM_MEDIUM = 2,          /* 重组表达式 */
    REWRITE_NUM_LOW = 3              /* 微调不影响正确性 */
} RewriteNumPriority;

typedef struct RewriteNumRule {
    char *name;
    char *pattern_expr;
    char *replacement_expr;
    RewriteNumPriority priority;
    double accuracy_improvement;     /* 精度改进倍数（估计值） */
    char *condition_desc;
    bool (*condition)(double *vars, int n);   /* 触发条件检测 */
} RewriteNumRule;
```

`rewrite_strategy.h` 补充定义 `lv_RWS_FIRST/BEST/BREADTH/DEPTH/INNERMOST/OUTERMOST/PARALLEL/EGRAPH` 八种方向策略，并以 `lvRewriteRuleEx`（含 `lvRewriteConditionFn` 条件函数与优先级）组成 `lvRewriteEngineEx` 字符串级规则引擎。

## 主要接口

### 规则生命周期与匹配替换

| 接口 | 签名要点 | 说明 |
|------|----------|------|
| `rewrite_rule_create` | `(name, pattern, replacement, measure)` | 创建规则（measure 用于循环检测） |
| `rewrite_rule_destroy` | `(rule)` | 销毁规则 |
| `rewrite_rules_load_from_file` | `(filepath, &rules, &count)` | 从 `.lvz` 文件热加载规则 |
| `rewrite_rule_unload` | `(&rules, &count, name)` | 按名称卸载规则 |
| `find_rewrite_match` | `(graph, rule, tolerant)` | 查找首个匹配 |
| `find_best_match` | `(graph, rule, tolerant)` | 查找最佳匹配 |
| `vf2_find_match` | `(target_graph, pattern, tolerant)` | VF2 子图同构匹配 |
| `apply_rewrite` | `(graph, rule, match)` | 应用替换，返回 `RewriteStatus` |
| `find_all_non_overlapping_matches` | `(graph, rule, used_ids, used_count, &matches, &count)` | 全部非重叠匹配（按约束数降序） |
| `rewrite_apply_all_matches` | `(graph, rule, matches, count, &applied)` | 批量应用，逐匹配快照回滚 |

### 循环检测与策略

| 接口 | 签名要点 | 说明 |
|------|----------|------|
| `rewrite_compute_wl_hash` | `(graph)` | 计算 WL 图核哈希（同构必要条件的快速过滤） |
| `wl_history_init` / `wl_history_destroy` | `(hist)` | 哈希历史环形缓冲管理 |
| `detect_rewrite_loop_wl` | `(graph, hist)` | 图核哈希撞库即判定循环 |
| `rewrite_validate_measure` | `(graph, rule, snapshot_before)` | 验证归约度量确实减少 |
| `rewrite_with_rules` | `(graph, rules, count, step_limit, normalize_between)` | 多规则顺序重写 |
| `rewrite_strategy_*` | 见 `rewrite_strategy_create_idle` 等 12 个构造器 | 构建策略树 |
| `rewrite_strategy_apply` | `(graph, strategy, rules, count, &out_graph, &steps)` | 执行策略树（Maude `srewrite` 对应） |
| `rewrite_search_backward` | `(target, rules, count, max_depth, bfs, &path, &len)` | 逆向证明搜索（Maude `search =>*`） |
| `rewrite_num_optimize` | `(expr, rules, count, &improvement)` | 数值精度优化 |
| `rewrite_num_register_builtins` | `(void)` | 注册 6 条内置数值规则（如 sqrt-diff-recip） |
| `rewrite_set_stream_context` | `(ctx)` | 绑定流式输出上下文 |

## 工作流程

1. **装载**：`rewrite_rule_create` 或 `rewrite_rules_load_from_file` 构建 `RewriteRule` 数组；`rewrite_strategy_*` 组装策略树。
2. **匹配**：`find_rewrite_match` / `vf2_find_match` 在构造图上检索模式子图；`find_all_non_overlapping_matches` 先标记已用节点再迭代检索。
3. **验证**：命中后执行 `condition_func` 前条件；替换前 `graph_snapshot_create` 生成事务快照。
4. **替换**：`apply_rewrite` 按 `node_bindings`/`constraint_bindings` 停用旧结构、并入 `new_nodes`；冲突则 `graph_snapshot_restore` 回滚。
5. **终止检查**：`detect_rewrite_loop_wl` 将新哈希写入历史，撞库判定 `REWRITE_STATUS_CONFLUENCE_ISSUE`；否则 `rewrite_validate_measure` 校验归约度量。
6. **策略编排**：`rewrite_strategy_apply` 按策略树递归执行，`REPEAT`/`NORMALIZE` 迭代至不动点或步数上限；`rewrite_search_backward` 以 BFS/DFS 从目标命题逆向搜索公理路径。

## 模块关系

| 模块 | 关系 | 说明 |
|------|------|------|
| [02_constraint_graph.md](02_constraint_graph.md) | 数据载体 | 重写引擎的操作对象即 `ConstraintGraph`，匹配/替换围绕节点与约束展开 |
| [03_normalization.md](03_normalization.md) | 协同 | `rewrite_with_rules` 支持 `normalize_between_steps`，每步间调用规范化保证匹配基准一致 |
| [04_solver.md](04_solver.md) | 前置 | 重写生成的新子图交求解器传播；求解器确认无冲突后才提交替换 |
| [07_func_block.md](07_func_block.md) | β-归约 | `REWRITE_STRATEGY_KIND_BETA_REDUCE` 对函数块执行一次归约 |
| [09_proof.md](09_proof.md) | 上层 | `rewrite_search_backward` 输出的规则 ID 路径即证明步骤序列 |
| [11_wfc_paradigm.md](11_wfc_paradigm.md) | 并行 | 约束传播与重写可交替进行（传播坍缩 → 重写化简） |
| [25_engine_scheduler.md](25_engine_scheduler.md) | 调度 | 引擎调度器为匹配/替换分配执行预算与并行单元 |

## 版本历史

| 版本 | 变更 |
|------|------|
| v1.0 | VF2 匹配、`apply_rewrite`、快照回滚、WL 循环检测（P0 基础管线） |
| v1.1 | 规则热加载（`.lvz`）、`find_all_non_overlapping_matches` 批量应用 |
| v1.2 | 引入 `rewrite_strategy.h` 的 `lv_RWS_*` 八方向策略与条件规则 |
| v1.3 | Maude 策略组合子（11 种 kind）、逆向证明搜索 |
| v1.4 | Herbie 风格数值精度规则、`rewrite_validate_measure` 归约度量验证 |
