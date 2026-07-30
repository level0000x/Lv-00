/**
 * @file singular_backend.h
 * @brief Singular 计算机代数后端 API
 *
 * 提供 Singular 多项式环和 Gröbner 基计算接口。
 * 需要 libsingular 库链接。
 */

#ifndef lv_SINGULAR_BACKEND_H
#define lv_SINGULAR_BACKEND_H

#include "lv/numerical_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 注册 Singular 后端操作表到后端注册表 */
int lv_singular_register_backend(void);

/** @brief 检查 Singular 库是否可用 */
int lv_singular_available(void);

/** @brief 获取 Singular 版本字符串 */
const char *lv_singular_backend_version(void);

#ifdef __cplusplus
}
#endif

#endif /* lv_SINGULAR_BACKEND_H */
