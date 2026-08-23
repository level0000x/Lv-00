/**
 * @file test_lv_utils_array_ext.c
 * @brief 工具函数契约测试（批次 C-㊺续15：lv_utils.h 数组/字符串/分配族零覆盖）
 *
 * 覆盖零覆盖 API：
 *   lv_insertion_sort / lv_shift_left / lv_shift_right / lv_buffer_consume /
 *   lv_int_multiset_equal / lv_int_append_unique / lv_copy_int_array /
 *   lv_malloc_tracked / lv_calloc_tracked / lv_format_time / lv_fmt_tmp /
 *   lv_strncat（12 个）
 *
 * 契约要点（与实现核对）：
 *   - lv_shift_left 删除 index 前移；lv_shift_right 在 index 腾位右移。
 *   - lv_buffer_consume 消费前 pos 个元素并压缩；pos>=len 清空。
 *   - lv_int_multiset_equal：长度不等 0；空=空 1；分配失败 -1。
 *   - lv_int_append_unique：已存在 false，否则追加 true。
 *   - lv_copy_int_array：复制数组，调用者 lv_free。
 *   - lv_format_time：微秒时间戳格式化为 "YYYY-MM-DD HH:MM:SS"。
 *   - lv_fmt_tmp：格式化到 TLS 暂存缓冲，返回可读字符串。
 *   - lv_strncat：安全追加，保证 NUL 终止。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_utils.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：排序 ============== */

static int cmp_int_ctx(const void *a, const void *b, void *ctx) {
    (void) ctx;
    int ia = *(const int *) a;
    int ib = *(const int *) b;
    return (ia > ib) - (ia < ib);
}

static void test_insertion_sort_api(void) {
    /* 基本排序 */
    int arr[6] = {5, 2, 8, 1, 9, 3};
    lv_insertion_sort(arr, 6, sizeof(int), cmp_int_ctx, NULL);
    TEST_ASSERT_EQ(arr[0], 1);
    TEST_ASSERT_EQ(arr[1], 2);
    TEST_ASSERT_EQ(arr[2], 3);
    TEST_ASSERT_EQ(arr[3], 5);
    TEST_ASSERT_EQ(arr[4], 8);
    TEST_ASSERT_EQ(arr[5], 9);

    /* 已排序数组幂等 */
    int sorted[4] = {1, 2, 3, 4};
    lv_insertion_sort(sorted, 4, sizeof(int), cmp_int_ctx, NULL);
    TEST_ASSERT_EQ(sorted[0], 1);
    TEST_ASSERT_EQ(sorted[3], 4);

    /* 单元素/空数组安全 */
    int single[1] = {7};
    lv_insertion_sort(single, 1, sizeof(int), cmp_int_ctx, NULL);
    TEST_ASSERT_EQ(single[0], 7);
    lv_insertion_sort(NULL, 5, sizeof(int), cmp_int_ctx, NULL);
    lv_insertion_sort(arr, 5, 0, cmp_int_ctx, NULL);

    /* 大元素（>64B 走堆分配路径） */
    struct Big {
        int key;
        char pad[128];
    };
    struct Big big[3] = {{3, {0}}, {1, {0}}, {2, {0}}};
    lv_insertion_sort(big, 3, sizeof(struct Big), cmp_int_ctx, NULL);
    TEST_ASSERT_EQ(big[0].key, 1);
    TEST_ASSERT_EQ(big[2].key, 3);

    printf("  test_insertion_sort_api: PASSED\n");
}

/* ============== 测试：移位 ============== */

static void test_shift_api(void) {
    /* lv_shift_left：删除 index=1 前移 */
    int a[5] = {10, 20, 30, 40, 50};
    lv_shift_left(a, sizeof(int), 1, 5);
    TEST_ASSERT_EQ(a[0], 10);
    TEST_ASSERT_EQ(a[1], 30);
    TEST_ASSERT_EQ(a[2], 40);
    TEST_ASSERT_EQ(a[3], 50);

    /* lv_shift_right：在 index=2 腾位 */
    int b[5] = {1, 2, 3, 4, 0};
    lv_shift_right(b, sizeof(int), 2, 4);
    TEST_ASSERT_EQ(b[0], 1);
    TEST_ASSERT_EQ(b[1], 2);
    TEST_ASSERT_EQ(b[2], 3); /* 原 index=2 元素右移后仍占 index=2 的旧值？ */
    TEST_ASSERT_EQ(b[3], 3);
    TEST_ASSERT_EQ(b[4], 4);

    /* 边界：index 越界空操作 */
    int c[3] = {1, 2, 3};
    lv_shift_left(c, sizeof(int), 3, 3);
    TEST_ASSERT_EQ(c[0], 1);
    TEST_ASSERT_EQ(c[2], 3);
    lv_shift_right(c, sizeof(int), 3, 3);
    TEST_ASSERT_EQ(c[0], 1);
    lv_shift_left(NULL, sizeof(int), 0, 3);
    lv_shift_right(NULL, sizeof(int), 0, 3);

    /* lv_buffer_consume：消费前 2 个元素 */
    int d[5] = {1, 2, 3, 4, 5};
    size_t len = 5;
    lv_buffer_consume(d, sizeof(int), 2, &len);
    TEST_ASSERT_EQ(len, 3);
    TEST_ASSERT_EQ(d[0], 3);
    TEST_ASSERT_EQ(d[1], 4);
    TEST_ASSERT_EQ(d[2], 5);

    /* 全部消费：len 置 0 */
    len = 3;
    lv_buffer_consume(d, sizeof(int), 5, &len);
    TEST_ASSERT_EQ(len, 0);

    /* pos==0 空操作 / NULL 契约 */
    len = 2;
    lv_buffer_consume(d, sizeof(int), 0, &len);
    TEST_ASSERT_EQ(len, 2);
    lv_buffer_consume(NULL, sizeof(int), 1, &len);
    lv_buffer_consume(d, sizeof(int), 1, NULL);

    printf("  test_shift_api: PASSED\n");
}

/* ============== 测试：多集与唯一追加 ============== */

static void test_multiset_unique_api(void) {
    /* 多集相等：乱序但相同 */
    int a[4] = {3, 1, 4, 2};
    int b[4] = {2, 4, 1, 3};
    TEST_ASSERT_EQ(lv_int_multiset_equal(a, 4, b, 4), 1);

    /* 重复元素的多集 */
    int c[5] = {1, 1, 2, 2, 3};
    int d[5] = {2, 1, 3, 1, 2};
    TEST_ASSERT_EQ(lv_int_multiset_equal(c, 5, d, 5), 1);

    /* 不等：长度不同 */
    TEST_ASSERT_EQ(lv_int_multiset_equal(a, 4, b, 3), 0);
    /* 不等：内容不同 */
    int e[4] = {3, 1, 4, 9};
    TEST_ASSERT_EQ(lv_int_multiset_equal(a, 4, e, 4), 0);
    /* 空=空 */
    TEST_ASSERT_EQ(lv_int_multiset_equal(NULL, 0, NULL, 0), 1);
    /* 一个空一个非空 */
    TEST_ASSERT_EQ(lv_int_multiset_equal(a, 4, NULL, 0), 0);
    /* 非空但指针 NULL */
    TEST_ASSERT_EQ(lv_int_multiset_equal(NULL, 4, b, 4), 0);

    /* 唯一追加 */
    int arr[8] = {5, 3, 7};
    int count = 3;
    TEST_ASSERT(lv_int_append_unique(arr, &count, 9), "追加新值");
    TEST_ASSERT_EQ(count, 4);
    TEST_ASSERT_EQ(arr[3], 9);
    TEST_ASSERT(!lv_int_append_unique(arr, &count, 5), "重复值拒绝");
    TEST_ASSERT_EQ(count, 4);
    TEST_ASSERT(!lv_int_append_unique(arr, &count, 7), "重复值拒绝");
    TEST_ASSERT_EQ(count, 4);
    TEST_ASSERT(!lv_int_append_unique(NULL, &count, 1), "NULL 失败");
    TEST_ASSERT(!lv_int_append_unique(arr, NULL, 1), "NULL count 失败");

    printf("  test_multiset_unique_api: PASSED\n");
}

/* ============== 测试：拷贝数组与追踪分配 ============== */

static void test_copy_tracked_api(void) {
    /* lv_copy_int_array */
    int src[5] = {9, 8, 7, 6, 5};
    int *copy = lv_copy_int_array(src, 5);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQ(copy[0], 9);
    TEST_ASSERT_EQ(copy[4], 5);
    lv_free((void **) &copy);

    /* count==0 → NULL（count <= 0 视为非法，与实现一致） */
    copy = lv_copy_int_array(src, 0);
    TEST_ASSERT_NULL(copy);

    /* 非法参数 */
    copy = lv_copy_int_array(NULL, 5);
    TEST_ASSERT_NULL(copy);
    copy = lv_copy_int_array(src, -1);
    TEST_ASSERT_NULL(copy);

    /* lv_malloc_tracked / lv_calloc_tracked */
    int *t = (int *) lv_malloc_tracked(10 * sizeof(int), "test.c", 100);
    TEST_ASSERT_NOT_NULL(t);
    t[0] = 42;
    TEST_ASSERT_EQ(t[0], 42);
    lv_free((void **) &t);

    int *c = (int *) lv_calloc_tracked(8, sizeof(int), "test.c", 200);
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQ(c[0], 0);
    TEST_ASSERT_EQ(c[7], 0);
    lv_free((void **) &c);

    /* 零大小请求语义（非 NULL） */
    void *z = lv_malloc_tracked(0, "test.c", 300);
    TEST_ASSERT_NOT_NULL(z);
    lv_free(&z);
    void *zc = lv_calloc_tracked(0, 4, "test.c", 301);
    TEST_ASSERT_NOT_NULL(zc);
    lv_free(&zc);

    printf("  test_copy_tracked_api: PASSED\n");
}

/* ============== 测试：时间/格式化/字符串 ============== */

static void test_time_fmt_str_api(void) {
    /* lv_format_time：0 微秒 = 1970-01-01 00:00:00 */
    char buf[64];
    const char *s = lv_format_time(0, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(s, buf);
    TEST_ASSERT(strncmp(buf, "1970-01-01", 10) == 0, "epoch 起始");

    /* 非法参数 */
    TEST_ASSERT_NULL(lv_format_time(0, NULL, 10));
    TEST_ASSERT_NULL(lv_format_time(0, buf, 0));

    /* lv_fmt_tmp：格式化到 TLS 暂存 */
    char *t = lv_fmt_tmp("val=%d str=%s", 42, "hi");
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT(strcmp(t, "val=42 str=hi") == 0, "格式化内容");
    TEST_ASSERT_NULL(lv_fmt_tmp(NULL));

    /* lv_strncat */
    char dst[16] = "ab";
    char *r = lv_strncat(dst, "cd", sizeof(dst));
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(r, dst);
    TEST_ASSERT(strcmp(dst, "abcd") == 0, "追加结果");

    /* 容量不足：安全截断 + NUL 终止 */
    char small[5] = "ab";
    r = lv_strncat(small, "cdefgh", sizeof(small));
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT(strlen(small) < sizeof(small), "不越界");
    TEST_ASSERT_EQ(small[4], '\0');

    /* 满缓冲区（无 NUL） */
    char full[3] = {'a', 'b', 'c'};
    r = lv_strncat(full, "x", sizeof(full));
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQ(full[2], '\0');

    /* 非法参数 */
    TEST_ASSERT_NULL(lv_strncat(NULL, "x", 4));
    TEST_ASSERT_NULL(lv_strncat(dst, NULL, 4));
    TEST_ASSERT_NULL(lv_strncat(dst, "x", 0));

    printf("  test_time_fmt_str_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Utils Array Ext Test Suite")
    printf("=== Lv-00 Utils Array Ext Test Suite (batch C-㊺续15) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_insertion_sort_api);
    TEST_MAIN_RUN(test_shift_api);
    TEST_MAIN_RUN(test_multiset_unique_api);
    TEST_MAIN_RUN(test_copy_tracked_api);
    TEST_MAIN_RUN(test_time_fmt_str_api);

    lv_cleanup();
TEST_MAIN_END()
