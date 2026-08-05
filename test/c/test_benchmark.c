/**
 * @file test_benchmark.c
 * @brief 性能基准测试桩
 *
 * @details 占位测试：验证基准测试模块的基本行为。
 */

#include <stdio.h>

#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

int main(void) {
    printf("=== test_benchmark ===\n");
    TEST_ASSERT_CONTINUE(1, "placeholder");
    printf("Passed: %d, Failed: %d\n", g_pass_count, g_fail_count);
    return g_fail_count > 0 ? 1 : 0;
}
