/**
 * @file engine_stream.c
 * @brief 引擎流式输出 API（从 engine.c 拆分）
 *
 * @details 负责流式上下文的获取、启停与事件发射。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/engine.h"

#include <string.h>

#include "lv/lv.h"
#include "lv/stream.h"

#include "lv/stream.h"

/** @brief 获取引擎的流式上下文 @param engine 引擎实例 @return 流式上下文指针 */
StreamContext *engine_get_stream_context(const lvEngine *engine) {
    if (!engine)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "engine_get_stream_context: engine is NULL");
    return engine->stream_ctx;
}

/** @brief 启用或禁用流式输出 @param engine 引擎实例 @param enabled true 启用 */
void engine_set_streaming_enabled(lvEngine *engine, bool enabled) {
    if (!engine)
        return;
    if (!enabled && engine->stream_ctx) {
        /* 禁用时销毁流式上下文，同步清空子模块 */
        stream_context_destroy(engine->stream_ctx);
        engine->stream_ctx = NULL;
        /* 通过分发机制统一清空所有已注册子模块的流式上下文 */
        stream_context_dispatch_all(NULL);
    } else if (enabled && !engine->stream_ctx) {
        /* 启用时重新创建流式上下文，通过分发机制同步到所有子模块 */
        engine->stream_ctx = stream_context_create();
        if (!engine->stream_ctx) {
            /* lv_LOG_ERROR("engine_set_streaming_enabled: stream_context_create() 返回 NULL，流式输出未能启用"); */
            return;
        }
        stream_context_dispatch_all(engine->stream_ctx);
    }
}

/** @brief 查询流式输出是否启用 @param engine 引擎实例 @return true 已启用 */
bool engine_is_streaming_enabled(const lvEngine *engine) {
    if (!engine)
        return false;
    return engine->stream_ctx != NULL;
}

/** @brief 发射流式事件 @param engine 引擎实例 @param event_type 事件类型 @param ... 事件数据 */
void engine_emit_stream_event(lvEngine *engine, StreamEventType type, const char *description, int step_number,
                              int node_id, int constraint_id) {
    if (!engine || !engine->stream_ctx)
        return;

    StreamEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.timestamp_ms = stream_timestamp_ms();
    ev.step_number = step_number;
    ev.total_steps = -1;
    ev.node_id = node_id;
    ev.constraint_id = constraint_id;
    ev.rule_id = -1;
    ev.var_id = -1;
    ev.description = description;
    ev.progress = -1.0;
    ev.numeric_value = 0.0;

    stream_emit(engine->stream_ctx, &ev);
}
