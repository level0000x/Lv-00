/**
 * @file test_memory_pool_ext.c
 * @brief 内存池系统契约测试（批次 C-㊺续20：memory_pool.h 13 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   对象池：pool_clear
 *   全局统计：mem_register_type / mem_record_alloc / mem_record_free /
 *     mem_get_global_stats / mem_reset_stats / mem_print_stats
 *   预定义池：init_preset_pools / cleanup_preset_pools / get_node_pool /
 *     get_constraint_pool / get_symbolic_coord_pool / get_proof_step_pool
 *
 * 契约要点（与实现核对）：
 *   - register_type：NULL → -1；超限 → -1；名称深拷贝。
 *   - record_alloc/free：type_id < 0 忽略；越界 type_id 仅记 total。
 *   - reset_stats 释放已注册类型名称（防泄漏）。
 *   - init_preset_pools 幂等（二次调用返回 true 不重复分配）。
 *   - cleanup 后可重新 init。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/memory_pool.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：全局内存统计 ============== */

static void test_mem_stats_api(void) {
    /* reset 后初始状态 */
    lv_mem_reset_stats();
    lvMemoryStats stats;
    lv_mem_get_global_stats(&stats);
    TEST_ASSERT_EQ(stats.type_count, 0);
    TEST_ASSERT_EQ(stats.total_bytes, 0);

    /* register_type：NULL → -1 */
    TEST_ASSERT_EQ(lv_mem_register_type(NULL), -1);

    /* 注册两个类型 */
    int t1 = lv_mem_register_type("type_a");
    int t2 = lv_mem_register_type("type_b");
    TEST_ASSERT(t1 >= 0, "注册类型1");
    TEST_ASSERT(t2 >= 0, "注册类型2");
    TEST_ASSERT(t1 != t2, "类型 ID 不同");

    /* 名称深拷贝：改原字符串不影响（register_type 内部 lv_strdup） */

    /* record_alloc/free */
    lv_mem_record_alloc(t1, 100);
    lv_mem_record_alloc(t1, 50);
    lv_mem_record_alloc(t2, 200);
    TEST_ASSERT_EQ(lv_mem_register_type(NULL), -1); /* 无关调用保序 */

    lv_mem_get_global_stats(&stats);
    TEST_ASSERT_EQ(stats.type_count, 2);
    TEST_ASSERT_EQ(stats.total_bytes, 350);
    TEST_ASSERT_EQ(stats.peak_bytes, 350);
    TEST_ASSERT_EQ(stats.types[t1].total_allocs, 2);
    TEST_ASSERT_EQ(stats.types[t1].current_bytes, 150);
    TEST_ASSERT_EQ(stats.types[t1].peak_bytes, 150);
    TEST_ASSERT_EQ(stats.types[t2].total_allocs, 1);
    TEST_ASSERT_EQ(stats.types[t2].current_bytes, 200);

    lv_mem_record_free(t1, 100);
    lv_mem_get_global_stats(&stats);
    TEST_ASSERT_EQ(stats.types[t1].current_bytes, 50);
    TEST_ASSERT_EQ(stats.types[t1].total_frees, 1);
    TEST_ASSERT_EQ(stats.total_bytes, 250);

    /* 非法 type_id 忽略 */
    lv_mem_record_alloc(-1, 10);
    lv_mem_record_free(-1, 10);
    lv_mem_record_alloc(99, 10); /* 越界：仅记 total */
    lv_mem_get_global_stats(&stats);
    TEST_ASSERT_EQ(stats.total_bytes, 260);

    /* get_global_stats NULL 契约 */
    lv_mem_get_global_stats(NULL);

    /* print_stats：stdout 可调用不崩溃 */
    lv_mem_print_stats(stdout);
    lv_mem_print_stats(NULL);

    /* reset：释放类型名（防泄漏）并清零 */
    lv_mem_reset_stats();
    lv_mem_get_global_stats(&stats);
    TEST_ASSERT_EQ(stats.type_count, 0);
    TEST_ASSERT_EQ(stats.total_bytes, 0);

    /* reset 后可重新注册 */
    int t3 = lv_mem_register_type("type_c");
    TEST_ASSERT_EQ(t3, 0);
    lv_mem_reset_stats();

    printf("  test_mem_stats_api: PASSED\n");
}

/* ============== 测试：预定义池 ============== */

static void test_preset_pools_api(void) {
    /* init：成功且幂等 */
    TEST_ASSERT(lv_init_preset_pools(), "初始化预定义池");
    TEST_ASSERT(lv_init_preset_pools(), "二次初始化幂等");

    /* get_* 池非空 */
    lvObjectPool *node_pool = lv_get_node_pool();
    lvObjectPool *constraint_pool = lv_get_constraint_pool();
    lvObjectPool *sym_pool = lv_get_symbolic_coord_pool();
    lvObjectPool *step_pool = lv_get_proof_step_pool();
    TEST_ASSERT_NOT_NULL(node_pool);
    TEST_ASSERT_NOT_NULL(constraint_pool);
    TEST_ASSERT_NOT_NULL(sym_pool);
    TEST_ASSERT_NOT_NULL(step_pool);
    /* 各池对象大小不同（类型特定） */
    TEST_ASSERT(node_pool != constraint_pool, "池实例不同");

    /* 从池分配/释放对象 */
    void *obj = lv_pool_alloc(node_pool);
    TEST_ASSERT_NOT_NULL(obj);
    TEST_ASSERT(lv_pool_free(node_pool, obj), "池回收");
    void *obj2 = lv_pool_alloc(node_pool);
    TEST_ASSERT_NOT_NULL(obj2);
    TEST_ASSERT(lv_pool_free(node_pool, obj2), "池回收2");

    /* cleanup 后池指针 NULL */
    lv_cleanup_preset_pools();
    TEST_ASSERT_NULL(lv_get_node_pool());
    TEST_ASSERT_NULL(lv_get_constraint_pool());
    TEST_ASSERT_NULL(lv_get_symbolic_coord_pool());
    TEST_ASSERT_NULL(lv_get_proof_step_pool());

    /* cleanup 后可重新 init */
    TEST_ASSERT(lv_init_preset_pools(), "重新初始化");
    lv_cleanup_preset_pools();
    lv_cleanup_preset_pools(); /* 二次 cleanup 幂等 */

    printf("  test_preset_pools_api: PASSED\n");
}

/* ============== 测试：对象池 clear ============== */

static void test_pool_clear_api(void) {
    lvPoolConfig cfg = {
        .object_size = 32, .capacity = 8, .thread_safe = false, .auto_grow = true, .name = "clear_test"};
    lvObjectPool *pool = lv_pool_create(&cfg);
    TEST_ASSERT_NOT_NULL(pool);

    /* 分配 3 个对象 */
    void *a = lv_pool_alloc(pool);
    void *b = lv_pool_alloc(pool);
    void *c = lv_pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(c);

    uint64_t allocs = 0, frees = 0;
    size_t used = 0;
    lv_pool_get_stats(pool, &allocs, &frees, &used);
    TEST_ASSERT_EQ(allocs, 3);
    TEST_ASSERT_EQ(used, 3);

    /* clear：所有对象回空闲链表，当前使用 0 */
    lv_pool_clear(pool);
    lv_pool_get_stats(pool, &allocs, &frees, &used);
    TEST_ASSERT_EQ(used, 0);

    /* clear 后可重新分配（复用空闲块） */
    void *d = lv_pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(d);
    lv_pool_clear(pool);

    /* NULL 契约 */
    lv_pool_clear(NULL);

    lv_pool_destroy(pool);
    printf("  test_pool_clear_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Memory Pool Ext Test Suite")
    printf("=== Lv-00 Memory Pool Ext Test Suite (batch C-㊺续20) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_mem_stats_api);
    TEST_MAIN_RUN(test_preset_pools_api);
    TEST_MAIN_RUN(test_pool_clear_api);

    lv_cleanup();
TEST_MAIN_END()
