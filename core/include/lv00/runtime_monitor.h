#define LV00_RUNTIME_MONITOR_LOGLEVEL_SEEN 1

/**
 * @file runtime_monitor.h
 * @brief 运行时监控与日志系统
 *
 * @details 提供完整的运行时监控和日志功能：
 *   1. 结构化日志：多级别、多输出目标
 *   2. 性能监控：计时、内存、调用统计
 *   3. 健康检查：资源使用、异常检测
 *   4. 诊断报告：自动生成诊断信息
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#ifndef LV00_RUNTIME_MONITOR_H
#define LV00_RUNTIME_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* ============== 配置常量 ============== */

/** 日志消息最大长度 */
#define LV00_LOG_MSG_MAX_LEN 4096

/** 日志标签最大长度 */
#define LV00_LOG_TAG_MAX_LEN 64

/** 监控指标名称最大长度 */
#define LV00_METRIC_NAME_MAX_LEN 128

/** 最大计时器嵌套深度 */
#define LV00_TIMER_MAX_DEPTH 32

/** 性能统计样本最大数量 */
#define LV00_PERF_SAMPLE_MAX_COUNT 10000

/* ============== 日志系统 ============== */

/**
 * @brief 日志级别
 */
typedef enum {
#ifndef LOG_LEVEL_TRACE
    LOG_LEVEL_TRACE = 0,    /**< 最详细跟踪 */
#endif
#ifndef LOG_LEVEL_DEBUG
    LOG_LEVEL_DEBUG = 1,    /**< 调试信息 */
#endif
#ifndef LOG_LEVEL_INFO
    LOG_LEVEL_INFO = 2,     /**< 一般信息 */
#endif
#ifndef LOG_LEVEL_WARN
    LOG_LEVEL_WARN = 3,     /**< 警告 */
#endif
#ifndef LOG_LEVEL_ERROR
    LOG_LEVEL_ERROR = 4,    /**< 错误 */
#endif
#ifndef LOG_LEVEL_FATAL
    LOG_LEVEL_FATAL = 5,    /**< 致命错误 */
#endif
#ifndef LOG_LEVEL_OFF
    LOG_LEVEL_OFF = 6       /**< 关闭日志 */
#endif
} Lv00LogLevel;

/* Prevent redeclaration of LOG_LEVEL_* in debug.h */
#define LOG_LEVEL_TRACE 0
#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO  2
#define LOG_LEVEL_WARN  3
#define LOG_LEVEL_ERROR 4
#define LOG_LEVEL_FATAL 5
#define LOG_LEVEL_OFF   6
#define LOG_LEVEL_NONE  7

/**
 * @brief 日志输出目标类型
 */
typedef enum {
    LOG_TARGET_NONE = 0,        /**< 无输出 */
    LOG_TARGET_STDOUT = 1,      /**< 标准输出 */
    LOG_TARGET_STDERR = 2,      /**< 标准错误 */
    LOG_TARGET_FILE = 4,        /**< 文件 */
    LOG_TARGET_CALLBACK = 8,    /**< 回调函数 */
    LOG_TARGET_SYSLOG = 16      /**< 系统日志 */
} Lv00LogTarget;

/**
 * @brief 日志记录结构
 */
typedef struct {
    Lv00LogLevel level;             /**< 日志级别 */
    char tag[LV00_LOG_TAG_MAX_LEN]; /**< 日志标签 */
    char message[LV00_LOG_MSG_MAX_LEN]; /**< 日志消息 */
    char file[256];                 /**< 源文件名 */
    int line;                       /**< 行号 */
    char function[128];             /**< 函数名 */
    int64_t timestamp_ms;           /**< 时间戳（毫秒） */
    int thread_id;                  /**< 线程 ID */
} Lv00LogRecord;

/**
 * @brief 日志回调函数类型
 * @param record 日志记录
 * @param user_data 用户数据
 */
typedef void (*Lv00LogCallback)(const Lv00LogRecord *record, void *user_data);

/**
 * @brief 日志配置
 */
typedef struct {
    Lv00LogLevel min_level;     /**< 最小日志级别 */
    Lv00LogTarget targets;      /**< 输出目标（位掩码） */
    char file_path[256];        /**< 日志文件路径 */
    bool include_timestamp;     /**< 是否包含时间戳 */
    bool include_location;      /**< 是否包含位置信息 */
    bool include_thread_id;     /**< 是否包含线程 ID */
    bool colored_output;        /**< 是否彩色输出 */
    Lv00LogCallback callback;   /**< 回调函数 */
    void *callback_user_data;   /**< 回调用户数据 */
    size_t max_file_size;       /**< 最大文件大小（字节） */
    int max_backup_files;       /**< 最大备份文件数 */
} Lv00LogConfig;

/**
 * @brief 初始化日志系统
 * @param config 配置（NULL 使用默认配置）
 * @return 是否成功
 */
int lv00_log_init(const Lv00LogConfig *config);

/**
 * @brief 关闭日志系统
 */
void lv00_log_shutdown(void);

/**
 * @brief 设置日志级别
 * @param level 最小日志级别
 */
void lv00_log_set_level(Lv00LogLevel level);

/**
 * @brief 设置日志输出目标
 * @param targets 输出目标（位掩码）
 */
void lv00_log_set_targets(Lv00LogTarget targets);

/**
 * @brief 设置日志文件路径
 * @param path 文件路径
 * @return 是否成功
 */
int lv00_log_set_file(const char *path);

/**
 * @brief 设置日志回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void lv00_log_set_callback(Lv00LogCallback callback, void *user_data);

/**
 * @brief 记录日志（内部使用）
 * @param level 日志级别
 * @param tag 标签
 * @param file 源文件
 * @param line 行号
 * @param function 函数名
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
void lv00_log_write(Lv00LogLevel level, const char *tag,
                    const char *file, int line, const char *function,
                    const char *fmt, ...);

/* 便捷日志宏 */
#define LV00_LOG_TRACE(tag, fmt, ...) \
    lv00_log_write(LOG_LEVEL_TRACE, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LV00_LOG_DEBUG(tag, fmt, ...) \
    lv00_log_write(LOG_LEVEL_DEBUG, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LV00_LOG_INFO(tag, fmt, ...) \
    lv00_log_write(LOG_LEVEL_INFO, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LV00_LOG_WARN(tag, fmt, ...) \
    lv00_log_write(LOG_LEVEL_WARN, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LV00_LOG_WARNING(tag, fmt, ...) \
    lv00_log_write(LOG_LEVEL_WARN, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LV00_LOG_WARN_NT(fmt, ...) \
    lv00_log_write(LOG_LEVEL_WARN, "runtime", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LV00_LOG_ERROR(tag, fmt, ...) \
    lv00_log_write(LOG_LEVEL_ERROR, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LV00_LOG_FATAL(tag, fmt, ...) \
    lv00_log_write(LOG_LEVEL_FATAL, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/* ============== 性能监控 ============== */

/**
 * @brief 计时器状态
 */
typedef enum {
    TIMER_STOPPED,      /**< 已停止 */
    TIMER_RUNNING,      /**< 运行中 */
    TIMER_PAUSED        /**< 已暂停 */
} Lv00TimerState;

/**
 * @brief 计时器结构
 */
typedef struct {
    char name[LV00_METRIC_NAME_MAX_LEN]; /**< 计时器名称 */
    Lv00TimerState state;                /**< 状态 */
    int64_t start_time_ns;               /**< 开始时间（纳秒） */
    int64_t elapsed_ns;                  /**< 已用时间（纳秒） */
    int64_t total_ns;                    /**< 累计时间（纳秒） */
    uint64_t call_count;                 /**< 调用次数 */
    int depth;                           /**< 嵌套深度 */
} Lv00Timer;

/**
 * @brief 性能统计
 */
typedef struct {
    char name[LV00_METRIC_NAME_MAX_LEN]; /**< 指标名称 */
    uint64_t count;                      /**< 样本数 */
    double min_val;                      /**< 最小值 */
    double max_val;                      /**< 最大值 */
    double sum;                          /**< 总和 */
    double sum_sq;                       /**< 平方和 */
    double mean;                         /**< 均值 */
    double variance;                     /**< 方差 */
    double std_dev;                      /**< 标准差 */
    double last_val;                     /**< 最后值 */
    int64_t last_time_ns;                /**< 最后更新时间 */
} Lv00PerfStats;

/**
 * @brief 初始化性能监控
 * @return 是否成功
 */
int lv00_perf_init(void);

/**
 * @brief 关闭性能监控
 */
void lv00_perf_shutdown(void);

/**
 * @brief 创建计时器
 * @param name 计时器名称
 * @return 计时器指针（失败返回 NULL）
 */
Lv00Timer *lv00_timer_create(const char *name);

/**
 * @brief 销毁计时器
 * @param timer 计时器指针
 */
void lv00_timer_destroy(Lv00Timer *timer);

/**
 * @brief 启动计时器
 * @param timer 计时器
 */
void lv00_timer_start(Lv00Timer *timer);

/**
 * @brief 停止计时器
 * @param timer 计时器
 * @return 经过的毫秒数
 */
int64_t lv00_timer_stop(Lv00Timer *timer);

/**
 * @brief 暂停计时器
 * @param timer 计时器
 */
void lv00_timer_pause(Lv00Timer *timer);

/**
 * @brief 恢复计时器
 * @param timer 计时器
 */
void lv00_timer_resume(Lv00Timer *timer);

/**
 * @brief 重置计时器
 * @param timer 计时器
 */
void lv00_timer_reset(Lv00Timer *timer);

/**
 * @brief 获取计时器经过时间
 * @param timer 计时器
 * @return 经过的毫秒数
 */
int64_t lv00_timer_elapsed_ms(const Lv00Timer *timer);

/**
 * @brief 获取计时器经过时间（纳秒）
 * @param timer 计时器
 * @return 经过的纳秒数
 */
int64_t lv00_timer_elapsed_ns(const Lv00Timer *timer);

/**
 * @brief 作用域计时器（自动开始/停止）
 * @param name 计时器名称
 */
#define LV00_SCOPED_TIMER(name) \
    Lv00Timer *__timer_##name = lv00_timer_create(#name); \
    lv00_timer_start(__timer_##name); \
    __attribute__((cleanup(lv00_timer_auto_stop))) Lv00Timer **__timer_ptr_##name = &__timer_##name

/* 自动停止函数（内部使用） */
static inline void lv00_timer_auto_stop(Lv00Timer ***timer_ptr) {
    if (timer_ptr && *timer_ptr && **timer_ptr) {
        lv00_timer_stop(**timer_ptr);
        lv00_timer_destroy(**timer_ptr);
    }
}

/**
 * @brief 创建性能统计
 * @param name 统计名称
 * @return 统计指针
 */
Lv00PerfStats *lv00_perf_stats_create(const char *name);

/**
 * @brief 销毁性能统计
 * @param stats 统计指针
 */
void lv00_perf_stats_destroy(Lv00PerfStats *stats);

/**
 * @brief 记录性能样本
 * @param stats 统计
 * @param value 样本值
 */
void lv00_perf_stats_record(Lv00PerfStats *stats, double value);

/**
 * @brief 重置性能统计
 * @param stats 统计
 */
void lv00_perf_stats_reset(Lv00PerfStats *stats);

/**
 * @brief 获取所有计时器统计
 * @param out_stats 输出统计数组
 * @param max_count 最大数量
 * @return 实际数量
 */
uint32_t lv00_perf_get_all_timer_stats(Lv00PerfStats **out_stats, uint32_t max_count);

/* ============== 健康检查 ============== */

/**
 * @brief 健康状态
 */
typedef enum {
    HEALTH_OK,          /**< 正常 */
    HEALTH_WARNING,     /**< 警告 */
    HEALTH_CRITICAL,    /**< 严重 */
    HEALTH_UNKNOWN      /**< 未知 */
} Lv00HealthStatus;

/**
 * @brief 健康检查项
 */
typedef struct {
    char name[LV00_METRIC_NAME_MAX_LEN]; /**< 检查项名称 */
    Lv00HealthStatus status;             /**< 状态 */
    char message[256];                   /**< 状态消息 */
    double value;                        /**< 当前值 */
    double threshold_warning;            /**< 警告阈值 */
    double threshold_critical;           /**< 严重阈值 */
} Lv00HealthCheck;

/**
 * @brief 健康报告
 */
typedef struct {
    Lv00HealthCheck *checks;     /**< 检查项数组 */
    uint32_t check_count;        /**< 检查项数量 */
    Lv00HealthStatus overall;    /**< 总体状态 */
    int64_t timestamp_ms;        /**< 时间戳 */
} Lv00HealthReport;

/**
 * @brief 初始化健康检查
 * @return 是否成功
 */
int lv00_health_init(void);

/**
 * @brief 关闭健康检查
 */
void lv00_health_shutdown(void);

/**
 * @brief 执行健康检查
 * @return 健康报告（调用者负责释放）
 */
Lv00HealthReport *lv00_runtime_health_check(void);

/**
 * @brief 销毁健康报告
 * @param report 报告指针
 */
void lv00_health_report_destroy(Lv00HealthReport *report);

/**
 * @brief 设置内存使用阈值
 * @param warning_mb 警告阈值（MB）
 * @param critical_mb 严重阈值（MB）
 */
void lv00_health_set_memory_thresholds(double warning_mb, double critical_mb);

/**
 * @brief 设置 CPU 使用阈值
 * @param warning_percent 警告阈值（百分比）
 * @param critical_percent 严重阈值（百分比）
 */
void lv00_health_set_cpu_thresholds(double warning_percent, double critical_percent);

/* ============== 诊断报告 ============== */

/**
 * @brief 诊断报告结构
 */
typedef struct {
    /* 基本信息 */
    char version[64];           /**< 版本号 */
    char build_date[32];        /**< 构建日期 */
    int64_t uptime_ms;          /**< 运行时间（毫秒） */

    /* 内存统计 */
    uint64_t memory_total;      /**< 总内存使用 */
    uint64_t memory_peak;       /**< 峰值内存 */
    uint64_t alloc_count;       /**< 分配次数 */
    uint64_t free_count;        /**< 释放次数 */

    /* 性能统计 */
    uint64_t proof_count;       /**< 证明次数 */
    uint64_t solve_count;       /**< 求解次数 */
    double avg_proof_time_ms;   /**< 平均证明时间 */
    double avg_solve_time_ms;   /**< 平均求解时间 */

    /* 错误统计 */
    uint64_t error_count;       /**< 错误次数 */
    uint64_t warning_count;     /**< 警告次数 */
    char last_error[256];       /**< 最后错误消息 */

    /* 健康状态 */
    Lv00HealthStatus health;    /**< 健康状态 */

    /* 系统信息 */
    char os_info[256];          /**< 操作系统信息 */
    char cpu_info[256];         /**< CPU 信息 */
    uint32_t cpu_cores;         /**< CPU 核心数 */
    uint64_t total_memory_mb;   /**< 总内存（MB） */
} Lv00Diagnostics;

/**
 * @brief 生成诊断报告
 * @return 诊断报告（调用者负责释放）
 */
Lv00Diagnostics *lv00_diagnostics_generate(void);

/**
 * @brief 销毁诊断报告
 * @param diag 报告指针
 */
void lv00_diagnostics_destroy(Lv00Diagnostics *diag);

/**
 * @brief 将诊断报告写入文件
 * @param diag 诊断报告
 * @param path 文件路径
 * @return 是否成功
 */
int lv00_diagnostics_write_file(const Lv00Diagnostics *diag, const char *path);

/**
 * @brief 将诊断报告转换为 JSON
 * @param diag 诊断报告
 * @return JSON 字符串（调用者负责释放）
 */
char *lv00_diagnostics_to_json(const Lv00Diagnostics *diag);

/* ============== 事件追踪 ============== */

/**
 * @brief 事件类型
 */
typedef enum {
    EVENT_TYPE_PROOF_START,     /**< 证明开始 */
    EVENT_TYPE_PROOF_END,       /**< 证明结束 */
    EVENT_TYPE_SOLVE_START,     /**< 求解开始 */
    EVENT_TYPE_SOLVE_END,       /**< 求解结束 */
    EVENT_TYPE_CONSTRAINT_ADD,  /**< 约束添加 */
    EVENT_TYPE_CONSTRAINT_DEL,  /**< 约束删除 */
    EVENT_TYPE_NODE_CREATE,     /**< 节点创建 */
    EVENT_TYPE_NODE_DESTROY,    /**< 节点销毁 */
    EVENT_TYPE_ERROR,           /**< 错误 */
    EVENT_TYPE_WARNING,         /**< 警告 */
    EVENT_TYPE_CUSTOM           /**< 自定义 */
} Lv00EventType;

/**
 * @brief 事件记录
 */
typedef struct {
    Lv00EventType type;             /**< 事件类型 */
    char name[64];                  /**< 事件名称 */
    int64_t timestamp_ns;           /**< 时间戳（纳秒） */
    int64_t duration_ns;            /**< 持续时间（纳秒） */
    char data[256];                 /**< 事件数据 */
    int thread_id;                  /**< 线程 ID */
} Lv00EventRecord;

/**
 * @brief 初始化事件追踪
 * @param max_events 最大事件数
 * @return 是否成功
 */
int lv00_event_trace_init(uint32_t max_events);

/**
 * @brief 关闭事件追踪
 */
void lv00_event_trace_shutdown(void);

/**
 * @brief 记录事件
 * @param type 事件类型
 * @param name 事件名称
 * @param data 事件数据
 */
void lv00_event_trace_record(Lv00EventType type, const char *name, const char *data);

/**
 * @brief 开始事件（用于计时）
 * @param type 事件类型
 * @param name 事件名称
 * @return 事件 ID
 */
int lv00_event_trace_begin(Lv00EventType type, const char *name);

/**
 * @brief 结束事件
 * @param event_id 事件 ID
 * @param data 事件数据
 */
void lv00_event_trace_end(int event_id, const char *data);

/**
 * @brief 获取所有事件记录
 * @param out_events 输出事件数组
 * @param max_count 最大数量
 * @return 实际数量
 */
uint32_t lv00_event_trace_get_all(Lv00EventRecord **out_events, uint32_t max_count);

/**
 * @brief 清空事件记录
 */
void lv00_event_trace_clear(void);

/**
 * @brief 导出事件追踪为 Chrome Tracing 格式
 * @param path 输出文件路径
 * @return 是否成功
 */
int lv00_event_trace_export_chrome(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* LV00_RUNTIME_MONITOR_H */
