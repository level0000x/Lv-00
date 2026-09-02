#ifndef lv_STATUS_CODES_H
#define lv_STATUS_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/lv.h"

lv_PUBLIC_API int lv_status_is_success(int code);
lv_PUBLIC_API int lv_status_is_error(int code);
lv_PUBLIC_API const char *lv_status_message(int code);

/**
 * @brief 获取状态码所属类别名称（补声明，C-㊺续37：实现于 status_codes.c
 *        但头未声明，属未声明公共函数）
 * @param code 状态码
 * @return 类别名称字符串（中文，静态存储，无需释放）
 */
lv_PUBLIC_API const char *lv_status_category(int code);

#ifdef __cplusplus
}
#endif

#endif /* lv_STATUS_CODES_H */
