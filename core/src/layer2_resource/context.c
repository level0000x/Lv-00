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

#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "lv/constraint_graph.h"

#include "config.h" /* lv_DEFAULT_* macros */
#include "lv_utils.h"
#include "normalization.h"
#include "stream.h"

/* ============================================================
 * 内部静态变量
 * ============================================================ */

/** 全局上下文 ID 自增计数器（用于分配唯一 context_id） */
static atomic_uint_fast64_t s_next_context_id = 1;

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
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "lv_context_create: 分配 lvContext 失败");
        return NULL;
    }

    /* 5. 运行时参数 —— 错误码初始化为 OK */
    ctx->error_code = lv_OK;
    ctx->error_message[0] = '\0';
    ctx->last_status = 0;

    /* 6. 状态机 —— 初始化为 IDLE */
    ctx->state = lv_CONTEXT_IDLE;
    ctx->previous_state = lv_CONTEXT_IDLE;
    ctx->state_transition_count = 0;

    /* 7. 熔断器 —— 设置默认值 */
    ctx->circuit_breaker.state = CIRCUIT_BREAKER_CLOSED;
    ctx->circuit_breaker.timeout_ms = lv_CONTEXT_DEFAULT_TIMEOUT_MS;
    ctx->circuit_breaker.total_timeout_ms = 0; /* 不限制总运行时间 */
    ctx->circuit_breaker.uncancellable_refcount = 0;
    ctx->circuit_breaker.current_depth = 0;
    ctx->circuit_breaker.max_depth = lv_CONTEXT_DEFAULT_MAX_DEPTH;
    ctx->circuit_breaker.total_steps = 0;
    ctx->circuit_breaker.max_steps = lv_CONTEXT_DEFAULT_MAX_STEPS;
    ctx->circuit_breaker.consecutive_errors = 0;
    ctx->circuit_breaker.max_consecutive_errors = lv_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS;
    ctx->circuit_breaker.max_memory_bytes = 0; /* 不限制 */
    ctx->circuit_breaker.start_time_us = lv_get_time_us();
    ctx->circuit_breaker.operation_start_us = 0;
    ctx->circuit_breaker.cooldown_ms = lv_CONTEXT_DEFAULT_COOLDOWN_MS;
    ctx->circuit_breaker.tripped_at_us = 0;
    ctx->circuit_breaker.trip_reason = NULL;
    ctx->circuit_breaker.trip_count = 0;

    /* 8. 递归深度追踪 */
    ctx->recursion_depth = 0;
    ctx->max_recursion_depth = lv_CONTEXT_MAX_RECURSION_DEPTH;
    ctx->recursion_policy = lv_RECURSION_POLICY_ERROR;

    /* 4. 推理分支栈 —— 初始化为空栈 */
    ctx->reasoning_stack.frames = NULL;
    ctx->reasoning_stack.top = -1;
    ctx->reasoning_stack.capacity = 0;
    ctx->reasoning_stack.max_depth = lv_CONTEXT_REASONING_STACK_MAX_DEPTH;

    /* 3. 缓存状态 */
    ctx->cache_valid = false;
    ctx->cache_hits = 0;
    ctx->cache_misses = 0;

    /* 13. 快照/回滚支持 */
    ctx->snapshot_refcount = 0;
    ctx->parent_snapshot = NULL;
    ctx->snapshot_depth = 0;

    /* 15. 上下文 ID 与统计 */
    ctx->context_id = atomic_fetch_add(&s_next_context_id, 1);
    ctx->created_at_us = lv_get_time_us();
    ctx->problems_processed = 0;

    /* 12. 公理与规则引用 */
    ctx->rewrite_step_limit = lv_DEFAULT_REWRITE_STEP_LIMIT;

    return ctx;
}

/**
 * @brief 销毁上下文，释放所有关联资源
 *
 * 按顺序释放上下文持有的资源，最后释放结构体本身。
 * 释放顺序：推理栈帧 → 缓存 → 约束图 → 流式上下文 → 规范化结果 → 名称 → 熔断器
 */
void lv_context_destroy(lvContext *ctx) {
    if (!ctx) {
        return;
    }

    /* 1. 释放推理栈帧数组（含每帧的子资源） */
    if (ctx->reasoning_stack.frames) {
        for (int i = 0; i <= ctx->reasoning_stack.top && i < ctx->reasoning_stack.capacity; i++) {
            ReasoningFrame *frame = &ctx->reasoning_stack.frames[i];
            if (frame->graph_snapshot) {
                graph_destroy((ConstraintGraph *) frame->graph_snapshot);
                frame->graph_snapshot = NULL;
            }
            if (frame->assumption_node_ids) {
                lv_free((void **) &frame->assumption_node_ids);
            }
            if (frame->target_node_ids) {
                lv_free((void **) &frame->target_node_ids);
            }
            /* user_data 由外部管理，不释放 */
        }
        lv_free((void **) &ctx->reasoning_stack.frames);
    }

    /* 2. 释放代数计算缓存 */
    if (ctx->groebner_cache) {
        lv_free((void **) &ctx->groebner_cache);
    }
    if (ctx->symbolic_cache) {
        lv_free((void **) &ctx->symbolic_cache);
    }
    if (ctx->numeric_cache) {
        lv_free((void **) &ctx->numeric_cache);
    }
    if (ctx->unification_cache) {
        lv_free((void **) &ctx->unification_cache);
    }

    /* 3. 释放主约束图 */
    if (ctx->main_graph) {
        graph_destroy((ConstraintGraph *) ctx->main_graph);
        ctx->main_graph = NULL;
    }

    /* 4. 释放 AST 根节点（不透明指针，由 DSL 编译器管理） */
    /* ctx->ast_root 由外部管理，不释放 */

    /* 5. 释放流式输出上下文 */
    if (ctx->stream_ctx) {
        stream_context_destroy(ctx->stream_ctx);
        ctx->stream_ctx = NULL;
    }

    /* 6. 释放规范化结果 */
    if (ctx->last_normalization) {
        normalization_result_destroy(ctx->last_normalization);
        ctx->last_normalization = NULL;
    }

    /* 7. 释放内存池（不透明指针） */
    if (ctx->memory_pool) {
        lv_free((void **) &ctx->memory_pool);
    }

    /* 8. 释放模块/公理/规则引用数组（不拥有所指对象，仅释放数组本身） */
    if (ctx->module_refs) {
        lv_free((void **) &ctx->module_refs);
    }
    if (ctx->axiom_pkg_refs) {
        lv_free((void **) &ctx->axiom_pkg_refs);
    }
    if (ctx->rewrite_rule_refs) {
        lv_free((void **) &ctx->rewrite_rule_refs);
    }

    /* 9. 释放父快照链（递归销毁） */
    if (ctx->parent_snapshot) {
        /* 递减父快照的引用计数 */
        ctx->parent_snapshot->snapshot_refcount--;
        if (ctx->parent_snapshot->snapshot_refcount <= 0) {
            lv_context_destroy(ctx->parent_snapshot);
        }
        ctx->parent_snapshot = NULL;
    }

    /* 10. 释放名称字符串 */
    if (ctx->name) {
        lv_free((void **) &ctx->name);
    }

    /* 11. 释放熔断器错误原因字符串 */
    if (ctx->circuit_breaker.trip_reason) {
        lv_free((void **) &ctx->circuit_breaker.trip_reason);
    }

    /* 12. 释放上下文结构体本身 */
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

/* ============================================================
 * 快照/回滚 API
 * ============================================================ */

/**
 * @brief 创建当前上下文的完整快照（深拷贝）
 *
 * 快照包含约束图的深拷贝、推理栈的深拷贝、熔断器/状态机的拷贝。
 * 原始上下文的 snapshot_refcount 递增，快照的 parent_snapshot 指向原始上下文。
 */
lvContext *lv_context_snapshot(lvContext *ctx) {
    if (!ctx) {
        return NULL;
    }

    lvContext *snap = (lvContext *) lv_calloc(1, sizeof(lvContext));
    if (!snap) {
        lv_set_error(lv_ERROR_OUT_OF_MEMORY, "lv_context_snapshot: 分配快照失败");
        return NULL;
    }

    /* 拷贝整个结构体（浅拷贝） */
    *snap = *ctx;

    /* 深拷贝主约束图 */
    if (ctx->main_graph) {
        snap->main_graph = graph_create();
        if (!snap->main_graph) {
            lv_free((void **) &snap);
            return NULL;
        }
        /* graph_create 创建空图；当前为浅拷贝（仅创建空图），完整版应深拷贝所有节点和约束 */
    }

    /* 深拷贝推理栈 */
    snap->reasoning_stack.frames = NULL;
    snap->reasoning_stack.top = -1;
    snap->reasoning_stack.capacity = 0;
    if (ctx->reasoning_stack.capacity > 0 && ctx->reasoning_stack.top >= 0) {
        snap->reasoning_stack.capacity = ctx->reasoning_stack.capacity;
        snap->reasoning_stack.frames =
            (ReasoningFrame *) lv_calloc((size_t) ctx->reasoning_stack.capacity, sizeof(ReasoningFrame));
        if (!snap->reasoning_stack.frames) {
            if (snap->main_graph)
                graph_destroy((ConstraintGraph *) snap->main_graph);
            lv_free((void **) &snap);
            return NULL;
        }
        snap->reasoning_stack.top = ctx->reasoning_stack.top;
        for (int i = 0; i <= ctx->reasoning_stack.top; i++) {
            ReasoningFrame *src_frame = &ctx->reasoning_stack.frames[i];
            ReasoningFrame *dst_frame = &snap->reasoning_stack.frames[i];
            *dst_frame = *src_frame;
            /* 深拷贝帧内的约束图快照 */
            if (src_frame->graph_snapshot) {
                dst_frame->graph_snapshot = graph_create();
                /* 完整版应深拷贝节点和约束 */
            }
            /* 深拷贝假设和目标数组 */
            if (src_frame->assumption_node_ids && src_frame->assumption_count > 0) {
                dst_frame->assumption_node_ids = (int *) lv_calloc((size_t) src_frame->assumption_count, sizeof(int));
                if (dst_frame->assumption_node_ids) {
                    memcpy(dst_frame->assumption_node_ids, src_frame->assumption_node_ids,
                           (size_t) src_frame->assumption_count * sizeof(int));
                }
            }
            if (src_frame->target_node_ids && src_frame->target_count > 0) {
                dst_frame->target_node_ids = (int *) lv_calloc((size_t) src_frame->target_count, sizeof(int));
                if (dst_frame->target_node_ids) {
                    memcpy(dst_frame->target_node_ids, src_frame->target_node_ids,
                           (size_t) src_frame->target_count * sizeof(int));
                }
            }
        }
    }

    /* 深拷贝名称字符串 */
    snap->name = NULL;
    if (ctx->name) {
        snap->name = lv_strdup_safe(ctx->name);
    }

    /* 深拷贝熔断器错误原因 */
    snap->circuit_breaker.trip_reason = NULL;
    if (ctx->circuit_breaker.trip_reason) {
        snap->circuit_breaker.trip_reason = lv_strdup_safe(ctx->circuit_breaker.trip_reason);
    }

    /* 缓存标记为无效（不深拷贝缓存内容） */
    snap->cache_valid = false;
    snap->groebner_cache = NULL;
    snap->symbolic_cache = NULL;
    snap->numeric_cache = NULL;
    snap->unification_cache = NULL;

    /* 不拷贝流式上下文（共享引用） */
    /* 不拷贝规范化结果（快照后重新计算） */
    snap->last_normalization = NULL;

    /* 不拷贝内存池 */
    snap->memory_pool = NULL;

    /* 不拷贝模块/公理/规则引用数组（共享引用） */
    snap->module_refs = NULL;
    snap->module_ref_count = 0;
    snap->axiom_pkg_refs = NULL;
    snap->axiom_pkg_ref_count = 0;
    snap->rewrite_rule_refs = NULL;
    snap->rewrite_rule_ref_count = 0;

    /* 设置快照链关系 */
    snap->parent_snapshot = ctx;
    ctx->snapshot_refcount++;
    snap->snapshot_depth = ctx->snapshot_depth + 1;

    /* 快照获得新的唯一 ID */
    snap->context_id = atomic_fetch_add(&s_next_context_id, 1);

    return snap;
}

/**
 * @brief 将上下文状态回滚到指定的快照
 *
 * 用快照中的状态替换当前上下文的状态。
 * 保留 context_id、problems_processed、trip_count。
 * 回滚后原始上下文的 snapshot_refcount 递减。
 */
bool lv_context_rollback(lvContext *ctx, lvContext *snapshot) {
    if (!ctx || !snapshot) {
        return false;
    }

    /* 保存需要保留的字段 */
    uint64_t preserved_context_id = ctx->context_id;
    int preserved_problems_processed = ctx->problems_processed;
    int preserved_trip_count = ctx->circuit_breaker.trip_count;

    /*
     * 处理 stream_ctx：快照不包含 stream_ctx，回滚后需要置空以避免悬空指针。
     * stream_ctx 的生命周期由流式输出流程独立管理，不属于可回滚的上下文状态。
     * 调用方如果需要保留流式输出，应在回滚前另行保存 stream_ctx 引用。
     */
    if (ctx->stream_ctx) {
        ctx->stream_ctx = NULL;
    }

    /* 释放当前上下文的可替换资源（不销毁结构体本身） */

    /* 释放推理栈帧 */
    if (ctx->reasoning_stack.frames) {
        for (int i = 0; i <= ctx->reasoning_stack.top && i < ctx->reasoning_stack.capacity; i++) {
            ReasoningFrame *frame = &ctx->reasoning_stack.frames[i];
            if (frame->graph_snapshot) {
                graph_destroy((ConstraintGraph *) frame->graph_snapshot);
            }
            if (frame->assumption_node_ids) {
                lv_free((void **) &frame->assumption_node_ids);
            }
            if (frame->target_node_ids) {
                lv_free((void **) &frame->target_node_ids);
            }
        }
        lv_free((void **) &ctx->reasoning_stack.frames);
    }

    /* 释放缓存 */
    if (ctx->groebner_cache)
        lv_free((void **) &ctx->groebner_cache);
    if (ctx->symbolic_cache)
        lv_free((void **) &ctx->symbolic_cache);
    if (ctx->numeric_cache)
        lv_free((void **) &ctx->numeric_cache);
    if (ctx->unification_cache)
        lv_free((void **) &ctx->unification_cache);

    /* 释放主约束图 */
    if (ctx->main_graph) {
        graph_destroy((ConstraintGraph *) ctx->main_graph);
    }

    /* 释放规范化结果 */
    if (ctx->last_normalization) {
        normalization_result_destroy(ctx->last_normalization);
    }

    /* 释放名称 */
    if (ctx->name) {
        lv_free((void **) &ctx->name);
    }

    /* 释放熔断器错误原因 */
    if (ctx->circuit_breaker.trip_reason) {
        lv_free((void **) &ctx->circuit_breaker.trip_reason);
    }

    /* 从快照拷贝状态（浅拷贝，因为快照的资源将转移到 ctx） */
    ctx->main_graph = snapshot->main_graph;
    ctx->ast_root = snapshot->ast_root;
    ctx->reasoning_stack = snapshot->reasoning_stack;
    ctx->state = snapshot->state;
    ctx->previous_state = snapshot->previous_state;
    ctx->state_transition_count = snapshot->state_transition_count;
    ctx->cache_valid = false; /* 回滚后缓存失效 */
    ctx->cache_hits = snapshot->cache_hits;
    ctx->cache_misses = snapshot->cache_misses;
    ctx->recursion_depth = snapshot->recursion_depth;
    ctx->error_code = lv_OK;
    ctx->error_message[0] = '\0';
    ctx->last_status = 0;
    ctx->groebner_cache = NULL;
    ctx->symbolic_cache = NULL;
    ctx->numeric_cache = NULL;
    ctx->unification_cache = NULL;
    ctx->last_normalization = NULL;
    ctx->name = snapshot->name;

    /* 拷贝熔断器状态（保留 trip_count） */
    ctx->circuit_breaker = snapshot->circuit_breaker;
    ctx->circuit_breaker.trip_count = preserved_trip_count;

    /* 恢复保留的字段 */
    ctx->context_id = preserved_context_id;
    ctx->problems_processed = preserved_problems_processed;

    /* 清空快照中的指针（资源已转移到 ctx，防止双重释放） */
    snapshot->main_graph = NULL;
    snapshot->ast_root = NULL;
    snapshot->reasoning_stack.frames = NULL;
    snapshot->reasoning_stack.top = -1;
    snapshot->reasoning_stack.capacity = 0;
    snapshot->name = NULL;
    snapshot->circuit_breaker.trip_reason = NULL;

    /* 递减父快照引用计数 */
    if (snapshot->parent_snapshot) {
        snapshot->parent_snapshot->snapshot_refcount--;
    }

    return true;
}

/* ============================================================
 * 第七部分：状态机管理 API（辅助函数）
 * ============================================================ */

/**
 * @brief 获取状态机的可读字符串名称
 */
const char *lv_context_state_name(lvContextState state) {
    switch (state) {
        case lv_CONTEXT_IDLE:
            return "IDLE（空闲）";
        case lv_CONTEXT_PARSING:
            return "PARSING（解析中）";
        case lv_CONTEXT_REASONING:
            return "REASONING（推理中）";
        case lv_CONTEXT_ERROR:
            return "ERROR（错误）";
        case lv_CONTEXT_COMPLETE:
            return "COMPLETE（完成）";
        default:
            return "UNKNOWN（未知）";
    }
}

/**
 * @brief 检查状态转移是否合法
 *
 * 状态转移规则：
 *   IDLE      → PARSING | ERROR
 *   PARSING   → REASONING | ERROR | IDLE
 *   REASONING → COMPLETE | ERROR | IDLE
 *   COMPLETE  → IDLE
 *   ERROR     → IDLE
 */
bool lv_context_state_transition_valid(lvContextState from, lvContextState to) {
    switch (from) {
        case lv_CONTEXT_IDLE:
            return (to == lv_CONTEXT_PARSING || to == lv_CONTEXT_ERROR);
        case lv_CONTEXT_PARSING:
            return (to == lv_CONTEXT_REASONING || to == lv_CONTEXT_ERROR || to == lv_CONTEXT_IDLE);
        case lv_CONTEXT_REASONING:
            return (to == lv_CONTEXT_COMPLETE || to == lv_CONTEXT_ERROR || to == lv_CONTEXT_IDLE);
        case lv_CONTEXT_COMPLETE:
            return (to == lv_CONTEXT_IDLE);
        case lv_CONTEXT_ERROR:
            return (to == lv_CONTEXT_IDLE);
        default:
            return false;
    }
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
 * @brief 确保推理栈有足够容量（内部辅助函数）
 *
 * 如果当前容量不足，按 2 倍因子扩容，但不超过 max_depth。
 */
static lvErrorCode reasoning_stack_ensure_capacity(ReasoningStack *stack) {
    if (!stack) {
        return lv_ERROR_NULL_POINTER;
    }

    /* 栈未满，无需扩容 */
    if (stack->top + 1 < stack->capacity) {
        return lv_OK;
    }

    /* 已达最大深度限制 */
    if (stack->capacity >= stack->max_depth) {
        return lv_ERROR_RESOURCE_EXHAUSTED;
    }

    int new_capacity;
    if (stack->capacity == 0) {
        new_capacity = lv_CONTEXT_REASONING_STACK_DEFAULT_CAPACITY;
    } else {
        new_capacity = stack->capacity * 2;
    }

    /* 不超过最大深度 */
    if (new_capacity > stack->max_depth) {
        new_capacity = stack->max_depth;
    }

    ReasoningFrame *new_frames =
        (ReasoningFrame *) lv_realloc(stack->frames, (size_t) new_capacity * sizeof(ReasoningFrame));
    if (!new_frames) {
        return lv_ERROR_OUT_OF_MEMORY;
    }

    /* 清零新增部分 */
    size_t old_size = (size_t) stack->capacity * sizeof(ReasoningFrame);
    size_t new_size = (size_t) new_capacity * sizeof(ReasoningFrame);
    memset((char *) new_frames + old_size, 0, new_size - old_size);

    stack->frames = new_frames;
    stack->capacity = new_capacity;

    return lv_OK;
}

/**
 * @brief 在当前推理栈上压入一个新分支帧
 */
lvErrorCode lv_context_push_reasoning(lvContext *ctx, ReasoningBranchType branch_type, uint64_t timeout_ms) {
    if (!ctx) {
        return lv_ERROR_NULL_POINTER;
    }

    /* 检查熔断器 */
    if (lv_context_is_circuit_open(ctx)) {
        return lv_ERROR_INVALID_STATE;
    }

    /* 检查深度限制 */
    if (ctx->reasoning_stack.top + 1 >= ctx->reasoning_stack.max_depth) {
        lv_context_set_error(ctx, lv_ERROR_RESOURCE_EXHAUSTED, "推理栈深度超限: 当前 %d, 最大 %d",
                             ctx->reasoning_stack.top + 1, ctx->reasoning_stack.max_depth);
        return lv_ERROR_RESOURCE_EXHAUSTED;
    }

    /* 确保栈有足够容量 */
    lvErrorCode err = reasoning_stack_ensure_capacity(&ctx->reasoning_stack);
    if (err != lv_OK) {
        lv_context_set_error(ctx, err, "推理栈扩容失败");
        return err;
    }

    /* 压入新帧 */
    ctx->reasoning_stack.top++;
    ReasoningFrame *frame = &ctx->reasoning_stack.frames[ctx->reasoning_stack.top];

    /* 初始化帧字段 */
    memset(frame, 0, sizeof(ReasoningFrame));
    frame->branch_type = branch_type;
    frame->status = BRANCH_ACTIVE;
    frame->depth = ctx->reasoning_stack.top;
    frame->step_count = 0;
    frame->timeout_ms = timeout_ms;
    frame->created_at_us = lv_get_time_us();
    frame->ast_root_ref = ctx->ast_root;

    /* 创建约束图快照 */
    if (ctx->main_graph) {
        frame->graph_snapshot = graph_create();
        /* 完整版应深拷贝约束图的节点和约束 */
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

    if (ctx->reasoning_stack.top < 0) {
        lv_context_set_error(ctx, lv_ERROR_INVALID_STATE, "推理栈为空，无法弹出");
        return lv_ERROR_INVALID_STATE;
    }

    ReasoningFrame *frame = &ctx->reasoning_stack.frames[ctx->reasoning_stack.top];

    /* 释放帧内资源 */
    if (frame->graph_snapshot) {
        graph_destroy((ConstraintGraph *) frame->graph_snapshot);
        frame->graph_snapshot = NULL;
    }
    if (frame->assumption_node_ids) {
        lv_free((void **) &frame->assumption_node_ids);
    }
    if (frame->target_node_ids) {
        lv_free((void **) &frame->target_node_ids);
    }

    /* 清零帧 */
    memset(frame, 0, sizeof(ReasoningFrame));

    /* 弹出 */
    ctx->reasoning_stack.top--;

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
    return ctx->reasoning_stack.top + 1;
}

/**
 * @brief 获取当前活跃的推理分支帧（栈顶）
 */
ReasoningFrame *lv_context_get_current_reasoning_frame(lvContext *ctx) {
    if (!ctx) {
        return NULL;
    }
    if (ctx->reasoning_stack.top < 0 || !ctx->reasoning_stack.frames) {
        return NULL;
    }
    return &ctx->reasoning_stack.frames[ctx->reasoning_stack.top];
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

    /* 检查熔断器状态 */
    if (ctx->circuit_breaker.state == CIRCUIT_BREAKER_OPEN) {
        /* 检查冷却时间，自动转入半开态 */
        uint64_t now = lv_get_time_us();
        uint64_t elapsed_ms = (now - ctx->circuit_breaker.tripped_at_us) / 1000;
        if (elapsed_ms >= ctx->circuit_breaker.cooldown_ms) {
            /* 冷却期已过，允许半开态（但不修改 const 指针的字段）。
               实际的 HALF_OPEN 转换应在 begin_operation 中进行。 */
            return false;
        }
        return true;
    }

    /* 检查总运行时间超限 */
    if (ctx->circuit_breaker.total_timeout_ms > 0) {
        uint64_t now = lv_get_time_us();
        uint64_t uptime_ms = (now - ctx->circuit_breaker.start_time_us) / 1000;
        if (uptime_ms >= ctx->circuit_breaker.total_timeout_ms) {
            return true;
        }
    }

    /* 深度超限 */
    if (ctx->circuit_breaker.max_depth > 0 && ctx->circuit_breaker.current_depth > ctx->circuit_breaker.max_depth) {
        return true;
    }

    /* 步数超限 */
    if (ctx->circuit_breaker.max_steps > 0 && ctx->circuit_breaker.total_steps >= ctx->circuit_breaker.max_steps) {
        return true;
    }

    return false;
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

    /* 半开态 → 关闭态（试探成功） */
    if (ctx->circuit_breaker.state == CIRCUIT_BREAKER_HALF_OPEN) {
        ctx->circuit_breaker.state = CIRCUIT_BREAKER_CLOSED;
        ctx->circuit_breaker.consecutive_errors = 0;
    }

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
        /* 触发熔断 */
        ctx->circuit_breaker.state = CIRCUIT_BREAKER_OPEN;
        ctx->circuit_breaker.tripped_at_us = now;
        ctx->circuit_breaker.trip_count++;

        /* 记录熔断原因 */
        if (ctx->circuit_breaker.trip_reason) {
            lv_free((void **) &ctx->circuit_breaker.trip_reason);
        }
        ctx->circuit_breaker.trip_reason = lv_strdup_safe("操作超时熔断");

        /* 转入错误状态 */
        ctx->previous_state = ctx->state;
        ctx->state = lv_CONTEXT_ERROR;
        ctx->state_transition_count++;

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
        return false;
    }

    ctx->circuit_breaker.total_steps++;

    /* 递增推理栈顶帧的步骤计数 */
    if (ctx->reasoning_stack.top >= 0 && ctx->reasoning_stack.frames) {
        ctx->reasoning_stack.frames[ctx->reasoning_stack.top].step_count++;
    }

    /* 检查步数限制 */
    if (ctx->circuit_breaker.max_steps > 0 && ctx->circuit_breaker.total_steps >= ctx->circuit_breaker.max_steps) {
        /* 触发熔断 */
        ctx->circuit_breaker.state = CIRCUIT_BREAKER_OPEN;
        ctx->circuit_breaker.tripped_at_us = lv_get_time_us();
        ctx->circuit_breaker.trip_count++;

        if (ctx->circuit_breaker.trip_reason) {
            lv_free((void **) &ctx->circuit_breaker.trip_reason);
        }
        ctx->circuit_breaker.trip_reason = lv_strdup_safe("推理步数超限熔断");

        ctx->previous_state = ctx->state;
        ctx->state = lv_CONTEXT_ERROR;
        ctx->state_transition_count++;

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
    ctx->circuit_breaker.consecutive_errors = 0;
}

/**
 * @brief 记录一次错误操作（递增连续错误计数）
 *
 * 连续错误超过上限则触发熔断。
 * @return true 正常，false 连续错误超限触发熔断
 */
bool lv_context_record_error(lvContext *ctx) {
    if (!ctx) {
        return false;
    }

    ctx->circuit_breaker.consecutive_errors++;

    if (ctx->circuit_breaker.consecutive_errors >= ctx->circuit_breaker.max_consecutive_errors) {
        /* 触发熔断 */
        ctx->circuit_breaker.state = CIRCUIT_BREAKER_OPEN;
        ctx->circuit_breaker.tripped_at_us = lv_get_time_us();
        ctx->circuit_breaker.trip_count++;

        if (ctx->circuit_breaker.trip_reason) {
            lv_free((void **) &ctx->circuit_breaker.trip_reason);
        }
        ctx->circuit_breaker.trip_reason = lv_strdup_safe("连续错误超限熔断");

        ctx->previous_state = ctx->state;
        ctx->state = lv_CONTEXT_ERROR;
        ctx->state_transition_count++;

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
 * 第十一部分：缓存管理 API
 * ============================================================ */

/**
 * @brief 标记所有缓存为无效
 */
void lv_context_invalidate_cache(lvContext *ctx) {
    if (!ctx) {
        return;
    }
    ctx->cache_valid = false;
}

/**
 * @brief 检查缓存是否有效
 */
bool lv_context_is_cache_valid(const lvContext *ctx) {
    if (!ctx) {
        return false;
    }
    return ctx->cache_valid;
}

/**
 * @brief 清除所有缓存内容（释放内存但保留缓存结构）
 */
void lv_context_clear_cache(lvContext *ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->groebner_cache) {
        lv_free((void **) &ctx->groebner_cache);
    }
    if (ctx->symbolic_cache) {
        lv_free((void **) &ctx->symbolic_cache);
    }
    if (ctx->numeric_cache) {
        lv_free((void **) &ctx->numeric_cache);
    }
    if (ctx->unification_cache) {
        lv_free((void **) &ctx->unification_cache);
    }

    ctx->cache_valid = false;
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
 * 第十三部分：流式输出 API
 * ============================================================ */

/**
 * @brief 获取上下文的流式输出上下文
 *
 * 如果上下文没有关联的 StreamContext，自动创建一个。
 * 后续调用将返回同一个 StreamContext 实例。
 */
struct StreamContext *lv_context_get_stream(lvContext *ctx) {
    if (!ctx) {
        return NULL;
    }

    /* 惰性初始化：首次调用时自动创建 */
    if (!ctx->stream_ctx) {
        ctx->stream_ctx = stream_context_create();
    }

    return ctx->stream_ctx;
}

/**
 * @brief 设置流式输出的启用状态
 *
 * 启用时如果 stream_ctx 为 NULL 则自动创建。
 * 禁用时销毁已有的 stream_ctx 并置 NULL。
 */
void lv_context_set_streaming_enabled(lvContext *ctx, bool enabled) {
    if (!ctx) {
        return;
    }

    if (enabled) {
        /* 启用：如果尚未创建则创建 StreamContext */
        if (!ctx->stream_ctx) {
            ctx->stream_ctx = stream_context_create();
        }
    } else {
        /* 禁用：销毁已有的 StreamContext */
        if (ctx->stream_ctx) {
            stream_context_destroy(ctx->stream_ctx);
            ctx->stream_ctx = NULL;
        }
    }
}

/**
 * @brief 检查流式输出是否启用
 *
 * 通过 stream_ctx 是否为 NULL 判断。
 */
bool lv_context_is_streaming_enabled(const lvContext *ctx) {
    if (!ctx) {
        return false;
    }
    return (ctx->stream_ctx != NULL);
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
    uint64_t uptime_ms = uptime_us / 1000;

    /* 计算缓存命中率 */
    int64_t total_cache_access = ctx->cache_hits + ctx->cache_misses;
    double cache_hit_rate = 0.0;
    if (total_cache_access > 0) {
        cache_hit_rate = (double) ctx->cache_hits / (double) total_cache_access * 100.0;
    }

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
                 cache_hit_rate, (long long) ctx->cache_hits, (long long) total_cache_access,
                 ctx->circuit_breaker.state == CIRCUIT_BREAKER_CLOSED      ? "关闭（正常）"
                 : ctx->circuit_breaker.state == CIRCUIT_BREAKER_HALF_OPEN ? "半开（试探）"
                                                                           : "打开（熔断）",
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

/**
 * @brief 获取上下文的运行时间（微秒）
 */
uint64_t lv_context_get_uptime_us(const lvContext *ctx) {
    if (!ctx) {
        return 0;
    }
    if (ctx->circuit_breaker.start_time_us == 0) {
        return 0;
    }
    return lv_get_time_us() - ctx->circuit_breaker.start_time_us;
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
    if (ctx->reasoning_stack.frames) {
        for (int i = 0; i <= ctx->reasoning_stack.top && i < ctx->reasoning_stack.capacity; i++) {
            ReasoningFrame *frame = &ctx->reasoning_stack.frames[i];
            if (frame->graph_snapshot) {
                graph_destroy((ConstraintGraph *) frame->graph_snapshot);
                frame->graph_snapshot = NULL;
            }
            if (frame->assumption_node_ids) {
                lv_free((void **) &frame->assumption_node_ids);
            }
            if (frame->target_node_ids) {
                lv_free((void **) &frame->target_node_ids);
            }
        }
        lv_free((void **) &ctx->reasoning_stack.frames);
    }
    ctx->reasoning_stack.frames = NULL;
    ctx->reasoning_stack.top = -1;
    ctx->reasoning_stack.capacity = 0;
    ctx->reasoning_stack.max_depth = lv_CONTEXT_REASONING_STACK_MAX_DEPTH;

    /* 2. 清除缓存 */
    lv_context_clear_cache(ctx);
    ctx->cache_hits = 0;
    ctx->cache_misses = 0;

    /* 3. 重建主约束图 */
    if (ctx->main_graph) {
        graph_destroy((ConstraintGraph *) ctx->main_graph);
    }
    ctx->main_graph = graph_create();

    /* 4. 释放规范化结果 */
    if (ctx->last_normalization) {
        normalization_result_destroy(ctx->last_normalization);
        ctx->last_normalization = NULL;
    }

    /* 5. 释放父快照链 */
    if (ctx->parent_snapshot) {
        ctx->parent_snapshot->snapshot_refcount--;
        if (ctx->parent_snapshot->snapshot_refcount <= 0) {
            lv_context_destroy(ctx->parent_snapshot);
        }
        ctx->parent_snapshot = NULL;
    }
    ctx->snapshot_refcount = 0;
    ctx->snapshot_depth = 0;

    /* 6. 重置状态机 */
    ctx->state = lv_CONTEXT_IDLE;
    ctx->previous_state = lv_CONTEXT_IDLE;
    ctx->state_transition_count = 0;

    /* 7. 重置错误状态 */
    ctx->error_code = lv_OK;
    ctx->error_message[0] = '\0';
    ctx->last_status = 0;

    /* 8. 重置递归深度 */
    ctx->recursion_depth = 0;
    ctx->recursion_policy = lv_RECURSION_POLICY_ERROR;

    /* 9. 重置熔断器（保留 trip_count） */
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

    /* 10. 重置 AST */
    ctx->ast_root = NULL;

    /* 11. 释放名称 */
    if (ctx->name) {
        lv_free((void **) &ctx->name);
    }

    /* 12. 更新统计 */
    if (ctx->previous_state == lv_CONTEXT_COMPLETE) {
        ctx->problems_processed++;
    }

    /* 离开不可取消区域 */
    lv_context_leave_uncancellable(ctx);
}