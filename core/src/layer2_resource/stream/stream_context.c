/**
 * @file stream_context.c
 * @brief 流式输出系统 —— 生命周期与回调管理
 */

#include "stream_internal.h"


/* ==================== 生命周期 ==================== */

/**
 * @brief 创建流式上下文
 *
 * 分配并初始化 StreamContext，回调列表预分配 STREAM_INITIAL_CALLBACKS 容量。
 * @return 新上下文指针，内存不足返回 NULL
 */
StreamContext *stream_context_create(void) {
    StreamContext *ctx = (StreamContext *) lv_calloc(1, sizeof(StreamContext));
    if (!ctx)
        return NULL;
    memset(ctx, 0, sizeof(StreamContext));

    /* 初始化回调列表（公共设施）：初始容量 STREAM_INITIAL_CALLBACKS，
     * 硬上限 STREAM_MAX_CALLBACKS（超过后注册失败） */
    lv_callback_list_init(&ctx->callback_list, STREAM_INITIAL_CALLBACKS, STREAM_MAX_CALLBACKS);

    /* 初始化发射策略：默认立即发射模式 */
    ctx->emit_mode = STREAM_EMIT_IMMEDIATE;
    ctx->throttle_ms = STREAM_DEFAULT_THROTTLE;
    ctx->buffer = NULL;
    ctx->buffer_count = 0;
    ctx->buffer_capacity = 0;
    ctx->buffer_head = 0;
    ctx->last_emit_ms = 0;

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
 * 释放 StreamContext 及其所有资源（包括回调列表和事件缓冲区）。
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
    lv_callback_list_cleanup(&ctx->callback_list);
    lv_free((void **) &ctx);
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

    return lv_callback_list_add(&ctx->callback_list, (lvCallbackFn) callback, user_data,
                                STREAM_FILTER_ALL) >= 0;
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

    return lv_callback_list_add(&ctx->callback_list, (lvCallbackFn) callback, user_data, filter_mask);
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

    return lv_callback_list_remove_by_fn(&ctx->callback_list, (lvCallbackFn) callback);
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

    return lv_callback_list_remove_by_id(&ctx->callback_list, callback_id);
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

    return lv_callback_list_set_filter(&ctx->callback_list, callback_id, filter_mask);
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

    return lv_callback_list_get_filter(&ctx->callback_list, callback_id);
}

