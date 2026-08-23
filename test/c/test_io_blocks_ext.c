/**
 * @file test_io_blocks_ext.c
 * @brief IO 块工厂契约测试（批次 C-㊺续30：io_blocks.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（6 个）：
 *   lv_file_block_create / destroy
 *   lv_network_block_create / destroy
 *   lv_ui_event_block_create / destroy
 *
 * 契约要点（与 io_blocks.h / file_block.c / network_block.c / ui_block.c 核对）：
 *   - file_block_create：保存传入 effect，四个端口初始 -1，base 内部状态分配。
 *   - network_block_create：无参，effect 固定 lv_EFFECT_NETWORK，端口初始 -1。
 *   - ui_event_block_create：保存传入 effect，event/action 端口初始 -1（无 base）。
 *   - destroy 均 NULL 安全。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/io_blocks.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：文件块 ============== */

static void test_file_block_api(void) {
    lvFileBlock *b = lv_file_block_create(lv_EFFECT_FILE_READ);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(b->base);
    TEST_ASSERT_EQ((int) b->effect, (int) lv_EFFECT_FILE_READ);
    TEST_ASSERT_EQ(b->path_port, -1);
    TEST_ASSERT_EQ(b->data_port, -1);
    TEST_ASSERT_EQ(b->result_port, -1);
    TEST_ASSERT_EQ(b->status_port, -1);
    lv_file_block_destroy(b);

    /* 不同 effect 保存 */
    b = lv_file_block_create(lv_EFFECT_FILE_WRITE);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQ((int) b->effect, (int) lv_EFFECT_FILE_WRITE);
    lv_file_block_destroy(b);

    /* destroy NULL 安全 */
    lv_file_block_destroy(NULL);
}

/* ============== 测试：网络块 ============== */

static void test_network_block_api(void) {
    lvNetworkBlock *b = lv_network_block_create();
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(b->base);
    /* effect 固定为 NETWORK */
    TEST_ASSERT_EQ((int) b->effect, (int) lv_EFFECT_NETWORK);
    TEST_ASSERT_EQ(b->url_port, -1);
    TEST_ASSERT_EQ(b->request_port, -1);
    TEST_ASSERT_EQ(b->response_port, -1);
    TEST_ASSERT_EQ(b->status_port, -1);
    lv_network_block_destroy(b);

    /* destroy NULL 安全 */
    lv_network_block_destroy(NULL);
}

/* ============== 测试：UI 事件块 ============== */

static void test_ui_event_block_api(void) {
    lvUIEventBlock *b = lv_ui_event_block_create(lv_EFFECT_UI_INPUT);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQ((int) b->effect, (int) lv_EFFECT_UI_INPUT);
    TEST_ASSERT_EQ(b->event_port, -1);
    TEST_ASSERT_EQ(b->action_port, -1);
    lv_ui_event_block_destroy(b);

    /* 不同 effect 保存 */
    b = lv_ui_event_block_create(lv_EFFECT_UI_RENDER);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQ((int) b->effect, (int) lv_EFFECT_UI_RENDER);
    lv_ui_event_block_destroy(b);

    /* destroy NULL 安全 */
    lv_ui_event_block_destroy(NULL);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("IOBlocksExt")

    printf("\n--- io_blocks (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_file_block_api);
    TEST_MAIN_RUN(test_network_block_api);
    TEST_MAIN_RUN(test_ui_event_block_api);

TEST_MAIN_END()
