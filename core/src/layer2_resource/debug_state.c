/**
 * @file debug_state.c
 * @brief shared state and infrastructure
 * @details Split from debug.c
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

#include "lv/lv_json.h"

#include "lv/context.h"
#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"
#include "debug_internal.h"

/*=== 线程安全策略 ===
 *
 * 本模块的全局状态分为以下几类，各自有不同的保护策略：
 *
 * 1. 日志互斥锁（g_log_mutex）：
 *    - 保护：g_log_file, g_log_file_path, g_log_dir_path,
 *            g_current_log_size, g_initialized, g_log_level, g_debug_mode
 *    - 所有日志写入路径（debug_log, debug_log_legacy_impl 等）均通过
 *      log_lock()/log_unlock() 获取此锁。
 *    - debug_log_init() 在 POSIX 上通过 pthread_mutex_lock 保护初始化；
 *      在 Windows 上通过 InterlockedCompareExchange 确保单次初始化，
 *      并在写入全局路径前获取 g_log_mutex。
 *    - debug_log_shutdown() 在检查 g_initialized 之前获取锁，
 *      防止与 debug_log_init() 产生竞态条件。
 *
 * 2. 性能计数器互斥锁（g_counter_mutex）：
 *    - 保护：g_counters（PerformanceCounters 结构体）
 *    - 所有计数器读写路径均通过 counter_lock()/counter_unlock() 获取此锁。
 *
 * 3. 线程局部变量（lv_THREAD_LOCAL）：
 *    - g_log_level 和 g_debug_mode 为线程局部存储，
 *      每个线程有独立副本，无需锁保护。
 *
 * 4. 互斥锁自身的初始化：
 *    - POSIX：使用 PTHREAD_MUTEX_INITIALIZER 静态初始化，可直接使用。
 *    - Windows：使用 InterlockedCompareExchange + volatile LONG 标志
 *      确保每个 CRITICAL_SECTION 只初始化一次。
 *
 * 注意：g_log_file 是全局共享的（非线程局部），因为日志系统本身
 * 应该是全局共享的。关键是确保所有访问都在互斥锁保护下进行。
 */

/** 模块级唯一状态实例（替代原有的 17 个分散 static 变量） */
DebugState s_debug_state = {0};

/*=== 内部状态 ===*/

/* 全局日志级别（默认：普通模式下仅记录 INFO 及以上） */
lv_THREAD_LOCAL LogLevel g_log_level = LOG_LEVEL_INFO;

/* 调试模式标志（启用 DEBUG 级别日志） */
lv_THREAD_LOCAL bool g_debug_mode = false;

/* 日志文件句柄（全局共享，所有线程写入同一日志文件，由 log_mutex 保护） */

/* 日志文件路径（全局共享，由 log_mutex 保护） */

/* 日志目录路径（全局共享，由 log_mutex 保护） */

/* 当前日志文件大小（全局共享，由 log_mutex 保护） */

/* 初始化标志（全局共享，由 log_mutex 保护） */

/* log_mutex 初始化函数 */
static void log_mutex_init_func(void) {
    lv_mutex_init(&s_debug_state.log_mutex);
}

/* 性能计数器（全局共享，由 counter_mutex 保护） */

/* counter_mutex 初始化函数 */
static void counter_mutex_init_func(void) {
    lv_mutex_init(&s_debug_state.counter_mutex);
}

/*=== 环形日志缓冲区（v3.3.0 新增）===*/

/** 全局环形日志缓冲区（线程安全，前置声明以支持所有函数访问） */

/** 环形日志缓冲区容量（默认 256） */

/*=== 内部辅助函数 ===*/

/* 锁函数生成宏：消除 6 个锁函数的重复代码 */
#define lv_DEFINE_LOCK_FUNCS(name, mutex_var) \
    static void name##_lock(void) {           \
        lv_mutex_lock(&(mutex_var));          \
    }                                         \
    static void name##_unlock(void) {         \
        lv_mutex_unlock(&(mutex_var));        \
    }

/* 日志加锁 */
void log_lock(void) {
    lv_once(&s_debug_state.log_once, log_mutex_init_func);
    lv_mutex_lock(&s_debug_state.log_mutex);
}

void log_unlock(void) {
    lv_mutex_unlock(&s_debug_state.log_mutex);
}

/* 作用域守卫用互斥锁 getter（内部先 lv_once 初始化，与 log_lock 语义一致） */
lv_mutex_t *debug_log_mutex(void) {
    lv_once(&s_debug_state.log_once, log_mutex_init_func);
    return &s_debug_state.log_mutex;
}

/* 性能计数器加锁 */
void counter_lock(void) {
    lv_once(&s_debug_state.counter_once, counter_mutex_init_func);
    lv_mutex_lock(&s_debug_state.counter_mutex);
}

void counter_unlock(void) {
    lv_mutex_unlock(&s_debug_state.counter_mutex);
}

/* 作用域守卫用互斥锁 getter（内部先 lv_once 初始化，与 counter_lock 语义一致） */
lv_mutex_t *debug_counter_mutex(void) {
    lv_once(&s_debug_state.counter_once, counter_mutex_init_func);
    return &s_debug_state.counter_mutex;
}

/* 引用计数加锁/解锁（复用 counter_mutex；先 lv_once 初始化，与 counter_lock 语义一致） */
void debug_refcount_lock(void) {
    lv_once(&s_debug_state.counter_once, counter_mutex_init_func);
    lv_mutex_lock(&s_debug_state.counter_mutex);
}

void debug_refcount_unlock(void) {
    lv_mutex_unlock(&s_debug_state.counter_mutex);
}

/* 日志级别 → 名称映射表（TRACE=-1，以 TRACE 为基准偏移下标；未知级别返回 "UNKNOWN"） */
#define LV_LOGLEVEL_X(x) \
    x(LOG_LEVEL_TRACE - LOG_LEVEL_TRACE, "TRACE") \
    x(LOG_LEVEL_DEBUG - LOG_LEVEL_TRACE, "DEBUG") \
    x(LOG_LEVEL_INFO - LOG_LEVEL_TRACE, "INFO") \
    x(LOG_LEVEL_WARN - LOG_LEVEL_TRACE, "WARN") \
    x(LOG_LEVEL_ERROR - LOG_LEVEL_TRACE, "ERROR") \
    x(LOG_LEVEL_FATAL - LOG_LEVEL_TRACE, "FATAL")
static const char *kLogLevelNames[] = {
    lv_XMACRO_TO_NAME_ARRAY(LV_LOGLEVEL_X)
};
#undef LV_LOGLEVEL_X

/* 获取日志级别字符串 —— v3.3.0：扩展 TRACE 和 FATAL 级别 */
const char *log_level_string(LogLevel level) {
    if (level >= LOG_LEVEL_TRACE && level <= LOG_LEVEL_FATAL) {
        return kLogLevelNames[(int) level - LOG_LEVEL_TRACE];
    }
    return "UNKNOWN";
}

/* 获取主目录：薄包装，实现已收敛至 lv_path_home_dir()
 * （lv_path.c：跨平台 USERPROFILE/HOMEDRIVE+HOMEPATH 与 HOME 空值回落统一处理） */
const char *get_home_dir(void) {
    return lv_path_home_dir();
}

/**
 * @brief 递归创建目录
 * @param path 要创建的目录路径
 * @return 0 成功，-1 失败
 * @note 逐级建目录实现已收编至 lv_path_mkdirs（core/src/layer2_resource/lv_path.c）
 */
int create_directory(const char *path) {
    if (lv_path_mkdirs(path) != 0) {
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "创建目录失败");
    }
    return 0;
}

/**
 * @brief 获取当前时间戳字符串
 * @param buf  输出缓冲区，用于存储格式化后的时间戳
 * @param size 缓冲区大小（字节）
 * @note 时间戳格式为 "YYYY-MM-DD HH:MM:SS"
 *       统一走 lv_format_time（与 lv_utils_misc.c 的格式化实现同源），
 *       输出格式逐字符一致。
 */
void get_timestamp(char *buf, size_t size) {
    lv_format_time((uint64_t) time(NULL) * lv_US_PER_S, buf, size);
}

/* 日志文件轮转
 * @note 调用此函数前必须已持有 log_lock()，否则存在竞态条件风险。
 *       当前唯一调用点 check_rotation() 在 debug_log() 中被调用时已持有锁。 */
void rotate_logs(void) {
    char old_path[lv_LOG_PATH_MAX];
    char new_path[lv_LOG_PATH_MAX];
    char name_buf[64];
    int i;

    /* 如果存在则删除最旧的文件 */
    lv_snprintf(name_buf, sizeof(name_buf), "%s.%d", lv_DEBUG_LOG_BASENAME, lv_LOG_MAX_FILES);
    lv_path_join(s_debug_state.log_dir_path, name_buf, old_path, lv_LOG_PATH_MAX);
    if (lv_file_exists(old_path)) {
        remove(old_path);
    }

    /* 重命名现有文件: .4 -> .5, .3 -> .4, 依此类推 */
    for (i = lv_LOG_MAX_FILES - 1; i >= 1; i--) {
        lv_snprintf(name_buf, sizeof(name_buf), "%s.%d", lv_DEBUG_LOG_BASENAME, i);
        lv_path_join(s_debug_state.log_dir_path, name_buf, old_path, lv_LOG_PATH_MAX);
        lv_snprintf(name_buf, sizeof(name_buf), "%s.%d", lv_DEBUG_LOG_BASENAME, i + 1);
        lv_path_join(s_debug_state.log_dir_path, name_buf, new_path, lv_LOG_PATH_MAX);
        if (lv_file_exists(old_path)) {
            rename(old_path, new_path);
        }
    }

    /* 将当前日志重命名为 .1 */
    if (s_debug_state.log_file) {
        fclose(s_debug_state.log_file);
        s_debug_state.log_file = NULL;
    }

    /* 重置日志大小计数器，因为旧文件已被关闭 */
    s_debug_state.current_log_size = 0;

    lv_path_join(s_debug_state.log_dir_path, lv_DEBUG_LOG_BASENAME, old_path, lv_LOG_PATH_MAX);
    lv_snprintf(name_buf, sizeof(name_buf), "%s.1", lv_DEBUG_LOG_BASENAME);
    lv_path_join(s_debug_state.log_dir_path, name_buf, new_path, lv_LOG_PATH_MAX);
    if (lv_file_exists(old_path)) {
        rename(old_path, new_path);
    }

    /* 重新打开日志文件。
     * 修复：如果 fopen 失败，需要记录错误日志，确保 log_file 保持为 NULL，
     * 避免后续代码使用无效的文件指针。current_log_size 已在上面重置为 0。 */
    s_debug_state.log_file = fopen(old_path, "a");
    if (!s_debug_state.log_file) {
        /* fopen 失败：记录到 stderr（因为日志文件不可用），
         * log_file 保持 NULL，后续 debug_log 会跳过文件写入 */
        fprintf(stderr, "[DEBUG] rotate_logs: 无法重新打开日志文件: %s\n", old_path);
    }

    /* 获取当前文件大小（如果文件存在）。
     * 修复：显式检查 ftell 返回值是否为 -1L（表示错误），
     * 例如文件为管道或 fseek 失败时 ftell 会返回 -1L。 */
    if (s_debug_state.log_file) {
        long pos = ftell(s_debug_state.log_file);
        if (pos > 0) {
            s_debug_state.current_log_size = (size_t) pos;
        }
        /* pos == 0: 空文件或新建文件，current_log_size 保持 0，无需处理 */
        /* pos == -1L: ftell 错误（如文件为管道），保持 current_log_size = 0，
         * 避免将 (size_t)-1 赋值导致变为极大值，从而触发不必要的轮转 */
    }
}

/**
 * @brief 检查并在需要时执行日志文件轮转
 * @note 当当前日志文件大小达到 lv_LOG_MAX_SIZE 时，自动执行轮转操作，
 *       将旧日志文件重命名并创建新的日志文件
 */
void check_rotation(void) {
    if (s_debug_state.current_log_size >= lv_LOG_MAX_SIZE) {
        rotate_logs();
    }
}

/*=== 调试上下文函数 ===*/

/**
 * @brief 创建调试上下文
 * @return 新创建的 DebugContext 指针，内存分配失败时返回 NULL
 * @note 调用者在使用完毕后需调用 debug_context_destroy() 释放资源
 */
DebugContext *debug_context_create(void) {
    DebugContext *ctx = lv_calloc(1, sizeof(DebugContext));
    if (!ctx)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "分配 DebugContext 失败");
    ctx->port_invariant_checks = false;
    ctx->abort_on_violation = false;
    ctx->violation_count = 0;
    return ctx;
}

/**
 * @brief 销毁调试上下文并释放资源
 * @param ctx 要销毁的调试上下文指针，传入 NULL 时安全返回
 */
void debug_context_destroy(DebugContext *ctx) {
    lv_free((void **) &ctx);
}

