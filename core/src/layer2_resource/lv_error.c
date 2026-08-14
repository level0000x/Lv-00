/**
 * @file lv_error.c
 * @brief Lv-00 错误处理增强抽象层实现
 *
 * @details 实现错误上下文传播和链式错误追踪。
 *          使用线程局部存储管理当前线程的错误上下文，
 *          支持多级错误帧形成错误链。
 *
 * @version 1.1.0
 * @author Lv-00 Team
 */

#include "lv/lv_error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lv/lv_utils.h"
#include "lv/lv_strbuf.h"
#include "lv/cross_platform.h"

/* ============================================================
 * 线程局部错误上下文
 * ============================================================ */

/** 线程局部错误上下文存储 */
static lv_THREAD_LOCAL lvErrorContext g_error_context;

/** 线程局部初始化标志 */
static lv_THREAD_LOCAL bool g_error_context_initialized = false;

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 确保错误上下文已初始化
 *
 * 如果上下文尚未初始化，设置默认值。
 */
static void ensure_context_initialized(lvErrorContext *ctx)
{
    if (!ctx->frame_capacity) {
        ctx->frame_capacity = lv_ERROR_MAX_FRAMES;
        ctx->frame_count = 0;
        ctx->scratch[0] = '\0';
    }
}

/**
 * @brief 将错误帧压入栈顶
 *
 * @param ctx  错误上下文
 * @param code 错误码
 * @return 新错误帧的指针，栈满时返回 NULL
 */
static lvErrorFrame *push_frame(lvErrorContext *ctx, int code)
{
    if (ctx->frame_count >= ctx->frame_capacity) {
        /* 栈满：丢弃最旧的帧，为新帧腾出空间 */
        /* 将所有帧向前移动一位 */
        if (ctx->frame_count > 1) {
            /* 更新 cause 指针：每个帧的 cause 指向其前一个帧 */
            /* 先移动帧数据，再修复 cause 指针 */
            for (int i = 1; i < ctx->frame_count; i++) {
                ctx->frames[i - 1] = ctx->frames[i];
            }
            ctx->frame_count--;
            /* 修复 cause 指针：每个帧的 cause 指向其前一个帧 */
            for (int i = 1; i < ctx->frame_count; i++) {
                ctx->frames[i].cause = &ctx->frames[i - 1];
            }
            ctx->frames[0].cause = NULL;
        } else {
            ctx->frame_count = 0;
        }
    }

    lvErrorFrame *frame = &ctx->frames[ctx->frame_count];
    ctx->frame_count++;

    frame->file = NULL;
    frame->func = NULL;
    frame->line = 0;
    frame->code = code;
    frame->message[0] = '\0';
    frame->cause = NULL;

    return frame;
}

/* ============================================================
 * API 实现
 * ============================================================ */

/* exempt: 惰性守卫豁免 —— 该守卫初始化"线程局部（TLS）错误上下文"：
 * g_error_context_initialized 为 lv_THREAD_LOCAL，每线程独立初始化各自的上下文
 * （lv_once 为进程级一次性守卫，无法按线程触发）；且 lv_error_context_cleanup
 * 将 frame_capacity 置 0 后可重新初始化（reinit 语义），与 lv_once 不可重置矛盾。
 * 故保留手写 TLS 惰性守卫，不迁移。 */
lvErrorContext *lv_error_context_current(void)
{
    if (!g_error_context_initialized) {
        lv_error_context_init(&g_error_context);
        g_error_context_initialized = true;
    }
    return &g_error_context;
}

void lv_error_context_init(lvErrorContext *ctx)
{
    if (!ctx) return;
    ctx->frame_capacity = lv_ERROR_MAX_FRAMES;
    ctx->frame_count = 0;
    ctx->scratch[0] = '\0';
    memset(ctx->frames, 0, sizeof(ctx->frames));
}

void lv_error_context_cleanup(lvErrorContext *ctx)
{
    if (!ctx) return;
    ctx->frame_capacity = 0;
    ctx->frame_count = 0;
    ctx->scratch[0] = '\0';
    memset(ctx->frames, 0, sizeof(ctx->frames));
}

bool lv_error_set(lvErrorContext *ctx, int code, const char *format, ...)
{
    if (!ctx) return false;

    ensure_context_initialized(ctx);

    lvErrorFrame *frame = push_frame(ctx, code);
    if (!frame) return false;

    /* 记录调用位置 */
    frame->code = code;

    /* 格式化错误消息 */
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(frame->message, lv_ERROR_MSG_MAX, format, args);
        va_end(args);
    }

    return false;
}

bool lv_error_set_with_cause(lvErrorContext *ctx, int code,
                              lvErrorFrame *cause, const char *format, ...)
{
    if (!ctx) return false;

    ensure_context_initialized(ctx);

    lvErrorFrame *frame = push_frame(ctx, code);
    if (!frame) return false;

    frame->code = code;
    frame->cause = cause;

    /* 格式化错误消息 */
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(frame->message, lv_ERROR_MSG_MAX, format, args);
        va_end(args);
    }

    return false;
}

/**
 * @brief 带调用位置的错误设置（修复 lv_error_set 系列不填 file/func/line 的缺陷）
 *
 * 供 lv_error.h 的 lv_ERROR_CTX_RETURN 系列宏使用：宏内展开 __FILE__/__LINE__/__func__，
 * 使新式帧栈获得真实的位置信息（此前恒为 NULL/0，format_chain 输出 [?:0]）。
 */
bool lv_error_set_at(lvErrorContext *ctx, int code, const char *file, int line, const char *func,
                     const char *format, ...)
{
    if (!ctx) return false;

    ensure_context_initialized(ctx);

    lvErrorFrame *frame = push_frame(ctx, code);
    if (!frame) return false;

    frame->file = file;
    frame->func = func;
    frame->line = line;
    frame->code = code;

    /* 格式化错误消息 */
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(frame->message, lv_ERROR_MSG_MAX, format, args);
        va_end(args);
    }

    return false;
}

/**
 * @brief 从旧式错误体系推入帧（桥接入口）
 *
 * 旧式 lv_RETURN_ERROR 宏系经 lv_set_error_ctx/lv_set_error 设置错误时，
 * 由 error_codes.c 调用本函数把同一错误（含位置）推入新式 TLS 帧栈，
 * 使全部旧写端零改动获得 8 帧回溯、cause 链与根因优先输出能力。
 * 两个错误体系从此连通为单一错误模块。
 *
 * @param code    错误码（与旧式 g_last_error_code 同值）
 * @param file    源文件名（可为 NULL）
 * @param line    行号（可为 0）
 * @param func    函数名（可为 NULL）
 * @param message 已格式化的错误消息（可为 NULL）
 * @return true 成功，false 帧栈已满
 */
bool lv_error_push(lvErrorCode code, const char *file, int line, const char *func,
                   const char *message)
{
    lvErrorContext *ctx = lv_error_context_current();
    lvErrorFrame *frame = push_frame(ctx, code);
    if (!frame) return false;

    frame->file = file;
    frame->func = func;
    frame->line = line;
    frame->code = code;
    if (message)
        lv_strlcpy(frame->message, message, sizeof(frame->message));
    return true;
}

int lv_error_code(lvErrorContext *ctx)
{
    if (!ctx || ctx->frame_count <= 0) return lv_OK;
    return ctx->frames[ctx->frame_count - 1].code;
}

const char *lv_error_message(lvErrorContext *ctx)
{
    if (!ctx || ctx->frame_count <= 0) return "";
    return ctx->frames[ctx->frame_count - 1].message;
}

lvErrorFrame *lv_error_cause(lvErrorContext *ctx)
{
    if (!ctx || ctx->frame_count <= 0) return NULL;
    return ctx->frames[ctx->frame_count - 1].cause;
}

void lv_error_clear(lvErrorContext *ctx)
{
    if (!ctx) return;
    ctx->frame_count = 0;
    ctx->scratch[0] = '\0';
}

bool lv_error_has_error(lvErrorContext *ctx)
{
    if (!ctx) return false;
    return ctx->frame_count > 0;
}

char *lv_error_format_chain(lvErrorContext *ctx)
{
    if (!ctx || ctx->frame_count <= 0) return NULL;

    lvStrBuf sb = {0};

    /* 从最旧的帧开始遍历（根因优先） */
    for (int i = 0; i < ctx->frame_count; i++) {
        lvErrorFrame *frame = &ctx->frames[i];
        const char *file = frame->file ? frame->file : "?";
        const char *func = frame->func ? frame->func : "?";
        int line = frame->line;

        /* 补全错误码名称（接回旧式权威错误表），提升可读性 */
        const char *name = lv_error_name(frame->code);
        if (i == 0) {
            lv_strbuf_printf(&sb, "[%s:%d] %s: %s (code=%d, %s)",
                file, line, func, frame->message, frame->code, name ? name : "?");
        } else {
            lv_strbuf_printf(&sb, "\n  caused by -> [%s:%d] %s: %s (code=%d, %s)",
                file, line, func, frame->message, frame->code, name ? name : "?");
        }
    }

    return lv_strbuf_to_string(&sb);
}

const char *lv_error_file(lvErrorContext *ctx)
{
    if (!ctx || ctx->frame_count <= 0) return NULL;
    return ctx->frames[ctx->frame_count - 1].file;
}

const char *lv_error_func(lvErrorContext *ctx)
{
    if (!ctx || ctx->frame_count <= 0) return NULL;
    return ctx->frames[ctx->frame_count - 1].func;
}

int lv_error_line(lvErrorContext *ctx)
{
    if (!ctx || ctx->frame_count <= 0) return 0;
    return ctx->frames[ctx->frame_count - 1].line;
}