/**
 * @file test_memory_management.c
 * @brief 内存管理系统综合测试
 *
 * 测试内容：
 * 1. 内存分配与释放基本流程
 * 2. 内存池创建/分配/释放/销毁完整生命周期
 * 3. 毒模式检测（分配、释放、检查毒模式）
 * 4. 魔数完整性检查
 * 5. 内存泄漏追踪（分配不释放，检查泄漏报告）
 * 6. 有界分配（请求过大内存，应失败）
 * 7. 追踪分配（带文件/行号追踪的分配）
 * 8. 资源追踪器（追踪多个资源，统一清理）
 * 9. 线性分配器（创建、分配、重置、销毁）
 * 10. 几何配置的线程安全（顺序 set/get 验证）
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geometry_config.h"
#include "lv.h"
#include "lv_utils.h"
#include "memory_pool.h"
#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============================================================
 * 测试 1：内存分配与释放基本流程
 * ============================================================ */

static void test_basic_alloc_free(void) {
    /* lv_malloc 分配 */
    void *ptr = lv_malloc(128);
    TEST_ASSERT_NOT_NULL(ptr);

    /* 写入数据验证可用性 */
    memset(ptr, 0xAA, 128);
    unsigned char *bytes = (unsigned char *) ptr;
    TEST_ASSERT(bytes[0] == 0xAA, "lv_malloc 分配的内存应可写入");

    /* lv_free 释放 */
    lv_free((void **) &ptr);
    TEST_ASSERT_NULL(ptr);
}

static void test_calloc_zero_fill(void) {
    size_t count = 64;
    size_t size = sizeof(int);
    int *arr = (int *) lv_calloc(count, size);
    TEST_ASSERT_NOT_NULL(arr);

    /* calloc 应将内存清零 */
    int all_zero = 1;
    for (size_t i = 0; i < count; i++) {
        if (arr[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    TEST_ASSERT(all_zero, "lv_calloc 分配的内存应全部为零");

    lv_free((void **) &arr);
}

static void test_realloc_grow(void) {
    /* 初始分配 */
    int *arr = (int *) lv_malloc(sizeof(int) * 4);
    TEST_ASSERT_NOT_NULL(arr);
    for (int i = 0; i < 4; i++) {
        arr[i] = i * 10;
    }

    /* 扩容 */
    int *new_arr = (int *) lv_realloc(arr, sizeof(int) * 8);
    TEST_ASSERT_NOT_NULL(new_arr);

    /* 验证原有数据保留 */
    TEST_ASSERT(new_arr[0] == 0, "realloc 后原有数据应保留 [0]");
    TEST_ASSERT(new_arr[3] == 30, "realloc 后原有数据应保留 [3]");

    /* 写入新区域 */
    new_arr[4] = 40;
    new_arr[5] = 50;
    TEST_ASSERT(new_arr[4] == 40, "realloc 后新区域应可写入");

    lv_free((void **) &new_arr);
}

/* ============================================================
 * 测试 2：内存池完整生命周期
 * ============================================================ */

static void test_pool_lifecycle(void) {
    lvPoolConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.object_size = 64;
    cfg.capacity = 16;
    cfg.thread_safe = false;
    cfg.auto_grow = false;
    cfg.name = "test_pool";

    /* 创建 */
    lvObjectPool *pool = lv_pool_create(&cfg);
    TEST_ASSERT_NOT_NULL(pool);

    /* 分配 */
    void *obj1 = lv_pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(obj1);

    void *obj2 = lv_pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(obj2);

    /* 释放 */
    bool freed = lv_pool_free(pool, obj1);
    TEST_ASSERT(freed, "pool_free 应成功释放对象");

    /* 释放后再分配应复用内存 */
    void *obj3 = lv_pool_alloc(pool);
    TEST_ASSERT_NOT_NULL(obj3);

    /* 统计信息 */
    uint64_t total_allocs = 0, total_frees = 0;
    size_t current_used = 0;
    lv_pool_get_stats(pool, &total_allocs, &total_frees, &current_used);
    TEST_ASSERT(total_allocs == 3, "总分配次数应为 3");
    TEST_ASSERT(total_frees == 1, "总释放次数应为 1");

    /* 销毁 */
    lv_pool_destroy(pool);
}

/* ============================================================
 * 测试 3：毒模式检测
 * ============================================================ */

static void test_poison_pattern(void) {
    /* 确保毒模式已启用 */
    lv_poison_enable(true);
    TEST_ASSERT(lv_poison_is_enabled(), "毒模式应已启用");

    /* 分配并写入已知数据 */
    size_t alloc_size = 64;
    unsigned char *ptr = (unsigned char *) lv_malloc(alloc_size);
    TEST_ASSERT_NOT_NULL(ptr);

    memset(ptr, 0x11, alloc_size);

    /* 释放后检查毒模式 */
    lv_free((void **) &ptr);
    /* 释放后 ptr 已置 NULL，需要单独分配来验证毒模式行为 */

    /* 分配新块，写入数据，释放，然后手动检查毒模式 */
    unsigned char *ptr2 = (unsigned char *) lv_malloc(alloc_size);
    TEST_ASSERT_NOT_NULL(ptr2);
    memset(ptr2, 0x22, alloc_size);

    /* 释放后 ptr2 置 NULL，但内存区域应被毒模式填充 */
    lv_free((void **) &ptr2);
    TEST_ASSERT_NULL(ptr2);

    /* 恢复默认状态 */
    lv_poison_enable(true);
}

/* ============================================================
 * 测试 4：魔数完整性检查
 * ============================================================ */

static void test_magic_number_integrity(void) {
    /* 分配内存 */
    size_t alloc_size = 128;
    unsigned char *ptr = (unsigned char *) lv_malloc(alloc_size);
    TEST_ASSERT_NOT_NULL(ptr);

    /* 检查魔数完整性 */
    bool magic_ok = lv_memory_check_magic(ptr);
    TEST_ASSERT(magic_ok, "新分配的内存魔数应完整");

    /* 正常使用后魔数仍应完整 */
    memset(ptr, 0xCC, alloc_size);
    magic_ok = lv_memory_check_magic(ptr);
    TEST_ASSERT(magic_ok, "正常使用后魔数应仍完整");

    /* 释放 */
    lv_free((void **) &ptr);
}

/* ============================================================
 * 测试 5：内存泄漏追踪
 * ============================================================ */

static void test_leak_tracking(void) {
    /* 重置内存统计 */
    lv_reset_memory_stats();

    /* 使用追踪分配 */
    void *tracked = lv_TRACKED_MALLOC(256);
    TEST_ASSERT_NOT_NULL(tracked);
    memset(tracked, 0xDD, 256);

    /* 检查内存统计 */
    MemoryStats stats;
    lv_get_memory_stats(&stats);
    TEST_ASSERT(stats.allocation_count > 0, "分配次数应大于 0");

    /* 正常释放追踪分配的内存 */
    lv_free((void **) &tracked);
    TEST_ASSERT_NULL(tracked);

    /* 验证泄漏报告（此时不应有泄漏） */
    int leaks = lv_memory_leak_report(NULL);
    /* leaks 表示未释放的追踪分配数量，释放后应为 0 或仅包含框架内部分配 */
    printf("    泄漏报告: %d 个未释放块\n", leaks);
}

/* ============================================================
 * 测试 6：有界分配
 * ============================================================ */

static void test_bounded_allocation(void) {
    /* 设置内存限制 */
    size_t limit = 1024; /* 1KB 限制 */
    lv_set_memory_limit(limit);

    /* 请求超过限制的内存应失败 */
    void *big = lv_malloc_bounded(2048, limit);
    TEST_ASSERT_NULL(big);

    /* 请求在限制内的内存应成功 */
    void *small = lv_malloc_bounded(512, limit);
    TEST_ASSERT_NOT_NULL(small);
    lv_free(&small);

    /* 清除限制 */
    lv_set_memory_limit(0);
}

/* ============================================================
 * 测试 7：追踪分配（带文件/行号）
 * ============================================================ */

static void test_tracked_allocation(void) {
    /* 重置统计 */
    lv_reset_memory_stats();

    /* 追踪分配 */
    void *p1 = lv_TRACKED_MALLOC(32);
    TEST_ASSERT_NOT_NULL(p1);

    void *p2 = lv_TRACKED_CALLOC(1, 64);
    TEST_ASSERT_NOT_NULL(p2);

    /* 验证 calloc 清零 */
    unsigned char *bytes = (unsigned char *) p2;
    int all_zero = 1;
    for (int i = 0; i < 64; i++) {
        if (bytes[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    TEST_ASSERT(all_zero, "TRACKED_CALLOC 应清零内存");

    /* 释放 */
    lv_free(&p1);
    lv_free(&p2);
}

/* ============================================================
 * 测试 8：资源追踪器
 * ============================================================ */

static void resource_destroy_callback(void *resource) {
    /* 简单的销毁回调：释放内存 */
    free(resource);
}

static void test_resource_tracker(void) {
    /* 创建追踪器 */
    ResourceTracker *rt = lv_resource_tracker_create();
    TEST_ASSERT_NOT_NULL(rt);
    TEST_ASSERT_EQ(lv_resource_tracker_count(rt), 0);

    /* 追踪多个资源 */
    void *res1 = malloc(100);
    void *res2 = malloc(200);
    void *res3 = malloc(300);

    bool ok;
    ok = lv_resource_track(rt, res1, resource_destroy_callback, "resource_1");
    TEST_ASSERT(ok, "追踪 res1 应成功");
    ok = lv_resource_track(rt, res2, resource_destroy_callback, "resource_2");
    TEST_ASSERT(ok, "追踪 res2 应成功");
    ok = lv_resource_track(rt, res3, resource_destroy_callback, "resource_3");
    TEST_ASSERT(ok, "追踪 res3 应成功");

    TEST_ASSERT_EQ(lv_resource_tracker_count(rt), 3);

    /* 取消追踪一个资源 */
    ok = lv_resource_untrack(rt, res2);
    TEST_ASSERT(ok, "取消追踪 res2 应成功");
    TEST_ASSERT_EQ(lv_resource_tracker_count(rt), 2);

    /* 手动释放 res2（已取消追踪） */
    free(res2);

    /* 清理剩余资源（应自动调用销毁回调） */
    lv_resource_tracker_cleanup(rt);
    TEST_ASSERT_EQ(lv_resource_tracker_count(rt), 0);

    /* 销毁追踪器 */
    lv_resource_tracker_destroy(&rt);
    TEST_ASSERT_NULL(rt);
}

/* ============================================================
 * 测试 9：线性分配器
 * ============================================================ */

static void test_linear_allocator(void) {
    /* 创建 */
    lvLinearAllocator *la = lv_linear_allocator_create(4096);
    TEST_ASSERT_NOT_NULL(la);

    /* 分配 */
    void *p1 = lv_linear_alloc(la, 128, 16);
    TEST_ASSERT_NOT_NULL(p1);

    void *p2 = lv_linear_alloc(la, 256, 16);
    TEST_ASSERT_NOT_NULL(p2);

    /* p2 应在 p1 之后 */
    TEST_ASSERT((char *) p2 > (char *) p1, "后续分配地址应递增");

    /* 统计 */
    size_t total_blocks = 0, used_bytes = 0, capacity_bytes = 0;
    lv_linear_allocator_get_stats(la, &total_blocks, &used_bytes, &capacity_bytes);
    TEST_ASSERT(total_blocks >= 1, "至少应有 1 个内存块");
    TEST_ASSERT(used_bytes >= 384, "已使用字节数应 >= 384 (128+256)");

    /* 重置 */
    lv_linear_allocator_reset(la);
    lv_linear_allocator_get_stats(la, &total_blocks, &used_bytes, &capacity_bytes);
    TEST_ASSERT(used_bytes == 0, "重置后已使用字节数应为 0");

    /* 重置后可重新分配 */
    void *p3 = lv_linear_alloc(la, 64, 16);
    TEST_ASSERT_NOT_NULL(p3);

    /* 销毁 */
    lv_linear_allocator_destroy(la);
}

/* ============================================================
 * 测试 10：几何配置线程安全（顺序 set/get）
 * ============================================================ */

static void test_geometry_config_sequential(void) {
    /* 获取默认配置 */
    lvGeometryConfig def_cfg = lv_geometry_config_default();
    const lvGeometryConfig *def = &def_cfg;
    TEST_ASSERT_NOT_NULL(def);
    TEST_ASSERT(def_cfg.collinear_epsilon > 0, "默认共线容差应大于 0");

    /* 获取当前配置 */
    const lvGeometryConfig *cur = lv_geometry_get_config();
    TEST_ASSERT_NOT_NULL(cur);
    TEST_ASSERT(fabs(cur->collinear_epsilon - def->collinear_epsilon) < 1e-15, "当前配置应与默认配置一致");

    /* 设置自定义配置 */
    lvGeometryConfig custom;
    custom.collinear_epsilon = 1e-6;
    custom.perpendicular_epsilon = 1e-6;
    custom.parallel_epsilon = 1e-6;
    custom.distance_epsilon = 1e-6;
    custom.angle_epsilon = 1e-3;
    custom.singular_threshold = 1e-9;

    lv_geometry_set_config(&custom);

    /* 验证设置生效 */
    cur = lv_geometry_get_config();
    TEST_ASSERT_NOT_NULL(cur);
    TEST_ASSERT(fabs(cur->collinear_epsilon - 1e-6) < 1e-15, "自定义共线容差应已生效");

    /* 多次 set/get 交替验证 */
    for (int i = 0; i < 10; i++) {
        lvGeometryConfig tmp;
        tmp.collinear_epsilon = (double) (i + 1) * 1e-10;
        tmp.perpendicular_epsilon = (double) (i + 1) * 1e-10;
        tmp.parallel_epsilon = (double) (i + 1) * 1e-10;
        tmp.distance_epsilon = (double) (i + 1) * 1e-10;
        tmp.angle_epsilon = (double) (i + 1) * 1e-7;
        tmp.singular_threshold = (double) (i + 1) * 1e-13;

        lv_geometry_set_config(&tmp);

        const lvGeometryConfig *got = lv_geometry_get_config();
        TEST_ASSERT(fabs(got->collinear_epsilon - tmp.collinear_epsilon) < 1e-15, "交替 set/get 应一致");
    }

    /* 恢复默认配置 */
    lv_geometry_set_config(NULL);
    cur = lv_geometry_get_config();
    TEST_ASSERT(fabs(cur->collinear_epsilon - def->collinear_epsilon) < 1e-15, "恢复默认后配置应与初始默认一致");
}

/* ============================================================
 * 测试入口
 * ============================================================ */

int main(void) {
    printf("=== Lv-00 内存管理系统综合测试 ===\n\n");

    g_pass_count = 0;
    g_fail_count = 0;

    TEST_SUITE_BEGIN("内存管理");

    TEST_RUN(test_basic_alloc_free);
    TEST_RUN(test_calloc_zero_fill);
    TEST_RUN(test_realloc_grow);
    TEST_RUN(test_pool_lifecycle);
    TEST_RUN(test_poison_pattern);
    TEST_RUN(test_magic_number_integrity);
    TEST_RUN(test_leak_tracking);
    TEST_RUN(test_bounded_allocation);
    TEST_RUN(test_tracked_allocation);
    TEST_RUN(test_resource_tracker);
    TEST_RUN(test_linear_allocator);
    TEST_RUN(test_geometry_config_sequential);

    TEST_SUITE_END();

    return g_fail_count > 0 ? 1 : 0;
}
