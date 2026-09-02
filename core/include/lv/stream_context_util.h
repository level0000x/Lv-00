#ifndef lv_STREAM_CONTEXT_UTIL_H
#define lv_STREAM_CONTEXT_UTIL_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*StreamContextSetter)(StreamContext *ctx);

lv_PUBLIC_API void stream_context_register_setter(StreamContextSetter setter);

/* ═══════════════════════════════════════════════════════════════════
 * LV_STREAM_CTX_DECLARE / LV_STREAM_CTX_DEFINE —— 模块流上下文三件套宏
 *
 * 收敛各模块重复的样板：
 *   声明（模块 .h 中）：  extern lv_THREAD_LOCAL StreamContext *xxx_stream_ctx;
 *                         void xxx_set_stream_context(StreamContext *ctx);
 *   定义（模块 .c 中）：  lv_THREAD_LOCAL StreamContext *xxx_stream_ctx = NULL;
 *                         void xxx_set_stream_context(StreamContext *ctx) { xxx_stream_ctx = ctx; }
 *
 * 用法（以 unify 模块为例）：
 *   // unify.h
 *   LV_STREAM_CTX_DECLARE(unify);
 *   // unify.c
 *   LV_STREAM_CTX_DEFINE(unify);
 *
 * 注意：仅覆盖「变量名 = <module>_stream_ctx 且 setter 与变量同文件」的公共形态；
 * 命名不一致（如 axiom_pkg：变量 axiom_stream_ctx）或 setter 位于其他文件
 * （如 prop_verifier / solver_engine）的模块保留手写。
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief 声明宏（放模块公共头 .h）：extern TLS 变量 + setter 原型 */
#define LV_STREAM_CTX_DECLARE(module)                           \
    extern lv_THREAD_LOCAL StreamContext *module##_stream_ctx; \
    void module##_set_stream_context(StreamContext *ctx)

/** @brief 定义宏（放模块实现 .c）：TLS 变量定义 + setter 实现 */
#define LV_STREAM_CTX_DEFINE(module)                            \
    lv_THREAD_LOCAL StreamContext *module##_stream_ctx = NULL; \
    void module##_set_stream_context(StreamContext *ctx) {     \
        module##_stream_ctx = ctx;                             \
    }

/**
 * @brief 便捷函数: 格式化文本并发射简单流式事件
 *
 * 内部完成「lvStrBuf 组装 + stream_emit_simple + lvStrBuf 销毁」，
 * 调用方无需手动管理缓冲区。等价于：
 * @code
 *   if (ctx) {
 *       lvStrBuf sb = {0};
 *       lv_strbuf_printf(&sb, fmt, ...);
 *       stream_emit_simple(ctx, type, sb.data, step_number);
 *       lv_strbuf_destroy(&sb);
 *   }
 * @endcode
 *
 * @param ctx         流式上下文（可为 NULL，此时为空操作）
 * @param type        事件类型
 * @param step_number 步骤编号（各模块可将其视为阶段号 phase）
 * @param fmt         printf 风格格式串
 * @param ...         格式化参数
 */
lv_PUBLIC_API void stream_emit_fmt(StreamContext *ctx, StreamEventType type, int step_number, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
