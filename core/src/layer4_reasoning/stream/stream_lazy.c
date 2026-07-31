/**
 * @file stream_lazy.c
 * @brief 流式输出系统 —— 惰性拉取模式
 */

#include "stream_internal.h"


/* ============================================================
 * 惰性拉取模式（完整实现 —— LZ/2026-05-23）
 *
 * 惰性模式让事件排队到 lazy_queue，消费者通过
 * stream_lazy_next / stream_lazy_drain 主动拉取。
 * 与 BUFFERED/THROTTLED 不同，惰性模式不自动调用 stream_dispatch，
 * 而是由消费者控制何时分发。
 *
 * 当 lazy_threshold > 0 且队列达到阈值时，
 * stream_emit 会自动触发 stream_flush 刷新。
 * ============================================================ */

/**
 * @brief 确保惰性队列有足够容量
 */
static bool stream_lazy_ensure_capacity(StreamContext *ctx) {
    if (ctx->lazy_capacity == 0) {
        ctx->lazy_capacity = 64;
        ctx->lazy_queue = (StreamEvent *) lv_calloc(1, sizeof(StreamEvent) * (size_t) ctx->lazy_capacity);
        return ctx->lazy_queue != NULL;
    }
    if (ctx->lazy_count >= ctx->lazy_capacity) {
        int new_cap = ctx->lazy_capacity * 2;
        if (new_cap > STREAM_MAX_LAZY) {
            ctx->dropped_count++;
            return false;
        }
        StreamEvent *new_queue = (StreamEvent *) lv_calloc((size_t) new_cap, sizeof(StreamEvent));
        if (!new_queue)
            return false;
        /* 拷贝环形缓冲区到线性数组 */
        for (int i = 0; i < ctx->lazy_count; i++) {
            int src = (ctx->lazy_head + i) % ctx->lazy_capacity;
            memcpy(&new_queue[i], &ctx->lazy_queue[src], sizeof(StreamEvent));
        }
        lv_free((void **) &ctx->lazy_queue);
        ctx->lazy_queue = new_queue;
        ctx->lazy_capacity = new_cap;
        ctx->lazy_head = 0;
    }
    return true;
}

/**
 * @brief 事件入队到惰性队列
 */
void stream_lazy_enqueue(StreamContext *ctx, const StreamEvent *event) {
    if (!ctx || !event)
        return;
    if (!stream_lazy_ensure_capacity(ctx))
        return;
    int tail = (ctx->lazy_head + ctx->lazy_count) % ctx->lazy_capacity;
    memcpy(&ctx->lazy_queue[tail], event, sizeof(StreamEvent));
    ctx->lazy_count++;
}

/**
 * @brief 惰性拉取下一个待处理事件
 *
 * 从惰性队列头部取出第一个事件返回。
 * 调用者应当在使用完后通过 stream_lazy_drain 或 stream_flush 清理。
 *
 * @param ctx 流式上下文
 * @return 队列头部事件指针（属于队列内部内存），队列为空返回NULL
 */
const StreamEvent *stream_lazy_next(StreamContext *ctx) {
    if (!ctx || ctx->lazy_count == 0)
        return NULL;
    return &ctx->lazy_queue[ctx->lazy_head];
}

/**
 * @brief 批量惰性拉取事件
 *
 * 从惰性队列头部开始，按 FIFO 顺序逐个调用 callback。
 * 最多处理 max_count 个事件。每处理一个，从队列中移除。
 *
 * @param ctx       流式上下文
 * @param callback  处理每个事件的回调
 * @param user_data 回调用户数据
 * @param max_count 最大处理事件数（<=0 表示无限制，但不推荐超过队列大小）
 * @return 实际处理的事件数
 */
int stream_lazy_drain(StreamContext *ctx, StreamCallback callback, void *user_data, int max_count) {
    if (!ctx || !callback || ctx->lazy_count == 0)
        return 0;

    int limit = max_count > 0 ? max_count : ctx->lazy_count;
    if (limit > ctx->lazy_count)
        limit = ctx->lazy_count;

    int drained = 0;
    for (int i = 0; i < limit; i++) {
        int idx = (ctx->lazy_head + i) % ctx->lazy_capacity;
        callback(&ctx->lazy_queue[idx], user_data);
        drained++;

        /* 更新 dispatch 统计 */
        ctx->total_count++;
    }

    /* 将剩余事件前移 */
    ctx->lazy_head = (ctx->lazy_head + drained) % ctx->lazy_capacity;
    ctx->lazy_count -= drained;

    return drained;
}

/**
 * @brief 获取惰性队列中的待处理事件数
 */
int stream_lazy_pending(const StreamContext *ctx) {
    if (!ctx)
        return 0;
    return ctx->lazy_count;
}

/**
 * @brief 设置惰性模式的自动刷新阈值
 *
 * 当 lazy_threshold > 0 时，stream_emit 在懒队列达到阈值
 * 时自动调用 stream_flush() 刷新事件。
 *
 * @param ctx       流式上下文
 * @param threshold 事件数阈值（0 禁用自动刷新）
 */
void stream_set_lazy_threshold(StreamContext *ctx, int threshold) {
    if (!ctx)
        return;
    ctx->lazy_threshold = (threshold > 0) ? threshold : 0;
}
