/**
 * @file context.c
 * @brief Lv-00 隔离上下文系统 —— 核心实现
 *
 * 实现 Lv00Context 的生命周期管理、错误管理等核心功能。
 * 当前为桩实现（stub），提供基本的创建/销毁/错误设置功能，
 * 使链接器能够正确解析所有 context.h 中声明的符号。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-25
 */

#include "context.h"
#include "lv00_utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
    ctx->context_id = s_next_context_id++;
    ctx->created_at_us = lv00_get_time_us();
    ctx->problems_processed = 0;

    /* 12. 公理与规则引用 */
    ctx->rewrite_step_limit = 1000;

    return ctx;
}

/**
 * @brief 销毁上下文，释放所有关联资源
 *
 * 按顺序释放上下文持有的资源，最后释放结构体本身。
 * 当前为桩实现，仅释放上下文结构体和名称字符串。
 */
void lv00_context_destroy(Lv00Context *ctx) {
    if (!ctx) {
        return;
    }

    /* 释放推理栈帧数组 */
    if (ctx->reasoning_stack.frames) {
        lv00_free((void **)&ctx->reasoning_stack.frames);
    }

    /* 释放名称字符串 */
    if (ctx->name) {
        lv00_free((void **)&ctx->name);
    }

    /* 释放熔断器错误原因字符串 */
    if (ctx->circuit_breaker.trip_reason) {
        lv00_free((void **)&ctx->circuit_breaker.trip_reason);
    }

    /* 释放上下文结构体本身 */
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
