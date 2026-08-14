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
#include "lv/lv_xmacro.h"
#include "lv/recursion.h"

/* ============================================================
 * 时间工具
 * ============================================================ */

uint64_t lv_circuit_breaker_now_us(void) {
    return lv_get_time_us();
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

    /* 核心实现：维度检查（含 trip）与冷却迁移（OPEN→HALF_OPEN）由核心熔断器处理 */
    return lv_circuit_breaker_check_guarded(&ctx->circuit_breaker);
}

void lv_circuit_breaker_trip(lvContext *ctx, const char *reason) {
    if (!ctx)
        return;

    /* 核心实现：状态/时间/计数/原因均由核心熔断器记录 */
    lv_circuit_breaker_do_trip(&ctx->circuit_breaker, reason);
}

bool lv_circuit_breaker_record_failure(lvContext *ctx) {
    if (!ctx)
        return false;

    /* 核心实现：与 lv_circuit_breaker_record_error 语义一致 */
    return lv_circuit_breaker_record_error(&ctx->circuit_breaker);
}

const char *lv_circuit_breaker_state_name(lvContext *ctx) {
    if (!ctx)
        return "无上下文";
    /* 委托 L2 规范实现（状态名表与枚举同属 L2，避免跨层重复维护） */
    return lv_circuit_breaker_state_name_cb(&ctx->circuit_breaker);
}

int lv_circuit_breaker_summary(lvContext *ctx, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return 0;

    CircuitBreaker *cb = ctx ? &ctx->circuit_breaker : NULL;
    if (!cb) {
        return snprintf(buf, buf_size, "熔断器：无上下文");
    }

    const char *state_str = lv_circuit_breaker_state_name(ctx);
    uint64_t uptime_ms = lv_circuit_breaker_uptime_us(cb) / lv_US_PER_MS;

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
 * 线程局部递归深度保护（轻量级熔断器）
 *
 * 与 lvContext 内的 CircuitBreaker 不同，此处提供
 * 无上下文依赖的全局递归深度保护，供递归调用链中的
 * 任意位置快速检查。lv_MAX_RECURSION_DEPTH (128)
 * 定义在 recursion.h 中。
 *
 * 深度计数器与熔断标志均声明为线程局部（lv_THREAD_LOCAL），
 * 消除"进程级共享被多线程互踩"的跨线程语义错误：
 * 每个线程维护自己的递归深度，线程 A 的 enter/leave
 * 不再影响线程 B 的深度与熔断状态。
 * ============================================================ */

/** 当前线程递归深度（线程局部） */
static lv_THREAD_LOCAL int g_recursion_depth = 0;

/** 当前线程熔断器是否已触发（线程局部） */
static lv_THREAD_LOCAL bool g_circuit_breaker_triggered = false;

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
