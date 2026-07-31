#include "lv/lv_log.h"

#include <stdarg.h>
#include <time.h>

/* ================================================================
 * 内部状态
 * ================================================================ */

/** 当前日志级别，低于此级别的不输出 */
static lvLogLevel g_min_level = lv_LOG_INFO;

/** 日志输出目标，默认 stderr */
static FILE *g_output = NULL;

/** 是否输出时间戳 */
static bool g_timestamp_enabled = false;

/** 是否输出源位置（file:line function） */
static bool g_source_enabled = false;

/* ================================================================
 * 级别名称表
 * ================================================================ */

static const char *s_level_names[] = {
    [lv_LOG_DEBUG] = "DEBUG",
    [lv_LOG_INFO]  = "INFO",
    [lv_LOG_WARN]  = "WARN",
    [lv_LOG_ERROR] = "ERROR",
    [lv_LOG_FATAL] = "FATAL"
};

/* ================================================================
 * 公共 API 实现
 * ================================================================ */

void lv_log(lvLogLevel level, const char *fmt, ...) {
    if (level < g_min_level || level > lv_LOG_FATAL) {
        return;
    }

    FILE *fp = g_output ? g_output : stderr;

    /* 级别前缀 */
    fprintf(fp, "[%s] ", s_level_names[level]);

    /* 时间戳 */
    if (g_timestamp_enabled) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        if (tm_info) {
            char tbuf[32];
            strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm_info);
            fprintf(fp, "%s ", tbuf);
        }
    }

    /* 格式化消息体 */
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);

    fprintf(fp, "\n");
    fflush(fp);
}

lvLogLevel lv_log_get_level(void) {
    return g_min_level;
}

void lv_log_set_output(FILE *fp) {
    g_output = fp;
}

void lv_log_enable_timestamp(bool enable) {
    g_timestamp_enabled = enable;
}

void lv_log_enable_source(bool enable) {
    g_source_enabled = enable;
}
