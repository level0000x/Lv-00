#include "lv/lv_log.h"
#include "lv/lv_thread.h"
#include "lv/lv_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 * 内部状态
 * ================================================================ */

/** 当前日志级别，低于此级别的不输出 */
static lvLogLevel g_min_level = lv_LOG_INFO;

/* ------------------------------------------------------------------
 * 输出定制状态（由 lv_log_set_output / lv_log_enable_timestamp /
 * lv_log_enable_source 控制，均受 g_log_state_lock 保护）
 * ------------------------------------------------------------------ */

/** 日志输出目标（NULL = 使用默认 stderr） */
static FILE *g_output_fp = NULL;

/** 是否已显式调用过 lv_log_set_output（区分"默认"与"显式重定向到 stderr"） */
static bool g_output_configured = false;

/** 是否在输出前缀追加时间戳 [HH:MM:SS] */
static bool g_timestamp_enabled = false;

/** 是否在输出前缀追加源文件:行号 */
static bool g_source_enabled = false;

/** 全局状态互斥锁：保护上述配置项的并发读写（惰性初始化，首次加锁时自动完成） */
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

    /* 锁内快照输出定制状态，避免后续读取期间被其它线程修改 */
    const bool customized = g_output_configured || g_timestamp_enabled || g_source_enabled;
    const bool with_ts = g_timestamp_enabled;
    const bool with_src = g_source_enabled;
    FILE *out = g_output_fp != NULL ? g_output_fp : stderr;

    lv_lazy_lock_unlock(&g_log_state_lock);

    /* 格式化消息后委托统一日志主管道（lv_log_message -> debug_log）：
     * 时间戳、级别过滤、文件输出与环形缓冲区等均由主管道统一处理，
     * 本函数保留 g_min_level 过滤语义（与 lv_log_get_level 保持一致）。 */
    char buf[lv_LOG_MSG_MAX_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    /* 未显式定制输出时保持默认行为：委托统一日志主管道 */
    if (!customized) {
        lv_log_message(g_lvlog_level_map[level], __FILE__, __LINE__, "%s", buf);
        return;
    }

    /* 显式定制路径：按开关构造前缀（[HH:MM:SS] 与 源文件:行号），
     * 一次性 fprintf 输出（stdio 对同一 FILE 的整次调用原子化，多线程安全） */
    char prefix[lv_LOG_MSG_MAX_LEN];
    prefix[0] = '\0';
    size_t pos = 0;
    if (with_ts) {
        time_t now = time(NULL);
        struct tm tmv;
        lv_LOCALTIME(&now, &tmv);
        strftime(prefix, sizeof(prefix), "[%H:%M:%S] ", &tmv);
        pos = strlen(prefix);
    }
    if (with_src) {
        lv_snprintf(prefix + pos, sizeof(prefix) - pos, "%s:%d ", __FILE__, __LINE__);
    }
    fprintf(out, "%s%s\n", prefix, buf);
    fflush(out);
}

lvLogLevel lv_log_get_level(void) {
    lvLogLevel level;
    lv_lazy_lock_lock(&g_log_state_lock, g_log_state_lock_init_once);
    level = g_min_level;
    lv_lazy_lock_unlock(&g_log_state_lock);
    return level;
}

void lv_log_set_output(FILE *fp) {
    /* 替换/关闭日志输出目标：非 NULL 时 lv_log 直接输出到 fp（显式定制路径）；
     * NULL 时恢复默认（g_output_configured 复位——重新委托统一日志主管道
     * lv_log_message，而非停留于"显式输出到 stderr"的定制路径）。 */
    lv_lazy_lock_lock(&g_log_state_lock, g_log_state_lock_init_once);
    g_output_fp = fp;
    g_output_configured = (fp != NULL);
    lv_lazy_lock_unlock(&g_log_state_lock);
}

void lv_log_enable_timestamp(bool enable) {
    /* 开启后在输出前缀追加 [HH:MM:SS] 时间戳 */
    lv_lazy_lock_lock(&g_log_state_lock, g_log_state_lock_init_once);
    g_timestamp_enabled = enable;
    lv_lazy_lock_unlock(&g_log_state_lock);
}

void lv_log_enable_source(bool enable) {
    /* 开启后在输出前缀追加 源文件:行号（__FILE__/__LINE__ 在记录点展开） */
    lv_lazy_lock_lock(&g_log_state_lock, g_log_state_lock_init_once);
    g_source_enabled = enable;
    lv_lazy_lock_unlock(&g_log_state_lock);
}