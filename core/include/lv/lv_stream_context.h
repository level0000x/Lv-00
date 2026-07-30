/**
 * @file lv_stream_context.h
 * @brief 流式输出上下文独立模块 —— 管理实时事件输出
 *
 * @details 从 lvContext God Object 中提取的流式输出子系统。
 *          提供流式上下文的惰性创建、获取、启用/禁用等管理功能。
 *
 *          流式上下文（StreamContext）用于向前端/日志系统发射实时事件，
 *          支撑 Web 前端实时可视化和证明步骤动画渲染。
 *
 *          本模块是 lvContext 中 stream_ctx 管理的独立版本，
 *          不直接依赖 lvContext 的实现细节。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 * @date   2026-07-31
 */
#ifndef lv_LV_STREAM_CONTEXT_H
#define lv_LV_STREAM_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* lv_PUBLIC_API —— 若未定义则提供默认实现 */
#ifndef lv_PUBLIC_API
#if defined(_WIN32) || defined(_MSC_VER)
#ifdef lv_BUILD_SHARED
#define lv_PUBLIC_API __declspec(dllexport)
#else
#define lv_PUBLIC_API
#endif
#elif defined(__GNUC__) || defined(__clang__)
#ifdef lv_BUILD_SHARED
#define lv_PUBLIC_API __attribute__((visibility("default")))
#else
#define lv_PUBLIC_API
#endif
#else
#define lv_PUBLIC_API
#endif
#endif

/* 前向声明 —— StreamContext 在 stream.h 中定义 */
struct StreamContext;
struct lvContext;

/* ============================================================
 * 流式输出 API
 *
 * 管理 lvContext 关联的流式输出上下文（StreamContext）。
 * 流式上下文用于实时事件发射，支持惰性创建。
 * ============================================================ */

/**
 * @brief 获取上下文的流式输出上下文
 *
 * 如果上下文没有关联的 StreamContext，自动创建一个。
 * 后续调用将返回同一个 StreamContext 实例。
 *
 * @param ctx 上下文指针（可为 NULL）
 * @return 流式上下文指针，ctx 为 NULL 时返回 NULL
 */
lv_PUBLIC_API struct StreamContext *lv_context_get_stream(struct lvContext *ctx);

/**
 * @brief 设置流式输出的启用状态
 *
 * 启用时如果 stream_ctx 为 NULL 则自动创建。
 * 禁用时销毁已有的 stream_ctx 并置 NULL。
 *
 * @param ctx     上下文指针（非 NULL）
 * @param enabled true 启用，false 禁用
 */
lv_PUBLIC_API void lv_context_set_streaming_enabled(struct lvContext *ctx, bool enabled);

/**
 * @brief 检查流式输出是否启用
 *
 * 通过 stream_ctx 是否为 NULL 判断。
 *
 * @param ctx 上下文指针（可为 NULL）
 * @return true 启用
 */
lv_PUBLIC_API bool lv_context_is_streaming_enabled(const struct lvContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* lv_LV_STREAM_CONTEXT_H */
