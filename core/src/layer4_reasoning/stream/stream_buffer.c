/**
 * @file stream_buffer.c
 * @brief 流式输出系统 —— 事件缓冲区与事件发射
 */

#include "stream_internal.h"


/* ==================== 事件缓冲区管理（内部） ==================== */

/**
 * @brief 确保事件缓冲区有足够容量
 *
 * 如果缓冲区未分配，则首次分配 STREAM_INITIAL_BUFFER 容量。
 * 如果已满，则倍增扩容，上限为 STREAM_MAX_BUFFER。
 *
 * @param ctx 流式上下文
 * @return true 容量足够或扩容成功，false 缓冲区已满或分配失败
 */
static bool stream_ensure_buffer(StreamContext *ctx) {
    if (ctx->buffer_count < ctx->buffer_capacity)
        return true;
    if (ctx->buffer_capacity >= STREAM_MAX_BUFFER)
        return false;

    int new_cap = ctx->buffer_capacity == 0 ? STREAM_INITIAL_BUFFER : ctx->buffer_capacity * 2;
    if (new_cap > STREAM_MAX_BUFFER)
        new_cap = STREAM_MAX_BUFFER;

    StreamEvent *new_buf = (StreamEvent *) lv_realloc(ctx->buffer, (size_t) new_cap * sizeof(StreamEvent));
    if (!new_buf)
        return false;

    ctx->buffer = new_buf;
    ctx->buffer_capacity = new_cap;
    return true;
}

/**
 * @brief 将事件追加到缓冲区（环形队列尾部）
 *
 * 如果缓冲区已满（达到 STREAM_MAX_BUFFER），事件被丢弃并增加 dropped_count。
 *
 * @param ctx   流式上下文
 * @param event 待缓冲的事件
 */
static void stream_buffer_push(StreamContext *ctx, const StreamEvent *event) {
    if (!stream_ensure_buffer(ctx)) {
        /* 缓冲区满，丢弃事件并计数 */
        ctx->dropped_count++;
        return;
    }

    /* 计算写入位置（环形队列） */
    int write_pos = (ctx->buffer_head + ctx->buffer_count) % ctx->buffer_capacity;
    ctx->buffer[write_pos] = *event;
    ctx->buffer_count++;
}

/* ==================== 事件发射 ==================== */

/**
 * @brief 分发过滤：事件类型位掩码匹配（与 stream 蓝本原逻辑一致）
 */
static bool stream_cb_filter(const lvCallbackEntry *entry, const void *arg) {
    const StreamEvent *event = (const StreamEvent *) arg;
    uint64_t mask = entry->filter;
    return (mask & STREAM_EVENT_MASK(event->type)) != 0;
}

/**
 * @brief 分发调用：将泛型回调转回 StreamCallback 签名后调用
 */
static void stream_cb_invoke(const lvCallbackEntry *entry, const void *arg) {
    const StreamEvent *event = (const StreamEvent *) arg;
    StreamCallback cb = (StreamCallback) entry->callback;
    cb(event, entry->user_data);
}

/**
 * @brief 内部发射函数：分发事件到所有过滤匹配的回调
 *
 * 遍历回调列表，对每个回调检查其 filter 过滤值（事件类型位掩码）
 * 是否与事件类型匹配。仅当 (filter & STREAM_EVENT_MASK(event->type)) != 0
 * 时才调用回调。迭代安全由公共设施保证（快照 count + 越界检查，
 * 遍历中注册/注销安全）。
 *
 * @param ctx   流式上下文
 * @param event 事件数据
 */
void stream_dispatch(StreamContext *ctx, const StreamEvent *event) {
    if (!ctx || !event)
        return;
    lv_callback_list_dispatch(&ctx->callback_list, event, stream_cb_filter, stream_cb_invoke);
}

/**
 * @brief 检查节流间隔是否已过
 *
 * 如果距上次发射已超过 throttle_ms 毫秒，则返回 true 并更新时间戳。
 * 首次调用（last_emit_ms == 0）时直接返回 true。
 *
 * @param ctx 流式上下文
 * @return true 节流间隔已过，false 尚未到期
 */
static bool stream_throttle_expired(StreamContext *ctx) {
    long now = stream_timestamp_ms();
    if (ctx->last_emit_ms == 0 || (now - ctx->last_emit_ms) >= ctx->throttle_ms) {
        ctx->last_emit_ms = now;
        return true;
    }
    return false;
}

/**
 * @brief 更新事件统计计数器
 *
 * 在每次事件发射时调用，累加对应类型的计数和总数。
 *
 * @param ctx   流式上下文
 * @param event 事件数据
 */
static void stream_update_stats(StreamContext *ctx, const StreamEvent *event) {
    if (!ctx)
        return;
    int type_idx = (int) event->type;
    if (type_idx >= 0 && type_idx < STREAM_EVENT_TYPE_COUNT) {
        ctx->event_counts[type_idx]++;
    }
    ctx->total_count++;
}

/* 发射模式 → 事件处理函数（函数指针表分发） */
typedef void (*StreamEmitHandler)(StreamContext *ctx, const StreamEvent *event);

static void emit_mode_immediate(StreamContext *ctx, const StreamEvent *event) {
    stream_dispatch(ctx, event);
}

static void emit_mode_buffered(StreamContext *ctx, const StreamEvent *event) {
    stream_buffer_push(ctx, event);
}

static void emit_mode_throttled(StreamContext *ctx, const StreamEvent *event) {
    stream_buffer_push(ctx, event);
    if (stream_throttle_expired(ctx)) {
        stream_flush(ctx);
    }
}

static void emit_mode_lazy(StreamContext *ctx, const StreamEvent *event) {
    /* 惰性模式：事件仅入队到 lazy_queue，
     * 由消费者通过 stream_lazy_next / stream_lazy_drain 主动拉取。
     * 当队列达到阈值时自动触发刷新。 */
    stream_lazy_enqueue(ctx, event);
    if (ctx->lazy_threshold > 0 && ctx->lazy_count >= ctx->lazy_threshold) {
        stream_flush(ctx);
    }
}

static const StreamEmitHandler kEmitHandlers[] = {
    [STREAM_EMIT_IMMEDIATE] = emit_mode_immediate,
    [STREAM_EMIT_BUFFERED]  = emit_mode_buffered,
    [STREAM_EMIT_THROTTLED] = emit_mode_throttled,
    [STREAM_EMIT_LAZY]      = emit_mode_lazy,
};

/* ==================== 事件发射 ==================== */

/**
 * @brief 发射流式事件
 *
 * 根据当前发射策略采取不同行为：
 * - IMMEDIATE: 立即分发到所有过滤匹配的回调，并更新统计
 * - BUFFERED: 事件入队缓冲区，等待 stream_flush()
 * - THROTTLED: 事件入队，间隔到期时自动刷新缓冲区
 *
 * 无论哪种模式，都会更新事件统计计数器。
 *
 * @param ctx   流式上下文
 * @param event 事件数据指针
 */
void stream_emit(StreamContext *ctx, const StreamEvent *event) {
    if (!ctx || !event)
        return;

    /* 更新事件统计（无论哪种发射模式都计数） */
    stream_update_stats(ctx, event);

    /* 异步模式：加锁入队 + 条件变量通知消费者 */
    if (ctx->async_enabled && ctx->async_running) {
        lv_mutex_lock(&ctx->async_mutex);
        if (ctx->buffer_count < ctx->buffer_capacity) {
            int write_pos = (ctx->buffer_head + ctx->buffer_count) % ctx->buffer_capacity;
            ctx->buffer[write_pos] = *event;
            ctx->buffer_count++;
        } else {
            ctx->dropped_count++;
        }
        lv_cond_signal(&ctx->async_cond_not_empty);
        lv_mutex_unlock(&ctx->async_mutex);
        return;
    }

    /* 发射模式 → 处理函数表分发（未知模式不处理，与原无 default 行为一致） */
    if ((unsigned) ctx->emit_mode < sizeof(kEmitHandlers) / sizeof(kEmitHandlers[0]) &&
        kEmitHandlers[ctx->emit_mode]) {
        kEmitHandlers[ctx->emit_mode](ctx, event);
    }
}

/**
 * 发射简化流式事件。
 * 封装 StreamEvent 结构体的构造过程，用基本参数填充事件字段，
 * 一次性完成事件组装和发射。不需要的字段（如 node_id、constraint_id 等）
 * 自动设为 -1。
 * @param ctx         流式上下文
 * @param type        事件类型
 * @param description 事件描述字符串
 * @param step_number 当前步骤编号
 */
