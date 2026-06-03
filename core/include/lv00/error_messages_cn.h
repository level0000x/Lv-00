/**
 * @file error_messages_cn.h
 * @brief Lv-00 中文错误信息系统
 *
 * @details 提供中文版本的错误信息、错误码名称和错误类别。
 *          用于提升中文用户的交互体验。
 *
 * 【使用说明】
 * - 如果需要使用中文错误信息，调用 lv00_error_string_cn() 而非 lv00_error_string()
 * - 如果需要使用中文错误码名称，调用 lv00_error_name_cn() 而非 lv00_error_name()
 * - 如果需要使用中文错误类别，调用 lv00_error_category_cn() 而非 lv00_error_category()
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */

#ifndef LV00_ERROR_MESSAGES_CN_H
#define LV00_ERROR_MESSAGES_CN_H

#include "error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取错误码对应的中文错误信息
 * @param code 错误码
 * @return 中文错误信息字符串（静态存储，无需释放）
 */
LV00_PUBLIC_API const char *lv00_error_string_cn(Lv00ErrorCode code);

/**
 * @brief 获取错误码的中文简称
 * @param code 错误码
 * @return 中文错误名称字符串（如 "成功"、"参数错误"）
 */
LV00_PUBLIC_API const char *lv00_error_name_cn(Lv00ErrorCode code);

/**
 * @brief 获取错误码所属的中文错误类别
 * @param code 错误码
 * @return 中文错误类别名称字符串（如 "系统错误"、"约束图错误"）
 */
LV00_PUBLIC_API const char *lv00_error_category_cn(Lv00ErrorCode code);

/**
 * @brief 获取完整的中文错误描述（包含错误类别、名称和详细信息）
 *
 * @param code 错误码
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 实际写入的字符数，失败返回-1
 */
LV00_PUBLIC_API int lv00_get_error_description_cn(Lv00ErrorCode code, char *buf, size_t buf_size);

/* ============================================================
 * 便捷宏：检查并格式化中文错误
 * ============================================================ */

/**
 * @brief 获取当前线程的最后错误的中文描述
 *
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 实际写入的字符数，失败返回-1
 */
LV00_PUBLIC_API int lv00_get_last_error_description_cn(char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* LV00_ERROR_MESSAGES_CN_H */
