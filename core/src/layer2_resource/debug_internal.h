/**
 * @file debug_internal.h
 * @brief Internal shared definitions for debug module.
 */

#ifndef lv_DEBUG_INTERNAL_H
#define lv_DEBUG_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include "debug.h"
#include "lv_internal.h"
#include "lv/lv_platform.h"

/* Thread-local stream context (defined in debug.c) */
extern lv_THREAD_LOCAL StreamContext *debug_stream_ctx;

/* ---- shared constants (moved from debug.c header) ---- */
#define lv_EMERGENCY_LOG_BUFFER_SIZE 256

/* ---- shared global state ---- */
typedef struct DebugState {
    /* 日志文件状态 */
    FILE *log_file;                                   /**< 日志文件句柄 */
    char log_file_path[lv_LOG_PATH_MAX];              /**< 日志文件路径 */
    char log_dir_path[lv_LOG_PATH_MAX];               /**< 日志目录路径 */
    size_t current_log_size;                          /**< 当前日志文件大小 */
    volatile bool initialized;                        /**< 初始化标志 */

    /* 线程安全 */
    lv_mutex_t log_mutex;                             /**< 日志互斥锁 */
    lv_once_t log_once;                               /**< 日志一次初始化控制 */

    /* 性能计数器 */
    PerformanceCounters counters;                      /**< 性能计数器 */
    lv_mutex_t counter_mutex;                         /**< 计数器互斥锁 */
    lv_once_t counter_once;                           /**< 计数器一次初始化控制 */

    /* 环形日志缓冲区 */
    lvLogRingBuffer *log_ring_buffer;                 /**< 环形日志缓冲区指针 */
    int log_ring_buffer_capacity;                     /**< 环形日志缓冲区容量 */

    /* 紧急保存日志缓冲区 */
    char *log_buffer[lv_EMERGENCY_LOG_BUFFER_SIZE];   /**< 紧急日志缓冲条目 */
    int log_buffer_head;                              /**< 环形缓冲头部索引 */
    int log_buffer_count;                             /**< 缓冲中条目数 */

    /* 追踪会话 */
    TraceSession *trace_session;                      /**< 追踪会话指针 */

    /* 端口不变量描述（unused, retained for compatibility） */
    const char *port_invariant_description;            /**< 端口不变量描述字符串 */
} DebugState;

extern DebugState s_debug_state;
extern lv_THREAD_LOCAL LogLevel g_log_level;
extern lv_THREAD_LOCAL bool g_debug_mode;

/* ---- shared helpers (defined in debug_state.c) ---- */
void log_lock(void);
void log_unlock(void);
void counter_lock(void);
void counter_unlock(void);
void debug_refcount_lock(void);
void debug_refcount_unlock(void);
#define debug_lock_refcount debug_refcount_lock
#define debug_unlock_refcount debug_refcount_unlock
const char *log_level_string(LogLevel level);
const char *get_home_dir(void);
int create_directory(const char *path);
void get_timestamp(char *buf, size_t size);
void rotate_logs(void);
void check_rotation(void);

/* emergency buffer helper (defined in debug_emergency.c) */
void log_buffer_append(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* lv_DEBUG_INTERNAL_H */
