/**
 * @file lv00_internal.h
 * @brief Lv-00 项目内部头文件 —— 内部工具宏与常量
 *
 * @details 提供项目内部使用的通用工具宏、常量和辅助函数声明，
 * 仅供 .c 源文件内部使用，不作为公共 API 暴露。
 *
 * 包含内容：
 * - LV00_ARRAY_GROWTH_FACTOR：动态数组增长因子
 * - LV00_SOLVER_SCALE_FACTOR：SAT 赋值到坐标的缩放因子
 * - LV00_CHECK_NULL / LV00_CHECK_NULL_VOID / LV00_CHECK_ALLOC：空指针和分配检查宏
 * - LV00_UNUSED：抑制未使用变量警告
 * - lv00_ensure_capacity：动态数组容量确保函数
 * - lv00_set_error_ctx：带上下文的错误设置函数
 * - LV00_ERROR_*：错误码（通过包含 error_codes.h 获取）
 *
 * @note 本头文件会自动包含 error_codes.h 和 lv00_utils.h，
 *       因此使用本头文件的 .c 文件无需再单独包含它们。
 *
 * @version 3.5.0
 */

#ifndef LV00_INTERNAL_H
#define LV00_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/*
 * 错误码定义和错误处理宏（LV00_CHECK_NULL, LV00_CHECK_NULL_VOID,
 * LV00_CHECK_ALLOC, lv00_set_error_ctx, LV00_ERROR_* 等）
 */
#include "config.h"
#include "error_codes.h"

/*
 * 工具函数（lv00_malloc, lv00_free, lv00_realloc, lv00_calloc,
 * lv00_strdup, lv00_ensure_capacity 等）
 */
#include "lv00_utils.h"

/*
 * LV00_UNUSED 宏（抑制未使用变量/参数的编译器警告）
 * 定义在 cross_platform.h 中
 */
#include "cross_platform.h"

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ================================================================
 * 内部常量
 * ================================================================ */

/**
 * @brief 动态数组增长因子
 *
 * 当数组需要扩容时，新容量 = 当前容量 * LV00_ARRAY_GROWTH_FACTOR。
 * 值为 2（倍增策略），兼顾内存使用和性能。
 */
#ifndef LV00_ARRAY_GROWTH_FACTOR
#define LV00_ARRAY_GROWTH_FACTOR 2
#endif

/**
 * @brief SAT 赋值到坐标的缩放因子
 *
 * 将 SAT 求解器的布尔赋值映射为有理数坐标时的缩放系数。
 * 例如：赋值为真 -> +SCALE_FACTOR，赋值为假 -> -SCALE_FACTOR。
 */
#ifndef LV00_SOLVER_SCALE_FACTOR
#define LV00_SOLVER_SCALE_FACTOR 1000
#endif

/* ================================================================
 * 日志级别与日志宏
 * ================================================================ */

#ifndef LV00_LOG_LEVEL_OFF
#define LV00_LOG_LEVEL_OFF     0
#define LV00_LOG_LEVEL_ERROR   1
#define LV00_LOG_LEVEL_WARNING 2
#define LV00_LOG_LEVEL_INFO    3
#define LV00_LOG_LEVEL_DEBUG   4
#endif

/**
 * @brief 日志消息分发函数（需在使用日志宏的 .c 文件中提供实现）
 *
 * @param level   日志级别（LV00_LOG_LEVEL_*）
 * @param file    源文件名（由宏自动传入 __FILE__）
 * @param line    行号（由宏自动传入 __LINE__）
 * @param fmt     格式化字符串
 * @param ...     可变参数
 */
extern void lv00_log_message(int level, const char *file, int line,
                             const char *fmt, ...);

#define LV00_LOG_WARNING(fmt, ...) \
    lv00_log_message(LV00_LOG_LEVEL_WARNING, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define LV00_LOG_ERROR(fmt, ...) \
    lv00_log_message(LV00_LOG_LEVEL_ERROR, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define LV00_LOG_INFO(fmt, ...) \
    lv00_log_message(LV00_LOG_LEVEL_INFO, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define LV00_LOG_DEBUG(fmt, ...) \
    lv00_log_message(LV00_LOG_LEVEL_DEBUG, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)

/* ================================================================
 * 安全字符串格式化宏
 * ================================================================ */

/**
 * @brief 安全 snprintf 包装宏
 *
 * 将 snprintf 的返回值规范化为实际写入的字符数（不含终止符），
 * 即使输出被截断也不会返回负值或超过缓冲区大小的值。
 *
 * @param retvar  接收规范化返回值的 int 变量名
 * @param buf     目标缓冲区
 * @param size    缓冲区大小
 * @param fmt     格式化字符串
 * @param ...     可变参数
 */
#define LV00_SAFE_SNPRINTF(retvar, buf, size, fmt, ...) \
    do { \
        int _sn_rc = snprintf((buf), (size), (fmt), ##__VA_ARGS__); \
        (retvar) = ((_sn_rc) < 0) ? 0 : (((size_t)(_sn_rc) >= (size)) ? (int)((size) - 1) : (_sn_rc)); \
    } while (0)

/* ================================================================
 * 流上下文声明宏
 * ================================================================ */

/**
 * @brief 声明模块级线程局部流上下文变量
 *
 * 在每个使用流式输出的模块 .c 文件中使用，声明一个线程局部的
 * StreamContext 指针，供 stream_emit_simple 等函数使用。
 *
 * @param module  模块名称（用于构造变量名）
 */
#define LV00_DECLARE_STREAM_CTX(module) \
    static LV00_THREAD_LOCAL StreamContext *module##_stream_ctx = NULL

/* ================================================================
 * 错误返回宏
 * ================================================================ */

/**
 * @brief 设置错误上下文并返回的复合宏
 *
 * 将 lv00_set_error_ctx 和 return 合并为一步，减少遗漏 return 的风险。
 * 适用于函数错误路径中的快速退出。
 *
 * @param err_code  错误码（LV00_ERROR_*）
 * @param ret_val   返回值
 * @param fmt       格式化错误消息
 * @param ...       可变参数
 */
#define LV00_ERROR_RETURN(err_code, ret_val, fmt, ...) \
    do { \
        lv00_set_error_ctx((err_code), __FILE__, __LINE__, __func__, (fmt), ##__VA_ARGS__); \
        return (ret_val); \
    } while (0)

/* ================================================================
 * 安全整数加法宏
 * ================================================================ */

/**
 * @brief 带溢出检测的整数加法
 *
 * 执行有符号 int 加法，若发生溢出则返回 overflow_val。
 *
 * @param a             第一个操作数
 * @param b             第二个操作数
 * @param overflow_val  溢出时的返回值
 */
#define LV00_SAFE_ADD(a, b, overflow_val) \
    (((a) >= 0 && (b) > 0 && (a) > INT_MAX - (b)) ? (overflow_val) : \
     ((a) < 0 && (b) < 0 && (a) < INT_MIN - (b)) ? (overflow_val) : \
     (int)((a) + (b)))

#ifdef __cplusplus
}
#endif

#endif /* LV00_INTERNAL_H */
