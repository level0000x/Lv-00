/**
 * @file lv_stream_context.c
 * @brief 流式输出上下文独立模块 —— 实现
 *
 * @details 从 lvContext God Object 中提取的流式输出子系统。
 *          管理 lvContext 中 stream_ctx 字段的惰性创建、启用/禁用。
 *
 *          本模块是 context.c 中"第十三部分：流式输出 API"的独立版本，
 *          不直接依赖 context.c 的其他部分。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date   2026-07-31
 */

#include "lv/lv_stream_context.h"

#include "lv/context.h"
#include "lv/stream.h"

/* ============================================================
 * 流式输出 API
 * ============================================================ */

/**
 * @brief 获取上下文的流式输出上下文
 *
 * 如果上下文没有关联的 StreamContext，自动创建一个。
 * 后续调用将返回同一个 StreamContext 实例。
 */
struct StreamContext *lv_context_get_stream(lvContext *ctx) {
    if (!ctx) {
        return NULL;
    }

    /* 惰性初始化：首次调用时自动创建 */
    if (!ctx->stream_ctx) {
        ctx->stream_ctx = stream_context_create();
    }

    return ctx->stream_ctx;
}

/**
 * @brief 设置流式输出的启用状态
 *
 * 启用时如果 stream_ctx 为 NULL 则自动创建。
 * 禁用时销毁已有的 stream_ctx 并置 NULL。
 */
void lv_context_set_streaming_enabled(lvContext *ctx, bool enabled) {
    if (!ctx) {
        return;
    }

    if (enabled) {
        /* 启用：如果尚未创建则创建 StreamContext */
        if (!ctx->stream_ctx) {
            ctx->stream_ctx = stream_context_create();
        }
    } else {
        /* 禁用：销毁已有的 StreamContext */
        if (ctx->stream_ctx) {
            stream_context_destroy(ctx->stream_ctx);
            ctx->stream_ctx = NULL;
        }
    }
}

/**
 * @brief 检查流式输出是否启用
 *
 * 通过 stream_ctx 是否为 NULL 判断。
 */
bool lv_context_is_streaming_enabled(const lvContext *ctx) {
    if (!ctx) {
        return false;
    }
    return (ctx->stream_ctx != NULL);
}
