/**
 * @file debug_trace.c
 * @brief 调试追踪实现
 *
 * 记录执行步骤、设置断点、管理追踪信息。
 * 基于 debug.h 中定义的 TraceSession / TraceEvent 结构，
 * 提供追踪会话的创建、事件记录、断点管理和导出功能。
 *
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/debug.h"
#include "lv/lv_json.h"
#include "lv/lv_utils.h"

/* ========================================================================
 * 追踪会话管理
 * ======================================================================== */

#define INITIAL_EVENT_CAPACITY 64
#define MAX_BREAKPOINTS 32

/** 断点描述 */
typedef struct {
    TraceEventType type; /**< 断点关联的事件类型 */
    int step_number;     /**< 断点触发的步骤号（-1 表示任意步骤） */
    int hit_count;       /**< 命中次数 */
    int enabled;         /**< 是否启用 */
} TraceBreakpoint;

/** 调试追踪器（全局单例） */
typedef struct {
    TraceSession session;                         /**< 追踪会话 */
    TraceBreakpoint breakpoints[MAX_BREAKPOINTS]; /**< 断点数组 */
    int breakpoint_count;                         /**< 断点数量 */
    int paused;                                   /**< 是否因断点暂停 */
} DebugTraceState;

static DebugTraceState g_trace_state = {0};

/* ========================================================================
 * 追踪会话 API
 * ======================================================================== */

/** @brief 创建追踪会话 */
TraceSession *trace_session_create(void) {
    TraceSession *session = (TraceSession *) lv_calloc(1, sizeof(TraceSession));
    if (session == NULL)
        return NULL;

    session->capacity = INITIAL_EVENT_CAPACITY;
    session->events = (TraceEvent *) lv_calloc((size_t) session->capacity, sizeof(TraceEvent));
    if (session->events == NULL) {
        lv_free((void **) &session);
        return NULL;
    }

    session->event_count = 0;
    session->active = 1;
    return session;
}

/** @brief 销毁追踪会话，释放所有关联资源 */
void trace_session_destroy(TraceSession *session) {
    if (session == NULL)
        return;

    for (int i = 0; i < session->event_count; i++) {
        lv_free((void **) &session->events[i].description);
        lv_free((void **) &session->events[i].details);
    }
    lv_free((void **) &session->events);
    lv_free((void **) &session);
}

/** @brief 向追踪会话中记录一个事件 */
void trace_record_event(TraceSession *session, TraceEventType type, int step, const char *description,
                        const char *details) {
    if (session == NULL || !session->active)
        return;

    /* 容量检查，必要时扩容 */
    if (session->event_count >= session->capacity) {
        int new_cap = session->capacity * 2;
        TraceEvent *new_events = (TraceEvent *) lv_realloc(session->events, (size_t) new_cap * sizeof(TraceEvent));
        if (new_events == NULL)
            return;
        session->events = new_events;
        session->capacity = new_cap;
    }

    TraceEvent *evt = &session->events[session->event_count++];
    evt->type = type;
    evt->step_number = step;
    evt->timestamp = (double) clock() / CLOCKS_PER_SEC;
    evt->description = (description != NULL) ? lv_strdup_safe(description) : NULL;
    evt->details = (details != NULL) ? lv_strdup_safe(details) : NULL;
}

/* ========================================================================
 * 断点管理
 * ======================================================================== */

/** @brief 添加一个调试断点 */
int debug_trace_add_breakpoint(TraceEventType type, int step_number) {
    DebugTraceState *state = &g_trace_state;
    if (state->breakpoint_count >= MAX_BREAKPOINTS)
        return -1;

    TraceBreakpoint *bp = &state->breakpoints[state->breakpoint_count++];
    bp->type = type;
    bp->step_number = step_number;
    bp->hit_count = 0;
    bp->enabled = 1;
    return state->breakpoint_count - 1;
}

/** @brief 移除指定索引的断点 */
void debug_trace_remove_breakpoint(int index) {
    DebugTraceState *state = &g_trace_state;
    if (index < 0 || index >= state->breakpoint_count)
        return;

    /* 移除断点：将后续元素前移 */
    for (int i = index; i < state->breakpoint_count - 1; i++) {
        state->breakpoints[i] = state->breakpoints[i + 1];
    }
    state->breakpoint_count--;
}

/** @brief 检查是否命中断点 */
int debug_trace_check_breakpoint(TraceEventType type, int step) {
    DebugTraceState *state = &g_trace_state;
    for (int i = 0; i < state->breakpoint_count; i++) {
        TraceBreakpoint *bp = &state->breakpoints[i];
        if (!bp->enabled)
            continue;
        if (bp->type != type)
            continue;
        if (bp->step_number >= 0 && bp->step_number != step)
            continue;
        bp->hit_count++;
        state->paused = 1;
        return i; /* 返回命中的断点索引 */
    }
    return -1; /* 未命中 */
}

/* ========================================================================
 * 导出
 * ======================================================================== */

/** @brief 将追踪会话导出为 JSON 字符串 */
char *trace_session_export_json(const TraceSession *session) {
    if (session == NULL)
        return NULL;

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, (size_t) session->event_count * 256 + 256))
        return NULL;

    lv_json_buf_append_fmt(&buf, "{\"event_count\":%d,\"events\":[\n", session->event_count);

    for (int i = 0; i < session->event_count; i++) {
        const TraceEvent *evt = &session->events[i];
        const char *type_str = "unknown";
        if (evt->type == TRACE_NORMALIZATION)
            type_str = "normalization";
        else if (evt->type == TRACE_REWRITE)
            type_str = "rewrite";
        else if (evt->type == TRACE_SOLVER)
            type_str = "solver";

        lv_json_buf_append_raw(&buf, "  {\"type\":\"");
        lv_json_buf_append_string(&buf, type_str);
        lv_json_buf_append_fmt(&buf, "\",\"step\":%d,\"time\":%.6f,\"desc\":\"", evt->step_number, evt->timestamp);
        lv_json_buf_append_string(&buf, evt->description ? evt->description : "");
        lv_json_buf_append_raw(&buf, "\"}");
        if (i < session->event_count - 1)
            lv_json_buf_append_raw(&buf, ",");
        lv_json_buf_append_raw(&buf, "\n");
    }

    lv_json_buf_append_raw(&buf, "]}\n");
    return lv_json_buf_finalize(&buf);
}

/** @brief 获取全局调试追踪会话 */
TraceSession *debug_get_trace_session(void) {
    return &g_trace_state.session;
}
