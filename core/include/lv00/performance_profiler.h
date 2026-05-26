/**
 * @file performance_profiler.h
 * @brief 性能分析器 —— 内存分配热点追踪与性能基准测试
 *
 * @details 提供轻量级性能分析能力：
 * - 内存分配热点追踪：统计各模块的分配次数、大小、峰值
 * - 时间性能分析：函数级耗时测量
 * - 性能基准测试：标准化测试场景，对比优化前后性能
 *
 * 使用示例：
 * ```c
 * // 开始性能分析会话
 * Lv00PerfSession *session = lv00_perf_session_create("solver_test");
 * 
 * // 标记代码区域
 * LV00_PERF_BEGIN(session, "constraint_solve");
 * solve_constraints(graph);
 * LV00_PERF_END(session, "constraint_solve");
 * 
 * // 记录内存分配
 * lv00_perf_record_alloc(session, "GeomNode", sizeof(GeomNode));
 * 
 * // 输出报告
 * lv00_perf_report_print(session, stdout);
 * lv00_perf_session_destroy(session);
 * ```
 *
 * @version 3.5.0
 */

#ifndef LV00_PERFORMANCE_PROFILER_H
#define LV00_PERFORMANCE_PROFILER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * 性能分析会话
 * ================================================================ */

typedef struct Lv00PerfSession Lv00PerfSession;

/**
 * @brief 创建性能分析会话
 * @param name 会话名称（用于标识）
 * @return 新会话，失败返回 NULL
 */
Lv00PerfSession *lv00_perf_session_create(const char *name);

/**
 * @brief 销毁性能分析会话
 * @param session 会话指针
 */
void lv00_perf_session_destroy(Lv00PerfSession *session);

/**
 * @brief 重置会话数据
 * @param session 会话指针
 */
void lv00_perf_session_reset(Lv00PerfSession *session);

/* ================================================================
 * 时间性能测量
 * ================================================================ */

/**
 * @brief 标记代码区域开始
 * @param session 会话
 * @param region_name 区域名称
 */
void lv00_perf_begin(Lv00PerfSession *session, const char *region_name);

/**
 * @brief 标记代码区域结束
 * @param session 会话
 * @param region_name 区域名称（必须与 begin 匹配）
 */
void lv00_perf_end(Lv00PerfSession *session, const char *region_name);

/**
 * @brief 测量单次操作耗时
 * @param session 会话
 * @param operation_name 操作名称
 * @param elapsed_ns 耗时（纳秒）
 */
void lv00_perf_record_time(Lv00PerfSession *session, const char *operation_name, uint64_t elapsed_ns);

/* ================================================================
 * 内存分配追踪
 * ================================================================ */

/**
 * @brief 记录内存分配
 * @param session 会话
 * @param type_name 类型名称
 * @param size 分配大小
 */
void lv00_perf_record_alloc(Lv00PerfSession *session, const char *type_name, size_t size);

/**
 * @brief 记录内存释放
 * @param session 会话
 * @param type_name 类型名称
 * @param size 释放大小
 */
void lv00_perf_record_free(Lv00PerfSession *session, const char *type_name, size_t size);

/* ================================================================
 * 报告输出
 * ================================================================ */

/**
 * @brief 打印性能报告
 * @param session 会话
 * @param output 输出流（如 stdout）
 */
void lv00_perf_report_print(const Lv00PerfSession *session, void *output);

/**
 * @brief 导出为 JSON
 * @param session 会话
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际写入字节数，失败返回负值
 */
int lv00_perf_report_to_json(const Lv00PerfSession *session, char *buffer, size_t buffer_size);

/* ================================================================
 * 便捷宏
 * ================================================================ */

#ifdef LV00_ENABLE_PROFILING

#include <time.h>

#define LV00_PERF_BEGIN(session, name) lv00_perf_begin(session, name)
#define LV00_PERF_END(session, name) lv00_perf_end(session, name)

#define LV00_PERF_SCOPE(session, name) \
    for (struct { int i; uint64_t start; } _perf = {0, lv00_perf_get_time_ns()}; \
         _perf.i < 1; \
         _perf.i++, lv00_perf_record_time(session, name, lv00_perf_get_time_ns() - _perf.start))

#else

#define LV00_PERF_BEGIN(session, name) ((void)0)
#define LV00_PERF_END(session, name) ((void)0)
#define LV00_PERF_SCOPE(session, name) if (0)

#endif

/**
 * @brief 获取当前时间（纳秒）
 * @return 纳秒时间戳
 */
uint64_t lv00_perf_get_time_ns(void);

/* ================================================================
 * 基准测试框架
 * ================================================================ */

/**
 * @brief 基准测试配置
 */
typedef struct {
    int warmup_iterations;      /**< 预热迭代次数 */
    int measurement_iterations; /**< 测量迭代次数 */
    int min_duration_ms;        /**< 最小测量时长（毫秒） */
} Lv00BenchmarkConfig;

/**
 * @brief 基准测试结果
 */
typedef struct {
    double mean_ns;             /**< 平均耗时（纳秒） */
    double stddev_ns;           /**< 标准差（纳秒） */
    double min_ns;              /**< 最小耗时 */
    double max_ns;              /**< 最大耗时 */
    int iterations;             /**< 实际迭代次数 */
} Lv00BenchmarkResult;

/**
 * @brief 获取默认基准测试配置
 * @return 默认配置
 */
const Lv00BenchmarkConfig *lv00_benchmark_default_config(void);

/**
 * @brief 运行基准测试
 * @param name 测试名称
 * @param func 被测函数（无参数无返回值）
 * @param config 配置（NULL 使用默认）
 * @param result 输出结果
 * @return 0 表示成功
 */
int lv00_benchmark_run(const char *name, void (*func)(void), 
                        const Lv00BenchmarkConfig *config,
                        Lv00BenchmarkResult *result);

/**
 * @brief 打印基准测试结果
 * @param name 测试名称
 * @param result 结果
 * @param output 输出流
 */
void lv00_benchmark_print_result(const char *name, const Lv00BenchmarkResult *result, void *output);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PERFORMANCE_PROFILER_H */
