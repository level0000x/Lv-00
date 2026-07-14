/**
 * @file benchmark.c
 * @brief 性能基准测试框架 - 微/宏基准测试、统计分析、结果比较
 *
 * @details 实现基准测试套件的创建/运行/报告、计时器、
 *          性能监控器等核心功能。
 */
#include "lv00/benchmark.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* 默认测试套件容量 */
#define SUITE_INIT_CAPACITY 16

/* ============ 计时器 ============ */

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

void lv00_timer_stop(Lv00Timer *timer) {
    if (!timer || !timer->running) return;
    timer->end = lv00_get_time_us();
    timer->running = false;
}

void lv00_timer_reset(Lv00Timer *timer) {
    if (!timer) return;
    timer->start = 0;
    timer->end = 0;
    timer->running = false;
}

uint64_t lv00_timer_elapsed_us(const Lv00Timer *timer) {
    if (!timer) return 0;
    if (timer->running) {
        return lv00_get_time_us() - timer->start;
    }
    return timer->end - timer->start;
}

double lv00_timer_elapsed_ms(const Lv00Timer *timer) {
    return (double)lv00_timer_elapsed_us(timer) / 1000.0;
}

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

void lv00_bench_suite_destroy(Lv00BenchSuite *suite) {
    if (!suite) return;
    lv00_free((void **)&suite->cases);
    lv00_free((void **)&suite->results);
    lv00_free((void **)&suite);
}

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

int lv00_bench_suite_result_count(const Lv00BenchSuite *suite) {
    return suite ? suite->result_count : 0;
}

const Lv00BenchResult *lv00_bench_suite_get_result(const Lv00BenchSuite *suite, int index) {
    if (!suite || index < 0 || index >= suite->result_count) return NULL;
    return &suite->results[index];
}

/* ============ 单次基准测试 ============ */

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

Lv00PerfMonitor *lv00_perf_monitor_create(void) {
    return lv00_calloc(1, sizeof(Lv00PerfMonitor));
}

void lv00_perf_monitor_destroy(Lv00PerfMonitor *monitor) {
    lv00_free((void **)&monitor);
}

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

void lv00_perf_record_error(Lv00PerfMonitor *monitor) {
    if (monitor) monitor->errors++;
}

void lv00_perf_record_alloc(Lv00PerfMonitor *monitor, size_t size) {
    if (!monitor) return;
    monitor->memory_allocated += size;
    monitor->memory_current += size;
    if (monitor->memory_current > monitor->memory_peak) {
        monitor->memory_peak = monitor->memory_current;
    }
}

void lv00_perf_record_free(Lv00PerfMonitor *monitor, size_t size) {
    if (!monitor) return;
    monitor->memory_freed += size;
    if (monitor->memory_current >= size) {
        monitor->memory_current -= size;
    } else {
        monitor->memory_current = 0;
    }
}

double lv00_perf_avg_time_us(const Lv00PerfMonitor *monitor) {
    if (!monitor || monitor->operations == 0) return 0.0;
    return (double)monitor->total_time_us / (double)monitor->operations;
}

double lv00_perf_throughput(const Lv00PerfMonitor *monitor) {
    if (!monitor || monitor->total_time_us == 0) return 0.0;
    return (double)monitor->operations * 1000000.0 / (double)monitor->total_time_us;
}

/* ============ 内存统计辅助（简化实现） ============ */

size_t lv00_get_memory_usage(void) {
    MemoryStats stats;
    lv00_get_memory_stats(&stats);
    return stats.current_used;
}

size_t lv00_get_peak_memory_usage(void) {
    MemoryStats stats;
    lv00_get_memory_stats(&stats);
    return stats.peak_used;
}

void lv00_reset_memory_stats(void) {
    /* 委托给 lv00_utils 的实现 */
}

/* ============ 内置基准测试（桩实现） ============ */

Lv00BenchSuite *lv00_bench_run_core_tests(void) {
    return lv00_bench_suite_create("core");
}

Lv00BenchSuite *lv00_bench_run_memory_tests(void) {
    return lv00_bench_suite_create("memory");
}

Lv00BenchSuite *lv00_bench_run_simd_tests(void) {
    return lv00_bench_suite_create("simd");
}

Lv00BenchSuite *lv00_bench_run_thread_tests(void) {
    return lv00_bench_suite_create("thread");
}
