#ifndef lv_LOG_H
#define lv_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 日志级别枚举
 *
 * 【类型协调】lvLogLevel 与 debug.h 的 LogLevel / runtime_monitor.h 的 lvLogLevel
 * 是同一个级别的三个视角，必须互斥定义：
 *   - runtime_monitor.h 已先包含 → 直接复用其 lvLogLevel
 *   - debug.h 已先包含（LogLevel 已定义）→ 复用 LogLevel
 *   - 均未包含 → 定义本头文件的独立枚举
 */
#if defined(lv_RUNTIME_MONITOR_LOGLEVEL_SEEN)
/* lvLogLevel 已由 runtime_monitor.h 定义，直接复用 */
#ifndef lv_LOG_DEBUG
#define lv_LOG_DEBUG LOG_LEVEL_DEBUG
#define lv_LOG_INFO  LOG_LEVEL_INFO
#define lv_LOG_WARN  LOG_LEVEL_WARN
#define lv_LOG_ERROR LOG_LEVEL_ERROR
#define lv_LOG_FATAL LOG_LEVEL_FATAL
#define lv_LOG_NONE  LOG_LEVEL_OFF
#endif
#elif defined(lv_LOGLEVEL_DEFINED)
/* debug.h 已定义 LogLevel，复用其类型 */
typedef LogLevel lvLogLevel;
#ifndef lv_LOG_DEBUG
#define lv_LOG_DEBUG LOG_LEVEL_DEBUG
#define lv_LOG_INFO  LOG_LEVEL_INFO
#define lv_LOG_WARN  LOG_LEVEL_WARN
#define lv_LOG_ERROR LOG_LEVEL_ERROR
#define lv_LOG_FATAL LOG_LEVEL_FATAL
#define lv_LOG_NONE  LOG_LEVEL_NONE
#endif
#else
typedef enum {
    lv_LOG_DEBUG = 0,   /**< 调试信息 */
    lv_LOG_INFO  = 1,   /**< 一般信息 */
    lv_LOG_WARN  = 2,   /**< 警告 */
    lv_LOG_ERROR = 3,   /**< 错误 */
    lv_LOG_FATAL = 4,   /**< 致命错误 */
    lv_LOG_NONE  = 5    /**< 不输出任何日志 */
} lvLogLevel;
#define lv_LOGLEVEL_DEFINED 1
#define lv_LOG_H_LOGLEVEL_DEFINED 1
#endif

/**
 * @brief 输出一条日志
 * @param level  日志级别
 * @param fmt    printf 风格格式字符串
 * @param ...    可变参数
 */
void lv_log(lvLogLevel level, const char *fmt, ...);

/**
 * @brief 获取当前日志级别
 * @return 当前日志级别
 */
lvLogLevel lv_log_get_level(void);

/**
 * @brief 重定向日志输出
 * @param fp  文件指针（NULL = 恢复到默认 stderr）
 */

/**
 * @brief 关闭日志系统
 *
 * @note 此函数由运行时监控日志子系统（runtime_monitor.c）实现，
 *       声明集中放置于本日志头文件，与 lv_log_init() 配对使用。
 */
void lv_log_shutdown(void);
void lv_log_set_output(FILE *fp);

/**
 * @brief 启用/禁用日志时间戳前缀
 * @param enable  true = 开启，false = 关闭（默认 false）
 */
void lv_log_enable_timestamp(bool enable);

/**
 * @brief 启用/禁用日志源位置（file:line function）前缀
 * @param enable  true = 开启，false = 关闭（默认 false）
 */
void lv_log_enable_source(bool enable);

/** @cond 内部宏辅助 */
#if defined(__GNUC__) || defined(__clang__)
#define lv_LOG_PRINTF_ATTR(f, v) __attribute__((format(printf, f, v)))
#else
#define lv_LOG_PRINTF_ATTR(f, v)
#endif
/** @endcond */

/**
 * @name 便捷日志宏
 * @{
 *
 * @note 级别常量使用数值字面量而非枚举成员名：本项目的 lvLogLevel
 *       存在三套互斥定义（本头独立枚举 / runtime_monitor.h / debug.h），
 *       复用分支下 lv_LOG_* 标识符可能不存在（如 runtime_monitor.h 的
 *       lv_LOG_ERROR 是带 tag 的函数式宏）。三套枚举数值一致
 *       （DEBUG=0, INFO=1, WARN=2, ERROR=3, FATAL=4），故此处直接用
 *       数值字面量，免疫命名冲突。
 */
#define lv_DEBUG(fmt, ...)  lv_log((lvLogLevel)0, fmt, ##__VA_ARGS__)
#define lv_INFO(fmt, ...)   lv_log((lvLogLevel)1, fmt, ##__VA_ARGS__)
#define lv_WARN(fmt, ...)   lv_log((lvLogLevel)2, fmt, ##__VA_ARGS__)
#define lv_ERROR(fmt, ...)  lv_log((lvLogLevel)3, fmt, ##__VA_ARGS__)
#define lv_FATAL(fmt, ...)  lv_log((lvLogLevel)4, fmt, ##__VA_ARGS__)
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* lv_LOG_H */
