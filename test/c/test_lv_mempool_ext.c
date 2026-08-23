/**
 * @file test_lv_mempool_ext.c
 * @brief 内存池公共 API 契约测试（批次 C-㊺续33：lv_mempool.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（4 个）：
 *   lv_mempool_create / destroy / alloc / free
 *
 * 契约要点（与 lv_mempool.c / debug.c mem_pool_* 核对）：
 *   - create(block_size, initial_blocks)：分配池，失败返回 NULL。
 *   - alloc：从池分配一个块，池满返回 NULL。
 *   - free：归还块。
 *   - destroy：NULL 安全。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <string.h>

#include "lv/lv_mempool.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：分配/归还往返 ============== */

static void test_alloc_free_roundtrip(void) {
    lvMemPool *pool = lv_mempool_create(64, 8);
    TEST_ASSERT_NOT_NULL(pool);

    /* 分配一个块并写入 */
    char *b = (char *) lv_mempool_alloc(pool);
    TEST_ASSERT_NOT_NULL(b);
    strcpy(b, "hello-pool");
    TEST_ASSERT_STR_EQ(b, "hello-pool");

    /* 归还后再分配：同一块（或池内任意块）可用 */
    lv_mempool_free(pool, b);
    char *b2 = (char *) lv_mempool_alloc(pool);
    TEST_ASSERT_NOT_NULL(b2);

    lv_mempool_free(pool, b2);
    lv_mempool_destroy(pool);
}

/* ============== 测试：NULL 契约 ============== */

static void test_null_contract(void) {
    lv_mempool_destroy(NULL);

    /* 空块大小创建不崩溃 */
    lvMemPool *pool = lv_mempool_create(0, 0);
    if (pool) {
        lv_mempool_destroy(pool);
    }
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("MemPoolExt")

    printf("\n--- lv_mempool (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_alloc_free_roundtrip);
    TEST_MAIN_RUN(test_null_contract);

TEST_MAIN_END()
