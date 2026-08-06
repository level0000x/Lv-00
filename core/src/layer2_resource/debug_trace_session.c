/**
 * @file debug_trace_session.c
 * @brief trace session and log pipeline
 * @details Split from debug.c. Contains the unified log pipeline
 *          (debug_log and friends) plus trace session helpers.
 */

#include "lv/lv_file.h"
#include "lv/lv_path.h"
#include "lv/lv_platform.h"
#include "lv/lv_thread.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "lv/engine.h"
#include "lv/lv_json.h"

#include "context.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"
#include "debug_internal.h"

/* ============== 遗留日志函数（向后兼容） ============== */

/**
 * @brief 遗留日志实现：直接输出到控制台和日志文件
 *
 * 不经过级别过滤，始终以 DEBUG 语义输出。
 * 由 debug_log_normalization / debug_log_rewrite / debug_log_solver 调用。
 */
static void debug_log_legacy_impl(const char *subsystem, const char *fmt, va_list args) {
    log_lock();

    /* 输出到控制台 */
    va_list args_copy;
    va_copy(args_copy, args);
    vprintf(fmt, args_copy);
    va_end(args_copy);
    printf("\n");

    /* 同时输出到日志文件（如果已初始化） */
    if (s_debug_state.log_file && s_debug_state.initialized) {
        char timestamp[lv_DEBUG_TIMESTAMP_BUF_SIZE];
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(s_debug_state.log_file, "[%s] [DEBUG] [%s] ", timestamp, subsystem);
        va_copy(args_copy, args);
        vfprintf(s_debug_state.log_file, fmt, args_copy);
        va_end(args_copy);
        fprintf(s_debug_state.log_file, "\n");
        fflush(s_debug_state.log_file);
    }

    log_unlock();
}

void debug_log_normalization(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    debug_log_legacy_impl("normalization", fmt, args);
    va_end(args);
}

void debug_log_rewrite(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    debug_log_legacy_impl("rewrite", fmt, args);
    va_end(args);
}

void debug_log_solver(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    debug_log_legacy_impl("solver", fmt, args);
    va_end(args);
}

/*=== 新日志系统实现 ===*/

int debug_log_init(void) {
    /* log_lock() 内部通过 lv_once 保证互斥锁只初始化一次 */
    log_lock();
    if (s_debug_state.initialized) {
        log_unlock();
        return 0; /* 已初始化 */
    }

    /* 构建日志目录路径: ~/.lv/logs */
    const char *home = get_home_dir();
    char home_lv[lv_LOG_PATH_MAX];
    lv_path_join(home, ".lv", home_lv, sizeof(home_lv));
    lv_path_join(home_lv, "logs", s_debug_state.log_dir_path, lv_LOG_PATH_MAX);

    /* 创建日志目录 */
    if (create_directory(s_debug_state.log_dir_path) != 0) {
        lv_set_error(lv_ERROR_IO, "[DEBUG] Warning: Could not create log directory: %s", s_debug_state.log_dir_path);
        /* 继续运行，不使用文件日志 */
    }

    /* 构建日志文件路径 */
    lv_path_join(s_debug_state.log_dir_path, lv_DEBUG_LOG_BASENAME, s_debug_state.log_file_path, lv_LOG_PATH_MAX);

    /* 打开日志文件 */
    s_debug_state.log_file = fopen(s_debug_state.log_file_path, "a");
    if (!s_debug_state.log_file) {
        lv_set_error(lv_ERROR_IO, "[DEBUG] Warning: Could not open log file: %s", s_debug_state.log_file_path);
        /* 继续运行，不使用文件日志 */
    } else {
        /* 获取当前文件大小 */
        fseek(s_debug_state.log_file, 0, SEEK_END);
        long pos = ftell(s_debug_state.log_file);
        s_debug_state.current_log_size = (pos > 0) ? (size_t) pos : 0;
    }

    s_debug_state.initialized = true;
    log_unlock();

    /* 【v3.3.0】创建全局环形日志缓冲区 */
    if (!s_debug_state.log_ring_buffer) {
        s_debug_state.log_ring_buffer = lv_log_ring_buffer_create(s_debug_state.log_ring_buffer_capacity);
        if (s_debug_state.log_ring_buffer) {
            lv_log_ring_buffer_write(s_debug_state.log_ring_buffer, LOG_LEVEL_INFO, "debug", "debug_log_init",
                                     __FILE__, __LINE__, "环形日志缓冲区已创建（容量: %d）",
                                     s_debug_state.log_ring_buffer_capacity);
        }
    }

    /* 记录初始化日志 */
    LOG_INFO("debug", "=== Lv-00 v%s Logging System Initialized ===", lv_VERSION_STRING);

    return 0;
}

void debug_log_shutdown(void) {
    /* 必须在锁内检查 initialized，防止与 debug_log_init() 产生竞态条件 */
    log_lock();
    if (!s_debug_state.initialized) {
        log_unlock();
        return;
    }

    /* 记录关闭日志 */
    if (s_debug_state.log_file) {
        char timestamp[lv_DEBUG_TIMESTAMP_BUF_SIZE];
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(s_debug_state.log_file, "[%s] [INFO] [debug] === Logging System Shutdown ===\n", timestamp);
        fclose(s_debug_state.log_file);
        s_debug_state.log_file = NULL;
    }

    /* 先将 initialized 设为 false，阻止新日志进入 */
    s_debug_state.initialized = false;

    /* 销毁全局追踪会话，防止内存泄漏 */
    if (s_debug_state.trace_session) {
        trace_session_destroy(s_debug_state.trace_session);
        s_debug_state.trace_session = NULL;
    }

    log_unlock();

    /* 【v3.3.0】销毁全局环形日志缓冲区 */
    if (s_debug_state.log_ring_buffer) {
        lv_log_ring_buffer_destroy(s_debug_state.log_ring_buffer);
        s_debug_state.log_ring_buffer = NULL;
    }
}

void debug_log_cleanup(void) {
    /* 委托给 debug_log_shutdown 处理实际清理 */
    debug_log_shutdown();
}

void debug_set_log_level(LogLevel level) {
    log_lock();
    g_log_level = level;
    log_unlock();
}

LogLevel debug_get_log_level(void) {
    log_lock();
    LogLevel level = g_log_level;
    log_unlock();
    return level;
}

void debug_set_mode(bool debug_mode) {
    log_lock();
    g_debug_mode = debug_mode;
    if (debug_mode) {
        g_log_level = LOG_LEVEL_DEBUG;
    } else {
        g_log_level = LOG_LEVEL_WARN;
    }
    log_unlock();
}

bool debug_is_debug_mode(void) {
    log_lock();
    bool mode = g_debug_mode;
    log_unlock();
    return mode;
}

void debug_log(LogLevel level, const char *module, const char *fmt, ...) {
    log_lock();

    /* 检查该日志级别是否需要记录（在锁内检查，防止与 debug_set_log_level 并发修改竞争） */
    if (level < g_log_level) {
        log_unlock();
        return;
    }

    /* 检查是否需要轮转 */
    check_rotation();

    /* 格式化消息 */
    char timestamp[lv_DEBUG_TIMESTAMP_BUF_SIZE];
    get_timestamp(timestamp, sizeof(timestamp));

    /* 格式化可变参数 */
    char message[lv_DEBUG_LOG_MESSAGE_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    /* 构建日志行 */
    char log_line[lv_DEBUG_LOG_LINE_BUF_SIZE];
    int len = snprintf(log_line, sizeof(log_line), "[%s] [%s] [%s] %s\n", timestamp, log_level_string(level),
                       module ? module : "unknown", message);

    /* ERROR 和 WARN 输出到 stderr，其余输出到 stdout */
    if (level >= LOG_LEVEL_WARN) {
        fputs(log_line, stderr);
    } else {
        fputs(log_line, stdout);
    }

    /* 写入日志文件 */
    if (s_debug_state.log_file && s_debug_state.initialized) {
        fputs(log_line, s_debug_state.log_file);
        fflush(s_debug_state.log_file);
        s_debug_state.current_log_size += (size_t) len;
    }

    /* 追加到紧急保存日志缓冲区 */
    log_buffer_append(log_line);

    log_unlock();

    /* 【v3.3.0】FATAL 级别额外处理：触发紧急保存 */
    if (level == LOG_LEVEL_FATAL) {
        /* FATAL 不在此处加锁/解锁，因为 emergency_save 会在内部自行加锁 */
        EmergencySaveConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.filepath = "lv_emergency_save.log"; /* 使用默认路径 */
        cfg.include_graph = true;               /* 包含约束图快照 */
        cfg.include_counters = true;            /* 包含性能计数器 */
        cfg.include_log_buffer = true;          /* 包含日志缓冲区 */
        cfg.include_memory_map = false;
        debug_emergency_save(NULL, &cfg);
    }
}
