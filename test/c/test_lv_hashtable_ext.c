/**
 * @file test_lv_hashtable_ext.c
 * @brief 哈希表设施契约测试（批次 C-㊺续29：lv_hashtable.h 3 个零覆盖 API）
 *
 * 覆盖：int_hash / int_foreach / str_foreach
 * 契约：int_hash 在 2 的幂容量内返回 [0, capacity)；foreach 遍历全部
 *   占用条目。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/lv_hashtable.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_ht_int_hash_api(void) {
    /* hash：2 的幂容量 → [0, capacity) */
    for (int cap = 1; cap <= 1024; cap <<= 1) {
        for (int k = -100; k <= 100; k += 17) {
            unsigned h = lv_hashtable_int_hash(k, cap);
            TEST_ASSERT(h < (unsigned) cap, "hash 在容量内");
        }
    }
    /* 相同键同 hash */
    TEST_ASSERT_EQ(lv_hashtable_int_hash(42, 256), lv_hashtable_int_hash(42, 256));
    /* 非 2 幂容量防御 */
    unsigned h = lv_hashtable_int_hash(42, 10);
    TEST_ASSERT(h < 10, "非 2 幂取模");

    printf("  test_ht_int_hash_api: PASSED\n");
}

typedef struct {
    int sum;
    int count;
} SumCtx;

static void int_visitor(int key, void *value, void *ctx) {
    (void) value;
    SumCtx *s = (SumCtx *) ctx;
    s->sum += key;
    s->count++;
}

static void str_visitor(const char *key, void *value, void *ctx) {
    (void) value;
    SumCtx *s = (SumCtx *) ctx;
    s->sum += (int) strlen(key);
    s->count++;
}

static void test_ht_foreach_api(void) {
    /* int foreach */
    lvHashtable *ht = lv_hashtable_int_create(8);
    int vals[5] = {10, 20, 30, 40, 50};
    lv_hashtable_int_insert(ht, 1, &vals[0]);
    lv_hashtable_int_insert(ht, 2, &vals[1]);
    lv_hashtable_int_insert(ht, 3, &vals[2]);
    lv_hashtable_int_insert(ht, 4, &vals[3]);
    lv_hashtable_int_insert(ht, 5, &vals[4]);

    SumCtx ctx = {0, 0};
    lv_hashtable_int_foreach(ht, int_visitor, &ctx);
    TEST_ASSERT_EQ(ctx.count, 5);
    TEST_ASSERT_EQ(ctx.sum, 15); /* 1+2+3+4+5 */

    /* NULL 契约 */
    lv_hashtable_int_foreach(NULL, int_visitor, &ctx);
    lv_hashtable_int_foreach(ht, NULL, &ctx);
    lv_hashtable_int_destroy(ht);

    /* str foreach */
    lvHashtable *sht = lv_hashtable_str_create(8);
    char a[] = "aa", b[] = "bbbb";
    lv_hashtable_str_insert(sht, "k1", a);
    lv_hashtable_str_insert(sht, "k2", b);

    SumCtx sctx = {0, 0};
    lv_hashtable_str_foreach(sht, str_visitor, &sctx);
    TEST_ASSERT_EQ(sctx.count, 2);
    TEST_ASSERT_EQ(sctx.sum, 4); /* strlen("k1")+strlen("k2") = 2+2 = 4 */

    lv_hashtable_str_foreach(NULL, str_visitor, &sctx);
    lv_hashtable_str_foreach(sht, NULL, &sctx);
    lv_hashtable_str_destroy(sht);

    printf("  test_ht_foreach_api: PASSED\n");
}

TEST_MAIN_BEGIN("Lv-00 Hashtable Ext Test Suite")
    printf("=== Lv-00 Hashtable Ext Test Suite (batch C-㊺续29) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_ht_int_hash_api);
    TEST_MAIN_RUN(test_ht_foreach_api);
    lv_cleanup();
TEST_MAIN_END()
