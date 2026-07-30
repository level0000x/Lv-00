/**
 * @file runtime_monitor.c
 * @brief 运行时监控与日志系统实现
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"

#include "runtime_monitor.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_parse_utils.h"

#include "config.h" /* lv_LOCALTIME */
#include "lv_utils.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#endif

/* ============== 内部常量 ============== */

/** 日志缓冲区大小 */
#define LOG_BUFFER_SIZE 8192

/** 最大计时器数量 */
#define MAX_TIMERS 256

/** 最大性能统计数量 */
#define MAX_PERF_STATS 256

/** 最大事件数量 */
#define MAX_EVENTS 10000

/* ============== 平台抽象层 ============== */

#include "lv/lv_thread.h"

/* ============== 日志系统实现 ============== */

static struct {
    lvLogConfig config;
    FILE *log_file;
    lv_mutex_t mutex;
    bool initialized;
    uint64_t current_file_size;
} g_log_system = {0};

static const char *level_strings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"};

static const char *level_colors[] = {"\033[37m",   /* TRACE: 白色 */
                                     "\033[36m",   /* DEBUG: 青色 */
                                     "\033[32m",   /* INFO: 绿色 */
                                     "\033[33m",   /* WARN: 黄色 */
                                     "\033[31m",   /* ERROR: 红色 */
                                     "\033[35;1m", /* FATAL: 紫色加粗 */
                                     ""};

static lv_mutex_t g_log_init_mutex;
static lv_once_t g_log_init_once = lv_ONCE_INIT;

static void log_init_mutex_func(void) {
    lv_mutex_init(&g_log_init_mutex);
}

bool lv_log_init(const lvLogConfig *config) {
    lv_once(&g_log_init_once, log_init_mutex_func);
    lv_mutex_lock(&g_log_init_mutex);
    if (g_log_system.initialized) {
        lv_mutex_unlock(&g_log_init_mutex);
        return true;
    }

    memset(&g_log_system, 0, sizeof(g_log_system));

    if (config) {
        memcpy(&g_log_system.config, config, sizeof(lvLogConfig));
    } else {
        /* 默认配置 */
        g_log_system.config.min_level = LOG_LEVEL_INFO;
        g_log_system.config.targets = LOG_TARGET_STDOUT;
        g_log_system.config.include_timestamp = true;
        g_log_system.config.include_location = false;
        g_log_system.config.include_thread_id = false;
        g_log_system.config.colored_output = true;
        g_log_system.config.max_file_size = 10 * 1024 * 1024; /* 10 MB */
        g_log_system.config.max_backup_files = 5;
    }

    lv_mutex_init(&g_log_system.mutex);

    /* 打开日志文件 */
    if ((g_log_system.config.targets & LOG_TARGET_FILE) && g_log_system.config.file_path[0]) {
        g_log_system.log_file = fopen(g_log_system.config.file_path, "a");
        if (!g_log_system.log_file) {
            g_log_system.config.targets &= ~LOG_TARGET_FILE;
        }
    }

    g_log_system.initialized = true;
    lv_mutex_unlock(&g_log_init_mutex);
    return true;
}

void lv_log_shutdown(void) {
    if (!g_log_system.initialized) {
        return;
    }

    if (g_log_system.log_file) {
        fclose(g_log_system.log_file);
        g_log_system.log_file = NULL;
    }

    lv_mutex_destroy(&g_log_system.mutex);
    g_log_system.initialized = false;
}

void lv_log_set_level(lvLogLevel level) {
    if (level >= LOG_LEVEL_TRACE && level <= LOG_LEVEL_OFF) {
        lv_mutex_lock(&g_log_system.mutex);
        g_log_system.config.min_level = level;
        lv_mutex_unlock(&g_log_system.mutex);
    }
}

void lv_log_set_targets(lvLogTarget targets) {
    /* 线程安全：加锁保护全局日志目标的修改 */
    lv_mutex_lock(&g_log_system.mutex);
    g_log_system.config.targets = targets;
    lv_mutex_unlock(&g_log_system.mutex);
}

bool lv_log_set_file(const char *path) {
    if (!path) {
        return false;
    }

    lv_mutex_lock(&g_log_system.mutex);

    /* 先打开新文件，确保成功后再关闭旧文件，避免 fopen 失败导致日志丢失 */
    FILE *new_file = fopen(path, "a");
    if (!new_file) {
        lv_mutex_unlock(&g_log_system.mutex);
        return false;
    }

    /* 新文件打开成功，关闭旧文件 */
    if (g_log_system.log_file) {
        fclose(g_log_system.log_file);
    }

    strncpy(g_log_system.config.file_path, path, sizeof(g_log_system.config.file_path) - 1);
    g_log_system.log_file = new_file;
    g_log_system.current_file_size = 0;

    lv_mutex_unlock(&g_log_system.mutex);

    return true;
}

void lv_log_set_callback(lvLogCallback callback, void *user_data) {
    /* 线程安全：加锁保护回调和用户数据的修改，防止与日志写入并发冲突 */
    lv_mutex_lock(&g_log_system.mutex);
    g_log_system.config.callback = callback;
    g_log_system.config.callback_user_data = user_data;
    lv_mutex_unlock(&g_log_system.mutex);
}

static void rotate_log_file(void) {
    if (!g_log_system.log_file || g_log_system.current_file_size < g_log_system.config.max_file_size) {
        return;
    }

    fclose(g_log_system.log_file);

    /* 重命名现有文件 */
    char old_path[512], new_path[512];
    strncpy(old_path, g_log_system.config.file_path, sizeof(old_path) - 1);

    for (int i = g_log_system.config.max_backup_files - 1; i >= 0; i--) {
        if (i == 0) {
            snprintf(new_path, sizeof(new_path), "%s.1", old_path);
        } else {
            snprintf(new_path, sizeof(new_path), "%s.%d", old_path, i + 1);
        }

        char prev_path[512];
        if (i == 0) {
            strncpy(prev_path, old_path, sizeof(prev_path) - 1);
        } else {
            snprintf(prev_path, sizeof(prev_path), "%s.%d", old_path, i);
        }

        /* 删除旧备份 */
        remove(new_path);
        rename(prev_path, new_path);
    }

    g_log_system.log_file = fopen(g_log_system.config.file_path, "a");
    if (!g_log_system.log_file) {
        /* 日志文件打开失败，不清除 current_file_size，后续写入将被丢弃 */
        return;
    }
    g_log_system.current_file_size = 0;
}

void lv_log_write(lvLogLevel level, const char *tag, const char *file, int line, const char *function, const char *fmt,
                  ...) {
    if (!g_log_system.initialized) {
        return;
    }

    if (level < g_log_system.config.min_level) {
        return;
    }

    lv_mutex_lock(&g_log_system.mutex);

    /* 格式化消息 */
    char message[lv_LOG_MSG_MAX_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    /* 构建日志记录 */
    lvLogRecord record;
    record.level = level;
    record.line = line;
    record.timestamp_ms = lv_get_time_ns() / 1000000;
    record.thread_id = (int)lv_thread_id();

    if (tag) {
        strncpy(record.tag, tag, sizeof(record.tag) - 1);
    } else {
        record.tag[0] = '\0';
    }
    strncpy(record.message, message, sizeof(record.message) - 1);
    strncpy(record.file, file ? file : "", sizeof(record.file) - 1);
    strncpy(record.function, function ? function : "", sizeof(record.function) - 1);

    /* 输出到回调 */
    if ((g_log_system.config.targets & LOG_TARGET_CALLBACK) && g_log_system.config.callback) {
        g_log_system.config.callback(&record, g_log_system.config.callback_user_data);
    }

    /* 构建输出字符串 */
    char output[LOG_BUFFER_SIZE];
    int pos = 0;

    if (g_log_system.config.include_timestamp) {
        time_t now = time(NULL);
        struct tm tm_info;
        if (lv_LOCALTIME(&now, &tm_info) == 0)
            pos += strftime(output + pos, sizeof(output) - pos, "%Y-%m-%d %H:%M:%S", &tm_info);
        if (pos < (int) sizeof(output))
            pos += snprintf(output + pos, sizeof(output) - pos, ".%03d ", (int) (record.timestamp_ms % 1000));
    }

    if (g_log_system.config.colored_output) {
        if (pos < (int) sizeof(output))
            pos += snprintf(output + pos, sizeof(output) - pos, "%s%-5s\033[0m ", level_colors[level],
                            level_strings[level]);
    } else {
        if (pos < (int) sizeof(output))
            pos += snprintf(output + pos, sizeof(output) - pos, "%-5s ", level_strings[level]);
    }

    if (record.tag[0] && pos < (int) sizeof(output)) {
        pos += snprintf(output + pos, sizeof(output) - pos, "[%s] ", record.tag);
    }

    if (g_log_system.config.include_thread_id && pos < (int) sizeof(output)) {
        pos += snprintf(output + pos, sizeof(output) - pos, "(T%d) ", record.thread_id);
    }

    if (pos < (int) sizeof(output))
        pos += snprintf(output + pos, sizeof(output) - pos, "%s", record.message);

    if (g_log_system.config.include_location && pos < (int) sizeof(output)) {
        const char *basename = strrchr(record.file, '/');
        if (!basename)
            basename = strrchr(record.file, '\\');
        basename = basename ? basename + 1 : record.file;
        pos += snprintf(output + pos, sizeof(output) - pos, " (%s:%d in %s)", basename, record.line, record.function);
    }

    if (pos < (int) sizeof(output))
        pos += snprintf(output + pos, sizeof(output) - pos, "\n");

    /* 输出到标准输出/错误 */
    if (g_log_system.config.targets & LOG_TARGET_STDOUT) {
        fputs(output, stdout);
    }
    if (g_log_system.config.targets & LOG_TARGET_STDERR) {
        fputs(output, stderr);
    }

    /* 输出到文件 */
    if ((g_log_system.config.targets & LOG_TARGET_FILE) && g_log_system.log_file) {
        size_t len = strlen(output);
        size_t written = fwrite(output, 1, len, g_log_system.log_file);
        if (written != len) {
            lv_LOG_WARN_NT("日志文件写入不完整（期望 %zu, 实际 %zu）", len, written);
        }
        fflush(g_log_system.log_file);
        g_log_system.current_file_size += len;

        rotate_log_file();
    }

    lv_mutex_unlock(&g_log_system.mutex);
}

/* ============== 性能监控实现 ============== */

static struct {
    lvTimer *timers[MAX_TIMERS];
    uint32_t timer_count;
    lvPerfStats *stats[MAX_PERF_STATS];
    uint32_t stats_count;
    lv_mutex_t mutex;
    bool initialized;
} g_perf_system = {0};

static lv_mutex_t g_perf_init_mutex;
static lv_once_t g_perf_init_once = lv_ONCE_INIT;

static void perf_init_mutex_func(void) {
    lv_mutex_init(&g_perf_init_mutex);
}

bool lv_perf_init(void) {
    lv_once(&g_perf_init_once, perf_init_mutex_func);
    lv_mutex_lock(&g_perf_init_mutex);
    if (g_perf_system.initialized) {
        lv_mutex_unlock(&g_perf_init_mutex);
        return true;
    }

    memset(&g_perf_system, 0, sizeof(g_perf_system));
    lv_mutex_init(&g_perf_system.mutex);
    g_perf_system.initialized = true;
    lv_mutex_unlock(&g_perf_init_mutex);
    return true;
}

void lv_perf_shutdown(void) {
    if (!g_perf_system.initialized) {
        return;
    }

    lv_mutex_lock(&g_perf_system.mutex);

    for (uint32_t i = 0; i < g_perf_system.timer_count; i++) {
        lv_free((void **) &g_perf_system.timers[i]);
    }
    for (uint32_t i = 0; i < g_perf_system.stats_count; i++) {
        lv_free((void **) &g_perf_system.stats[i]);
    }

    lv_mutex_unlock(&g_perf_system.mutex);
    lv_mutex_destroy(&g_perf_system.mutex);
    g_perf_system.initialized = false;
}

lvTimer *lv_timer_create(const char *name) {
    if (!g_perf_system.initialized || g_perf_system.timer_count >= MAX_TIMERS) {
        return NULL;
    }

    lvTimer *timer = (lvTimer *) lv_calloc(1, sizeof(lvTimer));
    if (!timer) {
        return NULL;
    }

    if (name) {
        strncpy(timer->name, name, sizeof(timer->name) - 1);
    }
    timer->state = TIMER_STOPPED;

    lv_mutex_lock(&g_perf_system.mutex);
    g_perf_system.timers[g_perf_system.timer_count++] = timer;
    lv_mutex_unlock(&g_perf_system.mutex);

    return timer;
}

void lv_timer_destroy(lvTimer *timer) {
    if (!timer) {
        return;
    }

    lv_mutex_lock(&g_perf_system.mutex);
    for (uint32_t i = 0; i < g_perf_system.timer_count; i++) {
        if (g_perf_system.timers[i] == timer) {
            g_perf_system.timers[i] = g_perf_system.timers[--g_perf_system.timer_count];
            break;
        }
    }
    lv_mutex_unlock(&g_perf_system.mutex);

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
    if (!g_perf_system.initialized || g_perf_system.stats_count >= MAX_PERF_STATS) {
        return NULL;
    }

    lvPerfStats *stats = (lvPerfStats *) lv_calloc(1, sizeof(lvPerfStats));
    if (!stats) {
        return NULL;
    }

    if (name) {
        strncpy(stats->name, name, sizeof(stats->name) - 1);
    }
    stats->min_val = 1e308;
    stats->max_val = -1e308;

    lv_mutex_lock(&g_perf_system.mutex);
    g_perf_system.stats[g_perf_system.stats_count++] = stats;
    lv_mutex_unlock(&g_perf_system.mutex);

    return stats;
}

void lv_perf_stats_destroy(lvPerfStats *stats) {
    if (!stats) {
        return;
    }

    lv_mutex_lock(&g_perf_system.mutex);
    for (uint32_t i = 0; i < g_perf_system.stats_count; i++) {
        if (g_perf_system.stats[i] == stats) {
            g_perf_system.stats[i] = g_perf_system.stats[--g_perf_system.stats_count];
            break;
        }
    }
    lv_mutex_unlock(&g_perf_system.mutex);

    lv_free((void **) &stats);
}

void lv_perf_stats_record(lvPerfStats *stats, double value) {
    if (!stats) {
        return;
    }

    lv_mutex_lock(&g_perf_system.mutex);

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
    lv_mutex_unlock(&g_perf_system.mutex);
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

static struct {
    double memory_warning_mb;
    double memory_critical_mb;
    double cpu_warning_percent;
    double cpu_critical_percent;
    lv_mutex_t mutex;
    bool initialized;
} g_health_system = {0};

static lv_mutex_t g_health_init_mutex;
static lv_once_t g_health_init_once = lv_ONCE_INIT;

static void health_init_mutex_func(void) {
    lv_mutex_init(&g_health_init_mutex);
}

bool lv_health_init(void) {
    lv_once(&g_health_init_once, health_init_mutex_func);
    lv_mutex_lock(&g_health_init_mutex);
    if (g_health_system.initialized) {
        lv_mutex_unlock(&g_health_init_mutex);
        return true;
    }

    memset(&g_health_system, 0, sizeof(g_health_system));
    lv_mutex_init(&g_health_system.mutex);

    /* 默认阈值 */
    g_health_system.memory_warning_mb = 1024;  /* 1 GB */
    g_health_system.memory_critical_mb = 2048; /* 2 GB */
    g_health_system.cpu_warning_percent = 80;
    g_health_system.cpu_critical_percent = 95;

    g_health_system.initialized = true;
    lv_mutex_unlock(&g_health_init_mutex);
    return true;
}

void lv_health_shutdown(void) {
    if (!g_health_system.initialized) {
        return;
    }

    lv_mutex_destroy(&g_health_system.mutex);
    g_health_system.initialized = false;
}

void lv_health_set_memory_thresholds(double warning_mb, double critical_mb) {
    lv_mutex_lock(&g_health_system.mutex);
    g_health_system.memory_warning_mb = warning_mb;
    g_health_system.memory_critical_mb = critical_mb;
    lv_mutex_unlock(&g_health_system.mutex);
}

void lv_health_set_cpu_thresholds(double warning_percent, double critical_percent) {
    lv_mutex_lock(&g_health_system.mutex);
    g_health_system.cpu_warning_percent = warning_percent;
    g_health_system.cpu_critical_percent = critical_percent;
    lv_mutex_unlock(&g_health_system.mutex);
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
        return NULL;
    }

    report->check_count = 5;
    report->checks = (lvHealthCheck *) lv_calloc(report->check_count, sizeof(lvHealthCheck));
    if (!report->checks) {
        lv_free((void **) &report);
        return NULL;
    }

    report->timestamp_ms = lv_get_time_ns() / 1000000;
    report->overall = HEALTH_OK;

    /* 内存检查 */
    lvHealthCheck *check = &report->checks[0];
    strncpy(check->name, "Memory Usage", sizeof(check->name) - 1);
    check->threshold_warning = g_health_system.memory_warning_mb;
    check->threshold_critical = g_health_system.memory_critical_mb;

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
    check->threshold_warning = g_health_system.cpu_warning_percent;
    check->threshold_critical = g_health_system.cpu_critical_percent;
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
    check->value = g_perf_system.timer_count;
    check->status = HEALTH_OK;
    snprintf(check->message, sizeof(check->message), "%u active timers", g_perf_system.timer_count);

    /* 性能统计检查 */
    check = &report->checks[4];
    strncpy(check->name, "Performance Stats", sizeof(check->name) - 1);
    check->value = g_perf_system.stats_count;
    check->status = HEALTH_OK;
    snprintf(check->message, sizeof(check->message), "%u performance stats tracked", g_perf_system.stats_count);

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
        return NULL;
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
        return false;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        return false;
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
        return NULL;
    }

    char *json = (char *) lv_malloc(4096);
    if (!json) {
        return NULL;
    }

    snprintf(json, 4096,
             "{"
             "\"version\":\"%s\","
             "\"build_date\":\"%s\","
             "\"uptime_ms\":%lld,"
             "\"memory\":{"
             "\"total\":%llu,"
             "\"peak\":%llu,"
             "\"alloc_count\":%llu,"
             "\"free_count\":%llu"
             "},"
             "\"performance\":{"
             "\"proof_count\":%llu,"
             "\"solve_count\":%llu,"
             "\"avg_proof_time_ms\":%.2f,"
             "\"avg_solve_time_ms\":%.2f"
             "},"
             "\"errors\":{"
             "\"count\":%llu,"
             "\"warning_count\":%llu,"
             "\"last_error\":\"%s\""
             "},"
             "\"system\":{"
             "\"os\":\"%s\","
             "\"cpu_cores\":%u,"
             "\"total_memory_mb\":%u"
             "}"
             "}",
             diag->version, diag->build_date, (long long) diag->uptime_ms, (unsigned long long) diag->memory_total,
             (unsigned long long) diag->memory_peak, (unsigned long long) diag->alloc_count,
             (unsigned long long) diag->free_count, (unsigned long long) diag->proof_count,
             (unsigned long long) diag->solve_count, diag->avg_proof_time_ms, diag->avg_solve_time_ms,
             (unsigned long long) diag->error_count, (unsigned long long) diag->warning_count, diag->last_error,
             diag->os_info, diag->cpu_cores, diag->total_memory_mb);

    return json;
}

/* ============== 事件追踪实现 ============== */

static struct {
    lvEventRecord *events;
    uint32_t max_events;
    uint32_t event_count;
    lv_mutex_t mutex;
    bool initialized;
} g_event_system = {0};

static lv_mutex_t g_event_init_mutex;
static lv_once_t g_event_init_once = lv_ONCE_INIT;

static void event_init_mutex_func(void) {
    lv_mutex_init(&g_event_init_mutex);
}

bool lv_event_trace_init(uint32_t max_events) {
    lv_once(&g_event_init_once, event_init_mutex_func);
    lv_mutex_lock(&g_event_init_mutex);
    if (g_event_system.initialized) {
        lv_mutex_unlock(&g_event_init_mutex);
        return true;
    }

    /* 使用请求的大小，最小为 MAX_EVENTS */
    uint32_t actual_max = (max_events > 0) ? max_events : MAX_EVENTS;

    memset(&g_event_system, 0, sizeof(g_event_system));
    g_event_system.events = (lvEventRecord *) lv_calloc(actual_max, sizeof(lvEventRecord));
    if (!g_event_system.events) {
        lv_mutex_unlock(&g_event_init_mutex);
        return false;
    }
    g_event_system.max_events = actual_max;
    lv_mutex_init(&g_event_system.mutex);
    g_event_system.initialized = true;
    lv_mutex_unlock(&g_event_init_mutex);
    return true;
}

void lv_event_trace_shutdown(void) {
    if (!g_event_system.initialized) {
        return;
    }

    lv_free((void **) &g_event_system.events);
    lv_mutex_destroy(&g_event_system.mutex);
    g_event_system.initialized = false;
}

void lv_event_trace_record(lvEventType type, const char *name, const char *data) {
    if (!g_event_system.initialized || g_event_system.event_count >= g_event_system.max_events) {
        return;
    }

    lv_mutex_lock(&g_event_system.mutex);

    lvEventRecord *event = &g_event_system.events[g_event_system.event_count++];
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

    lv_mutex_unlock(&g_event_system.mutex);
}

int lv_event_trace_begin(lvEventType type, const char *name) {
    if (!g_event_system.initialized || g_event_system.event_count >= g_event_system.max_events) {
        return -1;
    }

    lv_mutex_lock(&g_event_system.mutex);

    int id = (int) g_event_system.event_count;
    lvEventRecord *event = &g_event_system.events[g_event_system.event_count++];
    event->type = type;
    event->timestamp_ns = lv_get_time_ns();
    event->thread_id = (int)lv_thread_id();

    if (name) {
        strncpy(event->name, name, sizeof(event->name) - 1);
    }

    lv_mutex_unlock(&g_event_system.mutex);

    return id;
}

void lv_event_trace_end(int event_id, const char *data) {
    if (!g_event_system.initialized || event_id < 0 || event_id >= (int) g_event_system.event_count) {
        return;
    }

    lv_mutex_lock(&g_event_system.mutex);

    lvEventRecord *event = &g_event_system.events[event_id];
    event->duration_ns = lv_get_time_ns() - event->timestamp_ns;

    if (data) {
        strncpy(event->data, data, sizeof(event->data) - 1);
    }

    lv_mutex_unlock(&g_event_system.mutex);
}

uint32_t lv_event_trace_get_all(lvEventRecord **out_events, uint32_t max_count) {
    if (!g_event_system.initialized || !out_events) {
        return 0;
    }

    lv_mutex_lock(&g_event_system.mutex);

    uint32_t count = g_event_system.event_count < max_count ? g_event_system.event_count : max_count;
    *out_events = (lvEventRecord *) lv_calloc((size_t) count, sizeof(lvEventRecord));
    if (*out_events) {
        memcpy(*out_events, g_event_system.events, count * sizeof(lvEventRecord));
    }

    lv_mutex_unlock(&g_event_system.mutex);

    return count;
}

void lv_event_trace_clear(void) {
    if (!g_event_system.initialized) {
        return;
    }

    lv_mutex_lock(&g_event_system.mutex);
    g_event_system.event_count = 0;
    lv_mutex_unlock(&g_event_system.mutex);
}

bool lv_event_trace_export_chrome(const char *path) {
    if (!g_event_system.initialized || !path) {
        return false;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        return false;
    }

    fprintf(fp, "[\n");

    lv_mutex_lock(&g_event_system.mutex);

    for (uint32_t i = 0; i < g_event_system.event_count; i++) {
        lvEventRecord *event = &g_event_system.events[i];

        const char *type_str = "X";
        switch (event->type) {
            case EVENT_TYPE_PROOF_START:
            case EVENT_TYPE_SOLVE_START:
                type_str = "B";
                break;
            case EVENT_TYPE_PROOF_END:
            case EVENT_TYPE_SOLVE_END:
                type_str = "E";
                break;
            default:
                type_str = "X";
                break;
        }

        fprintf(fp, "  {\"name\":\"%s\",\"cat\":\"%s\",\"ph\":\"%s\",\"ts\":%lld,\"dur\":%lld,\"pid\":1,\"tid\":%d}",
                event->name,
                event->type == EVENT_TYPE_PROOF_START || event->type == EVENT_TYPE_PROOF_END ? "proof" : "other",
                type_str, (long long) (event->timestamp_ns / 1000), (long long) (event->duration_ns / 1000),
                event->thread_id);

        if (i < g_event_system.event_count - 1) {
            fprintf(fp, ",\n");
        } else {
            fprintf(fp, "\n");
        }
    }

    lv_mutex_unlock(&g_event_system.mutex);

    fprintf(fp, "]\n");
    fclose(fp);

    return true;
}
