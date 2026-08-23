/**
 * @file test_lv_arena_ext.c
 * @brief 竞技场分配器契约测试（批次 C-㊺续28：lv_arena.h 8 个零覆盖 API）
 *
 * 覆盖：calloc / lock / mark / reset_to_mark / strdup / tmp /
 *   total_allocated / unlock
 * 契约：分配对齐、calloc 清零、strdup 深拷贝、mark/rollback、tmp 单例。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_arena.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_arena_alloc_api(void) {
    lvArena *a = lv_arena_create(0, false);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQ(lv_arena_total_allocated(a), 0);

    /* calloc：清零 */
    void *p = lv_arena_calloc(a, 100);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT(((char *) p)[0] == 0 && ((char *) p)[99] == 0, "calloc 清零");
    memset(p, 0xAB, 100);

    /* 对齐：8 字节 */
    void *q = lv_arena_calloc(a, 8);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT(((uintptr_t) q % 8) == 0, "8 字节对齐");

    /* strdup：深拷贝 */
    const char *src = "arena string";
    char *dup = lv_arena_strdup(a, src);
    TEST_ASSERT_NOT_NULL(dup);
    TEST_ASSERT(strcmp(dup, src) == 0, "strdup 内容");
    TEST_ASSERT(dup != src, "深拷贝");
    TEST_ASSERT_NULL(lv_arena_strdup(a, NULL));
    TEST_ASSERT_NULL(lv_arena_strdup(NULL, src));

    /* total_allocated 增加 */
    TEST_ASSERT(lv_arena_total_allocated(a) > 0, "分配后计数增加");
    TEST_ASSERT_EQ(lv_arena_total_allocated(NULL), 0);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_arena_calloc(NULL, 10));
    TEST_ASSERT_NULL(lv_arena_calloc(a, 0));
    lv_arena_lock(NULL);
    lv_arena_unlock(NULL);

    lv_arena_destroy(a);
    lv_arena_destroy(NULL);
    printf("  test_arena_alloc_api: PASSED\n");
}

static void test_arena_mark_api(void) {
    lvArena *a = lv_arena_create(0, false);

    /* 第一次分配 */
    lv_arena_calloc(a, 16);
    /* mark */
    lvArenaMark mark = lv_arena_mark(a);
    TEST_ASSERT_NOT_NULL(mark.block);

    /* mark 后分配 */
    lv_arena_calloc(a, 32);
    void *p = lv_arena_calloc(a, 64);
    TEST_ASSERT_NOT_NULL(p);

    /* reset_to_mark：释放 mark 后分配 */
    lv_arena_reset_to_mark(a, mark);
    TEST_ASSERT(lv_arena_total_used(a) <= 16 + mark.offset, "回滚后 used 缩减");

    /* 无效 mark：no-op */
    lvArenaMark bad = {NULL, 0};
    lv_arena_reset_to_mark(a, bad);
    TEST_ASSERT_NOT_NULL(lv_arena_calloc(a, 8));

    lv_arena_destroy(a);
    printf("  test_arena_mark_api: PASSED\n");
}

static void test_arena_tmp_api(void) {
    /* tmp 单例 */
    lvArena *t1 = lv_arena_tmp();
    TEST_ASSERT_NOT_NULL(t1);
    lvArena *t2 = lv_arena_tmp();
    TEST_ASSERT_EQ(t1, t2);

    /* tmp 可分配 */
    void *p = lv_arena_alloc(t1, 32);
    TEST_ASSERT_NOT_NULL(p);

    printf("  test_arena_tmp_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 Arena Ext Test Suite")
    printf("=== Lv-00 Arena Ext Test Suite (batch C-㊺续28) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_arena_alloc_api);
    TEST_MAIN_RUN(test_arena_mark_api);
    TEST_MAIN_RUN(test_arena_tmp_api);
    lv_cleanup();
TEST_MAIN_END()
