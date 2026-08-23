/**
 * @file test_fast_index_ext.c
 * @brief 快速空间索引契约测试（批次 C-㊺续32：fast_index.h 零覆盖 API）
 *
 * 覆盖零覆盖 API（4 个）：
 *   lv_fast_index_create / destroy / insert / query
 *
 * 契约要点（与 fast_index.c 核对）：
 *   - create：capacity <= 0 时回退默认 64 桶。
 *   - insert：idx NULL 或 w/h < 0 返回 -1；成功 0；包围盒覆盖的网格单元都记录。
 *   - query：idx/out_ids NULL 或 max_out <= 0 返回 -1；命中网格单元返回候选数
 *     （可能含碰撞候选，调用者自行精确检查）。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>

#include "lv/fast_index.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：create/destroy ============== */

static void test_create_destroy(void) {
    lvFastIndex *idx = lv_fast_index_create(128);
    TEST_ASSERT_NOT_NULL(idx);
    lv_fast_index_destroy(idx);

    /* capacity <= 0 回退默认 */
    idx = lv_fast_index_create(0);
    TEST_ASSERT_NOT_NULL(idx);
    lv_fast_index_destroy(idx);
    idx = lv_fast_index_create(-5);
    TEST_ASSERT_NOT_NULL(idx);
    lv_fast_index_destroy(idx);

    lv_fast_index_destroy(NULL);
}

/* ============== 测试：insert/query ============== */

static void test_insert_query(void) {
    lvFastIndex *idx = lv_fast_index_create(64);
    TEST_ASSERT_NOT_NULL(idx);

    /* 插入节点 1：包围盒 (0,0,1,1) */
    TEST_ASSERT_EQ(lv_fast_index_insert(idx, 1, 0.0, 0.0, 1.0, 1.0), 0);
    /* 插入节点 2：包围盒 (10,10,1,1) */
    TEST_ASSERT_EQ(lv_fast_index_insert(idx, 2, 10.0, 10.0, 1.0, 1.0), 0);

    /* 查询节点 1 所在单元 */
    int out[16];
    int n = lv_fast_index_query(idx, 0.5, 0.5, out, 16);
    TEST_ASSERT(n >= 1, "query hit node1 cell");
    int found1 = 0;
    for (int i = 0; i < n; i++) {
        if (out[i] == 1)
            found1 = 1;
    }
    TEST_ASSERT(found1, "node1 found");

    /* 查询节点 2 所在单元（应命中 node2，不命中 node1） */
    n = lv_fast_index_query(idx, 10.5, 10.5, out, 16);
    TEST_ASSERT(n >= 1, "query hit node2 cell");
    int found2 = 0;
    int bad1 = 0;
    for (int i = 0; i < n; i++) {
        if (out[i] == 2)
            found2 = 1;
        if (out[i] == 1)
            bad1 = 1;
    }
    TEST_ASSERT(found2, "node2 found");
    TEST_ASSERT(!bad1, "node1 not in node2 cell");

    /* 空区域查询：无命中 */
    n = lv_fast_index_query(idx, 100.0, 100.0, out, 16);
    TEST_ASSERT_EQ(n, 0);

    /* NULL / 参数契约 */
    TEST_ASSERT_EQ(lv_fast_index_insert(NULL, 1, 0, 0, 1, 1), -1);
    TEST_ASSERT_EQ(lv_fast_index_insert(idx, 1, 0, 0, -1, 1), -1);
    TEST_ASSERT_EQ(lv_fast_index_insert(idx, 1, 0, 0, 1, -1), -1);
    TEST_ASSERT_EQ(lv_fast_index_query(NULL, 0, 0, out, 16), -1);
    TEST_ASSERT_EQ(lv_fast_index_query(idx, 0, 0, NULL, 16), -1);
    TEST_ASSERT_EQ(lv_fast_index_query(idx, 0, 0, out, 0), -1);

    lv_fast_index_destroy(idx);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("FastIndexExt")

    printf("\n--- fast_index (zero-coverage) ---\n");
    TEST_MAIN_RUN(test_create_destroy);
    TEST_MAIN_RUN(test_insert_query);

TEST_MAIN_END()
