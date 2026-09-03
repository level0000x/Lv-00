/**
 * @file test_mpfr_smoke.c
 * @brief MPFR 链路冒烟测试（dependency-policy B0 / 批次 245）
 *
 * 目的：批次 A 起 MPFR/MPC 为 REQUIRED 依赖但全库零 include——本条验证
 * 编译（mpfr.h）、静态链接（libmpfr.a）与基本运算链路可用，防止依赖地基
 * 配置漂移（如 find 到错误库/头）。
 *
 * 只做库功能冒烟（不涉及 Lv-00 语义）：任意精度加法/比较/精度位查询。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include <mpfr.h>

int g_pass_count = 0;
int g_fail_count = 0;

static void test_mpfr_basic(void) {
    mpfr_set_default_prec(128);
    TEST_ASSERT(mpfr_get_default_prec() >= 128, "默认精度 128");

    mpfr_t x, y, s;
    mpfr_inits2(128, x, y, s, (mpfr_ptr) 0);

    /* 0.1 + 0.2 != 0.3（二进制浮点不精确；MPFR 高精度下可比 double 精确） */
    TEST_ASSERT_EQ(mpfr_set_str(x, "0.1", 10, MPFR_RNDN), 0);
    TEST_ASSERT_EQ(mpfr_set_str(y, "0.2", 10, MPFR_RNDN), 0);
    mpfr_add(s, x, y, MPFR_RNDN);
    TEST_ASSERT(mpfr_cmp_ui(s, 0) > 0, "0.1+0.2 > 0");

    mpfr_set_str(x, "1", 10, MPFR_RNDN);
    mpfr_set_str(y, "3", 10, MPFR_RNDN);
    mpfr_div(s, x, y, MPFR_RNDN); /* 1/3 */
    TEST_ASSERT(mpfr_cmp_si(s, 1) < 0 && mpfr_cmp_ui(s, 0) > 0, "1/3 in (0,1)");

    mpfr_clear(x);
    mpfr_clear(y);
    mpfr_clear(s);
    printf("  test_mpfr_basic: PASSED\n");
}

static void test_mpfr_string_roundtrip(void) {
    mpfr_t v;
    mpfr_init2(v, 128);

    char buf[64];
    mpfr_set_str(v, "3.141592653589793238462643383279502884", 10, MPFR_RNDN);
    int written = mpfr_snprintf(buf, sizeof(buf), "%.10Rf", v);
    TEST_ASSERT(written > 0, "mpfr_snprintf 有输出");
    /* 前 10 位小数：128bit RNDN 下为 3.1415926535/…36（前缀一致即可） */
    TEST_ASSERT(strncmp(buf, "3.141592653", 11) == 0, "pi 前缀命中");

    mpfr_clear(v);
    printf("  test_mpfr_string_roundtrip: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 MPFR Smoke Test Suite")
    printf("=== Lv-00 MPFR Smoke Test Suite (dependency foundation B0) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_mpfr_basic);
    TEST_MAIN_RUN(test_mpfr_string_roundtrip);

    lv_cleanup();
TEST_MAIN_END()
