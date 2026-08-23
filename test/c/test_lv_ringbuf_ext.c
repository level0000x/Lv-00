/**
 * @file test_lv_ringbuf_ext.c
 * @brief 泛型环形缓冲区契约测试（批次 C-㊺续28：lv_ringbuf.h 7 个零覆盖 API）
 *
 * 覆盖：init / destroy / write / read / get / clear / resize
 * 契约：未填满连续排列、填满后 head 循环、resize 保留最新。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_ringbuf.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_ringbuf_basic_api(void) {
    lvRingBuf rb;
    /* init */
    TEST_ASSERT(lv_ringbuf_init(&rb, sizeof(int), 4), "init 成功");
    TEST_ASSERT(!lv_ringbuf_init(NULL, sizeof(int), 4), "NULL rb 失败");
    TEST_ASSERT(!lv_ringbuf_init(&rb, 0, 4), "elem_size 0 失败");
    TEST_ASSERT(!lv_ringbuf_init(&rb, sizeof(int), 0), "capacity 0 失败");

    /* write/read：未填满 */
    int v = 10;
    lv_ringbuf_write(&rb, &v);
    v = 20;
    lv_ringbuf_write(&rb, &v);
    v = 30;
    lv_ringbuf_write(&rb, &v);

    int out;
    TEST_ASSERT(lv_ringbuf_read(&rb, 0, &out), "read idx0");
    TEST_ASSERT_EQ(out, 10);
    TEST_ASSERT(lv_ringbuf_read(&rb, 2, &out), "read idx2");
    TEST_ASSERT_EQ(out, 30);
    TEST_ASSERT(!lv_ringbuf_read(&rb, 3, &out), "越界 read 失败");
    TEST_ASSERT(!lv_ringbuf_read(&rb, -1, &out), "负索引失败");

    /* get */
    int *p = (int *) lv_ringbuf_get(&rb, 1);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQ(*p, 20);
    TEST_ASSERT_NULL(lv_ringbuf_get(&rb, 5));

    /* write NULL 契约 */
    lv_ringbuf_write(&rb, NULL);
    lv_ringbuf_write(NULL, &v);

    lv_ringbuf_destroy(&rb);
    lv_ringbuf_destroy(NULL);
    printf("  test_ringbuf_basic_api: PASSED\n");
}

static void test_ringbuf_overflow_api(void) {
    lvRingBuf rb;
    lv_ringbuf_init(&rb, sizeof(int), 3);

    /* 填满 3 个 */
    for (int i = 1; i <= 3; i++)
        lv_ringbuf_write(&rb, &i);
    /* 覆盖：写 4,5 → 最旧 1,2 被覆盖，保留 3,4,5 */
    int v = 4;
    lv_ringbuf_write(&rb, &v);
    v = 5;
    lv_ringbuf_write(&rb, &v);

    int out;
    TEST_ASSERT(lv_ringbuf_read(&rb, 0, &out), "溢出后 idx0");
    TEST_ASSERT_EQ(out, 3);
    TEST_ASSERT(lv_ringbuf_read(&rb, 2, &out), "溢出后 idx2");
    TEST_ASSERT_EQ(out, 5);

    lv_ringbuf_destroy(&rb);
    printf("  test_ringbuf_overflow_api: PASSED\n");
}

static void test_ringbuf_clear_resize_api(void) {
    lvRingBuf rb;
    lv_ringbuf_init(&rb, sizeof(int), 4);

    int v = 7;
    lv_ringbuf_write(&rb, &v);
    v = 8;
    lv_ringbuf_write(&rb, &v);

    /* clear */
    lv_ringbuf_clear(&rb);
    int out;
    TEST_ASSERT(!lv_ringbuf_read(&rb, 0, &out), "clear 后空");
    TEST_ASSERT_NULL(lv_ringbuf_get(&rb, 0));

    /* resize 扩大：保留现有 */
    lv_ringbuf_write(&rb, &v);
    v = 9;
    lv_ringbuf_write(&rb, &v);
    TEST_ASSERT(lv_ringbuf_resize(&rb, 8), "扩大成功");
    TEST_ASSERT(lv_ringbuf_read(&rb, 1, &out), "扩大后 idx1");
    TEST_ASSERT_EQ(out, 9);

    /* resize 缩小：保留最新 */
    for (int i = 1; i <= 5; i++) {
        v = i;
        lv_ringbuf_write(&rb, &v);
    }
    TEST_ASSERT(lv_ringbuf_resize(&rb, 3), "缩小成功");
    TEST_ASSERT(lv_ringbuf_read(&rb, 0, &out), "缩小后 idx0");
    TEST_ASSERT(out >= 3, "缩小保留最新");
    TEST_ASSERT(lv_ringbuf_read(&rb, 2, &out), "缩小后 idx2");
    TEST_ASSERT(out >= 5, "最新在末尾");

    /* resize 非法 */
    TEST_ASSERT(!lv_ringbuf_resize(&rb, 0), "capacity 0 失败");
    TEST_ASSERT(!lv_ringbuf_resize(NULL, 4), "NULL 失败");

    lv_ringbuf_destroy(&rb);
    printf("  test_ringbuf_clear_resize_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 RingBuf Ext Test Suite")
    printf("=== Lv-00 RingBuf Ext Test Suite (batch C-㊺续28) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_ringbuf_basic_api);
    TEST_MAIN_RUN(test_ringbuf_overflow_api);
    TEST_MAIN_RUN(test_ringbuf_clear_resize_api);
    lv_cleanup();
TEST_MAIN_END()
