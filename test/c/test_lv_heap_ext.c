/**
 * @file test_lv_heap_ext.c
 * @brief 泛型二叉堆契约测试（批次 C-㊺续28：lv_heap.h 6 个零覆盖 API）
 *
 * 覆盖：init / destroy / push / pop / top / size / empty
 * 契约：最小堆 pop 顺序、最大堆 pop 顺序、空堆哨兵。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_heap.h"

int g_pass_count = 0;
int g_fail_count = 0;

static int cmp_int(const void *a, const void *b) {
    int ia = *(const int *) a;
    int ib = *(const int *) b;
    return (ia > ib) - (ia < ib);
}

static void test_heap_min_api(void) {
    lvHeap heap;
    TEST_ASSERT(lv_heap_init(&heap, sizeof(int), lv_MIN_HEAP, cmp_int, 0), "最小堆 init");
    TEST_ASSERT(lv_heap_empty(&heap), "初始空");
    TEST_ASSERT_EQ(lv_heap_size(&heap), 0);
    TEST_ASSERT(!lv_heap_top(&heap, NULL) || lv_heap_top(&heap, (int[1]){0}) == false, "空堆 top 失败");
    int dummy;
    TEST_ASSERT(!lv_heap_top(&heap, &dummy), "空堆 top 失败");
    TEST_ASSERT(!lv_heap_pop(&heap, &dummy), "空堆 pop 失败");

    /* push 乱序 */
    int v;
    v = 30; lv_heap_push(&heap, &v);
    v = 10; lv_heap_push(&heap, &v);
    v = 50; lv_heap_push(&heap, &v);
    v = 20; lv_heap_push(&heap, &v);
    v = 40; lv_heap_push(&heap, &v);
    TEST_ASSERT_EQ(lv_heap_size(&heap), 5);
    TEST_ASSERT(!lv_heap_empty(&heap), "非空");

    /* top = min */
    TEST_ASSERT(lv_heap_top(&heap, &dummy), "top 成功");
    TEST_ASSERT_EQ(dummy, 10);

    /* pop 顺序：升序 */
    int expected[5] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(lv_heap_pop(&heap, &dummy), "pop 成功");
        TEST_ASSERT_EQ(dummy, expected[i]);
    }
    TEST_ASSERT(lv_heap_empty(&heap), "pop 完为空");

    /* NULL 契约 */
    TEST_ASSERT(!lv_heap_init(NULL, sizeof(int), lv_MIN_HEAP, cmp_int, 0), "NULL 失败");
    TEST_ASSERT(!lv_heap_init(&heap, 0, lv_MIN_HEAP, cmp_int, 0), "elem_size 0 失败");
    TEST_ASSERT(!lv_heap_init(&heap, sizeof(int), lv_MIN_HEAP, NULL, 0), "NULL cmp 失败");
    TEST_ASSERT(!lv_heap_push(NULL, &dummy), "NULL push 失败");
    TEST_ASSERT(!lv_heap_push(&heap, NULL), "NULL elem 失败");
    TEST_ASSERT(!lv_heap_pop(NULL, &dummy), "NULL pop 失败");
    TEST_ASSERT(!lv_heap_top(NULL, &dummy), "NULL top 失败");
    TEST_ASSERT_EQ(lv_heap_size(NULL), 0);
    TEST_ASSERT(lv_heap_empty(NULL), "NULL 视为空");
    lv_heap_destroy(NULL);

    lv_heap_destroy(&heap);
    printf("  test_heap_min_api: PASSED\n");
}

static void test_heap_max_api(void) {
    lvHeap heap;
    lv_heap_init(&heap, sizeof(int), lv_MAX_HEAP, cmp_int, 4);

    int v;
    v = 30; lv_heap_push(&heap, &v);
    v = 10; lv_heap_push(&heap, &v);
    v = 50; lv_heap_push(&heap, &v);
    v = 20; lv_heap_push(&heap, &v);

    int out;
    TEST_ASSERT(lv_heap_top(&heap, &out), "top 成功");
    TEST_ASSERT_EQ(out, 50);

    /* pop 顺序：降序 */
    int expected[4] = {50, 30, 20, 10};
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(lv_heap_pop(&heap, &out), "pop 成功");
        TEST_ASSERT_EQ(out, expected[i]);
    }

    lv_heap_destroy(&heap);
    printf("  test_heap_max_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 Heap Ext Test Suite")
    printf("=== Lv-00 Heap Ext Test Suite (batch C-㊺续28) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_heap_min_api);
    TEST_MAIN_RUN(test_heap_max_api);
    lv_cleanup();
TEST_MAIN_END()
