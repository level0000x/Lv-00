/**
 * @file engine_error.c
 * @brief 引擎错误管理（从 engine.c 拆分）
 *
 * @details 统一的错误处理系统 v3.4.1：
 *          - 引擎实例优先：所有错误信息优先存储在引擎实例中（engine->last_status/error）
 *          - 全局状态回退：仅在引擎实例不可用时使用线程局部状态
 *          - 自动清理：成功操作后自动清除错误状态，避免错误滞留
 *          - 线程安全：线程局部存储确保多线程环境下的错误隔离
 *
 * @author Lv-00 Project
 * @version 3.4.1
 */

#include "lv/engine.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/lv.h"

#include "engine_internal.h"

/** 错误缓冲区大小 */
#define lv_ENGINE_ERROR_SIZE 512

void engine_set_error(lvEngine *engine, EngineStatus status, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (engine) {
        engine->last_status = status;
        vsnprintf(engine->last_error, sizeof(engine->last_error), fmt, args);
    }

    va_end(args);
}

/**
 * @brief 获取引擎最近一次操作的状态码
 *
 * 每个引擎实例独立维护自己的错误状态。
 * 与 engine_get_state 等查询类 getter 约定一致：
 * 传入 NULL 时返回新引擎实例的初始状态。
 *
 * @param[in] engine 引擎实例（为 NULL 时返回 ENGINE_STATUS_OK）
 * @return 最近一次操作的状态码
 */
EngineStatus engine_get_last_status(const lvEngine *engine) {
    if (!engine) {
        return ENGINE_STATUS_OK;
    }
    return engine->last_status;
}

/**
 * @brief 获取引擎最近一次错误的描述字符串
 *
 * @param[in] engine 引擎实例（为 NULL 时返回空字符串）
 * @return 内部静态错误字符串指针。调用者不得 free。
 *         在下一次可能修改错误状态的操作前有效。
 *         如无错误，返回空字符串。
 */
const char *engine_get_last_error(const lvEngine *engine) {
    if (!engine) {
        return "";
    }
    return engine->last_error;
}
