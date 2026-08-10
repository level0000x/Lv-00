# 10. 递归与条件系统 (Recursion & Condition System)

## 模块概述

递归与条件系统（`recursion.h`）依据 Lv-00 设计文档第 9 节实现递归构造机制，为函数块（Function Block）的递归调用提供终止性保证。系统由四部分构成：

1. **测度系统（MeasureSystem）**：管理递归终止判定的度量集合，支持符号测度（长度/面积/角度/深度）与非符号测度（公理包定义的抽象序结构）。
2. **选择器块（SelectorBlock）**：条件分支构造，根据测试点与测试区域判定结果选择真/假分支子图，并维护分支互斥性。
3. **递归深度监控（RecursionContext）**：上下文级递归执行控制，记录调用栈与测度值历史，验证每次递归调用测度严格递减。
4. **全局熔断保护**：线程局部全局深度计数器（`lv_MAX_RECURSION_DEPTH` = 128），超限即触发熔断器，防止无限递归导致栈溢出。

## 核心设计原则

1. **测度良基性（终止性）**：每次递归调用必须使至少一个测度值严格递减；测度携带 `is_well_founded` 良基标记，从良基关系上证明递归必然终止。
2. **符号/非符号双层测度**：符号测度归约到符号坐标上的代数表达式（`measure_compute_value_symbolic`）；非符号测度由公理包提供比较器（`NonSymbolicComparator`）并通过模板展开（`NonSymbolicMeasureValidationMeta`）验证递减性。
3. **双层深度防护**：上下文级 `max_depth`（默认 10000，硬上限 `lv_MAX_RECURSION_DEPTH_LIMIT` = 100000）负责测度语义验证；全局级 `lv_MAX_RECURSION_DEPTH` = 128 作为更严格的硬熔断阈值，深度超限自动终止递归链并触发熔断器。
4. **分支互斥性**：选择器块真/假分支的节点 ID 集合必须互斥（`selector_block_validate_branches`），保证同一节点不会被两个分支同时定义。
5. **互递归一致性**：支持函数块间相互递归，验证多个递归上下文在统一测度下各自递减且合并后交叉递减（`recursion_check_mutual_with_contexts`）。
6. **枚举命名规范**：所有枚举值统一使用 UPPER_SNAKE_CASE，并为旧代码保留短名称兼容别名。

## 关键数据结构

```c
/* 测度 —— 定义递归终止判定的度量方式 */
struct Measure {
    int id;                          /* 测度 ID */
    MeasureType type;                /* MEASURE_TYPE_SYMBOLIC / MEASURE_TYPE_CUSTOM */
    char *name;
    int reference_node_id;           /* 参考节点 ID（符号测度） */
    enum {
        MEASURE_KIND_LENGTH,         /* 线段长度 —— 端点间欧氏距离 */
        MEASURE_KIND_AREA,           /* 区域面积 */
        MEASURE_KIND_ANGLE,          /* 夹角 */
        MEASURE_KIND_DEPTH,          /* 嵌套深度 */
        MEASURE_KIND_CUSTOM          /* 自定义测度函数 */
    } kind;
    int (*compare_func)(GeomNode *a, GeomNode *b, void *user_data);
    void *user_data;
    bool is_well_founded;            /* 良基关系标记 */
};

/* 测度系统 —— 递归终止条件的测度集合 */
struct MeasureSystem {
    Measure **measures;              /* 测度数组（指数增长策略扩容） */
    int measure_count;
    int measure_capacity;
    Measure *default_measure;        /* 默认测度 */
    bool has_non_symbolic;
    NonSymbolicMeasureMeta *non_symbolic_metas;      /* 非符号测度元数据 */
    NonSymbolicMeasureValidationMeta *validation_metas; /* 验证模板元数据 */
};

/* 递归上下文 —— 上下文级递归执行控制 */
struct RecursionContext {
    int current_depth;               /* 当前递归深度 */
    int max_depth;                   /* 最大深度（默认 10000） */
    Measure *active_measure;
    SymbolicCoord **measure_values;  /* 测度值历史（验证单调递减） */
    int *call_stack;                 /* 调用栈（函数块 ID） */
    bool is_terminated;
    char *termination_reason;        /* 终止原因 */
    RecursionDepthCallback depth_callback;  /* 深度超限回调 */
};

/* 选择器块 —— 条件分支构造 */
struct SelectorBlock {
    int id;
    int test_point_id;               /* 测试点 ID */
    int test_region_id;              /* 测试区域 ID */
    int true_branch_root_id;         /* 真分支根节点 ID */
    int false_branch_root_id;        /* 假分支根节点 ID */
    BranchState true_state;          /* BRANCH_ACTIVE_SELECTED 等 */
    int *true_branch_node_ids;       /* 真分支子图节点 ID 数组 */
    int *false_branch_node_ids;
    ConstraintGraph *graph;          /* 所属约束图 */
};
```

递归检查结果枚举：`RECURSION_CHECK_RESULT_OK / NOT_DECREASING / DEPTH_EXCEEDED / CYCLE_DETECTED / MEASURE_UNKNOWN / ERROR`。

## 主要接口

### 测度系统

| 函数 | 签名要点 | 说明 |
|------|----------|------|
| `measure_system_create` | `MeasureSystem *measure_system_create(void)` | 创建测度系统 |
| `measure_system_destroy` | `void (MeasureSystem *ms)` | 销毁测度系统 |
| `measure_create_symbolic` | `Measure *(const char *name, int kind, int ref_node_id)` | 创建符号测度 |
| `measure_create_custom` | `Measure *(const char *name, int (*)(GeomNode*,GeomNode*,void*), void *user_data)` | 创建非符号测度 |
| `measure_system_add` | `bool (MeasureSystem *ms, Measure *m)` | 添加测度 |
| `measure_compute_value` | `SymbolicCoord *(Measure*, GeomNode*, ConstraintGraph*)` | 计算节点测度值 |
| `measure_compute_value_symbolic` | `SymbolicCoord *(Measure*, GeomNode*, ConstraintGraph*)` | 纯符号计算（面积测度） |
| `measure_compare` | `MeasureCompareResult (Measure*, SymbolicCoord*, SymbolicCoord*)` | 比较测度值 |
| `measure_system_register_non_symbolic` | `bool (MeasureSystem*, int, NonSymbolicComparator, bool)` | 注册非符号测度元数据 |
| `measure_system_validate_non_symbolic` | `bool (MeasureSystem *ms)` | 验证全部非符号测度 |

### 递归上下文

| 函数 | 签名要点 | 说明 |
|------|----------|------|
| `recursion_context_create` | `RecursionContext *(int max_depth)` | 创建递归上下文 |
| `recursion_context_enter` | `RecursionCheckResult (RecursionContext*, int func_block_id, const GeomNode*, ConstraintGraph*)` | 进入递归调用 |
| `recursion_context_exit` | `void (RecursionContext*)` | 退出递归调用 |
| `recursion_context_check_decreasing` | `RecursionCheckResult (const RecursionContext*, SymbolicCoord *new_value)` | 验证整条调用链单调递减 |
| `recursion_context_set_depth_callback` | `void (RecursionContext*, RecursionDepthCallback, void*)` | 注册深度超限回调 |
| `recursion_validate_measure` | `RecursionCheckResult (const RecursionContext*, const Measure*, const ConstraintGraph*, int node_id)` | 符号测度验证 |
| `recursion_check_mutual` | `bool (int *func_ids, int count, MeasureSystem *ms)` | 互递归测度一致性 |
| `recursion_check_mutual_with_contexts` | `bool (RecursionContext *ctx_a, RecursionContext *ctx_b)` | 双上下文交叉递减验证 |

### 全局深度保护（熔断器）

| 函数 | 签名要点 | 说明 |
|------|----------|------|
| `lv_recursion_enter` | `bool lv_recursion_enter(void)` | 全局深度 +1，超 `lv_MAX_RECURSION_DEPTH` 触发熔断 |
| `lv_recursion_leave` | `void lv_recursion_leave(void)` | 全局深度 -1，归零且未熔断时自动重置 |
| `lv_recursion_circuit_breaker_triggered` | `bool (void)` | 查询熔断器是否已触发 |
| `lv_recursion_reset` | `void (void)` | 清零深度并清除熔断标志 |
| `lv_recursion_get_depth` | `int (void)` | 获取当前全局递归深度 |

### 选择器块与验证

| 函数 | 签名要点 | 说明 |
|------|----------|------|
| `selector_block_create` | `SelectorBlock *(int id, ConstraintGraph *graph)` | 创建选择器块 |
| `selector_block_set_condition` | `bool (SelectorBlock*, int point_id, int region_id)` | 设置测试条件 |
| `selector_block_set_branches` | `bool (SelectorBlock*, int true_root, int false_root)` | 设置分支根 |
| `selector_block_set_branch_nodes` | `void (SelectorBlock*, int*, int, int*, int)` | 设置分支子图节点 |
| `selector_block_evaluate` | `bool (SelectorBlock*, ConstraintGraph*)` | 评估并激活分支 |
| `selector_block_get_active_branch` | `int (SelectorBlock*)` | 获取活跃分支根节点 |
| `selector_block_validate_branches` | `bool (const SelectorBlock*)` | 校验两分支节点集合互斥 |
| `recursion_run_builtin_tests` | `int (MeasureSystem*, RecursionTestResult **, int *)` | 运行模块内置测试套件 |

## 工作流程

**递归执行与终止性验证**：

1. 创建 `RecursionContext`（`recursion_context_create`），绑定活动测度并注册深度回调。
2. 每次递归调用前执行 `lv_recursion_enter()`：全局深度 +1；若深度超过 128，熔断器置位，`recursion_context_enter` 返回 `RECURSION_CHECK_RESULT_DEPTH_EXCEEDED`。
3. `recursion_context_enter` 记录函数块 ID 入调用栈，计算当前节点测度值并追加到 `measure_values` 历史。
4. `recursion_context_check_decreasing` 遍历整条测度值历史，验证严格单调递减；非递减返回 `NOT_DECREASING`，循环检测返回 `CYCLE_DETECTED`。
5. 选择器块在递归体内作为条件分支：`selector_block_evaluate` 依据测试点/区域判定激活真或假分支子图。
6. 递归返回时 `recursion_context_exit` 弹栈、`lv_recursion_leave()` 全局深度 -1；深度归零时自动复位熔断器状态。

**非符号测度验证**：公理包注册比较器（`measure_system_register_non_symbolic`）与验证模板（`recursion_validate_non_symbolic_with_axiom`），对 before/after 测度值经模板展开后判定递减（`recursion_validate_non_symbolic_measure`）。

## 模块关系

| 关联文档 | 关系说明 |
|----------|----------|
| [07_func_block.md](07_func_block.md) | 递归上下文以函数块 ID 维护调用栈；选择器块为函数块多解选择器的条件化扩展 |
| [12_context_and_lifecycle.md](12_context_and_lifecycle.md) | 递归上下文的创建/销毁与 `lvContext` 生命周期管理协同 |
| [23_core_infrastructure.md](23_core_infrastructure.md) | 依赖 `constraint_graph.h`（约束图）、`symbolic_coord.h`（符号坐标）与 `stream.h`（`recursion_set_stream_context` 流式接入） |
| [24_constraint_propagation.md](24_constraint_propagation.md) | 选择器块分支激活驱动约束子图的条件传播 |
| [25_engine_scheduler.md](25_engine_scheduler.md) | 递归执行由引擎调度器驱动，深度监控为调度提供中止信号 |
| [32_runtime_monitoring.md](32_runtime_monitoring.md) | `runtime_guard.h` 定义 `lv_RUNTIME_GUARD_MAX_RECURSE`（128）与数据完整性校验中的递归深度检查 |

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| v1.0.0 | 2026-08-10 | 初始版本：测度系统、选择器块、递归上下文与全局深度熔断器 |
| v1.1.0 | 2026-08-10 | 新增：非符号测度元数据（修改 6）、分支子图节点管理（修改 3）、深度超限回调（修改 5）、完整测度链递减验证（修改 1）、互递归双上下文验证（修改 2）、内置测试套件（Feature 1）、公理模板展开（Feature 2） |
