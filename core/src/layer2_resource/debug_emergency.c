/**
 * @file debug_emergency.c
 * @brief emergency save
 * @details Split from debug.c
 */

#include "lv/lv_file.h"
#include "lv/lv_platform.h"
#include "lv/lv_thread.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "lv/engine.h"
#include "lv/lv_json.h"

#include "context.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "type_system.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_strbuf.h"
#include "debug_internal.h"

/* ================================================================== */
/*  紧急保存实现                                                       */
/* ================================================================== */

/* 全局紧急保存处理器 (线程局部) */
static lv_THREAD_LOCAL EmergencySaveHandler g_emergency_handler = NULL;

void debug_set_emergency_handler(EmergencySaveHandler handler) {
    g_emergency_handler = handler;
}

bool debug_emergency_save(const char *filepath, const EmergencySaveConfig *config) {
    if (!filepath)
        return false;

    if (debug_stream_ctx) {
        stream_emit_error(debug_stream_ctx, "紧急保存触发", 0);
    }

    FILE *f = fopen(filepath, "w");
    if (!f)
        return false;

    /* 写入时间戳 */
    char time_buf[lv_DEBUG_TIMESTAMP_BUF_SIZE];
    get_timestamp(time_buf, sizeof(time_buf));
    fprintf(f, "=== Lv-00 Emergency Save ===\n");
    fprintf(f, "Timestamp: %s\n\n", time_buf);

    /* 写入性能计数器 */
    if (config && config->include_counters) {
        PerformanceCounters pc;
        debug_get_counters(&pc);
        fprintf(f, "[Performance Counters]\n");
        fprintf(f, "total_nodes_created=%llu\n", (unsigned long long) pc.total_nodes_created);
        fprintf(f, "current_nodes_alive=%llu\n", (unsigned long long) pc.current_nodes_alive);
        fprintf(f, "total_constraints_created=%llu\n", (unsigned long long) pc.total_constraints_created);
        fprintf(f, "current_constraints_alive=%llu\n", (unsigned long long) pc.current_constraints_alive);
        fprintf(f, "solver_call_count=%llu\n", (unsigned long long) pc.solver_call_count);
        fprintf(f, "solver_total_time_us=%llu\n", (unsigned long long) pc.solver_total_time_us);
        fprintf(f, "solver_avg_time_us=%.3f\n", pc.solver_avg_time_us);
        fprintf(f, "rewrite_total_steps=%llu\n", (unsigned long long) pc.rewrite_total_steps);
        fprintf(f, "rewrite_rule_applications=%llu\n", (unsigned long long) pc.rewrite_rule_applications);
        fprintf(f, "unify_check_count=%llu\n", (unsigned long long) pc.unify_check_count);
        fprintf(f, "unify_success_count=%llu\n", (unsigned long long) pc.unify_success_count);
        fprintf(f, "memory_usage_peak=%llu\n", (unsigned long long) pc.memory_usage_peak);
        fprintf(f, "memory_current=%llu\n", (unsigned long long) pc.memory_current);
        fprintf(f, "\n");
    }

    /* 写入日志缓冲区（最近 N 条日志，由环形日志缓冲区导出，格式与原 log_buffer 一致） */
    if (config && config->include_log_buffer) {
        int count = 0;
        lvLogEntry *entries = lv_log_ring_buffer_export(s_debug_state.log_ring_buffer, &count);
        fprintf(f, "[Recent Log Buffer (%d entries)]\n", count);
        /* 按时间顺序输出：导出结果已按插入顺序排列（最旧在前） */
        if (entries) {
            for (int i = 0; i < count; i++) {
                const char *mod = entries[i].module_name ? entries[i].module_name : "unknown";
                fprintf(f, "[%s] [%s] [%s] %s\n", entries[i].timestamp_str,
                        log_level_string(entries[i].level), mod, entries[i].message);
            }
            lv_free((void **) &entries);
        }
        fprintf(f, "\n");
    }

    /* 写入内存映射信息 */
    if (config && config->include_memory_map) {
        PerformanceCounters pc;
        debug_get_counters(&pc);
        fprintf(f, "[Memory Map]\n");
        fprintf(f, "current_usage_bytes=%llu\n", (unsigned long long) pc.memory_current);
        fprintf(f, "peak_usage_bytes=%llu\n", (unsigned long long) pc.memory_usage_peak);
        fprintf(f, "\n");
    }

    /* include_graph 标记（实际图快照需要引擎上下文，此处记录标记） */
    if (config && config->include_graph) {
        fprintf(f, "[Constraint Graph Snapshot]\n");
        fprintf(f, "Note: Full graph snapshot requires engine context.\n");
        fprintf(f, "Use debug_emergency_save from within engine context for complete dump.\n\n");
    }

    fprintf(f, "=== End of Emergency Save ===\n");
    fclose(f);
    return true;
}
