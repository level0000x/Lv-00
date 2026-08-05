/**
 * @file test_type_equiv_explorer.c
 * @brief 交互式类型等价探索器测试（简化版）
 *
 * 测试内容（简化版——跳过需完整 ConstraintGraph 构造的场景）：
 *   - 创建/销毁 NULL 防护
 *   - search NULL 防护
 *   - get_path NULL 防护
 *   - get_stats NULL 防护
 *
 * 完整 TypeRegion 构造依赖于 ConstraintGraph 上下文，
 * 该测试将在集成测试层编写。
 */

#include <stdio.h>
#include <stdlib.h>

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：全部 NULL 输入防护 ============== */

static void test_null_inputs(void) {
    /* NULL TypeSystem */
    TEST_ASSERT_NULL(type_equiv_explore_create(NULL, NULL, NULL));

    /* NULL explorer → search */
    TEST_ASSERT_EQ(type_equiv_explore_search(NULL, 10), false);

    /* NULL explorer → get_path */
    TEST_ASSERT_NULL(type_equiv_explore_get_path(NULL));

    /* NULL explorer → stats */
    int n = -1, d = -1;
    bool f = true, e = true;
    type_equiv_explore_get_stats(NULL, &n, &d, &f, &e);
    TEST_ASSERT_EQ(n, 0);
    TEST_ASSERT_EQ(d, 0);
    TEST_ASSERT_EQ(f, false);
    TEST_ASSERT_EQ(e, false);

    /* NULL explorer → destroy（安全） */
    type_equiv_explore_destroy(NULL);
}

/* ============== 测试：部分 NULL 输出参数 ============== */

static void test_partial_null_outputs(void) {
    type_equiv_explore_get_stats(NULL, NULL, NULL, NULL, NULL);
    /* 不崩溃即为通过 */
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Type Equivalence Explorer")
    lv_init();

    TEST_MAIN_RUN(test_null_inputs);
    TEST_MAIN_RUN(test_partial_null_outputs);

    lv_cleanup();
TEST_MAIN_END()
