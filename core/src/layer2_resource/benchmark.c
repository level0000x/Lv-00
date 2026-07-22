/**
 * @file benchmark.c
 * @brief 性能基准测试框架 - 微/宏基准测试、统计分析、结果比较
 *
 * @details 实现基准测试套件的创建/运行/报告、计时器、
 *          性能监控器等核心功能。
 */
#include "lv00/benchmark.h"
#include "lv00/lv00_utils.h"
#include "lv00/symbolic_coord.h"
#include "lv00/constraint_graph.h"
#include "lv00/simd_ops.h"
#include "lv00/thread_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* 默认测试套件容量 */
#define SUITE_INIT_CAPACITY 16

/* ============ 计时器 ============ */

/**
 * @brief 创建并初始化一个新的计时器对象
 *
 * @return 返回初始化的 Lv00Timer 结构体
 */
Lv00Timer lv00_timer_create(void) {
    Lv00Timer timer;
    timer.start = 0;
    timer.end = 0;
    timer.running = false;
    return timer;
}

void lv00_timer_start(Lv00Timer *timer) {
    if (!timer) return;
    timer->start = lv00_get_time_us();
    timer->running = true;
}

/**
 * @brief 停止计时器，记录结束时间戳
 *
 * @param timer 计时器指针
 */
void lv00_timer_stop(Lv00Timer *timer) {
    if (!timer || !timer->running) return;
    timer->end = lv00_get_time_us();
    timer->running = false;
}

/**
 * @brief 重置计时器，清零所有时间戳并停止运行
 *
 * @param timer 计时器指针
 */
void lv00_timer_reset(Lv00Timer *timer) {
    if (!timer) return;
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
uint64_t lv00_timer_elapsed_us(const Lv00Timer *timer) {
    if (!timer) return 0;
    if (timer->running) {
        return lv00_get_time_us() - timer->start;
    }
    return timer->end - timer->start;
}

/**
 * @brief 获取计时器已流逝的时间（毫秒）
 *
 * @param timer 计时器指针
 * @return 已流逝的毫秒数
 */
double lv00_timer_elapsed_ms(const Lv00Timer *timer) {
    return (double)lv00_timer_elapsed_us(timer) / 1000.0;
}

/**
 * @brief 获取计时器已流逝的时间（秒）
 *
 * @param timer 计时器指针
 * @return 已流逝的秒数
 */
double lv00_timer_elapsed_sec(const Lv00Timer *timer) {
    return (double)lv00_timer_elapsed_us(timer) / 1000000.0;
}

/* ============ 基准测试套件 ============ */

struct Lv00BenchSuite {
    char name[64];
    Lv00BenchCase *cases;
    int case_count;
    int case_capacity;
    Lv00BenchResult *results;
    int result_count;
};

/**
 * @brief 创建新的基准测试套件
 *
 * @param name 套件名称
 * @return 成功返回 Lv00BenchSuite 指针，失败返回 NULL
 */
Lv00BenchSuite *lv00_bench_suite_create(const char *name) {
    Lv00BenchSuite *suite = lv00_calloc(1, sizeof(Lv00BenchSuite));
    if (!suite) return NULL;
    if (name) lv00_strlcpy(suite->name, name, sizeof(suite->name));
    suite->cases = lv00_calloc(SUITE_INIT_CAPACITY, sizeof(Lv00BenchCase));
    suite->results = lv00_calloc(SUITE_INIT_CAPACITY, sizeof(Lv00BenchResult));
    if (!suite->cases || !suite->results) {
        lv00_free((void **)&suite->cases);
        lv00_free((void **)&suite->results);
        lv00_free((void **)&suite);
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
void lv00_bench_suite_destroy(Lv00BenchSuite *suite) {
    if (!suite) return;
    lv00_free((void **)&suite->cases);
    lv00_free((void **)&suite->results);
    lv00_free((void **)&suite);
}

/**
 * @brief 向测试套件中添加一个基准测试用例
 *
 * @param suite 测试套件指针
 * @param case_ 测试用例指针
 * @return 成功返回 0，失败返回 -1
 */
int lv00_bench_suite_add(Lv00BenchSuite *suite, const Lv00BenchCase *case_) {
    if (!suite || !case_) return -1;
    if (suite->case_count >= suite->case_capacity) {
        int new_cap = suite->case_capacity * 2;
        void *p = lv00_realloc(suite->cases, (size_t)new_cap * sizeof(Lv00BenchCase));
        if (!p) return -1;
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
int lv00_bench_suite_result_count(const Lv00BenchSuite *suite) {
    return suite ? suite->result_count : 0;
}

/**
 * @brief 获取测试套件中指定索引的测试结果
 *
 * @param suite 测试套件指针
 * @param index 结果索引
 * @return 成功返回结果指针，失败（越界或 suite 为 NULL）返回 NULL
 */
const Lv00BenchResult *lv00_bench_suite_get_result(const Lv00BenchSuite *suite, int index) {
    if (!suite || index < 0 || index >= suite->result_count) return NULL;
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
 * @return 返回 Lv00BenchResult 结构体，包含平均值、标准差、吞吐量等统计信息
 */
Lv00BenchResult lv00_benchmark_run(Lv00BenchFunc func, void *user_data,
                                    int min_iterations, double target_time_sec) {
    Lv00BenchResult result;
    memset(&result, 0, sizeof(result));

    if (!func) {
        lv00_strlcpy(result.error_msg, "空函数指针", sizeof(result.error_msg));
        return result;
    }
    if (min_iterations <= 0) min_iterations = LV00_BENCH_MIN_ITERATIONS;
    if (target_time_sec <= 0) target_time_sec = LV00_BENCH_TARGET_TIME_SEC;

    /* 预热运行 */
    func(min_iterations, user_data);

    /* 正式测试 */
    int iterations = min_iterations;
    uint64_t total_us = 0;
    Lv00Timer timer = lv00_timer_create();

    while (iterations <= LV00_BENCH_MAX_ITERATIONS) {
        lv00_timer_start(&timer);
        total_us = func(iterations, user_data);
        lv00_timer_stop(&timer);

        if (total_us == 0) {
            /* 函数未返回有效耗时，使用计时器 */
            total_us = lv00_timer_elapsed_us(&timer);
        }

        if ((double)total_us / 1000000.0 >= target_time_sec) break;
        iterations *= 2;
    }

    result.iterations = iterations;
    result.total_time_us = total_us;
    result.mean_us = (double)total_us / (double)iterations;
    result.min_us = result.mean_us;
    result.max_us = result.mean_us;
    result.std_dev_us = 0.0;
    result.ops_per_sec = (total_us > 0) ?
        (double)iterations * 1000000.0 / (double)total_us : 0.0;
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
 * @return 返回 Lv00BenchResult 结构体
 */
Lv00BenchResult lv00_benchmark_run_full(const Lv00BenchCase *case_) {
    Lv00BenchResult result;
    memset(&result, 0, sizeof(result));
    if (!case_ || !case_->func) return result;

    if (case_->setup) case_->setup(case_->user_data);

    result = lv00_benchmark_run(case_->func, case_->user_data,
                                 case_->min_iterations, case_->target_time_sec);
    if (case_->name[0]) lv00_strlcpy(result.name, case_->name, sizeof(result.name));

    if (case_->teardown) case_->teardown(case_->user_data);
    return result;
}

/**
 * @brief 运行基准测试套件中的所有用例，收集所有测试结果
 *
 * @param suite 测试套件指针
 * @return 成功返回运行的结果数量，失败返回 -1
 */
int lv00_bench_suite_run(Lv00BenchSuite *suite) {
    if (!suite) return -1;
    suite->result_count = 0;
    for (int i = 0; i < suite->case_count; i++) {
        Lv00BenchResult r = lv00_benchmark_run_full(&suite->cases[i]);
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
 * @return 返回 Lv00BenchComparison 结构体，包含比值和回归判定
 */
Lv00BenchComparison lv00_bench_compare(const Lv00BenchResult *baseline,
                                        const Lv00BenchResult *current,
                                        double regression_threshold) {
    Lv00BenchComparison cmp;
    memset(&cmp, 0, sizeof(cmp));
    cmp.regression_threshold = regression_threshold;

    if (!baseline || !current) return cmp;
    if (baseline->mean_us > 0) cmp.mean_ratio = current->mean_us / baseline->mean_us;
    if (baseline->min_us > 0) cmp.min_ratio = current->min_us / baseline->min_us;
    if (baseline->max_us > 0) cmp.max_ratio = current->max_us / baseline->max_us;
    if (baseline->ops_per_sec > 0) cmp.ops_ratio = current->ops_per_sec / baseline->ops_per_sec;

    cmp.is_regression = (cmp.mean_ratio > (1.0 + regression_threshold));
    return cmp;
}

/* ============ 性能监控器 ============ */

/**
 * @brief 创建性能监控器，用于统计操作耗时与内存使用
 *
 * @return 成功返回 Lv00PerfMonitor 指针，失败返回 NULL
 */
Lv00PerfMonitor *lv00_perf_monitor_create(void) {
    return lv00_calloc(1, sizeof(Lv00PerfMonitor));
}

/**
 * @brief 销毁性能监控器，释放占用内存
 *
 * @param monitor 监控器指针
 */
void lv00_perf_monitor_destroy(Lv00PerfMonitor *monitor) {
    lv00_free((void **)&monitor);
}

/**
 * @brief 记录一次操作耗时，更新最小/最大/总时间统计
 *
 * @param monitor 监控器指针
 * @param time_us 本次操作耗时（微秒）
 */
void lv00_perf_record_op(Lv00PerfMonitor *monitor, uint64_t time_us) {
    if (!monitor) return;
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
void lv00_perf_record_error(Lv00PerfMonitor *monitor) {
    if (monitor) monitor->errors++;
}

/**
 * @brief 记录内存分配事件，更新当前/峰值/累计分配量
 *
 * @param monitor 监控器指针
 * @param size 分配的内存大小（字节）
 */
void lv00_perf_record_alloc(Lv00PerfMonitor *monitor, size_t size) {
    if (!monitor) return;
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
void lv00_perf_record_destroy(Lv00PerfMonitor *monitor, size_t size) {
    if (!monitor) return;
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
double lv00_perf_avg_time_us(const Lv00PerfMonitor *monitor) {
    if (!monitor || monitor->operations == 0) return 0.0;
    return (double)monitor->total_time_us / (double)monitor->operations;
}

/**
 * @brief 计算吞吐量（每秒操作次数）
 *
 * @param monitor 监控器指针
 * @return 每秒操作次数，如果无耗时数据返回 0.0
 */
double lv00_perf_throughput(const Lv00PerfMonitor *monitor) {
    if (!monitor || monitor->total_time_us == 0) return 0.0;
    return (double)monitor->operations * 1000000.0 / (double)monitor->total_time_us;
}

/* ============ 内存统计辅助（简化实现） ============ */

/**
 * @brief 获取当前内存使用量
 *
 * @return 当前内存使用量（字节）
 */
size_t lv00_get_memory_usage(void) {
    MemoryStats stats;
    lv00_get_memory_stats(&stats);
    return stats.current_used;
}

/**
 * @brief 获取历史峰值内存使用量
 *
 * @return 峰值内存使用量（字节）
 */
size_t lv00_get_peak_memory_usage(void) {
    MemoryStats stats;
    lv00_get_memory_stats(&stats);
    return stats.peak_used;
}

/* ============ 内置基准测试 ============ */

/* ================================================================
 * 核心模块基准测试函数
 * ================================================================ */

static uint64_t bench_core_symbolic_coord(int iterations, void *user_data) {
    (void)user_data;
    Lv00Timer timer = lv00_timer_create();
    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        SymbolicCoord *c = symbolic_coord_create_rational(i % 100, 100);
        symbolic_coord_destroy(c);
    }
    lv00_timer_stop(&timer);
    return lv00_timer_elapsed_us(&timer);
}

static uint64_t bench_core_constraint_graph(int iterations, void *user_data) {
    (void)user_data;
    Lv00Timer timer = lv00_timer_create();
    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        ConstraintGraph *g = graph_create();
        if (g) {
            SymbolicCoord *coord = symbolic_coord_create_rational(i % 100, 100);
            SymbolicCoord *coords[] = { coord };
            (void)graph_add_point(g, coords, 1);
            symbolic_coord_destroy(coord);
            graph_destroy(g);
        }
    }
    lv00_timer_stop(&timer);
    return lv00_timer_elapsed_us(&timer);
}

static uint64_t bench_core_memory_pool(int iterations, void *user_data) {
    (void)user_data;
    Lv00Timer timer = lv00_timer_create();
    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        void *p = lv00_malloc(64);
        lv00_free((void **)&p);
    }
    lv00_timer_stop(&timer);
    return lv00_timer_elapsed_us(&timer);
}

/**
 * @brief 运行核心模块的基准测试套件
 *
 * 包括：符号坐标创建/销毁、约束图节点添加、内存池分配/释放
 *
 * @return 返回名称为 "core" 的测试套件指针
 */
Lv00BenchSuite *lv00_bench_run_core_tests(void) {
    Lv00BenchSuite *suite = lv00_bench_suite_create("core");
    if (!suite) return NULL;

    Lv00BenchCase cases[] = {
        { "symbolic_coord_create_destroy", bench_core_symbolic_coord,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
        { "constraint_graph_add_node", bench_core_constraint_graph,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
        { "memory_pool_alloc_free", bench_core_memory_pool,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
    };

    int n = (int)(sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        lv00_bench_suite_add(suite, &cases[i]);
    }

    return suite;
}

/* ================================================================
 * 内存模块基准测试函数
 * ================================================================ */

static uint64_t bench_memory_malloc_small(int iterations, void *user_data) {
    (void)user_data;
    Lv00Timer timer = lv00_timer_create();
    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        void *p1 = lv00_malloc(64);
        void *p2 = lv00_malloc(256);
        lv00_free((void **)&p2);
        lv00_free((void **)&p1);
    }
    lv00_timer_stop(&timer);
    return lv00_timer_elapsed_us(&timer);
}

static uint64_t bench_memory_malloc_large(int iterations, void *user_data) {
    (void)user_data;
    Lv00Timer timer = lv00_timer_create();
    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        void *p1 = lv00_malloc(1024 * 1024);
        void *p2 = lv00_malloc(4 * 1024 * 1024);
        lv00_free((void **)&p2);
        lv00_free((void **)&p1);
    }
    lv00_timer_stop(&timer);
    return lv00_timer_elapsed_us(&timer);
}

static uint64_t bench_memory_realloc_growth(int iterations, void *user_data) {
    (void)user_data;
    Lv00Timer timer = lv00_timer_create();
    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        int *arr = (int *)lv00_malloc(sizeof(int));
        if (!arr) continue;
        arr[0] = 0;
        for (int j = 2; j <= 10000; j++) {
            int *new_arr = (int *)lv00_realloc(arr, (size_t)j * sizeof(int));
            if (!new_arr) {
                lv00_free((void **)&arr);
                arr = NULL;
                break;
            }
            arr = new_arr;
            arr[j - 1] = j;
        }
        if (arr) lv00_free((void **)&arr);
    }
    lv00_timer_stop(&timer);
    return lv00_timer_elapsed_us(&timer);
}

static uint64_t bench_memory_alloc_free_stress(int iterations, void *user_data) {
    (void)user_data;
#define STRESS_BLOCK_COUNT 20
    Lv00Timer timer = lv00_timer_create();
    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        void *blocks[STRESS_BLOCK_COUNT];
        for (int k = 0; k < STRESS_BLOCK_COUNT; k++) {
            size_t sz = (size_t)(64 << (k % 6)); /* 64, 128, 256, 512, 1024, 2048 */
            blocks[k] = lv00_malloc(sz);
        }
        for (int k = STRESS_BLOCK_COUNT - 1; k >= 0; k--) {
            lv00_free((void **)&blocks[k]);
        }
    }
    lv00_timer_stop(&timer);
    return lv00_timer_elapsed_us(&timer);
#undef STRESS_BLOCK_COUNT
}

/**
 * @brief 运行内存模块的基准测试套件
 *
 * 包括：小对象分配、大对象分配、realloc 增长、交错分配压力测试
 *
 * @return 返回名称为 "memory" 的测试套件指针
 */
Lv00BenchSuite *lv00_bench_run_memory_tests(void) {
    Lv00BenchSuite *suite = lv00_bench_suite_create("memory");
    if (!suite) return NULL;

    Lv00BenchCase cases[] = {
        { "lv00_malloc_small", bench_memory_malloc_small,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
        { "lv00_malloc_large", bench_memory_malloc_large,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
        { "lv00_realloc_growth", bench_memory_realloc_growth,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
        { "lv00_alloc_free_stress", bench_memory_alloc_free_stress,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
    };

    int n = (int)(sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        lv00_bench_suite_add(suite, &cases[i]);
    }

    return suite;
}

/* ================================================================
 * SIMD 模块基准测试函数
 * ================================================================ */

#define SIMD_VECTOR_DIM 4000

static uint64_t bench_simd_vector_dot(int iterations, void *user_data) {
    (void)user_data;
    Lv00Timer timer = lv00_timer_create();

    /* 分配并初始化向量 */
    int dim = SIMD_VECTOR_DIM;
    double *va = (double *)lv00_malloc((size_t)dim * sizeof(double));
    double *vb = (double *)lv00_malloc((size_t)dim * sizeof(double));
    if (!va || !vb) {
        lv00_free((void **)&va);
        lv00_free((void **)&vb);
        return 0;
    }
    for (int i = 0; i < dim; i++) {
        va[i] = (double)(i + 1);
        vb[i] = (double)(dim - i);
    }

    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        double dot = 0.0;
        for (int j = 0; j < dim; j += 4) {
            Lv00Vec4d a = lv00_vec4d_load(&va[j]);
            Lv00Vec4d b = lv00_vec4d_load(&vb[j]);
            Lv00Vec4d m = lv00_vec4d_mul(a, b);
            dot += m.v[0] + m.v[1] + m.v[2] + m.v[3];
        }
        /* 防止编译器优化掉结果 */
        if (dot < -1e30) (void)dot;
    }
    lv00_timer_stop(&timer);

    lv00_free((void **)&va);
    lv00_free((void **)&vb);
    return lv00_timer_elapsed_us(&timer);
}

static uint64_t bench_simd_vector_add(int iterations, void *user_data) {
    (void)user_data;
    Lv00Timer timer = lv00_timer_create();

    int dim = SIMD_VECTOR_DIM;
    double *va = (double *)lv00_malloc((size_t)dim * sizeof(double));
    double *vb = (double *)lv00_malloc((size_t)dim * sizeof(double));
    double *vr = (double *)lv00_malloc((size_t)dim * sizeof(double));
    if (!va || !vb || !vr) {
        lv00_free((void **)&va);
        lv00_free((void **)&vb);
        lv00_free((void **)&vr);
        return 0;
    }
    for (int i = 0; i < dim; i++) {
        va[i] = (double)(i + 1);
        vb[i] = (double)(dim - i);
    }

    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        for (int j = 0; j < dim; j += 4) {
            Lv00Vec4d a = lv00_vec4d_load(&va[j]);
            Lv00Vec4d b = lv00_vec4d_load(&vb[j]);
            Lv00Vec4d r = lv00_vec4d_add(a, b);
            vr[j]     = r.v[0];
            vr[j + 1] = r.v[1];
            vr[j + 2] = r.v[2];
            vr[j + 3] = r.v[3];
        }
    }
    lv00_timer_stop(&timer);

    lv00_free((void **)&va);
    lv00_free((void **)&vb);
    lv00_free((void **)&vr);
    return lv00_timer_elapsed_us(&timer);
}

static uint64_t bench_simd_vector_scale(int iterations, void *user_data) {
    (void)user_data;
    Lv00Timer timer = lv00_timer_create();

    int dim = SIMD_VECTOR_DIM;
    double *va = (double *)lv00_malloc((size_t)dim * sizeof(double));
    double *vr = (double *)lv00_malloc((size_t)dim * sizeof(double));
    if (!va || !vr) {
        lv00_free((void **)&va);
        lv00_free((void **)&vr);
        return 0;
    }
    for (int i = 0; i < dim; i++) {
        va[i] = (double)(i + 1);
    }

    Lv00Vec4d scalar = lv00_vec4d_set1(2.5);

    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        for (int j = 0; j < dim; j += 4) {
            Lv00Vec4d a = lv00_vec4d_load(&va[j]);
            Lv00Vec4d r = lv00_vec4d_mul(a, scalar);
            vr[j]     = r.v[0];
            vr[j + 1] = r.v[1];
            vr[j + 2] = r.v[2];
            vr[j + 3] = r.v[3];
        }
    }
    lv00_timer_stop(&timer);

    lv00_free((void **)&va);
    lv00_free((void **)&vr);
    return lv00_timer_elapsed_us(&timer);
}

static uint64_t bench_simd_matrix_vector(int iterations, void *user_data) {
    (void)user_data;
    Lv00Timer timer = lv00_timer_create();

    /* 4x4 矩阵（列主序）*/
    double mat[16] = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 2.0, 0.0, 0.0,
        0.0, 0.0, 3.0, 0.0,
        1.0, 2.0, 3.0, 1.0
    };

    Lv00Vec4d vec = lv00_vec4d_set(1.0, 2.0, 3.0, 1.0);

    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        Lv00Vec4d result = lv00_simd_mat4x4_vec4_mul(mat, vec);
        /* 防止编译器优化掉 */
        if (result.v[0] < -1e30) (void)result;
    }
    lv00_timer_stop(&timer);

    return lv00_timer_elapsed_us(&timer);
}

#undef SIMD_VECTOR_DIM

/**
 * @brief 运行 SIMD 优化模块的基准测试套件
 *
 * 包括：向量点积、向量加法、向量标量乘法、矩阵-向量乘法
 *
 * @return 返回名称为 "simd" 的测试套件指针
 */
Lv00BenchSuite *lv00_bench_run_simd_tests(void) {
    Lv00BenchSuite *suite = lv00_bench_suite_create("simd");
    if (!suite) return NULL;

    Lv00BenchCase cases[] = {
        { "simd_vector_dot", bench_simd_vector_dot,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
        { "simd_vector_add", bench_simd_vector_add,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
        { "simd_vector_scale", bench_simd_vector_scale,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
        { "simd_matrix_vector", bench_simd_matrix_vector,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
    };

    int n = (int)(sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        lv00_bench_suite_add(suite, &cases[i]);
    }

    return suite;
}

/* ================================================================
 * 多线程模块基准测试函数
 * ================================================================ */

/* 线程池内部函数的前向声明（不在公开头文件中） */
extern Lv00ThreadPool *lv00_thread_pool_create(int num_threads);
extern void lv00_thread_pool_destroy(Lv00ThreadPool *pool);

/* 与 thread_pool.c 中 Lv00ThreadTask 布局匹配的本地结构 */
typedef struct {
    void (*func)(void *arg);
    void *arg;
    Lv00WaitGroup *group;
    void *next;
} BenchTask;

/* 占位任务函数 */
static void bench_thread_dummy_task(void *arg) {
    (void)arg;
}

/* 并行求和任务参数 */
typedef struct {
    const double *array;
    int start;
    int end;
    double partial_sum;
} BenchSumArg;

static void bench_thread_sum_task(void *arg) {
    BenchSumArg *sa = (BenchSumArg *)arg;
    double sum = 0.0;
    for (int i = sa->start; i < sa->end; i++) {
        sum += sa->array[i];
    }
    sa->partial_sum = sum;
}

static uint64_t bench_thread_submit(int iterations, void *user_data) {
    (void)user_data;
    Lv00ThreadPool *pool = lv00_get_global_thread_pool();
    if (!pool) return 0;

    Lv00Timer timer = lv00_timer_create();
    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        BenchTask *task = (BenchTask *)lv00_calloc(1, sizeof(BenchTask));
        if (!task) continue;
        task->func = bench_thread_dummy_task;
        task->arg = NULL;
        Lv00WaitGroup *wg = lv00_thread_pool_submit(pool, (Lv00ThreadTask *)task);
        if (!wg) {
            lv00_free((void **)&task);
            continue;
        }
        lv00_thread_pool_wait_group(pool, wg, -1);
    }
    lv00_timer_stop(&timer);
    return lv00_timer_elapsed_us(&timer);
}

#define THREAD_PARALLEL_SIZE 100000
#define THREAD_PARALLEL_CHUNKS 4

static uint64_t bench_thread_parallel_sum(int iterations, void *user_data) {
    (void)user_data;
    Lv00ThreadPool *pool = lv00_get_global_thread_pool();
    if (!pool) return 0;

    double *array = (double *)lv00_malloc(THREAD_PARALLEL_SIZE * sizeof(double));
    if (!array) return 0;
    for (int k = 0; k < THREAD_PARALLEL_SIZE; k++) {
        array[k] = (double)(k + 1);
    }

    int chunk_size = THREAD_PARALLEL_SIZE / THREAD_PARALLEL_CHUNKS;

    Lv00Timer timer = lv00_timer_create();
    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        BenchSumArg args[THREAD_PARALLEL_CHUNKS];
        BenchTask *tasks[THREAD_PARALLEL_CHUNKS];
        Lv00WaitGroup *groups[THREAD_PARALLEL_CHUNKS];
        int submitted = 0;

        for (int c = 0; c < THREAD_PARALLEL_CHUNKS; c++) {
            args[c].array = array;
            args[c].start = c * chunk_size;
            args[c].end = (c == THREAD_PARALLEL_CHUNKS - 1)
                              ? THREAD_PARALLEL_SIZE
                              : (c + 1) * chunk_size;
            args[c].partial_sum = 0.0;

            tasks[c] = (BenchTask *)lv00_calloc(1, sizeof(BenchTask));
            if (!tasks[c]) break;
            tasks[c]->func = bench_thread_sum_task;
            tasks[c]->arg = &args[c];

            groups[c] = lv00_thread_pool_submit(pool, (Lv00ThreadTask *)tasks[c]);
            if (!groups[c]) {
                lv00_free((void **)&tasks[c]);
                break;
            }
            submitted++;
        }

        /* 等待所有提交的任务完成 */
        for (int c = 0; c < submitted; c++) {
            lv00_thread_pool_wait_group(pool, groups[c], -1);
        }

        /* 汇总 */
        double total = 0.0;
        for (int c = 0; c < submitted; c++) {
            total += args[c].partial_sum;
        }
        if (total < -1e30) (void)total;
    }
    lv00_timer_stop(&timer);

    lv00_free((void **)&array);
    return lv00_timer_elapsed_us(&timer);
}

#undef THREAD_PARALLEL_SIZE
#undef THREAD_PARALLEL_CHUNKS

static uint64_t bench_thread_create_destroy(int iterations, void *user_data) {
    (void)user_data;
    Lv00Timer timer = lv00_timer_create();
    lv00_timer_start(&timer);
    for (int i = 0; i < iterations; i++) {
        Lv00ThreadPool *pool = lv00_thread_pool_create(2);
        if (pool) {
            lv00_thread_pool_destroy(pool);
        }
    }
    lv00_timer_stop(&timer);
    return lv00_timer_elapsed_us(&timer);
}

/**
 * @brief 运行多线程模块的基准测试套件
 *
 * 包括：任务提交吞吐量、并行数组求和、线程池创建/销毁延迟
 *
 * @return 返回名称为 "thread" 的测试套件指针
 */
Lv00BenchSuite *lv00_bench_run_thread_tests(void) {
    Lv00BenchSuite *suite = lv00_bench_suite_create("thread");
    if (!suite) return NULL;

    Lv00BenchCase cases[] = {
        { "thread_pool_submit", bench_thread_submit,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
        { "thread_pool_parallel_sum", bench_thread_parallel_sum,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
        { "thread_pool_create_destroy", bench_thread_create_destroy,
          NULL, NULL, NULL,
          LV00_BENCH_MIN_ITERATIONS, LV00_BENCH_MAX_ITERATIONS, LV00_BENCH_TARGET_TIME_SEC },
    };

    int n = (int)(sizeof(cases) / sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        lv00_bench_suite_add(suite, &cases[i]);
    }

    return suite;
}
