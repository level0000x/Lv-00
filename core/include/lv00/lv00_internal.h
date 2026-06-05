/**
 * @file lv00_internal.h
 * @brief Lv-00 项目内部头文件 —— 内部工具宏与常量
 *
 * @details 提供项目内部使用的通用工具宏、常量和辅助函数声明，
 * 仅供 .c 源文件内部使用，不作为公共 API 暴露。
 *
 * 包含内容：
 * - LV00_ARRAY_GROWTH_FACTOR：动态数组增长因子
 * - LV00_SOLVER_SCALE_FACTOR：SAT 赋值到坐标的缩放因子
 * - LV00_CHECK_NULL / LV00_CHECK_NULL_VOID / LV00_CHECK_ALLOC：空指针和分配检查宏
 * - LV00_UNUSED：抑制未使用变量警告
 * - lv00_ensure_capacity：动态数组容量确保函数
 * - lv00_set_error_ctx：带上下文的错误设置函数
 * - LV00_ERROR_*：错误码（通过包含 error_codes.h 获取）
 *
 * @note 本头文件会自动包含 error_codes.h 和 lv00_utils.h，
 *       因此使用本头文件的 .c 文件无需再单独包含它们。
 *
 * @version 3.5.0
 */

#ifndef LV00_INTERNAL_H
#define LV00_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/*
 * 错误码定义和错误处理宏（LV00_CHECK_NULL, LV00_CHECK_NULL_VOID,
 * LV00_CHECK_ALLOC, lv00_set_error_ctx, LV00_ERROR_* 等）
 */
#include "error_codes.h"

/*
 * 工具函数（lv00_malloc, lv00_free, lv00_realloc, lv00_calloc,
 * lv00_strdup, lv00_ensure_capacity 等）
 */
#include "lv00_utils.h"

/*
 * LV00_UNUSED 宏（抑制未使用变量/参数的编译器警告）
 * 定义在 cross_platform.h 中
 */
#include "cross_platform.h"

#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ================================================================
 * 内部常量
 * ================================================================ */

/**
 * @brief 动态数组增长因子
 *
 * 当数组需要扩容时，新容量 = 当前容量 * LV00_ARRAY_GROWTH_FACTOR。
 * 值为 2（倍增策略），兼顾内存使用和性能。
 */
#ifndef LV00_ARRAY_GROWTH_FACTOR
#define LV00_ARRAY_GROWTH_FACTOR 2
#endif

/**
 * @brief SAT 赋值到坐标的缩放因子
 *
 * 将 SAT 求解器的布尔赋值映射为有理数坐标时的缩放系数。
 * 例如：赋值为真 -> +SCALE_FACTOR，赋值为假 -> -SCALE_FACTOR。
 */
#ifndef LV00_SOLVER_SCALE_FACTOR
#define LV00_SOLVER_SCALE_FACTOR 1000
#endif

#ifdef __cplusplus
}
#endif

#endif /* LV00_INTERNAL_H */
