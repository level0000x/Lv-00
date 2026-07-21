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

#include "lv00/debug.h"
#include "lv00/lv00_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========================================================================
 * 追踪会话管理
 * ======================================================================== */

#define INITIAL_EVENT_CAPACITY 64
#define MAX_BREAKPOINTS 32

/** 断点描述 */
typedef struct {
    TraceEventType type;    /**< 断点关联的事件类型 */
    int step_number;        /**< 断点触发的步骤号（-1 表示任意步骤） */
    int hit_count;          /**< 命中次数 */
    int enabled;            /**< 是否启用 */
} TraceBreakpoint;

/** 调试追踪器（全局单例） */
typedef struct {
    TraceSession session;          /**< 追踪会话 */
    TraceBreakpoint breakpoints[MAX_BREAKPOINTS]; /**< 断点数组 */
    int breakpoint_count;          /**< 断点数量 */
    int paused;                    /**< 是否因断点暂停 */
} DebugTraceState;

static DebugTraceState g_trace_state = {0};

/* ========================================================================
 * 追踪会话 API
 * ======================================================================== */

TraceSession *trace_session_create(void)
{
    TraceSession *session = (TraceSession *)calloc(1, sizeof(TraceSession));
    if (session == NULL) return NULL;

    session->capacity = INITIAL_EVENT_CAPACITY;
    session->events = (TraceEvent *)calloc((size_t)session->capacity, sizeof(TraceEvent));
    if (session->events == NULL) {
        free(session);
        return NULL;
    }

    session->event_count = 0;
    session->active = 1;
    return session;
}

void trace_session_destroy(TraceSession *session)
{
    if (session == NULL) return;

    for (int i = 0; i < session->event_count; i++) {
        free(session->events[i].description);
        free(session->events[i].details);
    }
    free(session->events);
    free(session);
}

void trace_record_event(TraceSession *session, TraceEventType type,
                         int step, const char *description, const char *details)
{
    if (session == NULL || !session->active) return;

    /* 容量检查，必要时扩容 */
    if (session->event_count >= session->capacity) {
        int new_cap = session->capacity * 2;
        TraceEvent *new_events = (TraceEvent *)lv00_realloc(
            session->events, (size_t)new_cap * sizeof(TraceEvent));
        if (new_events == NULL) return;
        session->events = new_events;
        session->capacity = new_cap;
    }

    TraceEvent *evt = &session->events[session->event_count++];
    evt->type = type;
    evt->step_number = step;
    evt->timestamp = (double)clock() / CLOCKS_PER_SEC;
    evt->description = (description != NULL) ? strdup(description) : NULL;
    evt->details = (details != NULL) ? strdup(details) : NULL;
}

/* ========================================================================
 * 断点管理
 * ======================================================================== */

int debug_trace_add_breakpoint(TraceEventType type, int step_number)
{
    DebugTraceState *state = &g_trace_state;
    if (state->breakpoint_count >= MAX_BREAKPOINTS) return -1;

    TraceBreakpoint *bp = &state->breakpoints[state->breakpoint_count++];
    bp->type = type;
    bp->step_number = step_number;
    bp->hit_count = 0;
    bp->enabled = 1;
    return state->breakpoint_count - 1;
}

void debug_trace_remove_breakpoint(int index)
{
    DebugTraceState *state = &g_trace_state;
    if (index < 0 || index >= state->breakpoint_count) return;

    /* 移除断点：将后续元素前移 */
    for (int i = index; i < state->breakpoint_count - 1; i++) {
        state->breakpoints[i] = state->breakpoints[i + 1];
    }
    state->breakpoint_count--;
}

int debug_trace_check_breakpoint(TraceEventType type, int step)
{
    DebugTraceState *state = &g_trace_state;
    for (int i = 0; i < state->breakpoint_count; i++) {
        TraceBreakpoint *bp = &state->breakpoints[i];
        if (!bp->enabled) continue;
        if (bp->type != type) continue;
        if (bp->step_number >= 0 && bp->step_number != step) continue;
        bp->hit_count++;
        state->paused = 1;
        return i; /* 返回命中的断点索引 */
    }
    return -1; /* 未命中 */
}

/* ========================================================================
 * 导出
 * ======================================================================== */

char *trace_session_export_json(const TraceSession *session)
{
    if (session == NULL) return NULL;

    /* 估算缓冲区大小 */
    size_t buf_size = (size_t)(session->event_count * 256) + 256;
    char *buf = (char *)malloc(buf_size);
    if (buf == NULL) return NULL;

    int offset = 0;
    offset += snprintf(buf + offset, buf_size - (size_t)offset,
                       "{\"event_count\":%d,\"events\":[\n",
                       session->event_count);

    for (int i = 0; i < session->event_count; i++) {
        const TraceEvent *evt = &session->events[i];
        const char *type_str = "unknown";
        if (evt->type == TRACE_NORMALIZATION) type_str = "normalization";
        else if (evt->type == TRACE_REWRITE) type_str = "rewrite";
        else if (evt->type == TRACE_SOLVER) type_str = "solver";

        offset += snprintf(buf + offset, buf_size - (size_t)offset,
                           "  {\"type\":\"%s\",\"step\":%d,\"time\":%.6f,"
                           "\"desc\":\"%s\"}%s\n",
                           type_str, evt->step_number, evt->timestamp,
                           evt->description ? evt->description : "",
                           (i < session->event_count - 1) ? "," : "");
    }

    snprintf(buf + offset, buf_size - (size_t)offset, "]}\n");
    return buf;
}

TraceSession *debug_get_trace_session(void)
{
    return &g_trace_state.session;
}
