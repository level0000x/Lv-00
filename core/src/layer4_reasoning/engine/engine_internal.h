/**
 * @file engine_internal.h
 * @brief 引擎内部共享声明（engine.c 拆分子模块共用）
 */

#ifndef lv_ENGINE_INTERNAL_H
#define lv_ENGINE_INTERNAL_H

#include "lv/engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 设置引擎错误状态（统一接口）
 *
 * 将错误信息存储在引擎实例中。
 * 使用可变参数支持格式化错误消息。
 *
 * @param engine 引擎实例
 * @param status 错误状态码
 * @param fmt 格式化字符串（printf 风格）
 * @param ... 可变参数
 */
void engine_set_error(lvEngine *engine, EngineStatus status, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* lv_ENGINE_INTERNAL_H */
