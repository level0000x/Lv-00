#ifndef lv_CHECK_H
#define lv_CHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv/lv_log.h"
#include "lv/error_codes.h"

/**
 * @file lv_check.h
 * @brief 统一前置条件检查宏
 *
 * 简化常见的参数校验、状态检查、分配结果检查模式。
 * 所有宏在检查失败时自动记录日志并返回错误。
 *
 * 注意：这些宏使用 lv_RETURN_ERROR，其中已内置 return。
 * 不要在宏外再写 return。
 */

/**
 * @brief 检查指针非空
 * @param ptr  待检查的指针
 */
#define lv_CHECK_NOT_NULL(ptr)                                        \
    do {                                                               \
        if ((ptr) == NULL) {                                           \
            lv_ERROR("CHECK: %s 为空 [%s:%d]", #ptr, __FILE__, __LINE__); \
            lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, #ptr);              \
        }                                                              \
    } while (0)

/**
 * @brief 检查指针非空（void 函数用，不返回值）
 * @param ptr  待检查的指针
 */
#define lv_CHECK_NOT_NULL_VOID(ptr)                                   \
    do {                                                               \
        if ((ptr) == NULL) {                                           \
            lv_ERROR("CHECK: %s 为空 [%s:%d]", #ptr, __FILE__, __LINE__); \
            return;                                                    \
        }                                                              \
    } while (0)

/**
 * @brief 检查参数条件
 * @param cond  条件表达式（为真则通过）
 * @param code  错误码
 * @param fmt   错误描述格式字符串
 * @param ...   可变参数
 */
#define lv_CHECK_ARG(cond, code, fmt, ...)                            \
    do {                                                               \
        if (!(cond)) {                                                 \
            lv_ERROR("CHECK: " fmt " [%s:%d]", ##__VA_ARGS__, __FILE__, __LINE__); \
            lv_RETURN_ERROR(code, fmt);                                \
        }                                                              \
    } while (0)

/**
 * @brief 检查参数条件（void 函数用）
 * @param cond  条件表达式
 * @param code  错误码
 * @param fmt   错误描述格式字符串
 * @param ...   可变参数
 */
#define lv_CHECK_ARG_VOID(cond, code, fmt, ...)                       \
    do {                                                               \
        if (!(cond)) {                                                 \
            lv_ERROR("CHECK: " fmt " [%s:%d]", ##__VA_ARGS__, __FILE__, __LINE__); \
            return;                                                    \
        }                                                              \
    } while (0)

/**
 * @brief 检查状态条件
 * @param cond  条件表达式
 * @param code  错误码
 * @param fmt   错误描述格式字符串
 * @param ...   可变参数
 */
#define lv_CHECK_STATE(cond, code, fmt, ...)                          \
    do {                                                               \
        if (!(cond)) {                                                 \
            lv_ERROR("CHECK(state): " fmt " [%s:%d]", ##__VA_ARGS__, __FILE__, __LINE__); \
            lv_RETURN_ERROR(code, fmt);                                \
        }                                                              \
    } while (0)

/* lv_CHECK_ALLOC(ptr, ret) 已在 error_codes.h 中定义 */
/* lv_CLAMP(val, min, max) 已在 lv_utils.h 中定义（就地钳制，返回钳后值） */
/* lv_CHECK_RANGE(val, min, max) 3 参数版 与 lv_CHECK_ENUM(val, max) 见本文件下方定义；
 * lv_CLAMP(val, min, max) 已在 lv_utils.h 中定义 */

/**
 * @brief 检查边界
 * @param idx  索引值
 * @param max  上限（不包含）
 * @param fmt  错误描述
 */
#define lv_CHECK_BOUNDS(idx, max, fmt)                                 \
    do {                                                               \
        if ((idx) < 0 || (idx) >= (int)(max)) {                        \
            lv_ERROR("CHECK: 索引越界 %d/%zu (%s) [%s:%d]",           \
                     (int)(idx), (size_t)(max), fmt, __FILE__, __LINE__); \
            lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, fmt);              \
        }                                                              \
    } while (0)

/**
 * @brief 检查边界（返回 NULL）
 * @param idx  索引值
 * @param max  上限（不包含）
 * @param fmt  错误描述
 */
#define lv_CHECK_BOUNDS_NULL(idx, max, fmt)                            \
    do {                                                               \
        if ((idx) < 0 || (idx) >= (int)(max)) {                        \
            lv_ERROR("CHECK: 索引越界 %d/%zu (%s) [%s:%d]",           \
                     (int)(idx), (size_t)(max), fmt, __FILE__, __LINE__); \
            return NULL;                                                \
        }                                                              \
    } while (0)

/**
 * @brief 检查值范围（含边界）
 * @param v    待检查的值
 * @param min  下界（包含）
 * @param max  上界（包含）
 *
 * 越界时返回 -1（lv_ERROR_INVALID_PARAM）。
 * 若需自定义返回值，请使用 error_codes.h 中的 4 参数版 lv_CHECK_RANGE(val, min, max, ret)。
 * 注意：error_codes.h 亦定义同名 4 参数版宏，此处 #undef 后统一为 3 参数版。
 */
#ifdef lv_CHECK_RANGE
#undef lv_CHECK_RANGE
#endif
#define lv_CHECK_RANGE(v, min, max)                                       \
    do {                                                                  \
        if ((v) < (min) || (v) > (max)) {                                 \
            lv_ERROR("CHECK: %s 越界，有效范围 [%s, %s] [%s:%d]", #v, #min, #max, __FILE__, __LINE__); \
            lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, #v);                  \
        }                                                                 \
    } while (0)

/**
 * @brief 检查枚举值边界（枚举数组下标防护）
 * @param v    枚举值
 * @param max  枚举上界（不包含，通常为 lv_ARRAY_SIZE(...)）
 *
 * 等价于手写 (unsigned)(v) >= (unsigned)(max) 检查。
 * 失败时返回 lv_ERROR_INVALID_PARAM（枚举值），适配 lvError 返回型函数。
 */
#define lv_CHECK_ENUM(v, max)                                             \
    do {                                                                  \
        if ((unsigned)(v) >= (unsigned)(max)) {                           \
            lv_ERROR("CHECK: 枚举 %s 越界 %u/%u [%s:%d]", #v,             \
                     (unsigned)(v), (unsigned)(max), __FILE__, __LINE__); \
            lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM, "枚举值越界: %s", #v); \
        }                                                                 \
    } while (0)

/**
 * @brief 传播错误：调用函数，检查返回值，非零时自动返回
 * @param call  函数调用表达式（返回 int）
 * @param code  错误码
 * @param fmt   错误描述
 * @param ...   可变参数
 *
 * 示例：
 * @code
 *   lv_PROPAGATE(lv_session_run(session), lv_ERROR_INTERNAL, "run failed");
 * @endcode
 */
#define lv_PROPAGATE(call, code, fmt, ...)                            \
    do {                                                               \
        int _lv_r = (call);                                           \
        if (_lv_r != 0) {                                              \
            lv_ERROR("PROPAGATE: %s -> %d [%s:%d]", #call, _lv_r, __FILE__, __LINE__); \
            lv_RETURN_ERROR(code, fmt, ##__VA_ARGS__);                 \
        }                                                              \
    } while (0)

/**
 * @brief 传播错误（void 函数版本）
 * @param call  函数调用表达式
 */
#define lv_PROPAGATE_VOID(call, code, fmt, ...)                       \
    do {                                                               \
        int _lv_r = (call);                                           \
        if (_lv_r != 0) {                                              \
            lv_ERROR("PROPAGATE: %s -> %d [%s:%d]", #call, _lv_r, __FILE__, __LINE__); \
            return;                                                    \
        }                                                              \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* lv_CHECK_H */
