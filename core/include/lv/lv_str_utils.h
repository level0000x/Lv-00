/**
 * @file lv_str_utils.h
 * @brief 统一字符串工具函数集
 *
 * 提供常见的字符串检查、分割、替换等操作。
 * 所有函数都通过 lvStrBuf 安全构建结果，避免固定缓冲区溢出。
 * 返回堆分配字符串的函数，调用者需用 lv_free 释放。
 */

#ifndef lv_STR_UTILS_H
#define lv_STR_UTILS_H

#include "lv_strbuf.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== 字符串检查 ===== */

/** @brief 检查字符串是否以 prefix 开头 */
bool lv_str_startswith(const char *str, const char *prefix);

/** @brief 检查字符串是否以 suffix 结尾 */
bool lv_str_endswith(const char *str, const char *suffix);

/** @brief 检查字符串是否包含 substr */
bool lv_str_contains(const char *str, const char *substr);

/* ===== 字符串裁剪 ===== */

/**
 * @brief 去除字符串两端的空白字符
 * @return 指向 str 中第一个非空白字符的指针（修改原字符串，在末尾写 '\0'）
 */
char *lv_str_trim(char *str);

/**
 * @brief 去除字符串左端空白
 * @return 指向 str 中第一个非空白字符的指针
 */
char *lv_str_ltrim(char *str);

/**
 * @brief 去除字符串右端空白
 * @return str 本身
 */
char *lv_str_rtrim(char *str);

/* ===== 字符串分割 ===== */

/** @brief 字符串分割结果 */
typedef struct {
    char **items;
    size_t count;
} lvStrSplitResult;

/**
 * @brief 按分隔符分割字符串
 * @param str   要分割的字符串（不会被修改）
 * @param delim 分隔符字符串
 * @return 分割结果（items 中每个元素均为堆分配），调用者需用 lv_str_split_free() 释放
 */
lvStrSplitResult lv_str_split(const char *str, const char *delim);

/** @brief 释放分割结果 */
void lv_str_split_free(lvStrSplitResult *result);

/* ===== 字符串替换 ===== */

/**
 * @brief 替换字符串中所有出现的 old_str 为 new_str
 * @param str     原始字符串
 * @param old_str 要替换的子串
 * @param new_str 替换为的新子串
 * @return 堆分配的新字符串，调用者需用 lv_free 释放
 */
char *lv_str_replace(const char *str, const char *old_str, const char *new_str);

/* ===== 字符串拼接 ===== */

/**
 * @brief 连接字符串数组
 * @param items  字符串数组
 * @param count  数组元素个数
 * @param separator 分隔符
 * @return 堆分配的新字符串，调用者需用 lv_free 释放
 */
char *lv_str_join(const char **items, size_t count, const char *separator);

/* ===== 字符串转义 ===== */

/**
 * @brief 对字符串进行 JSON 转义并追加到 lvStrBuf
 * @param sb  目标 lvStrBuf（追加模式）
 * @param str 要转义的源字符串
 * @param len 源字符串长度
 */
void lv_str_escape_json(lvStrBuf *sb, const char *str, size_t len);

/**
 * @brief 对字符串进行 XML 转义并追加到 lvStrBuf
 * @param sb  目标 lvStrBuf（追加模式）
 * @param str 要转义的源字符串
 * @param len 源字符串长度
 */
void lv_str_escape_xml(lvStrBuf *sb, const char *str, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* lv_STR_UTILS_H */
