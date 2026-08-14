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

#include "lv/context.h"
#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"
#include "lv/type_system.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"
#include "debug_internal.h"

/* ============== 遗留日志函数（向后兼容） ============== */

/**
 * @brief 遗留日志实现：直接输出到控制台和日志文件
 *
 * 不经过级别过滤，始终以 DEBUG 语义输出。
 * 由 debug_log_rewrite 调用。
 */
static void debug_log_legacy_impl(const char *subsystem, const char *fmt, va_list args) {
    /* 作用域锁守卫：离开函数（含所有 return 分支）自动解锁 */
    DEBUG_LOG_LOCK_GUARD();

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
}

void debug_log_rewrite(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    debug_log_legacy_impl("rewrite", fmt, args);
    va_end(args);
}

/*=== 新日志系统实现 ===*/

/* exempt: 惰性守卫豁免 —— 日志会话为"init→shutdown 可重入"模式：
 * debug_log_shutdown 将 s_debug_state.initialized 置 false 并关闭 log_file，
 * 允许再次 init（lv_once 不可重置，转换后 shutdown 无法恢复）；
 * 且 L55/L242 的 "log_file && initialized" 为消费者活性检查（日志文件句柄
 * 与标志位联合判定，非初始化守卫）。锁已由 DEBUG_LOG_LOCK_GUARD（debug_internal.h
 * 的 lv_once 互斥锁）保证线程安全，故保留手写标志检查，不迁移。 */
int debug_log_init(void) {
    /* 内层作用域锁守卫（DEBUG_LOG_LOCK_GUARD 经 debug_log_mutex 的 lv_once 保证互斥锁只初始化一次）：
     * 块结束自动解锁，随后的环形缓冲区创建与 LOG_INFO 保持在锁外
     * （LOG_INFO → debug_log 会再次获取 log_mutex，若仍在锁内将自死锁）。 */
    {
        DEBUG_LOG_LOCK_GUARD();
        if (s_debug_state.initialized) {
            return 0; /* 已初始化（守卫自动解锁） */
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
    }

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
    /* 内层作用域锁守卫：必须在锁内检查 initialized，防止与 debug_log_init() 产生竞态条件；
     * 块结束自动解锁，随后的环形缓冲区销毁保持在锁外（与原来一致）。 */
    {
        DEBUG_LOG_LOCK_GUARD();
        if (!s_debug_state.initialized) {
            return; /* 守卫自动解锁 */
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
    }

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
    /* 作用域锁守卫：函数末尾自动解锁 */
    DEBUG_LOG_LOCK_GUARD();
    g_log_level = level;
}

LogLevel debug_get_log_level(void) {
    /* 作用域锁守卫：函数末尾自动解锁 */
    DEBUG_LOG_LOCK_GUARD();
    LogLevel level = g_log_level;
    return level;
}

void debug_set_mode(bool debug_mode) {
    /* 作用域锁守卫：函数末尾自动解锁 */
    DEBUG_LOG_LOCK_GUARD();
    g_debug_mode = debug_mode;
    if (debug_mode) {
        g_log_level = LOG_LEVEL_DEBUG;
    } else {
        g_log_level = LOG_LEVEL_WARN;
    }
}

bool debug_is_debug_mode(void) {
    /* 作用域锁守卫：函数末尾自动解锁 */
    DEBUG_LOG_LOCK_GUARD();
    bool mode = g_debug_mode;
    return mode;
}

void debug_log(LogLevel level, const char *module, const char *fmt, ...) {
    /* 格式化消息缓冲区（块外声明：供块外环形缓冲区写入使用） */
    char message[lv_DEBUG_LOG_MESSAGE_BUF_SIZE];
    /* 内层作用域锁守卫：块结束自动解锁，随后的环形日志缓冲区写入保持在锁外
     * （lv_log_ring_buffer_write 内部自带锁，避免锁内嵌套）。 */
    {
        DEBUG_LOG_LOCK_GUARD();

        /* 检查该日志级别是否需要记录（在锁内检查，防止与 debug_set_log_level 并发修改竞争） */
        if (level < g_log_level) {
            return; /* 守卫自动解锁 */
        }

        /* 检查是否需要轮转 */
        check_rotation();

        /* 格式化消息 */
        char timestamp[lv_DEBUG_TIMESTAMP_BUF_SIZE];
        get_timestamp(timestamp, sizeof(timestamp));

        /* 格式化可变参数 */
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
    }

    /* 【v3.3.0】写入全局环形日志缓冲区（替代原 log_buffer；锁外调用，wrapper 内部自行加锁） */
    if (s_debug_state.log_ring_buffer) {
        lv_log_ring_buffer_write(s_debug_state.log_ring_buffer, level, module ? module : "unknown", NULL, NULL, 0,
                                 "%s", message);
    }

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
