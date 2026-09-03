/**
 * @file test_mpc_smoke.c
 * @brief MPC 链路冒烟测试（dependency-policy / 批次 246）
 *
 * 目的：MPC（任意精度复数，REQUIRED 依赖）编译/静态链接/基本运算验证。
 * 链接顺序：mpc → mpfr → gmp（静态库依赖方向）。
 */

#include <stdio.h>
#include <stdlib.h>

#include "test_unified.h"
#include <mpc.h>

int g_pass_count = 0;
int g_fail_count = 0;

static void test_mpc_basic(void) {
    mpc_t a, b, s;
    mpc_init2(a, 128);
    mpc_init2(b, 128);
    mpc_init2(s, 128);

    /* a = 1+2i, b = 1+0i（数值构造，避免文本解析格式差异） */
    mpc_set_si_si(a, 1, 2, MPC_RNDNN);
    mpc_set_si_si(b, 1, 0, MPC_RNDNN);
    mpc_add(s, a, b, MPC_RNDNN); /* (2 + 2i) */

    mpfr_t re, im;
    mpfr_inits2(128, re, im, (mpfr_ptr) 0);
    mpc_real(re, s, MPFR_RNDN);
    mpc_imag(im, s, MPFR_RNDN);
    TEST_ASSERT_EQ(mpfr_cmp_ui(re, 2), 0);
    TEST_ASSERT_EQ(mpfr_cmp_ui(im, 2), 0);

    mpfr_clears(re, im, (mpfr_ptr) 0);
    mpc_clear(a);
    mpc_clear(b);
    mpc_clear(s);
    printf("  test_mpc_basic: PASSED\n");
}

static void test_mpc_precision(void) {
    mpc_t z;
    mpc_init2(z, 256);
    TEST_ASSERT(mpc_get_prec(z) >= 256, "256bit 精度");
    mpc_set_si_si(z, -3, 4, MPC_RNDNN);
    /* |z| = 5：cabs */
    mpfr_t m;
    mpfr_init2(m, 128);
    mpc_abs(m, z, MPFR_RNDN);
    TEST_ASSERT_EQ(mpfr_cmp_ui(m, 5), 0);
    mpfr_clear(m);
    mpc_clear(z);
    printf("  test_mpc_precision: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 MPC Smoke Test Suite")
    printf("=== Lv-00 MPC Smoke Test Suite (complex dependency foundation) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_mpc_basic);
    TEST_MAIN_RUN(test_mpc_precision);

    lv_cleanup();
TEST_MAIN_END()
