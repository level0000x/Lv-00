/**
 * @file test_control_flow_blocks_ext.c
 * @brief 控制流块契约测试（批次 C-㊺续23：control_flow_blocks.h 11 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   If 块：if_block_create / destroy / set_branches
 *   While 块：while_block_create / destroy / set_body / set_invariant
 *   Match 块：match_block_create / destroy / set_case / set_default
 *
 * 契约要点（与实现核对）：
 *   - create：端口初始化为 -1；if determinism=PURE；while determinism=
 *     LOOP_REQUIRES_PROOF（设置 invariant 后 VERIFIED）。
 *   - if_block_set_branches：NULL block → -1（lv_RETURN_ERROR），成功 0。
 *   - while_block_set_body/invariant：NULL block → -1，成功 0；invariant
 *     非 NULL 置 determinism=VERIFIED。
 *   - match_block_create：case_count>0 分配 cases 数组；set_case 越界
 *     index → -1；set_default 成功 0。
 *   - destroy 均 NULL 安全。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/control_flow_blocks.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：If 块 ============== */

static void test_if_block_api(void) {
    lvIfBlock *blk = lv_if_block_create();
    TEST_ASSERT_NOT_NULL(blk);
    TEST_ASSERT_EQ(blk->condition_port, -1);
    TEST_ASSERT_EQ(blk->then_output, -1);
    TEST_ASSERT_EQ(blk->else_output, -1);
    TEST_ASSERT_EQ((int) blk->determinism, (int) lv_DETERMINISM_PURE);

    /* set_branches：NULL block → -1；成功 0 */
    TEST_ASSERT_EQ(lv_if_block_set_branches(NULL, (void *) 1, (void *) 2), -1);
    int rc = lv_if_block_set_branches(blk, (void *) 0x1111, (void *) 0x2222);
    TEST_ASSERT_EQ(rc, 0);
    TEST_ASSERT_EQ(blk->branches.then_branch, (void *) 0x1111);
    TEST_ASSERT_EQ(blk->branches.else_branch, (void *) 0x2222);

    /* 二次设置覆盖 */
    lv_if_block_set_branches(blk, NULL, (void *) 0x3333);
    TEST_ASSERT_NULL(blk->branches.then_branch);
    TEST_ASSERT_EQ(blk->branches.else_branch, (void *) 0x3333);

    lv_if_block_destroy(blk);
    lv_if_block_destroy(NULL);
    printf("  test_if_block_api: PASSED\n");
}

/* ============== 测试：While 块 ============== */

static void test_while_block_api(void) {
    lvWhileBlock *blk = lv_while_block_create();
    TEST_ASSERT_NOT_NULL(blk);
    TEST_ASSERT_EQ(blk->init_port, -1);
    TEST_ASSERT_EQ(blk->condition_port, -1);
    TEST_ASSERT_EQ(blk->output_port, -1);
    TEST_ASSERT_EQ((int) blk->determinism, (int) lv_DETERMINISM_LOOP_REQUIRES_PROOF);
    TEST_ASSERT(blk->max_iterations > 0, "最大迭代数为正");

    /* set_body：NULL block → -1；成功 0 */
    TEST_ASSERT_EQ(lv_while_block_set_body(NULL, (void *) 1), -1);
    TEST_ASSERT_EQ(lv_while_block_set_body(blk, (void *) 0x4444), 0);
    TEST_ASSERT_EQ(blk->body, (void *) 0x4444);

    /* set_invariant：NULL block → -1；成功 0；invariant 置 VERIFIED */
    TEST_ASSERT_EQ(lv_while_block_set_invariant(NULL, (void *) 1), -1);
    TEST_ASSERT_EQ(lv_while_block_set_invariant(blk, (void *) 0x5555), 0);
    TEST_ASSERT_EQ(blk->invariant, (void *) 0x5555);
    TEST_ASSERT_EQ((int) blk->determinism, (int) lv_DETERMINISM_VERIFIED);

    /* invariant NULL 时 determinism 不变 */
    TEST_ASSERT_EQ(lv_while_block_set_invariant(blk, NULL), 0);
    TEST_ASSERT_EQ((int) blk->determinism, (int) lv_DETERMINISM_VERIFIED);

    lv_while_block_destroy(blk);
    lv_while_block_destroy(NULL);
    printf("  test_while_block_api: PASSED\n");
}

/* ============== 测试：Match 块 ============== */

static void test_match_block_api(void) {
    /* create(3) */
    lvMatchBlock *blk = lv_match_block_create(3);
    TEST_ASSERT_NOT_NULL(blk);
    TEST_ASSERT_EQ(blk->case_count, 3);
    TEST_ASSERT_NOT_NULL(blk->cases);
    TEST_ASSERT_EQ(blk->input_port, -1);
    TEST_ASSERT_EQ(blk->output_port, -1);

    /* set_case：正常 index */
    TEST_ASSERT_EQ(lv_match_block_set_case(blk, 0, (void *) 0x11, (void *) 0x22), 0);
    TEST_ASSERT_EQ(blk->cases[0].pattern, (void *) 0x11);
    TEST_ASSERT_EQ(blk->cases[0].handler, (void *) 0x22);
    TEST_ASSERT_EQ(lv_match_block_set_case(blk, 2, (void *) 0x33, (void *) 0x44), 0);
    TEST_ASSERT_EQ(blk->cases[2].pattern, (void *) 0x33);

    /* set_case：越界/NULL → -1 */
    TEST_ASSERT_EQ(lv_match_block_set_case(blk, 3, (void *) 1, (void *) 2), -1);
    TEST_ASSERT_EQ(lv_match_block_set_case(blk, -1, (void *) 1, (void *) 2), -1);
    TEST_ASSERT_EQ(lv_match_block_set_case(NULL, 0, (void *) 1, (void *) 2), -1);

    /* set_default */
    TEST_ASSERT_EQ(lv_match_block_set_default(blk, (void *) 0x66), 0);
    TEST_ASSERT_EQ(blk->default_handler, (void *) 0x66);
    TEST_ASSERT_EQ(lv_match_block_set_default(NULL, (void *) 1), -1);

    lv_match_block_destroy(blk);
    lv_match_block_destroy(NULL);

    /* create(0)：cases NULL 但成功 */
    lvMatchBlock *zero = lv_match_block_create(0);
    TEST_ASSERT_NOT_NULL(zero);
    TEST_ASSERT_EQ(zero->case_count, 0);
    TEST_ASSERT_NULL(zero->cases);
    /* set_case 对 0 case → -1 */
    TEST_ASSERT_EQ(lv_match_block_set_case(zero, 0, (void *) 1, (void *) 2), -1);
    lv_match_block_destroy(zero);

    printf("  test_match_block_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Control Flow Blocks Ext Test Suite")
    printf("=== Lv-00 Control Flow Blocks Ext Test Suite (batch C-㊺续23) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_if_block_api);
    TEST_MAIN_RUN(test_while_block_api);
    TEST_MAIN_RUN(test_match_block_api);

    lv_cleanup();
TEST_MAIN_END()
