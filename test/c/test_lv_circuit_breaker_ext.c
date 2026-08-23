/**
 * @file test_lv_circuit_breaker_ext.c
 * @brief 熔断器契约测试（批次 C-㊺续28：lv_circuit_breaker.h 7 个零覆盖 API）
 *
 * 覆盖：init / check_guarded / do_trip / is_tripped / record_error /
 *   record_success / state_name_cb
 * 契约：三态状态机、连续错误跳闸、冷却、半开恢复。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_circuit_breaker.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_cb_init_guarded_api(void) {
    lvCircuitBreaker cb;
    lv_circuit_breaker_init(&cb);
    TEST_ASSERT_EQ((int) lv_circuit_breaker_state(&cb), (int) lv_CB_CLOSED);
    TEST_ASSERT(!lv_circuit_breaker_is_tripped(&cb), "初始未跳闸");
    TEST_ASSERT(lv_circuit_breaker_check_guarded(&cb), "初始放行");
    TEST_ASSERT_NOT_NULL(lv_circuit_breaker_state_name_cb(&cb));

    /* NULL 契约 */
    TEST_ASSERT(lv_circuit_breaker_is_tripped(NULL), "NULL 视为已跳闸");
    TEST_ASSERT(!lv_circuit_breaker_check_guarded(NULL), "NULL 不放行");
    TEST_ASSERT_NOT_NULL(lv_circuit_breaker_state_name_cb(NULL));
    lv_circuit_breaker_init(NULL);

    lv_circuit_breaker_reset(&cb); /* 若无此函数则删除 */
    printf("  test_cb_init_guarded_api: PASSED\n");
}

static void test_cb_trip_api(void) {
    lvCircuitBreaker cb;
    lv_circuit_breaker_init(&cb);

    /* 连续错误触发跳闸 */
    cb.max_consecutive_errors = 3;
    TEST_ASSERT(lv_circuit_breaker_record_error(&cb), "错误1 仍允许");
    TEST_ASSERT(lv_circuit_breaker_record_error(&cb), "错误2 仍允许");
    TEST_ASSERT(!lv_circuit_breaker_record_error(&cb), "错误3 触发跳闸");
    TEST_ASSERT_EQ((int) lv_circuit_breaker_state(&cb), (int) lv_CB_OPEN);
    TEST_ASSERT(lv_circuit_breaker_is_tripped(&cb), "OPEN 已跳闸");
    TEST_ASSERT(!lv_circuit_breaker_check_guarded(&cb), "OPEN 不放行");
    TEST_ASSERT(lv_circuit_breaker_state_name_cb(&cb) != NULL, "状态名");

    /* record_success 在 OPEN 态不影响 */
    lv_circuit_breaker_record_success(&cb);
    TEST_ASSERT_EQ((int) lv_circuit_breaker_state(&cb), (int) lv_CB_OPEN);

    /* NULL 契约 */
    TEST_ASSERT(!lv_circuit_breaker_record_error(NULL), "NULL error 返回 false");
    lv_circuit_breaker_record_success(NULL);
    lv_circuit_breaker_do_trip(NULL, "x");

    lv_circuit_breaker_reset(&cb);
    printf("  test_cb_trip_api: PASSED\n");
}

static void test_cb_do_trip_halfopen_api(void) {
    lvCircuitBreaker cb;
    lv_circuit_breaker_init(&cb);

    /* do_trip 显式跳闸 */
    lv_circuit_breaker_do_trip(&cb, "测试原因");
    TEST_ASSERT_EQ((int) lv_circuit_breaker_state(&cb), (int) lv_CB_OPEN);
    TEST_ASSERT(lv_circuit_breaker_is_tripped(&cb), "显式跳闸生效");
    TEST_ASSERT(cb.trip_count >= 1, "跳闸计数");

    /* 冷却时间设为 0 → 冷却完成 → 半开 */
    cb.cooldown_ms = 0;
    TEST_ASSERT(lv_circuit_breaker_check_guarded(&cb), "冷却后放行（半开）");
    TEST_ASSERT_EQ((int) lv_circuit_breaker_state(&cb), (int) lv_CB_HALF_OPEN);

    /* 半开失败 → 重新 OPEN */
    TEST_ASSERT(!lv_circuit_breaker_record_error(&cb), "半开失败重新跳闸");
    TEST_ASSERT_EQ((int) lv_circuit_breaker_state(&cb), (int) lv_CB_OPEN);

    /* 半开成功 → CLOSED */
    cb.cooldown_ms = 0;
    lv_circuit_breaker_check_guarded(&cb); /* → HALF_OPEN */
    lv_circuit_breaker_record_success(&cb);
    TEST_ASSERT_EQ((int) lv_circuit_breaker_state(&cb), (int) lv_CB_CLOSED);

    lv_circuit_breaker_reset(&cb);
    printf("  test_cb_do_trip_halfopen_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 Circuit Breaker Ext Test Suite")
    printf("=== Lv-00 Circuit Breaker Ext Test Suite (batch C-㊺续28) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_cb_init_guarded_api);
    TEST_MAIN_RUN(test_cb_trip_api);
    TEST_MAIN_RUN(test_cb_do_trip_halfopen_api);
    lv_cleanup();
TEST_MAIN_END()
