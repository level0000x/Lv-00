/**
 * @file test_bootstrap_test_ext.c
 * @brief 自举测试框架清理契约测试（批次 C-㊺续36：bootstrap_test.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（2 个）：
 *   lv_bootstrap_test_framework_cleanup / lv_primitive_wrapper_cleanup
 *
 * 契约要点（与 bootstrap_test_init.c / bootstrap_test_primitive.c 核对）：
 *   - framework_cleanup：未初始化时直接返回（幂等）；已初始化时清理原语
 *     包装器与核心系统。
 *   - primitive_wrapper_cleanup：重置原语注册表。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/bootstrap_test.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：清理幂等 ============== */

static void test_cleanup_idempotent(void) {
    /* 未初始化：cleanup 直接返回（幂等，不崩） */
    lv_bootstrap_test_framework_cleanup();
    lv_bootstrap_test_framework_cleanup();

    /* 原语包装器清理：重置计数（不崩） */
    lv_primitive_wrapper_cleanup();
    lv_primitive_wrapper_cleanup();
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("BootstrapTestExt")

    printf("\n--- bootstrap_test (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_cleanup_idempotent);

TEST_MAIN_END()
