/**
 * @file debug_log_ctx.c
 * @brief context-aware logging
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

/* ============================================================
 * 带上下文的日志函数实现（v3.3.0 新增）
 *
 * lv_log_with_context() 是结构化日志的核心入口。
 * 它将日志同时写入标准日志流、文件日志和环形缓冲区。
 * ============================================================ */

/**
 * @brief 记录带完整上下文的日志
 *
 * 流程：
 * 1. 如果上下文有效，提取 context_id 用于追踪
 * 2. 调用 debug_log() 写入标准日志流（受级别过滤控制）
 * 3. 环形缓冲区由 debug_log() 统一写入（双缓冲合一后不再重复写）
 * 4. 若上下文有效，将刚写入条目的 context_id 关联到上下文
 * 5. FATAL 级别日志触发紧急保存（在 debug_log 内部处理）
 *
 * @param ctx           上下文指针（可为 NULL）
 * @param level         日志级别
 * @param module_name   模块名称
 * @param function_name 函数名称
 * @param file_name     文件名
 * @param line_number   行号
 * @param fmt           格式字符串
 * @param ...           格式参数
 */
void lv_log_with_context(struct lvContext *ctx, LogLevel level, const char *module_name, const char *function_name,
                         const char *file_name, int line_number, const char *fmt, ...) {
    /* 1. 格式化消息到临时缓冲区 */
    char message[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    /* 2. 写入标准日志流（debug_log 负责级别过滤、文件/控制台输出及环形缓冲区写入） */
    debug_log(level, module_name, "%s [%s:%d]", message, function_name, line_number);

    /* 3. 若上下文有效，将环形缓冲区中刚写入条目的 context_id 关联到上下文 */
    if (s_debug_state.log_ring_buffer && ctx && ctx->context_id > 0) {
        /* 作用域锁守卫：块结束自动解锁 */
        DEBUG_LOG_LOCK_GUARD();
        /* debug_log 仅在级别过滤通过时写入环形缓冲区，此处需同样校验，
         * 避免过滤未通过的调用错误覆盖前一条日志的 context_id */
        if (level >= g_log_level) {
            int last_idx = lv_ringbuf_count(&s_debug_state.log_ring_buffer->base) - 1;
            if (last_idx >= 0) {
                lvLogEntry *last_entry = (lvLogEntry *) lv_ringbuf_get(&s_debug_state.log_ring_buffer->base, last_idx);
                if (last_entry) {
                    last_entry->context_id = ctx->context_id;
                }
            }
        }
    }
}

/*=== Performance Counters Implementation ===*/

void debug_get_counters(PerformanceCounters *counters) {
    if (!counters)
        return;

    /* 作用域锁守卫：函数末尾自动解锁 */
    DEBUG_COUNTER_LOCK_GUARD();
    *counters = s_debug_state.counters;

    /* 计算平均求解器耗时 */
    if (s_debug_state.counters.solver_call_count > 0) {
        counters->solver_avg_time_us = (double) s_debug_state.counters.solver_total_time_us / (double) s_debug_state.counters.solver_call_count;
    }
}

void debug_reset_counters(void) {
    /* 作用域锁守卫：函数末尾自动解锁 */
    DEBUG_COUNTER_LOCK_GUARD();
    memset(&s_debug_state.counters, 0, sizeof(s_debug_state.counters));
}

void debug_counter_node_created(void) {
    lv_ATOMIC_INC64(&s_debug_state.counters.total_nodes_created);
    lv_ATOMIC_INC64(&s_debug_state.counters.current_nodes_alive);
}

void debug_counter_node_destroyed(void) {
    lv_ATOMIC_DEC64(&s_debug_state.counters.current_nodes_alive);
}

void debug_counter_constraint_created(void) {
    lv_ATOMIC_INC64(&s_debug_state.counters.total_constraints_created);
    lv_ATOMIC_INC64(&s_debug_state.counters.current_constraints_alive);
}

void debug_counter_constraint_destroyed(void) {
    lv_ATOMIC_DEC64(&s_debug_state.counters.current_constraints_alive);
}

void debug_counter_solver_called(uint64_t time_us) {
    lv_ATOMIC_INC64(&s_debug_state.counters.solver_call_count);
    lv_ATOMIC_ADD64(&s_debug_state.counters.solver_total_time_us, time_us);
}

void debug_counter_rewrite_step(void) {
    lv_ATOMIC_INC64(&s_debug_state.counters.rewrite_total_steps);
}

void debug_counter_rule_applied(void) {
    lv_ATOMIC_INC64(&s_debug_state.counters.rewrite_rule_applications);
}

void debug_counter_unify_called(bool success) {
    lv_ATOMIC_INC64(&s_debug_state.counters.unify_check_count);
    if (success) {
        lv_ATOMIC_INC64(&s_debug_state.counters.unify_success_count);
    }
}

void debug_counter_memory_update(uint64_t current_bytes) {
    lv_ATOMIC_STORE(&s_debug_state.counters.memory_current, current_bytes);
    uint64_t old_peak = s_debug_state.counters.memory_usage_peak;
    while (current_bytes > old_peak) {
        if (lv_ATOMIC_CAS_BOOL(&s_debug_state.counters.memory_usage_peak, current_bytes, &old_peak)) {
            break;
        }
    }
}

/*=== 工具函数 ===*/

char *debug_counters_report(void) {
    PerformanceCounters counters;
    debug_get_counters(&counters);

    /* 将重复的格式化字符串提取为静态常量，消除重复代码 */
    static const char *report_format =
        "=== Lv-00 Performance Counters Report ===\n"
        "\n"
        "[Node Statistics]\n"
        "  Total created:     %llu\n"
        "  Currently alive:   %llu\n"
        "\n"
        "[Constraint Statistics]\n"
        "  Total created:     %llu\n"
        "  Currently alive:   %llu\n"
        "\n"
        "[Solver Statistics]\n"
        "  Call count:        %llu\n"
        "  Total time:        %.3f ms\n"
        "  Average time:      %.3f us\n"
        "\n"
        "[Rewrite Engine Statistics]\n"
        "  Total steps:       %llu\n"
        "  Rule applications: %llu\n"
        "\n"
        "[Unify Check Statistics]\n"
        "  Total checks:      %llu\n"
        "  Success count:     %llu\n"
        "  Success rate:      %.2f%%\n"
        "\n"
        "[Memory Statistics]\n"
        "  Current usage:     %.2f MB\n"
        "  Peak usage:        %.2f MB\n"
        "\n"
        "========================================\n";

    /* 用 lvStrBuf 累积输出（自动扩容），消除"两遍探测 + 精确分配"样板 */
    lvStrBuf sb = {0};
    lv_strbuf_printf(
        &sb, report_format, (unsigned long long) counters.total_nodes_created,
        (unsigned long long) counters.current_nodes_alive, (unsigned long long) counters.total_constraints_created,
        (unsigned long long) counters.current_constraints_alive, (unsigned long long) counters.solver_call_count,
        (double) counters.solver_total_time_us / 1000.0, counters.solver_avg_time_us,
        (unsigned long long) counters.rewrite_total_steps, (unsigned long long) counters.rewrite_rule_applications,
        (unsigned long long) counters.unify_check_count, (unsigned long long) counters.unify_success_count,
        counters.unify_check_count > 0 ? (100.0 * counters.unify_success_count / counters.unify_check_count) : 0.0,
        (double) counters.memory_current / (1024.0 * 1024.0), (double) counters.memory_usage_peak / (1024.0 * 1024.0));
    return lv_strbuf_to_string(&sb);
}

int debug_get_log_path(char *buf, size_t size) {
    if (!buf || size == 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "缓冲区参数无效");

    /* 作用域锁守卫：函数末尾自动解锁 */
    DEBUG_LOG_LOCK_GUARD();
    /* 修复：使用 lv_strlcpy 替代 strncpy，自动保证零终止且更安全 */
    lv_strlcpy(buf, s_debug_state.log_file_path, size);

    return 0;
}
