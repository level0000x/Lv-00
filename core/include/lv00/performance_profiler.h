/**
 * @file performance_profiler.h
 * @brief 性能分析器 —— 基准测试与会话级性能追踪
 *
 * @details 提供两大功能：
 *   1. 基准测试（lv00_perf_benchmark_run）：对函数进行高精度计时，
 *      自动校准迭代次数，输出均值/最小值/最大值/标准差。
 *   2. 性能会话（Lv00PerfSession）：命名区域计时 + 内存分配追踪，
 *      支持文本报告和 JSON 导出。
 *
 * 使用示例：
 *   // 基准测试
 *   Lv00PerfBenchResult result;
 *   lv00_perf_benchmark_run("my_func", my_func, NULL, &result);
 *   lv00_perf_benchmark_print_result("my_func", &result, stdout);
 *
 *   // 性能会话
 *   Lv00PerfSession *s = lv00_perf_session_create("my_session");
 *   lv00_perf_begin(s, "region_a");
 *   // ... do work ...
 *   lv00_perf_end(s, "region_a");
 *   lv00_perf_session_record_alloc(s, "MyType", 1024);
 *   lv00_perf_report_print(s, stdout);
 *   lv00_perf_session_destroy(s);
 *
 * @version 1.0.0
 */

#ifndef LV00_PERFORMANCE_PROFILER_H
#define LV00_PERFORMANCE_PROFILER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdio.h>

/* ================================================================
 * 基准测试结果
 * ================================================================ */

/**
 * @brief 基准测试结果结构体
 */
typedef struct {
    const char *name;        /**< 基准测试名称 */
    int         iterations;  /**< 执行的迭代次数 */
    double      mean_ns;     /**< 平均耗时（纳秒） */
    double      min_ns;      /**< 最小耗时（纳秒） */
    double      max_ns;      /**< 最大耗时（纳秒） */
    double      stddev_ns;   /**< 标准差（纳秒） */
} Lv00PerfBenchResult;

/* ================================================================
 * 性能会话（不透明类型）
 * ================================================================ */

/**
 * @brief 性能会话 —— 不透明类型
 *
 * 管理一个性能追踪会话的完整生命周期：
 * - 命名区域计时（begin/end）
 * - 内存分配/释放记录
 * - 报告输出（文本 / JSON）
 */
typedef struct Lv00PerfSession Lv00PerfSession;

/* ================================================================
 * 基准测试 API
 * ================================================================ */

/**
 * @brief 运行函数基准测试
 *
 * 先进行 10 次预热，然后校准确定约 1 秒的迭代次数，
 * 最后执行计时并统计均值、最小值、最大值和标准差。
 *
 * @param name     基准测试名称
 * @param fn       待测试的函数指针
 * @param setup_fn 预留的 setup 函数（当前未使用，传 NULL）
 * @param result   输出结果结构体
 * @return 0 表示成功，-1 表示参数错误（fn 或 result 为 NULL）
 */
int lv00_perf_benchmark_run(const char *name, void (*fn)(void),
                            void *setup_fn, Lv00PerfBenchResult *result);

/**
 * @brief 打印基准测试结果
 *
 * 格式化输出到指定流，格式为：
 *   [name] iterations 次, mean=X ns, min=X ns, max=X ns, stddev=X ns
 *
 * @param name   基准测试名称
 * @param result 基准测试结果
 * @param out    输出流（如 stdout, stderr）
 */
void lv00_perf_benchmark_print_result(const char *name,
                                       const Lv00PerfBenchResult *result,
                                       FILE *out);

/* ================================================================
 * 性能会话 API
 * ================================================================ */

/**
 * @brief 创建性能会话
 *
 * 分配并初始化一个新的性能会话，记录会话开始时间。
 *
 * @param name 会话名称
 * @return 新会话指针，失败返回 NULL
 */
Lv00PerfSession *lv00_perf_session_create(const char *name);

/**
 * @brief 开始计时一个命名区域
 *
 * 若区域不存在则自动创建。若已处于活跃状态则覆盖开始时间。
 *
 * @param session     性能会话
 * @param region_name 区域名称
 */
void lv00_perf_begin(Lv00PerfSession *session, const char *region_name);

/**
 * @brief 结束计时一个命名区域
 *
 * 记录从上次 begin 到现在的耗时，累加到区域统计中。
 * 若区域未处于活跃状态则忽略。
 *
 * @param session     性能会话
 * @param region_name 区域名称
 */
void lv00_perf_end(Lv00PerfSession *session, const char *region_name);

/**
 * @brief 记录一次内存分配事件
 *
 * @param session   性能会话
 * @param type_name 分配的类型名称
 * @param bytes     分配的字节数
 */
void lv00_perf_session_record_alloc(Lv00PerfSession *session,
                                     const char *type_name, size_t bytes);

/**
 * @brief 记录一次内存释放事件
 *
 * @param session   性能会话
 * @param type_name 释放的类型名称
 * @param bytes     释放的字节数
 */
void lv00_perf_session_record_free(Lv00PerfSession *session,
                                    const char *type_name, size_t bytes);

/**
 * @brief 打印性能分析报告
 *
 * 输出所有计时区域（名称、次数、总耗时、平均耗时、最小/最大耗时）
 * 和内存统计（类型名、分配总量、释放总量、净分配量）。
 *
 * @param session 性能会话
 * @param out     输出流
 */
void lv00_perf_report_print(const Lv00PerfSession *session, FILE *out);

/**
 * @brief 将会话数据导出为 JSON 字符串
 *
 * JSON 格式：
 *   {"name":"...","regions":[{"name":"...","count":N,"total_ns":N}],
 *    "memory":[{"type":"...","alloc":N,"free":N,"net":N}]}
 *
 * @param session     性能会话
 * @param buffer      输出缓冲区
 * @param buffer_size 缓冲区大小（字节）
 * @return 写入的字节数（不含末尾 '\0'），失败返回 -1
 */
int lv00_perf_report_to_json(const Lv00PerfSession *session,
                              char *buffer, size_t buffer_size);

/**
 * @brief 重置性能会话
 *
 * 清空所有计时区域和内存统计数据，重新记录会话开始时间。
 *
 * @param session 性能会话
 */
void lv00_perf_session_reset(Lv00PerfSession *session);

/**
 * @brief 销毁性能会话并释放所有资源
 *
 * NULL 安全 —— 传入 NULL 时无操作。
 *
 * @param session 性能会话
 */
void lv00_perf_session_destroy(Lv00PerfSession *session);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PERFORMANCE_PROFILER_H */
