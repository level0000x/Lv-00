/**
 * @file test_lru_cache.c
 * @brief 蓝图 LRU 缓存契约测试（PERFORMANCE_OPTIMIZATION.md §2.3）
 *
 * LRU cache contract tests.
 * 覆盖：插入/命中、容量淘汰（LRU 顺序）、更新值并刷新、未命中 NULL、
 * NULL 参数安全、thread_safe 开关两路径、count/capacity 契约、边界键。
 * 值所有权归调用方：测试全程不释放缓存持有的值，仅销毁缓存。
 */

#include <stdint.h>

#include "test_unified.h"
#include "lv/lv_lru_cache.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* 制造非 NULL 的测试值指针（所有权归测试方；只比较指针身份，不解引用） */
static void *mk_val(int k) {
    return (void *) (intptr_t) (1000 + k);
}

/* ============== 测试用例 ============== */

/* 1. 插入 / 命中 / 计数 */
static void test_insert_and_hit(void) {
    lvLRUCache *cache = lv_lru_create(8, false);
    TEST_ASSERT_NOT_NULL(cache);
    TEST_ASSERT_EQ(lv_lru_count(cache), 0);

    TEST_ASSERT(lv_lru_put(cache, 1, mk_val(1)), "插入键 1");
    TEST_ASSERT(lv_lru_put(cache, 2, mk_val(2)), "插入键 2");
    TEST_ASSERT(lv_lru_put(cache, 3, mk_val(3)), "插入键 3");
    TEST_ASSERT_EQ(lv_lru_count(cache), 3);

    TEST_ASSERT_EQ(lv_lru_get(cache, 1), mk_val(1));
    TEST_ASSERT_EQ(lv_lru_get(cache, 2), mk_val(2));
    TEST_ASSERT_EQ(lv_lru_get(cache, 3), mk_val(3));
    /* 命中只刷新访问时间，不改变条目数 */
    TEST_ASSERT_EQ(lv_lru_count(cache), 3);

    lv_lru_destroy(cache);
}

/* 2. 容量淘汰：严格 LRU 顺序 */
static void test_eviction_lru_order(void) {
    lvLRUCache *cache = lv_lru_create(3, false);
    TEST_ASSERT_NOT_NULL(cache);
    TEST_ASSERT(lv_lru_put(cache, 1, mk_val(1)), "put 1");
    TEST_ASSERT(lv_lru_put(cache, 2, mk_val(2)), "put 2");
    TEST_ASSERT(lv_lru_put(cache, 3, mk_val(3)), "put 3");
    TEST_ASSERT_EQ(lv_lru_count(cache), 3);

    /* 访问 1 → MRU 序变为 1,3,2 → LRU（淘汰对象）为 2 */
    TEST_ASSERT_EQ(lv_lru_get(cache, 1), mk_val(1));
    TEST_ASSERT(lv_lru_put(cache, 4, mk_val(4)), "put 4 触发淘汰");
    TEST_ASSERT_EQ(lv_lru_count(cache), 3);
    TEST_ASSERT(lv_lru_get(cache, 2) == NULL, "键 2 应被淘汰");
    TEST_ASSERT_EQ(lv_lru_get(cache, 1), mk_val(1));
    TEST_ASSERT_EQ(lv_lru_get(cache, 3), mk_val(3));
    TEST_ASSERT_EQ(lv_lru_get(cache, 4), mk_val(4));

    lv_lru_destroy(cache);
}

/* 3. 更新值：键已存在 → 替换 value 并提升为最新使用 */
static void test_update_value_refresh(void) {
    lvLRUCache *cache = lv_lru_create(3, false);
    TEST_ASSERT_NOT_NULL(cache);
    TEST_ASSERT(lv_lru_put(cache, 1, mk_val(1)), "put 1");
    TEST_ASSERT(lv_lru_put(cache, 2, mk_val(2)), "put 2");
    TEST_ASSERT(lv_lru_put(cache, 3, mk_val(3)), "put 3");
    /* MRU: 3,2,1；LRU=1 */

    TEST_ASSERT(lv_lru_put(cache, 1, mk_val(101)), "更新键 1 的值");
    TEST_ASSERT_EQ(lv_lru_count(cache), 3); /* 更新不改变条目数 */
    TEST_ASSERT_EQ(lv_lru_get(cache, 1), mk_val(101)); /* 新值生效 */
    /* 更新使 1 变为 MRU → MRU: 1,3,2；LRU=2 */
    TEST_ASSERT(lv_lru_put(cache, 4, mk_val(4)), "put 4");
    TEST_ASSERT(lv_lru_get(cache, 2) == NULL, "更新后 2 成为 LRU 被淘汰");
    TEST_ASSERT_EQ(lv_lru_get(cache, 1), mk_val(101));

    /* 更新 MRU 头部节点本身不破坏链表 */
    TEST_ASSERT(lv_lru_put(cache, 1, mk_val(201)), "再次更新键 1");
    TEST_ASSERT_EQ(lv_lru_count(cache), 3);
    TEST_ASSERT_EQ(lv_lru_get(cache, 1), mk_val(201));
    lv_lru_destroy(cache);
}

/* 4. 未命中返回 NULL，且不影响计数 */
static void test_miss_returns_null(void) {
    lvLRUCache *cache = lv_lru_create(4, false);
    TEST_ASSERT_NOT_NULL(cache);
    TEST_ASSERT(lv_lru_get(cache, 42) == NULL, "空缓存未命中");
    TEST_ASSERT_EQ(lv_lru_count(cache), 0);

    TEST_ASSERT(lv_lru_put(cache, 7, mk_val(7)), "put 7");
    TEST_ASSERT(lv_lru_get(cache, 99) == NULL, "未插入键未命中");
    TEST_ASSERT(lv_lru_get(cache, 7) != NULL, "已插入键命中");
    TEST_ASSERT_EQ(lv_lru_count(cache), 1); /* 未命中不改变计数 */
    lv_lru_destroy(cache);
}

/* 5. NULL 参数安全 */
static void test_null_parameter_safety(void) {
    TEST_ASSERT(!lv_lru_put(NULL, 1, mk_val(1)), "NULL 缓存 put 拒绝");
    TEST_ASSERT(lv_lru_get(NULL, 1) == NULL, "NULL 缓存 get 返回 NULL");
    TEST_ASSERT_EQ(lv_lru_count(NULL), 0);
    TEST_ASSERT_EQ(lv_lru_capacity(NULL), 0);
    lv_lru_destroy(NULL); /* no-op 不崩溃 */

    lvLRUCache *cache = lv_lru_create(4, false);
    TEST_ASSERT_NOT_NULL(cache);
    TEST_ASSERT(!lv_lru_put(cache, 7, NULL), "NULL 值拒绝（与未命中语义冲突）");
    TEST_ASSERT_EQ(lv_lru_count(cache), 0);
    TEST_ASSERT(lv_lru_get(cache, 7) == NULL, "被拒 NULL 值未入库");
    /* 拒绝后缓存仍可正常使用 */
    TEST_ASSERT(lv_lru_put(cache, 8, mk_val(8)), "拒绝后仍可插入");
    TEST_ASSERT_EQ(lv_lru_get(cache, 8), mk_val(8));
    lv_lru_destroy(cache);
}

/* 6. count/capacity 契约：默认容量、容量查询、count 恒 ≤ capacity */
static void test_count_capacity_contract(void) {
    lvLRUCache *c0 = lv_lru_create(0, false);
    TEST_ASSERT_NOT_NULL(c0);
    TEST_ASSERT_EQ(lv_lru_capacity(c0), 64); /* <=0 → 默认 64 */
    TEST_ASSERT_EQ(lv_lru_count(c0), 0);
    lv_lru_destroy(c0);

    lvLRUCache *cn = lv_lru_create(-3, false);
    TEST_ASSERT_NOT_NULL(cn);
    TEST_ASSERT_EQ(lv_lru_capacity(cn), 64);
    lv_lru_destroy(cn);

    /* 容量 16：连续 put 100 个键，count 恒为 min(i,16)，最后 16 个存活 */
    lvLRUCache *c16 = lv_lru_create(16, false);
    TEST_ASSERT_NOT_NULL(c16);
    TEST_ASSERT_EQ(lv_lru_capacity(c16), 16);
    for (int i = 1; i <= 100; i++) {
        TEST_ASSERT(lv_lru_put(c16, i, mk_val(i)), "put i");
        TEST_ASSERT_EQ(lv_lru_count(c16), i < 16 ? i : 16);
        TEST_ASSERT(lv_lru_count(c16) <= lv_lru_capacity(c16), "count ≤ capacity");
    }
    TEST_ASSERT(lv_lru_get(c16, 84) == NULL, "键 84 应被淘汰");
    TEST_ASSERT_EQ(lv_lru_get(c16, 85), mk_val(85));
    TEST_ASSERT_EQ(lv_lru_get(c16, 100), mk_val(100));
    lv_lru_destroy(c16);

    /* 容量 1 边界 */
    lvLRUCache *c1 = lv_lru_create(1, false);
    TEST_ASSERT_NOT_NULL(c1);
    TEST_ASSERT(lv_lru_put(c1, 1, mk_val(1)), "cap1 put 1");
    TEST_ASSERT(lv_lru_put(c1, 2, mk_val(2)), "cap1 put 2 触发淘汰");
    TEST_ASSERT_EQ(lv_lru_count(c1), 1);
    TEST_ASSERT(lv_lru_get(c1, 1) == NULL, "cap1 键 1 被淘汰");
    TEST_ASSERT_EQ(lv_lru_get(c1, 2), mk_val(2));
    lv_lru_destroy(c1);
}

/* 7. thread_safe 开关两路径行为一致 */
static void test_thread_safe_flag_paths(void) {
    const bool flags[2] = {false, true};
    for (int f = 0; f < 2; f++) {
        lvLRUCache *cache = lv_lru_create(2, flags[f]);
        TEST_ASSERT_NOT_NULL(cache);
        TEST_ASSERT_EQ(lv_lru_capacity(cache), 2);

        TEST_ASSERT(lv_lru_put(cache, 1, mk_val(1)), "put 1");
        TEST_ASSERT(lv_lru_put(cache, 2, mk_val(2)), "put 2");
        TEST_ASSERT_EQ(lv_lru_get(cache, 1), mk_val(1)); /* 刷新 → MRU: 1,2 */
        TEST_ASSERT(lv_lru_put(cache, 3, mk_val(3)), "put 3 触发淘汰");
        TEST_ASSERT(lv_lru_get(cache, 2) == NULL, "键 2 被淘汰");
        TEST_ASSERT_EQ(lv_lru_get(cache, 1), mk_val(1));
        TEST_ASSERT_EQ(lv_lru_get(cache, 3), mk_val(3));
        TEST_ASSERT_EQ(lv_lru_count(cache), 2);
        lv_lru_destroy(cache);
    }
}

/* 8. 边界键（0 / 负数）属性测试 */
static void test_edge_keys(void) {
    lvLRUCache *cache = lv_lru_create(8, false);
    TEST_ASSERT_NOT_NULL(cache);
    TEST_ASSERT(lv_lru_put(cache, 0, mk_val(0)), "put 键 0");
    TEST_ASSERT(lv_lru_put(cache, -5, mk_val(-5)), "put 负数键");
    TEST_ASSERT(lv_lru_put(cache, 12345, mk_val(12345)), "put 大键");
    TEST_ASSERT_EQ(lv_lru_count(cache), 3);
    TEST_ASSERT_EQ(lv_lru_get(cache, 0), mk_val(0));
    TEST_ASSERT_EQ(lv_lru_get(cache, -5), mk_val(-5));
    TEST_ASSERT_EQ(lv_lru_get(cache, 12345), mk_val(12345));
    TEST_ASSERT(lv_lru_get(cache, -6) == NULL, "未插入的负数键未命中");
    lv_lru_destroy(cache);
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 LRU Cache Test Suite")
    printf("=== Lv-00 LRU Cache Test Suite (PERFORMANCE_OPTIMIZATION.md §2.3) ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_insert_and_hit);
    TEST_MAIN_RUN(test_eviction_lru_order);
    TEST_MAIN_RUN(test_update_value_refresh);
    TEST_MAIN_RUN(test_miss_returns_null);
    TEST_MAIN_RUN(test_null_parameter_safety);
    TEST_MAIN_RUN(test_count_capacity_contract);
    TEST_MAIN_RUN(test_thread_safe_flag_paths);
    TEST_MAIN_RUN(test_edge_keys);
    lv_cleanup();
TEST_MAIN_END()
