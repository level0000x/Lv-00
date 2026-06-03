/**
 * @file debug_trace.c
 * @brief 调试追踪系统实现 —— 证明状态快照、推导过程记录与性能分析计时器
 *
 * @details 实现调试追踪系统的三个核心模块：
 *   1. Lv00DebugSnapshot —— 从 ProofNavigator 捕获关键状态
 *   2. Lv00DerivationLog —— 结构化推导过程记录
 *   3. Lv00PerfTimer —— 嵌套区间计时器
 *
 * @version 3.6.0
 * @author Lv-00 Project
 *
 * @par 层级归属
 * 本模块属于 Layer 2 (Resource Management)。
 */

#include "debug_trace.h"

#include <stdio.h>
#include <string.h>

#include "lv00_utils.h"
#include "proof.h"

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 获取当前时间戳（微秒）
 *
 * 使用 lv00_utils.h 中提供的 lv00_get_time_us()。
 */
static uint64_t debug_trace_now_us(void) {
    return lv00_get_time_us();
}

/* ============================================================
 * 快照 API 实现
 * ============================================================ */

/**
 * @brief 从证明导航器创建快照
 */
LV00_PUBLIC_API Lv00DebugSnapshot lv00_debug_snapshot_create(const ProofNavigator *nav,
                                                              const char *label) {
    Lv00DebugSnapshot snap;
    memset(&snap, 0, sizeof(snap));

    if (nav) {
        snap.step_index       = nav->current_step;
        snap.step_count       = nav->step_count;
        snap.is_complete      = nav->is_complete;
        snap.final_color      = (int)nav->final_color;
        snap.proof_state      = (int)nav->proof_state;
        snap.breakpoint_count = nav->breakpoint_count;
    }

    /* 填充标签 */
    if (label) {
        lv00_strlcpy(snap.label, label, sizeof(snap.label));
    } else {
        snap.label[0] = '\0';
    }

    snap.timestamp_us = debug_trace_now_us();

    return snap;
}

/**
 * @brief 获取快照标签
 */
LV00_PUBLIC_API const char *lv00_debug_snapshot_get_label(const Lv00DebugSnapshot *snapshot) {
    LV00_RETURN_IF_NULL(snapshot, "");
    return snapshot->label;
}

/**
 * @brief 获取快照时间戳
 */
LV00_PUBLIC_API uint64_t lv00_debug_snapshot_get_timestamp(const Lv00DebugSnapshot *snapshot) {
    if (!snapshot) return 0;
    return snapshot->timestamp_us;
}

/**
 * @brief 将快照信息格式化为字符串
 */
LV00_PUBLIC_API int lv00_debug_snapshot_format(const Lv00DebugSnapshot *snapshot,
                                                char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return 0;
    if (!snapshot) {
        lv00_strlcpy(buf, "(null snapshot)", buf_size);
        return (int)strlen(buf);
    }

    return lv00_snprintf(buf, buf_size,
        "Snapshot[%s]: step=%d/%d complete=%s color=%d state=%d breakpoints=%d ts=%llu",
        snapshot->label[0] ? snapshot->label : "(unnamed)",
        snapshot->step_index,
        snapshot->step_count,
        snapshot->is_complete ? "yes" : "no",
        snapshot->final_color,
        snapshot->proof_state,
        snapshot->breakpoint_count,
        (unsigned long long)snapshot->timestamp_us);
}

/* ============================================================
 * 推导日志 API 实现
 * ============================================================ */

/**
 * @brief 创建推导日志
 */
LV00_PUBLIC_API Lv00DerivationLog *lv00_derivation_log_create(void) {
    Lv00DerivationLog *log = (Lv00DerivationLog *)lv00_calloc(1, sizeof(Lv00DerivationLog));
    if (!log) return NULL;

    log->entries = (Lv00DerivationEntry *)lv00_calloc(
        LV00_DERIVATION_LOG_DEFAULT_CAPACITY, sizeof(Lv00DerivationEntry));
    if (!log->entries) {
        lv00_free((void **)&log);
        return NULL;
    }

    log->capacity = LV00_DERIVATION_LOG_DEFAULT_CAPACITY;
    log->count    = 0;
    log->active   = true;
    log->total_elapsed_us = 0;

    return log;
}

/**
 * @brief 销毁推导日志
 */
LV00_PUBLIC_API void lv00_derivation_log_destroy(Lv00DerivationLog *log) {
    if (!log) return;
    lv00_free((void **)&log->entries);
    lv00_free((void **)&log);
}

/**
 * @brief 清空推导日志
 */
LV00_PUBLIC_API void lv00_derivation_log_clear(Lv00DerivationLog *log) {
    if (!log) return;
    log->count = 0;
    log->total_elapsed_us = 0;
}

/**
 * @brief 激活/禁用推导日志
 */
LV00_PUBLIC_API void lv00_derivation_log_set_active(Lv00DerivationLog *log, bool active) {
    if (!log) return;
    log->active = active;
}

/**
 * @brief 检查推导日志是否激活
 */
LV00_PUBLIC_API bool lv00_derivation_log_is_active(const Lv00DerivationLog *log) {
    if (!log) return false;
    return log->active;
}

/**
 * @brief 追加一条推导日志条目
 */
LV00_PUBLIC_API bool lv00_derivation_log_append(Lv00DerivationLog *log,
                                                Lv00DerivationEntryType type,
                                                const char *rule_name,
                                                const char *description,
                                                uint64_t elapsed_us) {
    if (!log) return false;
    if (!log->active) return true; /* 未激活时静默成功 */
    if (log->count >= LV00_DERIVATION_LOG_MAX_CAPACITY) return false; /* 容量上限 */

    /* 扩容检查 */
    if (log->count >= log->capacity) {
        int new_cap = log->capacity * 2;
        if (new_cap > LV00_DERIVATION_LOG_MAX_CAPACITY) {
            new_cap = LV00_DERIVATION_LOG_MAX_CAPACITY;
        }
        Lv00DerivationEntry *new_entries = (Lv00DerivationEntry *)lv00_realloc(
            log->entries, (size_t)new_cap * sizeof(Lv00DerivationEntry));
        if (!new_entries) return false;
        log->entries  = new_entries;
        log->capacity = new_cap;
    }

    /* 填充条目 */
    Lv00DerivationEntry *entry = &log->entries[log->count];
    memset(entry, 0, sizeof(Lv00DerivationEntry));

    entry->type        = type;
    entry->step_number = log->count;
    entry->depth       = 0; /* 调用者可通过外部逻辑设置深度 */
    entry->timestamp_us = debug_trace_now_us();
    entry->elapsed_us  = elapsed_us;

    if (rule_name) {
        lv00_strlcpy(entry->rule_name, rule_name, sizeof(entry->rule_name));
    }
    if (description) {
        lv00_strlcpy(entry->description, description, sizeof(entry->description));
    }

    log->count++;
    log->total_elapsed_us += elapsed_us;

    return true;
}

/**
 * @brief 获取指定索引的日志条目
 */
LV00_PUBLIC_API const Lv00DerivationEntry *lv00_derivation_log_get_entry(
    const Lv00DerivationLog *log, int index) {
    if (!log || index < 0 || index >= log->count) return NULL;
    return &log->entries[index];
}

/**
 * @brief 获取日志条目数量
 */
LV00_PUBLIC_API int lv00_derivation_log_get_count(const Lv00DerivationLog *log) {
    if (!log) return 0;
    return log->count;
}

/**
 * @brief 按类型统计日志条目数量
 */
LV00_PUBLIC_API int lv00_derivation_log_count_by_type(const Lv00DerivationLog *log,
                                                      Lv00DerivationEntryType type) {
    if (!log) return 0;

    int count = 0;
    for (int i = 0; i < log->count; i++) {
        if (log->entries[i].type == type) {
            count++;
        }
    }
    return count;
}

/**
 * @brief 获取日志总耗时
 */
LV00_PUBLIC_API uint64_t lv00_derivation_log_get_total_elapsed(const Lv00DerivationLog *log) {
    if (!log) return 0;
    return log->total_elapsed_us;
}

/**
 * @brief 条目类型转字符串
 */
LV00_PUBLIC_API const char *lv00_derivation_entry_type_to_string(Lv00DerivationEntryType type) {
    switch (type) {
        case LV00_DERIVATION_RULE_APPLY:    return "RULE_APPLY";
        case LV00_DERIVATION_UNIFY_CHECK:   return "UNIFY_CHECK";
        case LV00_DERIVATION_REWRITE_STEP:  return "REWRITE_STEP";
        case LV00_DERIVATION_NORMALIZATION: return "NORMALIZATION";
        case LV00_DERIVATION_BRANCH:        return "BRANCH";
        case LV00_DERIVATION_BACKTRACK:     return "BACKTRACK";
        case LV00_DERIVATION_SUCCESS:       return "SUCCESS";
        case LV00_DERIVATION_FAILURE:        return "FAILURE";
        default:                            return "UNKNOWN";
    }
}

/**
 * @brief 将推导日志导出为文本
 */
LV00_PUBLIC_API int lv00_derivation_log_format(const Lv00DerivationLog *log,
                                                char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return 0;
    if (!log || log->count == 0) {
        lv00_strlcpy(buf, "(empty derivation log)", buf_size);
        return (int)strlen(buf);
    }

    int offset = 0;
    /* 写入头部摘要 */
    offset += lv00_snprintf(buf + offset, buf_size - (size_t)offset,
        "=== Derivation Log (%d entries, total %.3f ms) ===\n",
        log->count,
        log->total_elapsed_us / 1000.0);

    /* 逐条写入 */
    for (int i = 0; i < log->count && (size_t)offset < buf_size - 1; i++) {
        const Lv00DerivationEntry *e = &log->entries[i];
        offset += lv00_snprintf(buf + offset, buf_size - (size_t)offset,
            "  [%04d] %-16s %s%s%.3f ms\n",
            e->step_number,
            lv00_derivation_entry_type_to_string(e->type),
            e->rule_name[0] ? e->rule_name : "",
            e->rule_name[0] ? ": " : "",
            e->elapsed_us / 1000.0);
    }

    return offset;
}

/* ============================================================
 * 性能计时器 API 实现
 * ============================================================ */

/**
 * @brief 创建性能计时器
 */
LV00_PUBLIC_API Lv00PerfTimer *lv00_perf_timer_create(void) {
    Lv00PerfTimer *timer = (Lv00PerfTimer *)lv00_calloc(1, sizeof(Lv00PerfTimer));
    if (!timer) return NULL;

    timer->current_depth   = 0;
    timer->total_elapsed_us = 0;
    timer->call_count      = 0;
    timer->active          = true;

    return timer;
}

/**
 * @brief 销毁性能计时器
 */
LV00_PUBLIC_API void lv00_perf_timer_destroy(Lv00PerfTimer *timer) {
    if (!timer) return;
    lv00_free((void **)&timer);
}

/**
 * @brief 重置性能计时器
 */
LV00_PUBLIC_API void lv00_perf_timer_reset(Lv00PerfTimer *timer) {
    if (!timer) return;
    memset(timer->stack, 0, sizeof(timer->stack));
    timer->current_depth    = 0;
    timer->total_elapsed_us = 0;
    timer->call_count       = 0;
}

/**
 * @brief 开始一次计时区间
 */
LV00_PUBLIC_API bool lv00_perf_timer_begin(Lv00PerfTimer *timer, const char *label) {
    if (!timer || !timer->active) return false;
    if (timer->current_depth >= LV00_PERF_TIMER_MAX_DEPTH) return false;

    Lv00PerfTimerInterval *interval = &timer->stack[timer->current_depth];
    memset(interval, 0, sizeof(Lv00PerfTimerInterval));

    if (label) {
        lv00_strlcpy(interval->label, label, sizeof(interval->label));
    }
    interval->begin_us = debug_trace_now_us();
    interval->depth    = timer->current_depth;
    interval->completed = false;

    timer->current_depth++;
    timer->call_count++;

    return true;
}

/**
 * @brief 结束最近一次计时区间
 */
LV00_PUBLIC_API bool lv00_perf_timer_end(Lv00PerfTimer *timer) {
    if (!timer || !timer->active) return false;
    if (timer->current_depth <= 0) return false;

    timer->current_depth--;
    Lv00PerfTimerInterval *interval = &timer->stack[timer->current_depth];

    interval->end_us     = debug_trace_now_us();
    interval->elapsed_us = interval->end_us - interval->begin_us;
    interval->completed   = true;

    timer->total_elapsed_us += interval->elapsed_us;

    return true;
}

/**
 * @brief 获取当前嵌套深度
 */
LV00_PUBLIC_API int lv00_perf_timer_get_depth(const Lv00PerfTimer *timer) {
    if (!timer) return 0;
    return timer->current_depth;
}

/**
 * @brief 获取总累计耗时
 */
LV00_PUBLIC_API uint64_t lv00_perf_timer_get_total_elapsed(const Lv00PerfTimer *timer) {
    if (!timer) return 0;
    return timer->total_elapsed_us;
}

/**
 * @brief 获取总调用次数
 */
LV00_PUBLIC_API uint64_t lv00_perf_timer_get_call_count(const Lv00PerfTimer *timer) {
    if (!timer) return 0;
    return timer->call_count;
}

/**
 * @brief 获取最近一次完成的区间耗时
 */
LV00_PUBLIC_API uint64_t lv00_perf_timer_get_last_elapsed(const Lv00PerfTimer *timer) {
    if (!timer || timer->current_depth <= 0) return 0;

    /* 查找最近完成的区间（从栈顶向下搜索） */
    for (int i = timer->current_depth - 1; i >= 0; i--) {
        if (timer->stack[i].completed) {
            return timer->stack[i].elapsed_us;
        }
    }
    return 0;
}

/**
 * @brief 将计时器统计信息格式化为字符串
 */
LV00_PUBLIC_API int lv00_perf_timer_format(const Lv00PerfTimer *timer,
                                            char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) return 0;
    if (!timer) {
        lv00_strlcpy(buf, "(null timer)", buf_size);
        return (int)strlen(buf);
    }

    double avg_ms = 0.0;
    if (timer->call_count > 0) {
        avg_ms = (timer->total_elapsed_us / 1000.0) / (double)timer->call_count;
    }

    return lv00_snprintf(buf, buf_size,
        "PerfTimer: calls=%llu total=%.3f ms avg=%.3f ms depth=%d",
        (unsigned long long)timer->call_count,
        timer->total_elapsed_us / 1000.0,
        avg_ms,
        timer->current_depth);
}
