/**
 * @file test_solver.c
 * @brief 求解器单元测试
 */
#include <stdio.h>

#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

int main(void) {
    printf("=== test_solver ===\n");
    /* TODO: add actual solver tests */
    TEST_ASSERT_CONTINUE(1, "placeholder");
    printf("Passed: %d, Failed: %d\n", g_pass_count, g_fail_count);
    return g_fail_count > 0 ? 1 : 0;
}
