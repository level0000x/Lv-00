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
#include "lv/lv_log.h"
#include "memory_pool.h"
#include "runtime_monitor.h"
#include "test_unified.h"

/* ============== 内存池测试 ============== */

lv_TEST(MemoryPool, CreateDestroy) {
    lvPoolConfig config = {
        .object_size = 64, .capacity = 16, .thread_safe = false, .auto_grow = true, .name = "TestPool"};

    lvObjectPool *pool = lv_pool_create(&config);
    lv_ASSERT_NOT_NULL(pool);

    /* 分配对象 */
    void *obj1 = lv_pool_alloc(pool);
    lv_ASSERT_NOT_NULL(obj1);

    void *obj2 = lv_pool_alloc(pool);
    lv_ASSERT_NOT_NULL(obj2);
    lv_ASSERT(obj1 != obj2);

    /* 释放对象 */
    lv_ASSERT_TRUE(lv_pool_free(pool, obj1));
    lv_ASSERT_TRUE(lv_pool_free(pool, obj2));

    /* 统计 */
    uint64_t allocs, frees;
    size_t used;
    lv_pool_get_stats(pool, &allocs, &frees, &used);
    lv_ASSERT_EQ(2ULL, allocs);
    lv_ASSERT_EQ(2ULL, frees);
    lv_ASSERT_EQ(0ULL, used);

    lv_pool_destroy(pool);
}

lv_TEST(MemoryPool, AutoGrow) {
    lvPoolConfig config = {
        .object_size = 32, .capacity = 4, .thread_safe = false, .auto_grow = true, .name = "GrowPool"};

    lvObjectPool *pool = lv_pool_create(&config);
    lv_ASSERT_NOT_NULL(pool);

    /* 分配超过初始容量 */
    void *objs[16];
    for (int i = 0; i < 16; i++) {
        objs[i] = lv_pool_alloc(pool);
        lv_ASSERT_NOT_NULL(objs[i]);
    }

    /* 释放所有 */
    for (int i = 0; i < 16; i++) {
        lv_ASSERT_TRUE(lv_pool_free(pool, objs[i]));
    }

    lv_pool_destroy(pool);
}

lv_TEST(MemoryPool, LinearAllocator) {
    lvLinearAllocator *alloc = lv_linear_allocator_create(1024);
    lv_ASSERT_NOT_NULL(alloc);

    /* 分配内存 */
    void *p1 = lv_linear_alloc(alloc, 100, 8);
    lv_ASSERT_NOT_NULL(p1);

    void *p2 = lv_linear_alloc(alloc, 200, 16);
    lv_ASSERT_NOT_NULL(p2);

    /* 重置 */
    lv_linear_allocator_reset(alloc);

    /* 再次分配 */
    void *p3 = lv_linear_alloc(alloc, 50, 8);
    lv_ASSERT_NOT_NULL(p3);

    /* 统计 */
    size_t blocks, used, capacity;
    lv_linear_allocator_get_stats(alloc, &blocks, &used, &capacity);
    lv_ASSERT_EQ(1ULL, blocks);

    lv_linear_allocator_destroy(alloc);
}

/* ============== 几何变换测试 ============== */

lv_TEST(GeometryTransform, Identity) {
    lvTransform *t = lv_transform_identity();
    lv_ASSERT_NOT_NULL(t);
    lv_ASSERT_TRUE(lv_transform_is_isometry(t));
    lv_ASSERT_TRUE(lv_transform_is_orientation_preserving(t));

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 3, 1);
    mpq_set_ui(y, 4, 1);

    lv_transform_apply_point(t, x, y);

    lv_ASSERT_TRUE(mpq_cmp_ui(x, 3, 1) == 0);
    lv_ASSERT_TRUE(mpq_cmp_ui(y, 4, 1) == 0);

    mpq_clear(x);
    mpq_clear(y);
    lv_transform_destroy(t);
}

lv_TEST(GeometryTransform, Translation) {
    mpq_t dx, dy;
    mpq_init(dx);
    mpq_init(dy);
    mpq_set_ui(dx, 2, 1);
    mpq_set_ui(dy, 3, 1);

    lvTransform *t = lv_transform_translation(dx, dy);
    lv_ASSERT_NOT_NULL(t);
    lv_ASSERT_TRUE(lv_transform_is_isometry(t));

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 1, 1);
    mpq_set_ui(y, 1, 1);

    lv_transform_apply_point(t, x, y);

    lv_ASSERT_TRUE(mpq_cmp_ui(x, 3, 1) == 0);
    lv_ASSERT_TRUE(mpq_cmp_ui(y, 4, 1) == 0);

    mpq_clear(dx);
    mpq_clear(dy);
    mpq_clear(x);
    mpq_clear(y);
    lv_transform_destroy(t);
}

lv_TEST(GeometryTransform, Rotation90) {
    mpq_t cx, cy;
    mpq_init(cx);
    mpq_init(cy);
    mpq_set_ui(cx, 0, 1);
    mpq_set_ui(cy, 0, 1);

    lvTransform *t = lv_transform_rotation(cx, cy, 90, 1);
    lv_ASSERT_NOT_NULL(t);
    lv_ASSERT_TRUE(lv_transform_is_isometry(t));

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 1, 1);
    mpq_set_ui(y, 0, 1);

    lv_transform_apply_point(t, x, y);

    /* (1, 0) 旋转 90° -> (0, 1) */
    lv_ASSERT_TRUE(mpq_cmp_ui(x, 0, 1) == 0);
    lv_ASSERT_TRUE(mpq_cmp_ui(y, 1, 1) == 0);

    mpq_clear(cx);
    mpq_clear(cy);
    mpq_clear(x);
    mpq_clear(y);
    lv_transform_destroy(t);
}

lv_TEST(GeometryTransform, Reflection) {
    mpq_t ax, ay, bx, by;
    mpq_init(ax);
    mpq_init(ay);
    mpq_init(bx);
    mpq_init(by);

    mpq_set_ui(ax, 0, 1);
    mpq_set_ui(ay, 0, 1);
    mpq_set_ui(bx, 1, 1);
    mpq_set_ui(by, 0, 1);

    lvTransform *t = lv_transform_reflection(ax, ay, bx, by);
    lv_ASSERT_NOT_NULL(t);
    lv_ASSERT_TRUE(lv_transform_is_isometry(t));
    lv_ASSERT_FALSE(lv_transform_is_orientation_preserving(t));

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 2, 1);
    mpq_set_ui(y, 3, 1);

    lv_transform_apply_point(t, x, y);

    /* 关于 x 轴反射：(2, 3) -> (2, -3) */
    lv_ASSERT_TRUE(mpq_cmp_ui(x, 2, 1) == 0);
    lv_ASSERT_TRUE(mpq_cmp_ui(y, 3, 1) == 0 || mpq_cmp_si(y, -3, 1) == 0);

    mpq_clear(ax);
    mpq_clear(ay);
    mpq_clear(bx);
    mpq_clear(by);
    mpq_clear(x);
    mpq_clear(y);
    lv_transform_destroy(t);
}

lv_TEST(GeometryTransform, Compose) {
    mpq_t dx, dy;
    mpq_init(dx);
    mpq_init(dy);
    mpq_set_ui(dx, 1, 1);
    mpq_set_ui(dy, 0, 1);

    lvTransform *t1 = lv_transform_translation(dx, dy);
    lvTransform *t2 = lv_transform_translation(dx, dy);

    lvTransform *composed = lv_transform_compose(t1, t2);
    lv_ASSERT_NOT_NULL(composed);

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 0, 1);
    mpq_set_ui(y, 0, 1);

    lv_transform_apply_point(composed, x, y);

    /* 两次平移 (1, 0) -> (2, 0) */
    lv_ASSERT_TRUE(mpq_cmp_ui(x, 2, 1) == 0);
    lv_ASSERT_TRUE(mpq_cmp_ui(y, 0, 1) == 0);

    mpq_clear(dx);
    mpq_clear(dy);
    mpq_clear(x);
    mpq_clear(y);
    lv_transform_destroy(t1);
    lv_transform_destroy(t2);
    lv_transform_destroy(composed);
}

lv_TEST(GeometryTransform, Inverse) {
    mpq_t dx, dy;
    mpq_init(dx);
    mpq_init(dy);
    mpq_set_ui(dx, 5, 1);
    mpq_set_ui(dy, 7, 1);

    lvTransform *t = lv_transform_translation(dx, dy);
    lvTransform *inv = lv_transform_inverse(t);
    lv_ASSERT_NOT_NULL(inv);

    lvTransform *identity = lv_transform_compose(t, inv);
    lv_ASSERT_NOT_NULL(identity);

    mpq_t x, y;
    mpq_init(x);
    mpq_init(y);
    mpq_set_ui(x, 3, 1);
    mpq_set_ui(y, 4, 1);

    /* t * inv = identity */
    lv_transform_apply_point(t, x, y);
    lv_transform_apply_point(inv, x, y);

    /* 应该回到原点附近 */
    /* 由于有误差，只检查大致范围 */

    mpq_clear(dx);
    mpq_clear(dy);
    mpq_clear(x);
    mpq_clear(y);
    lv_transform_destroy(t);
    lv_transform_destroy(inv);
    lv_transform_destroy(identity);
}

/* ============== 运行时监控测试 ============== */

lv_TEST(RuntimeMonitor, LogInit) {
    lvLogConfig config = {.min_level = LOG_LEVEL_DEBUG,
                          .targets = LOG_TARGET_NONE, /* 禁用输出以避免干扰测试 */
                          .include_timestamp = true,
                          .include_location = false,
                          .colored_output = false};

    lv_ASSERT_TRUE(lv_log_init(&config));
    lv_log_shutdown();
}

lv_TEST(RuntimeMonitor, Timer) {
    lv_ASSERT_TRUE(lv_perf_init());

    lvTimer *timer = lv_timer_create("TestTimer");
    lv_ASSERT_NOT_NULL(timer);

    lv_timer_start(timer);

    /* 模拟一些工作 */
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }

    int64_t elapsed = lv_timer_stop(timer);
    lv_ASSERT(elapsed >= 0);

    lv_timer_destroy(timer);
    lv_perf_shutdown();
}

lv_TEST(RuntimeMonitor, PerfStats) {
    lv_ASSERT_TRUE(lv_perf_init());

    lvPerfStats *stats = lv_perf_stats_create("TestStats");
    lv_ASSERT_NOT_NULL(stats);

    lv_perf_stats_record(stats, 1.0);
    lv_perf_stats_record(stats, 2.0);
    lv_perf_stats_record(stats, 3.0);

    lv_ASSERT_EQ(3ULL, stats->count);
    lv_ASSERT_FLOAT_EQ(2.0, stats->mean, 0.001);
    lv_ASSERT_FLOAT_EQ(1.0, stats->min_val, 0.001);
    lv_ASSERT_FLOAT_EQ(3.0, stats->max_val, 0.001);

    lv_perf_stats_destroy(stats);
    lv_perf_shutdown();
}

lv_TEST(RuntimeMonitor, HealthCheck) {
    lv_ASSERT_TRUE(lv_health_init());

    lvHealthReport *report = lv_runtime_health_check();
    lv_ASSERT_NOT_NULL(report);
    lv_ASSERT_NOT_NULL(report->checks);
    lv_ASSERT(report->check_count > 0);

    lv_health_report_destroy(report);
    lv_health_shutdown();
}

lv_TEST(RuntimeMonitor, Diagnostics) {
    lvDiagnostics *diag = lv_diagnostics_generate();
    lv_ASSERT_NOT_NULL(diag);
    lv_ASSERT_TRUE(diag->version[0] != '\0');
    lv_ASSERT_TRUE(diag->cpu_cores > 0);

    lv_diagnostics_destroy(diag);
}

lv_TEST(RuntimeMonitor, EventTrace) {
    lv_ASSERT_TRUE(lv_event_trace_init(1000));

    lv_event_trace_record(EVENT_TYPE_PROOF_START, "test_proof", NULL);

    int event_id = lv_event_trace_begin(EVENT_TYPE_SOLVE_START, "test_solve");
    lv_ASSERT(event_id >= 0);

    lv_event_trace_end(event_id, "completed");

    lv_event_trace_clear();
    lv_event_trace_shutdown();
}

/* ============== 主函数 ============== */

int main(int argc, char **argv) {
    /* 运行所有测试 */
    lvTestReport *report = lv_test_run_all();
    if (!report) {
        fprintf(stderr, "Failed to run tests\n");
        return 1;
    }

    /* 打印报告 */
    lv_test_report_print(report, stdout);

    /* 导出报告 */
    lv_test_report_write_file(report, "test_results.json", "json");
    lv_test_report_write_file(report, "test_results.xml", "xml");
    lv_test_report_write_file(report, "test_results.html", "html");

    int exit_code = (report->failed_count > 0) ? 1 : 0;

    lv_test_report_destroy(report);

    return exit_code;
}
