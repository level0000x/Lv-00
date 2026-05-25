/**
 * @file benchmark.h
 * @brief 性能基准测试框架
 *
 * @details 提供轻量级性能测试框架，支持：
 *   1. 微基准测试（函数级）
 *   2. 宏基准测试（模块级）
 *   3. 自动迭代次数调整
 *   4. 统计分析（均值、标准差、百分位）
 *   5. 结果比较与回归检测
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_BENCHMARK_H
#define LV00_BENCHMARK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============== 配置常量 ============== */

/** 默认最小迭代次数 */
#define LV00_BENCH_MIN_ITERATIONS 10

/** 默认最大迭代次数 */
#define LV00_BENCH_MAX_ITERATIONS 1000000

/** 默认目标运行时间（秒） */
#define LV00_BENCH_TARGET_TIME_SEC 1.0

/** 百分位数量 */
#define LV00_BENCH_PERCENTILE_COUNT 5

/* ============== 基准测试函数类型 ============== */

/**
 * @brief 基准测试函数
 *
 * @param iterations 迭代次数
 * @param user_data 用户数据
 * @return 总耗时（微秒）
 */
typedef uint64_t (*Lv00BenchFunc)(int iterations, void *user_data);

/**
 * @brief 设置函数（可选）
 */
typedef void (*Lv00BenchSetupFunc)(void *user_data);

/**
 * @brief 清理函数（可选）
 */
typedef void (*Lv00BenchTeardownFunc)(void *user_data);

/* ============== 统计结果 ============== */

/**
 * @brief 基准测试结果
 */
typedef struct {
    /* 基本信息 */
    char name[64];              /**< 测试名称 */
    int iterations;             /**< 迭代次数 */

    /* 时间统计 */
    uint64_t total_time_us;     /**< 总耗时（微秒） */
    double mean_us;             /**< 平均耗时 */
    double std_dev_us;          /**< 标准差 */
    double min_us;              /**< 最小耗时 */
    double max_us;              /**< 最大耗时 */

    /* 百分位 */
    double percentiles[LV00_BENCH_PERCENTILE_COUNT]; /**< P50, P75, P90, P95, P99 */

    /* 吞吐量 */
    double ops_per_sec;         /**< 每秒操作数 */

    /* 内存（可选） */
    size_t memory_before;       /**< 测试前内存使用 */
    size_t memory_after;        /**< 测试后内存使用 */
    size_t memory_peak;         /**< 峰值内存使用 */

    /* 状态 */
    bool success;               /**< 是否成功 */
    char error_msg[256];        /**< 错误信息 */
} Lv00BenchResult;

/* ============== 基准测试套件 ============== */

/**
 * @brief 基准测试用例
 */
typedef struct {
    char name[64];                  /**< 用例名称 */
    Lv00BenchFunc func;             /**< 测试函数 */
    Lv00BenchSetupFunc setup;       /**< 设置函数 */
    Lv00BenchTeardownFunc teardown; /**< 清理函数 */
    void *user_data;                /**< 用户数据 */
    int min_iterations;             /**< 最小迭代次数 */
    int max_iterations;             /**< 最大迭代次数 */
    double target_time_sec;         /**< 目标运行时间 */
} Lv00BenchCase;

/**
 * @brief 基准测试套件
 */
typedef struct Lv00BenchSuite Lv00BenchSuite;

/**
 * @brief 创建基准测试套件
 */
Lv00BenchSuite *lv00_bench_suite_create(const char *name);

/**
 * @brief 销毁基准测试套件
 */
void lv00_bench_suite_destroy(Lv00BenchSuite *suite);

/**
 * @brief 添加测试用例
 */
int lv00_bench_suite_add(Lv00BenchSuite *suite, const Lv00BenchCase *case_);

/**
 * @brief 运行所有测试
 */
int lv00_bench_suite_run(Lv00BenchSuite *suite);

/**
 * @brief 获取测试结果数量
 */
int lv00_bench_suite_result_count(const Lv00BenchSuite *suite);

/**
 * @brief 获取测试结果
 */
const Lv00BenchResult *lv00_bench_suite_get_result(const Lv00BenchSuite *suite, int index);

/**
 * @brief 打印测试报告
 */
void lv00_bench_suite_print_report(const Lv00BenchSuite *suite, void *stream);

/**
 * @brief 导出结果为JSON
 */
char *lv00_bench_suite_to_json(const Lv00BenchSuite *suite);

/**
 * @brief 导出结果为Markdown表格
 */
char *lv00_bench_suite_to_markdown(const Lv00BenchSuite *suite);

/* ============== 单次基准测试 ============== */

/**
 * @brief 运行单次基准测试
 *
 * @param func 测试函数
 * @param user_data 用户数据
 * @param min_iterations 最小迭代次数
 * @param target_time_sec 目标运行时间
 * @return 测试结果
 */
Lv00BenchResult lv00_benchmark_run(Lv00BenchFunc func, void *user_data,
                                    int min_iterations, double target_time_sec);

/**
 * @brief 运行带设置/清理的基准测试
 */
Lv00BenchResult lv00_benchmark_run_full(const Lv00BenchCase *case_);

/* ============== 结果比较 ============== */

/**
 * @brief 比较两个结果
 */
typedef struct {
    double mean_ratio;         /**< 均值比率 (new/old) */
    double min_ratio;          /**< 最小值比率 */
    double max_ratio;          /**< 最大值比率 */
    double ops_ratio;          /**< 吞吐量比率 */
    bool is_regression;        /**< 是否为性能回归 */
    double regression_threshold; /**< 回归阈值 */
} Lv00BenchComparison;

/**
 * @brief 比较两个基准测试结果
 */
Lv00BenchComparison lv00_bench_compare(const Lv00BenchResult *baseline,
                                        const Lv00BenchResult *current,
                                        double regression_threshold);

/**
 * @brief 打印比较结果
 */
void lv00_bench_print_comparison(const Lv00BenchComparison *cmp,
                                  const Lv00BenchResult *baseline,
                                  const Lv00BenchResult *current,
                                  void *stream);

/* ============== 内置基准测试 ============== */

/**
 * @brief 运行Lv-00核心模块基准测试
 *
 * 包括：
 *   - 符号坐标创建/销毁
 *   - 约束图操作
 *   - 归一化性能
 *   - 求解器性能
 *   - 内存池性能
 *   - SIMD操作性能
 *
 * @return 基准测试套件
 */
Lv00BenchSuite *lv00_bench_run_core_tests(void);

/**
 * @brief 运行内存基准测试
 */
Lv00BenchSuite *lv00_bench_run_memory_tests(void);

/**
 * @brief 运行SIMD基准测试
 */
Lv00BenchSuite *lv00_bench_run_simd_tests(void);

/**
 * @brief 运行多线程基准测试
 */
Lv00BenchSuite *lv00_bench_run_thread_tests(void);

/* ============== 计时器辅助 ============== */

/**
 * @brief 高精度计时器
 */
typedef struct {
    uint64_t start;    /**< 开始时间 */
    uint64_t end;      /**< 结束时间 */
    bool running;      /**< 是否运行中 */
} Lv00Timer;

/**
 * @brief 创建计时器
 */
Lv00Timer lv00_timer_create(void);

/**
 * @brief 开始计时
 */
void lv00_timer_start(Lv00Timer *timer);

/**
 * @brief 停止计时
 */
void lv00_timer_stop(Lv00Timer *timer);

/**
 * @brief 重置计时器
 */
void lv00_timer_reset(Lv00Timer *timer);

/**
 * @brief 获取经过时间（微秒）
 */
uint64_t lv00_timer_elapsed_us(const Lv00Timer *timer);

/**
 * @brief 获取经过时间（毫秒）
 */
double lv00_timer_elapsed_ms(const Lv00Timer *timer);

/**
 * @brief 获取经过时间（秒）
 */
double lv00_timer_elapsed_sec(const Lv00Timer *timer);

/* ============== 内存统计辅助 ============== */

/**
 * @brief 获取当前内存使用量
 */
size_t lv00_get_memory_usage(void);

/**
 * @brief 获取峰值内存使用量
 */
size_t lv00_get_peak_memory_usage(void);

/**
 * @brief 重置内存统计
 */
void lv00_reset_memory_stats(void);

/* ============== 性能监控 ============== */

/**
 * @brief 性能监控器
 */
typedef struct {
    /* 计数器 */
    uint64_t operations;        /**< 操作次数 */
    uint64_t errors;            /**< 错误次数 */

    /* 时间 */
    uint64_t total_time_us;     /**< 总耗时 */
    uint64_t min_time_us;       /**< 最小耗时 */
    uint64_t max_time_us;       /**< 最大耗时 */

    /* 内存 */
    size_t memory_allocated;    /**< 分配内存 */
    size_t memory_freed;        /**< 释放内存 */
    size_t memory_current;      /**< 当前内存 */
    size_t memory_peak;         /**< 峰值内存 */

    /* 自定义计数器 */
    uint64_t custom_counters[8];
} Lv00PerfMonitor;

/**
 * @brief 创建性能监控器
 */
Lv00PerfMonitor *lv00_perf_monitor_create(void);

/**
 * @brief 销毁性能监控器
 */
void lv00_perf_monitor_destroy(Lv00PerfMonitor *monitor);

/**
 * @brief 记录操作
 */
void lv00_perf_record_op(Lv00PerfMonitor *monitor, uint64_t time_us);

/**
 * @brief 记录错误
 */
void lv00_perf_record_error(Lv00PerfMonitor *monitor);

/**
 * @brief 记录内存分配
 */
void lv00_perf_record_alloc(Lv00PerfMonitor *monitor, size_t size);

/**
 * @brief 记录内存释放
 */
void lv00_perf_record_free(Lv00PerfMonitor *monitor, size_t size);

/**
 * @brief 获取平均操作时间
 */
double lv00_perf_avg_time_us(const Lv00PerfMonitor *monitor);

/**
 * @brief 获取吞吐量
 */
double lv00_perf_throughput(const Lv00PerfMonitor *monitor);

/**
 * @brief 打印监控报告
 */
void lv00_perf_print_report(const Lv00PerfMonitor *monitor, void *stream);

#ifdef __cplusplus
}
#endif

#endif /* LV00_BENCHMARK_H */
