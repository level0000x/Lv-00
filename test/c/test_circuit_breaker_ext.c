/**
 * @file test_circuit_breaker_ext.c
 * @brief 熔断器向后兼容层契约测试（批次 C-㊺续33：circuit_breaker.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（3 个）：
 *   lv_circuit_breaker_now_us / uptime_us / record_failure（ctx 版）
 *
 * 契约要点（与 circuit_breaker.c / lv_circuit_breaker.c 核对）：
 *   - now_us：单调微秒时间戳（> 0，后续调用 ≥ 先前）。
 *   - uptime_us：cb NULL → 0；now <= start_time → 0；否则差值。
 *   - record_failure：ctx NULL → false；CLOSED 递增连续错误计数，达到
 *     max_consecutive_errors 跳闸并返回 false。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/circuit_breaker.h"
#include "lv/context.h"
#include "lv/lv_circuit_breaker.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：时间戳 ============== */

static void test_now_us(void) {
    uint64_t t1 = lv_circuit_breaker_now_us();
    uint64_t t2 = lv_circuit_breaker_now_us();
    TEST_ASSERT(t1 > 0, "timestamp positive");
    TEST_ASSERT(t2 >= t1, "timestamp monotonic");
}

/* ============== 测试：运行时间 ============== */

static void test_uptime_us(void) {
    /* NULL 契约 */
    TEST_ASSERT_EQ((unsigned long long) lv_circuit_breaker_uptime_us(NULL), 0ULL);

    /* 以当前时间为起点：运行时间接近 0 */
    lvCircuitBreaker cb;
    memset(&cb, 0, sizeof(cb));
    cb.start_time_us = lv_circuit_breaker_now_us();
    uint64_t up = lv_circuit_breaker_uptime_us(&cb);
    TEST_ASSERT(up < 100000, "uptime small right after start");

    /* start_time 在未来：返回 0 */
    cb.start_time_us = lv_circuit_breaker_now_us() + 1000000;
    TEST_ASSERT_EQ((unsigned long long) lv_circuit_breaker_uptime_us(&cb), 0ULL);
}

/* ============== 测试：失败记录（ctx 版） ============== */

static void test_record_failure(void) {
    /* NULL 契约 */
    TEST_ASSERT(!lv_circuit_breaker_record_failure(NULL), "NULL ctx false");

    lvContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.circuit_breaker.state = lv_CB_CLOSED;
    ctx.circuit_breaker.max_consecutive_errors = 3;
    ctx.circuit_breaker.consecutive_errors = 0;

    /* 前 2 次：仍 CLOSED，返回 true */
    TEST_ASSERT(lv_circuit_breaker_record_failure(&ctx), "failure 1");
    TEST_ASSERT(lv_circuit_breaker_record_failure(&ctx), "failure 2");

    /* 第 3 次：达到上限跳闸，返回 false */
    TEST_ASSERT(!lv_circuit_breaker_record_failure(&ctx), "failure 3 trips");
    TEST_ASSERT_EQ((int) ctx.circuit_breaker.state, (int) lv_CB_OPEN);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("CircuitBreakerExt")

    printf("\n--- circuit_breaker compat (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_now_us);
    TEST_MAIN_RUN(test_uptime_us);
    TEST_MAIN_RUN(test_record_failure);

TEST_MAIN_END()
