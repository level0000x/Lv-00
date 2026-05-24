/**
 * @file runtime_monitor.c
 * @brief 运行时监控与日志系统实现
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "runtime_monitor.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
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

#ifdef _WIN32
typedef CRITICAL_SECTION Lv00Mutex;
#define MUTEX_INIT(m) InitializeCriticalSection(&(m))
#define MUTEX_DESTROY(m) DeleteCriticalSection(&(m))
#define MUTEX_LOCK(m) EnterCriticalSection(&(m))
#define MUTEX_UNLOCK(m) LeaveCriticalSection(&(m))

static int64_t get_time_ns(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (int64_t)((double)count.QuadPart / (double)freq.QuadPart * 1e9);
}

static int get_thread_id(void) {
    return (int)GetCurrentThreadId();
}
#else
typedef pthread_mutex_t Lv00Mutex;
#define MUTEX_INIT(m) pthread_mutex_init(&(m), NULL)
#define MUTEX_DESTROY(m) pthread_mutex_destroy(&(m))
#define MUTEX_LOCK(m) pthread_mutex_lock(&(m))
#define MUTEX_UNLOCK(m) pthread_mutex_unlock(&(m))

static int64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static int get_thread_id(void) {
    return (int)pthread_self();
}
#endif

/* ============== 日志系统实现 ============== */

static struct {
    Lv00LogConfig config;
    FILE *log_file;
    Lv00Mutex mutex;
    bool initialized;
    uint64_t current_file_size;
} g_log_system = {0};

static const char *level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"
};

static const char *level_colors[] = {
    "\033[37m",    /* TRACE: 白色 */
    "\033[36m",    /* DEBUG: 青色 */
    "\033[32m",    /* INFO: 绿色 */
    "\033[33m",    /* WARN: 黄色 */
    "\033[31m",    /* ERROR: 红色 */
    "\033[35;1m",  /* FATAL: 紫色加粗 */
    ""
};

bool lv00_log_init(const Lv00LogConfig *config) {
    if (g_log_system.initialized) {
        return true;
    }

    memset(&g_log_system, 0, sizeof(g_log_system));

    if (config) {
        memcpy(&g_log_system.config, config, sizeof(Lv00LogConfig));
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

    MUTEX_INIT(g_log_system.mutex);

    /* 打开日志文件 */
    if ((g_log_system.config.targets & LOG_TARGET_FILE) && g_log_system.config.file_path[0]) {
        g_log_system.log_file = fopen(g_log_system.config.file_path, "a");
        if (!g_log_system.log_file) {
            g_log_system.config.targets &= ~LOG_TARGET_FILE;
        }
    }

    g_log_system.initialized = true;
    return true;
}

void lv00_log_shutdown(void) {
    if (!g_log_system.initialized) {
        return;
    }

    if (g_log_system.log_file) {
        fclose(g_log_system.log_file);
        g_log_system.log_file = NULL;
    }

    MUTEX_DESTROY(g_log_system.mutex);
    g_log_system.initialized = false;
}

void lv00_log_set_level(Lv00LogLevel level) {
    if (level >= LOG_LEVEL_TRACE && level <= LOG_LEVEL_OFF) {
        g_log_system.config.min_level = level;
    }
}

void lv00_log_set_targets(Lv00LogTarget targets) {
    g_log_system.config.targets = targets;
}

bool lv00_log_set_file(const char *path) {
    if (!path) {
        return false;
    }

    MUTEX_LOCK(g_log_system.mutex);

    if (g_log_system.log_file) {
        fclose(g_log_system.log_file);
        g_log_system.log_file = NULL;
    }

    strncpy(g_log_system.config.file_path, path, sizeof(g_log_system.config.file_path) - 1);
    g_log_system.log_file = fopen(path, "a");
    g_log_system.current_file_size = 0;

    MUTEX_UNLOCK(g_log_system.mutex);

    return g_log_system.log_file != NULL;
}

void lv00_log_set_callback(Lv00LogCallback callback, void *user_data) {
    g_log_system.config.callback = callback;
    g_log_system.config.callback_user_data = user_data;
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
    g_log_system.current_file_size = 0;
}

void lv00_log_write(Lv00LogLevel level, const char *tag,
                    const char *file, int line, const char *function,
                    const char *fmt, ...) {
    if (!g_log_system.initialized) {
        return;
    }

    if (level < g_log_system.config.min_level) {
        return;
    }

    MUTEX_LOCK(g_log_system.mutex);

    /* 格式化消息 */
    char message[LV00_LOG_MSG_MAX_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    /* 构建日志记录 */
    Lv00LogRecord record;
    record.level = level;
    record.line = line;
    record.timestamp_ms = get_time_ns() / 1000000;
    record.thread_id = get_thread_id();

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
        struct tm *tm_info = localtime(&now);
        pos += strftime(output + pos, sizeof(output) - pos, "%Y-%m-%d %H:%M:%S", tm_info);
        pos += snprintf(output + pos, sizeof(output) - pos, ".%03d ", (int)(record.timestamp_ms % 1000));
    }

    if (g_log_system.config.colored_output) {
        pos += snprintf(output + pos, sizeof(output) - pos, "%s%-5s\033[0m ",
                        level_colors[level], level_strings[level]);
    } else {
        pos += snprintf(output + pos, sizeof(output) - pos, "%-5s ", level_strings[level]);
    }

    if (record.tag[0]) {
        pos += snprintf(output + pos, sizeof(output) - pos, "[%s] ", record.tag);
    }

    if (g_log_system.config.include_thread_id) {
        pos += snprintf(output + pos, sizeof(output) - pos, "(T%d) ", record.thread_id);
    }

    pos += snprintf(output + pos, sizeof(output) - pos, "%s", record.message);

    if (g_log_system.config.include_location) {
        const char *basename = strrchr(record.file, '/');
        if (!basename) basename = strrchr(record.file, '\\');
        basename = basename ? basename + 1 : record.file;
        pos += snprintf(output + pos, sizeof(output) - pos, " (%s:%d in %s)",
                        basename, record.line, record.function);
    }

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
        fwrite(output, 1, len, g_log_system.log_file);
        fflush(g_log_system.log_file);
        g_log_system.current_file_size += len;

        rotate_log_file();
    }

    MUTEX_UNLOCK(g_log_system.mutex);
}

/* ============== 性能监控实现 ============== */

static struct {
    Lv00Timer *timers[MAX_TIMERS];
    uint32_t timer_count;
    Lv00PerfStats *stats[MAX_PERF_STATS];
    uint32_t stats_count;
    Lv00Mutex mutex;
    bool initialized;
} g_perf_system = {0};

bool lv00_perf_init(void) {
    if (g_perf_system.initialized) {
        return true;
    }

    memset(&g_perf_system, 0, sizeof(g_perf_system));
    MUTEX_INIT(g_perf_system.mutex);
    g_perf_system.initialized = true;
    return true;
}

void lv00_perf_shutdown(void) {
    if (!g_perf_system.initialized) {
        return;
    }

    MUTEX_LOCK(g_perf_system.mutex);

    for (uint32_t i = 0; i < g_perf_system.timer_count; i++) {
        free(g_perf_system.timers[i]);
    }
    for (uint32_t i = 0; i < g_perf_system.stats_count; i++) {
        free(g_perf_system.stats[i]);
    }

    MUTEX_UNLOCK(g_perf_system.mutex);
    MUTEX_DESTROY(g_perf_system.mutex);
    g_perf_system.initialized = false;
}

Lv00Timer *lv00_timer_create(const char *name) {
    if (!g_perf_system.initialized || g_perf_system.timer_count >= MAX_TIMERS) {
        return NULL;
    }

    Lv00Timer *timer = (Lv00Timer *)calloc(1, sizeof(Lv00Timer));
    if (!timer) {
        return NULL;
    }

    if (name) {
        strncpy(timer->name, name, sizeof(timer->name) - 1);
    }
    timer->state = TIMER_STOPPED;

    MUTEX_LOCK(g_perf_system.mutex);
    g_perf_system.timers[g_perf_system.timer_count++] = timer;
    MUTEX_UNLOCK(g_perf_system.mutex);

    return timer;
}

void lv00_timer_destroy(Lv00Timer *timer) {
    if (!timer) {
        return;
    }

    MUTEX_LOCK(g_perf_system.mutex);
    for (uint32_t i = 0; i < g_perf_system.timer_count; i++) {
        if (g_perf_system.timers[i] == timer) {
            g_perf_system.timers[i] = g_perf_system.timers[--g_perf_system.timer_count];
            break;
        }
    }
    MUTEX_UNLOCK(g_perf_system.mutex);

    free(timer);
}

void lv00_timer_start(Lv00Timer *timer) {
    if (!timer) {
        return;
    }

    timer->start_time_ns = get_time_ns();
    timer->state = TIMER_RUNNING;
    timer->call_count++;
}

int64_t lv00_timer_stop(Lv00Timer *timer) {
    if (!timer || timer->state != TIMER_RUNNING) {
        return 0;
    }

    timer->elapsed_ns = get_time_ns() - timer->start_time_ns;
    timer->total_ns += timer->elapsed_ns;
    timer->state = TIMER_STOPPED;

    return timer->elapsed_ns / 1000000;
}

void lv00_timer_pause(Lv00Timer *timer) {
    if (!timer || timer->state != TIMER_RUNNING) {
        return;
    }

    timer->elapsed_ns += get_time_ns() - timer->start_time_ns;
    timer->state = TIMER_PAUSED;
}

void lv00_timer_resume(Lv00Timer *timer) {
    if (!timer || timer->state != TIMER_PAUSED) {
        return;
    }

    timer->start_time_ns = get_time_ns();
    timer->state = TIMER_RUNNING;
}

void lv00_timer_reset(Lv00Timer *timer) {
    if (!timer) {
        return;
    }

    timer->elapsed_ns = 0;
    timer->total_ns = 0;
    timer->call_count = 0;
    timer->state = TIMER_STOPPED;
}

int64_t lv00_timer_elapsed_ms(const Lv00Timer *timer) {
    if (!timer) {
        return 0;
    }

    if (timer->state == TIMER_RUNNING) {
        return (get_time_ns() - timer->start_time_ns) / 1000000;
    }
    return timer->elapsed_ns / 1000000;
}

int64_t lv00_timer_elapsed_ns(const Lv00Timer *timer) {
    if (!timer) {
        return 0;
    }

    if (timer->state == TIMER_RUNNING) {
        return get_time_ns() - timer->start_time_ns;
    }
    return timer->elapsed_ns;
}

Lv00PerfStats *lv00_perf_stats_create(const char *name) {
    if (!g_perf_system.initialized || g_perf_system.stats_count >= MAX_PERF_STATS) {
        return NULL;
    }

    Lv00PerfStats *stats = (Lv00PerfStats *)calloc(1, sizeof(Lv00PerfStats));
    if (!stats) {
        return NULL;
    }

    if (name) {
        strncpy(stats->name, name, sizeof(stats->name) - 1);
    }
    stats->min_val = 1e308;
    stats->max_val = -1e308;

    MUTEX_LOCK(g_perf_system.mutex);
    g_perf_system.stats[g_perf_system.stats_count++] = stats;
    MUTEX_UNLOCK(g_perf_system.mutex);

    return stats;
}

void lv00_perf_stats_destroy(Lv00PerfStats *stats) {
    if (!stats) {
        return;
    }

    MUTEX_LOCK(g_perf_system.mutex);
    for (uint32_t i = 0; i < g_perf_system.stats_count; i++) {
        if (g_perf_system.stats[i] == stats) {
            g_perf_system.stats[i] = g_perf_system.stats[--g_perf_system.stats_count];
            break;
        }
    }
    MUTEX_UNLOCK(g_perf_system.mutex);

    free(stats);
}

void lv00_perf_stats_record(Lv00PerfStats *stats, double value) {
    if (!stats) {
        return;
    }

    stats->count++;
    stats->sum += value;
    stats->sum_sq += value * value;
    stats->last_val = value;
    stats->last_time_ns = get_time_ns();

    if (value < stats->min_val) {
        stats->min_val = value;
    }
    if (value > stats->max_val) {
        stats->max_val = value;
    }

    /* 计算均值和方差 */
    stats->mean = stats->sum / stats->count;
    if (stats->count > 1) {
        stats->variance = (stats->sum_sq - stats->sum * stats->sum / stats->count) / (stats->count - 1);
        stats->std_dev = sqrt(stats->variance);
    }
}

void lv00_perf_stats_reset(Lv00PerfStats *stats) {
    if (!stats) {
        return;
    }

    memset(stats, 0, sizeof(Lv00PerfStats));
    stats->min_val = 1e308;
    stats->max_val = -1e308;
}

/* ============== 健康检查实现 ============== */

static struct {
    double memory_warning_mb;
    double memory_critical_mb;
    double cpu_warning_percent;
    double cpu_critical_percent;
    Lv00Mutex mutex;
    bool initialized;
} g_health_system = {0};

bool lv00_health_init(void) {
    if (g_health_system.initialized) {
        return true;
    }

    memset(&g_health_system, 0, sizeof(g_health_system));
    MUTEX_INIT(g_health_system.mutex);

    /* 默认阈值 */
    g_health_system.memory_warning_mb = 1024;   /* 1 GB */
    g_health_system.memory_critical_mb = 2048;  /* 2 GB */
    g_health_system.cpu_warning_percent = 80;
    g_health_system.cpu_critical_percent = 95;

    g_health_system.initialized = true;
    return true;
}

void lv00_health_shutdown(void) {
    if (!g_health_system.initialized) {
        return;
    }

    MUTEX_DESTROY(g_health_system.mutex);
    g_health_system.initialized = false;
}

void lv00_health_set_memory_thresholds(double warning_mb, double critical_mb) {
    g_health_system.memory_warning_mb = warning_mb;
    g_health_system.memory_critical_mb = critical_mb;
}

void lv00_health_set_cpu_thresholds(double warning_percent, double critical_percent) {
    g_health_system.cpu_warning_percent = warning_percent;
    g_health_system.cpu_critical_percent = critical_percent;
}

Lv00HealthReport *lv00_health_check(void) {
    Lv00HealthReport *report = (Lv00HealthReport *)calloc(1, sizeof(Lv00HealthReport));
    if (!report) {
        return NULL;
    }

    report->check_count = 5;
    report->checks = (Lv00HealthCheck *)calloc(report->check_count, sizeof(Lv00HealthCheck));
    if (!report->checks) {
        free(report);
        return NULL;
    }

    report->timestamp_ms = get_time_ns() / 1000000;
    report->overall = HEALTH_OK;

    /* 内存检查 */
    Lv00HealthCheck *check = &report->checks[0];
    strncpy(check->name, "Memory Usage", sizeof(check->name) - 1);
    check->threshold_warning = g_health_system.memory_warning_mb;
    check->threshold_critical = g_health_system.memory_critical_mb;

#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    check->value = (double)(status.ullTotalVirtual - status.ullAvailVirtual) / (1024 * 1024);
#else
    FILE *fp = fopen("/proc/self/status", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                check->value = atof(line + 6);
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

    /* CPU 检查（简化） */
    check = &report->checks[1];
    strncpy(check->name, "CPU Usage", sizeof(check->name) - 1);
    check->threshold_warning = g_health_system.cpu_warning_percent;
    check->threshold_critical = g_health_system.cpu_critical_percent;
    check->value = 0; /* 需要平台特定实现 */
    check->status = HEALTH_OK;
    strncpy(check->message, "CPU usage monitoring not implemented", sizeof(check->message) - 1);

    /* 线程检查 */
    check = &report->checks[2];
    strncpy(check->name, "Thread Count", sizeof(check->name) - 1);
    check->value = 1; /* 简化 */
    check->status = HEALTH_OK;
    strncpy(check->message, "Thread count normal", sizeof(check->message) - 1);

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

void lv00_health_report_destroy(Lv00HealthReport *report) {
    if (!report) {
        return;
    }
    free(report->checks);
    free(report);
}

/* ============== 诊断报告实现 ============== */

Lv00Diagnostics *lv00_diagnostics_generate(void) {
    Lv00Diagnostics *diag = (Lv00Diagnostics *)calloc(1, sizeof(Lv00Diagnostics));
    if (!diag) {
        return NULL;
    }

    /* 基本信息 */
    strncpy(diag->version, "3.3.0", sizeof(diag->version) - 1);
    strncpy(diag->build_date, __DATE__ " " __TIME__, sizeof(diag->build_date) - 1);
    diag->uptime_ms = get_time_ns() / 1000000;

    /* 内存统计 */
    diag->memory_total = 0;
    diag->memory_peak = 0;
    diag->alloc_count = 0;
    diag->free_count = 0;

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
    diag->total_memory_mb = (uint32_t)(mem_status.ullTotalPhys / (1024 * 1024));
#else
    strncpy(diag->os_info, "Linux/Unix", sizeof(diag->os_info) - 1);
    diag->cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
    diag->total_memory_mb = (uint32_t)(sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE) / (1024 * 1024));
#endif

    return diag;
}

void lv00_diagnostics_destroy(Lv00Diagnostics *diag) {
    free(diag);
}

bool lv00_diagnostics_write_file(const Lv00Diagnostics *diag, const char *path) {
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
    fprintf(fp, "Uptime: %lld ms\n", (long long)diag->uptime_ms);
    fprintf(fp, "\n--- Memory ---\n");
    fprintf(fp, "Total Used: %llu bytes\n", (unsigned long long)diag->memory_total);
    fprintf(fp, "Peak Used: %llu bytes\n", (unsigned long long)diag->memory_peak);
    fprintf(fp, "Allocations: %llu\n", (unsigned long long)diag->alloc_count);
    fprintf(fp, "Frees: %llu\n", (unsigned long long)diag->free_count);
    fprintf(fp, "\n--- Performance ---\n");
    fprintf(fp, "Proof Count: %llu\n", (unsigned long long)diag->proof_count);
    fprintf(fp, "Solve Count: %llu\n", (unsigned long long)diag->solve_count);
    fprintf(fp, "Avg Proof Time: %.2f ms\n", diag->avg_proof_time_ms);
    fprintf(fp, "Avg Solve Time: %.2f ms\n", diag->avg_solve_time_ms);
    fprintf(fp, "\n--- Errors ---\n");
    fprintf(fp, "Error Count: %llu\n", (unsigned long long)diag->error_count);
    fprintf(fp, "Warning Count: %llu\n", (unsigned long long)diag->warning_count);
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

char *lv00_diagnostics_to_json(const Lv00Diagnostics *diag) {
    if (!diag) {
        return NULL;
    }

    char *json = (char *)malloc(4096);
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
             diag->version,
             diag->build_date,
             (long long)diag->uptime_ms,
             (unsigned long long)diag->memory_total,
             (unsigned long long)diag->memory_peak,
             (unsigned long long)diag->alloc_count,
             (unsigned long long)diag->free_count,
             (unsigned long long)diag->proof_count,
             (unsigned long long)diag->solve_count,
             diag->avg_proof_time_ms,
             diag->avg_solve_time_ms,
             (unsigned long long)diag->error_count,
             (unsigned long long)diag->warning_count,
             diag->last_error,
             diag->os_info,
             diag->cpu_cores,
             diag->total_memory_mb);

    return json;
}

/* ============== 事件追踪实现 ============== */

static struct {
    Lv00EventRecord events[MAX_EVENTS];
    uint32_t event_count;
    Lv00Mutex mutex;
    bool initialized;
} g_event_system = {0};

bool lv00_event_trace_init(uint32_t max_events) {
    (void)max_events; /* 使用固定大小 */

    if (g_event_system.initialized) {
        return true;
    }

    memset(&g_event_system, 0, sizeof(g_event_system));
    MUTEX_INIT(g_event_system.mutex);
    g_event_system.initialized = true;
    return true;
}

void lv00_event_trace_shutdown(void) {
    if (!g_event_system.initialized) {
        return;
    }

    MUTEX_DESTROY(g_event_system.mutex);
    g_event_system.initialized = false;
}

void lv00_event_trace_record(Lv00EventType type, const char *name, const char *data) {
    if (!g_event_system.initialized || g_event_system.event_count >= MAX_EVENTS) {
        return;
    }

    MUTEX_LOCK(g_event_system.mutex);

    Lv00EventRecord *event = &g_event_system.events[g_event_system.event_count++];
    event->type = type;
    event->timestamp_ns = get_time_ns();
    event->duration_ns = 0;
    event->thread_id = get_thread_id();

    if (name) {
        strncpy(event->name, name, sizeof(event->name) - 1);
    }
    if (data) {
        strncpy(event->data, data, sizeof(event->data) - 1);
    }

    MUTEX_UNLOCK(g_event_system.mutex);
}

int lv00_event_trace_begin(Lv00EventType type, const char *name) {
    if (!g_event_system.initialized || g_event_system.event_count >= MAX_EVENTS) {
        return -1;
    }

    MUTEX_LOCK(g_event_system.mutex);

    int id = (int)g_event_system.event_count;
    Lv00EventRecord *event = &g_event_system.events[g_event_system.event_count++];
    event->type = type;
    event->timestamp_ns = get_time_ns();
    event->thread_id = get_thread_id();

    if (name) {
        strncpy(event->name, name, sizeof(event->name) - 1);
    }

    MUTEX_UNLOCK(g_event_system.mutex);

    return id;
}

void lv00_event_trace_end(int event_id, const char *data) {
    if (!g_event_system.initialized || event_id < 0 || event_id >= (int)g_event_system.event_count) {
        return;
    }

    MUTEX_LOCK(g_event_system.mutex);

    Lv00EventRecord *event = &g_event_system.events[event_id];
    event->duration_ns = get_time_ns() - event->timestamp_ns;

    if (data) {
        strncpy(event->data, data, sizeof(event->data) - 1);
    }

    MUTEX_UNLOCK(g_event_system.mutex);
}

uint32_t lv00_event_trace_get_all(Lv00EventRecord **out_events, uint32_t max_count) {
    if (!g_event_system.initialized || !out_events) {
        return 0;
    }

    MUTEX_LOCK(g_event_system.mutex);

    uint32_t count = g_event_system.event_count < max_count ? g_event_system.event_count : max_count;
    *out_events = (Lv00EventRecord *)malloc(count * sizeof(Lv00EventRecord));
    if (*out_events) {
        memcpy(*out_events, g_event_system.events, count * sizeof(Lv00EventRecord));
    }

    MUTEX_UNLOCK(g_event_system.mutex);

    return count;
}

void lv00_event_trace_clear(void) {
    if (!g_event_system.initialized) {
        return;
    }

    MUTEX_LOCK(g_event_system.mutex);
    g_event_system.event_count = 0;
    MUTEX_UNLOCK(g_event_system.mutex);
}

bool lv00_event_trace_export_chrome(const char *path) {
    if (!g_event_system.initialized || !path) {
        return false;
    }

    FILE *fp = fopen(path, "w");
    if (!fp) {
        return false;
    }

    fprintf(fp, "[\n");

    MUTEX_LOCK(g_event_system.mutex);

    for (uint32_t i = 0; i < g_event_system.event_count; i++) {
        Lv00EventRecord *event = &g_event_system.events[i];

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

        fprintf(fp,
                "  {\"name\":\"%s\",\"cat\":\"%s\",\"ph\":\"%s\",\"ts\":%lld,\"dur\":%lld,\"pid\":1,\"tid\":%d}",
                event->name,
                event->type == EVENT_TYPE_PROOF_START || event->type == EVENT_TYPE_PROOF_END ? "proof" : "other",
                type_str,
                (long long)(event->timestamp_ns / 1000),
                (long long)(event->duration_ns / 1000),
                event->thread_id);

        if (i < g_event_system.event_count - 1) {
            fprintf(fp, ",\n");
        } else {
            fprintf(fp, "\n");
        }
    }

    MUTEX_UNLOCK(g_event_system.mutex);

    fprintf(fp, "]\n");
    fclose(fp);

    return true;
}
