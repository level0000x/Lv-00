/**
 * @file context.c
 * @brief Lv-00 隔离上下文系统 —— 核心实现
 *
 * 实现 lvContext 的生命周期管理、错误管理等核心功能。
 * 当前为基础实现，提供完整的资源生命周期管理（12 种资源类型的创建/销毁/快照/回滚），
 * 使链接器能够正确解析所有 context.h 中声明的符号。
 * 完整实现需要添加：线程局部存储、资源隔离边界、上下文传播机制。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include "context.h"
#include "lv/circuit_breaker.h"
#include "lv/lv_lifecycle.h"
#include "lv/lv_xmacro.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv_internal.h"

#include "lv/constraint_graph.h"

#include "config.h" /* lv_DEFAULT_* macros */
#include "lv_utils.h"
#include "normalization.h"
#include "stream.h"

/* ============================================================
 * 内部静态变量
 * ============================================================ */

/** 全局上下文 ID 自增计数器（用于分配唯一 context_id） */
static uint64_t s_next_context_id = 1;

/* ============================================================
 * 第六部分：生命周期管理 API
 * ============================================================ */

/**
 * @brief 创建并初始化一个新的隔离上下文
 *
 * 使用 lv_calloc 分配零初始化的 lvContext 结构体，
 * 然后设置各字段的默认值。
 */
lvContext *lv_context_create(void) {
    lvContext *ctx = (lvContext *) lv_calloc(1, sizeof(lvContext));
    if (!ctx) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_context_create: 分配 lvContext 失败");
    }

    /* 5. 运行时参数 —— 错误码初始化为 OK */
    ctx->error_code = lv_OK;
    ctx->error_message[0] = '\0';
    ctx->last_status = 0;

    /* 6. 状态机 —— 初始化为 IDLE */
    ctx->state = lv_CONTEXT_IDLE;
    ctx->previous_state = lv_CONTEXT_IDLE;
    ctx->state_transition_count = 0;

    /* 7. 熔断器 —— 由核心模块统一初始化 */
    lv_circuit_breaker_init(&ctx->circuit_breaker);

    /* 单次超时与冷却时间由配置驱动（lv_CONTEXT_DEFAULT_* 取自 lv_config_current()） */
    ctx->circuit_breaker.timeout_ms = lv_CONTEXT_DEFAULT_TIMEOUT_MS;
    ctx->circuit_breaker.cooldown_ms = lv_CONTEXT_DEFAULT_COOLDOWN_MS;

    /* 8. 递归深度追踪 */
    ctx->recursion_depth = 0;
    ctx->max_recursion_depth = lv_CONTEXT_MAX_RECURSION_DEPTH;
    ctx->recursion_policy = lv_RECURSION_POLICY_ERROR;

    /* 4. 推理分支栈 —— 初始化为空栈 */
    lv_reasoning_stack_init(&ctx->reasoning_stack);

    /* 13. 快照/回滚支持 */
    ctx->snapshot_refcount = 0;
    ctx->parent_snapshot = NULL;
    ctx->snapshot_depth = 0;

    /* 15. 上下文 ID 与统计 */
    ctx->context_id = lv_ATOMIC_ADD64(&s_next_context_id, 1);
    ctx->created_at_us = lv_get_time_us();
    ctx->problems_processed = 0;

    /* 3. 主约束图 —— 初始化为空图（头文件契约：create 后必须可用） */
    ctx->main_graph = graph_create();
    if (!ctx->main_graph) {
        lv_free((void **) &ctx);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_context_create: 创建主约束图失败");
    }

    /* 12. 公理与规则引用 */
    ctx->rewrite_step_limit = lv_DEFAULT_REWRITE_STEP_LIMIT;

    return ctx;
}

/* ── lv_context_destroy 子资源销毁适配（LV_DESTROY_SHIM：强类型 destroy → void* 回调） ── */

LV_DESTROY_SHIM(destroy_ctx_main_graph, ConstraintGraph, graph_destroy)
LV_DESTROY_SHIM(destroy_ctx_stream_ctx, StreamContext, stream_context_destroy)
LV_DESTROY_SHIM(destroy_ctx_normalization, NormalizationResult, normalization_result_destroy)

/* 推理栈帧数组（含每帧的子资源）就地清空 */
static void destroy_ctx_reasoning_stack(void *obj, void *field_ptr) {
    lvContext *ctx = (lvContext *) obj;
    (void) field_ptr;
    lv_reasoning_stack_clear(&ctx->reasoning_stack);
}

/* 父快照引用计数递减：-1 后若归零则递归销毁（与 lv_context_reset 共用） */
static void ctx_release_parent_snapshot(lvContext *ctx) {
    if (!ctx->parent_snapshot)
        return;
    ctx->parent_snapshot->snapshot_refcount--;
    if (ctx->parent_snapshot->snapshot_refcount <= 0) {
        lv_context_destroy(ctx->parent_snapshot);
    }
    ctx->parent_snapshot = NULL;
}

static void destroy_ctx_parent_snapshot(void *obj, void *field_ptr) {
    (void) field_ptr;
    ctx_release_parent_snapshot((lvContext *) obj);
}

/* lv_context_destroy 字段描述表：释放顺序与原实现一致
 * （推理栈 → 主图 → 流式上下文 → 规范化结果 → 内存池 → 引用数组 →
 *   父快照（引用计数） → 名称 → 熔断器错误原因 → 外壳），全部置 NULL 安全 */
static const lvFieldDesc s_context_destroy_fields[] = {
    lv_FIELD_CUSTOM(lvContext, reasoning_stack, destroy_ctx_reasoning_stack),
    lv_FIELD_OBJECT(lvContext, main_graph, destroy_ctx_main_graph),
    lv_FIELD_OBJECT(lvContext, stream_ctx, destroy_ctx_stream_ctx),
    lv_FIELD_OBJECT(lvContext, last_normalization, destroy_ctx_normalization),
    lv_FIELD_PLAIN(lvContext, memory_pool),
    lv_FIELD_PLAIN(lvContext, module_refs),
    lv_FIELD_PLAIN(lvContext, axiom_pkg_refs),
    lv_FIELD_PLAIN(lvContext, rewrite_rule_refs),
    lv_FIELD_CUSTOM(lvContext, parent_snapshot, destroy_ctx_parent_snapshot),
    lv_FIELD_PLAIN(lvContext, name),
    /* circuit_breaker.trip_reason 为嵌套字段，手动给出复合偏移 */
    {"circuit_breaker.trip_reason", LV_FIELD_PLAIN_FREE,
     offsetof(lvContext, circuit_breaker) + offsetof(lvCircuitBreaker, trip_reason), 0, {NULL}},
};

/**
 * @brief 销毁上下文，释放所有关联资源
 *
 * 按顺序释放上下文持有的资源，最后释放结构体本身。
 * 释放顺序：推理栈帧 → 约束图 → 流式上下文 → 规范化结果 → 内存池 →
 * 引用数组 → 父快照（引用计数） → 名称 → 熔断器
 */
void lv_context_destroy(lvContext *ctx) {
    if (!ctx) {
        return;
    }

    /* 注意：ctx->ast_root 为不透明指针，由 DSL 编译器管理，不在此释放 */

    lv_obj_destroy_fields(ctx, s_context_destroy_fields,
                          sizeof(s_context_destroy_fields) / sizeof(s_context_destroy_fields[0]));
    lv_free((void **) &ctx);
}

/* ============================================================
 * 第十二部分：错误管理 API
 * ============================================================ */

/**
 * @brief 设置上下文的错误状态
 *
 * 使用 va_list 处理可变参数，将格式化的错误描述写入
 * ctx->error_message 定长缓冲区（512 字节），同时设置
 * ctx->error_code。
 */
void lv_context_set_error(lvContext *ctx, lvErrorCode code, const char *fmt, ...) {
    if (!ctx) {
        return;
    }

    ctx->error_code = code;

    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(ctx->error_message, sizeof(ctx->error_message), fmt, args);
        va_end(args);
    } else {
        ctx->error_message[0] = '\0';
    }

    /* 确保错误消息始终以 \0 终止 */
    ctx->error_message[sizeof(ctx->error_message) - 1] = '\0';
}


/* ================================================================
 * 第七部分：状态机管理 API（辅助函数）
 * ================================================================ */

/* ================================================================
 * 枚举 -> 名称 映射表（X-macro 键表生成，替代手写枚举条目）
 * ================================================================ */

/** @brief lvContextState 状态名键表（项序 = 枚举升序，供 lv_enum_to_str 二分） */
#define LV_CONTEXT_STATE_NAME_X(x)                                                                                    \
    x(lv_CONTEXT_IDLE, "IDLE（空闲）")                                                                                 \
    x(lv_CONTEXT_PARSING, "PARSING（解析中）")                                                                         \
    x(lv_CONTEXT_REASONING, "REASONING（推理中）")                                                                     \
    x(lv_CONTEXT_ERROR, "ERROR（错误）")                                                                               \
    x(lv_CONTEXT_COMPLETE, "COMPLETE（完成）")

/** @brief lv_context_state_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_lv_context_state_name_entries[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_CONTEXT_STATE_NAME_X)
};

const char *lv_context_state_name(lvContextState state) {
    return lv_enum_to_str(s_lv_context_state_name_entries, lv_ARRAY_SIZE(s_lv_context_state_name_entries), (int) state, "UNKNOWN（未知）");
}

/**
 * @brief 检查状态转移是否合法
 *
 * 状态转移合法性表：每行是一个位掩码，bit i 表示 from 状态可转移到枚举值 i 的目标状态
 *
 * 转移规则（与原 switch 完全一致）：
 *   IDLE      → PARSING | ERROR
 *   PARSING   → REASONING | ERROR | IDLE
 *   REASONING → COMPLETE | ERROR | IDLE
 *   COMPLETE  → IDLE
 *   ERROR     → IDLE
 */
/* exempt: 1-A 状态机合流终审一 —— 与 engine_state.c 的转移矩阵同构，
 * 但（a）返回错误码体系本质不同（lvErrorCode vs EngineStatus 两个独立枚举），
 * （b）无 self-transition 幂等分支（engine 允许 IDLE→IDLE no-op，此处查表判非法），
 * （c）越界目标状态并入"非法转移"错误消息（engine 单独"无效的目标状态"分支），
 * （d）NULL 入参返回 lv_ERROR_NULL_POINTER（engine 返回 ENGINE_STATUS_INVALID_ARGUMENT），
 * （e）熔断路径支持无条件强转 ERROR（见 ctx_force_to_error，engine 无此路径）。
 * 合成共享骨架需体内 if(mode) 分支且返回类型不可统一 → 伪同构，保留各自纯函数。 */
static const uint32_t kValidTransitions[] = {
    [lv_CONTEXT_IDLE]      = (1u << lv_CONTEXT_PARSING) | (1u << lv_CONTEXT_ERROR),
    [lv_CONTEXT_PARSING]   = (1u << lv_CONTEXT_REASONING) | (1u << lv_CONTEXT_ERROR) | (1u << lv_CONTEXT_IDLE),
    [lv_CONTEXT_REASONING] = (1u << lv_CONTEXT_COMPLETE) | (1u << lv_CONTEXT_ERROR) | (1u << lv_CONTEXT_IDLE),
    [lv_CONTEXT_COMPLETE]  = (1u << lv_CONTEXT_IDLE),
    [lv_CONTEXT_ERROR]     = (1u << lv_CONTEXT_IDLE),
};

bool lv_context_state_transition_valid(lvContextState from, lvContextState to) {
    if ((unsigned) from >= sizeof(kValidTransitions) / sizeof(kValidTransitions[0])) {
        return false; /* 非法源状态（原 default 分支语义） */
    }
    return ((unsigned) to < 32u) &&
           ((kValidTransitions[from] >> (unsigned) to) & 1u);
}

/**
 * @brief 熔断路径强制转入 ERROR（不查转移表）
 *
 * 超时/步数超限/连续错误超限触发熔断时，无条件将状态覆写为 ERROR。
 * 与 lv_context_set_state 的查表拒绝语义不同：此为"熔断权威覆写"，
 * 任何状态下都可被熔断抬入 ERROR。
 */
/* exempt: 状态机熔断强转 —— 不经过转移表校验，是熔断权威覆写路径；
 * 与 engine 的转移拒绝语义（非法转移仅报错不改状态）本质不同，保留独立纯函数。 */
static void ctx_force_to_error(lvContext *ctx) {
    ctx->previous_state = ctx->state;
    ctx->state = lv_CONTEXT_ERROR;
    ctx->state_transition_count++;
}

/**
 * @brief 获取上下文当前状态
 */
lvContextState lv_context_get_state(const lvContext *ctx) {
    if (!ctx) {
        return lv_CONTEXT_IDLE;
    }
    return ctx->state;
}

/**
 * @brief 尝试将上下文转移到指定状态
 */
lvErrorCode lv_context_set_state(lvContext *ctx, lvContextState new_state) {
    if (!ctx) {
        return lv_ERROR_NULL_POINTER;
    }

    /* 验证转移合法性 */
    if (!lv_context_state_transition_valid(ctx->state, new_state)) {
        lv_context_set_error(ctx, lv_ERROR_INVALID_STATE, "非法状态转移: %s → %s", lv_context_state_name(ctx->state),
                             lv_context_state_name(new_state));
        return lv_ERROR_INVALID_STATE;
    }

    ctx->previous_state = ctx->state;
    ctx->state = new_state;
    ctx->state_transition_count++;

    return lv_OK;
}
/* ============================================================
 * 第八部分：推理栈 API
 * ============================================================ */

/**
 * @brief 在当前推理栈上压入一个新分支帧
 *
 * 压入分支帧前会自动创建当前约束图的快照并保存在帧中。
 * 如果启用了熔断器且深度超限，此函数将失败。
 */
lvErrorCode lv_context_push_reasoning(lvContext *ctx, ReasoningBranchType branch_type, uint64_t timeout_ms) {
    if (!ctx) {
        return lv_ERROR_NULL_POINTER;
    }

    /* 检查熔断器 */
    if (lv_context_is_circuit_open(ctx)) {
        return lv_ERROR_INVALID_STATE;
    }

    /* 使用独立栈模块推入帧（包含深度检查和扩容） */
    lvErrorCode err = lv_reasoning_stack_push(&ctx->reasoning_stack, branch_type);
    if (err != lv_OK) {
        if (err == lv_ERROR_RESOURCE_EXHAUSTED) {
            lv_context_set_error(ctx, lv_ERROR_RESOURCE_EXHAUSTED, "推理栈深度超限: 当前 %d, 最大 %d",
                                 ctx->reasoning_stack.top + 1, ctx->reasoning_stack.max_depth);
        } else {
            lv_context_set_error(ctx, err, "推理栈操作失败");
        }
        return err;
    }

    /* 上下文特定的帧字段初始化 */
    ReasoningFrame *frame = &ctx->reasoning_stack.frames[ctx->reasoning_stack.top];
    frame->timeout_ms = timeout_ms;
    frame->created_at_us = lv_get_time_us();
    frame->ast_root_ref = ctx->ast_root;

    /* 创建约束图快照：通过 graph_copy 对当前主图做真深拷贝（节点/约束/
     * 类型特定数据/哈希索引），作为分支失败时回滚的目标。
     * graph_copy 失败时 frame->graph_snapshot 保持 NULL，
     * 推理栈释放（reasoning_frame_release）对 NULL 安全，回滚能力降级。 */
    if (ctx->main_graph) {
        frame->graph_snapshot = graph_copy((ConstraintGraph *) ctx->main_graph);
    }

    /* 递增熔断器深度 */
    ctx->circuit_breaker.current_depth = ctx->reasoning_stack.top + 1;

    return lv_OK;
}

/**
 * @brief 从推理栈弹出栈顶帧（分支闭合）
 */
lvErrorCode lv_context_pop_reasoning(lvContext *ctx) {
    if (!ctx) {
        return lv_ERROR_NULL_POINTER;
    }

    lvErrorCode err = lv_reasoning_stack_pop(&ctx->reasoning_stack);
    if (err != lv_OK) {
        if (err == lv_ERROR_INVALID_STATE) {
            lv_context_set_error(ctx, lv_ERROR_INVALID_STATE, "推理栈为空，无法弹出");
        }
        return err;
    }

    /* 更新熔断器深度 */
    ctx->circuit_breaker.current_depth = (ctx->reasoning_stack.top >= 0) ? ctx->reasoning_stack.top + 1 : 0;

    return lv_OK;
}

/**
 * @brief 获取当前推理栈深度
 */
int lv_context_get_reasoning_depth(const lvContext *ctx) {
    if (!ctx) {
        return 0;
    }
    return lv_reasoning_stack_count(&ctx->reasoning_stack);
}

/**
 * @brief 获取当前活跃的推理分支帧（栈顶）
 */
ReasoningFrame *lv_context_get_current_reasoning_frame(lvContext *ctx) {
    if (!ctx) {
        return NULL;
    }
    return lv_reasoning_stack_top(&ctx->reasoning_stack);
}
/* ============================================================
 * 第九部分：熔断器 API
 * ============================================================ */

/**
 * @brief 检查熔断器是否已触发
 *
 * 检查逻辑：
 * 1. 熔断器状态为 OPEN → 熔断
 * 2. 总运行时间超限（total_timeout_ms > 0）→ 熔断
 * 3. 深度超限 → 熔断
 * 4. 步数超限 → 熔断
 *
 * 半开态（HALF_OPEN）下允许试探性执行，不算熔断。
 */
bool lv_context_is_circuit_open(const lvContext *ctx) {
    if (!ctx) {
        return true; /* 无上下文视为熔断 */
    }

    /* 委托核心熔断器的纯判断（is_tripped 只判断不迁移状态） */
    return lv_circuit_breaker_is_tripped(&ctx->circuit_breaker);
}

/**
 * @brief 开始一次可熔断操作
 *
 * 记录操作开始时间，如果熔断器处于半开态则恢复为关闭态。
 */
void lv_context_begin_operation(lvContext *ctx) {
    if (!ctx) {
        return;
    }

    /* 半开态 → 关闭态（试探成功），并重置连续错误计数 */
    lv_circuit_breaker_record_success(&ctx->circuit_breaker);

    ctx->circuit_breaker.operation_start_us = lv_get_time_us();
}

/**
 * @brief 检查当前操作是否超时
 *
 * 比较当前时间与操作开始时间，超时则触发熔断器。
 * 在不可取消区域中不触发超时熔断。
 */
bool lv_context_check_timeout(lvContext *ctx) {
    if (!ctx) {
        return true;
    }

    /* 不可取消区域中不检查超时 */
    if (ctx->circuit_breaker.uncancellable_refcount > 0) {
        return false;
    }

    /* 未设置超时或未开始操作 */
    if (ctx->circuit_breaker.timeout_ms == 0 || ctx->circuit_breaker.operation_start_us == 0) {
        return false;
    }

    uint64_t now = lv_get_time_us();
    uint64_t elapsed_ms = (now - ctx->circuit_breaker.operation_start_us) / 1000;

    if (elapsed_ms >= ctx->circuit_breaker.timeout_ms) {
        /* 触发熔断（状态/时间/计数/原因由核心熔断器记录） */
        lv_circuit_breaker_do_trip(&ctx->circuit_breaker, "操作超时熔断");

        /* 转入错误状态（熔断强转，不查转移表） */
        ctx_force_to_error(ctx);

        lv_context_set_error(ctx, lv_ERROR_TIMEOUT, "操作超时: 已用 %llu ms, 限制 %llu ms",
                             (unsigned long long) elapsed_ms, (unsigned long long) ctx->circuit_breaker.timeout_ms);
        return true;
    }

    return false;
}

/**
 * @brief 进入不可取消区域
 */
void lv_context_enter_uncancellable(lvContext *ctx) {
    if (!ctx) {
        return;
    }
    ctx->circuit_breaker.uncancellable_refcount++;
}

/**
 * @brief 离开不可取消区域
 */
void lv_context_leave_uncancellable(lvContext *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->circuit_breaker.uncancellable_refcount > 0) {
        ctx->circuit_breaker.uncancellable_refcount--;
    }
}

/**
 * @brief 记录一次推理步骤
 *
 * 递增步骤计数，超过上限则触发熔断。
 * @return true 步骤在限制内，false 超限触发熔断
 */
bool lv_context_record_step(lvContext *ctx) {
    if (!ctx) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_context_record_step: ctx is NULL");
    }

    ctx->circuit_breaker.total_steps++;

    /* 递增推理栈顶帧的步骤计数 */
    if (ctx->reasoning_stack.top >= 0 && ctx->reasoning_stack.frames) {
        ctx->reasoning_stack.frames[ctx->reasoning_stack.top].step_count++;
    }

    /* 检查步数限制 */
    if (ctx->circuit_breaker.max_steps > 0 && ctx->circuit_breaker.total_steps >= ctx->circuit_breaker.max_steps) {
        /* 触发熔断（状态/时间/计数/原因由核心熔断器记录） */
        lv_circuit_breaker_do_trip(&ctx->circuit_breaker, "推理步数超限熔断");

        /* 转入错误状态（熔断强转，不查转移表） */
        ctx_force_to_error(ctx);

        lv_context_set_error(ctx, lv_ERROR_RESOURCE_EXHAUSTED, "推理步数超限: %lld / %lld",
                             (long long) ctx->circuit_breaker.total_steps, (long long) ctx->circuit_breaker.max_steps);
        return false;
    }

    return true;
}

/**
 * @brief 记录一次成功操作（重置连续错误计数）
 */
void lv_context_record_success(lvContext *ctx) {
    if (!ctx) {
        return;
    }
    lv_circuit_breaker_record_success(&ctx->circuit_breaker);
}

/**
 * @brief 记录一次错误操作（递增连续错误计数）
 *
 * 连续错误超过上限则触发熔断。
 * @return true 正常，false 连续错误超限触发熔断
 */
bool lv_context_record_error(lvContext *ctx) {
    if (!ctx) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_context_record_error: ctx is NULL");
    }

    ctx->circuit_breaker.consecutive_errors++;

    if (ctx->circuit_breaker.consecutive_errors >= ctx->circuit_breaker.max_consecutive_errors) {
        /* 触发熔断（状态/时间/计数/原因由核心熔断器记录） */
        lv_circuit_breaker_do_trip(&ctx->circuit_breaker, "连续错误超限熔断");

        /* 转入错误状态（熔断强转，不查转移表） */
        ctx_force_to_error(ctx);

        lv_context_set_error(ctx, lv_ERROR_RESOURCE_EXHAUSTED, "连续错误次数超限: %d / %d",
                             ctx->circuit_breaker.consecutive_errors, ctx->circuit_breaker.max_consecutive_errors);
        return false;
    }

    return true;
}
/* ============================================================
 * 第十部分：参数配置 API
 * ============================================================ */

/**
 * @brief 设置上下文超时时间
 */
void lv_context_set_timeout(lvContext *ctx, uint64_t timeout_ms) {
    if (!ctx) {
        return;
    }
    ctx->circuit_breaker.timeout_ms = timeout_ms;
}

/**
 * @brief 获取上下文超时时间
 */
uint64_t lv_context_get_timeout(const lvContext *ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->circuit_breaker.timeout_ms;
}

/**
 * @brief 设置递归/推理深度上限
 */
void lv_context_set_max_depth(lvContext *ctx, int max_depth) {
    if (!ctx) {
        return;
    }
    /* 限制在合理范围内 */
    if (max_depth < 1) {
        max_depth = 1;
    }
    if (max_depth > lv_CONTEXT_MAX_RECURSION_DEPTH) {
        max_depth = lv_CONTEXT_MAX_RECURSION_DEPTH;
    }
    ctx->circuit_breaker.max_depth = max_depth;
    ctx->max_recursion_depth = max_depth;
    ctx->reasoning_stack.max_depth = max_depth;
}

/**
 * @brief 获取递归/推理深度上限
 */
int lv_context_get_max_depth(const lvContext *ctx) {
    if (!ctx) {
        return lv_CONTEXT_DEFAULT_MAX_DEPTH;
    }
    return ctx->circuit_breaker.max_depth;
}

/**
 * @brief 设置最大推理步骤数
 */
void lv_context_set_max_steps(lvContext *ctx, int64_t max_steps) {
    if (!ctx) {
        return;
    }
    ctx->circuit_breaker.max_steps = max_steps;
}

/**
 * @brief 获取最大推理步骤数
 */
int64_t lv_context_get_max_steps(const lvContext *ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->circuit_breaker.max_steps;
}

/**
 * @brief 设置上下文名称（内部复制）
 */
void lv_context_set_name(lvContext *ctx, const char *name) {
    if (!ctx) {
        return;
    }

    /* 释放旧名称 */
    if (ctx->name) {
        lv_free((void **) &ctx->name);
        ctx->name = NULL;
    }

    /* 复制新名称 */
    if (name) {
        ctx->name = lv_strdup_safe(name);
    }
}

/**
 * @brief 获取上下文名称
 */
const char *lv_context_get_name(const lvContext *ctx) {
    if (!ctx) {
        return "null";
    }
    return ctx->name ? ctx->name : "";
}

/**
 * @brief 获取上下文 ID
 */
uint64_t lv_context_get_id(const lvContext *ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->context_id;
}

/* ============================================================
 * 第十二部分：错误管理 API（补充函数）
 * ============================================================ */

/**
 * @brief 清除上下文的错误状态
 */
void lv_context_clear_error(lvContext *ctx) {
    if (!ctx) {
        return;
    }
    ctx->error_code = lv_OK;
    ctx->error_message[0] = '\0';
    ctx->last_status = 0;
}

/**
 * @brief 获取上下文的错误码
 */
lvErrorCode lv_context_get_error_code(const lvContext *ctx) {
    if (!ctx) {
        return lv_ERROR_NULL_POINTER;
    }
    return ctx->error_code;
}

/**
 * @brief 获取上下文的错误消息
 */
const char *lv_context_get_error_message(const lvContext *ctx) {
    if (!ctx) {
        return "null context";
    }
    return ctx->error_message;
}
/* ============================================================
 * 第十四部分：统计与调试 API
 * ============================================================ */

/**
 * @brief 获取上下文统计信息的可读摘要
 *
 * 将关键统计指标格式化为可读字符串。
 */
int lv_context_get_stats(const lvContext *ctx, char *buf, size_t buf_size) {
    if (!ctx || !buf || buf_size == 0) {
        return 0;
    }

    /* 计算运行时间 */
    uint64_t uptime_us = 0;
    if (ctx->circuit_breaker.start_time_us > 0) {
        uptime_us = lv_get_time_us() - ctx->circuit_breaker.start_time_us;
    }
    uint64_t uptime_ms = uptime_us / lv_US_PER_MS;

    /* 格式化统计信息 */
    int written =
        snprintf(buf, buf_size,
                 "=== lvContext 统计信息 ===\n"
                 "上下文 ID:        %llu\n"
                 "名称:             %s\n"
                 "当前状态:         %s\n"
                 "推理栈深度:       %d / %d\n"
                 "已执行步数:       %lld / %lld\n"
                 "缓存命中率:       %.1f%% (%lld 次命中 / %lld 次访问)\n"
                 "熔断器状态:       %s\n"
                 "熔断次数:         %d\n"
                 "连续错误:         %d / %d\n"
                 "已处理问题数:     %d\n"
                 "状态转移次数:     %lld\n"
                 "运行时间:         %llu ms\n",
                 (unsigned long long) ctx->context_id, ctx->name ? ctx->name : "(无名)",
                 lv_context_state_name(ctx->state), ctx->reasoning_stack.top + 1, ctx->reasoning_stack.max_depth,
                 (long long) ctx->circuit_breaker.total_steps, (long long) ctx->circuit_breaker.max_steps,
                 lv_circuit_breaker_state_name((lvContext *) ctx),
                 ctx->circuit_breaker.trip_count, ctx->circuit_breaker.consecutive_errors,
                 ctx->circuit_breaker.max_consecutive_errors, ctx->problems_processed,
                 (long long) ctx->state_transition_count, (unsigned long long) uptime_ms);

    /* 确保字符串终止 */
    if (written > 0 && (size_t) written >= buf_size) {
        buf[buf_size - 1] = '\0';
        return (int) (buf_size - 1);
    }

    return written;
}

/* ============================================================
 * 第六部分（补充）：生命周期管理 API
 * ============================================================ */

/**
 * @brief 重置上下文，清除所有问题特定状态
 *
 * 将上下文恢复到刚创建时的"干净"状态。
 * 保留 context_id、模块引用、流式上下文、用户扩展和 trip_count。
 */
void lv_context_reset(lvContext *ctx) {
    if (!ctx) {
        return;
    }

    /* 进入不可取消区域，防止重置过程中被超时熔断 */
    lv_context_enter_uncancellable(ctx);

    /* 1. 释放推理栈帧 */
    lv_reasoning_stack_clear(&ctx->reasoning_stack);

    /* 2. 重建主约束图 */
    if (ctx->main_graph) {
        graph_destroy((ConstraintGraph *) ctx->main_graph);
    }
    ctx->main_graph = graph_create();
    if (!ctx->main_graph) {
        ctx->error_code = lv_ERROR_OUT_OF_MEMORY;
        snprintf(ctx->error_message, sizeof(ctx->error_message), "lv_context_reset: 创建主约束图失败");
        ctx->state = lv_CONTEXT_ERROR;
        lv_context_leave_uncancellable(ctx);
        return;
    }

    /* 4. 释放规范化结果 */
    if (ctx->last_normalization) {
        normalization_result_destroy(ctx->last_normalization);
        ctx->last_normalization = NULL;
    }

    /* 5. 释放父快照链（引用计数递减，归零时递归销毁，与 destroy 共用同一基元） */
    ctx_release_parent_snapshot(ctx);
    ctx->snapshot_refcount = 0;
    ctx->snapshot_depth = 0;

    /* 6. 更新统计 —— 必须在状态机重置之前读取 previous_state
     * （原顺序先清零后判断，导致统计恒不更新） */
    if (ctx->previous_state == lv_CONTEXT_COMPLETE) {
        ctx->problems_processed++;
    }

    /* 7. 重置状态机 */
    ctx->state = lv_CONTEXT_IDLE;
    ctx->previous_state = lv_CONTEXT_IDLE;
    ctx->state_transition_count = 0;

    /* 8. 重置错误状态 */
    ctx->error_code = lv_OK;
    ctx->error_message[0] = '\0';
    ctx->last_status = 0;

    /* 9. 重置递归深度 */
    ctx->recursion_depth = 0;
    ctx->recursion_policy = lv_RECURSION_POLICY_ERROR;

    /* 10. 重置熔断器（保留 trip_count） */
    int preserved_trip_count = ctx->circuit_breaker.trip_count;
    ctx->circuit_breaker.state = CIRCUIT_BREAKER_CLOSED;
    ctx->circuit_breaker.uncancellable_refcount = 0;
    ctx->circuit_breaker.current_depth = 0;
    ctx->circuit_breaker.total_steps = 0;
    ctx->circuit_breaker.consecutive_errors = 0;
    ctx->circuit_breaker.start_time_us = lv_get_time_us();
    ctx->circuit_breaker.operation_start_us = 0;
    ctx->circuit_breaker.tripped_at_us = 0;
    ctx->circuit_breaker.trip_count = preserved_trip_count;
    if (ctx->circuit_breaker.trip_reason) {
        lv_free((void **) &ctx->circuit_breaker.trip_reason);
        ctx->circuit_breaker.trip_reason = NULL;
    }

    /* 11. 重置 AST */
    ctx->ast_root = NULL;

    /* 12. 释放名称 */
    if (ctx->name) {
        lv_free((void **) &ctx->name);
    }

    /* 离开不可取消区域 */
    lv_context_leave_uncancellable(ctx);
}

/* ============================================================
 * 第六部分（补充）：快照/回滚 API
 *
 * 实现 context.h 承诺的 lv_context_snapshot() / lv_context_rollback()。
 * 当前为"基于 graph_copy 的图快照"最小可用版本（能力边界见头文件文档）：
 *   - 约束图深拷贝：graph_copy（唯一公共图级深拷贝入口，恒等 ID 方案）
 *   - 标量状态字段逐字段复制/恢复（状态机、错误、熔断器、递归深度等）
 *   - 推理栈不复制（快照的 reasoning_stack 为空栈，回滚后清空）
 *   - 共享资源引用（模块/公理/重写规则数组）与 stream_ctx/memory_pool 不复制
 * ============================================================ */

/**
 * @brief 创建当前上下文的快照（基于 graph_copy 的图快照）
 *
 * 快照是独立的 lvContext 实例：约束图做深拷贝，标量配置逐字段复制。
 * 快照通过 parent_snapshot 链接回源上下文并递增其 snapshot_refcount，
 * 因此调用方持有快照期间，源上下文不会被意外销毁（见 destroy 中
 * 父快照链释放逻辑）。
 */
lvContext *lv_context_snapshot(lvContext *ctx) {
    if (!ctx) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_context_snapshot: ctx is NULL");
    }

    lvContext *snap = (lvContext *) lv_calloc(1, sizeof(lvContext));
    if (!snap) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_context_snapshot: 分配快照失败");
    }

    /* 约束图深拷贝（唯一公共深拷贝入口 graph_copy） */
    if (ctx->main_graph) {
        snap->main_graph = graph_copy((ConstraintGraph *) ctx->main_graph);
        if (!snap->main_graph) {
            lv_free((void **) &snap);
            lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_context_snapshot: 复制约束图失败");
        }
    }

    /* 标量状态字段逐字段复制 */
    snap->error_code = ctx->error_code;
    memcpy(snap->error_message, ctx->error_message, sizeof(snap->error_message));
    snap->last_status = ctx->last_status;

    snap->state = ctx->state;
    snap->previous_state = ctx->previous_state;
    snap->state_transition_count = ctx->state_transition_count;

    /* 熔断器：结构体浅拷贝 + trip_reason 深拷贝（避免与源上下文共享字符串所有权） */
    snap->circuit_breaker = ctx->circuit_breaker;
    if (ctx->circuit_breaker.trip_reason) {
        snap->circuit_breaker.trip_reason = lv_strdup_safe(ctx->circuit_breaker.trip_reason);
    }

    snap->recursion_depth = ctx->recursion_depth;
    snap->max_recursion_depth = ctx->max_recursion_depth;
    snap->recursion_policy = ctx->recursion_policy;

    snap->rewrite_step_limit = ctx->rewrite_step_limit;
    snap->mem_stats = ctx->mem_stats;
    snap->user_extension = ctx->user_extension;
    snap->ast_root = ctx->ast_root; /* 浅拷贝（AST 由 DSL 编译器管理，上下文不拥有） */
    snap->problems_processed = ctx->problems_processed;

    /* 快照元数据：继承源上下文 ID（同一逻辑任务的快照） */
    snap->context_id = ctx->context_id;
    snap->created_at_us = lv_get_time_us();
    snap->name = ctx->name ? lv_strdup_safe(ctx->name) : NULL;

    /* 推理栈：最小版本不复制（能力边界见头文件文档） */
    lv_reasoning_stack_init(&snap->reasoning_stack);

    /* 快照链：链接回源上下文并递增引用计数 */
    snap->parent_snapshot = ctx;
    ctx->snapshot_refcount++;
    snap->snapshot_depth = ctx->snapshot_depth + 1;

    return snap;
}

/**
 * @brief 将上下文回滚到快照记录的状态
 *
 * 用快照的约束图深拷贝替换 ctx 的当前主图，并恢复标量状态字段。
 * 快照本身不被修改，可多次回滚到同一快照。
 */
lvErrorCode lv_context_rollback(lvContext *ctx, const lvContext *snapshot) {
    if (!ctx || !snapshot) {
        return lv_ERROR_NULL_POINTER;
    }

    /* 1. 约束图恢复：以快照图为源做深拷贝，替换当前主图
     *    （不转移所有权，快照可重复使用） */
    if (snapshot->main_graph) {
        ConstraintGraph *new_graph = graph_copy((ConstraintGraph *) snapshot->main_graph);
        if (!new_graph) {
            lv_context_set_error(ctx, lv_ERROR_OUT_OF_MEMORY, "lv_context_rollback: 恢复约束图失败（内存不足）");
            return lv_ERROR_OUT_OF_MEMORY;
        }
        if (ctx->main_graph) {
            graph_destroy((ConstraintGraph *) ctx->main_graph);
        }
        ctx->main_graph = new_graph;
    }

    /* 2. 恢复标量状态字段 */
    ctx->error_code = snapshot->error_code;
    memcpy(ctx->error_message, snapshot->error_message, sizeof(ctx->error_message));
    ctx->last_status = snapshot->last_status;

    ctx->state = snapshot->state;
    ctx->previous_state = snapshot->previous_state;
    ctx->state_transition_count = snapshot->state_transition_count;

    /* 熔断器：先释放旧 trip_reason，再整体恢复并深拷贝快照的 trip_reason */
    if (ctx->circuit_breaker.trip_reason) {
        lv_free((void **) &ctx->circuit_breaker.trip_reason);
    }
    ctx->circuit_breaker = snapshot->circuit_breaker;
    ctx->circuit_breaker.trip_reason = snapshot->circuit_breaker.trip_reason
                                           ? lv_strdup_safe(snapshot->circuit_breaker.trip_reason)
                                           : NULL;

    ctx->recursion_depth = snapshot->recursion_depth;
    ctx->max_recursion_depth = snapshot->max_recursion_depth;
    ctx->recursion_policy = snapshot->recursion_policy;

    ctx->rewrite_step_limit = snapshot->rewrite_step_limit;
    ctx->problems_processed = snapshot->problems_processed;
    ctx->ast_root = snapshot->ast_root; /* 浅拷贝 */

    /* 3. 推理栈：快照未记录推理栈（能力边界），回滚后清空当前推理栈 */
    if (ctx->reasoning_stack.frames) {
        lv_reasoning_stack_clear(&ctx->reasoning_stack);
    }
    ctx->circuit_breaker.current_depth = 0;

    return lv_OK;
}
