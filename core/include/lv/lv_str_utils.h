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

/** @brief 字符串是否为空（NULL 或空串均视为空） */
bool lv_str_is_empty(const char *s);

/** @brief 字符串是否非空 */
bool lv_str_nonempty(const char *s);

/**
 * @brief 大小写不敏感比较两个字符串（NULL 安全）
 * @param a 字符串（可为 NULL）
 * @param b 字符串（可为 NULL）
 * @return 与 strcmp 同符号：相等返回 0，a<b 为负，a>b 为正；
 *         NULL 视为小于任何非 NULL 字符串，两者均 NULL 视为相等
 * @note 内部按平台映射到 _stricmp（Windows）/ strcasecmp（POSIX），
 *       调用方无需再关心平台差异
 */
int lv_str_icmp(const char *a, const char *b);

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

/* ===== 十六进制编码 ===== */

/**
 * @brief 将字节数组编码为小写十六进制字符串（等价于逐字节 "%02x" 循环）
 *
 * 每字节输出 2 个字符（小写、无空格、无分隔符），并写入结尾 '\0'。
 *
 * @param bytes 输入字节数组
 * @param n     字节数
 * @param out   输出缓冲区（调用方保证容量 >= 2*n + 1）
 */
void lv_str_hex_encode(const unsigned char *bytes, size_t n, char *out);

/* ===== 字符串转义 ===== */

/**
 * @brief 对字符串进行 XML 转义并追加到 lvStrBuf
 * @param sb  目标 lvStrBuf（追加模式）
 * @param str 要转义的源字符串
 * @param len 源字符串长度
 */
void lv_str_escape_xml(lvStrBuf *sb, const char *str, size_t len);

/* ===== JSON/HTML 转义（统一公共 API，snprintf 语义） ===== */

/**
 * @brief 对字符串执行 JSON 转义（snprintf 语义）
 *
 * 转义 "、\\、\n、\r、\t、\b、\f 以及其它控制字符（编码为 \uXXXX）。
 *
 * @param src     源字符串（可为 NULL，按空串处理）
 * @param src_len 源字符串长度（字节）
 * @param dst     目标缓冲区（可为 NULL，此时仅计算所需长度）
 * @param dst_cap 目标缓冲区容量（字节）
 * @return 转义后所需长度（不含终止符 NUL）；若 dst 非空且 dst_cap>0，
 *         则最多写入 dst_cap-1 字节并以 NUL 结尾（截断安全）
 */
size_t lv_str_json_escape(const char *src, size_t src_len, char *dst, size_t dst_cap);

/**
 * @brief 对字符串执行 JSON 转义并分配新缓冲区（两遍法封装）
 *
 * 内部先经 lv_str_json_escape 计算所需长度，再分配 need+1 字节写入完整转义
 * 结果（含结尾 NUL），避免各调用点重复"先算长度再 malloc"的手写两遍法。
 *
 * @param src     源字符串（可为 NULL，按空串处理，返回分配的空串）
 * @param src_len 源字符串长度（字节）
 * @param out_len 可选输出：转义后所需长度（不含 NUL）；分配失败时不写入
 * @return 堆分配的转义后字符串（含 NUL），调用者需用 lv_free 释放；
 *         分配失败返回 NULL
 */
char *lv_str_json_escape_alloc(const char *src, size_t src_len, size_t *out_len);

/**
 * @brief 对字符串执行 JSON 反转义（snprintf 语义）
 *
 * 解码 "、\\、\/、\b、\f、\n、\r、\t 及 \uXXXX（编码为 UTF-8）。
 * 未声明的转义保留转义字符本身（如 \z → z）。
 *
 * @param src     源字符串（可为 NULL，按空串处理）
 * @param src_len 源字符串长度（字节）
 * @param dst     目标缓冲区（可为 NULL，此时仅计算所需长度）
 * @param dst_cap 目标缓冲区容量（字节）
 * @return 解码后所需长度（不含终止符 NUL）；截断语义同 lv_str_json_escape
 */
size_t lv_str_json_unescape(const char *src, size_t src_len, char *dst, size_t dst_cap);

/**
 * @brief 对字符串执行 HTML 实体转义（snprintf 语义）
 *
 * 转义 & → &amp;、< → &lt;、> → &gt;、" → &quot;、' → &#39;。
 *
 * @param src     源字符串（可为 NULL，按空串处理）
 * @param src_len 源字符串长度（字节）
 * @param dst     目标缓冲区（可为 NULL，此时仅计算所需长度）
 * @param dst_cap 目标缓冲区容量（字节）
 * @return 转义后所需长度（不含终止符 NUL）；截断语义同 lv_str_json_escape
 */
size_t lv_str_html_escape(const char *src, size_t src_len, char *dst, size_t dst_cap);

/**
 * @brief 对字符串执行 LaTeX 特殊字符转义（snprintf 语义）
 *
 * 转义 \\ → \textbackslash{}、{ → \{、} → \}、_ → \_、& → \&、# → \#、
 * $ → \$、% → \% 、^ → \^{}、~ → \~{}；
 * 控制字符与 ASCII 打印字符原样输出。
 *
 * @param src     源字符串（可为 NULL，按空串处理）
 * @param src_len 源字符串长度（字节）
 * @param dst     目标缓冲区（可为 NULL，此时仅计算所需长度）
 * @param dst_cap 目标缓冲区容量（字节）
 * @return 转义后所需长度（不含终止符 NUL）；截断语义同 lv_str_json_escape
 */
size_t lv_str_latex_escape(const char *src, size_t src_len, char *dst, size_t dst_cap);

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
