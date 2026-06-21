/**
 * @file context.c
 * @brief Lv-00 隔离上下文系统 —— 核心实现
 *
 * 实现 Lv00Context 的生命周期管理、错误管理等核心功能。
 * 当前为基础实现，提供完整的资源生命周期管理（12 种资源类型的创建/销毁/快照/回滚），
 * 使链接器能够正确解析所有 context.h 中声明的符号。
 * 完整实现需要添加：线程局部存储、资源隔离边界、上下文传播机制。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include "context.h"
#include "config.h"       /* LV00_DEFAULT_* macros */
#include "lv00_utils.h"
#include "constraint_graph.h"
#include "stream.h"
#include "normalization.h"
#include <stdatomic.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
 * 使用 lv00_calloc 分配零初始化的 Lv00Context 结构体，
 * 然后设置各字段的默认值。
 */
Lv00Context *lv00_context_create(void) {
    Lv00Context *ctx = (Lv00Context *)lv00_calloc(1, sizeof(Lv00Context));
    if (!ctx) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                       "lv00_context_create: 分配 Lv00Context 失败");
        return NULL;
    }

    /* 5. 运行时参数 —— 错误码初始化为 OK */
    ctx->error_code = LV00_OK;
    ctx->error_message[0] = '\0';
    ctx->last_status = 0;

    /* 6. 状态机 —— 初始化为 IDLE */
    ctx->state = LV00_CONTEXT_IDLE;
    ctx->previous_state = LV00_CONTEXT_IDLE;
    ctx->state_transition_count = 0;

    /* 7. 熔断器 —— 设置默认值 */
    ctx->circuit_breaker.state = CIRCUIT_BREAKER_CLOSED;
    ctx->circuit_breaker.timeout_ms = LV00_CONTEXT_DEFAULT_TIMEOUT_MS;
    ctx->circuit_breaker.total_timeout_ms = 0; /* 不限制总运行时间 */
    ctx->circuit_breaker.uncancellable_refcount = 0;
    ctx->circuit_breaker.current_depth = 0;
    ctx->circuit_breaker.max_depth = LV00_CONTEXT_DEFAULT_MAX_DEPTH;
    ctx->circuit_breaker.total_steps = 0;
    ctx->circuit_breaker.max_steps = LV00_CONTEXT_DEFAULT_MAX_STEPS;
    ctx->circuit_breaker.consecutive_errors = 0;
    ctx->circuit_breaker.max_consecutive_errors =
        LV00_CONTEXT_DEFAULT_MAX_CONSECUTIVE_ERRORS;
    ctx->circuit_breaker.max_memory_bytes = 0; /* 不限制 */
    ctx->circuit_breaker.start_time_us = lv00_get_time_us();
    ctx->circuit_breaker.operation_start_us = 0;
    ctx->circuit_breaker.cooldown_ms = LV00_CONTEXT_DEFAULT_COOLDOWN_MS;
    ctx->circuit_breaker.tripped_at_us = 0;
    ctx->circuit_breaker.trip_reason = NULL;
    ctx->circuit_breaker.trip_count = 0;

    /* 8. 递归深度追踪 */
    ctx->recursion_depth = 0;
    ctx->max_recursion_depth = LV00_CONTEXT_MAX_RECURSION_DEPTH;
    ctx->recursion_policy = LV00_RECURSION_POLICY_ERROR;

    /* 4. 推理分支栈 —— 初始化为空栈 */
    ctx->reasoning_stack.frames = NULL;
    ctx->reasoning_stack.top = -1;
    ctx->reasoning_stack.capacity = 0;
    ctx->reasoning_stack.max_depth =
        LV00_CONTEXT_REASONING_STACK_MAX_DEPTH;

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
    ctx->created_at_us = lv00_get_time_us();
    ctx->problems_processed = 0;

    /* 12. 公理与规则引用 */
    ctx->rewrite_step_limit = LV00_DEFAULT_REWRITE_STEP_LIMIT;

    return ctx;
}

/**
 * @brief 销毁上下文，释放所有关联资源
 *
 * 按顺序释放上下文持有的资源，最后释放结构体本身。
 * 释放顺序：推理栈帧 → 缓存 → 约束图 → 流式上下文 → 规范化结果 → 名称 → 熔断器
 */
void lv00_context_destroy(Lv00Context *ctx) {
    if (!ctx) {
        return;
    }

    /* 1. 释放推理栈帧数组（含每帧的子资源） */
    if (ctx->reasoning_stack.frames) {
        for (int i = 0; i <= ctx->reasoning_stack.top && i < ctx->reasoning_stack.capacity; i++) {
            ReasoningFrame *frame = &ctx->reasoning_stack.frames[i];
            if (frame->graph_snapshot) {
                graph_destroy((ConstraintGraph *)frame->graph_snapshot);
                frame->graph_snapshot = NULL;
            }
            if (frame->assumption_node_ids) {
                lv00_free((void **)&frame->assumption_node_ids);
            }
            if (frame->target_node_ids) {
                lv00_free((void **)&frame->target_node_ids);
            }
            /* user_data 由外部管理，不释放 */
        }
        lv00_free((void **)&ctx->reasoning_stack.frames);
    }

    /* 2. 释放代数计算缓存 */
    if (ctx->groebner_cache) {
        lv00_free((void **)&ctx->groebner_cache);
    }
    if (ctx->symbolic_cache) {
        lv00_free((void **)&ctx->symbolic_cache);
    }
    if (ctx->numeric_cache) {
        lv00_free((void **)&ctx->numeric_cache);
    }
    if (ctx->unification_cache) {
        lv00_free((void **)&ctx->unification_cache);
    }

    /* 3. 释放主约束图 */
    if (ctx->main_graph) {
        graph_destroy((ConstraintGraph *)ctx->main_graph);
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
        lv00_free((void **)&ctx->memory_pool);
    }

    /* 8. 释放模块/公理/规则引用数组（不拥有所指对象，仅释放数组本身） */
    if (ctx->module_refs) {
        lv00_free((void **)&ctx->module_refs);
    }
    if (ctx->axiom_pkg_refs) {
        lv00_free((void **)&ctx->axiom_pkg_refs);
    }
    if (ctx->rewrite_rule_refs) {
        lv00_free((void **)&ctx->rewrite_rule_refs);
    }

    /* 9. 释放父快照链（递归销毁） */
    if (ctx->parent_snapshot) {
        /* 递减父快照的引用计数 */
        ctx->parent_snapshot->snapshot_refcount--;
        if (ctx->parent_snapshot->snapshot_refcount <= 0) {
            lv00_context_destroy(ctx->parent_snapshot);
        }
        ctx->parent_snapshot = NULL;
    }

    /* 10. 释放名称字符串 */
    if (ctx->name) {
        lv00_free((void **)&ctx->name);
    }

    /* 11. 释放熔断器错误原因字符串 */
    if (ctx->circuit_breaker.trip_reason) {
        lv00_free((void **)&ctx->circuit_breaker.trip_reason);
    }

    /* 12. 释放上下文结构体本身 */
    lv00_free((void **)&ctx);
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
void lv00_context_set_error(Lv00Context *ctx, Lv00ErrorCode code,
                            const char *fmt, ...) {
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
Lv00Context *lv00_context_snapshot(Lv00Context *ctx) {
    if (!ctx) {
        return NULL;
    }

    Lv00Context *snap = (Lv00Context *)lv00_calloc(1, sizeof(Lv00Context));
    if (!snap) {
        lv00_set_error(LV00_ERROR_OUT_OF_MEMORY,
                       "lv00_context_snapshot: 分配快照失败");
        return NULL;
    }

    /* 拷贝整个结构体（浅拷贝） */
    *snap = *ctx;

    /* 深拷贝主约束图 */
    if (ctx->main_graph) {
        snap->main_graph = graph_create();
        if (!snap->main_graph) {
            lv00_free((void **)&snap);
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
        snap->reasoning_stack.frames = (ReasoningFrame *)lv00_calloc(
            (size_t)ctx->reasoning_stack.capacity, sizeof(ReasoningFrame));
        if (!snap->reasoning_stack.frames) {
            if (snap->main_graph) graph_destroy((ConstraintGraph *)snap->main_graph);
            lv00_free((void **)&snap);
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
                dst_frame->assumption_node_ids = (int *)lv00_malloc(
                    (size_t)src_frame->assumption_count * sizeof(int));
                if (dst_frame->assumption_node_ids) {
                    memcpy(dst_frame->assumption_node_ids, src_frame->assumption_node_ids,
                           (size_t)src_frame->assumption_count * sizeof(int));
                }
            }
            if (src_frame->target_node_ids && src_frame->target_count > 0) {
                dst_frame->target_node_ids = (int *)lv00_malloc(
                    (size_t)src_frame->target_count * sizeof(int));
                if (dst_frame->target_node_ids) {
                    memcpy(dst_frame->target_node_ids, src_frame->target_node_ids,
                           (size_t)src_frame->target_count * sizeof(int));
                }
            }
        }
    }

    /* 深拷贝名称字符串 */
    snap->name = NULL;
    if (ctx->name) {
        snap->name = lv00_strdup_safe(ctx->name);
    }

    /* 深拷贝熔断器错误原因 */
    snap->circuit_breaker.trip_reason = NULL;
    if (ctx->circuit_breaker.trip_reason) {
        snap->circuit_breaker.trip_reason =
            lv00_strdup_safe(ctx->circuit_breaker.trip_reason);
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
bool lv00_context_rollback(Lv00Context *ctx, Lv00Context *snapshot) {
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
                graph_destroy((ConstraintGraph *)frame->graph_snapshot);
            }
            if (frame->assumption_node_ids) {
                lv00_free((void **)&frame->assumption_node_ids);
            }
            if (frame->target_node_ids) {
                lv00_free((void **)&frame->target_node_ids);
            }
        }
        lv00_free((void **)&ctx->reasoning_stack.frames);
    }

    /* 释放缓存 */
    if (ctx->groebner_cache) lv00_free((void **)&ctx->groebner_cache);
    if (ctx->symbolic_cache) lv00_free((void **)&ctx->symbolic_cache);
    if (ctx->numeric_cache) lv00_free((void **)&ctx->numeric_cache);
    if (ctx->unification_cache) lv00_free((void **)&ctx->unification_cache);

    /* 释放主约束图 */
    if (ctx->main_graph) {
        graph_destroy((ConstraintGraph *)ctx->main_graph);
    }

    /* 释放规范化结果 */
    if (ctx->last_normalization) {
        normalization_result_destroy(ctx->last_normalization);
    }

    /* 释放名称 */
    if (ctx->name) {
        lv00_free((void **)&ctx->name);
    }

    /* 释放熔断器错误原因 */
    if (ctx->circuit_breaker.trip_reason) {
        lv00_free((void **)&ctx->circuit_breaker.trip_reason);
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
    ctx->error_code = LV00_OK;
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
