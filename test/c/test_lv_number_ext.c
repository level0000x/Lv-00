/**
 * @file test_lv_number_ext.c
 * @brief 数值句柄契约测试（批次 C-㊺续4：lv_number.h 16 个零覆盖 API）
 *
 * 覆盖 16 个 ctest 零覆盖 API：
 *   - 比较族：lv_number_compare / eq / lt / gt / lte / gte
 *   - 转换族：lv_number_to_double / to_int
 *   - 查询族：lv_number_is_zero / is_one / is_negative / is_positive /
 *     is_integer / type / hash
 *   - 销毁族：lv_number_destroy
 *
 * 契约要点（与实现核对）：
 *   - 全部 NULL 安全：compare(NULL)→0、布尔查询(NULL)→false、
 *     to_double(NULL)→0.0、to_int(NULL)→0。
 *   - is_positive = 非零且非负；is_integer 对 INTEGER 类型或整值。
 *   - 比较委托 vtable compare（-1/0/1）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_number.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：工厂与类型 ============== */

static void test_factory_api(void) {
    /* from_rational：类型 + 值 */
    lvNumber *r = lv_number_from_rational(3, 2);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(lv_number_type(r), lv_NUMBER_RATIONAL);
    TEST_ASSERT_EQ(lv_number_to_double(r), 1.5);
    lv_number_destroy(r);

    /* from_int：整数类型 */
    lvNumber *i = lv_number_from_int(7);
    TEST_ASSERT_NOT_NULL(i);
    TEST_ASSERT_EQ(lv_number_type(i), lv_NUMBER_INTEGER);
    TEST_ASSERT_EQ(lv_number_to_double(i), 7.0);
    lv_number_destroy(i);

    /* from_double：浮点类型 */
    lvNumber *d = lv_number_from_double(2.5);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQ(lv_number_type(d), lv_NUMBER_FLOAT);
    TEST_ASSERT_EQ(lv_number_to_double(d), 2.5);
    lv_number_destroy(d);

    /* destroy：NULL 安全 */
    lv_number_destroy(NULL);

    printf("  test_factory_api: PASSED\n");
}

/* ============== 测试：比较 ============== */

static void test_compare_api(void) {
    lvNumber *a = lv_number_from_int(3);
    lvNumber *b = lv_number_from_int(5);
    lvNumber *c = lv_number_from_int(3);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    /* compare：-1/0/1 */
    TEST_ASSERT(lv_number_compare(a, b) < 0, "3 < 5");
    TEST_ASSERT_EQ(lv_number_compare(a, c), 0);
    TEST_ASSERT(lv_number_compare(b, a) > 0, "5 > 3");

    /* 关系谓词 */
    TEST_ASSERT(lv_number_eq(a, c), "3 == 3");
    TEST_ASSERT(!lv_number_eq(a, b), "3 != 5");
    TEST_ASSERT(lv_number_lt(a, b), "3 < 5");
    TEST_ASSERT(!lv_number_lt(b, a), "5 !< 3");
    TEST_ASSERT(lv_number_gt(b, a), "5 > 3");
    TEST_ASSERT(lv_number_lte(a, c), "3 <= 3");
    TEST_ASSERT(lv_number_lte(a, b), "3 <= 5");
    TEST_ASSERT(lv_number_gte(b, a), "5 >= 3");
    TEST_ASSERT(lv_number_gte(a, c), "3 >= 3");

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_number_compare(NULL, b), 0);
    TEST_ASSERT(!lv_number_eq(NULL, b), "NULL eq");
    TEST_ASSERT(!lv_number_lt(a, NULL), "NULL lt");
    TEST_ASSERT(!lv_number_gt(NULL, NULL), "双 NULL gt");

    lv_number_destroy(a);
    lv_number_destroy(b);
    lv_number_destroy(c);
    printf("  test_compare_api: PASSED\n");
}

/* ============== 测试：查询 ============== */

static void test_query_api(void) {
    lvNumber *zero = lv_number_from_int(0);
    lvNumber *one = lv_number_from_int(1);
    lvNumber *neg = lv_number_from_int(-4);
    lvNumber *pos = lv_number_from_int(9);
    lvNumber *frac = lv_number_from_rational(3, 2);
    TEST_ASSERT_NOT_NULL(zero);
    TEST_ASSERT_NOT_NULL(one);
    TEST_ASSERT_NOT_NULL(neg);
    TEST_ASSERT_NOT_NULL(pos);
    TEST_ASSERT_NOT_NULL(frac);

    /* is_zero / is_one */
    TEST_ASSERT(lv_number_is_zero(zero), "0 为零");
    TEST_ASSERT(!lv_number_is_zero(one), "1 非零");
    TEST_ASSERT(lv_number_is_one(one), "1 为一");
    TEST_ASSERT(!lv_number_is_one(zero), "0 非一");

    /* is_negative / is_positive */
    TEST_ASSERT(lv_number_is_negative(neg), "-4 为负");
    TEST_ASSERT(!lv_number_is_negative(pos), "9 非负");
    TEST_ASSERT(lv_number_is_positive(pos), "9 为正");
    TEST_ASSERT(!lv_number_is_positive(zero), "0 非正");
    TEST_ASSERT(!lv_number_is_positive(neg), "-4 非正");

    /* is_integer */
    TEST_ASSERT(lv_number_is_integer(pos), "9 为整数");
    TEST_ASSERT(!lv_number_is_integer(frac), "3/2 非整数");

    /* hash：相等值同 hash、稳定 */
    lvNumber *pos2 = lv_number_from_int(9);
    TEST_ASSERT_NOT_NULL(pos2);
    TEST_ASSERT_EQ(lv_number_hash(pos), lv_number_hash(pos2));
    TEST_ASSERT(lv_number_hash(pos) != 0 || lv_number_hash(zero) != 0, "hash 非平凡");

    /* NULL 契约 */
    TEST_ASSERT(!lv_number_is_zero(NULL), "NULL is_zero");
    TEST_ASSERT(!lv_number_is_one(NULL), "NULL is_one");
    TEST_ASSERT(!lv_number_is_negative(NULL), "NULL is_negative");
    TEST_ASSERT(!lv_number_is_positive(NULL), "NULL is_positive");
    TEST_ASSERT(!lv_number_is_integer(NULL), "NULL is_integer");

    lv_number_destroy(zero);
    lv_number_destroy(one);
    lv_number_destroy(neg);
    lv_number_destroy(pos);
    lv_number_destroy(pos2);
    lv_number_destroy(frac);
    printf("  test_query_api: PASSED\n");
}

/* ============== 测试：转换 ============== */

static void test_convert_api(void) {
    /* to_double / to_int */
    lvNumber *n = lv_number_from_rational(7, 2);
    TEST_ASSERT_NOT_NULL(n);
    TEST_ASSERT_EQ(lv_number_to_double(n), 3.5);
    TEST_ASSERT_EQ(lv_number_to_int(n), 3); /* 截断 */

    lvNumber *neg = lv_number_from_int(-3);
    TEST_ASSERT_NOT_NULL(neg);
    TEST_ASSERT_EQ(lv_number_to_int(neg), -3);

    /* NULL 契约 */
    TEST_ASSERT_EQ(lv_number_to_double(NULL), 0.0);
    TEST_ASSERT_EQ(lv_number_to_int(NULL), 0);

    lv_number_destroy(n);
    lv_number_destroy(neg);
    printf("  test_convert_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Number Ext Test Suite")
    printf("=== Lv-00 Number Ext Test Suite (batch C-㊺续4) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_factory_api);
    TEST_MAIN_RUN(test_compare_api);
    TEST_MAIN_RUN(test_query_api);
    TEST_MAIN_RUN(test_convert_api);

    lv_cleanup();
TEST_MAIN_END()
