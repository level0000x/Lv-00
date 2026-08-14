/**
 * @file stream_stats.c
 * @brief 流式输出系统 —— 事件统计
 */

#include "stream_internal.h"


/* ==================== 事件统计 API ==================== */

/**
 * 重置事件统计计数器。
 * 将所有事件类型计数、总计数和丢弃计数清零。
 *
 * @param ctx 流式上下文
 */
void stream_reset_stats(StreamContext *ctx) {
    if (!ctx)
        return;
    memset(ctx->event_counts, 0, sizeof(ctx->event_counts));
    ctx->total_count = 0;
    ctx->dropped_count = 0;
}

/**
 * 获取指定事件类型的发射次数。
 *
 * @param ctx  流式上下文
 * @param type 事件类型
 * @return 发射次数，ctx 为 NULL 或类型越界时返回 0
 */
int64_t stream_get_event_count(const StreamContext *ctx, StreamEventType type) {
    if (!ctx)
        return 0;
    int idx = (int) type;
    if (idx >= 0 && idx < STREAM_EVENT_TYPE_COUNT) {
        return ctx->event_counts[idx];
    }
    return 0;
}

/**
 * 获取事件发射总数。
 *
 * @param ctx 流式上下文
 * @return 总发射次数，ctx 为 NULL 时返回 0
 */
int64_t stream_get_total_event_count(StreamContext *ctx) {
    if (!ctx)
        return 0;
    return ctx->total_count;
}

/**
 * 获取已丢弃的事件数（缓冲区满时）。
 *
 * @param ctx 流式上下文
 * @return 丢弃的事件数，ctx 为 NULL 时返回 0
 */
long stream_get_dropped_count(const StreamContext *ctx) {
    if (!ctx)
        return 0;
    return ctx->dropped_count;
}

