/**
 * @file test_performance.c
 * @brief 性能基准测试
 *
 * 测试内容：
 * - 内存池分配性能
 * - 约束图操作性能
 * - 几何计算性能
 * - 推理缓存性能
 *
 * @version 3.5.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00/lv00.h"
#include "lv00/performance_profiler.h"
#include "lv00/memory_pool.h"
#include "lv00/constraint_graph.h"
#include "lv00/reasoning_cache.h"
#include "test_helpers.h"

/* ================================================================
 * 基准测试：内存池分配
 * ================================================================ */

static Lv00ObjectPool *g_test_pool = NULL;

typedef struct TestObject {
    int data[16];  /* 64 bytes */
} TestObject;

static void benchmark_pool_alloc_free(void) {
    TestObject *obj = (TestObject *)lv00_pool_alloc(g_test_pool);
    if (obj) {
        obj->data[0] = 1;
        lv00_pool_free(g_test_pool, obj);
    }
}

static int test_pool_performance(void) {
    printf("\n--- Memory Pool Performance ---\n");
    
    Lv00PoolConfig config = {
        .object_size = sizeof(TestObject),
        .capacity = 1000,
        .thread_safe = false,
        .auto_grow = true,
        .name = "test_pool"
    };
    
    g_test_pool = lv00_pool_create(&config);
    TEST_ASSERT_NOT_NULL(g_test_pool, "Failed to create pool");
    
    Lv00BenchmarkResult result;
    int err = lv00_benchmark_run("pool_alloc_free", benchmark_pool_alloc_free, NULL, &result);
    TEST_ASSERT_EQ(err, 0, "Benchmark should succeed");
    
    lv00_benchmark_print_result("pool_alloc_free", &result, stdout);
    
    /* 性能断言：单次分配+释放应在 1 微秒内完成 */
    TEST_ASSERT(result.mean_ns < 1000.0, "Pool alloc+free should be < 1us");
    
    lv00_pool_destroy(g_test_pool);
    g_test_pool = NULL;
    
    return 0;
}

/* ================================================================
 * 基准测试：约束图操作
 * ================================================================ */

static ConstraintGraph *g_test_graph = NULL;

static void benchmark_graph_add_point(void) {
    static int counter = 0;
    SymbolicCoord *coords[2] = {
        symbolic_coord_create_rational(counter % 100, 1),
        symbolic_coord_create_rational(counter / 100, 1)
    };
    
    graph_add_point_with_id(g_test_graph, 1000 + counter, coords, 2);
    counter++;
    
    /* 注意：这里不销毁坐标，为了测试性能 */
}

static int test_graph_performance(void) {
    printf("\n--- Constraint Graph Performance ---\n");
    
    g_test_graph = graph_create();
    TEST_ASSERT_NOT_NULL(g_test_graph, "Failed to create graph");
    
    Lv00BenchmarkResult result;
    int err = lv00_benchmark_run("graph_add_point", benchmark_graph_add_point, NULL, &result);
    TEST_ASSERT_EQ(err, 0, "Benchmark should succeed");
    
    lv00_benchmark_print_result("graph_add_point", &result, stdout);
    
    graph_destroy(g_test_graph);
    g_test_graph = NULL;
    
    return 0;
}

/* ================================================================
 * 基准测试：推理缓存
 * ================================================================ */

static Lv00ReasoningCache *g_test_cache = NULL;

static void benchmark_cache_put_get(void) {
    static uint64_t key = 0;
    
    lv00_reasoning_cache_put(g_test_cache, key, (int)(key % 100));
    int value = lv00_reasoning_cache_get(g_test_cache, key);
    (void)value;  /* 抑制未使用警告 */
    
    key++;
}

static int test_cache_performance(void) {
    printf("\n--- Reasoning Cache Performance ---\n");
    
    g_test_cache = lv00_reasoning_cache_create(4096);
    TEST_ASSERT_NOT_NULL(g_test_cache, "Failed to create cache");
    
    Lv00BenchmarkResult result;
    int err = lv00_benchmark_run("cache_put_get", benchmark_cache_put_get, NULL, &result);
    TEST_ASSERT_EQ(err, 0, "Benchmark should succeed");
    
    lv00_benchmark_print_result("cache_put_get", &result, stdout);
    
    lv00_reasoning_cache_destroy(g_test_cache);
    g_test_cache = NULL;
    
    return 0;
}

/* ================================================================
 * 测试：性能分析器功能
 * ================================================================ */

static int test_perf_profiler_basic(void) {
    printf("\n--- Performance Profiler Basic ---\n");
    
    Lv00PerfSession *session = lv00_perf_session_create("test_session");
    TEST_ASSERT_NOT_NULL(session, "Failed to create session");
    
    /* 测试时间测量 */
    lv00_perf_begin(session, "test_region");
    
    /* 模拟一些工作 */
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }
    (void)sum;
    
    lv00_perf_end(session, "test_region");
    
    /* 测试内存记录 */
    lv00_perf_record_alloc(session, "TestType", 1024);
    lv00_perf_record_alloc(session, "TestType", 2048);
    lv00_perf_record_free(session, "TestType", 1024);
    
    /* 打印报告 */
    lv00_perf_report_print(session, stdout);
    
    /* 测试 JSON 导出 */
    char json_buffer[4096];
    int json_len = lv00_perf_report_to_json(session, json_buffer, sizeof(json_buffer));
    TEST_ASSERT_GT(json_len, 0, "JSON export should succeed");
    TEST_ASSERT(strstr(json_buffer, "test_session") != NULL, "JSON should contain session name");
    TEST_ASSERT(strstr(json_buffer, "test_region") != NULL, "JSON should contain region name");
    TEST_ASSERT(strstr(json_buffer, "TestType") != NULL, "JSON should contain type name");
    
    /* 测试重置 */
    lv00_perf_session_reset(session);
    
    lv00_perf_session_destroy(session);
    
    return 0;
}

/* ================================================================
 * 测试：性能对比（malloc vs 内存池）
 * ================================================================ */

typedef struct {
    int data[16];
} MallocTestObj;

static void benchmark_malloc_free(void) {
    MallocTestObj *obj = (MallocTestObj *)malloc(sizeof(MallocTestObj));
    if (obj) {
        obj->data[0] = 1;
        free(obj);
    }
}

static int test_malloc_vs_pool(void) {
    printf("\n--- Malloc vs Pool Comparison ---\n");
    
    /* 测试 malloc */
    Lv00BenchmarkResult malloc_result;
    lv00_benchmark_run("malloc_free", benchmark_malloc_free, NULL, &malloc_result);
    lv00_benchmark_print_result("malloc_free", &malloc_result, stdout);
    
    /* 测试内存池 */
    Lv00PoolConfig config = {
        .object_size = sizeof(MallocTestObj),
        .capacity = 1000,
        .thread_safe = false,
        .auto_grow = true,
        .name = "compare_pool"
    };
    
    g_test_pool = lv00_pool_create(&config);
    if (g_test_pool) {
        Lv00BenchmarkResult pool_result;
        lv00_benchmark_run("pool_alloc_free", benchmark_pool_alloc_free, NULL, &pool_result);
        lv00_benchmark_print_result("pool_alloc_free", &pool_result, stdout);
        
        /* 计算加速比 */
        double speedup = malloc_result.mean_ns / pool_result.mean_ns;
        printf("  Speedup: %.2fx\n", speedup);
        
        /* 内存池应该比 malloc 快 */
        TEST_ASSERT(speedup > 1.0, "Pool should be faster than malloc");
        
        lv00_pool_destroy(g_test_pool);
        g_test_pool = NULL;
    }
    
    return 0;
}

/* ================================================================
 * 测试套件
 * ================================================================ */

int main(void) {
    printf("=== Performance Tests ===\n");
    
    TEST_RUN(test_perf_profiler_basic);
    TEST_RUN(test_pool_performance);
    TEST_RUN(test_graph_performance);
    TEST_RUN(test_cache_performance);
    TEST_RUN(test_malloc_vs_pool);
    
    TEST_SUMMARY();
    return g_fail_count > 0 ? 1 : 0;
}
