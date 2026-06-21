# 25. 引擎核心与调度系统

## 25.1 模块概述

本文档描述 Lv-00 几何元语言系统的核心引擎和调度框架。引擎是系统的中央调度器，负责协调约束图、模块、公理包和重写规则之间的工作流程。调度器实现多求解引擎的动态路由，根据约束图特征自动选择最佳后端。

**覆盖头文件**：
- `ctx.h` —— 主引擎核心
- `engine_scheduler.h` —— 多引擎调度框架

---

## 25.2 ctx.h —— 主引擎核心

### 25.2.1 设计定位

`ctx.h` 是 Lv-00 系统的**中央调度器**，负责：
- 引擎生命周期管理（创建、销毁、状态机控制）
- 模块与公理包的动态加载
- 重写规则注册与管理
- 重写-求解协作流水线
- 位电路跳闸处理与冻结点回滚
- 五状态机形式化管理
- 流式输出集成
- 十层架构层级验证

### 25.2.2 十层架构层级标识

```c
#define LV00_LAYER_PARSER    1  // 输入解析层
#define LV00_LAYER_RESOURCE  2  // 资源管理层
#define LV00_LAYER_GEOMETRY  3  // 几何拓扑层
#define LV00_LAYER_REASONING 4  // 公理推理层
#define LV00_LAYER_OUTPUT    5  // 结果输出层
```

**层级验证宏**（编译时）：
```c
// 声明当前编译单元允许调用的最低层级
LV00_ALLOW_LAYER(LV00_LAYER_RESOURCE);  // 允许调用 Layer 2+

// 更严格的检查：要求必须高于目标层
LV00_REQUIRE_STRICTLY_ABOVE(LV00_LAYER_GEOMETRY);
```

**运行时层级验证标志**：
| 标志 | 值 | 说明 |
|------|-----|------|
| `LV00_LAYER_VALIDATION_FLAG_NONE` | 0x00 | 不执行层级验证 |
| `LV00_LAYER_VALIDATION_FLAG_RUNTIME` | 0x01 | 运行时调用栈检查 |
| `LV00_LAYER_VALIDATION_FLAG_STRICT` | 0x02 | 严格模式：违规即中止 |

### 25.2.3 五状态引擎状态机

```
                   ┌──────────────┐
                   │    IDLE      │  ← 初始状态
                   └──────┬───────┘
                          │ 开始解析输入
                          ▼
                   ┌──────────────┐
              ┌─── │   PARSING    │ ── 解析失败 ──→ ERROR
              │    └──────┬───────┘
              │           │ 解析完成，开始推理
              │           ▼
              │    ┌──────────────┐
              │    │  REASONING   │ ── 矛盾/超时 ──→ ERROR
              │    └──────┬───────┘
              │           │ 证明成功
              │           ▼
              │    ┌──────────────┐
              └───→│  COMPLETE    │ ── 重置 ──→ IDLE
                   └──────────────┘
                        ↑
                   ┌──────────────┐
                   │    ERROR     │ ── 重置 ──→ IDLE
                   └──────────────┘
```

**状态转移规则**：
| 从状态 | 到状态 | 触发条件 |
|--------|--------|----------|
| IDLE | PARSING | 开始接收新输入 |
| IDLE | ERROR | 初始化失败 |
| PARSING | REASONING | 解析成功完成 |
| PARSING | ERROR | 解析失败 |
| PARSING | IDLE | 取消/中断 |
| REASONING | COMPLETE | 证明/求解成功 |
| REASONING | ERROR | 矛盾/超时/资源耗尽 |
| REASONING | IDLE | 取消/中断 |
| COMPLETE | IDLE | 重置，准备新问题 |
| ERROR | IDLE | 重置，清理错误状态 |

```c
typedef enum {
    ENGINE_STATE_IDLE,      // 空闲状态
    ENGINE_STATE_PARSING,   // 解析状态
    ENGINE_STATE_REASONING, // 推理状态
    ENGINE_STATE_ERROR,     // 错误状态
    ENGINE_STATE_COMPLETE   // 完成状态
} EngineState;
```

### 25.2.4 LV00Context 结构体

```c
typedef struct LV00Context {
    // 核心数据容器
    ConstraintGraph *main_graph;    // 主约束图
    Module **loaded_modules;        // 已加载模块数组
    int module_count, module_capacity;
    AxiomPackage **axiom_packages;  // 已加载公理包数组
    int axiom_package_count, axiom_package_capacity;
    RewriteRule **rewrite_rules;    // 重写规则数组
    int rewrite_rule_count, rewrite_rule_capacity;
    
    int rewrite_step_limit;         // 重写步数上限（默认 1000）
    
    // 状态与快照
    void *frozen_point;             // 冻结点快照
    int last_unify_status;          // 上一次合一状态
    
    // 错误报告
    EngineStatus last_status;
    char last_error[LV00_CONFIG_ENGINE_ERROR_BUFFER_SIZE];
    
    // 流式输出
    StreamContext *stream_ctx;
    
    // 上下文与架构
    struct Lv00Context *context;
    int layer_validation_flags;
    
    // 状态机
    EngineState state;
    EngineState previous_state;
    int state_transition_count;
} LV00Context;
```

### 25.2.5 引擎生命周期 API

```c
// 创建与销毁
LV00Context *lv00_context_create();
void lv00_context_destroy(LV00Context *ctx);

// 资源加载
bool engine_add_rewrite_rule(LV00Context *ctx, const RewriteRule *rule);
ModuleLoadStatus engine_load_module(LV00Context *ctx, const char *filepath);
AxiomLoadStatus engine_load_axiom_package(LV00Context *ctx, const char *filepath);

// 函数块操作
bool engine_pack_function(LV00Context *ctx, 
    const int *internal_node_ids, int internal_count,
    const int *input_port_ids, int input_count,
    const int *output_port_ids, int output_count,
    int *out_func_block_id);
int *engine_instantiate_function(LV00Context *ctx, int func_block_id,
    const int *arg_mappings, int arg_count, int *out_result_count);
```

### 25.2.6 求解工作流

```c
// 完整求解流水线：重写 → 求解器 → 冲突检查 → 自由度更新
EngineSolveResult engine_solve(LV00Context *ctx);

// 重写-求解协作：先重写 → 遇停顿则求解 → 冲突暴露
int engine_rewrite_and_solve(LV00Context *ctx, 
    int max_rewrite_steps, int max_solve_steps);
```

**求解流水线**：
1. **重写阶段**：应用重写规则简化约束
2. **求解阶段**：调用代数求解器（Gröbner/SMT）
3. **冲突检查**：检测约束矛盾
4. **自由度更新**：更新未约束变量的自由度

### 25.2.7 位电路跳闸处理

当符号计算精度超限时触发熔断器：

```c
typedef enum {
    ENGINE_CIRCUIT_ACTION_IGNORE,   // 忽略，接受当前结果
    ENGINE_CIRCUIT_ACTION_ROLLBACK, // 回滚到冻结点快照
    ENGINE_CIRCUIT_ACTION_DOWNGRADE // 永久降级为 AMBER
} EngineCircuitAction;

EngineCircuitResult engine_handle_circuit_trip(LV00Context *ctx);
EngineCircuitResult engine_handle_circuit_trip_with_action(
    LV00Context *ctx, EngineCircuitAction action);
```

### 25.2.8 冻结点快照机制

```c
void *engine_create_frozen_point(LV00Context *ctx);
bool engine_restore_frozen_point(LV00Context *ctx, void *frozen_point);
void engine_destroy_frozen_point(void *frozen_point);
```

**用途**：位电路跳闸后的状态回滚，支持撤销到之前的约束图状态。

### 25.2.9 状态机 API

```c
EngineStatus lv00_engine_transition_state(LV00Context *ctx, EngineState new_state);
EngineState engine_get_state(const LV00Context *ctx);
bool engine_is_busy(const LV00Context *ctx);
const char *engine_state_name(EngineState state);
bool engine_is_valid_transition(EngineState from, EngineState to);
```

### 25.2.10 流式输出集成

```c
StreamContext *engine_get_stream_context(const LV00Context *ctx);
void engine_set_streaming_enabled(LV00Context *ctx, bool enabled);
bool engine_is_streaming_enabled(const LV00Context *ctx);
void engine_emit_stream_event(LV00Context *ctx, StreamEventType type,
    const char *description, int step_number, int node_id, int constraint_id);
```

---

## 25.3 engine_scheduler.h —— 多引擎调度框架

### 25.3.1 设计定位

参考 **polymake** 的多后端架构，实现：
- 后端注册表管理
- 约束图特征分析
- 自动路由决策
- 分发求解
- 回退链机制

### 25.3.2 图特征分析

```c
typedef struct GraphFeatures {
    // 基本统计
    int total_nodes;
    int total_constraints;
    
    // 变量相关
    int variable_nodes;  // 可求解变量节点数
    int fixed_nodes;     // 已确定坐标的节点数
    int port_nodes;      // 端口节点数
    int block_nodes;     // 函数块节点数
    
    // 约束类型分布
    int incidence_constraints;
    int betweenness_constraints;
    int intersection_constraints;
    int containment_constraints;
    int connection_constraints;
    
    // 非线性特征
    int nonlinear_constraints;
    double nonlinear_ratio;  // 非线性约束占比
    
    // 量词特征
    bool has_quantifier_like;   // 含类量词约束
    bool has_boolean_variables; // 含布尔变量
    
    // 规模估计
    int estimated_equation_count;
    int estimated_degree_max;
    
    int64_t analysis_time_us;
} GraphFeatures;

int scheduler_analyze_graph(const ConstraintGraph *graph, GraphFeatures *features);
```

### 25.3.3 自动路由规则

**标准路由规则**（按优先级升序）：

| 优先级 | 规则名称 | 条件 | 目标后端 |
|--------|----------|------|----------|
| 0 | quantifier-cvc5 | 含类量词约束 + cvc5 可用 | SMT_CVC5 |
| 10 | nonlinear-smt | 非线性占比 ≥ 0.3 + 有 SMT 可用 | 最佳 SMT |
| 20 | small-groebner | 变量数 < 50 | GROEBNER |
| 30 | large-smt | 变量数 ≥ 50 + 非线性 > 0 | 最佳 SMT |
| 100 | default-groebner | 无条件 | GROEBNER |

```c
typedef struct RoutingRule {
    char name[64];
    int priority;                     // 数值越低越优先
    bool enabled;
    RouteCondition conditions[4];
    int condition_count;
    RouteCombineMode combine_mode;    // AND / OR
    SolverBackendType target_backend;
} RoutingRule;
```

**条件类型**：
```c
typedef enum {
    ROUTE_COND_NONE,              // 无条件
    ROUTE_COND_VAR_COUNT_LE,      // 变量数 <= 阈值
    ROUTE_COND_VAR_COUNT_GE,      // 变量数 >= 阈值
    ROUTE_COND_NONLINEAR_RATIO_GE, // 非线性占比 >= 阈值
    ROUTE_COND_HAS_QUANTIFIER,    // 含类量词约束
    ROUTE_COND_HAS_BOOLEAN,       // 含布尔变量
    ROUTE_COND_DEGREE_GE,         // 最高度数 >= 阈值
    ROUTE_COND_BACKEND_AVAILABLE  // 指定后端可用
} RouteConditionType;
```

### 25.3.4 调度器核心 API

```c
// 生命周期
EngineScheduler *scheduler_create(void);
void scheduler_destroy(EngineScheduler *scheduler);
void scheduler_reset(EngineScheduler *scheduler);

// 后端注册
int scheduler_register_backend(EngineScheduler *scheduler,
    SolverBackendType type, SMTSolverCreateFunc create_func,
    int priority, const char *description);
int scheduler_unregister_backend(EngineScheduler *scheduler, SolverBackendType type);
int scheduler_list_available_backends(const EngineScheduler *scheduler,
    SolverBackendType *out_types, int max_count);

// 路由规则管理
int scheduler_add_routing_rule(EngineScheduler *scheduler, RoutingRule *rule);
int scheduler_remove_routing_rule(EngineScheduler *scheduler, const char *name);
int scheduler_load_preset_rules(EngineScheduler *scheduler);

// 后端选择
SolverBackendType scheduler_select_backend(const EngineScheduler *scheduler,
    const ConstraintGraph *graph, char *out_reason, size_t reason_size);
```

### 25.3.5 分发求解

```c
// 自动选择后端并求解
int scheduler_solve(EngineScheduler *scheduler, 
    const ConstraintGraph *graph, SMTSolverResult *out_result);

// 使用指定后端求解
int scheduler_solve_with_backend(EngineScheduler *scheduler,
    const ConstraintGraph *graph, SolverBackendType backend_type,
    SMTSolverResult *out_result);

// Gröbner 兼容接口
int scheduler_solve_groebner_compat(EngineScheduler *scheduler,
    const ConstraintGraph *graph, GroebnerResult **out_result);
```

**故障处理**：
1. 首选后端不可用 → 按 `enable_fallback` 决定是否尝试回退链
2. 回退链按优先级依次尝试
3. 所有回退均失败 → 返回错误

### 25.3.6 调度器配置

```c
void scheduler_set_default_backend(EngineScheduler *scheduler, SolverBackendType type);
void scheduler_set_fallback_policy(EngineScheduler *scheduler, bool enable,
    const SolverBackendType *fallback_types, int depth);
void scheduler_set_auto_create(EngineScheduler *scheduler, bool auto_create);
```

**默认配置**：
- `default_backend` = GROEBNER
- `enable_fallback` = true
- `auto_create_backends` = true
- `fallback_chain` = [GROEBNER]（深度 1）

### 25.3.7 统计与诊断

```c
typedef struct SchedulerStats {
    int64_t total_solves;
    int64_t total_solve_time_us;
    int64_t max_solve_time_us;
    int64_t fallback_count;
    int64_t selection_miss;
    int backend_solve_counts[COUNT];
} SchedulerStats;

void scheduler_get_stats(const EngineScheduler *scheduler, SchedulerStats *stats);
void scheduler_reset_stats(EngineScheduler *scheduler);
int scheduler_diagnose(const EngineScheduler *scheduler, char *buf, size_t buf_size);
```

---

## 25.4 代码-理论对应关系

| 代码概念 | 理论对应 | 文档位置 |
|----------|----------|----------|
| `LV00Context` | 中央调度器/协调器 | 本文档 25.2.4 |
| `EngineState` | 有限状态机 | 本文档 25.2.3 |
| `frozen_point` | 检查点/快照 | 本文档 25.2.8 |
| `GraphFeatures` | 问题特征向量 | 本文档 25.3.2 |
| `RoutingRule` | 启发式路由策略 | 本文档 25.3.3 |
| `scheduler_solve()` | 自动算法选择 | 本文档 25.3.5 |
| `fallback_chain` | 故障恢复策略 | 本文档 25.3.6 |

---

## 25.5 相关模块文档

| 文档 | 关联内容 |
|------|----------|
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图核心 |
| [03_normalization.md](03_normalization.md) | 图规范化 |
| [04_solver.md](04_solver.md) | 符号代数求解器 |
| [05_rewrite.md](05_rewrite.md) | 图重写引擎 |
| [14_solver_backends.md](14_solver_backends.md) | SMT/ATP 后端 |
| [ARCHITECTURE_v3.3.md](ARCHITECTURE_v3.3.md) | 五层架构 |

---

## 25.6 版本历史

- **v3.5.0** (当前)
  - 五状态机形式化
  - 层级验证（编译时 + 运行时）
  - 流式输出集成

- **v3.4.0**
  - 位电路跳闸处理
  - 冻结点快照机制
  - 重写-求解协作协议

- **v3.3.0**
  - 多引擎调度框架
  - 自动路由决策
  - 回退链机制
