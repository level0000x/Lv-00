/**
 * @file stream_context.c
 * @brief 流式输出系统 —— 生命周期与回调管理
 */

#include "stream_internal.h"


/* ==================== 生命周期 ==================== */

/**
 * @brief 创建流式上下文
 *
 * 分配并初始化 StreamContext，预分配 STREAM_INITIAL_CALLBACKS 容量的回调数组。
 * @return 新上下文指针，内存不足返回 NULL
 */
StreamContext *stream_context_create(void) {
    StreamContext *ctx = (StreamContext *) lv_calloc(1, sizeof(StreamContext));
    if (!ctx)
        return NULL;
    memset(ctx, 0, sizeof(StreamContext));

    /* 预分配初始容量的回调数组 */
    ctx->callbacks = (CallbackEntry *) lv_calloc(1, sizeof(CallbackEntry) * STREAM_INITIAL_CALLBACKS);
    if (!ctx->callbacks) {
        lv_free((void **) &ctx);
        return NULL;
    }
    ctx->callback_capacity = STREAM_INITIAL_CALLBACKS;

    /* 初始化发射策略：默认立即发射模式 */
    ctx->emit_mode = STREAM_EMIT_IMMEDIATE;
    ctx->throttle_ms = STREAM_DEFAULT_THROTTLE;
    ctx->buffer = NULL;
    ctx->buffer_count = 0;
    ctx->buffer_capacity = 0;
    ctx->buffer_head = 0;
    ctx->last_emit_ms = 0;

    /* 初始化回调 ID 计数器 */
    ctx->next_callback_id = 1;

    /* 初始化事件统计 */
    memset(ctx->event_counts, 0, sizeof(ctx->event_counts));
    ctx->total_count = 0;
    ctx->dropped_count = 0;

    /* 初始化惰性队列 */
    ctx->lazy_queue = NULL;
    ctx->lazy_count = 0;
    ctx->lazy_capacity = 0;
    ctx->lazy_head = 0;
    ctx->lazy_threshold = 0;

    /* 初始化异步模式字段 */
    ctx->async_enabled = false;
    ctx->async_running = false;
    memset(&ctx->async_thread, 0, sizeof(ctx->async_thread));
    memset(&ctx->async_mutex, 0, sizeof(ctx->async_mutex));
    memset(&ctx->async_cond_not_empty, 0, sizeof(ctx->async_cond_not_empty));
    memset(&ctx->async_cond_flushed, 0, sizeof(ctx->async_cond_flushed));
    ctx->async_flush_waiters = 0;

    return ctx;
}

/**
 * @brief 销毁流式上下文
 *
 * 释放 StreamContext 及其所有资源（包括动态分配的回调数组和事件缓冲区）。
 * 传入 NULL 安全返回。
 * @param ctx 流式上下文指针
 */
void stream_context_destroy(StreamContext *ctx) {
    if (!ctx)
        return;

    /* 如果异步模式已启用，先停止消费者线程 */
    if (ctx->async_enabled && ctx->async_running) {
        stream_set_async_mode(ctx, false, 0);
    }

    /* 清理异步同步原语（防御性清理） */
    lv_mutex_destroy(&ctx->async_mutex);
    lv_cond_destroy(&ctx->async_cond_not_empty);
    lv_cond_destroy(&ctx->async_cond_flushed);

    /* 释放事件缓冲区 */
    if (ctx->buffer) {
        lv_free((void **) &ctx->buffer);
    }
    /* 释放惰性队列 */
    if (ctx->lazy_queue) {
        lv_free((void **) &ctx->lazy_queue);
    }
    lv_free((void **) &ctx->callbacks);
    lv_free((void **) &ctx);
}

/**
 * @brief 确保回调数组有足够容量（动态扩容）
 *
 * @param ctx        流式上下文
 * @param min_capacity 所需的最小容量
 * @return true 容量足够或扩容成功，false 扩容失败或在硬上限
 */
static bool stream_ensure_capacity(StreamContext *ctx, int min_capacity) {
    if (min_capacity <= ctx->callback_capacity)
        return true;
    if (min_capacity > STREAM_MAX_CALLBACKS)
        return false; /* 超过硬上限 */

    /* 先计算目标容量：while 翻倍并钳制到硬上限 STREAM_MAX_CALLBACKS */
    int new_cap = ctx->callback_capacity;
    while (new_cap < min_capacity) {
        if (new_cap > STREAM_MAX_CALLBACKS / 2) {
            new_cap = STREAM_MAX_CALLBACKS;
            break;
        }
        new_cap *= 2;
    }
    if (new_cap > STREAM_MAX_CALLBACKS)
        new_cap = STREAM_MAX_CALLBACKS;

    int old_cap = ctx->callback_capacity;
    /* 统一扩容（min_growth 使 min_required = 钳制后的目标容量；
     * 注：倍增策略下分配容量可能略大于 STREAM_MAX_CALLBACKS，
     * 硬上限仍由上方 min_capacity 检查保证） */
    if (!lv_ensure_capacity((void **) &ctx->callbacks, old_cap,
                            &ctx->callback_capacity, sizeof(CallbackEntry),
                            new_cap - old_cap))
        return false;

    return true;
}

/* ==================== 回调管理 ==================== */

/**
 * @brief 注册流式事件回调（无过滤）
 *
 * 将回调函数添加到上下文的回调列表中，过滤掩码设为 STREAM_FILTER_ALL。
 * 最多支持 STREAM_MAX_CALLBACKS 个回调。
 * @param ctx       流式上下文
 * @param callback  回调函数指针
 * @param user_data 回调透传数据
 * @return true 注册成功，false 参数无效或回调已满
 */
bool stream_register_callback(StreamContext *ctx, StreamCallback callback, void *user_data) {
    if (!ctx || !callback)
        return false;

    /* 动态扩容确保足够容量 */
    if (!stream_ensure_capacity(ctx, ctx->callback_count + 1)) {
        return false; /* 超过硬上限或内存分配失败 */
    }

    ctx->callbacks[ctx->callback_count].callback = callback;
    ctx->callbacks[ctx->callback_count].user_data = user_data;
    ctx->callbacks[ctx->callback_count].id = ctx->next_callback_id++;
    ctx->callbacks[ctx->callback_count].filter_mask = STREAM_FILTER_ALL;
    ctx->callback_count++;

    return true;
}

/**
 * 注册流式事件回调（带事件类型过滤掩码）。
 * 仅当事件类型匹配 filter_mask 中的位时，回调才会被调用。
 * @param ctx          流式上下文
 * @param callback     回调函数指针
 * @param user_data    回调透传数据
 * @param filter_mask  事件类型位掩码（STREAM_FILTER_ALL 表示接收全部）
 * @return >=0 成功，返回回调 ID；<0 失败
 */
int stream_register_callback_ex(StreamContext *ctx, StreamCallback callback, void *user_data, uint64_t filter_mask) {
    if (!ctx || !callback)
        return -1;

    /* 动态扩容确保足够容量 */
    if (!stream_ensure_capacity(ctx, ctx->callback_count + 1)) {
        return -1; /* 超过硬上限或内存分配失败 */
    }

    int assigned_id = ctx->next_callback_id++;
    ctx->callbacks[ctx->callback_count].callback = callback;
    ctx->callbacks[ctx->callback_count].user_data = user_data;
    ctx->callbacks[ctx->callback_count].id = assigned_id;
    ctx->callbacks[ctx->callback_count].filter_mask = filter_mask;
    ctx->callback_count++;

    return assigned_id;
}

/**
 * 取消注册流式事件回调（按函数指针）。
 * 从回调列表中移除指定回调函数，后续回调前移一位以保持数组紧凑。
 * @param ctx      流式上下文
 * @param callback 要移除的回调函数指针
 * @return true 移除成功，false 未找到或参数无效
 */
bool stream_unregister_callback(StreamContext *ctx, StreamCallback callback) {
    if (!ctx || !callback)
        return false;

    for (int i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].callback == callback) {
            /* 将后续回调前移一位 */
            for (int j = i; j < ctx->callback_count - 1; j++) {
                ctx->callbacks[j] = ctx->callbacks[j + 1];
            }
            ctx->callback_count--;
            return true;
        }
    }
    return false;
}

/**
 * 通过回调 ID 注销流式事件回调。
 * 从回调列表中查找指定 ID 的回调并移除，后续回调前移以保持数组紧凑。
 * @param ctx          流式上下文
 * @param callback_id  注册时返回的回调 ID
 * @return true 成功，false 未找到或 ctx 为 NULL
 */
bool stream_unregister_callback_by_id(StreamContext *ctx, int callback_id) {
    if (!ctx || callback_id <= 0)
        return false;

    for (int i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].id == callback_id) {
            /* 将后续回调前移一位 */
            for (int j = i; j < ctx->callback_count - 1; j++) {
                ctx->callbacks[j] = ctx->callbacks[j + 1];
            }
            ctx->callback_count--;
            return true;
        }
    }
    return false;
}

/**
 * 更新回调的事件过滤掩码。
 * 通过回调 ID 查找回调并更新其 filter_mask 字段。
 * @param ctx          流式上下文
 * @param callback_id  注册时返回的回调 ID
 * @param filter_mask  新的事件类型位掩码
 * @return true 成功，false 未找到对应回调
 */
bool stream_set_callback_filter(StreamContext *ctx, int callback_id, uint64_t filter_mask) {
    if (!ctx || callback_id <= 0)
        return false;

    for (int i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].id == callback_id) {
            ctx->callbacks[i].filter_mask = filter_mask;
            return true;
        }
    }
    return false;
}

/**
 * 获取回调的事件过滤掩码。
 * @param ctx          流式上下文
 * @param callback_id  注册时返回的回调 ID
 * @return 过滤掩码，未找到时返回 STREAM_FILTER_NONE
 */
uint64_t stream_get_callback_filter(StreamContext *ctx, int callback_id) {
    if (!ctx || callback_id <= 0)
        return STREAM_FILTER_NONE;

    for (int i = 0; i < ctx->callback_count; i++) {
        if (ctx->callbacks[i].id == callback_id) {
            return ctx->callbacks[i].filter_mask;
        }
    }
    return STREAM_FILTER_NONE;
}

