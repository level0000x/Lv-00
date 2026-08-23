/**
 * @file test_rational_ext.c
 * @brief GMP 有理数契约测试（批次 C-㊺续37：rational.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（11 个）：
 *   lv_rational_create_from_i64 / create_from_mpz / den_is_safe / equal
 *   / estimate_loss / from_mpq / from_string / mul_is_safe / set / set_mpz / to_mpq
 *
 * 契约要点（与 lv_rational.c 核对）：
 *   - create_from_i64/mpz：den 0 → NULL；成功构造并规范化。
 *   - set/set_mpz：复制赋值；den 0 / r NULL → false。
 *   - equal：结构相等；NULL 契约。
 *   - from_string："a/b"、"整数"；非法 → NULL。
 *   - from_mpq/to_mpq：与 GMP 互操作。
 *   - den_is_safe：比特数 ≤ 65536 → true。
 *   - mul_is_safe：比特和超限 → false；NULL → false。
 *   - estimate_loss：总比特 ≤ 53 → 0；NULL/零 → 0。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/lv_utils.h"
#include "lv/rational.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：构造 ============== */

static void test_construct(void) {
    /* i64：3/2 */
    lvRational *r = lv_rational_create_from_i64(3, 2);
    TEST_ASSERT_NOT_NULL(r);
    char *s = lv_rational_to_string(r);
    TEST_ASSERT_STR_EQ(s, "3/2");
    lv_free((void **) &s);
    TEST_ASSERT_NULL(lv_rational_create_from_i64(1, 0));
    lv_rational_destroy(&r);

    /* mpz：1/3 */
    mpz_t num, den;
    mpz_init_set_ui(num, 1);
    mpz_init_set_ui(den, 3);
    r = lv_rational_create_from_mpz(num, den);
    TEST_ASSERT_NOT_NULL(r);
    s = lv_rational_to_string(r);
    TEST_ASSERT_STR_EQ(s, "1/3");
    lv_free((void **) &s);

    /* 分母 0 → NULL */
    mpz_t zero;
    mpz_init_set_ui(zero, 0);
    TEST_ASSERT_NULL(lv_rational_create_from_mpz(num, zero));
    mpz_clear(zero);

    lv_rational_destroy(&r);
    mpz_clear(num);
    mpz_clear(den);
}

/* ============== 测试：赋值 ============== */

static void test_set(void) {
    lvRational *a = lv_rational_create_from_i64(1, 2);
    lvRational *b = lv_rational_create_from_i64(3, 4);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    /* set：b 复制 a */
    lv_rational_set(b, a);
    TEST_ASSERT(lv_rational_equal(a, b), "set copies value");

    /* set_mpz：2/5；den 0 → false；NULL → false */
    mpz_t num, den;
    mpz_init_set_ui(num, 2);
    mpz_init_set_ui(den, 5);
    TEST_ASSERT(lv_rational_set_mpz(b, num, den), "set_mpz ok");
    char *s = lv_rational_to_string(b);
    TEST_ASSERT_STR_EQ(s, "2/5");
    lv_free((void **) &s);
    mpz_set_ui(den, 0);
    TEST_ASSERT(!lv_rational_set_mpz(b, num, den), "den 0 false");
    TEST_ASSERT(!lv_rational_set_mpz(NULL, num, den), "NULL false");
    mpz_clear(num);
    mpz_clear(den);

    lv_rational_destroy(&a);
    lv_rational_destroy(&b);
}

/* ============== 测试：比较 ============== */

static void test_equal(void) {
    lvRational *a = lv_rational_create_from_i64(1, 2);
    lvRational *b = lv_rational_create_from_i64(1, 2);
    lvRational *c = lv_rational_create_from_i64(1, 3);

    TEST_ASSERT(lv_rational_equal(a, b), "1/2 == 1/2");
    TEST_ASSERT(!lv_rational_equal(a, c), "1/2 != 1/3");

    lv_rational_destroy(&a);
    lv_rational_destroy(&b);
    lv_rational_destroy(&c);
}

/* ============== 测试：字符串 ============== */

static void test_from_string(void) {
    lvRational *r = lv_rational_from_string("3/4");
    TEST_ASSERT_NOT_NULL(r);
    char *s = lv_rational_to_string(r);
    TEST_ASSERT_STR_EQ(s, "3/4");
    lv_free((void **) &s);
    lv_rational_destroy(&r);

    /* 整数形式 */
    r = lv_rational_from_string("7");
    TEST_ASSERT_NOT_NULL(r);
    s = lv_rational_to_string(r);
    TEST_ASSERT_STR_EQ(s, "7");
    lv_free((void **) &s);
    lv_rational_destroy(&r);

    /* 非法/NULL */
    TEST_ASSERT_NULL(lv_rational_from_string("abc"));
    TEST_ASSERT_NULL(lv_rational_from_string(NULL));
    TEST_ASSERT_NULL(lv_rational_from_string(""));
}

/* ============== 测试：mpq 互操作 ============== */

static void test_mpq_interop(void) {
    /* from_mpq */
    mpq_t q;
    mpq_init(q);
    mpq_set_si(q, 3, 4);
    lvRational *r = lv_rational_from_mpq(q);
    TEST_ASSERT_NOT_NULL(r);
    char *s = lv_rational_to_string(r);
    TEST_ASSERT_STR_EQ(s, "3/4");
    lv_free((void **) &s);
    TEST_ASSERT_NULL(lv_rational_from_mpq(NULL));
    lv_rational_destroy(&r);

    /* to_mpq */
    lvRational *half = lv_rational_create_from_i64(1, 2);
    mpq_t out;
    mpq_init(out);
    lv_rational_to_mpq(half, out);
    TEST_ASSERT(mpq_cmp_ui(out, 1, 2) == 0, "to_mpq 1/2");
    mpq_clear(out);
    lv_rational_destroy(&half);
    mpq_clear(q);
}

/* ============== 测试：安全判定 ============== */

static void test_safety(void) {
    /* den_is_safe：小分母 true */
    mpz_t den;
    mpz_init_set_ui(den, 100);
    TEST_ASSERT(lv_rational_den_is_safe(den), "small den safe");
    TEST_ASSERT(!lv_rational_den_is_safe(NULL), "NULL den");

    /* 大分母（70000 比特 > 65536）：false */
    mpz_ui_pow_ui(den, 2, 70000);
    TEST_ASSERT(!lv_rational_den_is_safe(den), "huge den unsafe");
    mpz_clear(den);

    /* mul_is_safe：小乘 true；NULL false */
    lvRational *a = lv_rational_create_from_i64(1, 2);
    lvRational *b = lv_rational_create_from_i64(1, 3);
    TEST_ASSERT(lv_rational_mul_is_safe(a, b, 64), "small mul safe");
    TEST_ASSERT(!lv_rational_mul_is_safe(NULL, b, 64), "NULL a");
    TEST_ASSERT(!lv_rational_mul_is_safe(a, NULL, 64), "NULL b");

    /* estimate_loss：小值 0；NULL 0 */
    TEST_ASSERT_EQ(lv_rational_estimate_loss(a), 0);
    TEST_ASSERT_EQ(lv_rational_estimate_loss(NULL), 0);
    lvRational *zero = lv_rational_create_from_i64(0, 1);
    TEST_ASSERT_EQ(lv_rational_estimate_loss(zero), 0);
    lv_rational_destroy(&zero);

    lv_rational_destroy(&a);
    lv_rational_destroy(&b);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("RationalExt")

    printf("\n--- rational (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_construct);
    TEST_MAIN_RUN(test_set);
    TEST_MAIN_RUN(test_equal);
    TEST_MAIN_RUN(test_from_string);
    TEST_MAIN_RUN(test_mpq_interop);
    TEST_MAIN_RUN(test_safety);

TEST_MAIN_END()
