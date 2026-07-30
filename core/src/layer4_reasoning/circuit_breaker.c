/**
 * @file circuit_breaker.c
 * @brief 熔断器模块实现
 *
 * @details 实现多维熔断保护机制，包括：
 *          - 三态状态机（CLOSED / HALF_OPEN / OPEN）
 *          - 时间熔断（单次超时、总运行时间）
 *          - 深度熔断（递归/推理深度限制）
 *          - 次数熔断（推理步骤数限制）
 *          - 错误熔断（连续错误计数）
 *          - 冷却恢复机制
 *
 * @version 5.0.0
 */

#include "lv/lv_platform.h"

#include "lv/circuit_breaker.h"

#include <stdio.h>
#include <string.h>

#include "lv/context.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/recursion.h"

/* ============================================================
 * 时间工具
 * ============================================================ */

uint64_t lv_circuit_breaker_now_us(void) {
    return lv_get_time_ns() / 1000;
}

uint64_t lv_circuit_breaker_uptime_us(const CircuitBreaker *cb) {
    if (!cb)
        return 0;
    uint64_t now = lv_circuit_breaker_now_us();
    return (now > cb->start_time_us) ? (now - cb->start_time_us) : 0;
}

/* ============================================================
 * 核心熔断器 API
 * ============================================================ */

bool lv_circuit_breaker_check(lvContext *ctx) {
    if (!ctx)
        return false;

    CircuitBreaker *cb = &ctx->circuit_breaker;
    uint64_t now_us = lv_circuit_breaker_now_us();

    /* 检查总运行时间超时 */
    if (cb->total_timeout_ms > 0) {
        uint64_t uptime_ms = lv_circuit_breaker_uptime_us(cb) / 1000;
        if (uptime_ms > cb->total_timeout_ms && cb->uncancellable_refcount == 0) {
            lv_circuit_breaker_trip(ctx, "总运行时间超时");
            return false;
        }
    }

    /* 检查深度限制 */
    if (cb->max_depth > 0 && cb->current_depth > cb->max_depth) {
        lv_circuit_breaker_trip(ctx, "推理深度超限");
        return false;
    }

    /* 检查步骤数限制 */
    if (cb->max_steps > 0 && cb->total_steps > cb->max_steps) {
        lv_circuit_breaker_trip(ctx, "推理步骤数超限");
        return false;
    }

    /* 检查连续错误数限制 */
    if (cb->max_consecutive_errors > 0 && cb->consecutive_errors >= cb->max_consecutive_errors) {
        lv_circuit_breaker_trip(ctx, "连续错误数超限");
        return false;
    }

    /* 状态机逻辑 */
    switch (cb->state) {
        case CIRCUIT_BREAKER_CLOSED:
            /* 正常状态，允许执行 */
            return true;

        case CIRCUIT_BREAKER_OPEN: {
            /* 检查冷却时间是否已过 */
            if (cb->cooldown_ms > 0 && cb->tripped_at_us > 0) {
                uint64_t elapsed_ms = (now_us - cb->tripped_at_us) / 1000;
                if (elapsed_ms >= cb->cooldown_ms) {
                    /* 冷却完成，进入半开态 */
                    cb->state = CIRCUIT_BREAKER_HALF_OPEN;
                    return true;
                }
            }
            /* 冷却未完成，拒绝执行 */
            return false;
        }

        case CIRCUIT_BREAKER_HALF_OPEN:
            /* 半开态允许一次试探性调用 */
            return true;

        default:
            return false;
    }
}

void lv_circuit_breaker_trip(lvContext *ctx, const char *reason) {
    if (!ctx)
        return;

    CircuitBreaker *cb = &ctx->circuit_breaker;
    cb->state = CIRCUIT_BREAKER_OPEN;
    cb->tripped_at_us = lv_circuit_breaker_now_us();
    cb->trip_count++;

    /* 释放旧的跳闸原因 */
    if (cb->trip_reason) {
        lv_free((void **) &cb->trip_reason);
    }

    /* 复制新的跳闸原因 */
    if (reason) {
        cb->trip_reason = lv_strdup(reason);
    } else {
        cb->trip_reason = lv_strdup("未知原因");
    }
}

void lv_circuit_breaker_reset(lvContext *ctx) {
    if (!ctx)
        return;

    CircuitBreaker *cb = &ctx->circuit_breaker;
    cb->state = CIRCUIT_BREAKER_CLOSED;
    cb->consecutive_errors = 0;
    cb->current_depth = 0;
    cb->tripped_at_us = 0;
    cb->trip_count = 0;

    if (cb->trip_reason) {
        lv_free((void **) &cb->trip_reason);
    }

    cb->start_time_us = lv_circuit_breaker_now_us();
    cb->operation_start_us = cb->start_time_us;
}

void lv_circuit_breaker_record_success(lvContext *ctx) {
    if (!ctx)
        return;

    CircuitBreaker *cb = &ctx->circuit_breaker;

    /* 在半开态下，成功意味着可以恢复到关闭态 */
    if (cb->state == CIRCUIT_BREAKER_HALF_OPEN) {
        cb->state = CIRCUIT_BREAKER_CLOSED;
    }

    /* 重置连续错误计数 */
    cb->consecutive_errors = 0;
}

bool lv_circuit_breaker_record_failure(lvContext *ctx) {
    if (!ctx)
        return false;

    CircuitBreaker *cb = &ctx->circuit_breaker;

    /* 在半开态下，失败意味着重新打开熔断器 */
    if (cb->state == CIRCUIT_BREAKER_HALF_OPEN) {
        lv_circuit_breaker_trip(ctx, "半开态试探失败");
        return false;
    }

    /* 在关闭态下，递增连续错误计数 */
    cb->consecutive_errors++;
    if (cb->max_consecutive_errors > 0 && cb->consecutive_errors >= cb->max_consecutive_errors) {
        lv_circuit_breaker_trip(ctx, "连续错误数超限");
        return false;
    }

    return true; /* 仍在关闭态 */
}

const char *lv_circuit_breaker_state_name(lvContext *ctx) {
    if (!ctx)
        return "无上下文";

    CircuitBreaker *cb = &ctx->circuit_breaker;
    switch (cb->state) {
        case CIRCUIT_BREAKER_CLOSED:
            return "关闭（正常）";
        case CIRCUIT_BREAKER_HALF_OPEN:
            return "半开（试探中）";
        case CIRCUIT_BREAKER_OPEN:
            return "打开（熔断）";
        default:
            return "未知状态";
    }
}

int lv_circuit_breaker_summary(lvContext *ctx, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return 0;

    CircuitBreaker *cb = ctx ? &ctx->circuit_breaker : NULL;
    if (!cb) {
        return snprintf(buf, buf_size, "熔断器：无上下文");
    }

    const char *state_str = lv_circuit_breaker_state_name(ctx);
    uint64_t uptime_ms = lv_circuit_breaker_uptime_us(cb) / 1000;

    return snprintf(buf, buf_size,
                    "熔断器状态：%s | "
                    "连续错误：%d/%d | "
                    "深度：%d/%d | "
                    "步骤：%lld/%lld | "
                    "运行时间：%llu ms | "
                    "熔断次数：%d%s%s",
                    state_str, cb->consecutive_errors, cb->max_consecutive_errors, cb->current_depth, cb->max_depth,
                    (long long) cb->total_steps, (long long) cb->max_steps, (unsigned long long) uptime_ms,
                    cb->trip_count, cb->trip_reason ? " | 原因：" : "", cb->trip_reason ? cb->trip_reason : "");
}

/* ============================================================
 * 全局递归深度保护（轻量级熔断器）
 *
 * 与 lvContext 内的 CircuitBreaker 不同，此处提供
 * 无上下文依赖的全局递归深度保护，供递归调用链中的
 * 任意位置快速检查。lv_MAX_RECURSION_DEPTH (128)
 * 定义在 recursion.h 中。
 * ============================================================ */

/** 当前全局递归深度（进程级） */
static int g_recursion_depth = 0;

/** 全局熔断器是否已触发 */
static bool g_circuit_breaker_triggered = false;

bool lv_recursion_enter(void) {
    if (g_recursion_depth >= lv_MAX_RECURSION_DEPTH) {
        g_circuit_breaker_triggered = true;
        return false;
    }
    g_recursion_depth++;
    return true;
}

void lv_recursion_leave(void) {
    if (g_recursion_depth > 0)
        g_recursion_depth--;
}

bool lv_recursion_circuit_breaker_triggered(void) {
    return g_circuit_breaker_triggered;
}

void lv_recursion_reset(void) {
    g_recursion_depth = 0;
    g_circuit_breaker_triggered = false;
}

int lv_recursion_get_depth(void) {
    return g_recursion_depth;
}
