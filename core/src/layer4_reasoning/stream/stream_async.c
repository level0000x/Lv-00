/**
 * @file stream_async.c
 * @brief 流式输出系统 —— 异步模式（消费者线程）
 */

#include "stream_internal.h"


/* ==================== 异步模式 API ==================== */

/* ==================== 异步消费者线程 ==================== */

static void *async_consumer_thread(void *arg) {
    StreamContext *ctx = (StreamContext *) arg;

    while (true) {
        lv_mutex_lock(&ctx->async_mutex);

        /* 等待缓冲区非空或停止信号 */
        while (ctx->buffer_count == 0 && ctx->async_running) {
            lv_cond_wait(&ctx->async_cond_not_empty, &ctx->async_mutex);
        }

        /* 检查停止信号 */
        if (!ctx->async_running) {
            /* 排空剩余事件 */
            while (ctx->buffer_count > 0) {
                StreamEvent ev = ctx->buffer[ctx->buffer_head];
                ctx->buffer_head = (ctx->buffer_head + 1) % ctx->buffer_capacity;
                ctx->buffer_count--;
                lv_mutex_unlock(&ctx->async_mutex);
                stream_dispatch(ctx, &ev);
                lv_mutex_lock(&ctx->async_mutex);
            }
            ctx->buffer_head = 0;

            /* 通知等待 flush 的线程 */
            if (ctx->async_flush_waiters > 0) {
                lv_cond_signal(&ctx->async_cond_flushed);
            }

            lv_mutex_unlock(&ctx->async_mutex);
            break;
        }

        /* 取出一个事件 */
        StreamEvent ev = ctx->buffer[ctx->buffer_head];
        ctx->buffer_head = (ctx->buffer_head + 1) % ctx->buffer_capacity;
        ctx->buffer_count--;

        /* 如果队列已空，通知等待 flush 的线程 */
        if (ctx->buffer_count == 0 && ctx->async_flush_waiters > 0) {
            lv_cond_signal(&ctx->async_cond_flushed);
        }

        lv_mutex_unlock(&ctx->async_mutex);

        /* 在锁外执行回调（避免死锁） */
        stream_dispatch(ctx, &ev);
    }

    return NULL;
}

/**
 * 设置异步模式。
 *
 * 启用真正的多线程异步事件分发：
 * - 启用时：创建消费者线程，事件通过互斥锁保护的环形缓冲区传递，
 *           消费者线程在条件变量上等待，有事件时自动唤醒并分发。
 * - 禁用时：通知消费者线程排空剩余事件后退出，销毁同步原语，
 *           恢复为 IMMEDIATE 模式。
 *
 * @param ctx      流式上下文
 * @param enabled  true 启用异步，false 恢复同步
 * @param capacity 队列容量（仅在 enabled=true 时生效，0 使用默认值 1024）
 * @return true 成功，false 失败（ctx 为 NULL 或内存不足）
 */
bool stream_set_async_mode(StreamContext *ctx, bool enabled, int capacity) {
    if (!ctx)
        return false;

    if (enabled) {
        /* 如果已经启用，不重复创建 */
        if (ctx->async_enabled && ctx->async_running)
            return true;

        /* 确保缓冲区已分配 */
        int buf_cap = capacity > 0 ? capacity : STREAM_ASYNC_QUEUE_DEFAULT_CAPACITY;
        if (buf_cap > STREAM_MAX_BUFFER)
            buf_cap = STREAM_MAX_BUFFER;

        if (!ctx->buffer || ctx->buffer_capacity < buf_cap) {
            StreamEvent *new_buf = (StreamEvent *) lv_realloc(ctx->buffer, (size_t) buf_cap * sizeof(StreamEvent));
            if (!new_buf)
                return false;
            ctx->buffer = new_buf;
            ctx->buffer_capacity = buf_cap;
            ctx->buffer_count = 0;
            ctx->buffer_head = 0;
        }

        /* 创建同步原语（栈分配 + 初始化） */
        lv_mutex_init(&ctx->async_mutex);
        lv_cond_init(&ctx->async_cond_not_empty);
        lv_cond_init(&ctx->async_cond_flushed);

        /* 启动消费者线程 */
        ctx->async_running = true;
        ctx->async_flush_waiters = 0;

        if (lv_thread_create(&ctx->async_thread, async_consumer_thread, ctx) != 0) {
            ctx->async_running = false;
            lv_cond_destroy(&ctx->async_cond_flushed);
            lv_cond_destroy(&ctx->async_cond_not_empty);
            lv_mutex_destroy(&ctx->async_mutex);
            return false;
        }

        ctx->async_enabled = true;
        ctx->emit_mode = STREAM_EMIT_BUFFERED;

        return true;
    } else {
        /* 禁用异步模式 */
        if (!ctx->async_enabled)
            return true;

        /* 通知消费者线程停止 */
        lv_mutex_lock(&ctx->async_mutex);
        ctx->async_running = false;
        lv_cond_signal(&ctx->async_cond_not_empty);
        lv_mutex_unlock(&ctx->async_mutex);

        /* 等待消费者线程退出 */
        lv_thread_join(ctx->async_thread);

        /* 销毁同步原语 */
        lv_cond_destroy(&ctx->async_cond_flushed);
        lv_cond_destroy(&ctx->async_cond_not_empty);
        lv_mutex_destroy(&ctx->async_mutex);

        ctx->async_enabled = false;
        ctx->async_running = false;

        /* 恢复为 IMMEDIATE 模式 */
        ctx->emit_mode = STREAM_EMIT_IMMEDIATE;

        return true;
    }
}

/**
 * 刷新缓冲区中的事件。
 *
 * 将缓冲区中所有待处理事件按 FIFO 顺序分发到所有过滤匹配的回调。
 * 刷新完成后重置读写指针。
 *
 * @param ctx 流式上下文
 */
void stream_flush(StreamContext *ctx) {
    if (!ctx)
        return;

    /* 异步模式：阻塞等待消费者线程排空队列 */
    if (ctx->async_enabled && ctx->async_running) {
        lv_mutex_lock(&ctx->async_mutex);
        ctx->async_flush_waiters++;
        while (ctx->buffer_count > 0 && ctx->async_running) {
            lv_cond_wait(&ctx->async_cond_flushed, &ctx->async_mutex);
        }
        ctx->async_flush_waiters--;
        lv_mutex_unlock(&ctx->async_mutex);
        return;
    }

    /* 同步模式：直接分发 */
    if (ctx->buffer_count == 0)
        return;

    while (ctx->buffer_count > 0) {
        StreamEvent *ev = &ctx->buffer[ctx->buffer_head];
        stream_dispatch(ctx, ev);
        ctx->buffer_head = (ctx->buffer_head + 1) % ctx->buffer_capacity;
        ctx->buffer_count--;
    }
    ctx->buffer_head = 0;
}

/**
 * 获取缓冲区中待处理事件数量。
 *
 * @param ctx 流式上下文
 * @return 待处理事件数，ctx 为 NULL 时返回 0
 */
int stream_pending_count(StreamContext *ctx) {
    if (!ctx)
        return 0;
    return ctx->buffer_count;
}

