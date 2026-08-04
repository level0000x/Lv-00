#include "lv/lv_log.h"
#include "lv/lv_thread.h"

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

/** 全局状态互斥锁：保护上述 4 个配置项的并发读写（惰性初始化） */
static lv_mutex_t g_log_state_mutex;
static lv_once_t g_log_state_once;

/** 惰性初始化日志状态互斥锁（lv_once 保证只执行一次） */
static void log_state_mutex_init_func(void) {
    lv_mutex_init(&g_log_state_mutex);
}

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
    if (level > lv_LOG_FATAL) {
        return;
    }

    lv_once(&g_log_state_once, log_state_mutex_init_func);
    lv_mutex_lock(&g_log_state_mutex);

    if (level < g_min_level) {
        lv_mutex_unlock(&g_log_state_mutex);
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

    lv_mutex_unlock(&g_log_state_mutex);
}

lvLogLevel lv_log_get_level(void) {
    lvLogLevel level;
    lv_once(&g_log_state_once, log_state_mutex_init_func);
    lv_mutex_lock(&g_log_state_mutex);
    level = g_min_level;
    lv_mutex_unlock(&g_log_state_mutex);
    return level;
}

void lv_log_set_output(FILE *fp) {
    lv_once(&g_log_state_once, log_state_mutex_init_func);
    lv_mutex_lock(&g_log_state_mutex);
    g_output = fp;
    lv_mutex_unlock(&g_log_state_mutex);
}

void lv_log_enable_timestamp(bool enable) {
    lv_once(&g_log_state_once, log_state_mutex_init_func);
    lv_mutex_lock(&g_log_state_mutex);
    g_timestamp_enabled = enable;
    lv_mutex_unlock(&g_log_state_mutex);
}

void lv_log_enable_source(bool enable) {
    lv_once(&g_log_state_once, log_state_mutex_init_func);
    lv_mutex_lock(&g_log_state_mutex);
    g_source_enabled = enable;
    lv_mutex_unlock(&g_log_state_mutex);
}