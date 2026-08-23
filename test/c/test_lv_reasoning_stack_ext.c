/**
 * @file test_lv_reasoning_stack_ext.c
 * @brief 推理分支栈契约测试（批次 C-㊺续28：lv_reasoning_stack.h 7 个零覆盖 API）
 *
 * 覆盖：init / clear / ensure_capacity / push / pop / count / top
 * 契约：push 深度限制返回 RESOURCE_EXHAUSTED、pop 空栈 INVALID_STATE、
 *   top 帧字段（branch_type/status/depth）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_reasoning_stack.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_stack_lifecycle_api(void) {
    lvReasoningStack stack;
    lv_reasoning_stack_init(&stack);
    TEST_ASSERT_EQ(stack.top, -1);
    TEST_ASSERT_EQ(lv_reasoning_stack_count(&stack), 0);
    TEST_ASSERT_NULL(lv_reasoning_stack_top(&stack));

    /* push */
    TEST_ASSERT_EQ(lv_reasoning_stack_push(&stack, lv_BRANCH_ACTIVE), lv_OK);
    TEST_ASSERT_EQ(lv_reasoning_stack_count(&stack), 1);
    lvReasoningFrame *f = lv_reasoning_stack_top(&stack);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQ((int) f->branch_type, (int) lv_BRANCH_ACTIVE);
    TEST_ASSERT_EQ((int) f->status, (int) lv_BRANCH_ACTIVE);
    TEST_ASSERT_EQ(f->depth, 0);

    /* 再 push 两帧 */
    TEST_ASSERT_EQ(lv_reasoning_stack_push(&stack, lv_BRANCH_CLOSED), lv_OK);
    TEST_ASSERT_EQ(lv_reasoning_stack_push(&stack, lv_BRANCH_FAILED), lv_OK);
    TEST_ASSERT_EQ(lv_reasoning_stack_count(&stack), 3);
    TEST_ASSERT_EQ(lv_reasoning_stack_top(&stack)->depth, 2);

    /* pop */
    TEST_ASSERT_EQ(lv_reasoning_stack_pop(&stack), lv_OK);
    TEST_ASSERT_EQ(lv_reasoning_stack_count(&stack), 2);
    TEST_ASSERT_EQ(lv_reasoning_stack_top(&stack)->depth, 1);

    /* clear */
    lv_reasoning_stack_clear(&stack);
    TEST_ASSERT_EQ(lv_reasoning_stack_count(&stack), 0);
    TEST_ASSERT_NULL(lv_reasoning_stack_top(&stack));

    /* NULL 契约 */
    lv_reasoning_stack_init(NULL);
    lv_reasoning_stack_clear(NULL);
    TEST_ASSERT_EQ(lv_reasoning_stack_push(NULL, lv_BRANCH_ACTIVE), lv_ERROR_NULL_POINTER);
    TEST_ASSERT_EQ(lv_reasoning_stack_pop(NULL), lv_ERROR_NULL_POINTER);
    TEST_ASSERT_EQ(lv_reasoning_stack_count(NULL), 0);
    TEST_ASSERT_NULL(lv_reasoning_stack_top(NULL));
    TEST_ASSERT_EQ(lv_reasoning_stack_ensure_capacity(NULL), lv_ERROR_NULL_POINTER);

    /* 空栈 pop → INVALID_STATE */
    TEST_ASSERT_EQ(lv_reasoning_stack_pop(&stack), lv_ERROR_INVALID_STATE);

    printf("  test_stack_lifecycle_api: PASSED\n");
}

static void test_stack_depth_limit_api(void) {
    lvReasoningStack stack;
    lv_reasoning_stack_init(&stack);
    /* 设置小深度限制 */
    stack.max_depth = 4;

    /* push 4 帧成功 */
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQ(lv_reasoning_stack_push(&stack, lv_BRANCH_ACTIVE), lv_OK);
    }
    TEST_ASSERT_EQ(lv_reasoning_stack_count(&stack), 4);
    /* 第 5 帧超限 */
    TEST_ASSERT_EQ(lv_reasoning_stack_push(&stack, lv_BRANCH_ACTIVE), lv_ERROR_RESOURCE_EXHAUSTED);
    TEST_ASSERT_EQ(lv_reasoning_stack_count(&stack), 4);

    lv_reasoning_stack_clear(&stack);
    printf("  test_stack_depth_limit_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 Reasoning Stack Ext Test Suite")
    printf("=== Lv-00 Reasoning Stack Ext Test Suite (batch C-㊺续28) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_stack_lifecycle_api);
    TEST_MAIN_RUN(test_stack_depth_limit_api);
    lv_cleanup();
TEST_MAIN_END()
