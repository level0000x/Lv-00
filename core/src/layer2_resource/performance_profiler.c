/**
 * @file performance_profiler.c
 * @brief 性能分析器 - 基于段名的轻量级 profiling
 *
 * @details 支持嵌套/非嵌套的代码段计时，自动统计调用次数和总耗时，
 *          最终输出可读的性能报告。
 */
#include "lv00/performance_profiler.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 最大同时追踪的段数 */
#define MAX_SECTIONS 128

/* 单个性能段 */
typedef struct {
    char name[128];         /* 段名称 */
    uint64_t start_us;      /* 开始时间（微秒） */
    uint64_t total_us;      /* 累计耗时 */
    uint64_t call_count;    /* 调用次数 */
    int active;             /* 是否正在计时 */
} ProfilerSection;

/* 全局性能分析器状态 */
static ProfilerSection s_sections[MAX_SECTIONS];
static int s_section_count = 0;

/* 查找已有段 */
static int profiler_find_section(const char *name) {
    for (int i = 0; i < s_section_count; i++) {
        if (strcmp(s_sections[i].name, name) == 0) return i;
    }
    return -1;
}

/* 查找或创建段 */
static int profiler_get_or_create(const char *name) {
    int idx = profiler_find_section(name);
    if (idx >= 0) return idx;
    if (s_section_count >= MAX_SECTIONS) return -1;
    idx = s_section_count++;
    memset(&s_sections[idx], 0, sizeof(ProfilerSection));
    lv00_strlcpy(s_sections[idx].name, name, sizeof(s_sections[idx].name));
    return idx;
}

/**
 * @brief 开始对指定段计时
 * @param name 段名称（唯一标识）
 */
void lv00_profiler_start(const char *name) {
    if (!name) return;
    int idx = profiler_get_or_create(name);
    if (idx < 0) return;
    s_sections[idx].start_us = lv00_get_time_us();
    s_sections[idx].active = 1;
}

/**
 * @brief 结束对指定段计时
 * @param name 段名称（需与 start 配对）
 */
void lv00_profiler_end(const char *name) {
    if (!name) return;
    int idx = profiler_find_section(name);
    if (idx < 0) return;
    if (!s_sections[idx].active) return;

    uint64_t elapsed = lv00_get_time_us() - s_sections[idx].start_us;
    s_sections[idx].total_us += elapsed;
    s_sections[idx].call_count++;
    s_sections[idx].active = 0;
}

/**
 * @brief 打印性能分析报告
 *
 * 输出格式：
 *   [性能报告]
 *   段名            调用次数    总耗时(ms)   平均耗时(us)
 *   ---------------------------------------------------
 *   render          1234        56.78       46.02
 *   compute         5678        12.34       2.17
 */
void lv00_profiler_report(void) {
    if (s_section_count == 0) {
        fprintf(stderr, "[性能报告] 无数据\n");
        return;
    }
    fprintf(stderr, "\n[性能报告]\n");
    fprintf(stderr, "%-24s %10s %12s %12s\n", "段名", "调用次数", "总耗时(ms)", "平均耗时(us)");
    fprintf(stderr, "------------------------------------------------------------\n");
    for (int i = 0; i < s_section_count; i++) {
        ProfilerSection *s = &s_sections[i];
        double total_ms = (double)s->total_us / 1000.0;
        double avg_us = (s->call_count > 0) ?
            (double)s->total_us / (double)s->call_count : 0.0;
        fprintf(stderr, "%-24s %10llu %12.3f %12.2f\n",
                s->name,
                (unsigned long long)s->call_count,
                total_ms, avg_us);
    }
    fprintf(stderr, "\n");
}
