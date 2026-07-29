/**
 * @file benchmark.c
 * @brief 性能基准测试框架 - 微/宏基准测试、统计分析、结果比较
 *
 * @details 实现基准测试套件的创建/运行/报告、计时器、
 *          性能监控器等核心功能。
 */
#include "lv/benchmark.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_utils.h"
#include "lv/simd_ops.h"
#include "lv/symbolic_coord.h"
#include "lv/thread_pool.h"

/* 默认测试套件容量 */
#define SUITE_INIT_CAPACITY 16

/* ============ 计时器 ============ */

/**
 * @brief 创建并初始化一个新的计时器对象
 *
 * @return 返回初始化的 lvTimer 结构体
 */
lvTimer lv_timer_create(void) {
    lvTimer timer;
    timer.start = 0;
    timer.end = 0;
    timer.running = false;
    return timer;
}

void lv_timer_start(lvTimer *timer) {
    if (!timer)
        return;
    timer->start = lv_get_time_us();
    timer->running = true;
}

/**
 * @brief 停止计时器，记录结束时间戳
 *
 * @param timer 计时器指针
 */
void lv_timer_stop(lvTimer *timer) {
    if (!timer || !timer->running)
        return;
    timer->end = lv_get_time_us();
    timer->running = false;
}

/**
 * @brief 重置计时器，清零所有时间戳并停止运行
 *
 * @param timer 计时器指针
 */
void lv_timer_reset(lvTimer *timer) {
    if (!timer)
        return;
    timer->start = 0;
    timer->end = 0;
    timer->running = false;
}

/**
 * @brief 获取计时器已流逝的时间（微秒）
 *
 * @param timer 计时器指针
 * @return 已流逝的微秒数，如果 timer 为 NULL 返回 0
 */
uint64_t lv_timer_elapsed_us(const lvTimer *timer) {
    if (!timer)
        return 0;
    if (timer->running) {
        return lv_get_time_us() - timer->start;
    }
    return timer->end - timer->start;
}

/**
 * @brief 获取计时器已流逝的时间（毫秒）
 *
 * @param timer 计时器指针
 * @return 已流逝的毫秒数
 */
double lv_timer_elapsed_ms(const lvTimer *timer) {
    return (double) lv_timer_elapsed_us(timer) / 1000.0;
}

/**
 * @brief 获取计时器已流逝的时间（秒）
 *
 * @param timer 计时器指针
 * @return 已流逝的秒数
 */
double lv_timer_elapsed_sec(const lvTimer *timer) {
    return (double) lv_timer_elapsed_us(timer) / 1000000.0;
}

/* ============ 基准测试套件 ============ */

struct lvBenchSuite {
    char name[64];
    lvBenchCase *cases;
    int case_count;
    int case_capacity;
    lvBenchResult *results;
    int result_count;
};

/**
 * @brief 创建新的基准测试套件
 *
 * @param name 套件名称
 * @return 成功返回 lvBenchSuite 指针，失败返回 NULL
 */
lvBenchSuite *lv_bench_suite_create(const char *name) {
    lvBenchSuite *suite = lv_calloc(1, sizeof(lvBenchSuite));
    if (!suite)
        return NULL;
    if (name)
        lv_strlcpy(suite->name, name, sizeof(suite->name));
    suite->cases = lv_calloc(SUITE_INIT_CAPACITY, sizeof(lvBenchCase));
    suite->results = lv_calloc(SUITE_INIT_CAPACITY, sizeof(lvBenchResult));
    if (!suite->cases || !suite->results) {
        lv_free((void **) &suite->cases);
        lv_free((void **) &suite->results);
        lv_free((void **) &suite);
        return NULL;
    }
    suite->case_capacity = SUITE_INIT_CAPACITY;
    return suite;
}

/**
 * @brief 销毁基准测试套件，释放所有测试用例和结果内存
 *
 * @param suite 测试套件指针
 */
void lv_bench_suite_destroy(lvBenchSuite *suite) {
    if (!suite)
        return;
    lv_free((void **) &suite->cases);
    lv_free((void **) &suite->results);
    lv_free((void **) &suite);
}

/**
 * @brief 向测试套件中添加一个基准测试用例
 *
 * @param suite 测试套件指针
 * @param case_ 测试用例指针
 * @return 成功返回 0，失败返回 -1
 */
int lv_bench_suite_add(lvBenchSuite *suite, const lvBenchCase *case_) {
    if (!suite || !case_)
        return -1;
    if (suite->case_count >= suite->case_capacity) {
        if (suite->case_capacity > INT_MAX / 2)
            return -1;
        int new_cap = suite->case_capacity * 2;
        void *p = lv_realloc(suite->cases, (size_t) new_cap * sizeof(lvBenchCase));
        if (!p)
            return -1;
        suite->cases = p;
        suite->case_capacity = new_cap;
    }
    suite->cases[suite->case_count++] = *case_;
    return 0;
}

/**
 * @brief 获取测试套件的结果数量
 *
 * @param suite 测试套件指针
 * @return 结果数量，如果 suite 为 NULL 返回 0
 */
int lv_bench_suite_result_count(const lvBenchSuite *suite) {
    return suite ? suite->result_count : 0;
}

/**
 * @brief 获取测试套件中指定索引的测试结果
 *
 * @param suite 测试套件指针
 * @param index 结果索引
 * @return 成功返回结果指针，失败（越界或 suite 为 NULL）返回 NULL
 */
const lvBenchResult *lv_bench_suite_get_result(const lvBenchSuite *suite, int index) {
    if (!suite || index < 0 || index >= suite->result_count)
        return NULL;
    return &suite->results[index];
}

/* ============ 单次基准测试 ============ */

/**
 * @brief 执行单次基准测试，自动迭代找到满足目标时长的迭代次数并返回统计结果
 *
 * @param func 被测函数指针
 * @param user_data 用户自定义数据
 * @param min_iterations 最少迭代次数
 * @param target_time_sec 目标测试时长（秒）
 * @return 返回 lvBenchResult 结构体，包含平均值、标准差、吞吐量等统计信息
 */
lvBenchResult lv_benchmark_run(lvBenchFunc func, void *user_data, int min_iterations, double target_time_sec) {
    lvBenchResult result;
    memset(&result, 0, sizeof(result));

    if (!func) {
        lv_strlcpy(result.error_msg, "空函数指针", sizeof(result.error_msg));
        return result;
    }
    if (min_iterations <= 0)
        min_iterations = lv_BENCH_MIN_ITERATIONS;
    if (target_time_sec <= 0)
        target_time_sec = lv_BENCH_TARGET_TIME_SEC;

    /* 预热运行 */
    func(min_iterations, user_data);

    /* 正式测试 */
    int iterations = min_iterations;
    uint64_t total_us = 0;
    lvTimer timer = lv_timer_create();

    while (iterations <= lv_BENCH_MAX_ITERATIONS) {
        lv_timer_start(&timer);
        total_us = func(iterations, user_data);
        lv_timer_stop(&timer);

        if (total_us == 0) {
            /* 函数未返回有效耗时，使用计时器 */
            total_us = lv_timer_elapsed_us(&timer);
        }

        if ((double) total_us / 1000000.0 >= target_time_sec)
            break;
        iterations *= 2;
    }

    result.iterations = iterations;
    result.total_time_us = total_us;
    result.mean_us = (double) total_us / (double) iterations;
    result.min_us = result.mean_us;
    result.max_us = result.mean_us;
    result.std_dev_us = 0.0;
    result.ops_per_sec = (total_us > 0) ? (double) iterations * 1000000.0 / (double) total_us : 0.0;
    result.success = true;

    /* 简化百分位：均值作为近似 */
    result.percentiles[0] = result.mean_us; /* P50 */
    result.percentiles[1] = result.mean_us; /* P75 */
    result.percentiles[2] = result.mean_us; /* P90 */
    result.percentiles[3] = result.mean_us; /* P95 */
    result.percentiles[4] = result.mean_us; /* P99 */

    return result;
}

/**
 * @brief 执行完整的基准测试用例（包含 setup → run → teardown）
 *
 * @param case_ 测试用例指针
 * @return 返回 lvBenchResult 结构体
 */
lvBenchResult lv_benchmark_run_full(const lvBenchCase *case_) {
    lvBenchResult result;
    memset(&result, 0, sizeof(result));
    if (!case_ || !case_->func)
        return result;

    if (case_->setup)
        case_->setup(case_->user_data);

    result = lv_benchmark_run(case_->func, case_->user_data, case_->min_iterations, case_->target_time_sec);
    if (case_->name[0])
        lv_strlcpy(result.name, case_->name, sizeof(result.name));

    if (case_->teardown)
        case_->teardown(case_->user_data);
    return result;
}

/**
 * @brief 运行基准测试套件中的所有用例，收集所有测试结果
 *
 * @param suite 测试套件指针
 * @return 成功返回运行的结果数量，失败返回 -1
 */
int lv_bench_suite_run(lvBenchSuite *suite) {
    if (!suite)
        return -1;
    suite->result_count = 0;
    for (int i = 0; i < suite->case_count; i++) {
        lvBenchResult r = lv_benchmark_run_full(&suite->cases[i]);
        if (suite->result_count < suite->case_capacity) {
            suite->results[suite->result_count++] = r;
        }
    }
    return suite->result_count;
}

/* ============ 结果比较 ============ */

/**
 * @brief 比较基准测试结果与当前结果，检测性能回归
 *
 * @param baseline 基准结果指针
 * @param current 当前结果指针
 * @param regression_threshold 回归阈值（如 0.05 表示 5%）
 * @return 返回 lvBenchComparison 结构体，包含比值和回归判定
 */
lvBenchComparison lv_bench_compare(const lvBenchResult *baseline, const lvBenchResult *current,
                                   double regression_threshold) {
    lvBenchComparison cmp;
    memset(&cmp, 0, sizeof(cmp));
    cmp.regression_threshold = regression_threshold;

    if (!baseline || !current)
        return cmp;
    if (baseline->mean_us > 0)
        cmp.mean_ratio = current->mean_us / baseline->mean_us;
    if (baseline->min_us > 0)
        cmp.min_ratio = current->min_us / baseline->min_us;
    if (baseline->max_us > 0)
        cmp.max_ratio = current->max_us / baseline->max_us;
    if (baseline->ops_per_sec > 0)
        cmp.ops_ratio = current->ops_per_sec / baseline->ops_per_sec;

    cmp.is_regression = (cmp.mean_ratio > (1.0 + regression_threshold));
    return cmp;
}

/* ============ 性能监控器 ============ */

/**
 * @brief 创建性能监控器，用于统计操作耗时与内存使用
 *
 * @return 成功返回 lvPerfMonitor 指针，失败返回 NULL
 */
lvPerfMonitor *lv_perf_monitor_create(void) {
    return lv_calloc(1, sizeof(lvPerfMonitor));
}

/**
 * @brief 销毁性能监控器，释放占用内存
 *
 * @param monitor 监控器指针
 */
void lv_perf_monitor_destroy(lvPerfMonitor *monitor) {
    lv_free((void **) &monitor);
}

/**
 * @brief 记录一次操作耗时，更新最小/最大/总时间统计
 *
 * @param monitor 监控器指针
 * @param time_us 本次操作耗时（微秒）
 */
void lv_perf_record_op(lvPerfMonitor *monitor, uint64_t time_us) {
    if (!monitor)
        return;
    monitor->operations++;
    monitor->total_time_us += time_us;
    if (monitor->min_time_us == 0 || time_us < monitor->min_time_us) {
        monitor->min_time_us = time_us;
    }
    if (time_us > monitor->max_time_us) {
        monitor->max_time_us = time_us;
    }
}

/**
 * @brief 记录一次错误事件
 *
 * @param monitor 监控器指针
 */
void lv_perf_record_error(lvPerfMonitor *monitor) {
    if (monitor)
        monitor->errors++;
}

/**
 * @brief 记录内存分配事件，更新当前/峰值/累计分配量
 *
 * @param monitor 监控器指针
 * @param size 分配的内存大小（字节）
 */
void lv_perf_record_alloc(lvPerfMonitor *monitor, size_t size) {
    if (!monitor)
        return;
    monitor->memory_allocated += size;
    monitor->memory_current += size;
    if (monitor->memory_current > monitor->memory_peak) {
        monitor->memory_peak = monitor->memory_current;
    }
}

/**
 * @brief 记录内存释放事件，更新当前内存使用量
 *
 * @param monitor 监控器指针
 * @param size 释放的内存大小（字节）
 */
void lv_perf_record_destroy(lvPerfMonitor *monitor, size_t size) {
    if (!monitor)
        return;
    monitor->memory_freed += size;
    if (monitor->memory_current >= size) {
        monitor->memory_current -= size;
    } else {
        monitor->memory_current = 0;
    }
}

/**
 * @brief 计算平均每次操作耗时（微秒）
 *
 * @param monitor 监控器指针
 * @return 平均耗时，如果无操作记录返回 0.0
 */
double lv_perf_avg_time_us(const lvPerfMonitor *monitor) {
    if (!monitor || monitor->operations == 0)
        return 0.0;
    return (double) monitor->total_time_us / (double) monitor->operations;
}

/**
 * @brief 计算吞吐量（每秒操作次数）
 *
 * @param monitor 监控器指针
 * @return 每秒操作次数，如果无耗时数据返回 0.0
 */
double lv_perf_throughput(const lvPerfMonitor *monitor) {
    if (!monitor || monitor->total_time_us == 0)
        return 0.0;
    return (double) monitor->operations * 1000000.0 / (double) monitor->total_time_us;
}

/* ============ 内存统计辅助（简化实现） ============ */

/**
 * @brief 获取当前内存使用量
 *
 * @return 当前内存使用量（字节）
 */
size_t lv_get_memory_usage(void) {
    MemoryStats stats;
    lv_get_memory_stats(&stats);
    return stats.current_used;
}

/**
 * @brief 获取历史峰值内存使用量
 *
 * @return 峰值内存使用量（字节）
 */
size_t lv_get_peak_memory_usage(void) {
    MemoryStats stats;
    lv_get_memory_stats(&stats);
    return stats.peak_used;
}

/* ============ 内置基准测试 ============ */

/* ================================================================
 * 核心模块基准测试函数
 * ================================================================ */

static uint64_t bench_core_symbolic_coord(int iterations, void *user_data) {
    (void) user_data;
    lvTimer timer = lv_timer_create();
    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        SymbolicCoord *c = symbolic_coord_create_rational(i % 100, 100);
        symbolic_coord_destroy(c);
    }
    lv_timer_stop(&timer);
    return lv_timer_elapsed_us(&timer);
}

static uint64_t bench_core_constraint_graph(int iterations, void *user_data) {
    (void) user_data;
    lvTimer timer = lv_timer_create();
    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        ConstraintGraph *g = graph_create();
        if (g) {
            SymbolicCoord *coord = symbolic_coord_create_rational(i % 100, 100);
            SymbolicCoord *coords[] = {coord};
            (void) graph_add_point(g, coords, 1);
            symbolic_coord_destroy(coord);
            graph_destroy(g);
        }
    }
    lv_timer_stop(&timer);
    return lv_timer_elapsed_us(&timer);
}

static uint64_t bench_core_memory_pool(int iterations, void *user_data) {
    (void) user_data;
    lvTimer timer = lv_timer_create();
    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        void *p = lv_calloc(1, 64);
        lv_free((void **) &p);
    }
    lv_timer_stop(&timer);
    return lv_timer_elapsed_us(&timer);
}

/**
 * @brief 运行核心模块的基准测试套件
 *
 * 包括：符号坐标创建/销毁、约束图节点添加、内存池分配/释放
 *
 * @return 返回名称为 "core" 的测试套件指针
 */
lvBenchSuite *lv_bench_run_core_tests(void) {
    lvBenchSuite *suite = lv_bench_suite_create("core");
    if (!suite)
        return NULL;

    lvBenchCase cases[] = {
        {"symbolic_coord_create_destroy", bench_core_symbolic_coord, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS,
         lv_BENCH_MAX_ITERATIONS, lv_BENCH_TARGET_TIME_SEC},
        {"constraint_graph_add_node", bench_core_constraint_graph, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS,
         lv_BENCH_MAX_ITERATIONS, lv_BENCH_TARGET_TIME_SEC},
        {"memory_pool_alloc_free", bench_core_memory_pool, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS,
         lv_BENCH_MAX_ITERATIONS, lv_BENCH_TARGET_TIME_SEC},
    };

    int n = (int) (sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        lv_bench_suite_add(suite, &cases[i]);
    }

    return suite;
}

/* ================================================================
 * 内存模块基准测试函数
 * ================================================================ */

static uint64_t bench_memory_malloc_small(int iterations, void *user_data) {
    (void) user_data;
    lvTimer timer = lv_timer_create();
    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        void *p1 = lv_calloc(1, 64);
        void *p2 = lv_calloc(1, 256);
        lv_free((void **) &p2);
        lv_free((void **) &p1);
    }
    lv_timer_stop(&timer);
    return lv_timer_elapsed_us(&timer);
}

static uint64_t bench_memory_malloc_large(int iterations, void *user_data) {
    (void) user_data;
    lvTimer timer = lv_timer_create();
    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        void *p1 = lv_calloc(1, 1024 * 1024);
        void *p2 = lv_calloc(1, 4 * 1024 * 1024);
        lv_free((void **) &p2);
        lv_free((void **) &p1);
    }
    lv_timer_stop(&timer);
    return lv_timer_elapsed_us(&timer);
}

static uint64_t bench_memory_realloc_growth(int iterations, void *user_data) {
    (void) user_data;
    lvTimer timer = lv_timer_create();
    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        int *arr = (int *) lv_calloc(1, sizeof(int));
        if (!arr)
            continue;
        arr[0] = 0;
        for (int j = 2; j <= 10000; j++) {
            int *new_arr = (int *) lv_realloc(arr, (size_t) j * sizeof(int));
            if (!new_arr) {
                lv_free((void **) &arr);
                arr = NULL;
                break;
            }
            arr = new_arr;
            arr[j - 1] = j;
        }
        if (arr)
            lv_free((void **) &arr);
    }
    lv_timer_stop(&timer);
    return lv_timer_elapsed_us(&timer);
}

static uint64_t bench_memory_alloc_free_stress(int iterations, void *user_data) {
    (void) user_data;
#define STRESS_BLOCK_COUNT 20
    lvTimer timer = lv_timer_create();
    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        void *blocks[STRESS_BLOCK_COUNT];
        for (int k = 0; k < STRESS_BLOCK_COUNT; k++) {
            size_t sz = (size_t) (64 << (k % 6)); /* 64, 128, 256, 512, 1024, 2048 */
            blocks[k] = lv_calloc(1, sz);
        }
        for (int k = STRESS_BLOCK_COUNT - 1; k >= 0; k--) {
            lv_free((void **) &blocks[k]);
        }
    }
    lv_timer_stop(&timer);
    return lv_timer_elapsed_us(&timer);
#undef STRESS_BLOCK_COUNT
}

/**
 * @brief 运行内存模块的基准测试套件
 *
 * 包括：小对象分配、大对象分配、realloc 增长、交错分配压力测试
 *
 * @return 返回名称为 "memory" 的测试套件指针
 */
lvBenchSuite *lv_bench_run_memory_tests(void) {
    lvBenchSuite *suite = lv_bench_suite_create("memory");
    if (!suite)
        return NULL;

    lvBenchCase cases[] = {
        {"lv_malloc_small", bench_memory_malloc_small, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS,
         lv_BENCH_MAX_ITERATIONS, lv_BENCH_TARGET_TIME_SEC},
        {"lv_malloc_large", bench_memory_malloc_large, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS,
         lv_BENCH_MAX_ITERATIONS, lv_BENCH_TARGET_TIME_SEC},
        {"lv_realloc_growth", bench_memory_realloc_growth, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS,
         lv_BENCH_MAX_ITERATIONS, lv_BENCH_TARGET_TIME_SEC},
        {"lv_alloc_free_stress", bench_memory_alloc_free_stress, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS,
         lv_BENCH_MAX_ITERATIONS, lv_BENCH_TARGET_TIME_SEC},
    };

    int n = (int) (sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        lv_bench_suite_add(suite, &cases[i]);
    }

    return suite;
}

/* ================================================================
 * SIMD 模块基准测试函数
 * ================================================================ */

#define SIMD_VECTOR_DIM 4000

static uint64_t bench_simd_vector_dot(int iterations, void *user_data) {
    (void) user_data;
    lvTimer timer = lv_timer_create();

    /* 分配并初始化向量 */
    int dim = SIMD_VECTOR_DIM;
    double *va = (double *) lv_calloc((size_t) dim, sizeof(double));
    double *vb = (double *) lv_calloc((size_t) dim, sizeof(double));
    if (!va || !vb) {
        lv_free((void **) &va);
        lv_free((void **) &vb);
        return 0;
    }
    for (int i = 0; i < dim; i++) {
        va[i] = (double) (i + 1);
        vb[i] = (double) (dim - i);
    }

    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        double dot = 0.0;
        for (int j = 0; j < dim; j += 4) {
            lvVec4d a = lv_vec4d_load(&va[j]);
            lvVec4d b = lv_vec4d_load(&vb[j]);
            lvVec4d m = lv_vec4d_mul(a, b);
            dot += m.v[0] + m.v[1] + m.v[2] + m.v[3];
        }
        /* 防止编译器优化掉结果 */
        if (dot < -1e30)
            (void) dot;
    }
    lv_timer_stop(&timer);

    lv_free((void **) &va);
    lv_free((void **) &vb);
    return lv_timer_elapsed_us(&timer);
}

static uint64_t bench_simd_vector_add(int iterations, void *user_data) {
    (void) user_data;
    lvTimer timer = lv_timer_create();

    int dim = SIMD_VECTOR_DIM;
    double *va = (double *) lv_calloc((size_t) dim, sizeof(double));
    double *vb = (double *) lv_calloc((size_t) dim, sizeof(double));
    double *vr = (double *) lv_calloc((size_t) dim, sizeof(double));
    if (!va || !vb || !vr) {
        lv_free((void **) &va);
        lv_free((void **) &vb);
        lv_free((void **) &vr);
        return 0;
    }
    for (int i = 0; i < dim; i++) {
        va[i] = (double) (i + 1);
        vb[i] = (double) (dim - i);
    }

    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        for (int j = 0; j < dim; j += 4) {
            lvVec4d a = lv_vec4d_load(&va[j]);
            lvVec4d b = lv_vec4d_load(&vb[j]);
            lvVec4d r = lv_vec4d_add(a, b);
            vr[j] = r.v[0];
            vr[j + 1] = r.v[1];
            vr[j + 2] = r.v[2];
            vr[j + 3] = r.v[3];
        }
    }
    lv_timer_stop(&timer);

    lv_free((void **) &va);
    lv_free((void **) &vb);
    lv_free((void **) &vr);
    return lv_timer_elapsed_us(&timer);
}

static uint64_t bench_simd_vector_scale(int iterations, void *user_data) {
    (void) user_data;
    lvTimer timer = lv_timer_create();

    int dim = SIMD_VECTOR_DIM;
    double *va = (double *) lv_calloc((size_t) dim, sizeof(double));
    double *vr = (double *) lv_calloc((size_t) dim, sizeof(double));
    if (!va || !vr) {
        lv_free((void **) &va);
        lv_free((void **) &vr);
        return 0;
    }
    for (int i = 0; i < dim; i++) {
        va[i] = (double) (i + 1);
    }

    lvVec4d scalar = lv_vec4d_set1(2.5);

    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        for (int j = 0; j < dim; j += 4) {
            lvVec4d a = lv_vec4d_load(&va[j]);
            lvVec4d r = lv_vec4d_mul(a, scalar);
            vr[j] = r.v[0];
            vr[j + 1] = r.v[1];
            vr[j + 2] = r.v[2];
            vr[j + 3] = r.v[3];
        }
    }
    lv_timer_stop(&timer);

    lv_free((void **) &va);
    lv_free((void **) &vr);
    return lv_timer_elapsed_us(&timer);
}

static uint64_t bench_simd_matrix_vector(int iterations, void *user_data) {
    (void) user_data;
    lvTimer timer = lv_timer_create();

    /* 4x4 矩阵（列主序）*/
    double mat[16] = {1.0, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 3.0, 0.0, 1.0, 2.0, 3.0, 1.0};

    lvVec4d vec = lv_vec4d_set(1.0, 2.0, 3.0, 1.0);

    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        lvVec4d result = lv_simd_mat4x4_vec4_mul(mat, vec);
        /* 防止编译器优化掉 */
        if (result.v[0] < -1e30)
            (void) result;
    }
    lv_timer_stop(&timer);

    return lv_timer_elapsed_us(&timer);
}

#undef SIMD_VECTOR_DIM

/**
 * @brief 运行 SIMD 优化模块的基准测试套件
 *
 * 包括：向量点积、向量加法、向量标量乘法、矩阵-向量乘法
 *
 * @return 返回名称为 "simd" 的测试套件指针
 */
lvBenchSuite *lv_bench_run_simd_tests(void) {
    lvBenchSuite *suite = lv_bench_suite_create("simd");
    if (!suite)
        return NULL;

    lvBenchCase cases[] = {
        {"simd_vector_dot", bench_simd_vector_dot, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS, lv_BENCH_MAX_ITERATIONS,
         lv_BENCH_TARGET_TIME_SEC},
        {"simd_vector_add", bench_simd_vector_add, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS, lv_BENCH_MAX_ITERATIONS,
         lv_BENCH_TARGET_TIME_SEC},
        {"simd_vector_scale", bench_simd_vector_scale, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS,
         lv_BENCH_MAX_ITERATIONS, lv_BENCH_TARGET_TIME_SEC},
        {"simd_matrix_vector", bench_simd_matrix_vector, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS,
         lv_BENCH_MAX_ITERATIONS, lv_BENCH_TARGET_TIME_SEC},
    };

    int n = (int) (sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        lv_bench_suite_add(suite, &cases[i]);
    }

    return suite;
}

/* ================================================================
 * 多线程模块基准测试函数
 * ================================================================ */

/* 线程池内部函数的前向声明（不在公开头文件中） */
extern lvThreadPool *lv_thread_pool_create(int num_threads);
extern void lv_thread_pool_destroy(lvThreadPool *pool);

/* 与 thread_pool.c 中 lvThreadTask 布局匹配的本地结构 */
typedef struct {
    void (*func)(void *arg);
    void *arg;
    lvWaitGroup *group;
    void *next;
} BenchTask;

/* 占位任务函数 */
static void bench_thread_dummy_task(void *arg) {
    (void) arg;
}

/* 并行求和任务参数 */
typedef struct {
    const double *array;
    int start;
    int end;
    double partial_sum;
} BenchSumArg;

static void bench_thread_sum_task(void *arg) {
    BenchSumArg *sa = (BenchSumArg *) arg;
    double sum = 0.0;
    for (int i = sa->start; i < sa->end; i++) {
        sum += sa->array[i];
    }
    sa->partial_sum = sum;
}

static uint64_t bench_thread_submit(int iterations, void *user_data) {
    (void) user_data;
    lvThreadPool *pool = lv_get_global_thread_pool();
    if (!pool)
        return 0;

    lvTimer timer = lv_timer_create();
    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        BenchTask *task = (BenchTask *) lv_calloc(1, sizeof(BenchTask));
        if (!task)
            continue;
        task->func = bench_thread_dummy_task;
        task->arg = NULL;
        lvWaitGroup *wg = lv_thread_pool_submit(pool, (lvThreadTask *) task);
        if (!wg) {
            lv_free((void **) &task);
            continue;
        }
        lv_thread_pool_wait_group(pool, wg, -1);
    }
    lv_timer_stop(&timer);
    return lv_timer_elapsed_us(&timer);
}

#define THREAD_PARALLEL_SIZE 100000
#define THREAD_PARALLEL_CHUNKS 4

static uint64_t bench_thread_parallel_sum(int iterations, void *user_data) {
    (void) user_data;
    lvThreadPool *pool = lv_get_global_thread_pool();
    if (!pool)
        return 0;

    double *array = (double *) lv_calloc(THREAD_PARALLEL_SIZE, sizeof(double));
    if (!array)
        return 0;
    for (int k = 0; k < THREAD_PARALLEL_SIZE; k++) {
        array[k] = (double) (k + 1);
    }

    int chunk_size = THREAD_PARALLEL_SIZE / THREAD_PARALLEL_CHUNKS;

    lvTimer timer = lv_timer_create();
    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        BenchSumArg args[THREAD_PARALLEL_CHUNKS];
        BenchTask *tasks[THREAD_PARALLEL_CHUNKS];
        lvWaitGroup *groups[THREAD_PARALLEL_CHUNKS];
        int submitted = 0;

        for (int c = 0; c < THREAD_PARALLEL_CHUNKS; c++) {
            args[c].array = array;
            args[c].start = c * chunk_size;
            args[c].end = (c == THREAD_PARALLEL_CHUNKS - 1) ? THREAD_PARALLEL_SIZE : (c + 1) * chunk_size;
            args[c].partial_sum = 0.0;

            tasks[c] = (BenchTask *) lv_calloc(1, sizeof(BenchTask));
            if (!tasks[c])
                break;
            tasks[c]->func = bench_thread_sum_task;
            tasks[c]->arg = &args[c];

            groups[c] = lv_thread_pool_submit(pool, (lvThreadTask *) tasks[c]);
            if (!groups[c]) {
                lv_free((void **) &tasks[c]);
                break;
            }
            submitted++;
        }

        /* 等待所有提交的任务完成 */
        for (int c = 0; c < submitted; c++) {
            lv_thread_pool_wait_group(pool, groups[c], -1);
        }

        /* 汇总 */
        double total = 0.0;
        for (int c = 0; c < submitted; c++) {
            total += args[c].partial_sum;
        }
        if (total < -1e30)
            (void) total;
    }
    lv_timer_stop(&timer);

    lv_free((void **) &array);
    return lv_timer_elapsed_us(&timer);
}

#undef THREAD_PARALLEL_SIZE
#undef THREAD_PARALLEL_CHUNKS

static uint64_t bench_thread_create_destroy(int iterations, void *user_data) {
    (void) user_data;
    lvTimer timer = lv_timer_create();
    lv_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        lvThreadPool *pool = lv_thread_pool_create(2);
        if (pool) {
            lv_thread_pool_destroy(pool);
        }
    }
    lv_timer_stop(&timer);
    return lv_timer_elapsed_us(&timer);
}

/**
 * @brief 运行多线程模块的基准测试套件
 *
 * 包括：任务提交吞吐量、并行数组求和、线程池创建/销毁延迟
 *
 * @return 返回名称为 "thread" 的测试套件指针
 */
lvBenchSuite *lv_bench_run_thread_tests(void) {
    lvBenchSuite *suite = lv_bench_suite_create("thread");
    if (!suite)
        return NULL;

    lvBenchCase cases[] = {
        {"thread_pool_submit", bench_thread_submit, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS, lv_BENCH_MAX_ITERATIONS,
         lv_BENCH_TARGET_TIME_SEC},
        {"thread_pool_parallel_sum", bench_thread_parallel_sum, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS,
         lv_BENCH_MAX_ITERATIONS, lv_BENCH_TARGET_TIME_SEC},
        {"thread_pool_create_destroy", bench_thread_create_destroy, NULL, NULL, NULL, lv_BENCH_MIN_ITERATIONS,
         lv_BENCH_MAX_ITERATIONS, lv_BENCH_TARGET_TIME_SEC},
    };

    int n = (int) (sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        lv_bench_suite_add(suite, &cases[i]);
    }

    return suite;
}

/* ============ 报告导出 ============ */

void lv_bench_suite_print_report(const lvBenchSuite *suite, void *stream) {
    FILE *fp = stream ? (FILE *) stream : stdout;
    if (!suite) {
        fprintf(fp, "错误：套件为空\n");
        return;
    }
    fprintf(fp, "========================================\n");
    fprintf(fp, "  基准测试报告: %s\n", suite->name);
    fprintf(fp, "  日期: %s %s\n", __DATE__, __TIME__);
    fprintf(fp, "========================================\n");
    fprintf(fp, "%-32s %10s %12s %12s %14s\n", "测试项", "迭代次数", "平均(μs)", "标准差", "吞吐量(ops/s)");
    fprintf(fp, "----------------------------------------------------------------\n");
    for (int i = 0; i < suite->result_count; i++) {
        const lvBenchResult *r = &suite->results[i];
        fprintf(fp, "%-32s %10d %12.3f %12.3f %14.1f\n",
                r->name[0] ? r->name : "(未命名)",
                r->iterations,
                r->mean_us,
                r->std_dev_us,
                r->ops_per_sec);
    }
    fprintf(fp, "========================================\n");
}

char *lv_bench_suite_to_json(const lvBenchSuite *suite) {
    if (!suite) {
        char *buf = malloc(4);
        if (buf) buf[0] = '\0';
        return buf;
    }

    /* 第一遍：计算所需缓冲区大小 */
    int size = 2;
    size += (int) strlen("  \"name\": \"\",\n  \"results\": [\n  ]\n") + 64;
    for (int i = 0; i < suite->result_count; i++) {
        const lvBenchResult *r = &suite->results[i];
        size += 256 + (int) strlen(r->name);
    }

    char *buf = (char *) malloc((size_t) size);
    if (!buf)
        return NULL;

    /* 第二遍：格式化输出 */
    char *p = buf;
    p += sprintf(p, "{\n");
    p += sprintf(p, "  \"name\": \"%s\",\n", suite->name);
    p += sprintf(p, "  \"results\": [\n");
    for (int i = 0; i < suite->result_count; i++) {
        const lvBenchResult *r = &suite->results[i];
        p += sprintf(p,
                     "    {\n"
                     "      \"name\": \"%s\",\n"
                     "      \"iterations\": %d,\n"
                     "      \"mean_us\": %.3f,\n"
                     "      \"std_dev_us\": %.3f,\n"
                     "      \"min_us\": %.3f,\n"
                     "      \"max_us\": %.3f,\n"
                     "      \"ops_per_sec\": %.1f,\n"
                     "      \"total_time_us\": %llu,\n"
                     "      \"success\": %s\n"
                     "    }%s\n",
                     r->name,
                     r->iterations,
                     r->mean_us,
                     r->std_dev_us,
                     r->min_us,
                     r->max_us,
                     r->ops_per_sec,
                     (unsigned long long) r->total_time_us,
                     r->success ? "true" : "false",
                     (i < suite->result_count - 1) ? "," : "");
    }
    p += sprintf(p, "  ]\n");
    p += sprintf(p, "}\n");

    return buf;
}

char *lv_bench_suite_to_markdown(const lvBenchSuite *suite) {
    if (!suite) {
        char *buf = malloc(1);
        if (buf) buf[0] = '\0';
        return buf;
    }

    /* 第一遍：计算大小 */
    int size = 512;
    for (int i = 0; i < suite->result_count; i++) {
        const lvBenchResult *r = &suite->results[i];
        size += 128 + (int) strlen(r->name);
    }

    char *buf = (char *) malloc((size_t) size);
    if (!buf)
        return NULL;

    char *p = buf;
    p += sprintf(p, "# 基准测试报告: %s\n\n", suite->name);
    p += sprintf(p, "| 测试项 | 迭代次数 | 平均(μs) | 标准差 | 吞吐量(ops/s) |\n");
    p += sprintf(p, "|-------|---------|---------|-------|-------------|\n");
    for (int i = 0; i < suite->result_count; i++) {
        const lvBenchResult *r = &suite->results[i];
        p += sprintf(p, "| %s | %d | %.3f | %.3f | %.1f |\n",
                     r->name, r->iterations, r->mean_us, r->std_dev_us, r->ops_per_sec);
    }

    return buf;
}

void lv_perf_print_report(const lvPerfMonitor *monitor, void *stream) {
    FILE *fp = stream ? (FILE *) stream : stdout;
    if (!monitor) {
        fprintf(fp, "错误：监控器为空\n");
        return;
    }
    fprintf(fp, "========================================\n");
    fprintf(fp, "  性能监控报告\n");
    fprintf(fp, "  日期: %s %s\n", __DATE__, __TIME__);
    fprintf(fp, "========================================\n");
    fprintf(fp, "操作统计:\n");
    fprintf(fp, "  操作总数:        %llu\n", (unsigned long long) monitor->operations);
    fprintf(fp, "  错误数:          %llu\n", (unsigned long long) monitor->errors);
    fprintf(fp, "  总耗时(μs):      %llu\n", (unsigned long long) monitor->total_time_us);
    fprintf(fp, "  最小耗时(μs):    %llu\n", (unsigned long long) monitor->min_time_us);
    fprintf(fp, "  最大耗时(μs):    %llu\n", (unsigned long long) monitor->max_time_us);
    fprintf(fp, "  平均耗时(μs):    %.3f\n", lv_perf_avg_time_us(monitor));
    fprintf(fp, "  吞吐量(ops/s):   %.1f\n", lv_perf_throughput(monitor));
    fprintf(fp, "\n内存统计:\n");
    fprintf(fp, "  分配总量(字节):  %zu\n", monitor->memory_allocated);
    fprintf(fp, "  释放总量(字节):  %zu\n", monitor->memory_freed);
    fprintf(fp, "  当前使用(字节):  %zu\n", monitor->memory_current);
    fprintf(fp, "  峰值使用(字节):  %zu\n", monitor->memory_peak);
    fprintf(fp, "========================================\n");
}
