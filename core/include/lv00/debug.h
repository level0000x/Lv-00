/**
 * @file debug.h
 * @brief 调试子系统 —— 日志、性能计数器、断言、内存池与追踪
 * @details 提供分级日志系统（DEBUG/INFO/WARN/ERROR）、全局性能计数器、
 * 归一化/端口不变量断言、引用计数与垃圾回收、内存池、紧急保存机制
 * 以及归一化/重写/求解的追踪会话。
 */

#ifndef LV00_DEBUG_H
#define LV00_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* [C5 修复] 使用前向声明替代完整包含 engine.h，
 * 减少编译依赖链。仅需要 LV00Engine 和 ConstraintGraph 指针类型。 */
struct LV00Engine;
typedef struct LV00Engine LV00Engine;
struct ConstraintGraph;
typedef struct ConstraintGraph ConstraintGraph;

/* 版本宏统一定义在 lv00.h 中，通过 engine.h 间接包含 */
/* 如果因某些原因未定义，使用编译期拼接回退 */
#ifndef LV00_VERSION_STRING
#define LV00_VERSION_STRING_EXPAND_(maj, min, pat) #maj "." #min "." #pat
#define LV00_VERSION_STRING_MACRO_(maj, min, pat) LV00_VERSION_STRING_EXPAND_(maj, min, pat)
#define LV00_VERSION_STRING LV00_VERSION_STRING_MACRO_(3, 3, 0)
#endif

#define LV00_NAME "Lv-00 Geometry Metalanguage"

/* 日志轮转设置 */
#define LV00_LOG_MAX_FILES 5
#define LV00_LOG_MAX_SIZE (10 * 1024 * 1024) /* 10MB */
#define LV00_LOG_PATH_MAX 256

/* 日志级别: TRACE < DEBUG < INFO < WARN < ERROR < FATAL
 *
 * 注意：lv00_internal.h 中定义了另一套日志级别宏（LV00_LOG_LEVEL_*），
 * 用于内部日志函数 lv00_log_message()。该套宏为项目级权威定义。
 * 此处 LogLevel 枚举专用于 debug.h 的日志子系统 API
 *（debug_log / debug_set_log_level 等），两套系统相互独立。
 * 若需修改日志级别语义，请同步检查 lv00_internal.h 中的定义。
 *
 * 【v3.3.0 增强】新增 TRACE 和 FATAL 级别：
 *   TRACE — 最细粒度，记录函数进入/退出、参数值、循环迭代（极大量）
 *   FATAL — 不可恢复错误，记录后触发 emergency_save 和可能的 abort
 */
typedef enum {
#ifndef LOG_LEVEL_TRACE
    LOG_LEVEL_TRACE = -1, /**< 追踪级别：最详细的逐步骤日志（函数进入/退出、参数转储） */
#endif
#ifndef LOG_LEVEL_DEBUG
    LOG_LEVEL_DEBUG = 0,  /**< 调试级别：开发调试信息 */
#endif
#ifndef LOG_LEVEL_INFO
    LOG_LEVEL_INFO  = 1,  /**< 信息级别：常规运行时信息 */
#endif
#ifndef LOG_LEVEL_WARN
    LOG_LEVEL_WARN  = 2,  /**< 警告级别：潜在问题，不影响当前操作 */
#endif
#ifndef LOG_LEVEL_ERROR
    LOG_LEVEL_ERROR = 3,  /**< 错误级别：操作失败，但引擎可继续 */
#endif
#ifndef LOG_LEVEL_FATAL
    LOG_LEVEL_FATAL = 4,  /**< 致命级别：不可恢复错误，记录后触发保护性动作 */
#endif
#ifndef LOG_LEVEL_NONE
    LOG_LEVEL_NONE  = 5   /**< 禁用所有日志 */
#endif
} LogLevel;

/**
 * @brief 编译期日志级别过滤 —— 零运行时开销
 *
 * 在 CMakeLists.txt 中定义此宏来控制编译期最低日志级别。
 * 低于此级别的日志调用在编译期被完全剔除，不产生任何代码。
 *
 * 用法示例：
 *   cmake -DLV00_LOG_LEVEL_GUARD=LOG_LEVEL_WARN ..
 *   // TRACE, DEBUG, INFO 日志调用被编译期移除
 *
 * 默认值：不定义（所有级别在运行时决定）
 */
#ifdef LV00_LOG_GUARD
/* LV00_LOG_GUARD 由 CMake 传入，值如 LOG_LEVEL_WARN */
#define LV00_LOG_IS_ENABLED(level) ((level) >= (LV00_LOG_GUARD))
#else
/* 未定义时所有级别在运行时决定 */
#define LV00_LOG_IS_ENABLED(level) true
#endif /* LV00_LOG_GUARD */

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
    bool normalization_assertions;
    bool port_invariant_checks;
    bool rewrite_trace;
    bool solver_trace;
    bool abort_on_violation;
    int violation_count;
} DebugContext;

/*=== 调试上下文函数 ===*/
LV00_PUBLIC_API DebugContext *debug_context_create(void);
LV00_PUBLIC_API void debug_context_destroy(DebugContext *ctx);

/*=== 端口不变量断言 ===*/
LV00_PUBLIC_API int debug_assert_port_invariants(const LV00Engine *engine, DebugContext *ctx);

/*=== 遗留日志函数（向后兼容） ===*/
LV00_PUBLIC_API void debug_log_normalization(const char *fmt, ...);
LV00_PUBLIC_API void debug_log_rewrite(const char *fmt, ...);
LV00_PUBLIC_API void debug_log_solver(const char *fmt, ...);

/*=== 新日志系统 ===*/

/**
 * @brief 初始化日志系统。
 * 如需要则创建日志目录，打开日志文件。
 * @return 成功返回 0，失败返回负值
 */
LV00_PUBLIC_API int debug_log_init(void);

/**
 * @brief 关闭日志系统。
 * 关闭日志文件并释放资源。
 */
LV00_PUBLIC_API void debug_log_shutdown(void);

/**
 * @brief 设置当前日志级别。
 * @param level 最低日志级别
 */
LV00_PUBLIC_API void debug_set_log_level(LogLevel level);

/**
 * @brief 获取当前日志级别。
 * @return 当前日志级别
 */
LV00_PUBLIC_API LogLevel debug_get_log_level(void);

/**
 * @brief 设置调试模式（记录所有级别，包括 DEBUG）。
 * @param debug_mode true 启用调试模式，false 恢复普通模式
 */
LV00_PUBLIC_API void debug_set_mode(bool debug_mode);

/**
 * @brief 检查调试模式是否启用。
 * @return 调试模式启用时返回 true
 */
LV00_PUBLIC_API bool debug_is_debug_mode(void);

/**
 * @brief 核心日志函数，支持级别和模块参数。
 * @param level  此消息的日志级别
 * @param module 模块名称（如 "solver"、"rewrite"、"unify"）
 * @param fmt    printf 风格的格式化字符串
 * @param ...    格式化参数
 */
LV00_PUBLIC_API void debug_log(LogLevel level, const char *module, const char *fmt, ...);

/**
 * @brief 各级别日志的便捷宏。
 *
 * 【v3.3.0 增强】
 *   - 新增 LOG_TRACE / LOG_FATAL 宏
 *   - LOG_GUARDED 变体在编译期过滤低于阈值的日志调用（零开销）
 *   - LV00_LOG_GUARD 通过 CMake 定义，默认不定义
 */
#ifdef LV00_LOG_GUARD
/* 编译期过滤版本：低于阈值的日志被编译期移除，不产生任何代码 */
#define LOG_TRACE(module, fmt, ...)  do { \
    if (LV00_LOG_IS_ENABLED(LOG_LEVEL_TRACE)) \
        debug_log(LOG_LEVEL_TRACE, module, fmt, ##__VA_ARGS__); \
} while(0)
#define LOG_DEBUG(module, fmt, ...)  do { if (LV00_LOG_IS_ENABLED(LOG_LEVEL_DEBUG)) debug_log(LOG_LEVEL_DEBUG, module, fmt, ##__VA_ARGS__); } while(0)
#define LOG_INFO(module, fmt, ...)   do { if (LV00_LOG_IS_ENABLED(LOG_LEVEL_INFO))  debug_log(LOG_LEVEL_INFO,  module, fmt, ##__VA_ARGS__); } while(0)
#define LOG_WARN(module, fmt, ...)   do { if (LV00_LOG_IS_ENABLED(LOG_LEVEL_WARN))  debug_log(LOG_LEVEL_WARN,  module, fmt, ##__VA_ARGS__); } while(0)
#define LOG_ERROR(module, fmt, ...)  do { if (LV00_LOG_IS_ENABLED(LOG_LEVEL_ERROR)) debug_log(LOG_LEVEL_ERROR, module, fmt, ##__VA_ARGS__); } while(0)
#define LOG_FATAL(module, fmt, ...)  do { if (LV00_LOG_IS_ENABLED(LOG_LEVEL_FATAL)) debug_log(LOG_LEVEL_FATAL, module, fmt, ##__VA_ARGS__); } while(0)
#else
/* 无编译期过滤：所有级别在运行时由 debug_set_log_level 决定 */
#define LOG_TRACE(module, fmt, ...)  debug_log(LOG_LEVEL_TRACE, module, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(module, fmt, ...)  debug_log(LOG_LEVEL_DEBUG, module, fmt, ##__VA_ARGS__)
#define LOG_INFO(module, fmt, ...)   debug_log(LOG_LEVEL_INFO,  module, fmt, ##__VA_ARGS__)
#define LOG_WARN(module, fmt, ...)   debug_log(LOG_LEVEL_WARN,  module, fmt, ##__VA_ARGS__)
#define LOG_ERROR(module, fmt, ...)  debug_log(LOG_LEVEL_ERROR, module, fmt, ##__VA_ARGS__)
#define LOG_FATAL(module, fmt, ...)  debug_log(LOG_LEVEL_FATAL, module, fmt, ##__VA_ARGS__)
#endif /* LV00_LOG_GUARD */

/* ============================================================
 * 结构化日志与环形缓冲区（v3.3.0 新增）
 * ============================================================ */

/**
 * @brief 环形日志缓冲区默认容量
 *
 * 存储最近 N 条日志消息，用于崩溃后诊断。
 * 可通过 debug_set_ring_buffer_capacity() 调整。
 */
#define LV00_LOG_RING_BUFFER_DEFAULT_CAPACITY 256

/**
 * @brief 单条结构化日志记录
 *
 * 每条日志记录包含完整的上下文信息，用于：
 * - 崩溃后诊断（环形缓冲区可紧急保存）
 * - 日志分析工具的解析（结构化字段）
 * - 性能分析（记录时间戳和耗时）
 */
typedef struct Lv00LogEntry {
    LogLevel    level;          /**< 日志级别 */
    uint64_t    timestamp_us;   /**< 时间戳（微秒精度） */
    const char *module_name;    /**< 模块名称（如 "solver", "engine", "graph"） */
    const char *function_name;  /**< 函数名称（__func__） */
    int         line_number;    /**< 源文件行号（__LINE__） */
    const char *file_name;      /**< 源文件名（__FILE__） */
    char        message[512];   /**< 格式化后的日志消息（定长，防止 OOM） */
    uint64_t    context_id;     /**< 关联的上下文 ID（0 = 全局日志） */
} Lv00LogEntry;

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
 */
typedef struct Lv00LogRingBuffer {
    Lv00LogEntry *entries;   /**< 环形缓冲区条目数组 */
    int           capacity;  /**< 缓冲区容量（最大条目数） */
    int           head;      /**< 写入位置（下一条新日志将写入此位置） */
    int           count;     /**< 当前缓冲区中的日志数量（<= capacity） */
    bool          wrapped;   /**< 是否已经至少绕回一次 */
} Lv00LogRingBuffer;

/**
 * @brief 创建环形日志缓冲区
 * @param capacity 缓冲区容量（条目数，至少为 1；默认 256）
 * @return 新分配的环形缓冲区，失败返回 NULL
 */
LV00_PUBLIC_API Lv00LogRingBuffer *lv00_log_ring_buffer_create(int capacity);

/**
 * @brief 销毁环形日志缓冲区
 * @param rb 缓冲区指针（可为 NULL）
 */
LV00_PUBLIC_API void lv00_log_ring_buffer_destroy(Lv00LogRingBuffer *rb);

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
LV00_PUBLIC_API void lv00_log_ring_buffer_write(Lv00LogRingBuffer *rb, LogLevel level,
                                const char *module_name, const char *function_name,
                                const char *file_name, int line_number,
                                const char *fmt, ...);

/**
 * @brief 导出环形缓冲区中的所有日志（按时间顺序）
 *
 * 返回的数组由调用者负责释放（使用 lv00_free）。
 *
 * @param rb           环形缓冲区（非 NULL）
 * @param out_count    输出：实际导出的条目数量
 * @return 日志条目数组（按插入时间排序），失败返回 NULL
 */
LV00_PUBLIC_API Lv00LogEntry *lv00_log_ring_buffer_export(const Lv00LogRingBuffer *rb, int *out_count);

/**
 * @brief 清空环形缓冲区中的所有日志条目
 * @param rb 环形缓冲区（非 NULL）
 */
LV00_PUBLIC_API void lv00_log_ring_buffer_clear(Lv00LogRingBuffer *rb);

/**
 * @brief 设置环形缓冲区容量（保留现有条目，最多保留新容量条）
 *
 * 如果新容量小于当前条目数，最旧的多余条目将被丢弃。
 *
 * @param rb       环形缓冲区（非 NULL）
 * @param capacity 新容量（>= 1）
 * @return true 成功，false 失败（内存不足）
 */
LV00_PUBLIC_API bool lv00_log_ring_buffer_resize(Lv00LogRingBuffer *rb, int capacity);

/* ============================================================
 * 带上下文的日志函数（v3.3.0 新增）
 *
 * lv00_log_with_context() 是结构化日志的核心 API。
 * 与简单的 debug_log() 相比，它额外记录：
 *   - 上下文 ID（用于多上下文场景的日志追踪）
 *   - 函数名 / 文件名 / 行号（用于精确的故障定位）
 *   - 自动写入环形缓冲区
 *
 * 使用 LV00_LOG_CTX() 便捷宏，自动填入 __func__, __FILE__, __LINE__。
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
struct Lv00Context; /* 前向声明 */
LV00_PUBLIC_API void lv00_log_with_context(struct Lv00Context *ctx, LogLevel level,
                           const char *module_name, const char *function_name,
                           const char *file_name, int line_number,
                           const char *fmt, ...);

/**
 * @brief 带上下文的便捷日志宏 —— 自动填入位置信息
 *
 * 使用示例：
 * @code
 *   LV00_LOG_CTX(ctx, LOG_LEVEL_WARN, "solver",
 *                "约束 %d 的变量 %d 超出范围 [%d, %d]",
 *                constraint_id, var_id, min_val, max_val);
 * @endcode
 */
#define LV00_LOG_CTX(ctx, level, module, fmt, ...) \
    lv00_log_with_context((ctx), (level), (module), __func__, __FILE__, __LINE__, \
                          (fmt), ##__VA_ARGS__)

/*=== 性能计数器 ===*/

/**
 * @brief 获取全局性能计数器。
 * 返回当前计数器值的快照。
 * @param counters 用于存储计数器值的指针
 */
LV00_PUBLIC_API void debug_get_counters(PerformanceCounters *counters);

/**
 * @brief 将所有性能计数器重置为零。
 */
LV00_PUBLIC_API void debug_reset_counters(void);

/* 计数器递增函数（由其他模块调用） */
LV00_PUBLIC_API void debug_counter_node_created(void);
LV00_PUBLIC_API void debug_counter_node_destroyed(void);
LV00_PUBLIC_API void debug_counter_constraint_created(void);
LV00_PUBLIC_API void debug_counter_constraint_destroyed(void);
LV00_PUBLIC_API void debug_counter_solver_called(uint64_t time_us);
LV00_PUBLIC_API void debug_counter_rewrite_step(void);
LV00_PUBLIC_API void debug_counter_rule_applied(void);
LV00_PUBLIC_API void debug_counter_unify_called(bool success);
LV00_PUBLIC_API void debug_counter_memory_update(uint64_t current_bytes);

/*=== 工具函数 ===*/

/**
 * @brief 将性能计数器格式化为可读字符串。
 * 调用者须负责释放返回的字符串。
 * @return 新分配的包含计数器报告的字符串
 */
LV00_PUBLIC_API char *debug_counters_report(void);

/**
 * @brief 获取日志文件路径。
 * @param buf  存储路径的缓冲区
 * @param size 缓冲区大小
 * @return 成功返回 0，失败返回负值
 */
LV00_PUBLIC_API int debug_get_log_path(char *buf, size_t size);

/*=== 归一化不变量断言 ===*/

/**
 * @brief 对引擎的约束图断言归一化不变量。
 * 返回违规数量（0 = 全部通过）。
 */
LV00_PUBLIC_API int debug_assert_normalization_invariants(const LV00Engine *engine, DebugContext *ctx);

/*=== 内存池 ===*/

/**
 * @brief 内存池 —— 用于频繁分配/释放的小对象内存管理器
 */
typedef struct Lv00MemPool Lv00MemPool;
/* 向后兼容别名 */
#define MemPool Lv00MemPool

/**
 * @brief 创建内存池
 * @param block_size  每个块的大小（字节）
 * @param initial_blocks  初始块数量
 */
LV00_PUBLIC_API MemPool *mem_pool_create(size_t block_size, int initial_blocks);

/**
 * @brief 从内存池分配一个块
 */
LV00_PUBLIC_API void *mem_pool_alloc(MemPool *pool);

/**
 * @brief 将块归还内存池
 */
LV00_PUBLIC_API void mem_pool_free(MemPool *pool, void *block);

/**
 * @brief 销毁内存池
 */
LV00_PUBLIC_API void mem_pool_destroy(MemPool *pool);

/**
 * @brief 获取内存池统计信息
 * @param total_bytes 使用 size_t* 类型，避免大内存池场景下 int 截断
 */
LV00_PUBLIC_API void mem_pool_stats(const MemPool *pool, int *total_blocks, int *free_blocks, size_t *total_bytes);

/*=== 引用计数与垃圾回收 ===*/

/**
 * @brief 引用计数对象基类 —— 提供自动引用计数和析构回调机制
 */
typedef struct Lv00RefCounted {
    int ref_count;
    void (*destructor)(void *self);
} Lv00RefCounted;
/* 向后兼容别名 */
#define RefCounted Lv00RefCounted

/**
 * @brief 增加引用计数
 */
LV00_PUBLIC_API void ref_count_inc(void *obj);

/**
 * @brief 减少引用计数，到0时自动销毁
 */
LV00_PUBLIC_API bool ref_count_dec(void *obj);

/**
 * @brief 获取当前引用计数
 */
LV00_PUBLIC_API int ref_count_get(const void *obj);

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
LV00_PUBLIC_API bool debug_emergency_save(const char *filepath, const EmergencySaveConfig *config);

/**
 * @brief 设置紧急保存回调（在信号处理程序中调用）
 */
typedef void (*EmergencySaveHandler)(const char *reason);

LV00_PUBLIC_API void debug_set_emergency_handler(EmergencySaveHandler handler);

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
LV00_PUBLIC_API PortInvariantResult *debug_check_port_invariants(const ConstraintGraph *graph);

/**
 * @brief 销毁端口不变量检查结果
 */
LV00_PUBLIC_API void debug_port_invariant_result_destroy(PortInvariantResult *result);

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
typedef struct Lv00TraceSession {
    TraceEvent *events;
    int event_count;
    int capacity;
    bool active;
} Lv00TraceSession;
/* 向后兼容别名 */
#define TraceSession Lv00TraceSession

/**
 * @brief 创建追踪会话
 */
LV00_PUBLIC_API TraceSession *trace_session_create(void);

/**
 * @brief 销毁追踪会话
 */
LV00_PUBLIC_API void trace_session_destroy(TraceSession *session);

/**
 * @brief 记录追踪事件
 */
LV00_PUBLIC_API void trace_record_event(TraceSession *session, TraceEventType type, int step, const char *description,
                        const char *details);

/**
 * @brief 导出追踪会话为 JSON
 */
LV00_PUBLIC_API char *trace_session_export_json(const TraceSession *session);

/**
 * @brief 获取全局追踪会话
 */
LV00_PUBLIC_API TraceSession *debug_get_trace_session(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_DEBUG_H */
