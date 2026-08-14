#ifndef lv_LOG_H
#define lv_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 日志级别枚举（全项目唯一权威定义）
 *
 * 【单源声明】lvLogLevel 仅在本头文件定义，是全项目日志级别的唯一来源：
 *   - runtime_monitor.h 通过 #include 本头文件复用，不再各自定义
 *     lvLogLevel（消除"同概念双定义"冲突；本头文件的 lv_LOG_* 枚举成员
 *     与 runtime_monitor.h 的带 tag 函数式宏 lv_LOG_TRACE/lv_LOG_FATAL
 *     同名共存：函数式宏仅在有参数括号时展开，枚举成员照常可用）；
 *   - debug.h 的 LogLevel 为独立类型名（枚举成员 LOG_LEVEL_* 与本头
 *     lv_LOG_* 不同名），两枚举数值一致（DEBUG=0, INFO=1, WARN=2,
 *     ERROR=3, FATAL=4, NONE=5），可在同一编译单元共存。
 */
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
 * @note 级别常量使用数值字面量而非枚举成员名：lv_LOG_DEBUG/lv_LOG_INFO/
 *       lv_LOG_WARN/lv_LOG_ERROR/lv_LOG_FATAL 等标识符在本项目中被
 *       lv_internal.h（函数式日志宏）与 runtime_monitor.h（带 tag 的
 *       lv_LOG_TRACE/lv_LOG_FATAL）用作宏名，为免疫标识符解析歧义，
 *       此处直接使用与 lvLogLevel 枚举一致的数值字面量
 *       （DEBUG=0, INFO=1, WARN=2, ERROR=3, FATAL=4）。
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
