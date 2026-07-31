/**
 * @file error_messages_cn.c
 * @brief 中文错误消息映射
 *
 * 将 Lv-00 统一错误码映射为中文描述字符串。
 * 映射实现统一委托给 error_codes.c 的规范错误表
 * （lv_error_string / lv_error_category / lv_error_table_size），
 * 本文件不再维护重复的错误码 → 文本映射表。
 *
 * @version 2.0.0
 */

#include <stddef.h>

#include "lv/error_codes.h"
#include "lv/lv_internal.h"

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 获取错误码对应的中文描述
 * @param code 错误码
 * @return 中文描述字符串（静态存储，无需释放）
 */
const char *lv_error_message_cn(lvErrorCode code) {
    return lv_error_string(code);
}

/**
 * @brief 获取错误码所属的中文类别
 * @param code 错误码
 * @return 中文类别字符串
 */
const char *lv_error_category_cn(lvErrorCode code) {
    return lv_error_category(code);
}

/**
 * @brief 获取中文错误消息表的条目数量
 * @return 条目数量
 */
int lv_error_message_cn_count(void) {
    return lv_error_table_size();
}