/**
 * @file debug.h
 * @brief 调试子系统 —— 日志、性能计数器、断言、内存池与追踪
 * @details 提供分级日志系统（DEBUG/INFO/WARN/ERROR）、全局性能计数器、
 * 归一化/端口不变量断言、引用计数与垃圾回收、内存池、紧急保存机制
 * 以及归一化/重写/求解的追踪会话。
 */

#ifndef lv_DEBUG_H
#define lv_DEBUG_H

/* lv_PUBLIC_API 权威定义位于 lv.h（含 lv_USE_SHARED→dllimport 处理），
 * lv_THREAD_LOCAL 权威定义位于 cross_platform.h。
 * 此处 include lv_api_spec.h 获取 lv_PUBLIC_API 防御性兜底（独立包含本头时为空），
 * 避免重复定义造成语义分叉。 */
#include "lv_api_spec.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "lv/lv_ringbuf.h"

/* [C5 修复] 使用前向声明替代完整包含 engine.h，
 * 减少编译依赖链。仅需要 lvEngine 和 ConstraintGraph 指针类型。 */
struct lvEngine;
typedef struct lvEngine lvEngine;
struct ConstraintGraph;
typedef struct ConstraintGraph ConstraintGraph;

/* 版本宏统一定义在 lv.h 中，通过 engine.h 间接包含 */
/* 如果因某些原因未定义，使用编译期拼接回退 */
/* 【版本权威源统一】fallback 值与 lv.h 的 lv_VERSION_MAJOR/MINOR/PATCH（1.1.0）保持一致，
 * 避免 include 顺序不同导致版本字符串漂移 */
#ifndef lv_VERSION_STRING
#define lv_VERSION_STRING_EXPAND_(maj, min, pat) #maj "." #min "." #pat
#define lv_VERSION_STRING_MACRO_(maj, min, pat) lv_VERSION_STRING_EXPAND_(maj, min, pat)
#define lv_VERSION_STRING lv_VERSION_STRING_MACRO_(1, 1, 0)
#endif

#define lv_NAME "Lv-00 Geometry Metalanguage"

/* 日志轮转设置 */
#ifndef lv_LOG_MAX_FILES
#define lv_LOG_MAX_FILES 5
#endif
#ifndef lv_LOG_MAX_SIZE
#define lv_LOG_MAX_SIZE (10 * 1024 * 1024) /* 10MB */
#endif
#ifndef lv_LOG_PATH_MAX
#define lv_LOG_PATH_MAX 256
#endif

/* 日志级别: TRACE < DEBUG < INFO < WARN < ERROR < FATAL
 *
 * 【v3.5.0 统一】lv_internal.h 的 lv_LOG_* 宏通过 lv_log_message()
 * 委托到 debug_log()，所有日志最终汇入本模块的统一管道。
 * lv_LOG_LEVEL_* 常量与 LogLevel 枚举通过 lv_log_map_level() 映射。
 *
 * 【v3.3.0 增强】新增 TRACE 和 FATAL 级别：
 *   TRACE — 最细粒度，记录函数进入/退出、参数值、循环迭代（极大量）
 *   FATAL — 不可恢复错误，记录后触发 emergency_save 和可能的 abort
 */
/* 若 runtime_monitor.h 已定义 lvLogLevel 枚举（其哨兵宏 lv_RUNTIME_MONITOR_LOGLEVEL_SEEN 已定义），
 * 则 LogLevel 直接复用 lvLogLevel，避免枚举成员与 LOG_LEVEL_* 宏冲突。 */
#ifndef lv_RUNTIME_MONITOR_LOGLEVEL_SEEN
typedef enum {
    LOG_LEVEL_TRACE = -1, /**< 追踪级别：最详细的逐步骤日志（函数进入/退出、参数转储） */
    LOG_LEVEL_DEBUG = 0, /**< 调试级别：开发调试信息 */
    LOG_LEVEL_INFO = 1, /**< 信息级别：常规运行时信息 */
    LOG_LEVEL_WARN = 2, /**< 警告级别：潜在问题，不影响当前操作 */
    LOG_LEVEL_ERROR = 3, /**< 错误级别：操作失败，但引擎可继续 */
    LOG_LEVEL_FATAL = 4, /**< 致命级别：不可恢复错误，记录后触发保护性动作 */
    LOG_LEVEL_NONE = 5 /**< 禁用所有日志 */
} LogLevel;
#define lv_LOGLEVEL_DEFINED 1
#else
typedef lvLogLevel LogLevel;
#endif

/**
 * @brief 编译期日志级别过滤 —— 零运行时开销
 *
 * 在 CMakeLists.txt 中定义此宏来控制编译期最低日志级别。
 * 低于此级别的日志调用在编译期被完全剔除，不产生任何代码。
 *
 * 用法示例：
 *   cmake -Dlv_LOG_LEVEL_GUARD=LOG_LEVEL_WARN ..
 *   // TRACE, DEBUG, INFO 日志调用被编译期移除
 *
 * 默认值：不定义（所有级别在运行时决定）
 */
#ifdef lv_LOG_GUARD
/* lv_LOG_GUARD 由 CMake 传入，值如 LOG_LEVEL_WARN */
#define lv_LOG_IS_ENABLED(level) ((level) >= (lv_LOG_GUARD))
#else
/* 未定义时所有级别在运行时决定 */
#define lv_LOG_IS_ENABLED(level) true
#endif /* lv_LOG_GUARD */

/* 性能计数器（第18.5节） */
typedef struct PerformanceCounters {
    /* 节点统计 */
    uint64_t total_nodes_created; /* 创建的节点总数 */
    uint64_t current_nodes_alive; /* 当前存活节点数 */

    /* 约束统计 */
    uint64_t total_constraints_created; /* 创建的约束总数 */
    uint64_t current_constraints_alive; /* 当前存活约束数 */

    /* 求解器统计 */
    uint64_t solver_call_count;    /* 求解器调用次数 */
    uint64_t solver_total_time_us; /* 总耗时（微秒） */
    double solver_avg_time_us;     /* 平均耗时（微秒） */

    /* 重写引擎统计 */
    uint64_t rewrite_total_steps;       /* 重写总步数 */
    uint64_t rewrite_rule_applications; /* 规则应用次数 */

    /* 合一检查统计 */
    uint64_t unify_check_count;   /* 合一检查次数 */
    uint64_t unify_success_count; /* 合一成功次数 */

    /* 内存统计 */
    uint64_t memory_usage_peak; /* 内存使用峰值 */
    uint64_t memory_current;    /* 当前内存使用量 */
} PerformanceCounters;

/* 调试上下文，用于断言和追踪 */
typedef struct DebugContext {
    bool port_invariant_checks;
    bool abort_on_violation;
    int violation_count;
} DebugContext;

/*=== 调试上下文函数 ===*/
lv_PUBLIC_API DebugContext *debug_context_create(void);
lv_PUBLIC_API void debug_context_destroy(DebugContext *ctx);

/*=== 端口不变量断言 ===*/
lv_PUBLIC_API int debug_assert_port_invariants(const lvEngine *engine, DebugContext *ctx);

/*=== 遗留日志函数（向后兼容） ===*/
lv_PUBLIC_API void debug_log_rewrite(const char *fmt, ...);

/*=== 新日志系统 ===*/

/**
 * @brief 初始化日志系统。
 * 如需要则创建日志目录，打开日志文件。
 * @return 成功返回 0，失败返回负值
 */
lv_PUBLIC_API int debug_log_init(void);

/**
 * @brief 清理日志系统（轻量重置）。
 * 重置日志系统状态变量到零值，但不关闭日志文件、不释放已打开的资源。
 * 适用于需要在日志系统保持可用的情况下清空状态的场景。
 *
 * @note 与 debug_log_shutdown() 的区别：
 *       - debug_log_cleanup()  仅重置内部状态变量（不关闭文件/释放资源）；
 *       - debug_log_shutdown() 完整关闭日志系统（关闭文件并释放资源）。
 *       两者是不同粒度的收尾操作，可独立调用；完整退出时应使用
 *       debug_log_shutdown()（该函数已被 lv_init/lv_cleanup 的模块
 *       生命周期管理注册为日志模块的关闭回调）。
 */
lv_PUBLIC_API void debug_log_cleanup(void);

/**
 * @brief 关闭日志系统（完整关闭）。
 * 关闭日志文件并释放资源，日志系统在调用后不再可用（需重新 init）。
 *
 * @note 与 debug_log_cleanup() 的区别见 debug_log_cleanup() 的文档说明。
 *       模块生命周期：lv_init() 通过 lv_module_register("log", ...,
 *       debug_log_shutdown, ...) 将其注册为日志模块的关闭回调，
 *       lv_cleanup() 会经模块注册表统一调用本函数。
 */
lv_PUBLIC_API void debug_log_shutdown(void);

/**
 * @brief 设置当前日志级别。
 * @param level 最低日志级别
 */
lv_PUBLIC_API void debug_set_log_level(LogLevel level);

/**
 * @brief 获取当前日志级别。
 * @return 当前日志级别
 */
lv_PUBLIC_API LogLevel debug_get_log_level(void);

/**
 * @brief 设置调试模式（记录所有级别，包括 DEBUG）。
 * @param debug_mode true 启用调试模式，false 恢复普通模式
 */
lv_PUBLIC_API void debug_set_mode(bool debug_mode);

/**
 * @brief 检查调试模式是否启用。
 * @return 调试模式启用时返回 true
 */
lv_PUBLIC_API bool debug_is_debug_mode(void);

/**
 * @brief 核心日志函数，支持级别和模块参数。
 * @param level  此消息的日志级别
 * @param module 模块名称（如 "solver"、"rewrite"、"unify"）
 * @param fmt    printf 风格的格式化字符串
 * @param ...    格式化参数
 */
lv_PUBLIC_API void debug_log(LogLevel level, const char *module, const char *fmt, ...);

/**
 * @brief 各级别日志的便捷宏。
 *
 * 【v3.3.0 增强】
 *   - 新增 LOG_TRACE / LOG_FATAL 宏
 *   - LOG_GUARDED 变体在编译期过滤低于阈值的日志调用（零开销）
 *   - lv_LOG_GUARD 通过 CMake 定义，默认不定义
 */
#ifdef lv_LOG_GUARD
/* 编译期过滤版本：低于阈值的日志被编译期移除，不产生任何代码 */
#define LOG_TRACE(module, fmt, ...)                                 \
    do {                                                            \
        if (lv_LOG_IS_ENABLED(LOG_LEVEL_TRACE))                     \
            debug_log(LOG_LEVEL_TRACE, module, fmt, ##__VA_ARGS__); \
    } while (0)
#define LOG_DEBUG(module, fmt, ...)                                 \
    do {                                                            \
        if (lv_LOG_IS_ENABLED(LOG_LEVEL_DEBUG))                     \
            debug_log(LOG_LEVEL_DEBUG, module, fmt, ##__VA_ARGS__); \
    } while (0)
#define LOG_INFO(module, fmt, ...)                                 \
    do {                                                           \
        if (lv_LOG_IS_ENABLED(LOG_LEVEL_INFO))                     \
            debug_log(LOG_LEVEL_INFO, module, fmt, ##__VA_ARGS__); \
    } while (0)
#define LOG_WARN(module, fmt, ...)                                 \
    do {                                                           \
        if (lv_LOG_IS_ENABLED(LOG_LEVEL_WARN))                     \
            debug_log(LOG_LEVEL_WARN, module, fmt, ##__VA_ARGS__); \
    } while (0)
#define LOG_ERROR(module, fmt, ...)                                 \
    do {                                                            \
        if (lv_LOG_IS_ENABLED(LOG_LEVEL_ERROR))                     \
            debug_log(LOG_LEVEL_ERROR, module, fmt, ##__VA_ARGS__); \
    } while (0)
#define LOG_FATAL(module, fmt, ...)                                 \
    do {                                                            \
        if (lv_LOG_IS_ENABLED(LOG_LEVEL_FATAL))                     \
            debug_log(LOG_LEVEL_FATAL, module, fmt, ##__VA_ARGS__); \
    } while (0)
#else
/* 无编译期过滤：所有级别在运行时由 debug_set_log_level 决定 */
#define LOG_TRACE(module, fmt, ...) debug_log(LOG_LEVEL_TRACE, module, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(module, fmt, ...) debug_log(LOG_LEVEL_DEBUG, module, fmt, ##__VA_ARGS__)
#define LOG_INFO(module, fmt, ...) debug_log(LOG_LEVEL_INFO, module, fmt, ##__VA_ARGS__)
#define LOG_WARN(module, fmt, ...) debug_log(LOG_LEVEL_WARN, module, fmt, ##__VA_ARGS__)
#define LOG_ERROR(module, fmt, ...) debug_log(LOG_LEVEL_ERROR, module, fmt, ##__VA_ARGS__)
#define LOG_FATAL(module, fmt, ...) debug_log(LOG_LEVEL_FATAL, module, fmt, ##__VA_ARGS__)
#endif /* lv_LOG_GUARD */

/* ============================================================
 * 结构化日志与环形缓冲区（v3.3.0 新增）
 * ============================================================ */

/**
 * @brief 环形日志缓冲区默认容量
 *
 * 存储最近 N 条日志消息，用于崩溃后诊断。
 * 可通过 debug_set_ring_buffer_capacity() 调整。
 */
#define lv_LOG_RING_BUFFER_DEFAULT_CAPACITY 256

/**
 * @brief 单条结构化日志记录
 *
 * 每条日志记录包含完整的上下文信息，用于：
 * - 崩溃后诊断（环形缓冲区可紧急保存）
 * - 日志分析工具的解析（结构化字段）
 * - 性能分析（记录时间戳和耗时）
 */

/** 结构化日志的时间戳字符串长度（"YYYY-MM-DD HH:MM:SS"） */
#define lv_DEBUG_TIMESTAMP_STR_LEN 32

typedef struct lvLogEntry {
    LogLevel level;            /**< 日志级别 */
    uint64_t timestamp_us;     /**< 时间戳（微秒精度，平台相关时钟） */
    char timestamp_str[lv_DEBUG_TIMESTAMP_STR_LEN]; /**< 格式化的墙钟时间戳（YYYY-MM-DD HH:MM:SS） */
    const char *module_name;   /**< 模块名称（如 "solver", "engine", "graph"） */
    const char *function_name; /**< 函数名称（__func__） */
    int line_number;           /**< 源文件行号（__LINE__） */
    const char *file_name;     /**< 源文件名（__FILE__） */
    char message[512];         /**< 格式化后的日志消息（定长，防止 OOM） */
    uint64_t context_id;       /**< 关联的上下文 ID（0 = 全局日志） */
} lvLogEntry;

/**
 * @brief 环形日志缓冲区
 *
 * 固定容量的环形缓冲区，新条目覆盖最旧的条目。
 * 线程安全（内部使用互斥锁保护）。
 *
 * 使用场景：
 * - 崩溃后通过 emergency_save 导出最近日志
 * - 调试时查询最近操作序列
 * - 性能敏感场景下的轻量级日志存储
 *
 * @note 底层使用泛型 lvRingBuf 实现。
 *       lvLogRingBuffer 是围绕 lvRingBuf 的日志专用薄封装。
 */
typedef struct lvLogRingBuffer {
    lvRingBuf base; /**< 泛型环形缓冲区基类 */
} lvLogRingBuffer;

/**
 * @brief 创建环形日志缓冲区
 * @param capacity 缓冲区容量（条目数，至少为 1；默认 256）
 * @return 新分配的环形缓冲区，失败返回 NULL
 */
lv_PUBLIC_API lvLogRingBuffer *lv_log_ring_buffer_create(int capacity);

/**
 * @brief 销毁环形日志缓冲区
 * @param rb 缓冲区指针（可为 NULL）
 */
lv_PUBLIC_API void lv_log_ring_buffer_destroy(lvLogRingBuffer *rb);

/**
 * @brief 向环形缓冲区写入一条结构化日志
 *
 * 线程安全。如果缓冲区已满，覆盖最旧的条目。
 *
 * @param rb            环形缓冲区（非 NULL）
 * @param level         日志级别
 * @param module_name   模块名称
 * @param function_name 函数名称（__func__）
 * @param file_name     文件名（__FILE__）
 * @param line_number   行号（__LINE__）
 * @param fmt           printf 格式字符串
 * @param ...           格式参数
 */
lv_PUBLIC_API void lv_log_ring_buffer_write(lvLogRingBuffer *rb, LogLevel level, const char *module_name,
                                            const char *function_name, const char *file_name, int line_number,
                                            const char *fmt, ...);

/**
 * @brief 导出环形缓冲区中的所有日志（按时间顺序）
 *
 * 返回的数组由[take] 调用者负责释放（使用 lv_free）。
 *
 * @param rb           环形缓冲区（非 NULL）
 * @param out_count    输出：实际导出的条目数量
 * @return 日志条目数组（按插入时间排序），失败返回 NULL
 */
lv_PUBLIC_API lvLogEntry *lv_log_ring_buffer_export(const lvLogRingBuffer *rb, int *out_count);

/**
 * @brief 清空环形缓冲区中的所有日志条目
 * @param rb 环形缓冲区（非 NULL）
 */
lv_PUBLIC_API void lv_log_ring_buffer_clear(lvLogRingBuffer *rb);

/**
 * @brief 设置环形缓冲区容量（保留现有条目，最多保留新容量条）
 *
 * 如果新容量小于当前条目数，最旧的多余条目将被丢弃。
 *
 * @param rb       环形缓冲区（非 NULL）
 * @param capacity 新容量（>= 1）
 * @return true 成功，false 失败（内存不足）
 */
lv_PUBLIC_API bool lv_log_ring_buffer_resize(lvLogRingBuffer *rb, int capacity);

/* ============================================================
 * 带上下文的日志函数（v3.3.0 新增）
 *
 * lv_log_with_context() 是结构化日志的核心 API。
 * 与简单的 debug_log() 相比，它额外记录：
 *   - 上下文 ID（用于多上下文场景的日志追踪）
 *   - 函数名 / 文件名 / 行号（用于精确的故障定位）
 *   - 自动写入环形缓冲区
 *
 * 使用 lv_LOG_CTX() 便捷宏，自动填入 __func__, __FILE__, __LINE__。
 * ============================================================ */

/**
 * @brief 带上下文的日志记录函数
 *
 * 同时执行以下操作：
 * 1. 通过 debug_log() 写入标准日志流（级别过滤、文件输出等）
 * 2. 写入上下文关联的环形缓冲区（如果启用）
 * 3. FATAL 级别时触发紧急保存
 *
 * @param ctx           上下文指针（可为 NULL，此时仅写入全局日志）
 * @param level         日志级别
 * @param module_name   模块名称（如 "solver", "engine"）
 * @param function_name 函数名称（__func__）
 * @param file_name     源文件名（__FILE__）
 * @param line_number   行号（__LINE__）
 * @param fmt           printf 格式字符串
 * @param ...           格式参数
 */
struct lvContext; /* 前向声明 */
lv_PUBLIC_API void lv_log_with_context(struct lvContext *ctx, LogLevel level, const char *module_name,
                                       const char *function_name, const char *file_name, int line_number,
                                       const char *fmt, ...);

/**
 * @brief 带上下文的便捷日志宏 —— 自动填入位置信息
 *
 * 使用示例：
 * @code
 *   lv_LOG_CTX(ctx, LOG_LEVEL_WARN, "solver",
 *                "约束 %d 的变量 %d 超出范围 [%d, %d]",
 *                constraint_id, var_id, min_val, max_val);
 * @endcode
 */
#define lv_LOG_CTX(ctx, level, module, fmt, ...) \
    lv_log_with_context((ctx), (level), (module), __func__, __FILE__, __LINE__, (fmt), ##__VA_ARGS__)

/*=== 性能计数器 ===*/

/**
 * @brief 获取全局性能计数器。
 * 返回当前计数器值的快照。
 * @param counters 用于存储计数器值的指针
 */
lv_PUBLIC_API void debug_get_counters(PerformanceCounters *counters);

/**
 * @brief 将所有性能计数器重置为零。
 */
lv_PUBLIC_API void debug_reset_counters(void);

/* 计数器递增函数（由其他模块调用） */
lv_PUBLIC_API void debug_counter_node_created(void);
lv_PUBLIC_API void debug_counter_node_destroyed(void);
lv_PUBLIC_API void debug_counter_constraint_created(void);
lv_PUBLIC_API void debug_counter_constraint_destroyed(void);
lv_PUBLIC_API void debug_counter_solver_called(uint64_t time_us);
lv_PUBLIC_API void debug_counter_rewrite_step(void);
lv_PUBLIC_API void debug_counter_rule_applied(void);
lv_PUBLIC_API void debug_counter_unify_called(bool success);
lv_PUBLIC_API void debug_counter_memory_update(uint64_t current_bytes);

/*=== 工具函数 ===*/

/**
 * @brief 将性能计数器格式化为可读字符串。
 * 调用者须负责释放返回的字符串。
 * @return 新分配的包含计数器报告的字符串
 */
lv_PUBLIC_API char *debug_counters_report(void);

/**
 * @brief 获取日志文件路径。
 * @param buf  存储路径的缓冲区
 * @param size 缓冲区大小
 * @return 成功返回 0，失败返回负值
 */
lv_PUBLIC_API int debug_get_log_path(char *buf, size_t size);

/*=== 归一化不变量断言 ===*/

/**
 * @brief 对引擎的约束图断言归一化不变量。
 * 返回违规数量（0 = 全部通过）。
 */
lv_PUBLIC_API int debug_assert_normalization_invariants(const lvEngine *engine, DebugContext *ctx);

/*=== 内存池 ===*/

/**
 * @brief 内存池 —— 用于频繁分配/释放的小对象内存管理器
 */
typedef struct lvMemPool lvMemPool;
/* 向后兼容别名 */
#define MemPool lvMemPool

/**
 * @brief 创建内存池
 * @param block_size  每个块的大小（字节）
 * @param initial_blocks  初始块数量
 */
lv_PUBLIC_API MemPool *mem_pool_create(size_t block_size, int initial_blocks);

/**
 * @brief 从内存池分配一个块
 */
lv_PUBLIC_API void *mem_pool_alloc(MemPool *pool);

/**
 * @brief 将块归还内存池
 */
lv_PUBLIC_API void mem_pool_free(MemPool *pool, void *block);

/**
 * @brief 销毁内存池
 */
lv_PUBLIC_API void mem_pool_destroy(MemPool *pool);

/**
 * @brief 获取内存池统计信息
 * @param total_bytes 使用 size_t* 类型，避免大内存池场景下 int 截断
 */
lv_PUBLIC_API void mem_pool_stats(const MemPool *pool, int *total_blocks, int *free_blocks, size_t *total_bytes);

/*=== 引用计数与垃圾回收 ===*/

/**
 * @brief 引用计数对象基类 —— 提供自动引用计数和析构回调机制
 */
typedef struct lvRefCounted {
    int ref_count;
    void (*destructor)(void *self);
} lvRefCounted;
/* 向后兼容别名 */
#define RefCounted lvRefCounted

/**
 * @brief 增加引用计数
 */
lv_PUBLIC_API void ref_count_inc(void *obj);

/**
 * @brief 减少引用计数，到0时自动销毁
 */
lv_PUBLIC_API bool ref_count_dec(void *obj);

/**
 * @brief 获取当前引用计数
 */
lv_PUBLIC_API int ref_count_get(const void *obj);

/*=== 紧急保存 ===*/

/**
 * @brief 紧急保存——在崩溃/异常时保存调试信息
 *
 * 将当前引擎状态、性能计数器、日志缓冲区等保存到文件。
 */
typedef struct {
    char *filepath;          /* 保存路径 */
    bool include_graph;      /* 是否包含约束图快照 */
    bool include_counters;   /* 是否包含性能计数器 */
    bool include_log_buffer; /* 是否包含日志缓冲区 */
    bool include_memory_map; /* 是否包含内存分配映射 */
} EmergencySaveConfig;

/**
 * @brief 执行紧急保存
 */
lv_PUBLIC_API bool debug_emergency_save(const char *filepath, const EmergencySaveConfig *config);

/**
 * @brief 设置紧急保存回调（在信号处理程序中调用）
 */
typedef void (*EmergencySaveHandler)(const char *reason);

lv_PUBLIC_API void debug_set_emergency_handler(EmergencySaveHandler handler);

/*=== 端口不变量断言（完整版） ===*/

/**
 * @brief 端口不变量检查结果
 */
typedef struct {
    bool all_valid;
    int total_ports;
    int invalid_ports;
    int *invalid_port_ids;
    char **error_messages;
} PortInvariantResult;

/**
 * @brief 执行完整的端口不变量检查
 *
 * 检查所有端口的不变量：
 * 1. INPUT 端口的 namespace_depth <= 父函数块的 namespace_depth
 * 2. OUTPUT 端口的 namespace_depth <= 父函数块的 namespace_depth
 * 3. 端口连接的对方节点存在
 * 4. 端口的类型区域与连接节点的类型兼容
 */
lv_PUBLIC_API PortInvariantResult *debug_check_port_invariants(const ConstraintGraph *graph);

/**
 * @brief 销毁端口不变量检查结果
 */
lv_PUBLIC_API void debug_port_invariant_result_destroy(PortInvariantResult *result);

/*=== 归一化/重写/求解追踪 ===*/

/**
 * @brief 追踪事件类型
 */
typedef enum { TRACE_NORMALIZATION, TRACE_REWRITE, TRACE_SOLVER } TraceEventType;

/**
 * @brief 追踪事件
 */
typedef struct {
    TraceEventType type;
    double timestamp;
    int step_number;
    char *description;
    char *details; /* JSON 格式的详细信息 */
} TraceEvent;

/**
 * @brief 追踪会话 —— 记录归一化/重写/求解过程中的事件序列
 */
typedef struct lvTraceSession {
    TraceEvent *events;
    int event_count;
    int capacity;
    bool active;
} lvTraceSession;
/* 向后兼容别名 */
#define TraceSession lvTraceSession

/**
 * @brief 创建追踪会话
 */
lv_PUBLIC_API TraceSession *trace_session_create(void);

/**
 * @brief 销毁追踪会话
 */
lv_PUBLIC_API void trace_session_destroy(TraceSession *session);

/**
 * @brief 记录追踪事件
 */
lv_PUBLIC_API void trace_record_event(TraceSession *session, TraceEventType type, int step, const char *description,
                                      const char *details);

/**
 * @brief 导出追踪会话为 JSON
 */
lv_PUBLIC_API char *trace_session_export_json(const TraceSession *session);

/**
 * @brief 获取全局追踪会话
 */
lv_PUBLIC_API TraceSession *debug_get_trace_session(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_DEBUG_H */
