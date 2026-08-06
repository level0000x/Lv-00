/**
 * @file lv_error.h
 * @brief Lv-00 错误处理增强抽象层 — 错误上下文传播与链式追踪
 *
 * @details 在 error_codes.h 的基础错误码之上，提供：
 *          - 错误上下文传播（记录错误发生的文件/函数/行号）
 *          - 错误原因链（原始错误 -> 包装错误 -> 上层错误）
 *          - 结构化错误信息（格式化消息 + 错误帧栈）
 *
 *          线程局部存储自动管理当前线程的错误上下文，
 *          支持多层错误帧形成错误链，便于调试和日志。
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */
#ifndef lv_ERROR_H
#define lv_ERROR_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "error_codes.h"

/* 确保 lv_FORMAT_PRINTF（权威定义位于 cross_platform.h）与 lv_PUBLIC_API 可用 */
#include "cross_platform.h"
#ifndef lv_PUBLIC_API
  #define lv_PUBLIC_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 最大错误消息长度 */
#define lv_ERROR_MSG_MAX 256

/** 错误帧栈最大容量 */
#define lv_ERROR_MAX_FRAMES 8

/** 临时缓冲区大小 */
#define lv_ERROR_SCRATCH_SIZE 512

/* ============================================================
 * 类型定义
 * ============================================================ */

/**
 * @brief 错误上下文帧（记录错误发生的位置与原因）
 *
 * 每个错误帧记录一次错误发生的具体位置、错误码和消息，
 * 并通过 cause 指针链接到原始错误，形成错误链。
 */
typedef struct lvErrorFrame {
    const char *file;              /**< 源文件名 */
    const char *func;              /**< 函数名 */
    int line;                      /**< 行号 */
    int code;                      /**< 错误码 (来自 error_codes.h) */
    char message[lv_ERROR_MSG_MAX]; /**< 错误消息 */
    struct lvErrorFrame *cause;    /**< 原始错误（错误链） */
} lvErrorFrame;

/**
 * @brief 错误上下文（线程局部，自动管理错误链）
 *
 * 维护一个错误帧栈，支持多级错误帧形成错误链。
 * 通过 lv_error_context_current() 延迟初始化。
 */
typedef struct lvErrorContext {
    lvErrorFrame frames[lv_ERROR_MAX_FRAMES]; /**< 错误帧栈 */
    int frame_count;                          /**< 当前帧数 */
    int frame_capacity;                       /**< 帧栈容量 */
    char scratch[lv_ERROR_SCRATCH_SIZE];      /**< 临时缓冲区 */
} lvErrorContext;

/* ============================================================
 * API 函数
 * ============================================================ */

/**
 * @brief 获取当前线程的错误上下文（延迟初始化）
 *
 * 首次调用时自动初始化线程局部的错误上下文。
 * 后续调用返回同一实例。
 *
 * @return 当前线程的 lvErrorContext 指针
 */
lv_PUBLIC_API lvErrorContext *lv_error_context_current(void);

/**
 * @brief 手动初始化错误上下文（用于非标准场景）
 *
 * 通常不需要手动调用，使用 lv_error_context_current() 即可。
 * 适用于需要在特定作用域隔离错误上下文的场景。
 *
 * @param ctx 错误上下文指针
 */
lv_PUBLIC_API void lv_error_context_init(lvErrorContext *ctx);

/**
 * @brief 清理错误上下文
 *
 * 释放错误上下文持有的资源。
 * 对于使用 lv_error_context_current() 获取的上下文，无需手动清理。
 *
 * @param ctx 错误上下文指针
 */
lv_PUBLIC_API void lv_error_context_cleanup(lvErrorContext *ctx);

/**
 * @brief 设置错误
 *
 * 在当前错误上下文中记录一个新的错误帧。
 * 返回 false 以便在 return 语句中使用。
 *
 * @param ctx    错误上下文
 * @param code   错误码
 * @param format 格式化字符串（类似 printf）
 * @param ...    可变参数
 * @return false（方便在 return 语句中链式使用）
 */
lv_PUBLIC_API bool lv_error_set(lvErrorContext *ctx, int code, const char *format, ...)
    lv_FORMAT_PRINTF(3, 4);

/**
 * @brief 设置错误并携带原因（错误链）
 *
 * 记录一个新的错误帧，并通过 cause 参数链接到原始错误。
 * 返回 false 以便在 return 语句中使用。
 *
 * @param ctx    错误上下文
 * @param code   错误码
 * @param cause  原始错误帧（可为 NULL）
 * @param format 格式化字符串（类似 printf）
 * @param ...    可变参数
 * @return false（方便在 return 语句中链式使用）
 */
lv_PUBLIC_API bool lv_error_set_with_cause(lvErrorContext *ctx, int code,
                                            lvErrorFrame *cause, const char *format, ...)
    lv_FORMAT_PRINTF(4, 5);

/**
 * @brief 带调用位置的错误设置（供 lv_ERROR_CTX_RETURN 系列宏使用）
 *
 * 宏内展开 __FILE__/__LINE__/__func__ 传入，使新式帧栈获得真实位置信息。
 *
 * @param ctx    错误上下文
 * @param code   错误码
 * @param file   源文件名（__FILE__）
 * @param line   行号（__LINE__）
 * @param func   函数名（__func__）
 * @param format 格式化字符串（类似 printf）
 * @param ...    可变参数
 * @return false（方便在 return 语句中链式使用）
 */
lv_PUBLIC_API bool lv_error_set_at(lvErrorContext *ctx, int code, const char *file, int line,
                                   const char *func, const char *format, ...)
    lv_FORMAT_PRINTF(6, 7);

/**
 * @brief 从旧式错误体系推入帧（桥接入口，供 error_codes.c 调用）
 *
 * 旧式 lv_RETURN_ERROR 宏系经 lv_set_error_ctx/lv_set_error 设置错误时，
 * 由 error_codes.c 调用本函数把同一错误推入新式 TLS 帧栈，使全部旧写端
 * 零改动获得 8 帧回溯、cause 链与根因优先输出能力。
 *
 * @param code    错误码
 * @param file    源文件名（可为 NULL）
 * @param line    行号（可为 0）
 * @param func    函数名（可为 NULL）
 * @param message 已格式化的错误消息（可为 NULL）
 * @return true 成功，false 帧栈已满
 */
lv_PUBLIC_API bool lv_error_push(lvErrorCode code, const char *file, int line, const char *func,
                                 const char *message);

/**
 * @brief 获取当前错误码
 *
 * @param ctx 错误上下文
 * @return 当前错误码，无错误时返回 lv_OK
 */
lv_PUBLIC_API int lv_error_code(lvErrorContext *ctx);

/**
 * @brief 获取当前错误消息
 *
 * @param ctx 错误上下文
 * @return 错误消息字符串，无错误时返回空字符串
 */
lv_PUBLIC_API const char *lv_error_message(lvErrorContext *ctx);

/**
 * @brief 获取当前错误的原因（原始错误）
 *
 * @param ctx 错误上下文
 * @return 原始错误帧指针，无原因时返回 NULL
 */
lv_PUBLIC_API lvErrorFrame *lv_error_cause(lvErrorContext *ctx);

/**
 * @brief 清除错误
 *
 * 重置错误上下文，清除所有错误帧。
 *
 * @param ctx 错误上下文
 */
lv_PUBLIC_API void lv_error_clear(lvErrorContext *ctx);

/**
 * @brief 检查是否有错误
 *
 * @param ctx 错误上下文
 * @return 有错误返回 true，否则 false
 */
lv_PUBLIC_API bool lv_error_has_error(lvErrorContext *ctx);

/**
 * @brief 格式化完整错误信息（包含错误链）
 *
 * 遍历错误链，生成包含所有错误帧的完整错误信息。
 * 返回的字符串由调用者通过 lv_free 释放。
 *
 * @param ctx 错误上下文
 * @return 格式化的错误信息字符串，失败返回 NULL
 */
lv_PUBLIC_API char *lv_error_format_chain(lvErrorContext *ctx);

/**
 * @brief 获取当前错误帧的源文件名
 *
 * @param ctx 错误上下文
 * @return 源文件名，无错误时返回 NULL
 */
lv_PUBLIC_API const char *lv_error_file(lvErrorContext *ctx);

/**
 * @brief 获取当前错误帧的函数名
 *
 * @param ctx 错误上下文
 * @return 函数名，无错误时返回 NULL
 */
lv_PUBLIC_API const char *lv_error_func(lvErrorContext *ctx);

/**
 * @brief 获取当前错误帧的行号
 *
 * @param ctx 错误上下文
 * @return 行号，无错误时返回 0
 */
lv_PUBLIC_API int lv_error_line(lvErrorContext *ctx);

/* ============================================================
 * 便利宏
 *
 * 注意：这些宏与 error_codes.h 中的 lv_ERROR_RETURN 系列宏
 * 不同，它们接受 lvErrorContext * 参数，用于新的错误上下文系统。
 * ============================================================ */

/**
 * @brief 设置错误并返回 false
 *
 * 在函数中设置错误上下文并返回 false。
 *
 * @param ctx  错误上下文指针
 * @param code 错误码
 * @param ...  格式化字符串和参数
 */
#define lv_ERROR_CTX_RETURN(ctx, code, ...) \
    do { \
        lv_error_set_at(ctx, code, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        return false; \
    } while(0)

/**
 * @brief 设置错误并返回 NULL
 *
 * 在函数中设置错误上下文并返回 NULL。
 *
 * @param ctx  错误上下文指针
 * @param code 错误码
 * @param ...  格式化字符串和参数
 */
#define lv_ERROR_CTX_RETURN_NULL(ctx, code, ...) \
    do { \
        lv_error_set_at(ctx, code, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        return NULL; \
    } while(0)

/**
 * @brief 设置错误并返回 -1
 *
 * 在函数中设置错误上下文并返回 -1。
 *
 * @param ctx  错误上下文指针
 * @param code 错误码
 * @param ...  格式化字符串和参数
 */
#define lv_ERROR_CTX_RETURN_NEG1(ctx, code, ...) \
    do { \
        lv_error_set_at(ctx, code, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        return -1; \
    } while(0)

/**
 * @brief 包装下层错误（添加当前上下文信息）
 *
 * 将当前错误上下文中的最后一个错误帧作为 cause，
 * 设置新的错误并返回指定值。
 *
 * 用法: return lv_ERROR_CTX_WRAP(ctx, ret_val, "additional context");
 *
 * @param ctx     错误上下文指针
 * @param ret_val 返回值
 * @param ...     额外的上下文描述（格式化字符串和参数）
 */
#define lv_ERROR_CTX_WRAP(ctx, ret_val, ...) \
    do { \
        lvErrorFrame *_cause = lv_error_code(ctx) ? \
            (ctx->frame_count > 0 ? &ctx->frames[ctx->frame_count - 1] : NULL) : NULL; \
        if (_cause) { \
            lv_error_set_with_cause(ctx, _cause->code, _cause, __VA_ARGS__); \
        } \
        return ret_val; \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* lv_ERROR_H */