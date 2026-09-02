/**
 * @file preset_abstract_algebra.h
 * @brief 抽象代数预设函数块 - 头文件
 *
 * @details 提供抽象代数预设函数块的注册和查询接口。
 *          包含群论、环论、域论、模论和表示论等基础运算。
 *
 * @module AbstractAlgebra
 * @category PRESET_CATEGORY_ALGEBRA
 * @version 1.1.0
 */

#ifndef lv_PRESET_ABSTRACT_ALGEBRA_H
#define lv_PRESET_ABSTRACT_ALGEBRA_H

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ABSTRACT_ALGEBRA_PRESET_COUNT 40

lv_PUBLIC_API bool preset_abstract_algebra_register(void);
lv_PUBLIC_API int preset_abstract_algebra_count(void);
lv_PUBLIC_API bool preset_abstract_algebra_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* lv_PRESET_ABSTRACT_ALGEBRA_H */
