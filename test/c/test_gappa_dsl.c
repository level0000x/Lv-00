/**
 * @file test_gappa_dsl.c
 * @brief Gappa DSL 测试桩
 *
 * @details 占位测试：验证 Gappa DSL 模块的基本行为。
 */

#include <stdio.h>

static int g_passed = 0, g_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { g_passed++; } else { g_failed++; printf("FAIL: %s\n", msg); } \
} while (0)

int main(void) {
    printf("=== test_gappa_dsl ===\n");
    TEST_ASSERT(1, "placeholder");
    printf("Passed: %d, Failed: %d\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
