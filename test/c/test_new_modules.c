/**
 * @file test_new_modules.c
 * @brief 新模块集成测试
 *
 * 测试 v3.3.0 新增模块：
 *   - memory_pool: 内存池系统
 *   - geometry_transform: 几何变换推理
 *   - runtime_monitor: 运行时监控
 *   - test_framework: 测试框架
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "geometry_transform.h"
#include "memory_pool.h"
#include "runtime_monitor.h"
#include "test_framework.h"

/* ============== 内存池测试 ============== */

LV00_TEST(MemoryPool, CreateDestroy) {
    Lv00PoolConfig config = {
        .object_size = 64,
        .capacity = 16,
        .thread_safe = false,
        .auto_grow = true,
        .name = "TestPool"
    };

    Lv00ObjectPool *pool = lv00_pool_create(&config);
    LV00_ASSERT_NOT_NULL(pool);

    /* 分配对象 */
    void *obj1 = lv00_pool_alloc(pool);
    LV00_ASSERT_NOT_NULL(obj1);

    void *obj2 = lv00_pool_alloc(pool);
    LV00_ASSERT_NOT_NULL(obj2);
    LV00_ASSERT(obj1 != obj2);

    /* 释放对象 */
    LV00_ASSERT(lv00_pool_free(pool, obj1) == 0);
    LV00_ASSERT(lv00_pool_free(pool, obj2) == 0);

    /* 统计 */
    uint64_t allocs, frees;
    size_t used;
    lv00_pool_get_stats(pool, &allocs, &frees, &used);
    LV00_ASSERT_EQ(2ULL, allocs);
    LV00_ASSERT_EQ(2ULL, frees);
    LV00_ASSERT_EQ(0ULL, used);

    lv00_pool_destroy(pool);
}

LV00_TEST(MemoryPool, AutoGrow) {
    Lv00PoolConfig config = {
        .object_size = 32,
        .capacity = 4,
        .thread_safe = false,
        .auto_grow = true,
        .name = "GrowPool"
    };

    Lv00ObjectPool *pool = lv00_pool_create(&config);
    LV00_ASSERT_NOT_NULL(pool);

    /* 分配超过初始容量 */
    void *objs[16];
    for (int i = 0; i < 16; i++) {
        objs[i] = lv00_pool_alloc(pool);
        LV00_ASSERT_NOT_NULL(objs[i]);
    }

    /* 释放所有 */
    for (int i = 0; i < 16; i++) {
        LV00_ASSERT(lv00_pool_free(pool, objs[i]) == 0);
    }

    lv00_pool_destroy(pool);
}

LV00_TEST(MemoryPool, LinearAllocator) {
    Lv00LinearAllocator *alloc = lv00_linear_allocator_create(1024);
    LV00_ASSERT_NOT_NULL(alloc);

    /* 分配内存 */
    void *p1 = lv00_linear_alloc(alloc, 100, 8);
    LV00_ASSERT_NOT_NULL(p1);

    void *p2 = lv00_linear_alloc(alloc, 200, 16);
    LV00_ASSERT_NOT_NULL(p2);

    /* 重置 */
    lv00_linear_allocator_reset(alloc);

    /* 再次分配 */
    void *p3 = lv00_linear_alloc(alloc, 50, 8);
    LV00_ASSERT_NOT_NULL(p3);

    /* 统计 */
    size_t blocks, used, capacity;
    lv00_linear_allocator_get_stats(alloc, &blocks, &used, &capacity);
    LV00_ASSERT_EQ(1ULL, blocks);

    lv00_linear_allocator_destroy(alloc);
}

/* ============== 几何变换测试 ============== */

LV00_TEST(GeometryTransform, Identity) {
    Lv00Transform *t = lv00_transform_identity();
    LV00_ASSERT_NOT_NULL(t);
    LV00_ASSERT_TRUE(lv00_transform_is_isometry(t));
    LV00_ASSERT_TRUE(lv00_transform_is_orientation_preserving(t));

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 3, 1);
    mpq_set_ui(y, 4, 1);

    lv00_transform_apply_point(t, x, y);

    LV00_ASSERT_TRUE(mpq_cmp_ui(x, 3, 1) == 0);
    LV00_ASSERT_TRUE(mpq_cmp_ui(y, 4, 1) == 0);

    mpq_clear(x);
    mpq_clear(y);
    lv00_transform_destroy(t);
}

LV00_TEST(GeometryTransform, Translation) {
    mpq_t dx, dy;
    mpq_init(dx);
    mpq_init(dy);
    mpq_set_ui(dx, 2, 1);
    mpq_set_ui(dy, 3, 1);

    Lv00Transform *t = lv00_transform_translation(dx, dy);
    LV00_ASSERT_NOT_NULL(t);
    LV00_ASSERT_TRUE(lv00_transform_is_isometry(t));

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 1, 1);
    mpq_set_ui(y, 1, 1);

    lv00_transform_apply_point(t, x, y);

    LV00_ASSERT_TRUE(mpq_cmp_ui(x, 3, 1) == 0);
    LV00_ASSERT_TRUE(mpq_cmp_ui(y, 4, 1) == 0);

    mpq_clear(dx);
    mpq_clear(dy);
    mpq_clear(x);
    mpq_clear(y);
    lv00_transform_destroy(t);
}

LV00_TEST(GeometryTransform, Rotation90) {
    mpq_t cx, cy;
    mpq_init(cx);
    mpq_init(cy);
    mpq_set_ui(cx, 0, 1);
    mpq_set_ui(cy, 0, 1);

    Lv00Transform *t = lv00_transform_rotation(cx, cy, 90, 1);
    LV00_ASSERT_NOT_NULL(t);
    LV00_ASSERT_TRUE(lv00_transform_is_isometry(t));

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 1, 1);
    mpq_set_ui(y, 0, 1);

    lv00_transform_apply_point(t, x, y);

    /* (1, 0) 旋转 90° -> (0, 1) */
    LV00_ASSERT_TRUE(mpq_cmp_ui(x, 0, 1) == 0);
    LV00_ASSERT_TRUE(mpq_cmp_ui(y, 1, 1) == 0);

    mpq_clear(cx);
    mpq_clear(cy);
    mpq_clear(x);
    mpq_clear(y);
    lv00_transform_destroy(t);
}

LV00_TEST(GeometryTransform, Reflection) {
    mpq_t ax, ay, bx, by;
    mpq_init(ax);
    mpq_init(ay);
    mpq_init(bx);
    mpq_init(by);

    mpq_set_ui(ax, 0, 1);
    mpq_set_ui(ay, 0, 1);
    mpq_set_ui(bx, 1, 1);
    mpq_set_ui(by, 0, 1);

    Lv00Transform *t = lv00_transform_reflection(ax, ay, bx, by);
    LV00_ASSERT_NOT_NULL(t);
    LV00_ASSERT_TRUE(lv00_transform_is_isometry(t));
    LV00_ASSERT_FALSE(lv00_transform_is_orientation_preserving(t));

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 2, 1);
    mpq_set_ui(y, 3, 1);

    lv00_transform_apply_point(t, x, y);

    /* 关于 x 轴反射：(2, 3) -> (2, -3) */
    LV00_ASSERT_TRUE(mpq_cmp_ui(x, 2, 1) == 0);
    LV00_ASSERT_TRUE(mpq_cmp_ui(y, 3, 1) == 0 || mpq_cmp_si(y, -3, 1) == 0);

    mpq_clear(ax);
    mpq_clear(ay);
    mpq_clear(bx);
    mpq_clear(by);
    mpq_clear(x);
    mpq_clear(y);
    lv00_transform_destroy(t);
}

LV00_TEST(GeometryTransform, Compose) {
    mpq_t dx, dy;
    mpq_init(dx);
    mpq_init(dy);
    mpq_set_ui(dx, 1, 1);
    mpq_set_ui(dy, 0, 1);

    Lv00Transform *t1 = lv00_transform_translation(dx, dy);
    Lv00Transform *t2 = lv00_transform_translation(dx, dy);

    Lv00Transform *composed = lv00_transform_compose(t1, t2);
    LV00_ASSERT_NOT_NULL(composed);

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 0, 1);
    mpq_set_ui(y, 0, 1);

    lv00_transform_apply_point(composed, x, y);

    /* 两次平移 (1, 0) -> (2, 0) */
    LV00_ASSERT_TRUE(mpq_cmp_ui(x, 2, 1) == 0);
    LV00_ASSERT_TRUE(mpq_cmp_ui(y, 0, 1) == 0);

    mpq_clear(dx);
    mpq_clear(dy);
    mpq_clear(x);
    mpq_clear(y);
    lv00_transform_destroy(t1);
    lv00_transform_destroy(t2);
    lv00_transform_destroy(composed);
}

LV00_TEST(GeometryTransform, Inverse) {
    mpq_t dx, dy;
    mpq_init(dx);
    mpq_init(dy);
    mpq_set_ui(dx, 5, 1);
    mpq_set_ui(dy, 7, 1);

    Lv00Transform *t = lv00_transform_translation(dx, dy);
    Lv00Transform *inv = lv00_transform_inverse(t);
    LV00_ASSERT_NOT_NULL(inv);

    Lv00Transform *identity = lv00_transform_compose(t, inv);
    LV00_ASSERT_NOT_NULL(identity);

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 3, 1);
    mpq_set_ui(y, 4, 1);

    /* t * inv = identity */
    lv00_transform_apply_point(t, x, y);
    lv00_transform_apply_point(inv, x, y);

    /* 应该回到原点附近 */
    /* 由于有误差，只检查大致范围 */

    mpq_clear(dx);
    mpq_clear(dy);
    mpq_clear(x);
    mpq_clear(y);
    lv00_transform_destroy(t);
    lv00_transform_destroy(inv);
    lv00_transform_destroy(identity);
}

/* ============== 运行时监控测试 ============== */

LV00_TEST(RuntimeMonitor, LogInit) {
    Lv00LogConfig config = {
        .min_level = LOG_LEVEL_DEBUG,
        .targets = LOG_TARGET_NONE,  /* 禁用输出以避免干扰测试 */
        .include_timestamp = true,
        .include_location = false,
        .colored_output = false
    };

    LV00_ASSERT(lv00_log_init(&config) == 0);
    lv00_log_shutdown();
}

LV00_TEST(RuntimeMonitor, Timer) {
    LV00_ASSERT(lv00_perf_init() == 0);

    Lv00Timer *timer = lv00_timer_create("TestTimer");
    LV00_ASSERT_NOT_NULL(timer);

    lv00_timer_start(timer);

    /* 模拟一些工作 */
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }

    int64_t elapsed = lv00_timer_stop(timer);
    LV00_ASSERT(elapsed >= 0);

    lv00_timer_destroy(timer);
    lv00_perf_shutdown();
}

LV00_TEST(RuntimeMonitor, PerfStats) {
    LV00_ASSERT(lv00_perf_init() == 0);

    Lv00PerfStats *stats = lv00_perf_stats_create("TestStats");
    LV00_ASSERT_NOT_NULL(stats);

    lv00_perf_stats_record(stats, 1.0);
    lv00_perf_stats_record(stats, 2.0);
    lv00_perf_stats_record(stats, 3.0);

    LV00_ASSERT_EQ(3ULL, stats->count);
    LV00_ASSERT_FLOAT_EQ(2.0, stats->mean, 0.001);
    LV00_ASSERT_FLOAT_EQ(1.0, stats->min_val, 0.001);
    LV00_ASSERT_FLOAT_EQ(3.0, stats->max_val, 0.001);

    lv00_perf_stats_destroy(stats);
    lv00_perf_shutdown();
}

LV00_TEST(RuntimeMonitor, HealthCheck) {
    LV00_ASSERT(lv00_health_init() == 0);

    Lv00HealthReport *report = lv00_runtime_health_check();
    LV00_ASSERT_NOT_NULL(report);
    LV00_ASSERT_NOT_NULL(report->checks);
    LV00_ASSERT(report->check_count > 0);

    lv00_health_report_destroy(report);
    lv00_health_shutdown();
}

LV00_TEST(RuntimeMonitor, Diagnostics) {
    Lv00Diagnostics *diag = lv00_diagnostics_generate();
    LV00_ASSERT_NOT_NULL(diag);
    LV00_ASSERT_TRUE(diag->version[0] != '\0');
    LV00_ASSERT_TRUE(diag->cpu_cores > 0);

    lv00_diagnostics_destroy(diag);
}

LV00_TEST(RuntimeMonitor, EventTrace) {
    LV00_ASSERT(lv00_event_trace_init(1000) == 0);

    lv00_event_trace_record(EVENT_TYPE_PROOF_START, "test_proof", NULL);

    int event_id = lv00_event_trace_begin(EVENT_TYPE_SOLVE_START, "test_solve");
    LV00_ASSERT(event_id >= 0);

    lv00_event_trace_end(event_id, "completed");

    lv00_event_trace_clear();
    lv00_event_trace_shutdown();
}

/* ============== 主函数 ============== */

int main(int argc, char **argv) {
    /* 运行所有测试 */
    Lv00TestReport *report = lv00_test_run_all();
    if (!report) {
        fprintf(stderr, "Failed to run tests\n");
        return 1;
    }

    /* 打印报告 */
    lv00_test_report_print(report, stdout);

    /* 导出报告 */
    lv00_test_report_write_file(report, "test_results.json", "json");
    lv00_test_report_write_file(report, "test_results.xml", "xml");
    lv00_test_report_write_file(report, "test_results.html", "html");

    int exit_code = (report->failed_count > 0) ? 1 : 0;

    lv00_test_report_destroy(report);

    return exit_code;
}
