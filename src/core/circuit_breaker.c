/**
 * @file circuit_breaker.c
 * @brief 熔断器实现 —— 多维资源保护与自动恢复
 *
 * @details 实现熔断器的核心操作逻辑，包括状态检查、跳闸触发、
 *          冷却计时、半开态试探和成功/失败记录。
 *
 *          熔断器监控维度（在 context.h 的 CircuitBreaker 中定义）：
 *          - 时间熔断：单次操作超时
 *          - 深度熔断：递归/重写深度超限
 *          - 次数熔断：推理步数超限
 *          - 内存熔断：内存使用超限
 *          - 错误熔断：连续错误次数超限
 *
 *          状态转移逻辑：
 *
 *           ┌──────────────────────────────────────┐
 *           │              CLOSED                   │
 *           │  - 允许所有操作                        │
 *           │  - 累计连续错误计数                     │
 *           │  - 超过阈值 → OPEN + 记录原因          │
 *           └──────┬───────────────────────────────┘
 *                  │ 连续错误超限
 *                  ▼
 *           ┌──────────────────────────────────────┐
 *           │               OPEN                    │
 *           │  - 拒绝所有操作                        │
 *           │  - 启动冷却计时器                      │
 *           │  - 每次 check 检查是否过了冷却时间       │
 *           └──────┬───────────────────────────────┘
 *                  │ 冷却时间已过
 *                  ▼
 *           ┌──────────────────────────────────────┐
 *           │            HALF_OPEN                  │
 *           │  - 允许一次试探操作                     │
 *           │  - 试探成功 → CLOSED + 清零错误        │
 *           │  - 试探失败 → OPEN + 重新计时          │
 *           └──────────────────────────────────────┘
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-24
 *
 * @dependencies
 *   - circuit_breaker.h    : 熔断器公共接口
 *   - context.h            : CircuitBreaker 和 Lv00Context 定义
 *   - lv00_internal.h      : 内部常量和宏
 *   - debug.h              : 日志系统（用于跳闸/恢复记录）
 */

#include "circuit_breaker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "context.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ============================================================
 * 辅助函数：高精度时间戳
 * ============================================================ */

/**
 * @brief 获取当前单调时间戳（微秒）
 *
 * Windows: QueryPerformanceCounter + QueryPerformanceFrequency
 * POSIX:   clock_gettime(CLOCK_MONOTONIC)
 *
 * 使用单调时钟，不受系统时间调整影响。
 *
 * @return 微秒时间戳
 */
uint64_t lv00_circuit_breaker_now_us(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    static LARGE_INTEGER freq_cached = {0};
    /* 一次性缓存频率值（线程安全：多个线程同时执行仅导致重复计算，
     * 结果一致且无副作用） */
    if (freq_cached.QuadPart == 0) {
        QueryPerformanceFrequency(&freq_cached);
    }
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000ULL) / freq_cached.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
#endif
}

/**
 * @brief 获取熔断器的运行时间（微秒）
 *
 * @param cb 熔断器指针（非 NULL）
 * @return 自 start_time_us 以来的微秒数
 */
uint64_t lv00_circuit_breaker_uptime_us(const struct CircuitBreaker *cb) {
    if (!cb || cb->start_time_us == 0) {
        return 0;
    }
    uint64_t now = lv00_circuit_breaker_now_us();
    /* 处理时间回绕（理论上不可能发生，但做防御性处理） */
    if (now < cb->start_time_us) {
        return 0;
    }
    return now - cb->start_time_us;
}

/* ============================================================
 * 核心熔断器 API 实现
 * ============================================================ */

/**
 * @brief 检查熔断器是否允许操作继续
 *
 * 【核心逻辑】
 * 1. CLOSED 态 → 直接放行
 * 2. OPEN 态 → 检查冷却时间是否已过
 *    - 未过：拒绝
 *    - 已过：自动转为 HALF_OPEN（允许一次试探）
 * 3. HALF_OPEN 态 → 允许通过（试探性调用）
 *
 * @param ctx 上下文（非 NULL）
 * @return true 允许操作，false 拒绝
 */
bool lv00_circuit_breaker_check(struct Lv00Context *ctx) {
    if (!ctx) {
        return false; /* 无效上下文视为拒绝 */
    }

    CircuitBreaker *cb = &ctx->circuit_breaker;

    switch (cb->state) {
        case CIRCUIT_BREAKER_CLOSED:
            /* 关闭态：正常执行 */
            return true;

        case CIRCUIT_BREAKER_OPEN: {
            /* 打开态：检查冷却时间 */
            uint64_t now = lv00_circuit_breaker_now_us();
            uint64_t elapsed = (now >= cb->tripped_at_us)
                                   ? (now - cb->tripped_at_us)
                                   : 0;

            if (elapsed < cb->cooldown_ms * 1000ULL) {
                /* 冷却未完成，继续拒绝 */
                LOG_WARN("circuit_breaker",
                         "熔断器拒绝操作: 仍在冷却中（已过 %llu us / 需 %llu us）",
                         (unsigned long long)elapsed,
                         (unsigned long long)(cb->cooldown_ms * 1000ULL));
                return false;
            }

            /* 冷却完成：自动进入半开态，允许一次试探 */
            cb->state = CIRCUIT_BREAKER_HALF_OPEN;
            LOG_INFO("circuit_breaker",
                     "熔断器进入半开态: 冷却完成（原因: %s）",
                     cb->trip_reason ? cb->trip_reason : "未知");
            return true;
        }

        case CIRCUIT_BREAKER_HALF_OPEN:
            /* 半开态：允许试探性调用。
             * 注意：此调用只允许一次。调用结果由
             * record_success / record_failure 处理。 */
            return true;

        default:
            /* 无效状态：保守地拒绝 */
            LOG_ERROR("circuit_breaker", "熔断器处于未知状态 %d，拒绝操作", (int)cb->state);
            return false;
    }
}

/**
 * @brief 触发熔断器跳闸
 *
 * 将熔断器从任意状态转为 OPEN，记录跳闸原因和时间。
 * 同时递增 trip_count 用于生命周期统计。
 *
 * @param ctx    上下文（非 NULL）
 * @param reason 跳闸原因的可读描述
 */
void lv00_circuit_breaker_trip(struct Lv00Context *ctx, const char *reason) {
    if (!ctx) {
        return;
    }

    CircuitBreaker *cb = &ctx->circuit_breaker;

    /* 记录跳闸前的状态（用于日志） */
    const char *prev_state_name = lv00_circuit_breaker_state_name(ctx);

    /* 设置 OPEN 状态 */
    cb->state = CIRCUIT_BREAKER_OPEN;

    /* 存储跳闸原因（释放旧原因，复制新原因） */
    if (cb->trip_reason) {
        lv00_free((void **)&cb->trip_reason);
    }
    cb->trip_reason = reason ? lv00_strdup_safe(reason) : NULL;

    /* 记录跳闸时间 */
    cb->tripped_at_us = lv00_circuit_breaker_now_us();

    /* 递增跳闸计数 */
    cb->trip_count++;

    /* 记录日志 */
    LOG_WARN("circuit_breaker",
             "熔断器跳闸 #%d: 从 %s 进入 OPEN 态"
             "（原因: %s, 冷却: %llu ms）",
             cb->trip_count,
             prev_state_name,
             cb->trip_reason ? cb->trip_reason : "未指定",
             (unsigned long long)cb->cooldown_ms);

    /* 同步上下文状态到 ERROR */
    if (ctx->state != LV00_CONTEXT_ERROR) {
        lv00_context_set_error(ctx, LV00_ERROR_CIRCUIT_OPEN,
                               "熔断器跳闸: %s",
                               cb->trip_reason ? cb->trip_reason : "未指定原因");
    }
}

/**
 * @brief 重置熔断器到 CLOSED 状态
 *
 * 清除所有运行时状态：错误计数、跳闸原因、冷却计时。
 * 保留 trip_count 用于生命周期统计。
 *
 * @param ctx 上下文（非 NULL）
 */
void lv00_circuit_breaker_reset(struct Lv00Context *ctx) {
    if (!ctx) {
        return;
    }

    CircuitBreaker *cb = &ctx->circuit_breaker;

    LOG_INFO("circuit_breaker",
             "熔断器重置: 从 %s 恢复到 CLOSED 态（累计跳闸 %d 次）",
             lv00_circuit_breaker_state_name(ctx),
             cb->trip_count);

    /* 重置状态 */
    cb->state = CIRCUIT_BREAKER_CLOSED;

    /* 清除连续错误计数 */
    cb->consecutive_errors = 0;

    /* 清除跳闸原因 */
    if (cb->trip_reason) {
        lv00_free((void **)&cb->trip_reason);
        cb->trip_reason = NULL;
    }

    /* 重置时间戳 */
    cb->start_time_us = lv00_circuit_breaker_now_us();
    cb->tripped_at_us = 0;
    cb->operation_start_us = 0;

    /* 保留 trip_count —— 不清零，用于生命周期统计 */
}

/**
 * @brief 记录一次成功操作
 *
 * HALF_OPEN 态：成功 → 恢复为 CLOSED（熔断器恢复正常）。
 * CLOSED 态：重置连续错误计数（成功操作证明系统健康）。
 * OPEN 态：不应在此状态下记录成功（防御性处理：忽略）。
 *
 * @param ctx 上下文（非 NULL）
 */
void lv00_circuit_breaker_record_success(struct Lv00Context *ctx) {
    if (!ctx) {
        return;
    }

    CircuitBreaker *cb = &ctx->circuit_breaker;

    switch (cb->state) {
        case CIRCUIT_BREAKER_HALF_OPEN:
            /* 试探成功！恢复正常 */
            cb->state = CIRCUIT_BREAKER_CLOSED;
            cb->consecutive_errors = 0;
            cb->tripped_at_us = 0;
            if (cb->trip_reason) {
                lv00_free((void **)&cb->trip_reason);
                cb->trip_reason = NULL;
            }
            LOG_INFO("circuit_breaker",
                     "熔断器恢复: 半开态试探成功，回到 CLOSED 态（累计跳闸 %d 次）",
                     cb->trip_count);
            break;

        case CIRCUIT_BREAKER_CLOSED:
            /* 正常成功：重置连续错误计数 */
            cb->consecutive_errors = 0;
            break;

        case CIRCUIT_BREAKER_OPEN:
            /* OPEN 态不应该有成功操作（被 check 拒绝）。
             * 防御性处理：记录警告但不改变状态。 */
            LOG_WARN("circuit_breaker",
                     "熔断器在 OPEN 态收到成功记录（应该被 check 拒绝），忽略");
            break;

        default:
            break;
    }
}

/**
 * @brief 记录一次失败操作
 *
 * HALF_OPEN 态：失败 → 立即回到 OPEN（冷却时间重新计算）。
 * CLOSED 态：递增错误计数 → 超过阈值则触发跳闸。
 * OPEN 态：不应在此状态下记录失败（防御性处理：忽略）。
 *
 * @param ctx 上下文（非 NULL）
 * @return true  熔断器仍在 CLOSED 态（还可以继续尝试）
 *         false 熔断器已跳闸或已处于 OPEN/HALF_OPEN
 */
bool lv00_circuit_breaker_record_failure(struct Lv00Context *ctx) {
    if (!ctx) {
        return false;
    }

    CircuitBreaker *cb = &ctx->circuit_breaker;

    switch (cb->state) {
        case CIRCUIT_BREAKER_HALF_OPEN:
            /* 试探失败：立即回到 OPEN 态 */
            cb->state = CIRCUIT_BREAKER_OPEN;
            cb->tripped_at_us = lv00_circuit_breaker_now_us();
            cb->trip_count++;
            LOG_WARN("circuit_breaker",
                     "熔断器重新跳闸 #%d: 半开态试探失败，回到 OPEN 态"
                     "（连续错误: %d）",
                     cb->trip_count,
                     cb->consecutive_errors + 1);
            return false;

        case CIRCUIT_BREAKER_CLOSED: {
            /* 递增连续错误计数 */
            cb->consecutive_errors++;

            /* 检查是否超过阈值 */
            if (cb->max_consecutive_errors > 0 &&
                cb->consecutive_errors >= cb->max_consecutive_errors) {
                /* 触发跳闸 */
                char reason_buf[128];
                snprintf(reason_buf, sizeof(reason_buf),
                         "连续错误次数超限: %d/%d",
                         cb->consecutive_errors,
                         cb->max_consecutive_errors);
                lv00_circuit_breaker_trip(ctx, reason_buf);
                return false;
            }

            /* 还在容错范围内 */
            return true;
        }

        case CIRCUIT_BREAKER_OPEN:
            /* OPEN 态不应该有失败记录（被 check 拒绝）。
             * 防御性处理：不做额外操作。 */
            return false;

        default:
            return false;
    }
}

/**
 * @brief 获取熔断器状态的中文名称
 *
 * @param ctx 上下文（可为 NULL）
 * @return 状态中文名称字符串（静态，无需释放）
 */
const char *lv00_circuit_breaker_state_name(struct Lv00Context *ctx) {
    if (!ctx) {
        return "无效";
    }

    switch (ctx->circuit_breaker.state) {
        case CIRCUIT_BREAKER_CLOSED:
            return "关闭（正常）";
        case CIRCUIT_BREAKER_OPEN:
            return "打开（熔断）";
        case CIRCUIT_BREAKER_HALF_OPEN:
            return "半开（试探）";
        default:
            return "未知";
    }
}

/**
 * @brief 获取熔断器健康摘要
 *
 * 生成包含以下信息的可读摘要：
 * - 当前状态、连续错误数、跳闸总数
 * - 冷却时间和剩余冷却
 * - 上次跳闸原因（如果有）
 * - 运行时间
 *
 * @param ctx      上下文（非 NULL）
 * @param buf      输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 实际写入字符数（不含终止符）
 */
int lv00_circuit_breaker_summary(struct Lv00Context *ctx, char *buf, size_t buf_size) {
    if (!ctx || !buf || buf_size == 0) {
        return 0;
    }

    const CircuitBreaker *cb = &ctx->circuit_breaker;
    uint64_t uptime = lv00_circuit_breaker_uptime_us(cb);

    /* 计算剩余冷却时间 */
    const char *cooldown_info = "N/A";
    char cooldown_buf[64] = {0};
    if (cb->state == CIRCUIT_BREAKER_OPEN && cb->tripped_at_us > 0) {
        uint64_t now = lv00_circuit_breaker_now_us();
        uint64_t elapsed = (now >= cb->tripped_at_us) ? (now - cb->tripped_at_us) : 0;
        uint64_t remaining_us = (cb->cooldown_ms * 1000ULL > elapsed)
                                    ? (cb->cooldown_ms * 1000ULL - elapsed)
                                    : 0;
        snprintf(cooldown_buf, sizeof(cooldown_buf),
                 "剩余 %llu ms",
                 (unsigned long long)(remaining_us / 1000ULL));
        cooldown_info = cooldown_buf;
    }

    return snprintf(buf, buf_size,
                    "熔断器: %s | 连续错误: %d/%d | 总跳闸: %d | "
                    "冷却: %llu ms (%s) | 运行: %llu ms | "
                    "最后跳闸原因: %s",
                    lv00_circuit_breaker_state_name(ctx),
                    cb->consecutive_errors,
                    cb->max_consecutive_errors,
                    cb->trip_count,
                    (unsigned long long)cb->cooldown_ms,
                    cooldown_info,
                    (unsigned long long)(uptime / 1000ULL),
                    cb->trip_reason ? cb->trip_reason : "无");
}
