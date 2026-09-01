/**
 * @file test_lv_bitset_ext.c
 * @brief lvBitset 位图容器测试（K63/F89：唯一位图容器）
 */

#include <stdio.h>
#include <string.h>

#include "lv/lv_bitset.h"

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：基础 set/test/clear ============== */

static void test_bitset_basic(void) {
    lvBitset bs;
    lv_bitset_init(&bs);

    /* 空位图：未分配，test 返回 false */
    TEST_ASSERT(!lv_bitset_test(&bs, 0), "empty test false");

    /* 预留容量 */
    TEST_ASSERT(lv_bitset_reserve(&bs, 100), "reserve 100");
    TEST_ASSERT(!lv_bitset_test(&bs, 0), "unset bit false");
    TEST_ASSERT(!lv_bitset_test(&bs, 99), "unset bit 99 false");

    /* set/test */
    lv_bitset_set(&bs, 0);
    lv_bitset_set(&bs, 63);
    lv_bitset_set(&bs, 64);
    lv_bitset_set(&bs, 100);
    TEST_ASSERT(lv_bitset_test(&bs, 0), "bit 0");
    TEST_ASSERT(lv_bitset_test(&bs, 63), "bit 63");
    TEST_ASSERT(lv_bitset_test(&bs, 64), "bit 64 (cross word)");
    TEST_ASSERT(lv_bitset_test(&bs, 100), "bit 100");
    TEST_ASSERT(!lv_bitset_test(&bs, 1), "bit 1 unset");
    TEST_ASSERT(!lv_bitset_test(&bs, 65), "bit 65 unset");

    /* 越界 */
    TEST_ASSERT(!lv_bitset_test(&bs, 1000), "out of range test false");
    lv_bitset_set(&bs, 1000); /* 越界 set 静默 */

    /* clear */
    lv_bitset_clear(&bs, 63);
    TEST_ASSERT(!lv_bitset_test(&bs, 63), "bit 63 cleared");
    TEST_ASSERT(lv_bitset_test(&bs, 64), "bit 64 kept");

    /* clear_all */
    lv_bitset_clear_all(&bs);
    TEST_ASSERT(!lv_bitset_test(&bs, 0), "clear_all bit 0");
    TEST_ASSERT(!lv_bitset_test(&bs, 100), "clear_all bit 100");

    lv_bitset_destroy(&bs);
    TEST_ASSERT(!lv_bitset_test(&bs, 0), "destroyed empty");
}

/* ============== 测试：扩容保留已置位 ============== */

static void test_bitset_reserve_growth(void) {
    lvBitset bs;
    lv_bitset_init(&bs);
    TEST_ASSERT(lv_bitset_reserve(&bs, 10), "reserve 10");
    lv_bitset_set(&bs, 5);
    TEST_ASSERT(lv_bitset_reserve(&bs, 200), "grow to 200");
    TEST_ASSERT(lv_bitset_test(&bs, 5), "bit kept after grow");
    lv_bitset_set(&bs, 199);
    TEST_ASSERT(lv_bitset_test(&bs, 199), "new bit 199");
    lv_bitset_destroy(&bs);
}

/* ============== 测试：relation_model 去重语义复现 ============== */

static void test_bitset_dedup_semantics(void) {
    /* 模拟 relation_model 的 SEEN 去重：元素集合 {0, 3, 64, 129} */
    int elems[] = {0, 3, 64, 129, 3, 129};
    int max_elem = 129;
    lvBitset seen;
    lv_bitset_init(&seen);
    TEST_ASSERT(lv_bitset_reserve(&seen, (size_t) max_elem), "dedup reserve");
    int unique = 0;
    for (size_t i = 0; i < sizeof(elems) / sizeof(elems[0]); i++) {
        int id = elems[i];
        if (id >= 0 && !lv_bitset_test(&seen, (size_t) id)) {
            unique++;
            lv_bitset_set(&seen, (size_t) id);
        }
    }
    TEST_ASSERT_EQ(unique, 4); /* {0,3,64,129} 去重后 4 个唯一 */
    lv_bitset_destroy(&seen);
}

/* ============== Main ============== */

TEST_MAIN_BEGIN("LvBitsetExt")

    printf("\n--- lv_bitset (K63/F89) ---\n");
    TEST_MAIN_RUN(test_bitset_basic);
    TEST_MAIN_RUN(test_bitset_reserve_growth);
    TEST_MAIN_RUN(test_bitset_dedup_semantics);

TEST_MAIN_END()
