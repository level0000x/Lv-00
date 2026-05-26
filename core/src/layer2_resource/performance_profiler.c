/**
 * @file performance_profiler.c
 * @brief 性能分析器实现
 *
 * @version 3.5.0
 */

#include "performance_profiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "lv00_utils.h"

/* ================================================================
 * 内部数据结构
 * ================================================================ */

#define MAX_REGIONS 64
#define MAX_TYPES 64
#define NAME_MAX_LEN 64

/**
 * @brief 时间区域统计
 */
typedef struct {
    char name[NAME_MAX_LEN];
    uint64_t total_ns;
    uint64_t count;
    uint64_t min_ns;
    uint64_t max_ns;
    uint64_t start_ns;  /* 当前正在测量的开始时间 */
    bool active;
} TimeRegion;

/**
 * @brief 内存类型统计
 */
typedef struct {
    char name[NAME_MAX_LEN];
    uint64_t alloc_count;
    uint64_t free_count;
    uint64_t alloc_bytes;
    uint64_t free_bytes;
    uint64_t current_bytes;
    uint64_t peak_bytes;
} MemTypeStat;

/**
 * @brief 性能会话
 */
struct Lv00PerfSession {
    char name[NAME_MAX_LEN];
    
    /* 时间区域 */
    TimeRegion regions[MAX_REGIONS];
    int region_count;
    
    /* 内存统计 */
    MemTypeStat mem_stats[MAX_TYPES];
    int mem_type_count;
    
    /* 会话总时间 */
    uint64_t session_start_ns;
    uint64_t session_end_ns;
};

/* ================================================================
 * 时间获取
 * ================================================================ */

uint64_t lv00_perf_get_time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000000000LL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}

/* ================================================================
 * 会话管理
 * ================================================================ */

Lv00PerfSession *lv00_perf_session_create(const char *name) {
    Lv00PerfSession *session = (Lv00PerfSession *)lv00_malloc(sizeof(Lv00PerfSession));
    if (!session) return NULL;
    
    memset(session, 0, sizeof(Lv00PerfSession));
    
    if (name) {
        strncpy(session->name, name, NAME_MAX_LEN - 1);
        session->name[NAME_MAX_LEN - 1] = '\0';
    }
    
    session->session_start_ns = lv00_perf_get_time_ns();
    
    /* 初始化区域最小值为最大值 */
    for (int i = 0; i < MAX_REGIONS; i++) {
        session->regions[i].min_ns = UINT64_MAX;
    }
    
    return session;
}

void lv00_perf_session_destroy(Lv00PerfSession *session) {
    if (!session) return;
    session->session_end_ns = lv00_perf_get_time_ns();
    lv00_free((void **)&session);
}

void lv00_perf_session_reset(Lv00PerfSession *session) {
    if (!session) return;
    
    session->region_count = 0;
    session->mem_type_count = 0;
    memset(session->regions, 0, sizeof(session->regions));
    memset(session->mem_stats, 0, sizeof(session->mem_stats));
    
    for (int i = 0; i < MAX_REGIONS; i++) {
        session->regions[i].min_ns = UINT64_MAX;
    }
    
    session->session_start_ns = lv00_perf_get_time_ns();
    session->session_end_ns = 0;
}

/* ================================================================
 * 时间性能测量
 * ================================================================ */

static TimeRegion *find_or_create_region(Lv00PerfSession *session, const char *name) {
    /* 查找现有区域 */
    for (int i = 0; i < session->region_count; i++) {
        if (strcmp(session->regions[i].name, name) == 0) {
            return &session->regions[i];
        }
    }
    
    /* 创建新区域 */
    if (session->region_count >= MAX_REGIONS) {
        return NULL;  /* 区域已满 */
    }
    
    TimeRegion *region = &session->regions[session->region_count++];
    strncpy(region->name, name, NAME_MAX_LEN - 1);
    region->name[NAME_MAX_LEN - 1] = '\0';
    region->min_ns = UINT64_MAX;
    return region;
}

void lv00_perf_begin(Lv00PerfSession *session, const char *region_name) {
    if (!session || !region_name) return;
    
    TimeRegion *region = find_or_create_region(session, region_name);
    if (!region) return;
    
    region->start_ns = lv00_perf_get_time_ns();
    region->active = true;
}

void lv00_perf_end(Lv00PerfSession *session, const char *region_name) {
    if (!session || !region_name) return;
    
    TimeRegion *region = find_or_create_region(session, region_name);
    if (!region || !region->active) return;
    
    uint64_t end_ns = lv00_perf_get_time_ns();
    uint64_t elapsed = end_ns - region->start_ns;
    
    region->total_ns += elapsed;
    region->count++;
    region->active = false;
    
    if (elapsed < region->min_ns) region->min_ns = elapsed;
    if (elapsed > region->max_ns) region->max_ns = elapsed;
}

void lv00_perf_record_time(Lv00PerfSession *session, const char *operation_name, uint64_t elapsed_ns) {
    if (!session || !operation_name) return;
    
    TimeRegion *region = find_or_create_region(session, operation_name);
    if (!region) return;
    
    region->total_ns += elapsed_ns;
    region->count++;
    
    if (elapsed_ns < region->min_ns) region->min_ns = elapsed_ns;
    if (elapsed_ns > region->max_ns) region->max_ns = elapsed_ns;
}

/* ================================================================
 * 内存分配追踪
 * ================================================================ */

static MemTypeStat *find_or_create_mem_stat(Lv00PerfSession *session, const char *type_name) {
    /* 查找现有类型 */
    for (int i = 0; i < session->mem_type_count; i++) {
        if (strcmp(session->mem_stats[i].name, type_name) == 0) {
            return &session->mem_stats[i];
        }
    }
    
    /* 创建新类型 */
    if (session->mem_type_count >= MAX_TYPES) {
        return NULL;
    }
    
    MemTypeStat *stat = &session->mem_stats[session->mem_type_count++];
    strncpy(stat->name, type_name, NAME_MAX_LEN - 1);
    stat->name[NAME_MAX_LEN - 1] = '\0';
    return stat;
}

void lv00_perf_record_alloc(Lv00PerfSession *session, const char *type_name, size_t size) {
    if (!session || !type_name) return;
    
    MemTypeStat *stat = find_or_create_mem_stat(session, type_name);
    if (!stat) return;
    
    stat->alloc_count++;
    stat->alloc_bytes += size;
    stat->current_bytes += size;
    
    if (stat->current_bytes > stat->peak_bytes) {
        stat->peak_bytes = stat->current_bytes;
    }
}

void lv00_perf_record_free(Lv00PerfSession *session, const char *type_name, size_t size) {
    if (!session || !type_name) return;
    
    MemTypeStat *stat = find_or_create_mem_stat(session, type_name);
    if (!stat) return;
    
    stat->free_count++;
    stat->free_bytes += size;
    stat->current_bytes -= size;
}

/* ================================================================
 * 报告输出
 * ================================================================ */

void lv00_perf_report_print(const Lv00PerfSession *session, void *output) {
    if (!session) return;
    
    FILE *out = output ? (FILE *)output : stdout;
    
    fprintf(out, "\n=== Performance Report: %s ===\n", session->name);
    
    /* 时间统计 */
    if (session->region_count > 0) {
        fprintf(out, "\n--- Time Performance ---\n");
        fprintf(out, "%-30s %10s %12s %12s %12s %12s\n",
                "Region", "Count", "Total(ms)", "Mean(us)", "Min(us)", "Max(us)");
        fprintf(out, "%s\n", "--------------------------------------------------------------------------------");
        
        for (int i = 0; i < session->region_count; i++) {
            TimeRegion *r = &session->regions[i];
            if (r->count == 0) continue;
            
            double total_ms = r->total_ns / 1000000.0;
            double mean_us = (r->total_ns / r->count) / 1000.0;
            double min_us = r->min_ns / 1000.0;
            double max_us = r->max_ns / 1000.0;
            
            fprintf(out, "%-30s %10llu %12.3f %12.3f %12.3f %12.3f\n",
                    r->name, (unsigned long long)r->count, total_ms, mean_us, min_us, max_us);
        }
    }
    
    /* 内存统计 */
    if (session->mem_type_count > 0) {
        fprintf(out, "\n--- Memory Statistics ---\n");
        fprintf(out, "%-30s %10s %12s %12s %12s\n",
                "Type", "Allocs", "Alloc(MB)", "Current(MB)", "Peak(MB)");
        fprintf(out, "%s\n", "--------------------------------------------------------------------------------");
        
        for (int i = 0; i < session->mem_type_count; i++) {
            MemTypeStat *s = &session->mem_stats[i];
            if (s->alloc_count == 0) continue;
            
            double alloc_mb = s->alloc_bytes / (1024.0 * 1024.0);
            double current_mb = s->current_bytes / (1024.0 * 1024.0);
            double peak_mb = s->peak_bytes / (1024.0 * 1024.0);
            
            fprintf(out, "%-30s %10llu %12.3f %12.3f %12.3f\n",
                    s->name, (unsigned long long)s->alloc_count, alloc_mb, current_mb, peak_mb);
        }
    }
    
    fprintf(out, "\n");
}

int lv00_perf_report_to_json(const Lv00PerfSession *session, char *buffer, size_t buffer_size) {
    if (!session || !buffer || buffer_size == 0) return -1;
    
    int written = snprintf(buffer, buffer_size,
        "{\"name\":\"%s\",\"regions\":[", session->name);
    
    if (written < 0 || (size_t)written >= buffer_size) return -1;
    size_t pos = (size_t)written;
    
    /* 时间区域 */
    int region_count = 0;
    for (int i = 0; i < session->region_count; i++) {
        TimeRegion *r = &session->regions[i];
        if (r->count == 0) continue;
        
        int n = snprintf(buffer + pos, buffer_size - pos,
            "%s{\"name\":\"%s\",\"count\":%llu,\"total_ns\":%llu}",
            region_count > 0 ? "," : "",
            r->name,
            (unsigned long long)r->count,
            (unsigned long long)r->total_ns);
        
        if (n < 0 || (size_t)n >= buffer_size - pos) return -1;
        pos += (size_t)n;
        region_count++;
    }
    
    /* 内存统计 */
    written = snprintf(buffer + pos, buffer_size - pos, "],\"memory\":[");
    if (written < 0 || (size_t)written >= buffer_size - pos) return -1;
    pos += (size_t)written;
    
    int mem_count = 0;
    for (int i = 0; i < session->mem_type_count; i++) {
        MemTypeStat *s = &session->mem_stats[i];
        if (s->alloc_count == 0) continue;
        
        int n = snprintf(buffer + pos, buffer_size - pos,
            "%s{\"name\":\"%s\",\"alloc_count\":%llu,\"peak_bytes\":%llu}",
            mem_count > 0 ? "," : "",
            s->name,
            (unsigned long long)s->alloc_count,
            (unsigned long long)s->peak_bytes);
        
        if (n < 0 || (size_t)n >= buffer_size - pos) return -1;
        pos += (size_t)n;
        mem_count++;
    }
    
    if (pos + 3 >= buffer_size) return -1;
    buffer[pos++] = ']';
    buffer[pos++] = '}';
    buffer[pos] = '\0';
    
    return (int)pos;
}

/* ================================================================
 * 基准测试框架
 * ================================================================ */

static const Lv00BenchmarkConfig g_default_benchmark_config = {
    .warmup_iterations = 10,
    .measurement_iterations = 100,
    .min_duration_ms = 100
};

const Lv00BenchmarkConfig *lv00_benchmark_default_config(void) {
    return &g_default_benchmark_config;
}

int lv00_benchmark_run(const char *name, void (*func)(void),
                        const Lv00BenchmarkConfig *config,
                        Lv00BenchmarkResult *result) {
    if (!func || !result) return -1;
    
    const Lv00BenchmarkConfig *cfg = config ? config : &g_default_benchmark_config;
    
    /* 预热 */
    for (int i = 0; i < cfg->warmup_iterations; i++) {
        func();
    }
    
    /* 测量 */
    int iterations = cfg->measurement_iterations;
    uint64_t *times = (uint64_t *)malloc(iterations * sizeof(uint64_t));
    if (!times) return -1;
    
    uint64_t start_total = lv00_perf_get_time_ns();
    
    for (int i = 0; i < iterations; i++) {
        uint64_t start = lv00_perf_get_time_ns();
        func();
        uint64_t end = lv00_perf_get_time_ns();
        times[i] = end - start;
    }
    
    uint64_t total_duration_ms = (lv00_perf_get_time_ns() - start_total) / 1000000;
    
    /* 如果总时长不够，增加迭代次数 */
    while (total_duration_ms < (uint64_t)cfg->min_duration_ms && iterations < 10000) {
        int additional = iterations;
        uint64_t *new_times = (uint64_t *)realloc(times, (iterations + additional) * sizeof(uint64_t));
        if (!new_times) break;
        times = new_times;
        
        for (int i = 0; i < additional; i++) {
            uint64_t start = lv00_perf_get_time_ns();
            func();
            uint64_t end = lv00_perf_get_time_ns();
            times[iterations++] = end - start;
        }
        
        total_duration_ms = (lv00_perf_get_time_ns() - start_total) / 1000000;
    }
    
    /* 计算统计值 */
    double sum = 0.0;
    double min_ns = (double)times[0];
    double max_ns = (double)times[0];
    
    for (int i = 0; i < iterations; i++) {
        double t = (double)times[i];
        sum += t;
        if (t < min_ns) min_ns = t;
        if (t > max_ns) max_ns = t;
    }
    
    double mean = sum / iterations;
    
    /* 计算标准差 */
    double variance_sum = 0.0;
    for (int i = 0; i < iterations; i++) {
        double diff = (double)times[i] - mean;
        variance_sum += diff * diff;
    }
    double stddev = sqrt(variance_sum / iterations);
    
    result->mean_ns = mean;
    result->stddev_ns = stddev;
    result->min_ns = min_ns;
    result->max_ns = max_ns;
    result->iterations = iterations;
    
    free(times);
    return 0;
}

void lv00_benchmark_print_result(const char *name, const Lv00BenchmarkResult *result, void *output) {
    if (!result) return;
    
    FILE *out = output ? (FILE *)output : stdout;
    
    fprintf(out, "\n=== Benchmark: %s ===\n", name ? name : "unnamed");
    fprintf(out, "  Iterations: %d\n", result->iterations);
    fprintf(out, "  Mean:   %.3f us\n", result->mean_ns / 1000.0);
    fprintf(out, "  StdDev: %.3f us\n", result->stddev_ns / 1000.0);
    fprintf(out, "  Min:    %.3f us\n", result->min_ns / 1000.0);
    fprintf(out, "  Max:    %.3f us\n", result->max_ns / 1000.0);
}
