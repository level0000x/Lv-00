/**
 * @file test_lv_number_segment_ext.c
 * @brief lvNumber 池连续段契约测试（ND-5 / 批次 243）
 *
 * 契约要点（roadmap §4.2）：
 *   - segment_alloc(count)：0/NULL 拒绝；成功 = 连续内存、count 个零初始化节点；
 *   - segment_get：段内取句柄、越界 NULL、seg NULL；
 *   - rational_set：向段节点置 RATIONAL 值（NONE 首次 / RATIONAL 覆盖）；
 *     非 RATIONAL/NONE 状态拒绝（false 且节点不变）；
 *   - segment_destroy：整段清理（含 mpq 内部），之后可继续普通池操作；
 *   - 段节点禁止单独 destroy（契约），随段统一析构。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_number.h"
#include "lv/rational.h" /* lv_rational_create_from_si（置值源） */

int g_pass_count = 0;
int g_fail_count = 0;

static void test_segment_alloc_api(void) {
    /* 0 / 越界上限拒绝 */
    TEST_ASSERT_NULL(lv_number_segment_alloc(0));

    /* 正常分配：取句柄 + 越界 */
    lvNumberSegment *seg = lv_number_segment_alloc(16);
    TEST_ASSERT_NOT_NULL(seg);
    lvNumber *n0 = lv_number_segment_get(seg, 0);
    lvNumber *n15 = lv_number_segment_get(seg, 15);
    TEST_ASSERT_NOT_NULL(n0);
    TEST_ASSERT_NOT_NULL(n15);
    TEST_ASSERT(n0 != n15, "不同索引不同句柄");
    TEST_ASSERT_NULL(lv_number_segment_get(seg, 16));
    TEST_ASSERT_NULL(lv_number_segment_get(NULL, 0));
    lv_number_segment_destroy(seg);

    /* 大批量 + 循环 churn（内存复用无崩溃） */
    for (int round = 0; round < 200; round++) {
        lvNumberSegment *s = lv_number_segment_alloc((size_t) (round % 97) + 1);
        TEST_ASSERT_NOT_NULL(s);
        lv_number_segment_destroy(s);
    }
    printf("  test_segment_alloc_api: PASSED\n");
}

static void test_segment_rational_set(void) {
    lvNumberSegment *seg = lv_number_segment_alloc(8);
    TEST_ASSERT_NOT_NULL(seg);

    /* 首次置值：NONE → RATIONAL */
    lvRational *r32 = lv_rational_create_from_si(3, 2);
    lvRational *r5 = lv_rational_create_from_si(5, 1);
    TEST_ASSERT_NOT_NULL(r32);
    TEST_ASSERT_NOT_NULL(r5);

    lvNumber *a = lv_number_segment_get(seg, 0);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT(lv_number_rational_set(a, r32), "NONE → RATIONAL 置值");
    TEST_ASSERT_EQ((int) lv_number_type(a), (int) lv_NUMBER_RATIONAL);
    char *s = lv_number_to_string(a);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strcmp(s, "3/2") == 0, "段节点值 3/2");
    lv_free((void **) &s);

    /* 覆盖置值：RATIONAL → RATIONAL */
    TEST_ASSERT(lv_number_rational_set(a, r5), "RATIONAL 覆盖");
    s = lv_number_to_string(a);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strcmp(s, "5") == 0, "覆盖为 5");
    lv_free((void **) &s);

    /* 非 NONE/RATIONAL 状态拒绝：先用工厂造 int 节点再 set → false 且保持 int */
    lvNumber *i = lv_number_from_int(7);
    TEST_ASSERT_NOT_NULL(i);
    TEST_ASSERT(!lv_number_rational_set(i, r5), "int 节点拒绝置 RATIONAL");
    TEST_ASSERT_EQ((int) lv_number_type(i), (int) lv_NUMBER_INTEGER);
    TEST_ASSERT_DOUBLE(lv_number_to_double(i), 7, 1e-12);
    lv_number_destroy(i);

    /* NULL 契约 */
    TEST_ASSERT(!lv_number_rational_set(NULL, r5), "NULL 节点拒绝");
    TEST_ASSERT(!lv_number_rational_set(a, NULL), "NULL 源拒绝");

    lv_rational_destroy(&r32);
    lv_rational_destroy(&r5);
    lv_number_segment_destroy(seg);

    /* 段销毁后普通池操作正常 */
    lvNumber *after = lv_number_from_rational(1, 3);
    TEST_ASSERT_NOT_NULL(after);
    lvNumber *three = lv_number_from_int(3);
    lvNumber *prod = lv_number_mul(after, three);
    TEST_ASSERT_NOT_NULL(prod);
    TEST_ASSERT(lv_number_is_one(prod), "1/3 * 3 = 1");
    lv_number_destroy(prod);
    lv_number_destroy(three);
    lv_number_destroy(after);
    printf("  test_segment_rational_set: PASSED\n");
}

/* ============== 测试入口 ============== */

static void test_segment_fill_rationals(void) {
    lvRational *src[4];
    src[0] = lv_rational_create_from_si(3, 2);
    src[1] = lv_rational_create_from_si(5, 1);
    src[2] = lv_rational_create_from_si(-1, 4);
    src[3] = lv_rational_create_from_si(7, 3);
    for (int i = 0; i < 4; i++)
        TEST_ASSERT_NOT_NULL(src[i]);

    lvNumberSegment *seg = lv_number_segment_alloc(4);
    TEST_ASSERT_NOT_NULL(seg);
    TEST_ASSERT(lv_number_segment_fill_rationals(seg, (const struct lvRational *const *) src, 4),
                "批量装载 4 个");

    char *s = lv_number_to_string(lv_number_segment_get(seg, 0));
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strcmp(s, "3/2") == 0, "seg[0]=3/2");
    lv_free((void **) &s);
    s = lv_number_to_string(lv_number_segment_get(seg, 2));
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(strcmp(s, "-1/4") == 0, "seg[2]=-1/4");
    lv_free((void **) &s);

    /* 失败契约：count 超容量 / NULL 数组 / 含 NULL 元素 */
    TEST_ASSERT(!lv_number_segment_fill_rationals(seg, (const struct lvRational *const *) src, 5), "超容量拒绝");
    TEST_ASSERT(!lv_number_segment_fill_rationals(seg, NULL, 4), "NULL 数组拒绝");
    const struct lvRational *bad[4] = { src[0], NULL, src[2], src[3] };
    TEST_ASSERT(!lv_number_segment_fill_rationals(seg, bad, 4), "含 NULL 元素拒绝");

    lv_number_segment_destroy(seg);
    for (int i = 0; i < 4; i++)
        lv_rational_destroy(&src[i]);
    printf("  test_segment_fill_rationals: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 Number Segment Ext Test Suite")
    printf("=== Lv-00 Number Segment Ext Test Suite (ND-5 segment primitives) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_segment_alloc_api);
    TEST_MAIN_RUN(test_segment_rational_set);
    TEST_MAIN_RUN(test_segment_fill_rationals);

    lv_cleanup();
TEST_MAIN_END()
