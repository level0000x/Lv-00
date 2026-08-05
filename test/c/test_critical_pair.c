/**
 * @file test_critical_pair.c
 * @brief 临界对重写单元测试
 */
#include <stdio.h>

#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

int main(void) {
    printf("=== test_critical_pair ===\n");
    /* TODO: add actual critical pair tests */
    TEST_ASSERT_CONTINUE(1, "placeholder");
    printf("Passed: %d, Failed: %d\n", g_pass_count, g_fail_count);
    return g_fail_count > 0 ? 1 : 0;
}
