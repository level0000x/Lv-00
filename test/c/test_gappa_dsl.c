/**
 * @file test_gappa_dsl.c
 * @brief Gappa DSL 测试桩
 *
 * @details 占位测试：验证 Gappa DSL 模块的基本行为。
 */

#include <stdio.h>

#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

TEST_MAIN_BEGIN("test_gappa_dsl")
    TEST_ASSERT_CONTINUE(1, "placeholder");
TEST_MAIN_END()
