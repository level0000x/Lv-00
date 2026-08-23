/**
 * @file test_lv_number_ops_ext.c
 * @brief 数值统一抽象层契约测试（批次 C-㊺续24：lv_number.h 11 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   工厂：from_string
 *   算术：add / sub / mul / div / neg / abs / pow
 *   转换：to_string
 *   克隆：clone
 *   类型信息：type_name
 *
 * 契约要点（与实现核对）：
 *   - from_string：有理数优先（"3/4"→RATIONAL），浮点（"1.5"→FLOAT），
 *     非法字符串 → NULL。
 *   - add/sub/mul/div 经 ops vtable 分发，同类型运算保持类型。
 *   - div 整数除零 → NULL；float 除零 → inf。
 *   - neg = 0 - n；abs 负数取反、非负克隆。
 *   - pow：^0 → 1、正幂快速幂、负幂 1/base^|exp|。
 *   - to_string 调用者 free；clone 深拷贝独立。
 *   - type_name：各类型名、越界 → "Unknown"。
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_number.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：工厂 from_string ============== */

static void test_number_from_string_api(void) {
    /* 有理数：3/4 → RATIONAL */
    lvNumber *r = lv_number_from_string("3/4");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((int) lv_number_type(r), (int) lv_NUMBER_RATIONAL);
    TEST_ASSERT_DOUBLE(lv_number_to_double(r), 0.75, 1e-12);
    lv_number_destroy(r);

    /* 整数形式 → 有理数 */
    r = lv_number_from_string("7");
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_DOUBLE(lv_number_to_double(r), 7.0, 1e-12);
    lv_number_destroy(r);

    /* 浮点：1.5 → FLOAT */
    lvNumber *f = lv_number_from_string("1.5");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ((int) lv_number_type(f), (int) lv_NUMBER_FLOAT);
    TEST_ASSERT_DOUBLE(lv_number_to_double(f), 1.5, 1e-12);
    lv_number_destroy(f);

    /* 非法字符串 → NULL */
    TEST_ASSERT_NULL(lv_number_from_string("abc"));
    TEST_ASSERT_NULL(lv_number_from_string(""));
    TEST_ASSERT_NULL(lv_number_from_string(NULL));

    printf("  test_number_from_string_api: PASSED\n");
}

/* ============== 测试：算术 ============== */

static void test_number_arith_api(void) {
    /* 整数算术 */
    lvNumber *a = lv_number_from_int(6);
    lvNumber *b = lv_number_from_int(4);

    lvNumber *s = lv_number_add(a, b);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_DOUBLE(lv_number_to_double(s), 10, 1e-12);
    TEST_ASSERT_EQ((int) lv_number_type(s), (int) lv_NUMBER_INTEGER);
    lv_number_destroy(s);

    s = lv_number_sub(a, b);
    TEST_ASSERT_DOUBLE(lv_number_to_double(s), 2, 1e-12);
    lv_number_destroy(s);

    s = lv_number_mul(a, b);
    TEST_ASSERT_DOUBLE(lv_number_to_double(s), 24, 1e-12);
    lv_number_destroy(s);

    s = lv_number_div(a, b);
    TEST_ASSERT_DOUBLE(lv_number_to_double(s), 1, 1e-12); /* 整数除法截断 */
    lv_number_destroy(s);

    /* 整数除零 → NULL */
    lvNumber *zero = lv_number_from_int(0);
    TEST_ASSERT_NULL(lv_number_div(a, zero));

    /* 浮点算术 */
    lvNumber *fa = lv_number_from_double(1.5);
    lvNumber *fb = lv_number_from_double(2.0);
    s = lv_number_add(fa, fb);
    TEST_ASSERT_DOUBLE(lv_number_to_double(s), 3.5, 1e-12);
    lv_number_destroy(s);
    s = lv_number_mul(fa, fb);
    TEST_ASSERT_DOUBLE(lv_number_to_double(s), 3.0, 1e-12);
    lv_number_destroy(s);

    /* neg：6 → -6；-4 → 4 */
    lvNumber *n = lv_number_neg(a);
    TEST_ASSERT_DOUBLE(lv_number_to_double(n), -6, 1e-12);
    lv_number_destroy(n);
    lvNumber *neg4 = lv_number_neg(b);
    n = lv_number_neg(neg4);
    TEST_ASSERT_DOUBLE(lv_number_to_double(n), 4, 1e-12);
    lv_number_destroy(n);
    lv_number_destroy(neg4);

    /* abs：6 → 6；-6 → 6 */
    n = lv_number_abs(a);
    TEST_ASSERT_DOUBLE(lv_number_to_double(n), 6, 1e-12);
    lv_number_destroy(n);
    lvNumber *neg6 = lv_number_neg(a);
    n = lv_number_abs(neg6);
    TEST_ASSERT_DOUBLE(lv_number_to_double(n), 6, 1e-12);
    lv_number_destroy(n);
    lv_number_destroy(neg6);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_number_add(NULL, b));
    TEST_ASSERT_NULL(lv_number_add(a, NULL));
    TEST_ASSERT_NULL(lv_number_sub(NULL, b));
    TEST_ASSERT_NULL(lv_number_mul(NULL, b));
    TEST_ASSERT_NULL(lv_number_div(NULL, b));
    TEST_ASSERT_NULL(lv_number_neg(NULL));
    TEST_ASSERT_NULL(lv_number_abs(NULL));
    TEST_ASSERT_NULL(lv_number_pow(NULL, 2));

    lv_number_destroy(a);
    lv_number_destroy(b);
    lv_number_destroy(fa);
    lv_number_destroy(fb);
    lv_number_destroy(zero);
    printf("  test_number_arith_api: PASSED\n");
}

/* ============== 测试：pow ============== */

static void test_number_pow_api(void) {
    lvNumber *base = lv_number_from_int(2);

    /* ^0 → 1 */
    lvNumber *p = lv_number_pow(base, 0);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_DOUBLE(lv_number_to_double(p), 1, 1e-12);
    lv_number_destroy(p);

    /* ^3 → 8 */
    p = lv_number_pow(base, 3);
    TEST_ASSERT_DOUBLE(lv_number_to_double(p), 8, 1e-12);
    lv_number_destroy(p);

    /* ^-2 → 1/4 */
    p = lv_number_pow(base, -2);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_DOUBLE(lv_number_to_double(p), 0.25, 1e-12);
    lv_number_destroy(p);

    /* 浮点基：1.5^2 = 2.25 */
    lvNumber *fbase = lv_number_from_double(1.5);
    p = lv_number_pow(fbase, 2);
    TEST_ASSERT_DOUBLE(lv_number_to_double(p), 2.25, 1e-12);
    lv_number_destroy(p);

    lv_number_destroy(base);
    lv_number_destroy(fbase);
    printf("  test_number_pow_api: PASSED\n");
}

/* ============== 测试：to_string / clone / type_name ============== */

static void test_number_convert_api(void) {
    /* to_string：整数 "42" */
    lvNumber *i = lv_number_from_int(42);
    char *s = lv_number_to_string(i);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strcmp(s, "42") == 0, "整数 to_string");
    lv_free((void **) &s);

    /* 有理数 "3/4"（lv_rational_to_string 格式） */
    lvNumber *r = lv_number_from_string("3/4");
    s = lv_number_to_string(r);
    TEST_ASSERT_NOT_NULL(s);
    lv_free((void **) &s);

    /* 浮点 "1.5"（%.17g 格式） */
    lvNumber *f = lv_number_from_double(1.5);
    s = lv_number_to_string(f);
    TEST_ASSERT_NOT_NULL(s);
    lv_free((void **) &s);

    /* to_string NULL → NULL */
    TEST_ASSERT_NULL(lv_number_to_string(NULL));

    /* clone：独立深拷贝 */
    lvNumber *c = lv_number_clone(i);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT(c != i, "克隆独立句柄");
    TEST_ASSERT_DOUBLE(lv_number_to_double(c), 42, 1e-12);
    lv_number_destroy(c);
    TEST_ASSERT_NULL(lv_number_clone(NULL));

    /* type_name */
    TEST_ASSERT(strcmp(lv_number_type_name(lv_NUMBER_RATIONAL), "Rational") == 0, "Rational");
    TEST_ASSERT(strcmp(lv_number_type_name(lv_NUMBER_FLOAT), "Float") == 0, "Float");
    TEST_ASSERT(strcmp(lv_number_type_name(lv_NUMBER_INTEGER), "Integer") == 0, "Integer");
    TEST_ASSERT(strcmp(lv_number_type_name(lv_NUMBER_ALGEBRAIC), "Algebraic") == 0, "Algebraic");
    TEST_ASSERT(strcmp(lv_number_type_name(lv_NUMBER_INTERVAL), "Interval") == 0, "Interval");
    TEST_ASSERT(strcmp(lv_number_type_name((lvNumberType) 99), "Unknown") == 0, "越界 Unknown");

    lv_number_destroy(i);
    lv_number_destroy(r);
    lv_number_destroy(f);
    printf("  test_number_convert_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Number Ops Ext Test Suite")
    printf("=== Lv-00 Number Ops Ext Test Suite (batch C-㊺续24) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_number_from_string_api);
    TEST_MAIN_RUN(test_number_arith_api);
    TEST_MAIN_RUN(test_number_pow_api);
    TEST_MAIN_RUN(test_number_convert_api);

    lv_cleanup();
TEST_MAIN_END()
