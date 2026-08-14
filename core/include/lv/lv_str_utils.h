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
#include <gmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
 * @brief 大小写不敏感 + 长度限制的 ASCII 比较（lv_str_icmp 的定长变体）
 *
 * 【语义契约】逐字节比较 a 与 b 的前 n 个字节（ASCII 'A'..'Z' 折叠为 +32 的
 *             小写，其余字节原样比较），返回与 strncmp 同符号：前 n 字节内首个
 *             不同字节处 a<b 为负、a>b 为正；前 n 字节全部相等返回 0。遇到 NUL
 *             提前终止：NUL 视为小于任何非 NUL 字节（与 strncmp 一致），因此
 *             短于 n 的字符串收敛为"短者更小"，或双方恰好同位置 NUL 则视为相等。
 *             折叠语义与 ws_header_equals/ws_header_contains_token 既有手写
 *             折叠（'A'..'Z' → +32）完全一致，替换调用点后行为不变。
 * 【前置条件】a、b 指向的缓冲区长度均至少为 n 字节（n==0 时不访问缓冲区；
 *             恰好 n 字节场景由调用方保证缓冲区有效）。
 * 【NULL 与 n 边界】a==b（含均 NULL）返回 0；a 为 NULL 返回 -1、b 为 NULL
 *             返回 1（与 lv_str_icmp 的 NULL 处理一致）；n==0 返回 0。
 * 【失败语义】纯比较函数，无分配、无失败通道。
 * 【扩展点】如需 locale 感知折叠，可在此替换折叠实现；当前保持 ASCII 折叠
 *             与既有调用点语义一致。
 *
 * @param a 缓冲区（可为 NULL）
 * @param b 缓冲区（可为 NULL）
 * @param n 最多比较的字节数
 * @return 与 strncmp 同符号的比较结果
 */
int lv_str_icmp_n(const char *a, const char *b, size_t n);

/**
 * @brief 大小写敏感比较两个字符串是否相等（NULL 安全）
 * @param a 字符串（可为 NULL）
 * @param b 字符串（可为 NULL）
 * @return 两者内容相同（或均 NULL）返回 true，否则 false
 * @note 收敛对象（判据 B）：全库 strcmp(a,b)==0 / strcmp(a,b)!=0 的相等判定形态；
 *       NULL 语义与 lv_str_icmp 一致（两者均 NULL 视为相等）
 */
bool lv_str_eq(const char *a, const char *b);

/** @brief lv_str_eq 的取反（NULL 安全，两者均 NULL 视为相等） */
bool lv_str_ne(const char *a, const char *b);

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

/**
 * @brief 去除字符串末尾的换行/回车（'\n' 与 '\r'）
 *
 * 就地改写：把末尾连续的回车/换行替换为 '\0'。
 * 与 lv_str_rtrim 的区别：仅去除换行/回车，不去除空格/制表符等空白。
 *
 * @param str 输入字符串（就地修改）
 * @return str 本身
 */
char *lv_str_chomp(char *str);

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

/* ===== 流式文本解析原语 ===== */
const char *lv_str_skip_ws(const char *p);

/**
 * @brief 跳过空白字符的有界变体（NUL 结尾场景之外用于 `(p, end)` 有界解析器）
 *
 * 语义契约：从 p 起跳过 `' '`/`'\t'`/`'\n'`/`'\r'`（与 lv_str_skip_ws 同一
 *           空白集），但最多推进到 end（不含 end 处字符），返回首个非空白
 *           字符位置（或 end）。不修改字符串、不分配资源。
 * 前置条件：p、end 均非 NULL 且 p <= end；p > end 时按 p == end 处理（返回 p）。
 * 失败/截断语义：纯查询，无失败通道；p 或 end 为 NULL 时返回 p 原样。
 * 边界行为：p 已指向空白/end 时逐字符推进至 end 停止，不会越过 end。
 * 扩展点：无（「越过定界符」由调用方自行 ++；空白集变更只需同步 lv_str_skip_ws）。
 *
 * @note 收敛对象（判据 A）：全库「`while (pos < end && isspace(*pos)) pos++`」
 *       有界空白跳过循环。isspace 额外匹配的 `\v`/`\f` 在定理/证明/公式解析
 *       上下文中不出现，故收敛到项目规范 4 字符空白集后行为在实际输入上等价。
 */
const char *lv_str_skip_ws_n(const char *p, const char *end);

/**
 * @brief 从 p 向前推进，直到命中 any_of 中任一字符或 NUL（停在定界符处，不越过）
 *
 * 语义契约：返回 p 之后首个属于 any_of 字符集的字符位置（或 NUL 终止位置）；
 *           不越过定界符、不修改字符串、不分配资源。
 * 前置条件：p、any_of 非 NULL（任一为 NULL 时返回 p 原样）；any_of 为 NUL 终止定界符集。
 * 失败/截断语义：纯查询，无失败通道。
 * 边界行为：p 已指向 NUL 或定界符时返回 p 不动；空 any_of 推进到 NUL。
 * 扩展点：无（「越过定界符」由调用方自行 ++；空白跳过已有 lv_str_skip_ws）。
 *
 * @note 收敛对象（判据 A）：全库「while (*p && *p != X) p++」扫描到定界符的手写
 *       循环（单字符与多字符定界符两类），等价于 p + strcspn(p, any_of)。
 */
const char *lv_str_skip_until(const char *p, const char *any_of);

bool lv_str_read_int(const char **pp, int64_t *out);
bool lv_str_read_quoted(const char **pp, char **out);
const char *lv_str_read_token(const char **pp, char **out, const char *delims);

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

/**
 * @brief 向已有 lvStrBuf 追加式连接字符串数组（首项自动省略分隔符）
 *
 * 与 lv_str_join 共享内部骨架；适合元素逐个收集到数组后统一输出的场景，
 * 替代手写 `if (i > 0) printf(sep)` 骨架。
 *
 * @param sb        目标 lvStrBuf（须已初始化）
 * @param items     字符串数组
 * @param count     数组元素个数
 * @param separator 分隔符
 * @return 是否成功（sb 为空返回 false；count==0 视为空 join 返回 true）
 */
bool lv_strbuf_join(lvStrBuf *sb, const char *const *items, size_t count, const char *separator);

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
 * @brief 解析 JSON \uXXXX 转义为一个完整 Unicode 码点（含 UTF-16 代理对合并）
 *
 * src 指向 \u 之后的第一个十六进制字符。若码点为高代理（U+D800-U+DBFF）
 * 且紧随其后是 \uXXXX 形式的低代理（U+DC00-U+DFFF），合并为补充平面码点。
 *
 * @param src     源缓冲（\u 之后部分），可为 NULL
 * @param src_len src 剩余字节数
 * @param out_adv 输出实际消耗的字节数（4 位 = 4；含低代理合并 = 10；
 *                非法/不足 4 位时输出已解析位数 0-3）
 * @return 完整码点；孤立代理或非法十六进制返回 0xFFFFFFFF（调用方应写 U+FFFD）
 */
unsigned int lv_str_json_read_codepoint(const char *src, size_t src_len, size_t *out_adv);

/**
 * @brief 将 Unicode 码点编码为 UTF-8
 *
 * @param cp  码点（0x0-0x10FFFF）
 * @param dst 目标缓冲（至少 4 字节）
 * @param cap 目标容量（字节）
 * @return 写入字节数；cp 无效或容量不足返回 0
 */
size_t lv_str_codepoint_to_utf8(unsigned int cp, char *dst, size_t cap);

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
 * @brief 对字符串执行 HTML 实体转义并分配新缓冲区（两遍法封装）
 *
 * 内部先经 lv_str_html_escape 计算所需长度，再分配 need+1 字节写入完整
 * 转义结果（含结尾 NUL），避免各调用点重复"先算长度再 malloc"的手写两遍法。
 *
 * @param src 源字符串（可为 NULL，按空串处理，返回分配的空串）
 * @return 堆分配的转义后字符串（含 NUL），调用者需用 lv_free 释放；
 *         分配失败返回 NULL
 */
char *lv_str_html_escape_alloc(const char *src);

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

/**
 * @brief 对字符串执行 LaTeX 特殊字符转义并分配新缓冲区（两遍法封装）
 *
 * 内部先经 lv_str_latex_escape 计算所需长度，再分配 need+1 字节写入完整
 * 转义结果（含结尾 NUL），避免各调用点重复"先算长度再 malloc"的手写两遍法。
 *
 * @param src 源字符串（可为 NULL，按空串处理，返回分配的空串）
 * @return 堆分配的转义后字符串（含 NUL），调用者需用 lv_free 释放；
 *         分配失败返回 NULL
 */
char *lv_str_latex_escape_alloc(const char *src);

/* ===== 有理数格式化 ===== */

/**
 * @brief 将 GMP 有理数格式化为十进制字符串（统一 rational→string 入口）
 *
 * 内部以两遍法将 mpq_get_str / mpz_get_str 的 GMP 内存拷贝为 lv_malloc 堆串，
 * 调用者需用 lv_free 释放；den=1 且 omit_unit_denominator=true 时输出整数形式，
 * 否则输出恒定的 "num/den"。
 *
 * @param q                     GMP 有理数（可为 NULL，按失败处理）
 * @param omit_unit_denominator 为 true 时分母为 1 输出整数形式；为 false 时恒定 "num/den"
 * @return 堆分配的十进制字符串（含 NUL），分配失败返回 NULL
 */
char *lv_mpq_to_string(const mpq_t q, bool omit_unit_denominator);

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
