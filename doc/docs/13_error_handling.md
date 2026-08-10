# 13. 错误处理系统 (Error Handling System)

## 模块概述

错误处理系统由四个层次构成，共同保证 Lv-00 在长时推理、约束传播与插件加载中的健壮性与可诊断性：

1. **错误码体系（`error_codes.h` / `status_codes.h`）**：以 X-macro `LV_ERROR_CODES_X` 作为单一事实来源，生成 `lvErrorCode` 枚举、错误名、中文消息与类别映射；错误码按模块分层编号（0-999 共 13 个类别区间）。
2. **错误传播（`lv_error.h` + `lv_check.h`）**：线程局部错误上下文（`lvErrorContext`）维护最多 8 帧的错误链（cause 链），配套 `lv_CHECK_*` / `lv_PROPAGATE` / `lv_ERROR_CTX_*` 宏实现"检查即记录即返回"的传播模式。
3. **恢复策略**：`lvResult` 结果类型与 `lv_TRY` 早退传播、`lv_error_clear` 复位、熔断器 `record_success` 复位连续错误计数、`lv_guard_reset_stats` 统计清零。
4. **熔断器（`circuit_breaker.h` / `lv_circuit_breaker.h`）**：三态状态机（CLOSED / HALF_OPEN / OPEN），对时间、深度、步数、连续错误、内存五个维度进行熔断保护，防止错误级联导致系统雪崩。

## 核心设计原则

1. **分层编号可追溯**：错误码按系统（1-99）、内存（100-199）、解析器（130-139）、约束图（200-299）、符号坐标（300-399）、求解器（400-499）、重写（500-599）、合一（600-699）、函数块（700-799）、预设（750-799）、类型系统（800-899）、证明系统（900-999）分段；类别键经 `LV_EC_CAT_SHORT/LONG` 宏单点派生，避免名称双写不同步。
2. **单一事实来源**：错误码仅需在 `LV_ERROR_CODES_X` 追加一行，枚举与查找表由宏展开自动同步，`find_error_info` 的二分查找排序不变量由编译期展开保证。
3. **线程局部错误状态**：`lv_get_last_error_code` / `lv_set_error` 等基于 TLS，错误不跨线程污染；`lv_error.h` 在旧式 TLS 之上叠加 8 帧错误链与 cause 指针（`lv_error_push` 桥接旧写端零改动获得回溯能力）。
4. **宏即边界**：所有检查宏失败时自动记录日志并返回，避免遗漏 `return`；宏内部已内置 `return`（`lv_RETURN_ERROR`），调用处不得重复书写。
5. **熔断而非崩溃**：`lv_CB_*` 默认阈值（超时 30s、深度 100、步数 100 万、连续错误 10、冷却 5s）保证系统在异常路径下主动降级而非悬挂；`uncancellable_refcount` 尊重关键路径不被超时打断。
6. **运行时保护零开销**：`lv_ENABLE_RUNTIME_GUARDS` 未定义时所有守卫宏展开为空操作（`((void)0)`），禁用时 `lv_verify_data_integrity` 恒返回 true。

## 关键数据结构

```c
/* lvResult —— 统一错误传播结果类型 */
typedef struct {
    bool success;                    /* 操作是否成功 */
    lvErrorCode error_code;          /* 错误码（成功时为 lv_OK） */
    char error_message[lv_ERROR_MSG_MAX]; /* 错误描述 */
} lvResult;

/* 错误上下文帧 —— 记录错误发生的位置与原因 */
typedef struct lvErrorFrame {
    const char *file;                /* 源文件名 */
    const char *func;                /* 函数名 */
    int line;
    int code;                        /* 错误码（来自 error_codes.h） */
    char message[lv_ERROR_MSG_MAX];
    struct lvErrorFrame *cause;      /* 原始错误（错误链） */
} lvErrorFrame;

/* 错误上下文（线程局部，自动管理错误链） */
typedef struct lvErrorContext {
    lvErrorFrame frames[lv_ERROR_MAX_FRAMES]; /* 错误帧栈（容量 8） */
    int frame_count;
    int frame_capacity;
    char scratch[lv_ERROR_SCRATCH_SIZE];      /* 临时缓冲区 */
} lvErrorContext;

/* 熔断器（lv_circuit_breaker.h）—— 多维熔断保护 */
typedef struct lvCircuitBreaker {
    lvCircuitBreakerState state;     /* CLOSED / HALF_OPEN / OPEN */
    uint64_t timeout_ms;             /* 时间熔断：单次操作超时 */
    int current_depth;               /* 深度熔断：递归/推理嵌套深度 */
    int max_depth;
    int64_t total_steps;             /* 次数熔断：已执行推理步数 */
    int consecutive_errors;          /* 错误熔断：连续错误计数 */
    size_t max_memory_bytes;         /* 内存熔断：内存上限 */
    uint64_t cooldown_ms;            /* 冷却时间 */
    char *trip_reason;               /* 跳闸原因 */
    int trip_count;                  /* 累计熔断次数 */
} lvCircuitBreaker;

/* 运行时守卫统计（runtime_guard.h） */
typedef struct lvGuardStats {
    uint64_t lock_acquired_count;    /* 锁成功获取次数 */
    uint64_t lock_contention_count;  /* 锁争用次数 */
    uint64_t integrity_checks;       /* 数据完整性校验次数 */
    uint64_t integrity_failures;     /* 完整性校验失败次数 */
    uint64_t deadlock_warnings;      /* 死锁警告次数 */
} lvGuardStats;
```

错误码分层示意（`LV_ERROR_CODES_X`）：`lv_OK`(0) → 系统 `lv_ERROR_INVALID_PARAM`(2)、`lv_ERROR_IO`(12) → 内存 `lv_ERROR_OUT_OF_MEMORY`(100) → 解析器 `lv_ERROR_PARSER_DEPTH_EXCEEDED`(135) → 约束图 `lv_ERROR_CYCLIC_DEPENDENCY`(206) → 求解器 `lv_ERROR_SOLVER_NO_SOLUTION`(400) → 重写 `lv_ERROR_REWRITE_DEPTH`(502) → 证明系统 `lv_ERROR_CIRCUIT_OPEN`(903)。

## 主要接口

### 错误码查询与 TLS 错误状态

| 函数 | 签名要点 | 说明 |
|------|----------|------|
| `lv_error_string` / `lv_error_name` | `const char *(lvErrorCode)` | 错误中文消息 / 短名称 |
| `lv_error_category` | `const char *(lvErrorCode)` | 错误类别短名 |
| `lv_error_is_unknown` | `bool (lvErrorCode)` | 是否未收录于规范错误表 |
| `lv_error_code_from_string` | `lvErrorCode (const char *)` | 名称反向查找错误码 |
| `lv_error_table_validate` | `bool (void)` | debug 下验证错误表升序排序不变量 |
| `lv_get_last_error_code` / `lv_get_last_error_message` | TLS 读取 | 当前线程最后错误 |
| `lv_set_error` / `lv_set_error_ctx` | `void (code, ...)` | 设置线程错误（含文件/行/函数） |
| `lv_clear_error` | `void (void)` | 清除线程错误 |
| `lv_status_is_success` / `lv_status_is_error` / `lv_status_message` | `int`/`const char *` | status_codes.h 状态查询 |

### 检查与传播宏

| 宏 | 语义 | 说明 |
|----|------|------|
| `lv_CHECK_NOT_NULL(ptr)` | 空指针即返回 | 内置 `lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, ...)` |
| `lv_CHECK_ARG` / `lv_CHECK_STATE` / `lv_CHECK_BOUNDS` | 条件/状态/边界检查 | 失败记录日志并返回 |
| `lv_CHECK_ALLOC(ptr, ret)` | 分配检查 | 失败记 OOM 并返回 `ret` |
| `lv_PROPAGATE(call, code, fmt, ...)` | 调用非 0 即返回 | 包装下层函数错误传播 |
| `lv_TRY(expr)` / `lv_RESULT_OK` / `lv_RESULT_ERROR` | lvResult 早退传播 | `!success` 时直接 `return _r` |
| `lv_ERROR_RETURN` / `lv_RETURN_ERROR` / `_NULL` / `_BOOL` / `_VAL` | 设置并返回 | 统一经 `lv_set_error_ctx` 记录位置 |
| `lv_ERROR_CTX_RETURN` / `_NULL` / `_NEG1` / `lv_ERROR_CTX_WRAP` | 帧栈写入 | 经 `lv_error_set_at` 携带 `__FILE__/__LINE__/__func__` |

### 错误上下文帧（lv_error.h）

| 函数 | 签名要点 | 说明 |
|------|----------|------|
| `lv_error_context_current` | `lvErrorContext *(void)` | 获取线程错误上下文（延迟初始化） |
| `lv_error_set` / `lv_error_set_with_cause` / `lv_error_set_at` | `bool (...)` | 压入错误帧（可携带 cause） |
| `lv_error_push` | `bool (lvErrorCode, const char*, int, const char*, const char*)` | 旧式体系桥接入口 |
| `lv_error_code` / `lv_error_message` / `lv_error_cause` | 读取帧顶 | 当前错误码/消息/原因 |
| `lv_error_format_chain` | `char *(lvErrorContext*)` | 生成含错误链的完整信息（调用者 `lv_free`） |
| `lv_error_clear` / `lv_error_has_error` | 管理 | 复位/查询错误上下文 |
| `lv_ERROR_SLOT_SET` / `_WRITE` / `_COPY` / `_CLEAR` | 宏 | 模块级 last_error 缓冲统一写入口 |

### 熔断器（独立 + 上下文级）

| 函数 | 签名要点 | 说明 |
|------|----------|------|
| `lv_circuit_breaker_init` / `_reset` | `void (lvCircuitBreaker*)` | 初始化/复位（保留 trip_count） |
| `lv_circuit_breaker_is_tripped` | `bool (const lvCircuitBreaker*)` | 综合各维度判定是否跳闸 |
| `lv_circuit_breaker_check_guarded` | `bool (lvCircuitBreaker*)` | 维度超限自动跳闸，冷却后迁 HALF_OPEN |
| `lv_circuit_breaker_record_error` / `_record_success` | `bool`/`void` | 错误计数递增 / 成功复位（HALF_OPEN 恢复 CLOSED） |
| `lv_circuit_breaker_do_trip` | `void (lvCircuitBreaker*, const char *reason)` | 显式跳闸并记录原因 |
| `lv_circuit_breaker_check` | `bool (struct lvContext*)` | 上下文级：OPEN 且冷却中返回 false |
| `lv_circuit_breaker_trip` / `_record_failure` | `void`/`bool (struct lvContext*)` | 上下文级跳闸/失败记录 |
| `lv_circuit_breaker_state_name` / `_summary` | `const char *`/`int (ctx, buf, size)` | 状态名称 / 健康摘要 |

### 运行时保护（runtime_guard.h）

| 接口 | 说明 |
|------|------|
| `lv_guard_init` / `lv_guard_destroy` / `lv_guard_get_stats` / `lv_guard_reset_stats` | 守卫上下文生命周期与统计 |
| `lv_verify_data_integrity(ctx)` | 校验上下文状态、图指针、推理栈/递归深度、熔断器一致性 |
| `lv_RUNTIME_LOCK` / `lv_RUNTIME_UNLOCK` | 上下文级独占写锁 |
| `lv_READ_GUARD` / `lv_WRITE_GUARD` / 对应 UNGUARD | 读写守卫宏 |
| `lv_ASSERT_RUNTIME(ctx, cond, retval)` | 运行时断言，失败记录 `lv_ERROR_ASSERTION_FAILED` 并返回 |
| `lv_GUARDED_SECTION(ctx)` | 受保护区域（配合 UNLOCK 显式释放） |

## 工作流程

**错误发生与传播**：

1. 深层函数通过 `lv_CHECK_ARG` / `lv_PROPAGATE` 发现异常：`lv_set_error_ctx` 记录错误码与位置，`lv_error_push` 同步推入 8 帧栈，宏随即 `return`。
2. 中层函数若需补充上下文，用 `lv_ERROR_CTX_WRAP(ctx, ret, "...")` 将帧顶作为 `cause` 再压新帧，形成"原始错误 → 包装错误 → 上层错误"链；`lv_error_format_chain` 输出根因优先的完整回溯。
3. 顶层以 `lvResult` 承载（`lv_TRY` 早退），或经 `lv_get_error_description` 序列化到日志（`lv_LOG_FATAL`）与诊断报告（`lv_diagnostics_generate` 统计 error/warning 计数）。

**熔断器状态机**：CLOSED 下 `lv_circuit_breaker_record_error` 递增连续错误计数，超 `max_consecutive_errors`（默认 10）跳闸 → OPEN；OPEN 中 `check` 返回 false 拒绝执行，冷却（默认 5s）过后自动迁移 HALF_OPEN 允许一次试探；试探成功（`record_success`）恢复 CLOSED，失败重新 OPEN。维度超限（时间/深度/步数/内存）经 `lv_circuit_breaker_check_guarded` 同步触发跳闸，`uncancellable_refcount > 0` 时跳过超时熔断。

**恢复策略**：可恢复错误（超时、深度、步数）由调度层重试前先 `lv_error_clear` / `lv_circuit_breaker_reset`；不可恢复错误（OOM、图损坏 `lv_ERROR_GRAPH_CORRUPTED`、类型不一致）直接终止当前任务；`lv_recursion_reset` 复位全局递归熔断，`lv_guard_reset_stats` 清零守卫统计。

## 模块关系

| 关联文档 | 关系说明 |
|----------|----------|
| [07_func_block.md](07_func_block.md) | 函数块错误段 700-749（`lv_ERROR_FUNC_BLOCK_INVALID` 等）；递归选择器依赖熔断器深度维度 |
| [12_context_and_lifecycle.md](12_context_and_lifecycle.md) | 熔断器内嵌于 `lvContext`（`ctx->circuit_breaker`），上下文级 API 为兼容包装层；`lv_context_set_error` 为运行时断言写入口 |
| [23_core_infrastructure.md](23_core_infrastructure.md) | `cross_platform.h` 提供 `lv_FORMAT_PRINTF` 与 `lv_PUBLIC_API`；线程局部错误状态的底层支撑 |
| [25_engine_scheduler.md](25_engine_scheduler.md) | 调度器消费 `lv_ERROR_TIMEOUT` / 步数熔断信号，负责任务级恢复与重试策略 |
| [27_quantifier_logic.md](27_quantifier_logic.md) | 量词推理的深度/步数熔断与 `lv_ERROR_*` 传播 |
| [32_runtime_monitoring.md](32_runtime_monitoring.md) | `runtime_guard.h` / `runtime_monitor.h` 同属监控文档；健康检查、诊断报告与守卫统计协同 |
| [34_meta_proof_cache.md](34_meta_proof_cache.md) | 证明系统错误段 900-999（`lv_ERROR_PROOF_*`、`lv_ERROR_CIRCUIT_OPEN`），缓存失效与验证失败走同一传播链 |

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| v1.0.0 | 2026-08-10 | 初始版本：分层错误码体系、TLS 错误状态、检查/传播宏、熔断器三态状态机 |
| v1.1.0 | 2026-08-10 | 新增：`lvErrorContext` 8 帧错误链与 cause 传播（`lv_error.h`）、`lv_error_push` 旧式桥接、`lvResult`/`lv_TRY` 结果类型、独立 `lv_circuit_breaker.h` 多维熔断、模块级 `lv_ERROR_SLOT_*` 写入口收敛、`lv_error_is_unknown` 与错误表验证 |
