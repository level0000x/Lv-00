/**
 * @file test_lv_number_real_ext.c
 * @brief lvNumber REAL_MPFR 表示契约测试（P5 / 批次 251）
 *
 * 覆盖：kind=lv_NUMBER_REAL_MPFR 工厂（double/string）、查询（is_zero/one/negative/
 * positive/integer）、to_double/to_string、hash 同值、clone 独立、NULL 契约、非法串。
 * 注：仅非 WASM（mpfr 可用）构建有效；lv_NO_MPFR 下工厂返回 NULL。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_number.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_real_factory_api(void) {
    lvNumber *r = lv_number_real_from_double(3.141592653589793, 128);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ((int) lv_number_type(r), (int) lv_NUMBER_REAL_MPFR);
    TEST_ASSERT_DOUBLE(lv_number_to_double(r), 3.141592653589793, 1e-12);
    char *s = lv_number_to_string(r);
    TEST_ASSERT_NOT_NULL(s);
    lv_free((void **) &s);
    lv_number_destroy(r);

    /* string 构造（科学计数法也应可） */
    lvNumber *snum = lv_number_real_from_string("1.5e3", 128);
    TEST_ASSERT_NOT_NULL(snum);
    TEST_ASSERT_DOUBLE(lv_number_to_double(snum), 1500.0, 1e-9);
    lv_number_destroy(snum);

    /* 非法串 → NULL */
    TEST_ASSERT_NULL(lv_number_real_from_string("abc", 128));
    TEST_ASSERT_NULL(lv_number_real_from_string(NULL, 128));
    printf("  test_real_factory_api: PASSED\n");
}

static void test_real_query_api(void) {
    lvNumber *zero = lv_number_real_from_double(0.0, 64);
    lvNumber *one = lv_number_real_from_double(1.0, 64);
    lvNumber *neg = lv_number_real_from_double(-2.5, 64);
    lvNumber *whole = lv_number_real_from_double(2.0, 64);
    TEST_ASSERT_NOT_NULL(zero);
    TEST_ASSERT_NOT_NULL(one);
    TEST_ASSERT_NOT_NULL(neg);
    TEST_ASSERT_NOT_NULL(whole);

    TEST_ASSERT(lv_number_is_zero(zero), "real 0");
    TEST_ASSERT(!lv_number_is_zero(one), "1 非零");
    TEST_ASSERT(lv_number_is_one(one), "real 1");
    TEST_ASSERT(lv_number_is_negative(neg), "-2.5 负");
    TEST_ASSERT(!lv_number_is_negative(one), "1 非负");
    TEST_ASSERT(lv_number_is_positive(one), "1 正");
    TEST_ASSERT(!lv_number_is_positive(zero), "0 非正");
    TEST_ASSERT(lv_number_is_integer(whole), "2.0 整值");
    TEST_ASSERT(!lv_number_is_integer(neg), "-2.5 非整");

    lv_number_destroy(zero);
    lv_number_destroy(one);
    lv_number_destroy(neg);
    lv_number_destroy(whole);
    printf("  test_real_query_api: PASSED\n");
}

static void test_real_clone_hash(void) {
    lvNumber *r = lv_number_real_from_string("0.1", 128);
    TEST_ASSERT_NOT_NULL(r);
    lvNumber *c = lv_number_clone(r);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT(c != (lvNumber *) r, "克隆独立句柄");
    TEST_ASSERT_EQ(lv_number_hash(r), lv_number_hash(c));
    TEST_ASSERT_DOUBLE(lv_number_to_double(c), lv_number_to_double(r), 0.0);
    lv_number_destroy(c);
    lv_number_destroy(r);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_number_clone(NULL));
    TEST_ASSERT_EQ(lv_number_to_double(NULL), 0.0);
    lv_number_destroy(NULL);
    printf("  test_real_clone_hash: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Number REAL_MPFR Ext Test Suite")
    printf("=== Lv-00 Number REAL_MPFR Ext Test Suite (P5 MPFR representation) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_real_factory_api);
    TEST_MAIN_RUN(test_real_query_api);
    TEST_MAIN_RUN(test_real_clone_hash);

    lv_cleanup();
TEST_MAIN_END()
