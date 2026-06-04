# 上下文系统与运行时安全基础设施 (Context System and Runtime Safety Infrastructure)

## 模块概述

上下文系统与运行时安全基础设施是 Lv-00 十层架构中的**跨层基础设施**，为所有上层模块提供统一的状态管理、资源保护和并发安全保障。本组模块涵盖以下五个头文件：

| 模块 | 头文件 | 职责 |
|------|--------|------|
| 隔离上下文系统 | `context.h` | 统一状态容器、分支推理与熔断机制 |
| 独立熔断器 | `circuit_breaker.h` | 熔断器核心操作：检查、跳闸、重置 |
| 运行时数据保护 | `runtime_guard.h` | 读写锁宏、原子操作与数据完整性校验 |
| 统一返回状态码 | `status_codes.h` | 标准化函数返回类型与状态码 |
| 几何节点深拷贝 | `node_deep_copy.h` | 节点、端口、符号坐标的深拷贝公共接口 |

在 Lv-00 十层架构中的定位：

```
第 5 层：应用层（DSL / REPL / GUI）
第 4 层：推理层（证明 / 重写 / 归纳）  ← 上下文系统贯穿此层
第 3 层：几何层（约束图 / 归一化）    ← 深拷贝服务于此层
第 2 层：资源层（符号坐标 / 类型系统） ← 运行时保护贯穿此层
第 1 层：平台层（内存 / IO / 并发）    ← 状态码与运行时守卫基础
```

## 核心设计原则

1. **上下文隔离**：每个几何问题的求解过程拥有独立的上下文实例，消除全局状态竞争
2. **资源保护**：多维熔断器防止单次计算失控，运行时守卫保障数据并发安全
3. **状态可恢复**：完整的快照/回滚机制支持分支推理的任意回退
4. **零开销抽象**：运行时保护通过编译期开关控制，禁用时所有宏展开为空操作
5. **统一接口**：所有公共 API 返回标准化的 `Lv00Status` 类型

---

## 1. context.h：隔离上下文系统

`context.h` 是 Lv-00 项目的**最高优先级架构修复**，定义了 `Lv00Context` 结构体作为状态管理的中心容器。它将当前分散在全局/线程局部变量和 `LV00Engine` 中的状态统一收容到一个隔离的上下文容器中。

### 1.1 上下文状态机

`Lv00ContextState` 枚举定义了上下文从创建到销毁的完整生命周期：

```
IDLE ──→ PARSING ──→ REASONING ──→ COMPLETE ──→ (reset) ──→ IDLE
  │                      │
  └──────────────────────┼──→ ERROR ──→ (reset) ──→ IDLE
                         │
                         └──→ IDLE (取消/中断)
```

```c
typedef enum {
    LV00_CONTEXT_IDLE = 0,       /* 空闲状态 */
    LV00_CONTEXT_PARSING,        /* 解析状态 */
    LV00_CONTEXT_REASONING,      /* 推理状态 */
    LV00_CONTEXT_ERROR,          /* 错误状态 */
    LV00_CONTEXT_COMPLETE        /* 完成状态 */
} Lv00ContextState;
```

状态转移规则（严格）：

| 当前状态 | 可转移至 | 触发条件 |
|---------|---------|---------|
| IDLE | PARSING | 开始输入 |
| IDLE | ERROR | 初始化失败 |
| PARSING | REASONING | 解析完成 |
| PARSING | ERROR | 解析失败 |
| PARSING | IDLE | 取消/中断 |
| REASONING | COMPLETE | 推理成功 |
| REASONING | ERROR | 推理失败/熔断 |
| REASONING | IDLE | 取消/中断 |
| COMPLETE | IDLE | 通过 reset |
| ERROR | IDLE | 通过 reset |

### 1.2 Lv00Context 结构体（15 字段分组）

`Lv00Context` 包含以下 15 个功能分组：

| 编号 | 分组 | 字段 | 说明 |
|------|------|------|------|
| 1 | 几何对象容器 | `main_graph` | 约束图核心容器（点、线段、区域、端口、函数块、约束） |
| 2 | AST 语法树 | `ast_root` | 已解析输入的抽象语法树根节点 |
| 3 | 代数计算缓存 | `groebner_cache`, `symbolic_cache`, `numeric_cache`, `unification_cache`, `cache_valid`, `cache_hits`, `cache_misses` | Groebner 基、符号化简、数值近似、合一绑定表的缓存 |
| 4 | 推理分支栈 | `reasoning_stack` | 多路径推理的状态管理栈 |
| 5 | 运行时参数 | `name`, `error_code`, `error_message`, `last_status` | 上下文名称、错误码、错误描述、最后状态码 |
| 6 | 状态机 | `state`, `previous_state`, `state_transition_count` | 当前状态、上一状态、转移次数 |
| 7 | 熔断器 | `circuit_breaker` | 多维资源保护（时间/深度/次数/内存/错误） |
| 8 | 递归深度追踪 | `recursion_depth`, `max_recursion_depth`, `recursion_policy` | 递归深度计数与处理策略 |
| 9 | 流式输出 | `stream_ctx` | 向前端/日志系统发射实时事件 |
| 10 | 规范化结果 | `last_normalization` | 最近一次图归一化操作的结果 |
| 11 | 内存池 | `memory_pool`, `mem_stats` | 高效小对象分配的内存池 |
| 12 | 公理与规则 | `module_refs`, `axiom_pkg_refs`, `rewrite_rule_refs`, `rewrite_step_limit` | 共享模块/公理/规则注册表引用 |
| 13 | 快照/回滚 | `snapshot_refcount`, `parent_snapshot`, `snapshot_depth` | 通用状态保存/恢复机制 |
| 14 | 线程安全 | `mutex`, `graph_rwlock`, `cache_lock`（可选编译） | 上下文级互斥锁与读写锁 |
| 15 | 统计信息 | `context_id`, `created_at_us`, `problems_processed`, `user_extension` | 唯一 ID、创建时间、已处理问题数、扩展指针 |

### 1.3 ReasoningFrame 推理栈

推理过程中，前向证明、反证法、假设引入等操作会创建推理分支。每个分支帧 `ReasoningFrame` 记录进入该分支时的完整状态快照。

**推理分支类型**：

```c
typedef enum {
    REASONING_BRANCH_NONE,          /* 无分支（主推理线） */
    REASONING_BRANCH_FORWARD,       /* 前向证明分支 */
    REASONING_BRANCH_CONTRADICTION, /* 反证法分支 */
    REASONING_BRANCH_HYPOTHESIS     /* 假设引入分支 */
} ReasoningBranchType;
```

**推理栈帧结构**：

```c
typedef struct ReasoningFrame {
    ReasoningBranchType branch_type;    /* 分支类型 */
    ReasoningBranchStatus status;       /* 分支状态 */
    int depth;                          /* 推理深度 */
    int step_count;                     /* 推理步骤计数 */
    struct ConstraintGraph *graph_snapshot; /* 约束图深拷贝快照 */
    int *assumption_node_ids;           /* 假设节点 ID 数组 */
    int assumption_count;               /* 假设数量 */
    int *target_node_ids;               /* 目标节点 ID 数组 */
    int target_count;                   /* 目标数量 */
    void *ast_root_ref;                 /* AST 语法树引用 */
    uint64_t created_at_us;             /* 创建时间戳 */
    uint64_t timeout_ms;                /* 独立超时时间 */
    void *user_data;                    /* 用户扩展数据 */
} ReasoningFrame;
```

### 1.4 CircuitBreaker 多维熔断器

每个 `Lv00Context` 内建一个 `CircuitBreaker`，监控五个维度的资源指标：

| 维度 | 字段 | 默认阈值 | 说明 |
|------|------|---------|------|
| 时间熔断 | `timeout_ms`, `total_timeout_ms` | 30000 ms | 单次操作超时 / 总运行时间超限 |
| 深度熔断 | `current_depth`, `max_depth` | 100 | 递归/重写/推理嵌套深度超限 |
| 次数熔断 | `total_steps`, `max_steps` | 1000000 | 推理步骤总数超限 |
| 内存熔断 | `max_memory_bytes` | 0（不限制） | 内存使用超限 |
| 错误熔断 | `consecutive_errors`, `max_consecutive_errors` | 10 | 连续错误次数超限 |

熔断器状态机：

```
CLOSED ── 错误次数超阈值 ──→ OPEN
  ↑                            │
  │                            │ 冷却时间过后
  │                            ↓
  └── 试探成功 ←── HALF_OPEN ←─┘
```

### 1.5 默认常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `LV00_CONTEXT_DEFAULT_TIMEOUT_MS` | 30000 | 默认熔断超时（30 秒） |
| `LV00_CONTEXT_DEFAULT_MAX_DEPTH` | 100 | 默认递归/推理深度上限 |
| `LV00_CONTEXT_MAX_RECURSION_DEPTH` | 10000 | 递归深度绝对硬上限 |
| `LV00_CONTEXT_DEFAULT_MAX_STEPS` | 1000000 | 默认最大推理步骤数 |
| `LV00_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS` | 10 | 默认连续错误上限 |
| `LV00_CONTEXT_DEFAULT_COOLDOWN_MS` | 5000 | 熔断器默认冷却时间 |
| `LV00_CONTEXT_REASONING_STACK_DEFAULT_CAPACITY` | 8 | 推理栈默认初始容量 |
| `LV00_CONTEXT_REASONING_STACK_MAX_DEPTH` | 1000 | 推理栈最大深度上限 |

### 1.6 主要 API

#### 生命周期管理

| 函数 | 说明 |
|------|------|
| `lv00_context_create()` | 创建并初始化新的隔离上下文 |
| `lv00_context_destroy(ctx)` | 销毁上下文，释放所有关联资源 |
| `lv00_context_reset(ctx)` | 重置上下文到初始状态，准备下一个问题 |
| `lv00_context_snapshot(ctx)` | 创建当前上下文的完整深拷贝快照 |
| `lv00_context_rollback(ctx, snapshot)` | 将上下文状态回滚到指定快照 |

#### 状态机管理

| 函数 | 说明 |
|------|------|
| `lv00_context_get_state(ctx)` | 获取上下文当前状态 |
| `lv00_context_set_state(ctx, new_state)` | 尝试转移到指定状态（验证合法性） |
| `lv00_context_state_name(state)` | 获取状态的可读字符串名称 |
| `lv00_context_state_transition_valid(from, to)` | 检查状态转移是否合法 |

#### 推理栈管理

| 函数 | 说明 |
|------|------|
| `lv00_context_push_reasoning(ctx, branch_type, timeout_ms)` | 压入推理分支帧 |
| `lv00_context_pop_reasoning(ctx)` | 弹出栈顶帧（分支闭合） |
| `lv00_context_get_reasoning_depth(ctx)` | 获取当前推理栈深度 |
| `lv00_context_get_current_reasoning_frame(ctx)` | 获取当前活跃的推理分支帧 |

#### 熔断器操作

| 函数 | 说明 |
|------|------|
| `lv00_context_is_circuit_open(ctx)` | 检查熔断器是否已触发 |
| `lv00_context_begin_operation(ctx)` | 开始一次可熔断操作 |
| `lv00_context_check_timeout(ctx)` | 检查当前操作是否超时 |
| `lv00_context_enter_uncancellable(ctx)` | 进入不可取消区域 |
| `lv00_context_leave_uncancellable(ctx)` | 离开不可取消区域 |
| `lv00_context_record_step(ctx)` | 记录一次推理步骤 |
| `lv00_context_record_success(ctx)` | 记录一次成功操作 |
| `lv00_context_record_error(ctx)` | 记录一次错误操作 |

#### 参数配置

| 函数 | 说明 |
|------|------|
| `lv00_context_set_timeout(ctx, timeout_ms)` | 设置超时时间 |
| `lv00_context_set_max_depth(ctx, max_depth)` | 设置深度上限 |
| `lv00_context_set_max_steps(ctx, max_steps)` | 设置最大步骤数 |
| `lv00_context_set_name(ctx, name)` | 设置上下文名称 |
| `lv00_context_get_id(ctx)` | 获取上下文唯一 ID |

#### 缓存管理

| 函数 | 说明 |
|------|------|
| `lv00_context_invalidate_cache(ctx)` | 标记所有缓存为无效 |
| `lv00_context_is_cache_valid(ctx)` | 检查缓存是否有效 |
| `lv00_context_clear_cache(ctx)` | 清除所有缓存内容 |

#### 错误管理

| 函数 | 说明 |
|------|------|
| `lv00_context_set_error(ctx, code, fmt, ...)` | 设置错误状态（变参格式） |
| `lv00_context_clear_error(ctx)` | 清除错误状态 |
| `lv00_context_get_error_code(ctx)` | 获取错误码 |
| `lv00_context_get_error_message(ctx)` | 获取错误消息 |

#### 流式输出与统计

| 函数 | 说明 |
|------|------|
| `lv00_context_get_stream(ctx)` | 获取流式输出上下文 |
| `lv00_context_set_streaming_enabled(ctx, enabled)` | 设置流式输出启用状态 |
| `lv00_context_get_stats(ctx, buf, buf_size)` | 获取统计信息摘要 |
| `lv00_context_get_uptime_us(ctx)` | 获取运行时间（微秒） |

---

## 2. circuit_breaker.h：独立熔断器模块

`circuit_breaker.h` 提供熔断器的核心操作函数，独立于上下文结构体。`CircuitBreaker` 结构体在 `context.h` 中定义，本模块提供其操作实现。

### 2.1 状态机

```
CLOSED ── 错误次数超阈值 ──→ OPEN
  ↑                            │
  │                            │ 冷却时间过后
  │                            ↓
  └── 试探成功 ←── HALF_OPEN ←─┘
```

### 2.2 核心 API

| 函数 | 说明 |
|------|------|
| `lv00_circuit_breaker_check(ctx)` | 检查熔断器状态，判断是否可执行操作。CLOSED 正常返回 true；OPEN 检查冷却时间，冷却完成自动进入 HALF_OPEN |
| `lv00_circuit_breaker_trip(ctx, reason)` | 触发熔断器跳闸，状态设为 OPEN，记录原因和时间 |
| `lv00_circuit_breaker_reset(ctx)` | 重置熔断器到 CLOSED 状态，清除所有错误计数 |
| `lv00_circuit_breaker_record_success(ctx)` | 记录成功操作。HALF_OPEN 下恢复 CLOSED；CLOSED 下重置错误计数 |
| `lv00_circuit_breaker_record_failure(ctx)` | 记录失败操作。HALF_OPEN 下重新设为 OPEN；CLOSED 下递增错误计数，超限则跳闸 |
| `lv00_circuit_breaker_state_name(ctx)` | 获取熔断器状态的可读名称 |
| `lv00_circuit_breaker_summary(ctx, buf, buf_size)` | 获取熔断器健康摘要 |
| `lv00_circuit_breaker_uptime_us(cb)` | 获取运行时间（微秒） |
| `lv00_circuit_breaker_now_us()` | 获取当前微秒级时间戳 |

---

## 3. runtime_guard.h：运行时数据保护

`runtime_guard.h` 提供编译期可选的运行时保护机制，用于在多线程或无锁环境下保护 Lv-00 关键数据结构的并发访问和完整性。

### 3.1 编译期开关

通过 `LV00_ENABLE_RUNTIME_GUARDS` 宏控制启用/禁用：

```bash
cmake -DLV00_ENABLE_RUNTIME_GUARDS=ON ..
gcc -DLV00_ENABLE_RUNTIME_GUARDS ...
```

禁用时（默认），所有宏展开为空操作，零性能开销。

### 3.2 配置常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `LV00_RUNTIME_GUARD_MAX_RECURSE` | 128 | 默认递归深度上限 |
| `LV00_RUNTIME_GUARD_SPIN_ATTEMPTS` | 1024 | 自旋锁最大尝试次数 |
| `LV00_RUNTIME_GUARD_READ_WARN_US` | 5000 | 读锁持有最大时间警告阈值（微秒） |
| `LV00_RUNTIME_GUARD_WRITE_WARN_US` | 10000 | 写锁持有最大时间警告阈值（微秒） |

### 3.3 数据结构

**运行时保护统计**：

```c
typedef struct Lv00GuardStats {
    uint64_t lock_acquired_count;   /* 锁成功获取次数 */
    uint64_t lock_contention_count; /* 锁争用次数 */
    uint64_t lock_timeout_count;    /* 锁超时次数 */
    uint64_t read_guard_count;      /* 读守卫进入次数 */
    uint64_t write_guard_count;     /* 写守卫进入次数 */
    uint64_t integrity_checks;      /* 数据完整性校验次数 */
    uint64_t integrity_failures;    /* 数据完整性校验失败次数 */
    uint64_t deadlock_warnings;     /* 死锁警告次数 */
    uint64_t max_read_hold_us;      /* 最大读锁持有时间 */
    uint64_t max_write_hold_us;     /* 最大写锁持有时间 */
} Lv00GuardStats;
```

**运行时守卫上下文**：

```c
typedef struct Lv00GuardContext {
    Lv00RwLock ctx_rwlock;      /* 上下文级读写锁 */
    Lv00Mutex stat_mutex;       /* 统计信息互斥锁 */
    Lv00GuardStats stats;       /* 运行时保护统计 */
    bool initialized;           /* 是否已初始化 */
} Lv00GuardContext;
```

### 3.4 读写锁宏

| 宏 | 说明 |
|----|------|
| `LV00_RUNTIME_LOCK(ctx)` | 获取上下文运行时写锁（独占） |
| `LV00_RUNTIME_UNLOCK(ctx)` | 释放上下文运行时写锁 |
| `LV00_READ_GUARD(ctx)` | 获取读守卫（共享读锁） |
| `LV00_READ_UNGUARD(ctx)` | 释放读守卫 |
| `LV00_WRITE_GUARD(ctx)` | 获取写守卫（独占写锁） |
| `LV00_WRITE_UNGUARD(ctx)` | 释放写守卫 |
| `LV00_GUARDED_SECTION(ctx)` | RAII 风格保护区入口 |

### 3.5 原子操作宏

| 宏 | 说明 |
|----|------|
| `LV00_ATOMIC_INC(var)` | 原子递增（32 位） |
| `LV00_ATOMIC_DEC(var)` | 原子递减（32 位） |
| `LV00_ATOMIC_ADD(var, n)` | 原子加法（32 位） |
| `LV00_ATOMIC_LOAD(var)` | 原子加载（32 位） |
| `LV00_ATOMIC_STORE(var, n)` | 原子存储（32 位） |
| `LV00_ATOMIC_CAS(var, expected, desired)` | 原子比较并交换（32 位） |
| `LV00_ATOMIC_INC64(var)` | 原子递增（64 位） |
| `LV00_ATOMIC_ADD64(var, n)` | 原子加法（64 位） |

支持三种后端实现：C11 `<stdatomic.h>`、GCC/Clang 内建原子操作、MSVC `Interlocked*` 系列函数。

### 3.6 数据完整性校验

`lv00_verify_data_integrity(ctx)` 执行以下检查：

1. 上下文状态有效性（不超过 `LV00_CONTEXT_COMPLETE`）
2. 约束图主指针非空（若 state >= PARSING）
3. 推理栈深度不超过上限
4. 递归深度不超过上限
5. 熔断器状态与上下文状态一致

### 3.7 安全断言宏

| 宏 | 说明 |
|----|------|
| `LV00_ASSERT_RUNTIME(ctx, cond, retval)` | 运行时条件下断言：条件为假时记录错误并返回指定值 |

### 3.8 核心 API

| 函数 | 说明 |
|------|------|
| `lv00_guard_init(guard)` | 初始化运行时守卫上下文 |
| `lv00_guard_destroy(guard)` | 销毁运行时守卫上下文 |
| `lv00_guard_get_stats(guard, stats)` | 获取统计信息快照 |
| `lv00_guard_reset_stats(guard)` | 重置统计信息 |
| `lv00_verify_data_integrity(ctx)` | 数据完整性校验 |

---

## 4. status_codes.h：统一返回状态码

`status_codes.h` 提供标准化的函数返回类型 `Lv00Status` 及对应的状态码宏，确保所有公共 API 函数接口一致。

### 4.1 返回类型

```c
typedef int Lv00Status;
```

值为 0 表示成功，非 0 表示错误。

### 4.2 状态码定义

**通用错误 (1-19)**：

| 状态码 | 值 | 说明 |
|--------|-----|------|
| `LV00_OK` | 0 | 操作成功 |
| `LV00_ERR_MEMORY` | 1 | 内存分配失败 |
| `LV00_ERR_INVALID_ARG` | 2 | 无效参数 |
| `LV00_ERR_NOT_FOUND` | 3 | 未找到指定资源 |
| `LV00_ERR_ALREADY_EXISTS` | 4 | 资源已存在 |
| `LV00_ERR_UNSUPPORTED` | 5 | 操作不支持 |
| `LV00_ERR_TIMEOUT` | 6 | 操作超时 |
| `LV00_ERR_INTERNAL` | 7 | 内部错误 |
| `LV00_ERR_INVALID_STATE` | 8 | 无效状态 |
| `LV00_ERR_OVERFLOW` | 9 | 数值越界/溢出 |
| `LV00_ERR_IO` | 10 | IO 错误 |
| `LV00_ERR_PARSE` | 11 | 解析错误 |

**约束图错误 (20-29)**：

| 状态码 | 值 | 说明 |
|--------|-----|------|
| `LV00_ERR_NODE_CONFLICT` | 20 | 节点冲突 |
| `LV00_ERR_CONSTRAINT_CONFLICT` | 21 | 约束冲突 |
| `LV00_ERR_CONSTRAINT_DUPLICATE` | 22 | 重复约束 |
| `LV00_ERR_INVALID_REGION` | 23 | 无效区域 |
| `LV00_ERR_CYCLIC_DEPENDENCY` | 24 | 循环依赖 |

**求解器错误 (30-39)**：

| 状态码 | 值 | 说明 |
|--------|-----|------|
| `LV00_ERR_SOLVER_NO_SOLUTION` | 30 | 无解 |
| `LV00_ERR_SOLVER_INFINITE` | 31 | 无穷多解 |
| `LV00_ERR_SOLVER_OVERCONSTRAINED` | 32 | 过度约束 |
| `LV00_ERR_GROEBNER_FAILED` | 33 | Groebner 基计算失败 |

**合一检查错误 (40-49)**：

| 状态码 | 值 | 说明 |
|--------|-----|------|
| `LV00_ERR_UNIFY_FAILED` | 40 | 合一失败 |
| `LV00_ERR_UNIFY_TYPE_MISMATCH` | 41 | 类型不匹配 |

**证明系统错误 (50-59)**：

| 状态码 | 值 | 说明 |
|--------|-----|------|
| `LV00_ERR_PROOF_INVALID` | 50 | 无效证明 |
| `LV00_ERR_PROOF_INCOMPLETE` | 51 | 证明不完整 |
| `LV00_ERR_PROOF_VERIFY_FAILED` | 52 | 证明验证失败 |

**函数块错误 (60-69)**：

| 状态码 | 值 | 说明 |
|--------|-----|------|
| `LV00_ERR_FUNC_BLOCK_INVALID` | 60 | 无效函数块 |
| `LV00_ERR_FUNC_BLOCK_NON_DET` | 61 | 非确定性函数块 |

**预设系统错误 (70-79)**：

| 状态码 | 值 | 说明 |
|--------|-----|------|
| `LV00_ERR_PRESET_REGISTER` | 70 | 预设注册失败 |
| `LV00_ERR_PRESET_INSTANTIATE` | 71 | 预设实例化失败 |

### 4.3 辅助函数

| 函数 | 说明 |
|------|------|
| `lv00_status_to_string(status)` | 获取状态码的描述字符串 |
| `lv00_status_is_ok(status)` | 判断状态码是否表示成功 |

### 4.4 与 error_codes.h 的关系

- `error_codes.h` 定义了 `Lv00ErrorCode` 枚举（细粒度错误码，约 100+ 条目）
- `status_codes.h` 定义了 `Lv00Status` 类型（统一的返回类型）和精简状态码宏
- `Lv00ErrorCode` 可隐式转换为 `Lv00Status`（两者均为 `int`）

---

## 5. node_deep_copy.h：几何节点深拷贝公共接口

`node_deep_copy.h` 提供统一的节点和端口深拷贝函数，消除 `engine.c`、`proof.c`、`rewrite.c` 中的重复实现。

### 5.1 所有权语义

| 字段 | 拷贝策略 | 说明 |
|------|---------|------|
| `type_region` | 浅拷贝（指针赋值） | 所有权由 TypeSystem 统一管理 |
| `connected_to` | 置为 NULL | 需调用者通过 ID 映射更新连接关系 |
| `symbolic_coords` | 深拷贝 | 所有权归新节点所有 |

### 5.2 核心 API

| 函数 | 说明 |
|------|------|
| `node_deep_copy_port(orig)` | 深拷贝端口，返回新分配的端口副本 |
| `node_deep_copy_geom_node(orig, id_map)` | 深拷贝几何节点，`id_map` 为旧节点 ID 到新节点 ID 的映射（可为 NULL） |
| `node_deep_copy_symbolic_coord(orig)` | 深拷贝符号坐标，返回新分配的坐标副本 |

---

## 设计原则总结

### 上下文隔离

`Lv00Context` 将所有问题相关的状态封装在单一结构体中，实现：

- **问题间隔离**：不同几何问题的求解过程互不干扰
- **线程间隔离**：每个线程持有独立的上下文实例
- **分支间隔离**：推理分支通过快照/回滚实现状态独立

### 资源保护

多层级的资源保护机制：

- **编译期保护**：`runtime_guard.h` 通过宏提供零开销抽象
- **运行时保护**：读写锁、原子操作、数据完整性校验
- **熔断保护**：五维熔断器（时间/深度/次数/内存/错误）防止单次计算失控

### 状态可恢复

完整的状态保存与恢复机制：

- **快照**：`lv00_context_snapshot()` 创建上下文的完整深拷贝
- **回滚**：`lv00_context_rollback()` 恢复到任意快照状态
- **重置**：`lv00_context_reset()` 清空状态准备下一个问题
- **不可取消区域**：关键路径中阻止超时熔断，确保状态一致性
