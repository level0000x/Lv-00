#define lv_RUNTIME_MONITOR_LOGLEVEL_SEEN 1

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

#ifndef lv_RUNTIME_MONITOR_H
#define lv_RUNTIME_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* ============== 配置常量 ============== */

/** 日志消息最大长度 */
#define lv_LOG_MSG_MAX_LEN 4096

/** 日志标签最大长度 */
#define lv_LOG_TAG_MAX_LEN 64

/** 监控指标名称最大长度 */
#define lv_METRIC_NAME_MAX_LEN 128

/** 最大计时器嵌套深度 */
#define lv_TIMER_MAX_DEPTH 32

/** 性能统计样本最大数量 */
#define lv_PERF_SAMPLE_MAX_COUNT 10000

/* ============== 日志系统 ============== */

/**
 * @brief 日志级别（与 debug.h LogLevel 保持一致）
 */
typedef enum {
#ifndef LOG_LEVEL_TRACE
    LOG_LEVEL_TRACE = -1, /**< 最详细跟踪 */
#endif
#ifndef LOG_LEVEL_DEBUG
    LOG_LEVEL_DEBUG = 0, /**< 调试信息 */
#endif
#ifndef LOG_LEVEL_INFO
    LOG_LEVEL_INFO = 1, /**< 一般信息 */
#endif
#ifndef LOG_LEVEL_WARN
    LOG_LEVEL_WARN = 2, /**< 警告 */
#endif
#ifndef LOG_LEVEL_ERROR
    LOG_LEVEL_ERROR = 3, /**< 错误 */
#endif
#ifndef LOG_LEVEL_FATAL
    LOG_LEVEL_FATAL = 4, /**< 致命错误 */
#endif
#ifndef LOG_LEVEL_OFF
    LOG_LEVEL_OFF = 5 /**< 关闭日志 */
#endif
} lvLogLevel;

/* 与 debug.h 兼容（debug.h 是主定义源，运行时监控借用其级别） */
#define LOG_LEVEL_TRACE -1
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3
#define LOG_LEVEL_FATAL 4
#define LOG_LEVEL_OFF 5
#define LOG_LEVEL_NONE 6

/**
 * @brief 日志输出目标类型
 */
typedef enum {
    LOG_TARGET_NONE = 0,     /**< 无输出 */
    LOG_TARGET_STDOUT = 1,   /**< 标准输出 */
    LOG_TARGET_STDERR = 2,   /**< 标准错误 */
    LOG_TARGET_FILE = 4,     /**< 文件 */
    LOG_TARGET_CALLBACK = 8, /**< 回调函数 */
    LOG_TARGET_SYSLOG = 16   /**< 系统日志 */
} lvLogTarget;

/**
 * @brief 日志记录结构
 */
typedef struct {
    lvLogLevel level;                 /**< 日志级别 */
    char tag[lv_LOG_TAG_MAX_LEN];     /**< 日志标签 */
    char message[lv_LOG_MSG_MAX_LEN]; /**< 日志消息 */
    char file[256];                   /**< 源文件名 */
    int line;                         /**< 行号 */
    char function[128];               /**< 函数名 */
    int64_t timestamp_ms;             /**< 时间戳（毫秒） */
    int thread_id;                    /**< 线程 ID */
} lvLogRecord;

/**
 * @brief 日志回调函数类型
 * @param record 日志记录
 * @param user_data 用户数据
 */
typedef void (*lvLogCallback)(const lvLogRecord *record, void *user_data);

/**
 * @brief 日志配置
 */
typedef struct {
    lvLogLevel min_level;     /**< 最小日志级别 */
    lvLogTarget targets;      /**< 输出目标（位掩码） */
    char file_path[256];      /**< 日志文件路径 */
    bool include_timestamp;   /**< 是否包含时间戳 */
    bool include_location;    /**< 是否包含位置信息 */
    bool include_thread_id;   /**< 是否包含线程 ID */
    bool colored_output;      /**< 是否彩色输出 */
    lvLogCallback callback;   /**< 回调函数 */
    void *callback_user_data; /**< 回调用户数据 */
    size_t max_file_size;     /**< 最大文件大小（字节） */
    int max_backup_files;     /**< 最大备份文件数 */
} lvLogConfig;

/**
 * @brief 初始化日志系统
 * @param config 配置（NULL 使用默认配置）
 * @return 是否成功
 */
bool lv_log_init(const lvLogConfig *config);

/**
 * @brief 关闭日志系统
 */
void lv_log_shutdown(void);

/**
 * @brief 设置日志级别
 * @param level 最小日志级别
 */
void lv_log_set_level(lvLogLevel level);

/**
 * @brief 设置日志输出目标
 * @param targets 输出目标（位掩码）
 */
void lv_log_set_targets(lvLogTarget targets);

/**
 * @brief 设置日志文件路径
 * @param path 文件路径
 * @return 是否成功
 */
bool lv_log_set_file(const char *path);

/**
 * @brief 设置日志回调
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void lv_log_set_callback(lvLogCallback callback, void *user_data);

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
void lv_log_write(lvLogLevel level, const char *tag, const char *file, int line, const char *function, const char *fmt,
                  ...);

/* 便捷日志宏 */
#define lv_LOG_TRACE(tag, fmt, ...) lv_log_write(LOG_LEVEL_TRACE, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define lv_LOG_DEBUG(tag, fmt, ...) lv_log_write(LOG_LEVEL_DEBUG, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define lv_LOG_INFO(tag, fmt, ...) lv_log_write(LOG_LEVEL_INFO, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define lv_LOG_WARN(tag, fmt, ...) lv_log_write(LOG_LEVEL_WARN, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define lv_LOG_WARNING(tag, fmt, ...) \
    lv_log_write(LOG_LEVEL_WARN, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define lv_LOG_WARN_NT(fmt, ...) \
    lv_log_write(LOG_LEVEL_WARN, "runtime", __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define lv_LOG_ERROR(tag, fmt, ...) lv_log_write(LOG_LEVEL_ERROR, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define lv_LOG_FATAL(tag, fmt, ...) lv_log_write(LOG_LEVEL_FATAL, tag, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/* ============== 性能监控 ============== */

/**
 * @brief 计时器状态
 */
typedef enum {
    TIMER_STOPPED, /**< 已停止 */
    TIMER_RUNNING, /**< 运行中 */
    TIMER_PAUSED   /**< 已暂停 */
} lvTimerState;

/**
 * @brief 计时器结构
 */
typedef struct {
    char name[lv_METRIC_NAME_MAX_LEN]; /**< 计时器名称 */
    lvTimerState state;                /**< 状态 */
    int64_t start_time_ns;             /**< 开始时间（纳秒） */
    int64_t elapsed_ns;                /**< 已用时间（纳秒） */
    int64_t total_ns;                  /**< 累计时间（纳秒） */
    uint64_t call_count;               /**< 调用次数 */
    int depth;                         /**< 嵌套深度 */
} lvTimer;

/**
 * @brief 性能统计
 */
typedef struct {
    char name[lv_METRIC_NAME_MAX_LEN]; /**< 指标名称 */
    uint64_t count;                    /**< 样本数 */
    double min_val;                    /**< 最小值 */
    double max_val;                    /**< 最大值 */
    double sum;                        /**< 总和 */
    double sum_sq;                     /**< 平方和 */
    double m2;                         /**< Welford M₂：平方差累积和，用于数值稳定方差（见 Welford 在线算法） */
    double mean;                       /**< 均值 */
    double variance;                   /**< 方差 */
    double std_dev;                    /**< 标准差 */
    double last_val;                   /**< 最后值 */
    int64_t last_time_ns;              /**< 最后更新时间 */
} lvPerfStats;

/**
 * @brief 初始化性能监控
 * @return 是否成功
 */
bool lv_perf_init(void);

/**
 * @brief 关闭性能监控
 */
void lv_perf_shutdown(void);

/**
 * @brief 创建计时器
 * @param name 计时器名称
 * @return 计时器指针（失败返回 NULL）
 */
lvTimer *lv_timer_create(const char *name);

/**
 * @brief 销毁计时器
 * @param timer 计时器指针
 */
void lv_timer_destroy(lvTimer *timer);

/**
 * @brief 启动计时器
 * @param timer 计时器
 */
void lv_timer_start(lvTimer *timer);

/**
 * @brief 停止计时器
 * @param timer 计时器
 * @return 经过的毫秒数
 */
int64_t lv_timer_stop(lvTimer *timer);

/**
 * @brief 暂停计时器
 * @param timer 计时器
 */
void lv_timer_pause(lvTimer *timer);

/**
 * @brief 恢复计时器
 * @param timer 计时器
 */
void lv_timer_resume(lvTimer *timer);

/**
 * @brief 重置计时器
 * @param timer 计时器
 */
void lv_timer_reset(lvTimer *timer);

/**
 * @brief 获取计时器经过时间
 * @param timer 计时器
 * @return 经过的毫秒数
 */
int64_t lv_timer_elapsed_ms(const lvTimer *timer);

/**
 * @brief 获取计时器经过时间（纳秒）
 * @param timer 计时器
 * @return 经过的纳秒数
 */
int64_t lv_timer_elapsed_ns(const lvTimer *timer);

/**
 * @brief 作用域计时器（自动开始/停止）
 * @param name 计时器名称
 */
#define lv_SCOPED_TIMER(name)                         \
    lvTimer *__timer_##name = lv_timer_create(#name); \
    lv_timer_start(__timer_##name);                   \
    __attribute__((cleanup(lv_timer_auto_stop))) lvTimer **__timer_ptr_##name = &__timer_##name

/* 自动停止函数（内部使用） */
static inline void lv_timer_auto_stop(lvTimer ***timer_ptr) {
    if (timer_ptr && *timer_ptr && **timer_ptr) {
        lv_timer_stop(**timer_ptr);
        lv_timer_destroy(**timer_ptr);
    }
}

/**
 * @brief 创建性能统计
 * @param name 统计名称
 * @return 统计指针
 */
lvPerfStats *lv_perf_stats_create(const char *name);

/**
 * @brief 销毁性能统计
 * @param stats 统计指针
 */
void lv_perf_stats_destroy(lvPerfStats *stats);

/**
 * @brief 记录性能样本
 * @param stats 统计
 * @param value 样本值
 */
void lv_perf_stats_record(lvPerfStats *stats, double value);

/**
 * @brief 重置性能统计
 * @param stats 统计
 */
void lv_perf_stats_reset(lvPerfStats *stats);

/**
 * @brief 获取所有计时器统计
 * @param out_stats 输出统计数组
 * @param max_count 最大数量
 * @return 实际数量
 */
uint32_t lv_perf_get_all_timer_stats(lvPerfStats **out_stats, uint32_t max_count);

/* ============== 健康检查 ============== */

/**
 * @brief 健康状态
 */
typedef enum {
    HEALTH_OK,       /**< 正常 */
    HEALTH_WARNING,  /**< 警告 */
    HEALTH_CRITICAL, /**< 严重 */
    HEALTH_UNKNOWN   /**< 未知 */
} lvHealthStatus;

/**
 * @brief 健康检查项
 */
typedef struct {
    char name[lv_METRIC_NAME_MAX_LEN]; /**< 检查项名称 */
    lvHealthStatus status;             /**< 状态 */
    char message[256];                 /**< 状态消息 */
    double value;                      /**< 当前值 */
    double threshold_warning;          /**< 警告阈值 */
    double threshold_critical;         /**< 严重阈值 */
} lvHealthCheck;

/**
 * @brief 健康报告
 */
typedef struct {
    lvHealthCheck *checks;  /**< 检查项数组 */
    uint32_t check_count;   /**< 检查项数量 */
    lvHealthStatus overall; /**< 总体状态 */
    int64_t timestamp_ms;   /**< 时间戳 */
} lvHealthReport;

/**
 * @brief 初始化健康检查
 * @return 是否成功
 */
bool lv_health_init(void);

/**
 * @brief 关闭健康检查
 */
void lv_health_shutdown(void);

/**
 * @brief 执行健康检查
 * @return 健康报告（调用者负责释放）
 */
lvHealthReport *lv_runtime_health_check(void);

/**
 * @brief 销毁健康报告
 * @param report 报告指针
 */
void lv_health_report_destroy(lvHealthReport *report);

/**
 * @brief 设置内存使用阈值
 * @param warning_mb 警告阈值（MB）
 * @param critical_mb 严重阈值（MB）
 */
void lv_health_set_memory_thresholds(double warning_mb, double critical_mb);

/**
 * @brief 设置 CPU 使用阈值
 * @param warning_percent 警告阈值（百分比）
 * @param critical_percent 严重阈值（百分比）
 */
void lv_health_set_cpu_thresholds(double warning_percent, double critical_percent);

/* ============== 诊断报告 ============== */

/**
 * @brief 诊断报告结构
 */
typedef struct {
    /* 基本信息 */
    char version[64];    /**< 版本号 */
    char build_date[32]; /**< 构建日期 */
    int64_t uptime_ms;   /**< 运行时间（毫秒） */

    /* 内存统计 */
    uint64_t memory_total; /**< 总内存使用 */
    uint64_t memory_peak;  /**< 峰值内存 */
    uint64_t alloc_count;  /**< 分配次数 */
    uint64_t free_count;   /**< 释放次数 */

    /* 性能统计 */
    uint64_t proof_count;     /**< 证明次数 */
    uint64_t solve_count;     /**< 求解次数 */
    double avg_proof_time_ms; /**< 平均证明时间 */
    double avg_solve_time_ms; /**< 平均求解时间 */

    /* 错误统计 */
    uint64_t error_count;   /**< 错误次数 */
    uint64_t warning_count; /**< 警告次数 */
    char last_error[256];   /**< 最后错误消息 */

    /* 健康状态 */
    lvHealthStatus health; /**< 健康状态 */

    /* 系统信息 */
    char os_info[256];        /**< 操作系统信息 */
    char cpu_info[256];       /**< CPU 信息 */
    uint32_t cpu_cores;       /**< CPU 核心数 */
    uint64_t total_memory_mb; /**< 总内存（MB） */
} lvDiagnostics;

/**
 * @brief 生成诊断报告
 * @return 诊断报告（调用者负责释放）
 */
lvDiagnostics *lv_diagnostics_generate(void);

/**
 * @brief 销毁诊断报告
 * @param diag 报告指针
 */
void lv_diagnostics_destroy(lvDiagnostics *diag);

/**
 * @brief 将诊断报告写入文件
 * @param diag 诊断报告
 * @param path 文件路径
 * @return 是否成功
 */
bool lv_diagnostics_write_file(const lvDiagnostics *diag, const char *path);

/**
 * @brief 将诊断报告转换为 JSON
 * @param diag 诊断报告
 * @return JSON 字符串（调用者负责释放）
 */
char *lv_diagnostics_to_json(const lvDiagnostics *diag);

/* ============== 事件追踪 ============== */

/**
 * @brief 事件类型
 */
typedef enum {
    EVENT_TYPE_PROOF_START,    /**< 证明开始 */
    EVENT_TYPE_PROOF_END,      /**< 证明结束 */
    EVENT_TYPE_SOLVE_START,    /**< 求解开始 */
    EVENT_TYPE_SOLVE_END,      /**< 求解结束 */
    EVENT_TYPE_CONSTRAINT_ADD, /**< 约束添加 */
    EVENT_TYPE_CONSTRAINT_DEL, /**< 约束删除 */
    EVENT_TYPE_NODE_CREATE,    /**< 节点创建 */
    EVENT_TYPE_NODE_DESTROY,   /**< 节点销毁 */
    EVENT_TYPE_ERROR,          /**< 错误 */
    EVENT_TYPE_WARNING,        /**< 警告 */
    EVENT_TYPE_CUSTOM          /**< 自定义 */
} lvEventType;

/**
 * @brief 事件记录
 */
typedef struct {
    lvEventType type;     /**< 事件类型 */
    char name[64];        /**< 事件名称 */
    int64_t timestamp_ns; /**< 时间戳（纳秒） */
    int64_t duration_ns;  /**< 持续时间（纳秒） */
    char data[256];       /**< 事件数据 */
    int thread_id;        /**< 线程 ID */
} lvEventRecord;

/**
 * @brief 初始化事件追踪
 * @param max_events 最大事件数
 * @return 是否成功
 */
bool lv_event_trace_init(uint32_t max_events);

/**
 * @brief 关闭事件追踪
 */
void lv_event_trace_shutdown(void);

/**
 * @brief 记录事件
 * @param type 事件类型
 * @param name 事件名称
 * @param data 事件数据
 */
void lv_event_trace_record(lvEventType type, const char *name, const char *data);

/**
 * @brief 开始事件（用于计时）
 * @param type 事件类型
 * @param name 事件名称
 * @return 事件 ID
 */
int lv_event_trace_begin(lvEventType type, const char *name);

/**
 * @brief 结束事件
 * @param event_id 事件 ID
 * @param data 事件数据
 */
void lv_event_trace_end(int event_id, const char *data);

/**
 * @brief 获取所有事件记录
 * @param out_events 输出事件数组
 * @param max_count 最大数量
 * @return 实际数量
 */
uint32_t lv_event_trace_get_all(lvEventRecord **out_events, uint32_t max_count);

/**
 * @brief 清空事件记录
 */
void lv_event_trace_clear(void);

/**
 * @brief 导出事件追踪为 Chrome Tracing 格式
 * @param path 输出文件路径
 * @return 是否成功
 */
bool lv_event_trace_export_chrome(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* lv_RUNTIME_MONITOR_H */
