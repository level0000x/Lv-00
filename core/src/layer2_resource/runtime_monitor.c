/**
 * @file runtime_monitor.c
 * @brief 运行时监控与日志系统实现
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"

#include "runtime_monitor.h"

/* lv_log_shutdown 的声明已集中到 lv/lv_log.h；
 * 但 lv_log.h 与本头文件各自定义 lvLogLevel 类型，不能同时包含，
 * 故在此给出前置声明，保证本文件中定义的函数原型可见。 */
void lv_log_shutdown(void);

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"
#include "lv_internal.h"
#include "lv/lv_event_bus.h"

#include "config.h"
#include "lv_utils.h"

/* ============== 内部常量 ============== */

/** 最大计时器数量 */
#define MAX_TIMERS 256

/** 最大性能统计数量 */
#define MAX_PERF_STATS 256

/** 最大事件数量 */
#define MAX_EVENTS 10000

/* ============== 平台抽象层 ============== */

#include "lv/lv_thread.h"

/* ============== 日志系统实现 ============== */

/**
 * @brief 运行时监控模块全局状态（替代原有的 10 个分散 static 变量）
 *
 * 将 4 个子系统（日志、性能、健康、事件追踪）的数据体与初始化锁
 * 归并到单一上下文结构体中：原先每子系统一套 init_mutex/init_once +
 * 独立 static 数据体的重复模式，统一为一把 lv_lazy_lock 惰性锁
 * （首次加锁时自动初始化），各子系统的数据互斥锁按原锁粒度保留
 * （不改变并发语义）。
 */
typedef struct RuntimeMonitorState {
    /* 统一初始化互斥锁（替代原先 4 把子系统 init 锁） */
    lv_lazy_lock init_lock;   /**< 子系统初始化保护锁（惰性初始化，首次加锁时自动完成） */

    /* 日志子系统（原 g_log_system） */
    struct {
        lvLogConfig config;
        FILE *log_file;
        lv_mutex_t mutex;
        bool initialized;
        uint64_t current_file_size;
    } log;

    /* 性能子系统（原 g_perf_system） */
    struct {
        lvTimer *timers[MAX_TIMERS];
        uint32_t timer_count;
        lvPerfStats *stats[MAX_PERF_STATS];
        uint32_t stats_count;
        lv_mutex_t mutex;
        bool initialized;
    } perf;

    /* 健康子系统（原 g_health_system） */
    struct {
        double memory_warning_mb;
        double memory_critical_mb;
        double cpu_warning_percent;
        double cpu_critical_percent;
        lv_mutex_t mutex;
        bool initialized;
    } health;

    /* 事件追踪子系统（原 g_event_system） */
    struct {
        lvEventRecord *events;
        uint32_t max_events;
        uint32_t event_count;
        lv_mutex_t mutex;
        bool initialized;
    } event;

    /* 事件总线 */
    lvEventBus event_bus;
    lv_once_t event_bus_once;
} RuntimeMonitorState;

/** 模块级唯一状态实例（替代原有的 10 个分散 static 变量） */
static RuntimeMonitorState s_runtime_state = {0};

/** 统一初始化互斥锁的惰性初始化（首次加锁时由 lv_lazy_lock 自动完成） */
static void runtime_init_mutex_func(void) {
    lv_mutex_init(&s_runtime_state.init_lock.mutex);
}

bool lv_log_init(const lvLogConfig *config) {
    lv_lazy_lock_lock(&s_runtime_state.init_lock, runtime_init_mutex_func);
    if (s_runtime_state.log.initialized) {
        lv_lazy_lock_unlock(&s_runtime_state.init_lock);
        return true;
    }

    memset(&s_runtime_state.log, 0, sizeof(s_runtime_state.log));

    if (config) {
        memcpy(&s_runtime_state.log.config, config, sizeof(lvLogConfig));
    } else {
        /* 默认配置 */
        s_runtime_state.log.config.min_level = LOG_LEVEL_INFO;
        s_runtime_state.log.config.targets = LOG_TARGET_STDOUT;
        s_runtime_state.log.config.include_timestamp = true;
        s_runtime_state.log.config.include_location = false;
        s_runtime_state.log.config.include_thread_id = false;
        s_runtime_state.log.config.colored_output = true;
        s_runtime_state.log.config.max_file_size = 10 * 1024 * 1024; /* 10 MB */
        s_runtime_state.log.config.max_backup_files = 5;
    }

    lv_mutex_init(&s_runtime_state.log.mutex);

    /* 打开日志文件 */
    if ((s_runtime_state.log.config.targets & LOG_TARGET_FILE) && s_runtime_state.log.config.file_path[0]) {
        s_runtime_state.log.log_file = fopen(s_runtime_state.log.config.file_path, "a");
        if (!s_runtime_state.log.log_file) {
            s_runtime_state.log.config.targets &= ~LOG_TARGET_FILE;
        }
    }

    s_runtime_state.log.initialized = true;
    lv_lazy_lock_unlock(&s_runtime_state.init_lock);
    return true;
}

void lv_log_shutdown(void) {
    if (!s_runtime_state.log.initialized) {
        return;
    }

    if (s_runtime_state.log.log_file) {
        fclose(s_runtime_state.log.log_file);
        s_runtime_state.log.log_file = NULL;
    }

    lv_mutex_destroy(&s_runtime_state.log.mutex);
    s_runtime_state.log.initialized = false;
}

void lv_log_set_level(lvLogLevel level) {
    if (level >= LOG_LEVEL_TRACE && level <= LOG_LEVEL_OFF) {
        lv_mutex_lock(&s_runtime_state.log.mutex);
        s_runtime_state.log.config.min_level = level;
        lv_mutex_unlock(&s_runtime_state.log.mutex);
    }
}

void lv_log_set_targets(lvLogTarget targets) {
    /* 线程安全：加锁保护全局日志目标的修改 */
    lv_mutex_lock(&s_runtime_state.log.mutex);
    s_runtime_state.log.config.targets = targets;
    lv_mutex_unlock(&s_runtime_state.log.mutex);
}

bool lv_log_set_file(const char *path) {
    if (!path) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_log_set_file: path is NULL");
    }

    lv_mutex_lock(&s_runtime_state.log.mutex);

    /* 先打开新文件，确保成功后再关闭旧文件，避免 fopen 失败导致日志丢失 */
    FILE *new_file = fopen(path, "a");
    if (!new_file) {
        lv_mutex_unlock(&s_runtime_state.log.mutex);
        lv_RETURN_ERROR_BOOL(lv_ERROR_IO, "lv_log_set_file: fopen failed");
    }

    /* 新文件打开成功，关闭旧文件 */
    if (s_runtime_state.log.log_file) {
        fclose(s_runtime_state.log.log_file);
    }

    strncpy(s_runtime_state.log.config.file_path, path, sizeof(s_runtime_state.log.config.file_path) - 1);
    s_runtime_state.log.config.file_path[sizeof(s_runtime_state.log.config.file_path) - 1] = '\0';
    s_runtime_state.log.log_file = new_file;
    s_runtime_state.log.current_file_size = 0;

    lv_mutex_unlock(&s_runtime_state.log.mutex);

    return true;
}

void lv_log_set_callback(lvLogCallback callback, void *user_data) {
    /* 线程安全：加锁保护回调和用户数据的修改，防止与日志写入并发冲突 */
    lv_mutex_lock(&s_runtime_state.log.mutex);
    s_runtime_state.log.config.callback = callback;
    s_runtime_state.log.config.callback_user_data = user_data;
    lv_mutex_unlock(&s_runtime_state.log.mutex);
}

/* LogLevel 含负数（TRACE=-1），查找表下标需偏移：表下标 = level + LOG_LEVEL_INDEX_OFFSET。
 * 未显式列出的级别（如 LOG_LEVEL_OFF/LOG_LEVEL_ENUM_GUARD）默认 0 = lv_LOG_LEVEL_OFF，与 default 一致。 */
#define LOG_LEVEL_INDEX_OFFSET 1
static const int kLogLevelToLvLog[LOG_LEVEL_OFF + LOG_LEVEL_INDEX_OFFSET + 1] = {
    [LOG_LEVEL_TRACE + LOG_LEVEL_INDEX_OFFSET] = lv_LOG_LEVEL_DEBUG,  /* 追踪级别归入 DEBUG（主管道最低可输出级别） */
    [LOG_LEVEL_DEBUG + LOG_LEVEL_INDEX_OFFSET] = lv_LOG_LEVEL_DEBUG,
    [LOG_LEVEL_INFO + LOG_LEVEL_INDEX_OFFSET]  = lv_LOG_LEVEL_INFO,
    [LOG_LEVEL_WARN + LOG_LEVEL_INDEX_OFFSET]  = lv_LOG_LEVEL_WARNING,
    [LOG_LEVEL_ERROR + LOG_LEVEL_INDEX_OFFSET] = lv_LOG_LEVEL_ERROR,  /* ERROR/FATAL 归入 ERROR（主管道最高级别） */
    [LOG_LEVEL_FATAL + LOG_LEVEL_INDEX_OFFSET] = lv_LOG_LEVEL_ERROR,
};

/** 将 LOG_LEVEL_* 级别映射为 lv_internal.h 的 lv_LOG_LEVEL_*（数值越大越详细） */
static int runtime_log_level_to_lvlog(lvLogLevel level) {
    int idx = (int) level + LOG_LEVEL_INDEX_OFFSET;
    if (idx >= 0 && idx < (int) (sizeof(kLogLevelToLvLog) / sizeof(kLogLevelToLvLog[0])))
        return kLogLevelToLvLog[idx];
    return lv_LOG_LEVEL_OFF;
}

void lv_log_write(lvLogLevel level, const char *tag, const char *file, int line, const char *function, const char *fmt,
                  ...) {
    (void) function; /* 位置信息由 file/line 提供（function 主管道暂不消费） */

    if (!s_runtime_state.log.initialized) {
        return;
    }

    if (level < s_runtime_state.log.config.min_level) {
        return;
    }

    /* 格式化消息 */
    char message[lv_LOG_MSG_MAX_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    /* 委托统一日志主管道（lv_log_message -> debug_log）：
     * 级别过滤、时间戳、模块名、文件输出与环形缓冲区等均由主管道统一处理，
     * 避免本模块的彩色/多目标/回调输出实现与主管道重复。tag 作为前缀并入消息。 */
    if (tag && tag[0]) {
        lv_log_message(runtime_log_level_to_lvlog(level), file, line, "[%s] %s", tag, message);
    } else {
        lv_log_message(runtime_log_level_to_lvlog(level), file, line, "%s", message);
    }
}

/* ============== 性能监控实现 ============== */

bool lv_perf_init(void) {
    lv_lazy_lock_lock(&s_runtime_state.init_lock, runtime_init_mutex_func);
    if (s_runtime_state.perf.initialized) {
        lv_lazy_lock_unlock(&s_runtime_state.init_lock);
        return true;
    }

    memset(&s_runtime_state.perf, 0, sizeof(s_runtime_state.perf));
    lv_mutex_init(&s_runtime_state.perf.mutex);
    s_runtime_state.perf.initialized = true;
    lv_lazy_lock_unlock(&s_runtime_state.init_lock);
    return true;
}

void lv_perf_shutdown(void) {
    if (!s_runtime_state.perf.initialized) {
        return;
    }

    lv_mutex_lock(&s_runtime_state.perf.mutex);

    for (uint32_t i = 0; i < s_runtime_state.perf.timer_count; i++) {
        lv_free((void **) &s_runtime_state.perf.timers[i]);
    }
    for (uint32_t i = 0; i < s_runtime_state.perf.stats_count; i++) {
        lv_free((void **) &s_runtime_state.perf.stats[i]);
    }

    lv_mutex_unlock(&s_runtime_state.perf.mutex);
    lv_mutex_destroy(&s_runtime_state.perf.mutex);
    s_runtime_state.perf.initialized = false;
}

lvTimer *lv_timer_create(const char *name) {
    if (!s_runtime_state.perf.initialized || s_runtime_state.perf.timer_count >= MAX_TIMERS) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_STATE, "lv_timer_create: perf system not initialized or full");
    }

    lvTimer *timer = (lvTimer *) lv_calloc(1, sizeof(lvTimer));
    if (!timer) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_timer_create: calloc timer failed");
    }

    if (name) {
        strncpy(timer->name, name, sizeof(timer->name) - 1);
    }
    timer->state = TIMER_STOPPED;

    lv_mutex_lock(&s_runtime_state.perf.mutex);
    s_runtime_state.perf.timers[s_runtime_state.perf.timer_count++] = timer;
    lv_mutex_unlock(&s_runtime_state.perf.mutex);

    return timer;
}

void lv_timer_destroy(lvTimer *timer) {
    if (!timer) {
        return;
    }

    lv_mutex_lock(&s_runtime_state.perf.mutex);
    for (uint32_t i = 0; i < s_runtime_state.perf.timer_count; i++) {
        if (s_runtime_state.perf.timers[i] == timer) {
            s_runtime_state.perf.timers[i] = s_runtime_state.perf.timers[--s_runtime_state.perf.timer_count];
            break;
        }
    }
    lv_mutex_unlock(&s_runtime_state.perf.mutex);

    lv_free((void **) &timer);
}

void lv_timer_start(lvTimer *timer) {
    if (!timer) {
        return;
    }

    timer->start_time_ns = lv_get_time_ns();
    timer->state = TIMER_RUNNING;
    timer->call_count++;
}

int64_t lv_timer_stop(lvTimer *timer) {
    if (!timer || timer->state != TIMER_RUNNING) {
        return 0;
    }

    timer->elapsed_ns = lv_get_time_ns() - timer->start_time_ns;
    timer->total_ns += timer->elapsed_ns;
    timer->state = TIMER_STOPPED;

    return timer->elapsed_ns / 1000000;
}

void lv_timer_pause(lvTimer *timer) {
    if (!timer || timer->state != TIMER_RUNNING) {
        return;
    }

    timer->elapsed_ns += lv_get_time_ns() - timer->start_time_ns;
    timer->state = TIMER_PAUSED;
}

void lv_timer_resume(lvTimer *timer) {
    if (!timer || timer->state != TIMER_PAUSED) {
        return;
    }

    timer->start_time_ns = lv_get_time_ns();
    timer->state = TIMER_RUNNING;
}

void lv_timer_reset(lvTimer *timer) {
    if (!timer) {
        return;
    }

    timer->elapsed_ns = 0;
    timer->total_ns = 0;
    timer->call_count = 0;
    timer->state = TIMER_STOPPED;
}

int64_t lv_timer_elapsed_ms(const lvTimer *timer) {
    if (!timer) {
        return 0;
    }

    if (timer->state == TIMER_RUNNING) {
        return (lv_get_time_ns() - timer->start_time_ns) / 1000000;
    }
    return timer->elapsed_ns / 1000000;
}

int64_t lv_timer_elapsed_ns(const lvTimer *timer) {
    if (!timer) {
        return 0;
    }

    if (timer->state == TIMER_RUNNING) {
        return lv_get_time_ns() - timer->start_time_ns;
    }
    return timer->elapsed_ns;
}

lvPerfStats *lv_perf_stats_create(const char *name) {
    if (!s_runtime_state.perf.initialized || s_runtime_state.perf.stats_count >= MAX_PERF_STATS) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_STATE, "lv_perf_stats_create: perf system not initialized or full");
    }

    lvPerfStats *stats = (lvPerfStats *) lv_calloc(1, sizeof(lvPerfStats));
    if (!stats) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_perf_stats_create: calloc stats failed");
    }

    if (name) {
        strncpy(stats->name, name, sizeof(stats->name) - 1);
    }
    stats->min_val = 1e308;
    stats->max_val = -1e308;

    lv_mutex_lock(&s_runtime_state.perf.mutex);
    s_runtime_state.perf.stats[s_runtime_state.perf.stats_count++] = stats;
    lv_mutex_unlock(&s_runtime_state.perf.mutex);

    return stats;
}

void lv_perf_stats_destroy(lvPerfStats *stats) {
    if (!stats) {
        return;
    }

    lv_mutex_lock(&s_runtime_state.perf.mutex);
    for (uint32_t i = 0; i < s_runtime_state.perf.stats_count; i++) {
        if (s_runtime_state.perf.stats[i] == stats) {
            s_runtime_state.perf.stats[i] = s_runtime_state.perf.stats[--s_runtime_state.perf.stats_count];
            break;
        }
    }
    lv_mutex_unlock(&s_runtime_state.perf.mutex);

    lv_free((void **) &stats);
}

void lv_perf_stats_record(lvPerfStats *stats, double value) {
    if (!stats) {
        return;
    }

    lv_mutex_lock(&s_runtime_state.perf.mutex);

    /*
     * Welford 在线算法：避免朴素方差公式的灾难性抵消。
     *
     * 朴素公式：Var = (Σx² - (Σx)²/n) / (n-1)
     * 当数据均值远大于标准差时（如计时 n 秒级，差异 ms 级），
     * Σx² 和 (Σx)²/n 几乎相等，相减丢失几乎所有有效数字。
     *
     * Welford 算法（单遍增量）：
     *   n' = n + 1
     *   δ  = x - μ
     *   μ' = μ + δ / n'
     *   M₂' = M₂ + δ * (x - μ')
     *   Var = M₂ / (n - 1)
     */
    stats->count++;

    double delta = value - stats->mean;
    stats->mean += delta / (double) stats->count;
    double delta2 = value - stats->mean;
    stats->m2 += delta * delta2;

    stats->sum += value;
    stats->sum_sq += value * value;
    stats->last_val = value;
    stats->last_time_ns = lv_get_time_ns();

    if (value < stats->min_val) {
        stats->min_val = value;
    }
    if (value > stats->max_val) {
        stats->max_val = value;
    }

    /* 更新方差和标准差（Welford M₂ / (n-1)） */
    if (stats->count > 1) {
        stats->variance = stats->m2 / (double) (stats->count - 1);
        stats->std_dev = sqrt(stats->variance);
    }
    lv_mutex_unlock(&s_runtime_state.perf.mutex);
}

void lv_perf_stats_reset(lvPerfStats *stats) {
    if (!stats) {
        return;
    }

    memset(stats, 0, sizeof(lvPerfStats));
    stats->min_val = 1e308;
    stats->max_val = -1e308;
    stats->m2 = 0.0;
}

/* ============== 健康检查实现 ============== */

bool lv_health_init(void) {
    lv_lazy_lock_lock(&s_runtime_state.init_lock, runtime_init_mutex_func);
    if (s_runtime_state.health.initialized) {
        lv_lazy_lock_unlock(&s_runtime_state.init_lock);
        return true;
    }

    memset(&s_runtime_state.health, 0, sizeof(s_runtime_state.health));
    lv_mutex_init(&s_runtime_state.health.mutex);

    /* 默认阈值 */
    s_runtime_state.health.memory_warning_mb = 1024;  /* 1 GB */
    s_runtime_state.health.memory_critical_mb = 2048; /* 2 GB */
    s_runtime_state.health.cpu_warning_percent = 80;
    s_runtime_state.health.cpu_critical_percent = 95;

    s_runtime_state.health.initialized = true;
    lv_lazy_lock_unlock(&s_runtime_state.init_lock);
    return true;
}

void lv_health_shutdown(void) {
    if (!s_runtime_state.health.initialized) {
        return;
    }

    lv_mutex_destroy(&s_runtime_state.health.mutex);
    s_runtime_state.health.initialized = false;
}

void lv_health_set_memory_thresholds(double warning_mb, double critical_mb) {
    lv_mutex_lock(&s_runtime_state.health.mutex);
    s_runtime_state.health.memory_warning_mb = warning_mb;
    s_runtime_state.health.memory_critical_mb = critical_mb;
    lv_mutex_unlock(&s_runtime_state.health.mutex);
}

void lv_health_set_cpu_thresholds(double warning_percent, double critical_percent) {
    lv_mutex_lock(&s_runtime_state.health.mutex);
    s_runtime_state.health.cpu_warning_percent = warning_percent;
    s_runtime_state.health.cpu_critical_percent = critical_percent;
    lv_mutex_unlock(&s_runtime_state.health.mutex);
}

/* ============== 平台特定 CPU 使用率采样 ============== */

#ifdef _WIN32
static double get_cpu_usage_percent(void) {
    FILETIME idle1, kernel1, user1;
    FILETIME idle2, kernel2, user2;
    GetSystemTimes(&idle1, &kernel1, &user1);
    Sleep(100);
    GetSystemTimes(&idle2, &kernel2, &user2);
    ULONGLONG k1 = kernel1.dwLowDateTime | ((ULONGLONG) kernel1.dwHighDateTime << 32);
    ULONGLONG u1 = user1.dwLowDateTime | ((ULONGLONG) user1.dwHighDateTime << 32);
    ULONGLONG i1 = idle1.dwLowDateTime | ((ULONGLONG) idle1.dwHighDateTime << 32);
    ULONGLONG k2 = kernel2.dwLowDateTime | ((ULONGLONG) kernel2.dwHighDateTime << 32);
    ULONGLONG u2 = user2.dwLowDateTime | ((ULONGLONG) user2.dwHighDateTime << 32);
    ULONGLONG i2 = idle2.dwLowDateTime | ((ULONGLONG) idle2.dwHighDateTime << 32);
    ULONGLONG idle_diff = i2 - i1;
    ULONGLONG total_diff = (k2 - k1) + (u2 - u1);
    if (total_diff == 0)
        return 0.0;
    return 100.0 * (1.0 - (double) idle_diff / (double) total_diff);
}
#elif defined(__linux__)
static double get_cpu_usage_percent(void) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp)
        return 0.0;
    unsigned long long user1, nice1, sys1, idle1, iowait1, irq1, softirq1;
    if (fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu", &user1, &nice1, &sys1, &idle1, &iowait1, &irq1,
               &softirq1) != 7) {
        fclose(fp);
        return 0.0;
    }
    fclose(fp);
    usleep(100000); /* 100ms */
    fp = fopen("/proc/stat", "r");
    if (!fp)
        return 0.0;
    unsigned long long user2, nice2, sys2, idle2, iowait2, irq2, softirq2;
    if (fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu", &user2, &nice2, &sys2, &idle2, &iowait2, &irq2,
               &softirq2) != 7) {
        fclose(fp);
        return 0.0;
    }
    fclose(fp);
    unsigned long long total1 = user1 + nice1 + sys1 + idle1 + iowait1 + irq1 + softirq1;
    unsigned long long total2 = user2 + nice2 + sys2 + idle2 + iowait2 + irq2 + softirq2;
    unsigned long long total_diff = total2 - total1;
    unsigned long long idle_diff = (idle2 + iowait2) - (idle1 + iowait1);
    if (total_diff == 0)
        return 0.0;
    return 100.0 * (1.0 - (double) idle_diff / (double) total_diff);
}
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/types.h>
static double get_cpu_usage_percent(void) {
    host_cpu_load_info_data_t info1, info2;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t) &info1, &count) != KERN_SUCCESS)
        return 0.0;
    usleep(100000); /* 100ms */
    count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t) &info2, &count) != KERN_SUCCESS)
        return 0.0;
    unsigned long long total1 = (unsigned long long) info1.cpu_ticks[CPU_STATE_USER] +
                                info1.cpu_ticks[CPU_STATE_SYSTEM] + info1.cpu_ticks[CPU_STATE_IDLE] +
                                info1.cpu_ticks[CPU_STATE_NICE];
    unsigned long long total2 = (unsigned long long) info2.cpu_ticks[CPU_STATE_USER] +
                                info2.cpu_ticks[CPU_STATE_SYSTEM] + info2.cpu_ticks[CPU_STATE_IDLE] +
                                info2.cpu_ticks[CPU_STATE_NICE];
    unsigned long long idle_diff =
        (unsigned long long) info2.cpu_ticks[CPU_STATE_IDLE] - info1.cpu_ticks[CPU_STATE_IDLE];
    unsigned long long total_diff = total2 - total1;
    if (total_diff == 0)
        return 0.0;
    return 100.0 * (1.0 - (double) idle_diff / (double) total_diff);
}
#else
static double get_cpu_usage_percent(void) {
    return 0.0; /* 不支持的平台 */
}
#endif

lvHealthReport *lv_runtime_health_check(void) {
    lvHealthReport *report = (lvHealthReport *) lv_calloc(1, sizeof(lvHealthReport));
    if (!report) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_runtime_health_check: calloc report failed");
    }

    report->check_count = 5;
    report->checks = (lvHealthCheck *) lv_calloc(report->check_count, sizeof(lvHealthCheck));
    if (!report->checks) {
        lv_free((void **) &report);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_runtime_health_check: calloc checks failed");
    }

    report->timestamp_ms = lv_get_time_ns() / 1000000;
    report->overall = HEALTH_OK;

    /* 内存检查 */
    lvHealthCheck *check = &report->checks[0];
    strncpy(check->name, "Memory Usage", sizeof(check->name) - 1);
    check->threshold_warning = s_runtime_state.health.memory_warning_mb;
    check->threshold_critical = s_runtime_state.health.memory_critical_mb;

#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    check->value = (double) (status.ullTotalVirtual - status.ullAvailVirtual) / (1024 * 1024);
#else
    FILE *fp = fopen("/proc/self/status", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                check->value = lv_parse_double(line + 6, &check->value);
                break;
            }
        }
        fclose(fp);
    }
#endif

    if (check->value >= check->threshold_critical) {
        check->status = HEALTH_CRITICAL;
        snprintf(check->message, sizeof(check->message), "Memory usage critical: %.1f MB", check->value);
        report->overall = HEALTH_CRITICAL;
    } else if (check->value >= check->threshold_warning) {
        check->status = HEALTH_WARNING;
        snprintf(check->message, sizeof(check->message), "Memory usage high: %.1f MB", check->value);
        if (report->overall < HEALTH_WARNING) {
            report->overall = HEALTH_WARNING;
        }
    } else {
        check->status = HEALTH_OK;
        snprintf(check->message, sizeof(check->message), "Memory usage normal: %.1f MB", check->value);
    }

    /* CPU 检查 */
    check = &report->checks[1];
    strncpy(check->name, "CPU Usage", sizeof(check->name) - 1);
    check->threshold_warning = s_runtime_state.health.cpu_warning_percent;
    check->threshold_critical = s_runtime_state.health.cpu_critical_percent;
    check->value = get_cpu_usage_percent();

    if (check->value >= check->threshold_critical) {
        check->status = HEALTH_CRITICAL;
        snprintf(check->message, sizeof(check->message), "CPU usage critical: %.1f%%", check->value);
        report->overall = HEALTH_CRITICAL;
    } else if (check->value >= check->threshold_warning) {
        check->status = HEALTH_WARNING;
        snprintf(check->message, sizeof(check->message), "CPU usage high: %.1f%%", check->value);
        if (report->overall < HEALTH_WARNING) {
            report->overall = HEALTH_WARNING;
        }
    } else {
        check->status = HEALTH_OK;
        snprintf(check->message, sizeof(check->message), "CPU usage normal: %.1f%%", check->value);
    }

    /* 线程检查 */
    check = &report->checks[2];
    strncpy(check->name, "Thread Count", sizeof(check->name) - 1);
    /* 线程检查：通过环境变量 lv_MONITOR_THREADS 配置，默认 1，范围 [1, 64] */
    int monitor_threads = 1;
    const char *env_threads = getenv("lv_MONITOR_THREADS");
    if (env_threads && env_threads[0] != '\0') {
        long parsed = strtol(env_threads, NULL, 10);
        if (parsed < 1)
            parsed = 1;
        if (parsed > 64)
            parsed = 64;
        monitor_threads = (int) parsed;
    }
    check->value = (double) monitor_threads;
    check->status = HEALTH_OK;
    snprintf(check->message, sizeof(check->message), "Thread count: %d (configurable via lv_MONITOR_THREADS)",
             monitor_threads);

    /* 计时器检查 */
    check = &report->checks[3];
    strncpy(check->name, "Active Timers", sizeof(check->name) - 1);
    check->value = s_runtime_state.perf.timer_count;
    check->status = HEALTH_OK;
    snprintf(check->message, sizeof(check->message), "%u active timers", s_runtime_state.perf.timer_count);

    /* 性能统计检查 */
    check = &report->checks[4];
    strncpy(check->name, "Performance Stats", sizeof(check->name) - 1);
    check->value = s_runtime_state.perf.stats_count;
    check->status = HEALTH_OK;
    snprintf(check->message, sizeof(check->message), "%u performance stats tracked", s_runtime_state.perf.stats_count);

    return report;
}

void lv_health_report_destroy(lvHealthReport *report) {
    if (!report) {
        return;
    }
    lv_free((void **) &report->checks);
    lv_free((void **) &report);
}

/* ============== 诊断报告实现 ============== */

lvDiagnostics *lv_diagnostics_generate(void) {
    lvDiagnostics *diag = (lvDiagnostics *) lv_calloc(1, sizeof(lvDiagnostics));
    if (!diag) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_diagnostics_generate: calloc diag failed");
    }

    /* 基本信息 */
    strncpy(diag->version, "3.3.0", sizeof(diag->version) - 1);
    strncpy(diag->build_date, __DATE__ " " __TIME__, sizeof(diag->build_date) - 1);
    diag->uptime_ms = lv_get_time_ns() / 1000000;

    /* 内存统计 - 从 lv 内存管理器获取实际数据 */
    {
        MemoryStats mem_stats;
        lv_get_memory_stats(&mem_stats);
        diag->memory_total = mem_stats.current_used;
        diag->memory_peak = mem_stats.peak_used;
        diag->alloc_count = (uint64_t) mem_stats.allocation_count;
        diag->free_count = (uint64_t) mem_stats.free_count;
    }

    /* 性能统计 */
    diag->proof_count = 0;
    diag->solve_count = 0;
    diag->avg_proof_time_ms = 0;
    diag->avg_solve_time_ms = 0;

    /* 错误统计 */
    diag->error_count = 0;
    diag->warning_count = 0;

    /* 健康状态 */
    diag->health = HEALTH_OK;

    /* 系统信息 */
#ifdef _WIN32
    strncpy(diag->os_info, "Windows", sizeof(diag->os_info) - 1);
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    diag->cpu_cores = sys_info.dwNumberOfProcessors;

    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    GlobalMemoryStatusEx(&mem_status);
    diag->total_memory_mb = (uint32_t) (mem_status.ullTotalPhys / (1024 * 1024));
#else
    strncpy(diag->os_info, "Linux/Unix", sizeof(diag->os_info) - 1);
    diag->cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
    diag->total_memory_mb = (uint32_t) (sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE) / (1024 * 1024));
#endif

    return diag;
}

void lv_diagnostics_destroy(lvDiagnostics *diag) {
    lv_free((void **) &diag);
}

bool lv_diagnostics_write_file(const lvDiagnostics *diag, const char *path) {
    if (!diag || !path) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_diagnostics_write_file: NULL diag or path");
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_IO, "lv_diagnostics_write_file: fopen failed");
    }

    fprintf(fp, "========== Lv-00 Diagnostics Report ==========\n\n");
    fprintf(fp, "Version: %s\n", diag->version);
    fprintf(fp, "Build Date: %s\n", diag->build_date);
    fprintf(fp, "Uptime: %lld ms\n", (long long) diag->uptime_ms);
    fprintf(fp, "\n--- Memory ---\n");
    fprintf(fp, "Total Used: %llu bytes\n", (unsigned long long) diag->memory_total);
    fprintf(fp, "Peak Used: %llu bytes\n", (unsigned long long) diag->memory_peak);
    fprintf(fp, "Allocations: %llu\n", (unsigned long long) diag->alloc_count);
    fprintf(fp, "Frees: %llu\n", (unsigned long long) diag->free_count);
    fprintf(fp, "\n--- Performance ---\n");
    fprintf(fp, "Proof Count: %llu\n", (unsigned long long) diag->proof_count);
    fprintf(fp, "Solve Count: %llu\n", (unsigned long long) diag->solve_count);
    fprintf(fp, "Avg Proof Time: %.2f ms\n", diag->avg_proof_time_ms);
    fprintf(fp, "Avg Solve Time: %.2f ms\n", diag->avg_solve_time_ms);
    fprintf(fp, "\n--- Errors ---\n");
    fprintf(fp, "Error Count: %llu\n", (unsigned long long) diag->error_count);
    fprintf(fp, "Warning Count: %llu\n", (unsigned long long) diag->warning_count);
    if (diag->last_error[0]) {
        fprintf(fp, "Last Error: %s\n", diag->last_error);
    }
    fprintf(fp, "\n--- System ---\n");
    fprintf(fp, "OS: %s\n", diag->os_info);
    fprintf(fp, "CPU Cores: %u\n", diag->cpu_cores);
    fprintf(fp, "Total Memory: %u MB\n", diag->total_memory_mb);
    fprintf(fp, "\n==============================================\n");

    fclose(fp);
    return true;
}

char *lv_diagnostics_to_json(const lvDiagnostics *diag) {
    if (!diag) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_diagnostics_to_json: diag is NULL");
    }

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 4096))
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_diagnostics_to_json: json_buf_init failed");

    lv_json_buf_append_raw(&buf, "{");
    lv_json_buf_append_raw(&buf, "\"version\":");
    lv_json_buf_append_string(&buf, diag->version);
    lv_json_buf_append_raw(&buf, ",");
    lv_json_buf_append_raw(&buf, "\"build_date\":");
    lv_json_buf_append_string(&buf, diag->build_date);
    lv_json_buf_append_raw(&buf, ",");
    lv_json_buf_append_fmt(&buf, "\"uptime_ms\":%lld,", (long long) diag->uptime_ms);
    lv_json_buf_append_raw(&buf, "\"memory\":{");
    lv_json_buf_append_fmt(&buf, "\"total\":%llu,", (unsigned long long) diag->memory_total);
    lv_json_buf_append_fmt(&buf, "\"peak\":%llu,", (unsigned long long) diag->memory_peak);
    lv_json_buf_append_fmt(&buf, "\"alloc_count\":%llu,", (unsigned long long) diag->alloc_count);
    lv_json_buf_append_fmt(&buf, "\"free_count\":%llu", (unsigned long long) diag->free_count);
    lv_json_buf_append_raw(&buf, "},");
    lv_json_buf_append_raw(&buf, "\"performance\":{");
    lv_json_buf_append_fmt(&buf, "\"proof_count\":%llu,", (unsigned long long) diag->proof_count);
    lv_json_buf_append_fmt(&buf, "\"solve_count\":%llu,", (unsigned long long) diag->solve_count);
    lv_json_buf_append_fmt(&buf, "\"avg_proof_time_ms\":%.2f,", diag->avg_proof_time_ms);
    lv_json_buf_append_fmt(&buf, "\"avg_solve_time_ms\":%.2f", diag->avg_solve_time_ms);
    lv_json_buf_append_raw(&buf, "},");
    lv_json_buf_append_raw(&buf, "\"errors\":{");
    lv_json_buf_append_fmt(&buf, "\"count\":%llu,", (unsigned long long) diag->error_count);
    lv_json_buf_append_fmt(&buf, "\"warning_count\":%llu,", (unsigned long long) diag->warning_count);
    lv_json_buf_append_raw(&buf, "\"last_error\":");
    lv_json_buf_append_string(&buf, diag->last_error);
    lv_json_buf_append_raw(&buf, "},");
    lv_json_buf_append_raw(&buf, "\"system\":{");
    lv_json_buf_append_raw(&buf, "\"os\":");
    lv_json_buf_append_string(&buf, diag->os_info);
    lv_json_buf_append_raw(&buf, ",");
    lv_json_buf_append_fmt(&buf, "\"cpu_cores\":%u,", diag->cpu_cores);
    lv_json_buf_append_fmt(&buf, "\"total_memory_mb\":%u", diag->total_memory_mb);
    lv_json_buf_append_raw(&buf, "}}");

    return lv_json_buf_finalize(&buf);
}

/* ============== 事件追踪实现 ============== */

static void event_bus_init_func(void) {
    lv_event_bus_init(&s_runtime_state.event_bus, NULL);
}

bool lv_event_trace_init(uint32_t max_events) {
    lv_lazy_lock_lock(&s_runtime_state.init_lock, runtime_init_mutex_func);
    if (s_runtime_state.event.initialized) {
        lv_lazy_lock_unlock(&s_runtime_state.init_lock);
        return true;
    }

    /* 使用请求的大小，最小为 MAX_EVENTS */
    uint32_t actual_max = (max_events > 0) ? max_events : MAX_EVENTS;

    memset(&s_runtime_state.event, 0, sizeof(s_runtime_state.event));
    s_runtime_state.event.events = (lvEventRecord *) lv_calloc(actual_max, sizeof(lvEventRecord));
    if (!s_runtime_state.event.events) {
        lv_lazy_lock_unlock(&s_runtime_state.init_lock);
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "lv_event_trace_init: calloc events failed");
    }
    s_runtime_state.event.max_events = actual_max;
    lv_mutex_init(&s_runtime_state.event.mutex);
    s_runtime_state.event.initialized = true;
    lv_lazy_lock_unlock(&s_runtime_state.init_lock);
    return true;
}

void lv_event_trace_shutdown(void) {
    if (!s_runtime_state.event.initialized) {
        return;
    }

    lv_free((void **) &s_runtime_state.event.events);
    lv_event_bus_cleanup(&s_runtime_state.event_bus);
    lv_mutex_destroy(&s_runtime_state.event.mutex);
    s_runtime_state.event.initialized = false;
}

void lv_event_trace_record(lvEventType type, const char *name, const char *data) {
    if (!s_runtime_state.event.initialized || s_runtime_state.event.event_count >= s_runtime_state.event.max_events) {
        return;
    }

    lv_mutex_lock(&s_runtime_state.event.mutex);

    lvEventRecord *event = &s_runtime_state.event.events[s_runtime_state.event.event_count++];
    event->type = type;
    event->timestamp_ns = lv_get_time_ns();
    event->duration_ns = 0;
    event->thread_id = (int)lv_thread_id();

    if (name) {
        strncpy(event->name, name, sizeof(event->name) - 1);
    }
    if (data) {
        strncpy(event->data, data, sizeof(event->data) - 1);
    }

    lv_mutex_unlock(&s_runtime_state.event.mutex);

    lv_once(&s_runtime_state.event_bus_once, event_bus_init_func);
    lv_event_emit(&s_runtime_state.event_bus, (int)type, (void*)(intptr_t)event->duration_ns);
}

int lv_event_trace_begin(lvEventType type, const char *name) {
    if (!s_runtime_state.event.initialized || s_runtime_state.event.event_count >= s_runtime_state.event.max_events) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "lv_event_trace_begin: event system not initialized or full");
    }

    lv_mutex_lock(&s_runtime_state.event.mutex);

    int id = (int) s_runtime_state.event.event_count;
    lvEventRecord *event = &s_runtime_state.event.events[s_runtime_state.event.event_count++];
    event->type = type;
    event->timestamp_ns = lv_get_time_ns();
    event->thread_id = (int)lv_thread_id();

    if (name) {
        strncpy(event->name, name, sizeof(event->name) - 1);
    }

    lv_mutex_unlock(&s_runtime_state.event.mutex);

    lv_once(&s_runtime_state.event_bus_once, event_bus_init_func);
    lv_event_emit(&s_runtime_state.event_bus, (int)type, (void*)(intptr_t)0);

    return id;
}

void lv_event_trace_end(int event_id, const char *data) {
    if (!s_runtime_state.event.initialized || event_id < 0 || event_id >= (int) s_runtime_state.event.event_count) {
        return;
    }

    lv_mutex_lock(&s_runtime_state.event.mutex);

    lvEventRecord *event = &s_runtime_state.event.events[event_id];
    event->duration_ns = lv_get_time_ns() - event->timestamp_ns;

    if (data) {
        strncpy(event->data, data, sizeof(event->data) - 1);
    }

    lv_mutex_unlock(&s_runtime_state.event.mutex);

    lv_once(&s_runtime_state.event_bus_once, event_bus_init_func);
    lv_event_emit(&s_runtime_state.event_bus, (int)event->type, (void*)(intptr_t)event->duration_ns);
}

uint32_t lv_event_trace_get_all(lvEventRecord **out_events, uint32_t max_count) {
    if (!s_runtime_state.event.initialized || !out_events) {
        return 0;
    }

    lv_mutex_lock(&s_runtime_state.event.mutex);

    uint32_t count = s_runtime_state.event.event_count < max_count ? s_runtime_state.event.event_count : max_count;
    *out_events = (lvEventRecord *) lv_calloc((size_t) count, sizeof(lvEventRecord));
    if (*out_events) {
        memcpy(*out_events, s_runtime_state.event.events, count * sizeof(lvEventRecord));
    }

    lv_mutex_unlock(&s_runtime_state.event.mutex);

    return count;
}

void lv_event_trace_clear(void) {
    if (!s_runtime_state.event.initialized) {
        return;
    }

    lv_mutex_lock(&s_runtime_state.event.mutex);
    s_runtime_state.event.event_count = 0;
    lv_mutex_unlock(&s_runtime_state.event.mutex);
}

/** @brief 事件类型 -> Chrome trace 元信息 查找表（指定初始化器，编译器校验 lvEventType 对齐）
 *  ph  - Chrome trace phase 字符（B=开始, E=结束；未列入的事件保持默认 X）
 *  cat - 事件分类（proof 事件归 "proof"，其余归 "other"） */
typedef struct {
    const char *ph;  /**< phase 字符（NULL 表示无映射，使用默认 X/other） */
    const char *cat; /**< 事件分类 */
} lvEventTraceMeta;

/** @brief 事件类型 -> trace 元信息 查找表（指定初始化器） */
static const lvEventTraceMeta kEventTraceMeta[] = {
    [EVENT_TYPE_PROOF_START] = {"B", "proof"},
    [EVENT_TYPE_PROOF_END] = {"E", "proof"},
    [EVENT_TYPE_SOLVE_START] = {"B", "other"},
    [EVENT_TYPE_SOLVE_END] = {"E", "other"},
};

/* 编译期断言：4 个 trace 相关事件类型均在表内，防止枚举调整后漏配 */
_Static_assert(EVENT_TYPE_PROOF_START < (int) lv_ARRAY_SIZE(kEventTraceMeta) &&
                   EVENT_TYPE_PROOF_END < (int) lv_ARRAY_SIZE(kEventTraceMeta) &&
                   EVENT_TYPE_SOLVE_START < (int) lv_ARRAY_SIZE(kEventTraceMeta) &&
                   EVENT_TYPE_SOLVE_END < (int) lv_ARRAY_SIZE(kEventTraceMeta),
               "kEventTraceMeta 表未覆盖全部 trace 相关事件类型");

bool lv_event_trace_export_chrome(const char *path) {
    if (!s_runtime_state.event.initialized || !path) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "lv_event_trace_export_chrome: not initialized or NULL path");
    }

    lv_mutex_lock(&s_runtime_state.event.mutex);

    /* 使用 lvJsonBuf 构建 JSON */
    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, (size_t) s_runtime_state.event.event_count * 256 + 64)) {
        lv_mutex_unlock(&s_runtime_state.event.mutex);
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "lv_event_trace_export_chrome: json_buf_init failed");
    }

    lv_json_buf_append_raw(&buf, "[\n");

    for (uint32_t i = 0; i < s_runtime_state.event.event_count; i++) {
        lvEventRecord *event = &s_runtime_state.event.events[i];

        const char *type_str = "X";
        const char *cat = "other";
        if ((unsigned) event->type < lv_ARRAY_SIZE(kEventTraceMeta) && kEventTraceMeta[event->type].ph != NULL) {
            type_str = kEventTraceMeta[event->type].ph;
            cat = kEventTraceMeta[event->type].cat;
        }

        /* event->name 经 lv_json_buf_append_string 自动 JSON 转义，cat/ph 为内部固定串无需转义 */
        lv_json_buf_append_raw(&buf, "  {\"name\":");
        lv_json_buf_append_string(&buf, event->name);
        lv_json_buf_append_fmt(&buf,
                     ",\"cat\":\"%s\",\"ph\":\"%s\",\"ts\":%lld,\"dur\":%lld,\"pid\":1,\"tid\":%d}%s\n",
                     cat, type_str,
                     (long long) (event->timestamp_ns / 1000),
                     (long long) (event->duration_ns / 1000),
                     event->thread_id,
                     (i < s_runtime_state.event.event_count - 1) ? "," : "");
    }

    lv_mutex_unlock(&s_runtime_state.event.mutex);

    lv_json_buf_append_raw(&buf, "]\n");

    /* 写入文件 */
    FILE *fp = fopen(path, "w");
    if (!fp) {
        lv_json_buf_free(&buf);
        lv_RETURN_ERROR_BOOL(lv_ERROR_IO, "lv_event_trace_export_chrome: fopen failed");
    }
    fwrite(buf.buffer, 1, buf.pos, fp);
    fclose(fp);

    lv_json_buf_free(&buf);
    return true;
}
