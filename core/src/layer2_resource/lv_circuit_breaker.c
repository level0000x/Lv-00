/**
 * @file lv_circuit_breaker.c
 * @brief 熔断器（Circuit Breaker）独立模块实现
 *
 * @details 实现多维熔断保护机制：
 *          - 三态状态机（CLOSED / HALF_OPEN / OPEN）
 *          - 时间熔断（单次超时、总运行时间）
 *          - 深度熔断（递归/推理深度限制）
 *          - 次数熔断（推理步骤数限制）
 *          - 错误熔断（连续错误计数）
 *
 * @version 1.0.0
 * @date   2026-07-31
 */

#include "lv/lv_circuit_breaker.h"

#include <stdlib.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv/lv_internal.h"

/* ============================================================
 * 内部辅助
 * ============================================================ */

/**
 * @brief 获取当前微秒时间戳
 */
static uint64_t now_us(void) {
    return lv_get_time_us();
}

/**
 * @brief 释放旧的跳闸原因并设置新的
 */
static void set_trip_reason(lvCircuitBreaker *cb, const char *reason) {
    if (cb->trip_reason) {
        lv_free((void **) &cb->trip_reason);
    }
    if (reason) {
        cb->trip_reason = lv_strdup(reason);
    } else {
        cb->trip_reason = lv_strdup("未知原因");
    }
}

/**
 * @brief 将熔断器跳闸到 OPEN 状态
 *
 * 记录跳闸时间、累计熔断次数并更新跳闸原因（内部复制）。
 */
void lv_circuit_breaker_do_trip(lvCircuitBreaker *cb, const char *reason) {
    if (!cb) {
        return;
    }
    cb->state = lv_CB_OPEN;
    cb->tripped_at_us = now_us();
    cb->trip_count++;
    set_trip_reason(cb, reason);
}

/* ============================================================
 * 核心 API 实现
 * ============================================================ */

void lv_circuit_breaker_init(lvCircuitBreaker *cb) {
    if (!cb) {
        return;
    }

    memset(cb, 0, sizeof(*cb));
    cb->state = lv_CB_CLOSED;
    cb->timeout_ms = lv_CB_DEFAULT_TIMEOUT_MS;
    cb->total_timeout_ms = 0;
    cb->uncancellable_refcount = 0;
    cb->current_depth = 0;
    cb->max_depth = lv_CB_DEFAULT_MAX_DEPTH;
    cb->total_steps = 0;
    cb->max_steps = lv_CB_DEFAULT_MAX_STEPS;
    cb->consecutive_errors = 0;
    cb->max_consecutive_errors = lv_CB_DEFAULT_MAX_ERRORS;
    cb->max_memory_bytes = 0;
    cb->start_time_us = now_us();
    cb->operation_start_us = 0;
    cb->cooldown_ms = lv_CB_DEFAULT_COOLDOWN_MS;
    cb->tripped_at_us = 0;
    cb->trip_reason = NULL;
    cb->trip_count = 0;
}

bool lv_circuit_breaker_is_tripped(const lvCircuitBreaker *cb) {
    if (!cb) {
        return true;
    }

    /* 检查熔断器状态机 */
    if (cb->state == lv_CB_OPEN) {
        /* 冷却未完成 → 已跳闸 */
        if (cb->cooldown_ms > 0 && cb->tripped_at_us > 0) {
            uint64_t elapsed_ms = (now_us() - cb->tripped_at_us) / 1000;
            if (elapsed_ms < cb->cooldown_ms) {
                return true;
            }
        }
        /* 冷却已完成 → 可恢复，不算已跳闸 */
    }

    /* 检查总运行时间超限 */
    if (cb->total_timeout_ms > 0) {
        uint64_t uptime_ms = (now_us() - cb->start_time_us) / lv_US_PER_MS;
        if (uptime_ms >= cb->total_timeout_ms) {
            return true;
        }
    }

    /* 深度超限 */
    if (cb->max_depth > 0 && cb->current_depth > cb->max_depth) {
        return true;
    }

    /* 步数超限 */
    if (cb->max_steps > 0 && cb->total_steps >= cb->max_steps) {
        return true;
    }

    /* 连续错误超限 */
    if (cb->max_consecutive_errors > 0 && cb->consecutive_errors >= cb->max_consecutive_errors) {
        return true;
    }

    return false;
}

void lv_circuit_breaker_record_success(lvCircuitBreaker *cb) {
    if (!cb) {
        return;
    }

    /* 在半开态下，成功意味着可以恢复到关闭态 */
    if (cb->state == lv_CB_HALF_OPEN) {
        cb->state = lv_CB_CLOSED;
    }

    /* 重置连续错误计数 */
    cb->consecutive_errors = 0;
}

bool lv_circuit_breaker_record_error(lvCircuitBreaker *cb) {
    if (!cb) {
        return false;
    }

    /* 在半开态下，失败意味着重新打开熔断器 */
    if (cb->state == lv_CB_HALF_OPEN) {
        lv_circuit_breaker_do_trip(cb, "半开态试探失败");
        return false;
    }

    /* 在关闭态下，递增连续错误计数 */
    cb->consecutive_errors++;

    /* 检查是否超过连续错误上限 */
    if (cb->max_consecutive_errors > 0 && cb->consecutive_errors >= cb->max_consecutive_errors) {
        lv_circuit_breaker_do_trip(cb, "连续错误数超限");
        return false;
    }

    return true;
}

void lv_circuit_breaker_reset(lvCircuitBreaker *cb) {
    if (!cb) {
        return;
    }

    /* 保留熔断次数（统计用途） */
    int preserved_trip_count = cb->trip_count;
    char *old_reason = cb->trip_reason;

    memset(cb, 0, sizeof(*cb));
    cb->state = lv_CB_CLOSED;
    cb->timeout_ms = lv_CB_DEFAULT_TIMEOUT_MS;
    cb->max_depth = lv_CB_DEFAULT_MAX_DEPTH;
    cb->max_steps = lv_CB_DEFAULT_MAX_STEPS;
    cb->max_consecutive_errors = lv_CB_DEFAULT_MAX_ERRORS;
    cb->cooldown_ms = lv_CB_DEFAULT_COOLDOWN_MS;
    cb->start_time_us = now_us();
    cb->operation_start_us = 0;
    cb->tripped_at_us = 0;
    cb->trip_count = preserved_trip_count;

    /* 释放旧的跳闸原因（由 memset 置 NULL 后重新设置） */
    if (old_reason) {
        lv_free((void **) &old_reason);
    }
    cb->trip_reason = NULL;
}

lvCircuitBreakerState lv_circuit_breaker_state(const lvCircuitBreaker *cb) {
    if (!cb) {
        return lv_CB_CLOSED;
    }
    return cb->state;
}

bool lv_circuit_breaker_check_guarded(lvCircuitBreaker *cb) {
    if (!cb) {
        return false;
    }

    uint64_t now_us = lv_get_time_us();

    /* 检查总运行时间超时（不可取消区域不触发超时熔断） */
    if (cb->total_timeout_ms > 0) {
        uint64_t uptime_ms = (now_us > cb->start_time_us) ? (now_us - cb->start_time_us) / 1000 : 0;
        if (uptime_ms > cb->total_timeout_ms && cb->uncancellable_refcount == 0) {
            lv_circuit_breaker_do_trip(cb, "总运行时间超时");
            return false;
        }
    }

    /* 检查深度限制 */
    if (cb->max_depth > 0 && cb->current_depth > cb->max_depth) {
        lv_circuit_breaker_do_trip(cb, "推理深度超限");
        return false;
    }

    /* 检查步骤数限制 */
    if (cb->max_steps > 0 && cb->total_steps > cb->max_steps) {
        lv_circuit_breaker_do_trip(cb, "推理步骤数超限");
        return false;
    }

    /* 检查连续错误数限制 */
    if (cb->max_consecutive_errors > 0 && cb->consecutive_errors >= cb->max_consecutive_errors) {
        lv_circuit_breaker_do_trip(cb, "连续错误数超限");
        return false;
    }

    /* 状态机逻辑 */
    /* exempt: 1-B 状态机豁免 —— 熔断器 CLOSED/OPEN/HALF_OPEN 状态机：
     * 语义为"熔断保护"（OPEN 冷却后试探进入 HALF_OPEN，一次试探调用放行后
     * 由调用方决定 close 或再次 trip），无转移矩阵/位掩码查表，
     * 与 context.c/engine_state.c 的"推理任务五态状态机"语义异构，不迁移。 */
    switch (cb->state) {
        case lv_CB_CLOSED:
            /* 正常状态，允许执行 */
            return true;

        case lv_CB_OPEN: {
            /* 检查冷却时间是否已过 */
            if (cb->cooldown_ms > 0 && cb->tripped_at_us > 0) {
                uint64_t elapsed_ms = (now_us - cb->tripped_at_us) / lv_US_PER_MS;
                if (elapsed_ms >= cb->cooldown_ms) {
                    /* 冷却完成，进入半开态 */
                    cb->state = lv_CB_HALF_OPEN;
                    return true;
                }
            }
            /* 冷却未完成，拒绝执行 */
            return false;
        }

        case lv_CB_HALF_OPEN:
            /* 半开态允许一次试探性调用 */
            return true;

        default:
            return false;
    }
}
