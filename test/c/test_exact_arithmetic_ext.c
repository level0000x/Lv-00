/**
 * @file test_exact_arithmetic_ext.c
 * @brief 精确算术契约测试（批次 C-㊺续36：exact_arithmetic.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（2 个）：
 *   lv_timestamp_now / lv_safe_pow
 *
 * 契约要点（与 exact_arithmetic.c 核对）：
 *   - timestamp_now：返回 seconds/nanoseconds（ns = sec*1e9 + ns）。
 *   - safe_pow：result NULL 或 b < 0 → false；a^b 溢出检测；b==0 → 1。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/exact_arithmetic.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：时间戳 ============== */

static void test_timestamp_now(void) {
    lvTimestamp ts = lv_timestamp_now();
    TEST_ASSERT(ts.seconds > 0, "seconds positive");
    TEST_ASSERT(ts.nanoseconds >= 0 && ts.nanoseconds < 1000000000LL, "ns in range");

    /* 两次调用单调推进 */
    lvTimestamp ts2 = lv_timestamp_now();
    long long t1 = ts.seconds * 1000000000LL + ts.nanoseconds;
    long long t2 = ts2.seconds * 1000000000LL + ts2.nanoseconds;
    TEST_ASSERT(t2 >= t1, "timestamp monotonic");
}

/* ============== 测试：安全幂 ============== */

static void test_safe_pow(void) {
    int64_t r = 0;

    /* 2^10 = 1024 */
    TEST_ASSERT(lv_safe_pow(2, 10, &r), "2^10");
    TEST_ASSERT_EQ((long long) r, 1024LL);

    /* 3^0 = 1 */
    TEST_ASSERT(lv_safe_pow(3, 0, &r), "3^0");
    TEST_ASSERT_EQ((long long) r, 1LL);

    /* (-2)^3 = -8 */
    TEST_ASSERT(lv_safe_pow(-2, 3, &r), "(-2)^3");
    TEST_ASSERT_EQ((long long) r, -8LL);

    /* 负指数：false */
    TEST_ASSERT(!lv_safe_pow(2, -1, &r), "negative exponent");

    /* NULL 契约 */
    TEST_ASSERT(!lv_safe_pow(2, 3, NULL), "NULL result");

    /* 溢出：2^63 溢出 int64 */
    TEST_ASSERT(!lv_safe_pow(2, 63, &r), "overflow detected");
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("ExactArithmeticExt")

    printf("\n--- exact_arithmetic (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_timestamp_now);
    TEST_MAIN_RUN(test_safe_pow);

TEST_MAIN_END()
