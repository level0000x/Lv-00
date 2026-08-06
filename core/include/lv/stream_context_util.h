#ifndef lv_STREAM_CONTEXT_UTIL_H
#define lv_STREAM_CONTEXT_UTIL_H

#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*StreamContextSetter)(StreamContext *ctx);

void stream_context_register_setter(StreamContextSetter setter);

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
void stream_emit_fmt(StreamContext *ctx, StreamEventType type, int step_number, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
