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
 */
typedef enum {
    lv_LOG_DEBUG = 0,   /**< 调试信息 */
    lv_LOG_INFO  = 1,   /**< 一般信息 */
    lv_LOG_WARN  = 2,   /**< 警告 */
    lv_LOG_ERROR = 3,   /**< 错误 */
    lv_LOG_FATAL = 4,   /**< 致命错误 */
    lv_LOG_NONE  = 5    /**< 不输出任何日志 */
} lvLogLevel;

/**
 * @brief 输出一条日志
 * @param level  日志级别
 * @param fmt    printf 风格格式字符串
 * @param ...    可变参数
 */
void lv_log(lvLogLevel level, const char *fmt, ...);

/**
 * @brief 设置日志输出级别（低于此级别的日志将被过滤）
 * @param level  最低输出级别，默认 lv_LOG_INFO
 */
void lv_log_set_level(lvLogLevel level);

/**
 * @brief 获取当前日志级别
 * @return 当前日志级别
 */
lvLogLevel lv_log_get_level(void);

/**
 * @brief 重定向日志输出
 * @param fp  文件指针（NULL = 恢复到默认 stderr）
 */
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
 */
#define lv_DEBUG(fmt, ...)  lv_log(lv_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define lv_INFO(fmt, ...)   lv_log(lv_LOG_INFO,  fmt, ##__VA_ARGS__)
#define lv_WARN(fmt, ...)   lv_log(lv_LOG_WARN,  fmt, ##__VA_ARGS__)
#define lv_ERROR(fmt, ...)  lv_log(lv_LOG_ERROR, fmt, ##__VA_ARGS__)
#define lv_FATAL(fmt, ...)  lv_log(lv_LOG_FATAL, fmt, ##__VA_ARGS__)
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* lv_LOG_H */
