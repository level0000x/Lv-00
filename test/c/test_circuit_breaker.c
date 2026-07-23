/**
 * @file test_circuit_breaker.c
 * @brief 熔断器模块单元测试
 *
 * 测试 circuit_breaker 模块的核心功能：
 * - 熔断器的初始状态为 CLOSED
 * - check 在 CLOSED 态返回 true
 * - trip 后熔断器进入 OPEN 态，check 返回 false
 * - reset 后熔断器恢复到 CLOSED 态
 * - state_name 返回非空状态字符串
 * - 初始失败计数为 0
 * - 上下文完整生命周期创建/使用/销毁无崩溃
 *
 * 注意：由于 lvContext 结构复杂，本测试采用简化的独立测试方式：
 * 通过 lv_context_create() 创建上下文，直接验证其内嵌熔断器的行为。
 *
 * @details 熔断器状态机：
 *
 *           CLOSED ── trip ──→ OPEN ──→ 冷却后转为 HALF_OPEN
 *             ↑                               │
 *             └────────── reset ──────────────┘
 *             ↑                               │
 *             └── record_success ── HALF_OPEN ←┘
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date   2026-05-24
 */

#include <stdio.h>
#include <string.h>

#include "lv.h"
#include "circuit_breaker.h"
#include "context.h"
#include "test_helpers.h"

/* ============================================================
 * 全局测试计数器
 * ============================================================ */
int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 辅助宏 —— 用于直接检查熔断器状态字段
 * （lv_circuit_breaker_is_open/is_closed/is_half_open
 *  和 get_failure_count 在 circuit_breaker.h 中未提供独立函数，
 *  测试中通过直接读取 ctx->circuit_breaker 字段进行验证。）
 * ============================================================ */

/**
 * @brief 检查熔断器是否处于 OPEN 态
 */
static bool cb_is_open(const struct lvContext *ctx) {
    return ctx != NULL && ctx->circuit_breaker.state == CIRCUIT_BREAKER_OPEN;
}

/**
 * @brief 检查熔断器是否处于 CLOSED 态
 */
static bool cb_is_closed(const struct lvContext *ctx) {
    return ctx != NULL && ctx->circuit_breaker.state == CIRCUIT_BREAKER_CLOSED;
}

/**
 * @brief 检查熔断器是否处于 HALF_OPEN 态
 */
static bool cb_is_half_open(const struct lvContext *ctx) {
    return ctx != NULL && ctx->circuit_breaker.state == CIRCUIT_BREAKER_HALF_OPEN;
}

/**
 * @brief 获取连续错误计数
 */
static int cb_get_failure_count(const struct lvContext *ctx) {
    return ctx ? ctx->circuit_breaker.consecutive_errors : -1;
}

/* ============================================================
 * 测试用例 1: 创建上下文并验证熔断器初始为 CLOSED
 * ============================================================ */

/**
 * @brief 测试熔断器创建时的初始状态
 *
 * 验证通过 lv_context_create() 创建的上下文，其内嵌熔断器
 * 处于 CLOSED（正常）状态。
 */
static void test_circuit_breaker_create(void) {
    lvContext *ctx = lv_context_create();
    TEST_ASSERT_NOT_NULL(ctx);

    /* 验证熔断器初始状态为 CLOSED */
    TEST_ASSERT_MSG(cb_is_closed(ctx),
                    "context created, circuit breaker should be CLOSED");

    lv_context_destroy(ctx);
}

/* ============================================================
 * 测试用例 2: 验证初始状态下的各项检查
 * ============================================================ */

/**
 * @brief 测试熔断器初始状态的各项检查
 *
 * 在刚创建的上下文中：
 * - lv_circuit_breaker_check 应返回 true（允许操作）
 * - 熔断器应处于 CLOSED 态
 * - 熔断器不应处于 OPEN 态
 * - 熔断器不应处于 HALF_OPEN 态
 */
static void test_circuit_breaker_initial_state(void) {
    lvContext *ctx = lv_context_create();
    TEST_ASSERT_NOT_NULL(ctx);

    /* check 在 CLOSED 态应返回 true */
    TEST_ASSERT_MSG(lv_circuit_breaker_check(ctx),
                    "circuit_breaker_check should return true in CLOSED state");

    /* 验证 is_closed 为 true */
    TEST_ASSERT_MSG(cb_is_closed(ctx),
                    "circuit breaker should be CLOSED initially");

    /* 验证 is_open 为 false */
    TEST_ASSERT_MSG(!cb_is_open(ctx),
                    "circuit breaker should NOT be OPEN initially");

    /* 验证 is_half_open 为 false */
    TEST_ASSERT_MSG(!cb_is_half_open(ctx),
                    "circuit breaker should NOT be HALF_OPEN initially");

    lv_context_destroy(ctx);
}

/* ============================================================
 * 测试用例 3: 触发熔断，验证 OPEN 态
 * ============================================================ */

/**
 * @brief 测试熔断器 trip 操作
 *
 * 调用 lv_circuit_breaker_trip 后：
 * - 熔断器应进入 OPEN 态
 * - lv_circuit_breaker_check 应返回 false（拒绝操作）
 * - 熔断器不应再处于 CLOSED 态
 */
static void test_circuit_breaker_trip(void) {
    lvContext *ctx = lv_context_create();
    TEST_ASSERT_NOT_NULL(ctx);

    /* 起始状态应为 CLOSED */
    TEST_ASSERT_MSG(cb_is_closed(ctx),
                    "circuit breaker should be CLOSED before trip");

    /* 触发熔断 */
    lv_circuit_breaker_trip(ctx, "test trip reason");

    /* 验证熔断器进入 OPEN 态 */
    TEST_ASSERT_MSG(cb_is_open(ctx),
                    "circuit breaker should be OPEN after trip");

    /* 验证 check 返回 false */
    TEST_ASSERT_MSG(!lv_circuit_breaker_check(ctx),
                    "circuit_breaker_check should return false when OPEN");

    /* 验证不再是 CLOSED */
    TEST_ASSERT_MSG(!cb_is_closed(ctx),
                    "circuit breaker should NOT be CLOSED after trip");

    lv_context_destroy(ctx);
}

/* ============================================================
 * 测试用例 4: trip 后 reset，验证恢复到 CLOSED
 * ============================================================ */

/**
 * @brief 测试熔断器 reset 操作
 *
 * 先 trip 再 reset：
 * - reset 后熔断器应恢复到 CLOSED 态
 * - check 应重新返回 true
 * - 不应再处于 OPEN 态
 */
static void test_circuit_breaker_reset(void) {
    lvContext *ctx = lv_context_create();
    TEST_ASSERT_NOT_NULL(ctx);

    /* 触发熔断 */
    lv_circuit_breaker_trip(ctx, "test trip before reset");
    TEST_ASSERT_MSG(cb_is_open(ctx),
                    "circuit breaker should be OPEN after trip (before reset)");

    /* 重置熔断器 */
    lv_circuit_breaker_reset(ctx);

    /* 验证恢复到 CLOSED 态 */
    TEST_ASSERT_MSG(cb_is_closed(ctx),
                    "circuit breaker should be CLOSED after reset");

    /* 验证 check 重新返回 true */
    TEST_ASSERT_MSG(lv_circuit_breaker_check(ctx),
                    "circuit_breaker_check should return true after reset");

    /* 验证不再是 OPEN 态 */
    TEST_ASSERT_MSG(!cb_is_open(ctx),
                    "circuit breaker should NOT be OPEN after reset");

    lv_context_destroy(ctx);
}

/* ============================================================
 * 测试用例 5: 验证状态名称字符串
 * ============================================================ */

/**
 * @brief 测试熔断器状态名称函数
 *
 * 验证 lv_circuit_breaker_state_name 在不同状态下
 * 返回非空字符串，且不同状态返回不同字符串。
 */
static void test_circuit_breaker_state_name(void) {
    lvContext *ctx = lv_context_create();
    TEST_ASSERT_NOT_NULL(ctx);

    /* CLOSED 态的名称应非空 */
    const char *name_closed = lv_circuit_breaker_state_name(ctx);
    TEST_ASSERT_NOT_NULL(name_closed);
    TEST_ASSERT_MSG(strlen(name_closed) > 0,
                    "CLOSED state name should not be empty");

    /* trip 后的 OPEN 态名称应非空且不同于 CLOSED */
    lv_circuit_breaker_trip(ctx, "test state name");
    const char *name_open = lv_circuit_breaker_state_name(ctx);
    TEST_ASSERT_NOT_NULL(name_open);
    TEST_ASSERT_MSG(strlen(name_open) > 0,
                    "OPEN state name should not be empty");
    TEST_ASSERT_MSG(strcmp(name_closed, name_open) != 0,
                    "OPEN state name should differ from CLOSED state name");

    /* reset 后恢复 CLOSED，名称应与首次 CLOSED 一致 */
    lv_circuit_breaker_reset(ctx);
    const char *name_closed2 = lv_circuit_breaker_state_name(ctx);
    TEST_ASSERT_NOT_NULL(name_closed2);
    TEST_ASSERT_MSG(strcmp(name_closed, name_closed2) == 0,
                    "state name after reset should match initial CLOSED state name");

    lv_context_destroy(ctx);
}

/* ============================================================
 * 测试用例 6: 验证初始失败计数
 * ============================================================ */

/**
 * @brief 测试熔断器初始连续错误计数
 *
 * 验证新创建的上下文中连续错误计数为 0。
 * 另外验证 trip 后计数不受影响（trip 由外部调用，不经过 record_failure 递增）。
 */
static void test_circuit_breaker_failure_count(void) {
    lvContext *ctx = lv_context_create();
    TEST_ASSERT_NOT_NULL(ctx);

    /* 初始连续错误计数应为 0 */
    TEST_ASSERT_MSG(cb_get_failure_count(ctx) == 0,
                    "initial consecutive error count should be 0");

    /* trip 后仍然为 0（trip 不递增错误计数） */
    lv_circuit_breaker_trip(ctx, "test failure count");
    TEST_ASSERT_MSG(cb_get_failure_count(ctx) == 0,
                    "consecutive error count should remain 0 after direct trip");

    /* reset 后仍为 0 */
    lv_circuit_breaker_reset(ctx);
    TEST_ASSERT_MSG(cb_get_failure_count(ctx) == 0,
                    "consecutive error count should be 0 after reset");

    lv_context_destroy(ctx);
}

/* ============================================================
 * 测试用例 7: 上下文完整生命周期测试
 * ============================================================ */

/**
 * @brief 测试上下文完整生命周期（创建-使用-销毁）
 *
 * 执行一次完整的上下文生命周期：创建、使用熔断器 API、
 * 销毁。验证整个过程不崩溃，且在销毁后上下文已不可用。
 */
static void test_circuit_breaker_context_lifecycle(void) {
    /* 创建上下文 */
    lvContext *ctx = lv_context_create();
    TEST_ASSERT_NOT_NULL(ctx);

    /* 正常使用：check */
    TEST_ASSERT_MSG(lv_circuit_breaker_check(ctx),
                    "circuit_breaker_check should return true in new context");

    /* 正常使用：trip */
    lv_circuit_breaker_trip(ctx, "lifecycle test trip");
    TEST_ASSERT_MSG(cb_is_open(ctx),
                    "circuit breaker should be OPEN after trip in lifecycle test");

    /* 正常使用：state_name */
    const char *name = lv_circuit_breaker_state_name(ctx);
    TEST_ASSERT_NOT_NULL(name);

    /* 正常使用：reset */
    lv_circuit_breaker_reset(ctx);
    TEST_ASSERT_MSG(cb_is_closed(ctx),
                    "circuit breaker should be CLOSED after reset in lifecycle test");

    /* 正常使用：summary */
    char summary_buf[256];
    int summary_len = lv_circuit_breaker_summary(ctx, summary_buf, sizeof(summary_buf));
    TEST_ASSERT_MSG(summary_len > 0,
                    "circuit_breaker_summary should return positive length");
    TEST_ASSERT_MSG((size_t)summary_len < sizeof(summary_buf),
                    "circuit_breaker_summary should not exceed buffer size");

    /* 销毁上下文 —— 不应崩溃 */
    lv_context_destroy(ctx);

    /* 验证 ctx 的 trip_reason 已被正确清理（通过 reset 中的 lv_free）。
     * 由于上下文已销毁，仅做生存性断言：到这里没崩溃即通过。 */
}

/* ============================================================
 * 主入口
 * ============================================================ */

int main(void) {
    TEST_SUITE_BEGIN("Circuit Breaker Module");

    TEST_RUN(test_circuit_breaker_create);
    TEST_RUN(test_circuit_breaker_initial_state);
    TEST_RUN(test_circuit_breaker_trip);
    TEST_RUN(test_circuit_breaker_reset);
    TEST_RUN(test_circuit_breaker_state_name);
    TEST_RUN(test_circuit_breaker_failure_count);
    TEST_RUN(test_circuit_breaker_context_lifecycle);

    TEST_SUITE_END();
    return g_fail_count > 0 ? 1 : 0;
}
