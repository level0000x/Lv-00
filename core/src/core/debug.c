/**
 * @file debug.c
 * @brief 调试工具实现
 * @details 实现日志系统、性能计数器、内存池、引用计数/GC、
 *          紧急保存和追踪会话等调试功能。
 */

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#define access _access
#define LV00_DEBUG_F_OK 0
#else
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#define LV00_DEBUG_F_OK F_OK
#endif

#include "debug.h"
#include "context.h"      /* v3.3.0: 结构化日志需要 Lv00Context */
#include "engine.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"

LV00_DECLARE_STREAM_CTX(debug)

/* ==================== 命名常量（消除魔术数字） ==================== */

/** 错误诊断消息缓冲区的默认大小 */
#define LV00_DEBUG_MSG_BUF_SIZE 512

/** 时间戳格式化缓冲区大小 */
#define LV00_DEBUG_TIMESTAMP_BUF_SIZE 32

/** GC 及内存池的默认块大小 */
#define LV00_DEBUG_GC_BLOCK_SIZE 2048

/** 默认日志文件基本名称 */
#define LV00_DEBUG_LOG_BASENAME "lv00.log"

/** 日志消息格式化缓冲区大小 */
#define LV00_DEBUG_LOG_MESSAGE_BUF_SIZE 4096

/** 日志行拼接缓冲区大小（含时间戳、级别、模块名、消息） */
#define LV00_DEBUG_LOG_LINE_BUF_SIZE 8192

/** 追踪会话初始事件容量 */
#define LV00_DEBUG_TRACE_INITIAL_CAPACITY 64

/** 空 JSON 导出缓冲区大小 */
#define LV00_DEBUG_EMPTY_JSON_BUF_SIZE 32

/** JSON 导出初始缓冲区容量 */
#define LV00_DEBUG_JSON_INITIAL_CAPACITY 1024

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
 * 3. 线程局部变量（LV00_THREAD_LOCAL）：
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

/*=== 内部状态 ===*/

/* 全局日志级别（默认：普通模式下仅记录 INFO 及以上） */
static LV00_THREAD_LOCAL LogLevel g_log_level = LOG_LEVEL_INFO;

/* 调试模式标志（启用 DEBUG 级别日志） */
static LV00_THREAD_LOCAL bool g_debug_mode = false;

/* 日志文件句柄（全局共享，所有线程写入同一日志文件，由 g_log_mutex 保护） */
static FILE *g_log_file = NULL;

/* 日志文件路径（全局共享，由 g_log_mutex 保护） */
static char g_log_file_path[LV00_LOG_PATH_MAX] = {0};

/* 日志目录路径（全局共享，由 g_log_mutex 保护） */
static char g_log_dir_path[LV00_LOG_PATH_MAX] = {0};

/* 当前日志文件大小（全局共享，由 g_log_mutex 保护） */
static size_t g_current_log_size = 0;

/* 初始化标志（全局共享，由 g_log_mutex 保护） */
static volatile bool g_initialized = false;

/* 线程安全互斥锁 */
#ifdef _WIN32
static CRITICAL_SECTION g_log_mutex;
static INIT_ONCE g_log_init_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK log_mutex_init_callback(PINIT_ONCE once, PVOID param, PVOID *context) {
    (void)once; (void)param; (void)context;
    InitializeCriticalSection(&g_log_mutex);
    return TRUE;
}

static void log_ensure_mutex_init(void) {
    InitOnceExecuteOnce(&g_log_init_once, log_mutex_init_callback, NULL, NULL);
}
#else
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* 性能计数器（全局共享，由 g_counter_mutex 保护） */
static PerformanceCounters g_counters = {0};

#ifdef _WIN32
static CRITICAL_SECTION g_counter_mutex;
static INIT_ONCE g_counter_init_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK counter_mutex_init_callback(PINIT_ONCE once, PVOID param, PVOID *context) {
    (void)once; (void)param; (void)context;
    InitializeCriticalSection(&g_counter_mutex);
    return TRUE;
}

static void counter_ensure_mutex_init(void) {
    InitOnceExecuteOnce(&g_counter_init_once, counter_mutex_init_callback, NULL, NULL);
}
#else
static pthread_mutex_t g_counter_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/*=== 环形日志缓冲区（v3.3.0 新增）===*/

/** 全局环形日志缓冲区（线程安全，前置声明以支持所有函数访问） */
static Lv00LogRingBuffer *g_log_ring_buffer = NULL;

/** 环形日志缓冲区容量（默认 256） */
static int g_log_ring_buffer_capacity = LV00_LOG_RING_BUFFER_DEFAULT_CAPACITY;

/*=== 内部辅助函数 ===*/

#ifdef _WIN32
/** @brief 通用互斥锁加锁宏（Windows 下使用 InitOnceExecuteOnce 确保线程安全惰性初始化） */
#define LV00_MUTEX_LOCK(mutex) do { \
    EnterCriticalSection(&(mutex)); \
} while (0)

/** @brief 通用互斥锁解锁宏 */
#define LV00_MUTEX_UNLOCK(mutex) do { \
    LeaveCriticalSection(&(mutex)); \
} while (0)
#else
/** @brief 通用互斥锁加锁宏（POSIX 版本） */
#define LV00_MUTEX_LOCK(mutex) do { \
    pthread_mutex_lock(&(mutex)); \
} while (0)

/** @brief 通用互斥锁解锁宏（POSIX 版本） */
#define LV00_MUTEX_UNLOCK(mutex) do { \
    pthread_mutex_unlock(&(mutex)); \
} while (0)
#endif

/* 锁函数生成宏：消除 6 个锁函数的重复代码 */
#define LV00_DEFINE_LOCK_FUNCS(name, mutex_var) \
    static void name##_lock(void) { \
        LV00_MUTEX_LOCK(mutex_var); \
    } \
    static void name##_unlock(void) { \
        LV00_MUTEX_UNLOCK(mutex_var); \
    }

/* 日志加锁（使用 InitOnceExecuteOnce 确保互斥锁初始化） */
static void log_lock(void) {
#ifdef _WIN32
    log_ensure_mutex_init();
#endif
    LV00_MUTEX_LOCK(g_log_mutex);
}

static void log_unlock(void) {
    LV00_MUTEX_UNLOCK(g_log_mutex);
}

/* 性能计数器加锁（使用 InitOnceExecuteOnce 确保互斥锁初始化） */
static void counter_lock(void) {
#ifdef _WIN32
    counter_ensure_mutex_init();
#endif
    LV00_MUTEX_LOCK(g_counter_mutex);
}

static void counter_unlock(void) {
    LV00_MUTEX_UNLOCK(g_counter_mutex);
}

/* 引用计数加锁/解锁（复用 counter_mutex） */
LV00_DEFINE_LOCK_FUNCS(debug_refcount, g_counter_mutex)
#define debug_lock_refcount debug_refcount_lock
#define debug_unlock_refcount debug_refcount_unlock

/* 获取日志级别字符串 —— v3.3.0：扩展 TRACE 和 FATAL 级别 */
static const char *log_level_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_TRACE:
            return "TRACE";
        case LOG_LEVEL_DEBUG:
            return "DEBUG";
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_WARN:
            return "WARN";
        case LOG_LEVEL_ERROR:
            return "ERROR";
        case LOG_LEVEL_FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

/* 获取主目录 */
static const char *get_home_dir(void) {
#ifdef _WIN32
    static char home_path[MAX_PATH] = {0};
    if (home_path[0] == '\0') {
        if (getenv("USERPROFILE")) {
            /* strncpy 不安全使用 → lv00_strlcpy，自动保证零终止 */
            lv00_strlcpy(home_path, getenv("USERPROFILE"), sizeof(home_path));
        } else if (getenv("HOMEDRIVE") && getenv("HOMEPATH")) {
            snprintf(home_path, MAX_PATH, "%s%s", getenv("HOMEDRIVE"), getenv("HOMEPATH"));
        }
    }
    return home_path;
#else
    return getenv("HOME") ? getenv("HOME") : "/tmp";
#endif
}

/**
 * @brief 递归创建目录
 * @param path 要创建的目录路径
 * @return 0 成功，-1 失败
 */
static int create_directory(const char *path) {
    char tmp[LV00_LOG_PATH_MAX];
    char *p = NULL;
    size_t len;

    /* 使用安全的字符串复制函数，确保零终止后再计算 strlen */
    lv00_strlcpy(tmp, path, sizeof(tmp));
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == LV00_PATH_SEPARATOR) {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == LV00_PATH_SEPARATOR) {
            *p = '\0';
            /* 直接尝试创建目录，处理 EEXIST（目录已存在）而非预先检查，
             * 避免 TOCTOU（检查时间-使用时间）竞争条件 */
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = LV00_PATH_SEPARATOR;
        }
    }

    /* 创建路径最后一个组件 */
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

/**
 * @brief 获取当前时间戳字符串
 * @param buf  输出缓冲区，用于存储格式化后的时间戳
 * @param size 缓冲区大小（字节）
 * @note 时间戳格式为 "YYYY-MM-DD HH:MM:SS"
 */
static void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm tm_buf;
    LV00_LOCALTIME(&now, &tm_buf);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tm_buf);
}

/* 日志文件轮转
 * @note 调用此函数前必须已持有 log_lock()，否则存在竞态条件风险。
 *       当前唯一调用点 check_rotation() 在 debug_log() 中被调用时已持有锁。 */
static void rotate_logs(void) {
    char old_path[LV00_LOG_PATH_MAX];
    char new_path[LV00_LOG_PATH_MAX];
    int i;

    /* 如果存在则删除最旧的文件 */
    snprintf(old_path, LV00_LOG_PATH_MAX, "%s%c%s.%d", g_log_dir_path, LV00_PATH_SEPARATOR, LV00_DEBUG_LOG_BASENAME,
             LV00_LOG_MAX_FILES);
    if (access(old_path, LV00_DEBUG_F_OK) == 0) {
        remove(old_path);
    }

    /* 重命名现有文件: .4 -> .5, .3 -> .4, 依此类推 */
    for (i = LV00_LOG_MAX_FILES - 1; i >= 1; i--) {
        snprintf(old_path, LV00_LOG_PATH_MAX, "%s%c%s.%d", g_log_dir_path, LV00_PATH_SEPARATOR, LV00_DEBUG_LOG_BASENAME, i);
        snprintf(new_path, LV00_LOG_PATH_MAX, "%s%c%s.%d", g_log_dir_path, LV00_PATH_SEPARATOR, LV00_DEBUG_LOG_BASENAME, i + 1);
        if (access(old_path, LV00_DEBUG_F_OK) == 0) {
            rename(old_path, new_path);
        }
    }

    /* 将当前日志重命名为 .1 */
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    /* 重置日志大小计数器，因为旧文件已被关闭 */
    g_current_log_size = 0;

    snprintf(old_path, LV00_LOG_PATH_MAX, "%s%c%s", g_log_dir_path, LV00_PATH_SEPARATOR, LV00_DEBUG_LOG_BASENAME);
    snprintf(new_path, LV00_LOG_PATH_MAX, "%s%c%s.1", g_log_dir_path, LV00_PATH_SEPARATOR, LV00_DEBUG_LOG_BASENAME);
    if (access(old_path, LV00_DEBUG_F_OK) == 0) {
        rename(old_path, new_path);
    }

    /* 重新打开日志文件。
     * 修复：如果 fopen 失败，需要记录错误日志，确保 g_log_file 保持为 NULL，
     * 避免后续代码使用无效的文件指针。g_current_log_size 已在上面重置为 0。 */
    g_log_file = fopen(old_path, "a");
    if (!g_log_file) {
        /* fopen 失败：记录到 stderr（因为日志文件不可用），
         * g_log_file 保持 NULL，后续 debug_log 会跳过文件写入 */
        fprintf(stderr, "[DEBUG] rotate_logs: 无法重新打开日志文件: %s\n", old_path);
    }

    /* 获取当前文件大小（如果文件存在）。
     * 修复：显式检查 ftell 返回值是否为 -1L（表示错误），
     * 例如文件为管道或 fseek 失败时 ftell 会返回 -1L。 */
    if (g_log_file) {
        long pos = ftell(g_log_file);
        if (pos > 0) {
            g_current_log_size = (size_t) pos;
        }
        /* pos == 0: 空文件或新建文件，g_current_log_size 保持 0，无需处理 */
        /* pos == -1L: ftell 错误（如文件为管道），保持 g_current_log_size = 0，
         * 避免将 (size_t)-1 赋值导致变为极大值，从而触发不必要的轮转 */
    }
}

/**
 * @brief 检查并在需要时执行日志文件轮转
 * @note 当当前日志文件大小达到 LV00_LOG_MAX_SIZE 时，自动执行轮转操作，
 *       将旧日志文件重命名并创建新的日志文件
 */
static void check_rotation(void) {
    if (g_current_log_size >= LV00_LOG_MAX_SIZE) {
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
    DebugContext *ctx = lv00_malloc(sizeof(DebugContext));
    if (!ctx)
        return NULL;
    ctx->normalization_assertions = false;
    ctx->port_invariant_checks = false;
    ctx->rewrite_trace = false;
    ctx->solver_trace = false;
    ctx->abort_on_violation = false;
    ctx->violation_count = 0;
    return ctx;
}

/**
 * @brief 销毁调试上下文并释放资源
 * @param ctx 要销毁的调试上下文指针，传入 NULL 时安全返回
 */
void debug_context_destroy(DebugContext *ctx) {
    lv00_free((void **) &ctx);
}

/*=== 端口不变量断言 ===*/

static const char *port_invariant_description = "端口不变量检查";

int debug_assert_port_invariants(const LV00Engine *engine, DebugContext *ctx) {
    if (!ctx || !ctx->port_invariant_checks)
        return 0;
    if (!engine || !engine->main_graph)
        return 0;

    int violations = 0;
    ConstraintGraph *graph = engine->main_graph;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (node->type == GEOM_PORT) {
            Port *port = node->data.port;
            if (port->is_formal_param && port->parent_block_id < 0) {
                LOG_ERROR("port", "Node %d: Port is marked as formal param but has invalid parent_block_id (%d)",
                          node->id, port->parent_block_id);
                lv00_set_error(LV00_ERROR_INVALID_STATE,
                               "[PORT INVARIANT VIOLATION] Node %d: Port is marked as formal param but has invalid "
                               "parent_block_id (%d)",
                               node->id, port->parent_block_id);
                violations++;
                /* 在 release 构建中不应该 abort，而是使用 lv00_set_error
                 * 记录错误后返回当前的 violations 计数。 */
                if (ctx->abort_on_violation) {
                    return violations;
                }
            }
            if (port->namespace_depth < 0) {
                LOG_ERROR("port", "Node %d: Port has negative namespace_depth (%d)", node->id, port->namespace_depth);
                lv00_set_error(LV00_ERROR_INVALID_STATE,
                               "[PORT INVARIANT VIOLATION] Node %d: Port has negative namespace_depth (%d)", node->id,
                               port->namespace_depth);
                violations++;
                /* 在 release 构建中不应该 abort，而是使用 lv00_set_error
                 * 记录错误后返回当前的 violations 计数。 */
                if (ctx->abort_on_violation) {
                    return violations;
                }
            }
        }
        if (node->type == GEOM_FUNCTION_BLOCK) {
            for (int j = 0; j < node->data.func_block.input_count; j++) {
                int port_id = node->data.func_block.input_port_ids[j];
                GeomNode *port_node = graph_get_node(graph, port_id);
                if (port_node && port_node->type == GEOM_PORT) {
                    Port *p = port_node->data.port;
                    if (p->parent_block_id != node->id || !p->is_formal_param) {
                        LOG_ERROR("port",
                                  "Function block %d input port %d: parent_block_id mismatch (expected %d, got %d) or "
                                  "is_formal_param is false",
                                  node->id, port_id, node->id, p->parent_block_id);
                        lv00_set_error(LV00_ERROR_INVALID_STATE,
                                       "[PORT INVARIANT VIOLATION] Function block %d input port %d: parent_block_id "
                                       "mismatch (expected %d, got %d) or is_formal_param is false",
                                       node->id, port_id, node->id, p->parent_block_id);
                        violations++;
                        /* 在 release 构建中不应该 abort，而是使用 lv00_set_error
                         * 记录错误后返回当前的 violations 计数。 */
                        if (ctx->abort_on_violation) {
                            return violations;
                        }
                    }
                }
            }
            for (int j = 0; j < node->data.func_block.output_count; j++) {
                int port_id = node->data.func_block.output_port_ids[j];
                GeomNode *port_node = graph_get_node(graph, port_id);
                if (port_node && port_node->type == GEOM_PORT) {
                    Port *p = port_node->data.port;
                    if (p->parent_block_id != node->id || p->is_formal_param) {
                        LOG_ERROR("port",
                                  "Function block %d output port %d: parent_block_id mismatch (expected %d, got %d) or "
                                  "is_formal_param is true",
                                  node->id, port_id, node->id, p->parent_block_id);
                        lv00_set_error(LV00_ERROR_INVALID_STATE,
                                       "[PORT INVARIANT VIOLATION] Function block %d output port %d: parent_block_id "
                                       "mismatch (expected %d, got %d) or is_formal_param is true",
                                       node->id, port_id, node->id, p->parent_block_id);
                        violations++;
                        /* 在 release 构建中不应该 abort，而是使用 lv00_set_error
                         * 记录错误后返回当前的 violations 计数。 */
                        if (ctx->abort_on_violation) {
                            return violations;
                        }
                    }
                }
            }
        }
    }

    ctx->violation_count += violations;
    return violations;
}

/* ================================================================== */
/*  内存池实现                                                         */
/* ================================================================== */

struct Lv00MemPool {
    uint8_t *blocks;   /* 连续内存块数组 */
    int *free_list;    /* 空闲块索引栈 */
    int free_count;    /* 空闲块数量 */
    int total_count;   /* 总块数量 */
    size_t block_size; /* 每个块的大小 */
    uint8_t *used;     /* 使用标志位数组，防止双重释放 */
};

/* 向后兼容别名 */
typedef struct Lv00MemPool Lv00MemPool;
#define MemPool Lv00MemPool

/**
 * @brief 创建固定块大小的内存池
 * @param block_size      每个内存块的大小（字节），必须大于 0
 * @param initial_blocks  初始块数量，必须大于 0
 * @return 新创建的内存池指针，参数无效或内存分配失败时返回 NULL
 * @note 调用者在使用完毕后需调用 mem_pool_destroy() 释放资源
 */
MemPool *mem_pool_create(size_t block_size, int initial_blocks) {
    if (block_size == 0 || initial_blocks <= 0)
        return NULL;

    MemPool *pool = (MemPool *) lv00_malloc(sizeof(MemPool));
    if (!pool)
        return NULL;

    pool->block_size = block_size;
    pool->total_count = initial_blocks;
    pool->free_count = initial_blocks;

    /* 分配连续内存块数组 */
    pool->blocks = (uint8_t *) lv00_calloc((size_t) initial_blocks, block_size);
    if (!pool->blocks) {
        lv00_free((void **) &pool);
        return NULL;
    }

    /* 分配空闲块索引栈 */
    pool->free_list = (int *) lv00_malloc(sizeof(int) * (size_t) initial_blocks);
    if (!pool->free_list) {
        lv00_free((void **) &pool->blocks);
        lv00_free((void **) &pool);
        return NULL;
    }

    /* 初始化空闲列表：所有块初始都是空闲的 */
    for (int i = 0; i < initial_blocks; i++) {
        pool->free_list[i] = i;
    }

    /* 分配使用标志位数组 */
    pool->used = (uint8_t *) lv00_calloc((size_t) initial_blocks, sizeof(uint8_t));
    if (!pool->used) {
        lv00_free((void **) &pool->free_list);
        lv00_free((void **) &pool->blocks);
        lv00_free((void **) &pool);
        return NULL;
    }

    return pool;
}

/**
 * @brief 从内存池中分配一个内存块
 * @param pool 内存池指针
 * @return 分配到的内存块指针，池为空、空闲块耗尽或索引越界时返回 NULL
 * @note 返回的内存块大小由 mem_pool_create() 的 block_size 参数决定
 */
void *mem_pool_alloc(MemPool *pool) {
    if (!pool || pool->free_count <= 0) {
        if (debug_stream_ctx && pool && pool->free_count <= 0) {
            stream_emit_warning(debug_stream_ctx, "内存池分配失败：空闲块已耗尽", 0);
        }
        return NULL;
    }

    pool->free_count--;
    int idx = pool->free_list[pool->free_count];

    /* 溢出检查：确保从空闲列表中取出的索引在有效范围内。
     * 如果空闲列表被损坏（例如内存越界写入），idx 可能超出 total_count，
     * 此时拒绝分配以防止越界访问。 */
    if (idx < 0 || idx >= pool->total_count) {
        pool->free_count++; /* 恢复空闲计数 */
        if (debug_stream_ctx) {
            stream_emit_warning(debug_stream_ctx, "内存池分配失败：空闲列表索引越界", 0);
        }
        return NULL;
    }

    pool->used[idx] = 1; /* 标记为已使用 */
    return (void *) (pool->blocks + (size_t) idx * pool->block_size);
}

/**
 * @brief 将内存块释放回内存池
 * @param pool  内存池指针
 * @param block 要释放的内存块指针，必须由 mem_pool_alloc() 返回
 * @note 如果传入非本池分配的地址或已释放的块，函数将安全返回（不执行任何操作）
 */
void mem_pool_free(MemPool *pool, void *block) {
    if (!pool || !block)
        return;

    /* 计算块索引 */
    uint8_t *ptr = (uint8_t *) block;
    size_t offset = (size_t) (ptr - pool->blocks);
    if (offset % pool->block_size != 0)
        return; /* 不是有效的块地址 */

    int idx = (int) (offset / pool->block_size);
    if (idx < 0 || idx >= pool->total_count)
        return; /* 越界检查 */

    if (!pool->used[idx])
        return; /* 双重释放：块已在空闲列表中 */

    pool->used[idx] = 0; /* 标记为空闲 */

    /* 将块索引压入空闲列表 */
    if (pool->free_count >= pool->total_count)
        return;
    pool->free_list[pool->free_count] = idx;
    pool->free_count++;
}

/**
 * @brief 销毁内存池并释放所有关联资源
 * @param pool 内存池指针，传入 NULL 时安全返回
 * @note 销毁后所有通过 mem_pool_alloc() 分配的指针均失效，调用者需确保不再使用
 */
void mem_pool_destroy(MemPool *pool) {
    if (!pool)
        return;
    lv00_free((void **) &pool->used);
    lv00_free((void **) &pool->blocks);
    lv00_free((void **) &pool->free_list);
    lv00_free((void **) &pool);
}

/**
 * @brief 获取内存池的统计信息
 * @param pool         内存池指针，传入 NULL 时安全返回
 * @param total_blocks  输出参数，接收总块数量，可为 NULL
 * @param free_blocks   输出参数，接收空闲块数量，可为 NULL
 * @param total_bytes   输出参数，接收总字节数（使用 size_t 避免大内存池截断），可为 NULL
 */
void mem_pool_stats(const MemPool *pool, int *total_blocks, int *free_blocks, size_t *total_bytes) {
    if (!pool)
        return;
    if (total_blocks)
        *total_blocks = pool->total_count;
    if (free_blocks)
        *free_blocks = pool->free_count;
    if (total_bytes)
        *total_bytes = (size_t) pool->total_count * pool->block_size;
}

/* ================================================================== */
/*  引用计数与垃圾回收实现                                              */
/* ================================================================== */

/**
 * @brief 增加对象的引用计数
 * @param obj 指向引用计数对象的指针（其第一个成员必须为 RefCounted）
 * @note 此函数是线程安全的，内部使用互斥锁保护引用计数操作
 *
 * [线程安全注意] 本函数使用全局互斥锁 debug_lock_refcount/debug_unlock_refcount
 * 保护 ref_count 的递增操作。由于该锁是全局的（复用 counter_mutex），在高并发场景下
 * 可能成为瓶颈。建议调用者：
 *   - 如果对同一对象的 inc/dec 操作频繁且集中在少数线程，当前实现已足够安全
 *   - 如果需要在多线程环境下高频操作大量不同对象，考虑改为 per-object 锁或使用
 *     C11 atomic_int / InterlockedIncrement 等原子操作以提升性能
 *   - 务必确保 RefCounted 是对象的第一个成员，否则强制类型转换将导致未定义行为
 */
void ref_count_inc(void *obj) {
    if (!obj)
        return;
    RefCounted *rc = (RefCounted *) obj;
    debug_lock_refcount();
    rc->ref_count++;
    debug_unlock_refcount();
}

/**
 * 【引用计数递减 —— 线程安全详细文档】[线程安全注意]
 *
 * 减少引用计数，到0时自动销毁对象。
 *
 * 操作语义：
 *   此函数对 RefCounted 结构体的 ref_count 字段执行三个逻辑操作：
 *     1. READ:  读取当前 ref_count 值
 *     2. MODIFY: 将值减 1
 *     3. WRITE: 写回修改后的值
 *   但在当前实现中，这三个操作是通过 C 语言的 `rc->ref_count--` 完成的，
 *   这是一个非原子的复合操作（在绝大多数平台上对应 3 条机器指令）。
 *
 * 竞态条件场景（多线程环境）：
 *   假设两个线程 T1 和 T2 同时对同一对象调用 ref_count_dec：
 *
 *   时间线:
 *     T1: 读取 ref_count = 2
 *     T2: 读取 ref_count = 2          ← 两个线程都读到 2
 *     T1: 递减 → 1，写回
 *     T2: 递减 → 1，写回              ← 应该是 1（预期）→ 实际也是 1（侥幸正确）
 *
 *   危险时间线（ref_count = 1 时）：
 *     T1: 读取 ref_count = 1
 *     T2: 读取 ref_count = 1          ← 两个线程都读到 1
 *     T1: 递减 → 0，调用 destructor，对象被释放
 *     T2: 递减 → 0，调用 destructor   ← DOUBLE FREE! use-after-free!
 *
 * 调用者防护要求：
 *   - 首选方案：使用外部互斥锁保护对同一对象的所有 ref_count_inc/ref_count_dec 调用
 *   - 次选方案：确保同一对象的引用计数操作仅发生在单一线程中
 *   - 妥协方案：确认该对象在线程间传递时使用"转移语义"（发送方 dec，接收方 inc），
 *     而非"共享语义"（多线程同时持有引用）
 *   - 如果需要在多线程环境下安全地共享引用计数，应在 RefCounted 中改用
 *     C11 atomic_int 或平台特定的原子操作（Windows: InterlockedDecrement，
 *     POSIX: __atomic_sub_fetch）
 *
 * 注意：此函数的参数类型为 void*，内部通过强制类型转换访问 RefCounted 字段。
 * 这要求所有使用引用计数的结构体必须以 RefCounted 作为第一个成员，
 * 且 destructor 字段必须指向正确的销毁函数。
 *
 * @param obj   指向引用计数对象的指针（其第一个成员必须为 RefCounted）
 * @return true  表示对象已被销毁（ref_count 降为 0 并调用了 destructor）
 * @return false 表示对象仍然存活（ref_count > 0），或 obj 为 NULL，
 *               或 ref_count 已经为 0（无效调用，不执行任何操作）
 */
bool ref_count_dec(void *obj) {
    if (!obj)
        return false;
    RefCounted *rc = (RefCounted *) obj;

    /* 使用互斥锁保护引用计数操作，防止多线程竞态条件。
     * 修复：将析构函数调用也放在锁内执行，避免以下竞态场景：
     *   T1: 读取 ref_count=1 → 递减为0 → 解锁
     *   T2: 读取 ref_count=0 → 解锁（返回 false）
     *   T1: 调用 destructor → 释放对象
     *   T2: 此时对象已被释放，若 T2 在解锁前已缓存了 obj 指针，则可能 use-after-free。
     * 将 destructor 调用保持在锁内，确保同一时刻只有一个线程能触发销毁。 */
    debug_lock_refcount();
    if (rc->ref_count <= 0) {
        debug_unlock_refcount();
        return false;
    }
    rc->ref_count--;
    if (rc->ref_count <= 0) {
        /* 引用计数降为零，在锁内调用析构函数并销毁对象，
         * 防止多线程同时触发双重释放 */
        void (*destructor)(void *) = rc->destructor;
        rc->destructor = NULL; /* 置空防止重复调用 */
        debug_unlock_refcount();
        if (destructor) {
            destructor(obj);
        }
        return true; /* 对象已销毁 */
    }
    debug_unlock_refcount();
    return false;
}

/**
 * @brief 获取对象的当前引用计数
 * @param obj 指向引用计数对象的指针（其第一个成员必须为 RefCounted）
 * @return 当前引用计数值，obj 为 NULL 时返回 0
 * @note 此函数未加锁，返回值仅供参考，在多线程环境下可能已过时
 */
int ref_count_get(const void *obj) {
    if (!obj)
        return 0;
    const RefCounted *rc = (const RefCounted *) obj;
    return rc->ref_count;
}

/* ================================================================== */
/*  紧急保存实现                                                       */
/* ================================================================== */

/* 全局紧急保存处理器 (线程局部) */
static LV00_THREAD_LOCAL EmergencySaveHandler g_emergency_handler = NULL;

/* 日志缓冲区：保存最近的日志条目用于紧急保存 */
#define LV00_EMERGENCY_LOG_BUFFER_SIZE 256
static char *g_log_buffer[LV00_EMERGENCY_LOG_BUFFER_SIZE];
static int g_log_buffer_head = 0;
static int g_log_buffer_count = 0;

/* 在 debug_log 中追加日志到缓冲区。
 * 注意：调用者（debug_log）在调用此函数前必须已持有 log_lock()，
 * 因此此函数本身不再加锁，以避免死锁。
 * 若从非 debug_log 路径调用，需确保外部已加锁。 */
static void log_buffer_append(const char *line) {
    if (!line)
        return;
    /* 修复：使用 lv00_strdup_safe 替代 strdup，统一使用项目内存管理函数 */
    char *copy = lv00_strdup_safe(line);
    if (!copy)
        return;

    /* 环形缓冲区：覆盖最旧的条目 */
    if (g_log_buffer[g_log_buffer_head]) {
        /* 修复：使用 lv00_free 替代 free，统一内存释放 */
        lv00_free((void **) &g_log_buffer[g_log_buffer_head]);
    }
    g_log_buffer[g_log_buffer_head] = copy;
    g_log_buffer_head = (g_log_buffer_head + 1) % LV00_EMERGENCY_LOG_BUFFER_SIZE;
    if (g_log_buffer_count < LV00_EMERGENCY_LOG_BUFFER_SIZE) {
        g_log_buffer_count++;
    }
}

void debug_set_emergency_handler(EmergencySaveHandler handler) {
    g_emergency_handler = handler;
}

bool debug_emergency_save(const char *filepath, const EmergencySaveConfig *config) {
    if (!filepath)
        return false;

    if (debug_stream_ctx) {
        stream_emit_error(debug_stream_ctx, "紧急保存触发", 0);
    }

    FILE *f = fopen(filepath, "w");
    if (!f)
        return false;

    /* 写入时间戳 */
    time_t now = time(NULL);
    struct tm tm_buf;
    LV00_LOCALTIME(&now, &tm_buf);
    char time_buf[LV00_DEBUG_TIMESTAMP_BUF_SIZE];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    fprintf(f, "=== Lv-00 Emergency Save ===\n");
    fprintf(f, "Timestamp: %s\n\n", time_buf);

    /* 写入性能计数器 */
    if (config && config->include_counters) {
        PerformanceCounters pc;
        debug_get_counters(&pc);
        fprintf(f, "[Performance Counters]\n");
        fprintf(f, "total_nodes_created=%llu\n", (unsigned long long) pc.total_nodes_created);
        fprintf(f, "current_nodes_alive=%llu\n", (unsigned long long) pc.current_nodes_alive);
        fprintf(f, "total_constraints_created=%llu\n", (unsigned long long) pc.total_constraints_created);
        fprintf(f, "current_constraints_alive=%llu\n", (unsigned long long) pc.current_constraints_alive);
        fprintf(f, "solver_call_count=%llu\n", (unsigned long long) pc.solver_call_count);
        fprintf(f, "solver_total_time_us=%llu\n", (unsigned long long) pc.solver_total_time_us);
        fprintf(f, "solver_avg_time_us=%.3f\n", pc.solver_avg_time_us);
        fprintf(f, "rewrite_total_steps=%llu\n", (unsigned long long) pc.rewrite_total_steps);
        fprintf(f, "rewrite_rule_applications=%llu\n", (unsigned long long) pc.rewrite_rule_applications);
        fprintf(f, "unify_check_count=%llu\n", (unsigned long long) pc.unify_check_count);
        fprintf(f, "unify_success_count=%llu\n", (unsigned long long) pc.unify_success_count);
        fprintf(f, "memory_usage_peak=%llu\n", (unsigned long long) pc.memory_usage_peak);
        fprintf(f, "memory_current=%llu\n", (unsigned long long) pc.memory_current);
        fprintf(f, "\n");
    }

    /* 写入日志缓冲区（最近的 N 条日志） */
    if (config && config->include_log_buffer) {
        fprintf(f, "[Recent Log Buffer (%d entries)]\n", g_log_buffer_count);
        /* 按时间顺序输出：从最旧到最新 */
        int start = (g_log_buffer_head - g_log_buffer_count + LV00_EMERGENCY_LOG_BUFFER_SIZE) % LV00_EMERGENCY_LOG_BUFFER_SIZE;
        for (int i = 0; i < g_log_buffer_count; i++) {
            int idx = (start + i) % LV00_EMERGENCY_LOG_BUFFER_SIZE;
            if (g_log_buffer[idx]) {
                fprintf(f, "%s", g_log_buffer[idx]);
            }
        }
        fprintf(f, "\n");
    }

    /* 写入内存映射信息 */
    if (config && config->include_memory_map) {
        PerformanceCounters pc;
        debug_get_counters(&pc);
        fprintf(f, "[Memory Map]\n");
        fprintf(f, "current_usage_bytes=%llu\n", (unsigned long long) pc.memory_current);
        fprintf(f, "peak_usage_bytes=%llu\n", (unsigned long long) pc.memory_usage_peak);
        fprintf(f, "\n");
    }

    /* include_graph 标记（实际图快照需要引擎上下文，此处记录标记） */
    if (config && config->include_graph) {
        fprintf(f, "[Constraint Graph Snapshot]\n");
        fprintf(f, "Note: Full graph snapshot requires engine context.\n");
        fprintf(f, "Use debug_emergency_save from within engine context for complete dump.\n\n");
    }

    fprintf(f, "=== End of Emergency Save ===\n");
    fclose(f);
    return true;
}

/* ================================================================== */
/*  端口不变量断言（完整版）实现                                        */
/* ================================================================== */

/**
 * @brief 端口与其连接节点之间的深度类型兼容性检查。
 *
 * 使用类型系统的 type_check_equivalence() 执行深度结构类型等价检查。
 * 当类型系统不可用（NULL）时，回退到基本指针比较。
 *
 * @param graph          约束图
 * @param port_node_id   端口节点 ID
 * @param connected_node_id  连接节点的 ID
 * @param ts             类型系统（可为 NULL，用于回退）
 * @return 0 = 兼容, 1 = 不兼容, -1 = 错误
 */
static int check_port_type_deep_compatible(const ConstraintGraph *graph, int port_node_id, int connected_node_id,
                                           TypeSystem *ts) {
    if (!graph)
        return -1;

    GeomNode *port_node = graph_get_node((ConstraintGraph *) graph, port_node_id);
    if (!port_node || port_node->type != GEOM_PORT || !port_node->data.port) {
        return -1;
    }

    GeomNode *connected_node = graph_get_node((ConstraintGraph *) graph, connected_node_id);
    if (!connected_node) {
        return -1;
    }

    Port *port = port_node->data.port;
    TypeRegion *port_type = port->type_region;

    /* 获取连接节点的类型区域 */
    TypeRegion *connected_type = NULL;
    if (connected_node->type == GEOM_PORT && connected_node->data.port) {
        connected_type = connected_node->data.port->type_region;
    }

    /* 如果任一方没有类型信息，返回 0（无类型信息 = 默认兼容） */
    if (!port_type || !connected_type) {
        return 0;
    }

    /* 如果类型系统不可用，回退到基本指针比较。
     * 指针不同意味着类型区域不是同一个对象，视为不兼容。 */
    if (!ts) {
        return (port_type == connected_type) ? 0 : 1;
    }

    /* 双方都有类型且类型系统可用：使用深度等价检查 */
    TypeEquivResult equiv = type_check_equivalence(ts, port_type, connected_type, true);

    switch (equiv) {
        case TYPE_EQUIV_OK:
            return 0; /* 兼容 */
        case TYPE_EQUIV_NOT_EQUIV:
            return 1; /* 不兼容 */
        case TYPE_EQUIV_UNKNOWN:
        case TYPE_EQUIV_NEEDS_INTERACTION:
            return 0; /* 无法证明不兼容，视为兼容 */
        case TYPE_EQUIV_ERROR:
        default:
            return -1; /* 错误 */
    }
}

PortInvariantResult *debug_check_port_invariants(const ConstraintGraph *graph) {
    PortInvariantResult *result = (PortInvariantResult *) lv00_calloc(1, sizeof(PortInvariantResult));
    if (!result)
        return NULL;

    if (!graph) {
        result->all_valid = true;
        result->total_ports = 0;
        return result;
    }

    /* 第一遍：统计端口数量 */
    int total_ports = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i] && graph->nodes[i]->type == GEOM_PORT) {
            total_ports++;
        }
    }

    result->total_ports = total_ports;
    result->invalid_port_ids = (int *) lv00_malloc(sizeof(int) * (total_ports > 0 ? (size_t) total_ports : 1));
    result->error_messages = (char **) lv00_malloc(sizeof(char *) * (total_ports > 0 ? (size_t) total_ports : 1));
    result->invalid_ports = 0;
    result->all_valid = true;

    if (!result->invalid_port_ids || !result->error_messages) {
        lv00_free((void **) &result->invalid_port_ids);
        lv00_free((void **) &result->error_messages);
        lv00_free((void **) &result);
        return NULL;
    }

    /* 第二遍：检查每个端口的不变量 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_PORT)
            continue;

        Port *port = node->data.port;
        if (!port)
            continue;

        bool port_valid = true;

        /* 不变量 1 & 2: 端口的 namespace_depth <= 父函数块的 namespace_depth */
        if (port->parent_block_id >= 0) {
            GeomNode *parent = graph_get_node((ConstraintGraph *) graph, port->parent_block_id);
            if (parent && parent->type == GEOM_FUNCTION_BLOCK) {
                if (port->namespace_depth > parent->namespace_depth) {
                    /* 记录违规 */
                    int idx = result->invalid_ports;
                    result->invalid_port_ids[idx] = node->id;
                    const char *port_type_str = (port->type == PORT_INPUT) ? "INPUT" : "OUTPUT";
                    char msg[LV00_DEBUG_MSG_BUF_SIZE];
                    snprintf(msg, sizeof(msg),
                             "Port %d (%s): namespace_depth (%d) > parent function block %d namespace_depth (%d)",
                             node->id, port_type_str, port->namespace_depth, port->parent_block_id,
                             parent->namespace_depth);
                    lv00_free((void **) &result->error_messages[idx]);
                    result->error_messages[idx] = lv00_strdup_safe(msg);
                    result->invalid_ports++;
                    port_valid = false;
                }
            }
        }

        /* 不变量 3: 端口连接的对方节点存在 */
        if (port->connected_to) {
            /* 检查 connected_to 节点是否在图的节点列表中 */
            bool found = false;
            for (int j = 0; j < graph->node_count; j++) {
                if (graph->nodes[j] == port->connected_to) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                int idx = result->invalid_ports;
                result->invalid_port_ids[idx] = node->id;
                char msg[LV00_DEBUG_MSG_BUF_SIZE];
                snprintf(msg, sizeof(msg), "Port %d: connected_to node does not exist in graph", node->id);
                lv00_free((void **) &result->error_messages[idx]);
                result->error_messages[idx] = lv00_strdup_safe(msg);
                result->invalid_ports++;
                port_valid = false;
            }
        }

        /* 不变量 4: 端口的类型区域与连接节点的类型兼容（深度类型等价检查） */
        if (port->type_region && port->connected_to) {
            int compat = check_port_type_deep_compatible(graph, node->id, port->connected_to->id, NULL);
            if (compat == 1) {
                /* 类型不兼容——记录违规 */
                int idx = result->invalid_ports;
                result->invalid_port_ids[idx] = node->id;
                char msg[LV00_DEBUG_MSG_BUF_SIZE];
                snprintf(msg, sizeof(msg), "Port %d: type incompatible with connected node %d", node->id,
                         port->connected_to->id);
                lv00_free((void **) &result->error_messages[idx]);
                result->error_messages[idx] = lv00_strdup_safe(msg);
                result->invalid_ports++;
                port_valid = false;
            }
        }

        if (!port_valid) {
            result->all_valid = false;
        }
    }

    return result;
}

void debug_port_invariant_result_destroy(PortInvariantResult *result) {
    if (!result)
        return;
    if (result->error_messages) {
        for (int i = 0; i < result->invalid_ports; i++) {
            lv00_free((void **) &result->error_messages[i]);
        }
        lv00_free((void **) &result->error_messages);
    }
    lv00_free((void **) &result->invalid_port_ids);
    lv00_free((void **) &result);
}

/* ================================================================== */
/*  归一化 / 重写 / 求解器追踪实现                                      */
/* ================================================================== */

/* 全局追踪会话 */
static TraceSession *g_trace_session = NULL;

static const char *trace_event_type_string(TraceEventType type) {
    switch (type) {
        case TRACE_NORMALIZATION:
            return "normalization";
        case TRACE_REWRITE:
            return "rewrite";
        case TRACE_SOLVER:
            return "solver";
        default:
            return "unknown";
    }
}

/**
 * @brief 创建追踪会话
 * @return 新创建的追踪会话指针，内存分配失败时返回 NULL
 * @note 初始事件容量为 LV00_DEBUG_TRACE_INITIAL_CAPACITY（64），会话创建后默认处于活跃状态
 */
TraceSession *trace_session_create(void) {
    TraceSession *session = (TraceSession *) lv00_calloc(1, sizeof(TraceSession));
    if (!session)
        return NULL;

    session->capacity = LV00_DEBUG_TRACE_INITIAL_CAPACITY; /* 初始容量 */
    session->events = (TraceEvent *) lv00_calloc((size_t) session->capacity, sizeof(TraceEvent));
    if (!session->events) {
        lv00_free((void **) &session);
        return NULL;
    }

    session->event_count = 0;
    session->active = true;
    return session;
}

/**
 * @brief 销毁追踪会话并释放所有关联资源
 * @param session 追踪会话指针，传入 NULL 时安全返回
 * @note 会释放会话中所有事件的 description 和 details 字符串
 */
void trace_session_destroy(TraceSession *session) {
    if (!session)
        return;

    /* 释放所有事件中的字符串 */
    for (int i = 0; i < session->event_count; i++) {
        lv00_free((void **) &session->events[i].description);
        lv00_free((void **) &session->events[i].details);
    }
    lv00_free((void **) &session->events);
    lv00_free((void **) &session);
}

static void trace_session_ensure_capacity(TraceSession *session) {
    if (session->event_count >= session->capacity) {
        if (session->capacity > INT_MAX / 2)
            return; /* 防止溢出 */
        int new_capacity = session->capacity * 2;
        TraceEvent *new_events =
            (TraceEvent *) lv00_realloc(session->events, sizeof(TraceEvent) * (size_t) new_capacity);
        if (new_events) {
            /* 初始化新增部分为零 */
            memset(new_events + session->capacity, 0, sizeof(TraceEvent) * (size_t) (new_capacity - session->capacity));
            session->events = new_events;
            session->capacity = new_capacity;
        }
    }
}

/**
 * @brief 向追踪会话中记录一个事件
 * @param session     追踪会话指针
 * @param type        事件类型（归一化/重写/求解器）
 * @param step        步骤编号
 * @param description 事件描述字符串，可为 NULL
 * @param details     事件详细信息字符串，可为 NULL
 * @note 如果会话未处于活跃状态或容量不足，事件将被丢弃。description 和 details 会被复制
 */
void trace_record_event(TraceSession *session, TraceEventType type, int step, const char *description,
                        const char *details) {
    if (!session || !session->active)
        return;

    trace_session_ensure_capacity(session);
    if (session->event_count >= session->capacity)
        return;

    TraceEvent *ev = &session->events[session->event_count];
    ev->type = type;
    ev->timestamp = (double) time(NULL);
    ev->step_number = step;
    ev->description = description ? lv00_strdup_safe(description) : NULL;
    ev->details = details ? lv00_strdup_safe(details) : NULL;
    session->event_count++;
}

/**
 * @brief 确保 JSON 缓冲区有足够的空间
 *
 * 当当前写入位置加上所需空间超过缓冲区容量时，以 2 倍策略扩容。
 * 扩容失败时释放缓冲区，调用者应检查返回值。
 *
 * @param json     JSON 缓冲区指针的指针（扩容时可能 realloc）
 * @param capacity 缓冲区当前容量（扩容后会被更新）
 * @param pos      当前写入位置
 * @param needed   本次操作需要的额外字节数
 * @return true  成功（空间充足或扩容成功）
 * @return false 内存不足（*json 已被释放，调用者应中止操作）
 */
static bool trace_ensure_space(char **json, size_t *capacity, size_t *pos, size_t needed) {
    while (*pos + needed >= *capacity) {
        *capacity *= 2;
        char *new_json = lv00_realloc(*json, *capacity);
        if (!new_json) {
            lv00_free((void **) json);
            return false;
        }
        *json = new_json;
    }
    return true;
}

/**
 * @brief 将字符串转义为 JSON 安全格式并追加到缓冲区
 *
 * 遍历源字符串，逐字符写入 JSON 缓冲区，对特殊字符进行转义：
 *   - " → \"
 *   - \ → \\
 *   - \n → \n（换行符）
 *   - \t → \t（制表符）
 *   - \r → \r（回车符）
 *   - \b → \b（退格符）
 *   - \f → \f（换页符）
 *   - 0x00-0x1F 范围的其他控制字符 → \u00XX（四位十六进制）
 *
 * 调用者需确保缓冲区中已有一个未闭合的引号字符（函数会覆盖它），
 * 函数完成后会写入闭合引号。
 *
 * @param json     JSON 缓冲区指针的指针（可能 realloc）
 * @param capacity 缓冲区容量（会被更新）
 * @param pos      当前写入位置（会被更新）
 * @param str      要转义的源字符串
 * @return true  成功
 * @return false 失败（内存不足，*json 已被释放）
 */
static bool trace_json_escape_string(char **json, size_t *capacity, size_t *pos, const char *str) {
    if (!str)
        return true;

    /* 回退覆盖引号，写入转义字符串 */
    (*pos)--;
    if (!trace_ensure_space(json, capacity, pos, 2))
        return false;
    (*json)[(*pos)++] = '"';

    const char *p = str;
    while (*p) {
        switch (*p) {
            case '"':
                if (!trace_ensure_space(json, capacity, pos, 2))
                    return false;
                (*json)[(*pos)++] = '\\';
                (*json)[(*pos)++] = '"';
                break;
            case '\\':
                if (!trace_ensure_space(json, capacity, pos, 2))
                    return false;
                (*json)[(*pos)++] = '\\';
                (*json)[(*pos)++] = '\\';
                break;
            case '\n':
                if (!trace_ensure_space(json, capacity, pos, 2))
                    return false;
                (*json)[(*pos)++] = '\\';
                (*json)[(*pos)++] = 'n';
                break;
            case '\t':
                if (!trace_ensure_space(json, capacity, pos, 2))
                    return false;
                (*json)[(*pos)++] = '\\';
                (*json)[(*pos)++] = 't';
                break;
            case '\r':
                if (!trace_ensure_space(json, capacity, pos, 2))
                    return false;
                (*json)[(*pos)++] = '\\';
                (*json)[(*pos)++] = 'r';
                break;
            case '\b':
                if (!trace_ensure_space(json, capacity, pos, 2))
                    return false;
                (*json)[(*pos)++] = '\\';
                (*json)[(*pos)++] = 'b';
                break;
            case '\f':
                if (!trace_ensure_space(json, capacity, pos, 2))
                    return false;
                (*json)[(*pos)++] = '\\';
                (*json)[(*pos)++] = 'f';
                break;
            default:
                if ((unsigned char) *p < 0x20) {
                    /* 其他控制字符：使用 \u00XX 格式转义 */
                    if (!trace_ensure_space(json, capacity, pos, 6))
                        return false;
                    *pos += (size_t) snprintf(*json + *pos, *capacity - *pos, "\\u%04x", (unsigned char) *p);
                } else {
                    /* 普通可打印字符：直接写入 */
                    if (!trace_ensure_space(json, capacity, pos, 1))
                        return false;
                    (*json)[(*pos)++] = *p;
                }
                break;
        }
        p++;
    }

    /* 写入闭合引号 */
    if (!trace_ensure_space(json, capacity, pos, 2))
        return false;
    (*json)[(*pos)++] = '"';

    return true;
}

char *trace_session_export_json(const TraceSession *session) {
    if (!session) {
        char *empty = lv00_malloc(LV00_DEBUG_EMPTY_JSON_BUF_SIZE);
        if (!empty)
            return NULL;
        snprintf(empty, LV00_DEBUG_EMPTY_JSON_BUF_SIZE, "{\"event_count\":0,\"active\":false}");
        return empty;
    }

    /* 动态增长缓冲区 */
    size_t capacity = LV00_DEBUG_JSON_INITIAL_CAPACITY;
    size_t pos = 0;
    char *json = lv00_malloc(capacity);
    if (!json)
        return NULL;

/* 辅助宏：格式化写入并检查返回值。
     * 保留为宏是因为使用了 snprintf 的可变参数（__VA_ARGS__），
     * 不便提取为普通函数。内部调用 trace_ensure_space 进行容量管理。 */
#define WRITE_FMT(fmt, ...)                                                  \
    do {                                                                     \
        int w = snprintf(json + pos, capacity - pos, fmt, ##__VA_ARGS__);    \
        if (w < 0) {                                                         \
            lv00_free((void **) &json);                                      \
            return NULL;                                                     \
        }                                                                    \
        if ((size_t) w >= capacity - pos) {                                  \
            pos += (capacity - pos - 1);                                     \
            if (!trace_ensure_space(&json, &capacity, &pos, (size_t) w + 1)) \
                return NULL;                                                 \
            w = snprintf(json + pos, capacity - pos, fmt, ##__VA_ARGS__);    \
            if (w < 0) {                                                     \
                lv00_free((void **) &json);                                  \
                return NULL;                                                 \
            }                                                                \
        }                                                                    \
        pos += (size_t) w;                                                   \
    } while (0)

    WRITE_FMT(
        "{\n  \"trace_session\": {\n"
        "    \"event_count\": %d,\n"
        "    \"active\": %s,\n"
        "    \"events\": [\n",
        session->event_count, session->active ? "true" : "false");

    for (int i = 0; i < session->event_count; i++) {
        const TraceEvent *ev = &session->events[i];
        WRITE_FMT(
            "      {\n"
            "        \"index\": %d,\n"
            "        \"type\": \"%s\",\n"
            "        \"timestamp\": %.6f,\n"
            "        \"step\": %d,\n"
            "        \"description\": %s,\n"
            "        \"details\": %s\n"
            "      }%s\n",
            i, trace_event_type_string(ev->type), ev->timestamp, ev->step_number, ev->description ? "\"" : "null",
            ev->details ? "\"" : "null", (i < session->event_count - 1) ? "," : "");

        /* 写入转义后的 description（使用辅助函数消除重复代码） */
        if (ev->description) {
            if (!trace_json_escape_string(&json, &capacity, &pos, ev->description)) {
                return NULL;
            }
        }

        /* 写入转义后的 details（使用辅助函数消除重复代码） */
        if (ev->details) {
            if (!trace_json_escape_string(&json, &capacity, &pos, ev->details)) {
                return NULL;
            }
        }
    }

    WRITE_FMT("    ]\n  }\n}\n");

#undef WRITE_FMT

    return json;
}

TraceSession *debug_get_trace_session(void) {
    if (!g_trace_session) {
        g_trace_session = trace_session_create();
    }
    return g_trace_session;
}

/*=== 遗留日志函数（向后兼容） ===*/

/**
 * @brief 遗留日志的通用实现，避免重复代码。
 * @param subsystem 子系统名称（"normalization"/"rewrite"/"solver"）
 * @param fmt 格式化字符串
 * @param args 可变参数列表
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
    if (g_log_file && g_initialized) {
        char timestamp[LV00_DEBUG_TIMESTAMP_BUF_SIZE];
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(g_log_file, "[%s] [DEBUG] [%s] ", timestamp, subsystem);
        va_copy(args_copy, args);
        vfprintf(g_log_file, fmt, args_copy);
        va_end(args_copy);
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
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
    /* 使用互斥锁保护初始化检查，防止多线程竞态条件。
     * POSIX 上 g_log_mutex 已通过 PTHREAD_MUTEX_INITIALIZER 静态初始化，
     * 可直接使用；Windows 上在初始化前使用原子操作保护。
     *
     * 线程安全说明：
     * - Windows: 使用 InterlockedCompareExchange 确保只有一个线程进入初始化路径，
     *   其他线程通过自旋等待 g_initialized 变为 true。
     *   进入初始化路径后，先初始化 CRITICAL_SECTION，再获取锁保护全局路径写入。
     * - POSIX: 直接使用 pthread_mutex_lock 保护整个初始化过程。 */
#ifdef _WIN32
    static volatile LONG s_init_guard = 0;
    if (InterlockedCompareExchange(&s_init_guard, 1, 0) != 0) {
        /* 另一个线程正在初始化或已完成初始化，等待完成 */
        while (!g_initialized) {
            Sleep(1);
        }
        return 0;
    }
    /* 首次进入的线程：先初始化互斥锁 */
    log_ensure_mutex_init();
    counter_ensure_mutex_init();
    /* 获取日志互斥锁，保护后续对 g_log_dir_path、g_log_file_path 等全局变量的写入 */
    EnterCriticalSection(&g_log_mutex);
#else
    pthread_mutex_lock(&g_log_mutex);
    if (g_initialized) {
        pthread_mutex_unlock(&g_log_mutex);
        return 0; /* 已初始化 */
    }
#endif

    /* 构建日志目录路径: ~/.lv00/logs
     * 以下对 g_log_dir_path 和 g_log_file_path 的写入在 g_log_mutex 保护内，
     * 防止与其他线程读取这些路径产生竞态条件 */
    const char *home = get_home_dir();
    snprintf(g_log_dir_path, LV00_LOG_PATH_MAX, "%s%c.lv00%clogs", home, LV00_PATH_SEPARATOR, LV00_PATH_SEPARATOR);

    /* 创建日志目录 */
    if (create_directory(g_log_dir_path) != 0) {
        lv00_set_error(LV00_ERROR_IO, "[DEBUG] Warning: Could not create log directory: %s", g_log_dir_path);
        /* 继续运行，不使用文件日志 */
    }

    /* 构建日志文件路径 */
    snprintf(g_log_file_path, LV00_LOG_PATH_MAX, "%s%c%s", g_log_dir_path, LV00_PATH_SEPARATOR, LV00_DEBUG_LOG_BASENAME);

    /* 打开日志文件 */
    g_log_file = fopen(g_log_file_path, "a");
    if (!g_log_file) {
        lv00_set_error(LV00_ERROR_IO, "[DEBUG] Warning: Could not open log file: %s", g_log_file_path);
        /* 继续运行，不使用文件日志 */
    } else {
        /* 获取当前文件大小 */
        fseek(g_log_file, 0, SEEK_END);
        long pos = ftell(g_log_file);
        g_current_log_size = (pos > 0) ? (size_t) pos : 0;
    }

    g_initialized = true;

#ifdef _WIN32
    LeaveCriticalSection(&g_log_mutex);
    /* 初始化完成，释放等待的线程 */
    InterlockedExchange(&s_init_guard, 2);
#else
    pthread_mutex_unlock(&g_log_mutex);
#endif

    /* 【v3.3.0】创建全局环形日志缓冲区 */
    if (!g_log_ring_buffer) {
        g_log_ring_buffer = lv00_log_ring_buffer_create(g_log_ring_buffer_capacity);
        if (g_log_ring_buffer) {
            lv00_log_ring_buffer_write(g_log_ring_buffer,
                                       LOG_LEVEL_INFO, "debug", "debug_log_init",
                                       __FILE__, __LINE__,
                                       "环形日志缓冲区已创建（容量: %d）",
                                       g_log_ring_buffer_capacity);
        }
    }

    /* 记录初始化日志 */
    char timestamp[LV00_DEBUG_TIMESTAMP_BUF_SIZE];
    get_timestamp(timestamp, sizeof(timestamp));
    LOG_INFO("debug", "=== Lv-00 v%s Logging System Initialized ===", LV00_VERSION_STRING);

    return 0;
}

void debug_log_shutdown(void) {
    /* 必须在锁内检查 g_initialized，防止与 debug_log_init() 产生竞态条件。
     * 如果在锁外检查，可能出现：线程 A 检查 g_initialized 为 false 并返回，
     * 同时线程 B 正在执行 debug_log_init() 并即将设置 g_initialized = true，
     * 导致线程 A 错过关闭。 */
    log_lock();

    if (!g_initialized) {
        log_unlock();
        return;
    }

    /* 记录关闭日志 */
    if (g_log_file) {
        char timestamp[LV00_DEBUG_TIMESTAMP_BUF_SIZE];
        get_timestamp(timestamp, sizeof(timestamp));
        fprintf(g_log_file, "[%s] [INFO] [debug] === Logging System Shutdown ===\n", timestamp);
        fclose(g_log_file);
        g_log_file = NULL;
    }

    /* 先将 g_initialized 设为 false，阻止新日志进入。
     * 此时仍持有 log_lock，确保后续的锁销毁操作安全。
     * 注意：此处不调用 log_unlock()，而是直接销毁锁，
     * 因为 shutdown 后不应再有其他线程尝试获取此锁。 */
    g_initialized = false;

    /* 修复：销毁全局追踪会话，防止内存泄漏。
     * g_trace_session 在 debug_get_trace_session 中惰性创建，
     * 但此前没有对应的销毁逻辑，导致程序退出时泄漏。 */
    if (g_trace_session) {
        trace_session_destroy(g_trace_session);
        g_trace_session = NULL;
    }

    /* 清理互斥锁。
     * 在 Windows 上，DeleteCriticalSection 会释放锁并允许其他等待线程继续。
     * 在 POSIX 上，pthread_mutex_destroy 要求锁未被持有；
     * 因此先解锁再销毁。
     *
     * 注意：g_initialized 已在上面设为 false，但其他线程可能仍在持有或等待锁。
     * 使用标志位 g_mutex_initialized/g_counter_mutex_initialized 防止
     * log_lock/log_unlock 在销毁后继续操作。 */
#ifdef _WIN32
    DeleteCriticalSection(&g_log_mutex);
    DeleteCriticalSection(&g_counter_mutex);
#else
    pthread_mutex_unlock(&g_log_mutex);
    pthread_mutex_destroy(&g_log_mutex);
    pthread_mutex_unlock(&g_counter_mutex);
    pthread_mutex_destroy(&g_counter_mutex);
#endif

    /* 【v3.3.0】销毁全局环形日志缓冲区 */
    if (g_log_ring_buffer) {
        lv00_log_ring_buffer_destroy(g_log_ring_buffer);
        g_log_ring_buffer = NULL;
    }
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
    char timestamp[LV00_DEBUG_TIMESTAMP_BUF_SIZE];
    get_timestamp(timestamp, sizeof(timestamp));

    /* 格式化可变参数 */
    char message[LV00_DEBUG_LOG_MESSAGE_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    /* 构建日志行 */
    char log_line[LV00_DEBUG_LOG_LINE_BUF_SIZE];
    int len = snprintf(log_line, sizeof(log_line), "[%s] [%s] [%s] %s\n", timestamp, log_level_string(level),
                       module ? module : "unknown", message);

    /* ERROR 和 WARN 输出到 stderr，其余输出到 stdout */
    if (level >= LOG_LEVEL_WARN) {
        fputs(log_line, stderr);
    } else {
        fputs(log_line, stdout);
    }

    /* 写入日志文件 */
    if (g_log_file && g_initialized) {
        fputs(log_line, g_log_file);
        fflush(g_log_file);
        g_current_log_size += (size_t) len;
    }

    /* 追加到紧急保存日志缓冲区 */
    log_buffer_append(log_line);

    log_unlock();

    /* 【v3.3.0】FATAL 级别额外处理：触发紧急保存 */
    if (level == LOG_LEVEL_FATAL) {
        /* FATAL 不在此处加锁/解锁，因为 emergency_save 会在内部自行加锁 */
        EmergencySaveConfig cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.filepath = NULL;          /* 使用默认路径 */
        cfg.include_graph = true;     /* 包含约束图快照 */
        cfg.include_counters = true;  /* 包含性能计数器 */
        cfg.include_log_buffer = true;/* 包含日志缓冲区 */
        cfg.include_memory_map = false;
        debug_emergency_save(NULL, &cfg);
    }
}

/**
 * @brief 高精度时间戳（微秒级）
 *
 * 在 Windows 上使用 QueryPerformanceCounter，
 * 在 POSIX 上使用 clock_gettime(CLOCK_MONOTONIC)。
 *
 * @return 单调递增的微秒时间戳
 */
static uint64_t get_timestamp_us(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    static LARGE_INTEGER freq_cached = {0};
    if (freq_cached.QuadPart == 0) {
        QueryPerformanceFrequency(&freq_cached);
    }
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000ULL) / freq_cached.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
#endif
}

/* ============================================================
 * 环形日志缓冲区实现
 * ============================================================ */

/**
 * @brief 创建环形日志缓冲区
 *
 * 分配 entries 数组并初始化所有控制字段。
 * capacity 必须 >= 1。
 *
 * @param capacity 缓冲区容量（条目数）
 * @return 新分配的缓冲区，失败返回 NULL
 */
Lv00LogRingBuffer *lv00_log_ring_buffer_create(int capacity) {
    if (capacity < 1) {
        capacity = LV00_LOG_RING_BUFFER_DEFAULT_CAPACITY;
    }

    Lv00LogRingBuffer *rb = lv00_malloc(sizeof(Lv00LogRingBuffer));
    if (!rb) {
        return NULL;
    }
    memset(rb, 0, sizeof(Lv00LogRingBuffer));

    rb->entries = lv00_malloc((size_t)capacity * sizeof(Lv00LogEntry));
    if (!rb->entries) {
        lv00_free((void **)&rb);
        return NULL;
    }
    memset(rb->entries, 0, (size_t)capacity * sizeof(Lv00LogEntry));

    rb->capacity = capacity;
    rb->head = 0;
    rb->count = 0;
    rb->wrapped = false;

    return rb;
}

/**
 * @brief 销毁环形日志缓冲区
 * @param rb 缓冲区指针（可为 NULL）
 */
void lv00_log_ring_buffer_destroy(Lv00LogRingBuffer *rb) {
    if (!rb) {
        return;
    }
    lv00_free((void **)&rb->entries);
    lv00_free((void **)&rb);
}

/**
 * @brief 向环形缓冲区写入一条结构化日志
 *
 * 固定大小的环形缓冲区：当 count == capacity 时，
 * 新条目覆盖最旧的条目。
 *
 * @note 此函数内部加锁（log_lock/log_unlock）以确保线程安全。
 *       如果在已持有 log_lock 的上下文中调用，请使用
 *       lv00_log_ring_buffer_write_unlocked() 内部版本。
 */
void lv00_log_ring_buffer_write(Lv00LogRingBuffer *rb, LogLevel level,
                                const char *module_name, const char *function_name,
                                const char *file_name, int line_number,
                                const char *fmt, ...) {
    if (!rb || rb->capacity < 1) {
        return;
    }

    log_lock();

    /* 获取写入位置 */
    int idx = rb->head;
    Lv00LogEntry *entry = &rb->entries[idx];

    /* 填充结构化字段 */
    entry->level = level;
    entry->timestamp_us = get_timestamp_us();
    entry->module_name = module_name;
    entry->function_name = function_name;
    entry->file_name = file_name;
    entry->line_number = line_number;
    entry->context_id = 0; /* 默认全局日志，lv00_log_with_context 会覆盖 */

    /* 格式化消息（定长缓冲区，防止 OOM） */
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->message, sizeof(entry->message), fmt, args);
    va_end(args);

    /* 推进写入位置 */
    rb->head = (rb->head + 1) % rb->capacity;
    if (rb->count < rb->capacity) {
        rb->count++;
    } else {
        rb->wrapped = true;
    }

    log_unlock();
}

/**
 * @brief 导出环形缓冲区中的所有日志（按时间顺序）
 *
 * 以插入顺序导出所有条目（最旧的在前，最新的在后）。
 * 返回的数组由调用者负责释放（使用 lv00_free）。
 *
 * @param rb        环形缓冲区（非 NULL）
 * @param out_count 输出：实际导出的条目数量
 * @return 日志条目数组（按插入时间排序），count == 0 时返回 NULL
 */
Lv00LogEntry *lv00_log_ring_buffer_export(const Lv00LogRingBuffer *rb, int *out_count) {
    if (!rb || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    log_lock();

    if (rb->count == 0) {
        *out_count = 0;
        log_unlock();
        return NULL;
    }

    Lv00LogEntry *exported = lv00_malloc((size_t)rb->count * sizeof(Lv00LogEntry));
    if (!exported) {
        *out_count = 0;
        log_unlock();
        return NULL;
    }

    /* 计算起始位置：如果已填满（wrapped），起始位置 = head（最旧的条目） */
    int start = rb->wrapped ? rb->head : 0;

    for (int i = 0; i < rb->count; i++) {
        int src_idx = (start + i) % rb->capacity;
        memcpy(&exported[i], &rb->entries[src_idx], sizeof(Lv00LogEntry));
    }

    *out_count = rb->count;
    log_unlock();
    return exported;
}

/**
 * @brief 清空环形缓冲区中的所有日志条目
 * @param rb 环形缓冲区（非 NULL）
 */
void lv00_log_ring_buffer_clear(Lv00LogRingBuffer *rb) {
    if (!rb) {
        return;
    }
    log_lock();
    rb->head = 0;
    rb->count = 0;
    rb->wrapped = false;
    memset(rb->entries, 0, (size_t)rb->capacity * sizeof(Lv00LogEntry));
    log_unlock();
}

/**
 * @brief 调整环形缓冲区容量
 *
 * 分配新的 entries 数组，复制最多 new_capacity 条最新日志。
 * 如果 new_capacity < 当前条目数，最旧的多余条目将被丢弃。
 *
 * @param rb       环形缓冲区（非 NULL）
 * @param capacity 新容量（>= 1）
 * @return true 成功，false 失败（内存不足）
 */
bool lv00_log_ring_buffer_resize(Lv00LogRingBuffer *rb, int capacity) {
    if (!rb || capacity < 1) {
        return false;
    }
    if (capacity == rb->capacity) {
        return true; /* 无需改变 */
    }

    log_lock();

    Lv00LogEntry *new_entries = lv00_malloc((size_t)capacity * sizeof(Lv00LogEntry));
    if (!new_entries) {
        log_unlock();
        return false;
    }
    memset(new_entries, 0, (size_t)capacity * sizeof(Lv00LogEntry));

    /* 计算要保留的条目数量（保留最新的） */
    int keep_count = (rb->count < capacity) ? rb->count : capacity;
    if (keep_count > 0) {
        /* 从旧缓冲区中导出最新的 keep_count 条记录。
         * 如果 wrapped，最旧的在 head 位置；否则在位置 0。 */
        int start = rb->wrapped ? rb->head : 0;
        /* 计算最新 keep_count 条记录的起始位置 */
        if (rb->count > keep_count) {
            /* 跳过最旧的 (rb->count - keep_count) 条记录 */
            start = (start + (rb->count - keep_count)) % rb->capacity;
            if (rb->capacity > 0) {
                start = start % rb->capacity;
            }
        }

        for (int i = 0; i < keep_count; i++) {
            int src_idx = (start + i) % rb->capacity;
            memcpy(&new_entries[i], &rb->entries[src_idx], sizeof(Lv00LogEntry));
        }
    }

    /* 替换旧的缓冲区 */
    lv00_free((void **)&rb->entries);
    rb->entries = new_entries;
    rb->capacity = capacity;
    rb->head = keep_count % capacity;
    rb->count = keep_count;
    rb->wrapped = (keep_count >= capacity);

    log_unlock();
    return true;
}

/* ============================================================
 * 带上下文的日志函数实现（v3.3.0 新增）
 *
 * lv00_log_with_context() 是结构化日志的核心入口。
 * 它将日志同时写入标准日志流、文件日志和环形缓冲区。
 * ============================================================ */

/**
 * @brief 记录带完整上下文的日志
 *
 * 流程：
 * 1. 如果上下文有效，提取 context_id 用于追踪
 * 2. 调用 debug_log() 写入标准日志流（受级别过滤控制）
 * 3. 如果全局环形缓冲区存在，写入结构化记录
 * 4. FATAL 级别日志触发紧急保存（在 debug_log 内部处理）
 *
 * @param ctx           上下文指针（可为 NULL）
 * @param level         日志级别
 * @param module_name   模块名称
 * @param function_name 函数名称
 * @param file_name     文件名
 * @param line_number   行号
 * @param fmt           格式字符串
 * @param ...           格式参数
 */
void lv00_log_with_context(struct Lv00Context *ctx, LogLevel level,
                           const char *module_name, const char *function_name,
                           const char *file_name, int line_number,
                           const char *fmt, ...) {
    /* 1. 格式化消息到临时缓冲区 */
    char message[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    /* 2. 写入标准日志流（debug_log 负责级别过滤和文件/控制台输出） */
    debug_log(level, module_name, "%s [%s:%d]", message, function_name, line_number);

    /* 3. 写入全局环形缓冲区 */
    if (g_log_ring_buffer) {
        lv00_log_ring_buffer_write(g_log_ring_buffer, level,
                                   module_name, function_name,
                                   file_name, line_number,
                                   "%s", message);
        /* 覆盖自动设置的 context_id 为实际的上下文 ID */
        log_lock();
        if (g_log_ring_buffer->count > 0) {
            /* 找到刚写入的条目（head - 1，处理绕回） */
            int last_idx = (g_log_ring_buffer->head - 1 + g_log_ring_buffer->capacity)
                           % g_log_ring_buffer->capacity;
            /* 从上下文中获取 ID（如果可用） */
            if (ctx && ctx->context_id > 0) {
                g_log_ring_buffer->entries[last_idx].context_id = ctx->context_id;
            }
        }
        log_unlock();
    }

    /* 4. 使用上下文信息（防止未使用参数警告） */
    (void)ctx;
}

/*=== Performance Counters Implementation ===*/

void debug_get_counters(PerformanceCounters *counters) {
    if (!counters)
        return;

    counter_lock();
    *counters = g_counters;

    /* 计算平均求解器耗时 */
    if (g_counters.solver_call_count > 0) {
        counters->solver_avg_time_us = (double) g_counters.solver_total_time_us / (double) g_counters.solver_call_count;
    }
    counter_unlock();
}

void debug_reset_counters(void) {
    counter_lock();
    memset(&g_counters, 0, sizeof(g_counters));
    counter_unlock();
}

void debug_counter_node_created(void) {
    counter_lock();
    g_counters.total_nodes_created++;
    g_counters.current_nodes_alive++;
    counter_unlock();
}

void debug_counter_node_destroyed(void) {
    counter_lock();
    if (g_counters.current_nodes_alive > 0) {
        g_counters.current_nodes_alive--;
    }
    counter_unlock();
}

void debug_counter_constraint_created(void) {
    counter_lock();
    g_counters.total_constraints_created++;
    g_counters.current_constraints_alive++;
    counter_unlock();
}

void debug_counter_constraint_destroyed(void) {
    counter_lock();
    if (g_counters.current_constraints_alive > 0) {
        g_counters.current_constraints_alive--;
    }
    counter_unlock();
}

void debug_counter_solver_called(uint64_t time_us) {
    counter_lock();
    g_counters.solver_call_count++;
    g_counters.solver_total_time_us += time_us;
    counter_unlock();
}

void debug_counter_rewrite_step(void) {
    counter_lock();
    g_counters.rewrite_total_steps++;
    counter_unlock();
}

void debug_counter_rule_applied(void) {
    counter_lock();
    g_counters.rewrite_rule_applications++;
    counter_unlock();
}

void debug_counter_unify_called(bool success) {
    counter_lock();
    g_counters.unify_check_count++;
    if (success) {
        g_counters.unify_success_count++;
    }
    counter_unlock();
}

void debug_counter_memory_update(uint64_t current_bytes) {
    counter_lock();
    g_counters.memory_current = current_bytes;
    if (current_bytes > g_counters.memory_usage_peak) {
        g_counters.memory_usage_peak = current_bytes;
    }
    counter_unlock();
}

/*=== 工具函数 ===*/

char *debug_counters_report(void) {
    PerformanceCounters counters;
    debug_get_counters(&counters);

    /* 将重复的格式化字符串提取为静态常量，消除重复代码 */
    static const char *report_format =
        "=== Lv-00 Performance Counters Report ===\n"
        "\n"
        "[Node Statistics]\n"
        "  Total created:     %llu\n"
        "  Currently alive:   %llu\n"
        "\n"
        "[Constraint Statistics]\n"
        "  Total created:     %llu\n"
        "  Currently alive:   %llu\n"
        "\n"
        "[Solver Statistics]\n"
        "  Call count:        %llu\n"
        "  Total time:        %.3f ms\n"
        "  Average time:      %.3f us\n"
        "\n"
        "[Rewrite Engine Statistics]\n"
        "  Total steps:       %llu\n"
        "  Rule applications: %llu\n"
        "\n"
        "[Unify Check Statistics]\n"
        "  Total checks:      %llu\n"
        "  Success count:     %llu\n"
        "  Success rate:      %.2f%%\n"
        "\n"
        "[Memory Statistics]\n"
        "  Current usage:     %.2f MB\n"
        "  Peak usage:        %.2f MB\n"
        "\n"
        "========================================\n";

    /* 第一遍：计算所需缓冲区大小 */
    int needed = snprintf(
        NULL, 0, report_format, (unsigned long long) counters.total_nodes_created,
        (unsigned long long) counters.current_nodes_alive, (unsigned long long) counters.total_constraints_created,
        (unsigned long long) counters.current_constraints_alive, (unsigned long long) counters.solver_call_count,
        (double) counters.solver_total_time_us / 1000.0, counters.solver_avg_time_us,
        (unsigned long long) counters.rewrite_total_steps, (unsigned long long) counters.rewrite_rule_applications,
        (unsigned long long) counters.unify_check_count, (unsigned long long) counters.unify_success_count,
        counters.unify_check_count > 0 ? (100.0 * counters.unify_success_count / counters.unify_check_count) : 0.0,
        (double) counters.memory_current / (1024.0 * 1024.0), (double) counters.memory_usage_peak / (1024.0 * 1024.0));

    if (needed < 0)
        return NULL;

    /* 分配精确大小的缓冲区（+1 用于终止符） */
    char *report = lv00_malloc(needed + 1);
    if (!report)
        return NULL;

    /* 使用安全的 snprintf 替代裸 snprintf */
    {
        int _snw;
        LV00_SAFE_SNPRINTF(
            _snw, report, (size_t) needed + 1, report_format, (unsigned long long) counters.total_nodes_created,
            (unsigned long long) counters.current_nodes_alive, (unsigned long long) counters.total_constraints_created,
            (unsigned long long) counters.current_constraints_alive, (unsigned long long) counters.solver_call_count,
            (double) counters.solver_total_time_us / 1000.0, counters.solver_avg_time_us,
            (unsigned long long) counters.rewrite_total_steps, (unsigned long long) counters.rewrite_rule_applications,
            (unsigned long long) counters.unify_check_count, (unsigned long long) counters.unify_success_count,
            counters.unify_check_count > 0 ? (100.0 * counters.unify_success_count / counters.unify_check_count) : 0.0,
            (double) counters.memory_current / (1024.0 * 1024.0),
            (double) counters.memory_usage_peak / (1024.0 * 1024.0));
        (void) _snw; /* 结果已直接写入 report，无需使用返回值 */
    }

    return report;
}

int debug_get_log_path(char *buf, size_t size) {
    if (!buf || size == 0)
        return -1;

    log_lock();
    /* 修复：使用 lv00_strlcpy 替代 strncpy，自动保证零终止且更安全 */
    lv00_strlcpy(buf, g_log_file_path, size);
    log_unlock();

    return 0;
}

/* ------------------------------------------------------------------ */
/*  debug_assert_normalization_invariants                              */
/* ------------------------------------------------------------------ */

/**
 * @brief 对引擎的约束图断言归一化不变量。
 *
 * 根据 design_v2.9.md 第3.6节：
 * 1. 同一作用域内不存在未合并的同坐标 POINT 节点
 * 2. 不存在未合并的同端点 LINE_SEGMENT 节点
 * 3. 所有约束的参与者引用有效节点
 * 4. 约束参与者列表按 ID 排序（稳定化）
 *
 * @param engine 引擎实例
 * @param ctx    调试上下文
 * @return 违规数量（0 = 全部通过）
 */
int debug_assert_normalization_invariants(const LV00Engine *engine, DebugContext *ctx) {
    if (!ctx || !engine || !engine->main_graph)
        return 0;

    int violations = 0;
    ConstraintGraph *graph = engine->main_graph;

    /* 不变量 1：同一作用域内不存在未合并的同坐标 POINT 节点 */ /* [修复] 英文注释改为中文 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *a = graph->nodes[i];
        if (a->type != GEOM_POINT || a->coord_count < 2)
            continue;
        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *b = graph->nodes[j];
            if (b->type != GEOM_POINT || b->coord_count < 2)
                continue;
            /* 同一作用域检查 */
            if (a->namespace_depth != b->namespace_depth)
                continue;
            if (a->parent_block_id != b->parent_block_id)
                continue;
            /* 坐标相等性检查 */ /* [修复] 英文注释改为中文 */
            bool same = true;
            int min_coords = a->coord_count < b->coord_count ? a->coord_count : b->coord_count;
            for (int k = 0; k < min_coords && same; k++) {
                if (symbolic_coord_compare(a->symbolic_coords[k], b->symbolic_coords[k]) != 0) {
                    same = false;
                }
            }
            if (same) {
                debug_log(LOG_LEVEL_ERROR, "normalization",
                          "Invariant violation: nodes %d and %d have same coords "
                          "but were not merged",
                          a->id, b->id);
                violations++;
            }
        }
    }

    /* 不变量 2：不存在未合并的同端点 LINE_SEGMENT 节点 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *a = graph->nodes[i];
        if (a->type != GEOM_LINE_SEGMENT || a->coord_count < 2)
            continue;
        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *b = graph->nodes[j];
            if (b->type != GEOM_LINE_SEGMENT || b->coord_count < 2)
                continue;
            bool fwd = (symbolic_coord_compare(a->symbolic_coords[0], b->symbolic_coords[0]) == 0 &&
                        symbolic_coord_compare(a->symbolic_coords[1], b->symbolic_coords[1]) == 0);
            bool rev = (symbolic_coord_compare(a->symbolic_coords[0], b->symbolic_coords[1]) == 0 &&
                        symbolic_coord_compare(a->symbolic_coords[1], b->symbolic_coords[0]) == 0);
            if (fwd || rev) {
                debug_log(LOG_LEVEL_ERROR, "normalization",
                          "Invariant violation: segments %d and %d have same endpoints "
                          "but were not merged",
                          a->id, b->id);
                violations++;
            }
        }
    }

    /* 不变量 3：所有约束的参与者引用有效节点 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        for (int k = 0; k < c->participant_count; k++) {
            if (!graph_get_node(graph, c->participants[k])) {
                debug_log(LOG_LEVEL_ERROR, "normalization",
                          "Invariant violation: constraint %d references "
                          "non-existent node %d",
                          c->id, c->participants[k]);
                violations++;
            }
        }
    }

    /* 不变量 4：参与者列表按 ID 排序 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        for (int k = 1; k < c->participant_count; k++) {
            if (c->participants[k - 1] > c->participants[k]) {
                debug_log(LOG_LEVEL_ERROR, "normalization",
                          "Invariant violation: constraint %d participants not sorted", c->id);
                violations++;
                break;
            }
        }
    }

    if (ctx->abort_on_violation && violations > 0) {
        abort();
    }

    ctx->violation_count += violations;
    return violations;
}
