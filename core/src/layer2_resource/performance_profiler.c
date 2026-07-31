/**
 * @file performance_profiler.c
 * @brief 性能分析器 —— 基准测试与会话级性能追踪
 *
 * @details 实现以下功能：
 *   - 高精度基准测试（QPC 纳秒计时、自动校准、在线统计算法）
 *   - 命名区域计时（begin/end 配对、自动累积统计）
 *   - 内存分配/释放追踪（按类型分组统计）
 *   - 文本报告输出和 JSON 导出
 *
 * 计时方案：
 *   - Windows: QueryPerformanceCounter + QueryPerformanceFrequency
 *   - POSIX:  clock_gettime(CLOCK_MONOTONIC, ...)
 */

#include "lv/lv_platform.h"

#include "lv/performance_profiler.h"
#include "lv/lv_internal.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_utils.h"

/* ================================================================
 * 报告输出
 * ================================================================ */

#define MAX_REGIONS 256              /**< 最大计时区域数 */
#define MAX_MEM_TYPES 256            /**< 最大内存类型数 */
#define WARMUP_ITERS 10              /**< 预热迭代次数 */
#define CALIB_TARGET_NS 100000000ULL /**< 校准目标时间：100ms */

/* ================================================================
 * 内部数据结构
 * ================================================================ */

/**
 * @brief 计时区域
 */
typedef struct {
    char *name;        /**< 区域名称（堆分配副本） */
    int count;         /**< 完成计时的次数 */
    uint64_t total_ns; /**< 总耗时（纳秒） */
    uint64_t min_ns;   /**< 最小耗时（纳秒） */
    uint64_t max_ns;   /**< 最大耗时（纳秒） */
    uint64_t start_ns; /**< 当前 begin 的开始时间 */
    int active;        /**< 是否处于 begin..end 之间 */
} PerfRegion;

/**
 * @brief 内存统计条目
 */
typedef struct {
    char *type_name;          /**< 类型名称（堆分配副本） */
    size_t total_alloc_bytes; /**< 累计分配字节数 */
    size_t total_free_bytes;  /**< 累计释放字节数 */
    int alloc_count;          /**< 分配次数 */
    int free_count;           /**< 释放次数 */
} PerfMemStat;

/**
 * @brief 性能会话（不透明类型实现）
 */
struct lvPerfSession {
    char *name;             /**< 会话名称 */
    uint64_t start_time_ns; /**< 会话创建/重置时间 */
    PerfRegion regions[MAX_REGIONS];
    int region_count;
    PerfMemStat mem_stats[MAX_MEM_TYPES];
    int mem_count;
};

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * @brief 在会话中查找计时区域
 * @return 索引，未找到返回 -1
 */
static int find_region(const lvPerfSession *session, const char *name) {
    if (!session || !name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "session or name is NULL");
    for (int i = 0; i < session->region_count; i++) {
        if (strcmp(session->regions[i].name, name) == 0) {
            return i;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "region not found");
}

/**
 * @brief 查找或创建计时区域
 * @return 索引，容量满返回 -1
 */
static int get_or_create_region(lvPerfSession *session, const char *name) {
    if (!session || !name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "session or name is NULL");
    int idx = find_region(session, name);
    if (idx >= 0)
        return idx;
    if (session->region_count >= MAX_REGIONS)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "region capacity exhausted");
    idx = session->region_count++;
    session->regions[idx].name = lv_strdup_safe(name);
    session->regions[idx].count = 0;
    session->regions[idx].total_ns = 0;
    session->regions[idx].min_ns = UINT64_MAX;
    session->regions[idx].max_ns = 0;
    session->regions[idx].start_ns = 0;
    session->regions[idx].active = 0;
    return idx;
}

/**
 * @brief 在会话中查找内存统计条目
 * @return 索引，未找到返回 -1
 */
static int find_mem_stat(const lvPerfSession *session, const char *type_name) {
    if (!session || !type_name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "session or type_name is NULL");
    for (int i = 0; i < session->mem_count; i++) {
        if (strcmp(session->mem_stats[i].type_name, type_name) == 0) {
            return i;
        }
    }
    lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "mem stat not found");
}

/**
 * @brief 查找或创建内存统计条目
 * @return 索引，容量满返回 -1
 */
static int get_or_create_mem_stat(lvPerfSession *session, const char *type_name) {
    if (!session || !type_name)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "session or type_name is NULL");
    int idx = find_mem_stat(session, type_name);
    if (idx >= 0)
        return idx;
    if (session->mem_count >= MAX_MEM_TYPES)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "mem stat capacity exhausted");
    idx = session->mem_count++;
    session->mem_stats[idx].type_name = lv_strdup_safe(type_name);
    session->mem_stats[idx].total_alloc_bytes = 0;
    session->mem_stats[idx].total_free_bytes = 0;
    session->mem_stats[idx].alloc_count = 0;
    session->mem_stats[idx].free_count = 0;
    return idx;
}

/* ================================================================
 * 基准测试 API
 * ================================================================ */

int lv_perf_benchmark_run(const char *name, void (*fn)(void), void *setup_fn, lvPerfBenchResult *result) {
    (void) setup_fn; /* 当前未使用 */

    if (!fn || !result)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "fn or result is NULL");

    /* ---- 预热阶段 ---- */
    for (int i = 0; i < WARMUP_ITERS; i++) {
        fn();
    }

    /* ---- 校准阶段：运行约 100ms 以确定每次调用耗时 ---- */
    uint64_t calib_start = lv_get_time_ns();
    int calib_count = 0;
    while ((lv_get_time_ns() - calib_start) < CALIB_TARGET_NS) {
        fn();
        calib_count++;
    }
    uint64_t calib_elapsed = lv_get_time_ns() - calib_start;

    /* 计算为达到 ~1 秒所需的迭代次数 */
    double ns_per_iter;
    if (calib_count > 0 && calib_elapsed > 0) {
        ns_per_iter = (double) calib_elapsed / (double) calib_count;
    } else {
        ns_per_iter = 1.0; /* 极端情况回退 */
    }

    int iterations = (int) (1000000000.0 / ns_per_iter) + 1;
    if (iterations < 100)
        iterations = 100;
    if (iterations > 100000000)
        iterations = 100000000;

    /* ---- 正式计时：使用 Welford 在线统计算法 ---- */
    double mean = 0.0;
    double M2 = 0.0;
    double min_val = 1e100;
    double max_val = 0.0;

    for (int i = 0; i < iterations; i++) {
        uint64_t t0 = lv_get_time_ns();
        fn();
        uint64_t t1 = lv_get_time_ns();
        double elapsed = (double) (t1 - t0);

        /* Welford 在线算法更新 */
        double delta = elapsed - mean;
        mean += delta / (double) (i + 1);
        double delta2 = elapsed - mean;
        M2 += delta * delta2;

        if (elapsed < min_val)
            min_val = elapsed;
        if (elapsed > max_val)
            max_val = elapsed;
    }

    /* 计算标准差 */
    double variance = (iterations > 1) ? M2 / (double) iterations : 0.0;
    double stddev = sqrt(variance);

    /* 填充结果 */
    result->name = name;
    result->iterations = iterations;
    result->mean_ns = mean;
    result->min_ns = min_val;
    result->max_ns = max_val;
    result->stddev_ns = stddev;

    return 0;
}

void lv_perf_benchmark_print_result(const char *name, const lvPerfBenchResult *result, FILE *out) {
    if (!name)
        name = result ? result->name : "unknown";
    if (!out)
        out = stdout;

    if (!result) {
        fprintf(out, "[%s] (null result)\n", name);
        return;
    }

    fprintf(out, "[%s] %d iterations, mean=%.2f ns, min=%.2f ns, max=%.2f ns, stddev=%.2f ns\n", name,
            result->iterations, result->mean_ns, result->min_ns, result->max_ns, result->stddev_ns);
}

/* ================================================================
 * 性能会话 API
 * ================================================================ */

lvPerfSession *lv_perf_session_create(const char *name) {
    lvPerfSession *session = (lvPerfSession *) lv_malloc(sizeof(lvPerfSession));
    if (!session)
        return NULL;

    memset(session, 0, sizeof(*session));

    session->name = name ? lv_strdup_safe(name) : lv_strdup_safe("unnamed");
    if (!session->name) {
        lv_free((void **) &session);
        return NULL;
    }

    session->start_time_ns = lv_get_time_ns();
    return session;
}

void lv_perf_begin(lvPerfSession *session, const char *region_name) {
    if (!session || !region_name)
        return;

    int idx = get_or_create_region(session, region_name);
    if (idx < 0)
        return;

    session->regions[idx].start_ns = lv_get_time_ns();
    session->regions[idx].active = 1;
}

void lv_perf_end(lvPerfSession *session, const char *region_name) {
    if (!session || !region_name)
        return;

    int idx = find_region(session, region_name);
    if (idx < 0)
        return;
    if (!session->regions[idx].active)
        return;

    uint64_t now = lv_get_time_ns();
    uint64_t elapsed = now - session->regions[idx].start_ns;

    session->regions[idx].total_ns += elapsed;
    session->regions[idx].count++;
    session->regions[idx].active = 0;

    if (elapsed < session->regions[idx].min_ns) {
        session->regions[idx].min_ns = elapsed;
    }
    if (elapsed > session->regions[idx].max_ns) {
        session->regions[idx].max_ns = elapsed;
    }
}

void lv_perf_session_record_alloc(lvPerfSession *session, const char *type_name, size_t bytes) {
    if (!session || !type_name)
        return;

    int idx = get_or_create_mem_stat(session, type_name);
    if (idx < 0)
        return;

    session->mem_stats[idx].total_alloc_bytes += bytes;
    session->mem_stats[idx].alloc_count++;
}

void lv_perf_session_record_free(lvPerfSession *session, const char *type_name, size_t bytes) {
    if (!session || !type_name)
        return;

    int idx = get_or_create_mem_stat(session, type_name);
    if (idx < 0)
        return;

    session->mem_stats[idx].total_free_bytes += bytes;
    session->mem_stats[idx].free_count++;
}

void lv_perf_report_print(const lvPerfSession *session, FILE *out) {
    if (!session || !out)
        return;

    fprintf(out, "=== Performance Report: %s ===\n", session->name);

    /* ---- 计时区域 ---- */
    if (session->region_count > 0) {
        fprintf(out, "Regions:\n");
        for (int i = 0; i < session->region_count; i++) {
            const PerfRegion *r = &session->regions[i];
            double avg_ns = (r->count > 0) ? (double) r->total_ns / (double) r->count : 0.0;
            fprintf(out, "  %s: count=%d, total=%.0f ns, avg=%.2f ns", r->name, r->count, (double) r->total_ns, avg_ns);
            if (r->count > 0) {
                fprintf(out, ", min=%.0f ns, max=%.0f ns", (double) r->min_ns, (double) r->max_ns);
            }
            fprintf(out, "\n");
        }
    } else {
        fprintf(out, "Regions: (none)\n");
    }

    /* ---- 内存统计 ---- */
    if (session->mem_count > 0) {
        fprintf(out, "Memory:\n");
        for (int i = 0; i < session->mem_count; i++) {
            const PerfMemStat *m = &session->mem_stats[i];
            fprintf(out, "  %s: alloc=%zu B (%d calls), free=%zu B (%d calls), net=%zd B\n", m->type_name,
                    m->total_alloc_bytes, m->alloc_count, m->total_free_bytes, m->free_count,
                    (ptrdiff_t) (m->total_alloc_bytes - m->total_free_bytes));
        }
    } else {
        fprintf(out, "Memory: (none)\n");
    }

    fprintf(out, "\n");
}

int lv_perf_report_to_json(const lvPerfSession *session, char *buffer, size_t buffer_size) {
    if (!session || !buffer || buffer_size == 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "session, buffer or buffer_size is invalid");

    int written = 0;
    const char *fmt;
    int ret;

    /* ---- 开场 ---- */
    fmt = "{\"name\":\"%s\"";
    ret = snprintf(buffer + written, buffer_size - (size_t) written, fmt, session->name);
    if (ret < 0 || (size_t) ret >= buffer_size - (size_t) written)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to write JSON header");
    written += ret;

    /* ---- 区域数组 ---- */
    fmt = ",\"regions\":[";
    ret = snprintf(buffer + written, buffer_size - (size_t) written, "%s", fmt);
    if (ret < 0 || (size_t) ret >= buffer_size - (size_t) written)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to write JSON regions array header");
    written += ret;

    for (int i = 0; i < session->region_count; i++) {
        const PerfRegion *r = &session->regions[i];
        fmt = (i == 0) ? "{\"name\":\"%s\",\"count\":%d,\"total_ns\":%llu}"
                       : ",{\"name\":\"%s\",\"count\":%d,\"total_ns\":%llu}";
        ret = snprintf(buffer + written, buffer_size - (size_t) written, fmt, r->name, r->count,
                       (unsigned long long) r->total_ns);
        if (ret < 0 || (size_t) ret >= buffer_size - (size_t) written)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to write JSON region entry");
        written += ret;
    }

    fmt = "]";
    ret = snprintf(buffer + written, buffer_size - (size_t) written, "%s", fmt);
    if (ret < 0 || (size_t) ret >= buffer_size - (size_t) written)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to write JSON regions array trailer");
    written += ret;

    /* ---- 内存数组 ---- */
    fmt = ",\"memory\":[";
    ret = snprintf(buffer + written, buffer_size - (size_t) written, "%s", fmt);
    if (ret < 0 || (size_t) ret >= buffer_size - (size_t) written)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to write JSON memory array header");
    written += ret;

    for (int i = 0; i < session->mem_count; i++) {
        const PerfMemStat *m = &session->mem_stats[i];
        fmt = (i == 0) ? "{\"type\":\"%s\",\"alloc\":%zu,\"free\":%zu,\"net\":%zd}"
                       : ",{\"type\":\"%s\",\"alloc\":%zu,\"free\":%zu,\"net\":%zd}";
        ret = snprintf(buffer + written, buffer_size - (size_t) written, fmt, m->type_name, m->total_alloc_bytes,
                       m->total_free_bytes, (ptrdiff_t) (m->total_alloc_bytes - m->total_free_bytes));
        if (ret < 0 || (size_t) ret >= buffer_size - (size_t) written)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to write JSON memory entry");
        written += ret;
    }

    fmt = "]";
    ret = snprintf(buffer + written, buffer_size - (size_t) written, "%s", fmt);
    if (ret < 0 || (size_t) ret >= buffer_size - (size_t) written)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to write JSON memory array trailer");
    written += ret;

    /* ---- 收尾 ---- */
    fmt = "}";
    ret = snprintf(buffer + written, buffer_size - (size_t) written, "%s", fmt);
    if (ret < 0 || (size_t) ret >= buffer_size - (size_t) written)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "failed to write JSON footer");
    written += ret;

    return written; /* 不含末尾 '\0' */
}

void lv_perf_session_reset(lvPerfSession *session) {
    if (!session)
        return;

    /* 释放区域名称 */
    for (int i = 0; i < session->region_count; i++) {
        lv_free((void **) &session->regions[i].name);
    }
    session->region_count = 0;

    /* 释放内存类型名称 */
    for (int i = 0; i < session->mem_count; i++) {
        lv_free((void **) &session->mem_stats[i].type_name);
    }
    session->mem_count = 0;

    /* 重置时钟 */
    session->start_time_ns = lv_get_time_ns();
}

void lv_perf_session_destroy(lvPerfSession *session) {
    if (!session)
        return;

    /* 释放区域名称 */
    for (int i = 0; i < session->region_count; i++) {
        lv_free((void **) &session->regions[i].name);
    }

    /* 释放内存类型名称 */
    for (int i = 0; i < session->mem_count; i++) {
        lv_free((void **) &session->mem_stats[i].type_name);
    }

    lv_free((void **) &session->name);
    lv_free((void **) &session);
}
