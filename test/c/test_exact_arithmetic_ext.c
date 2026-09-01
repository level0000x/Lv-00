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
#include "lv/lv_arith_safe.h" /* lv_squarefree_i64（K2/F33 回归） */

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

/* K2/F33：lv_squarefree_i64 单一权威平方因子移除（收敛三处实现） */
static void test_squarefree(void) {
    TEST_ASSERT_EQ((long long) lv_squarefree_i64(72), 2LL);   /* 72 = 6²×2 */
    TEST_ASSERT_EQ((long long) lv_squarefree_i64(-12), -3LL); /* 符号保留：-(2²)×3 */
    TEST_ASSERT_EQ((long long) lv_squarefree_i64(0), 0LL);
    TEST_ASSERT_EQ((long long) lv_squarefree_i64(1), 1LL);
    TEST_ASSERT_EQ((long long) lv_squarefree_i64(-1), -1LL);
    TEST_ASSERT_EQ((long long) lv_squarefree_i64(8), 2LL);    /* 8 = 2²×2 */
    TEST_ASSERT_EQ((long long) lv_squarefree_i64(18), 2LL);   /* 18 = 3²×2 */
    TEST_ASSERT_EQ((long long) lv_squarefree_i64(12), 3LL);   /* 12 = 2²×3 */
    /* INT64_MIN 边界（防 -(n+1)+1 溢出路径） */
    TEST_ASSERT_EQ((long long) lv_squarefree_i64(-9223372036854775807LL - 1), -2LL);
}

/* K2/F33：自定义饱和乘法回调（验证 lv_pow_sq_i64 的 mul 参数化可换溢出策略） */
static bool sat_mul(int64_t a, int64_t b, int64_t *out) {
    *out = (a > 0 && b > 0 && a > INT64_MAX / b) ? INT64_MAX : a * b;
    return true;
}

/* K2/F33：lv_pow_sq_i64 参数化快速幂设施（mul 回调注入） */
static void test_pow_sq_i64(void) {
    int64_t r = 0;

    /* 权威回调：lv_safe_mul_i64（与 lv_safe_pow 同语义） */
    TEST_ASSERT(lv_pow_sq_i64(2, 10, lv_safe_mul_i64, &r), "2^10");
    TEST_ASSERT_EQ((long long) r, 1024LL);
    TEST_ASSERT(lv_pow_sq_i64(3, 0, lv_safe_mul_i64, &r), "3^0");
    TEST_ASSERT_EQ((long long) r, 1LL);
    TEST_ASSERT(lv_pow_sq_i64(-2, 3, lv_safe_mul_i64, &r), "(-2)^3");
    TEST_ASSERT_EQ((long long) r, -8LL);
    TEST_ASSERT(!lv_pow_sq_i64(2, -1, lv_safe_mul_i64, &r), "negative exponent");
    TEST_ASSERT(!lv_pow_sq_i64(2, 3, NULL, &r), "NULL mul callback");
    TEST_ASSERT(!lv_pow_sq_i64(2, 63, lv_safe_mul_i64, &r), "overflow detected");

    /* 饱和回调：2^62 不溢出（验证骨架换溢出策略） */
    TEST_ASSERT(lv_pow_sq_i64(2, 62, sat_mul, &r), "sat 2^62");
    TEST_ASSERT_EQ((long long) r, (long long) (1LL << 62));
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("ExactArithmeticExt")

    printf("\n--- exact_arithmetic (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_timestamp_now);
    TEST_MAIN_RUN(test_safe_pow);
    TEST_MAIN_RUN(test_pow_sq_i64); /* K2/F33 */
    TEST_MAIN_RUN(test_squarefree); /* K2/F33 */

TEST_MAIN_END()
