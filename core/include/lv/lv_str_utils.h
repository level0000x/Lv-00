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

/**
 * @brief 在关键字表中查找第一个 strstr 命中的索引
 * @param input    输入字符串
 * @param keywords NULL 结尾的关键字数组
 * @return 命中的索引；未命中返回 -1
 */
int lv_str_match_any(const char *input, const char *const *keywords);

/**
 * @brief 带边界校验的关键字匹配（命中后必须为分隔符结尾）
 * @param input    输入字符串
 * @param keywords NULL 结尾的关键字数组
 * @return 命中的索引；未命中返回 -1
 */
int lv_str_match_delimited(const char *input, const char *const *keywords);

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

/**
 * @brief strtok_r 的可移植封装（MSVC 下回退到 strtok_s）
 * @param str     要分割的字符串（首次调用传入，后续传 NULL）
 * @param delim   分隔符字符串
 * @param saveptr 保存分割位置的指针
 * @return 下一个 token，无更多 token 时返回 NULL
 */
char *lv_strtok_r(char *str, const char *delim, char **saveptr);

/* ===== 定界符扫描 ===== */

/**
 * @brief 从 p 处扫描，跳过一对匹配的定界符（含字符串字面量感知）
 * @param p       指向左定界符
 * @param open    左定界符字符
 * @param close   右定界符字符
 * @return 匹配的右定界符之后的位置；不平衡则返回 NULL
 */
const char *lv_str_skip_balanced(const char *p, char open, char close);

/**
 * @brief 校验字符串中 open/close 是否平衡
 */
bool lv_str_check_balanced(const char *p, char open, char close);

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

/* ===== 游标式缓冲追加 ===== */

/**
 * @brief 向游标式缓冲区追加带分隔符的项（首项自动省略分隔符）
 * @param dst  目标缓冲区
 * @param size 缓冲区容量
 * @param pos  当前写入位置（in/out）
 * @param sep  分隔符字符串（如 ", "）
 * @param item 要追加的项
 * @return 是否成功（空间不足返回 false）
 */
bool lv_str_append_sep(char *dst, size_t size, size_t *pos, const char *sep, const char *item);

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

/* ===== 报告表格辅助 ===== */

/**
 * @brief 向 lvStrBuf 追加一条分隔线（等号/短横线）
 * @param sb    目标 lvStrBuf（追加模式）
 * @param ch    分隔线字符（如 '=' 或 '-'）
 * @param count 分隔线字符数量
 */
void lv_strbuf_append_sep(lvStrBuf *sb, char ch, size_t count);

/**
 * @brief 向 lvStrBuf 追加一个按列宽左对齐的单元格
 * @param sb    目标 lvStrBuf（追加模式）
 * @param text  单元格文本（可为 NULL，按空串处理）
 * @param width 列宽（文本不足时以空格补齐；为 0 时不补齐）
 */
void lv_strbuf_append_cell(lvStrBuf *sb, const char *text, size_t width);

#ifdef __cplusplus
}
#endif

#endif /* lv_STR_UTILS_H */
