#include "lv/lv_log.h"
#include "lv/lv_thread.h"
#include "lv/lv_internal.h"

#include <stdarg.h>
#include <time.h>

/* ================================================================
 * 内部状态
 * ================================================================ */

/** 当前日志级别，低于此级别的不输出 */
static lvLogLevel g_min_level = lv_LOG_INFO;

/** 日志输出目标，默认 stderr（重定向到主管道后不再实际生效，保留兼容） */
static FILE *g_output = NULL;

/** 是否输出时间戳（重定向到主管道后不再实际生效，保留兼容） */
static bool g_timestamp_enabled = false;

/** 是否输出源位置（重定向到主管道后不再实际生效，保留兼容） */
static bool g_source_enabled = false;

/** 全局状态互斥锁：保护上述 4 个配置项的并发读写（惰性初始化，首次加锁时自动完成） */
lv_LAZY_LOCK_DEFINE(g_log_state_lock);

/**
 * @brief lvLogLevel(0=DEBUG..4=FATAL) -> lv_LOG_LEVEL_* 映射表
 *
 * 主管道 lv_log_message() 采用 lv_internal.h 的级别约定（数值越大越详细：
 * lv_LOG_LEVEL_ERROR=1, WARNING=2, INFO=3, DEBUG=4），与 lv_log.h 的
 * lvLogLevel（数值越大越严重）语义相反，故此处建立显式映射。
 */
static const int g_lvlog_level_map[] = {
    lv_LOG_LEVEL_DEBUG,   /* lv_LOG_DEBUG(0) */
    lv_LOG_LEVEL_INFO,    /* lv_LOG_INFO(1)  */
    lv_LOG_LEVEL_WARNING, /* lv_LOG_WARN(2)  */
    lv_LOG_LEVEL_ERROR,   /* lv_LOG_ERROR(3) */
    lv_LOG_LEVEL_ERROR,   /* lv_LOG_FATAL(4) */
};

/* ================================================================
 * 公共 API 实现
 * ================================================================ */

void lv_log(lvLogLevel level, const char *fmt, ...) {
    if (level > lv_LOG_FATAL) {
        return;
    }

    lv_lazy_lock_lock(&g_log_state_lock, g_log_state_lock_init_once);

    if (level < g_min_level) {
        lv_lazy_lock_unlock(&g_log_state_lock);
        return;
    }

    lv_lazy_lock_unlock(&g_log_state_lock);

    /* 格式化消息后委托统一日志主管道（lv_log_message -> debug_log）：
     * 时间戳、级别过滤、文件输出与环形缓冲区等均由主管道统一处理，
     * 本函数保留 g_min_level 过滤语义（与 lv_log_get_level 保持一致）。 */
    char buf[lv_LOG_MSG_MAX_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    lv_log_message(g_lvlog_level_map[level], __FILE__, __LINE__, "%s", buf);
}

lvLogLevel lv_log_get_level(void) {
    lvLogLevel level;
    lv_lazy_lock_lock(&g_log_state_lock, g_log_state_lock_init_once);
    level = g_min_level;
    lv_lazy_lock_unlock(&g_log_state_lock);
    return level;
}

void lv_log_set_output(FILE *fp) {
    lv_lazy_lock_lock(&g_log_state_lock, g_log_state_lock_init_once);
    g_output = fp;
    lv_lazy_lock_unlock(&g_log_state_lock);
}

void lv_log_enable_timestamp(bool enable) {
    lv_lazy_lock_lock(&g_log_state_lock, g_log_state_lock_init_once);
    g_timestamp_enabled = enable;
    lv_lazy_lock_unlock(&g_log_state_lock);
}

void lv_log_enable_source(bool enable) {
    lv_lazy_lock_lock(&g_log_state_lock, g_log_state_lock_init_once);
    g_source_enabled = enable;
    lv_lazy_lock_unlock(&g_log_state_lock);
}