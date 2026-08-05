/**
 * @file test_performance.c
 * @brief 性能基准测试
 *
 * 测试内容：
 * - 性能分析器基本功能
 * - 内存池分配性能
 * - malloc vs 内存池对比
 *
 * @version 3.5.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/memory_pool.h"
#include "lv/performance_profiler.h"

#include "test_helpers.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ================================================================
 * 辅助类型
 * ================================================================ */

typedef struct {
    int data[16]; /* 64 bytes */
} TestObj;

/* ================================================================
 * 测试：性能分析器基本功能
 * ================================================================ */

static void test_perf_profiler_basic(void) {
    printf("\n--- Performance Profiler Basic ---\n");

    lvPerfSession *session = lv_perf_session_create("test_session");
    TEST_ASSERT_NOT_NULL(session);

    /* 测试时间测量 */
    lv_perf_begin(session, "test_region");

    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }
    (void) sum;

    lv_perf_end(session, "test_region");

    /* 测试内存记录 */
    lv_perf_session_record_alloc(session, "TestType", 1024);
    lv_perf_session_record_alloc(session, "TestType", 2048);
    lv_perf_session_record_free(session, "TestType", 1024);

    /* 打印报告 */
    lv_perf_report_print(session, stdout);

    /* 测试 JSON 导出 */
    char json_buffer[4096];
    int json_len = lv_perf_report_to_json(session, json_buffer, sizeof(json_buffer));
    TEST_ASSERT(json_len > 0, "JSON export should succeed");
    TEST_ASSERT(strstr(json_buffer, "test_session") != NULL, "JSON should contain session name");
    TEST_ASSERT(strstr(json_buffer, "test_region") != NULL, "JSON should contain region name");
    TEST_ASSERT(strstr(json_buffer, "TestType") != NULL, "JSON should contain type name");

    /* 测试重置 */
    lv_perf_session_reset(session);

    lv_perf_session_destroy(session);
}

/* ================================================================
 * 测试：内存池分配性能
 * ================================================================ */

static lvObjectPool *g_test_pool = NULL;

static void bench_pool_alloc_free(void) {
    TestObj *obj = (TestObj *) lv_pool_alloc(g_test_pool);
    if (obj) {
        obj->data[0] = 1;
        lv_pool_free(g_test_pool, obj);
    }
}

static void test_pool_performance(void) {
    printf("\n--- Memory Pool Performance ---\n");

    lvPoolConfig config = {
        .object_size = sizeof(TestObj), .capacity = 1000, .thread_safe = false, .auto_grow = true, .name = "test_pool"};

    g_test_pool = lv_pool_create(&config);
    TEST_ASSERT_NOT_NULL(g_test_pool);

    lvPerfBenchResult result;
    int err = lv_perf_benchmark_run("pool_alloc_free", bench_pool_alloc_free, NULL, &result);
    TEST_ASSERT_EQ(err, 0);
    lv_perf_benchmark_print_result("pool_alloc_free", &result, stdout);

    TEST_ASSERT(result.mean_ns < 1000.0, "Pool alloc+free should be < 1us");

    lv_pool_destroy(g_test_pool);
    g_test_pool = NULL;
}

/* ================================================================
 * 测试：malloc vs 内存池对比
 * ================================================================ */

static void bench_malloc_free(void) {
    TestObj *obj = (TestObj *) lv_malloc(sizeof(TestObj));
    if (obj) {
        obj->data[0] = 1;
        lv_free((void **) &obj);
    }
}

static void test_malloc_vs_pool(void) {
    printf("\n--- Malloc vs Pool Comparison ---\n");

    lvPerfBenchResult malloc_result;
    lv_perf_benchmark_run("malloc_free", bench_malloc_free, NULL, &malloc_result);
    lv_perf_benchmark_print_result("malloc_free", &malloc_result, stdout);

    lvPoolConfig config = {.object_size = sizeof(TestObj),
                           .capacity = 1000,
                           .thread_safe = false,
                           .auto_grow = true,
                           .name = "compare_pool"};

    g_test_pool = lv_pool_create(&config);
    if (g_test_pool) {
        lvPerfBenchResult pool_result;
        lv_perf_benchmark_run("pool_alloc_free", bench_pool_alloc_free, NULL, &pool_result);
        lv_perf_benchmark_print_result("pool_alloc_free", &pool_result, stdout);

        double speedup = malloc_result.mean_ns / pool_result.mean_ns;
        printf("  Speedup: %.2fx\n", speedup);

        TEST_ASSERT(speedup > 1.0, "Pool should be faster than malloc");

        lv_pool_destroy(g_test_pool);
        g_test_pool = NULL;
    }
}

/* ================================================================
 * 测试套件
 * ================================================================ */

TEST_MAIN_BEGIN("Performance")
    printf("=== Performance Tests ===\n");

    TEST_MAIN_RUN(test_perf_profiler_basic);
    TEST_MAIN_RUN(test_pool_performance);
    TEST_MAIN_RUN(test_malloc_vs_pool);

TEST_MAIN_END()
