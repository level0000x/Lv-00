/**
 * @file lv_str_utils.c
 * @brief 统一字符串工具函数集实现
 */

#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"

#include <ctype.h>
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h> /* strcasecmp（lv_str_icmp） */
#endif

/* ===== 字符串检查 ===== */

bool lv_str_startswith(const char *str, const char *prefix) {
    if (!str || !prefix) return false;
    /* 仅依赖 strncmp：比较至多 plen 字节，遇 str 的 NUL 提前停止并判定不匹配，
     * 与手写 strncmp(str, "lit", N)==0 形态的读取模式完全一致（无全文 strlen 开销）。 */
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool lv_str_endswith(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    size_t slen = strlen(str);
    size_t suflen = strlen(suffix);
    if (suflen > slen) return false;
    return strcmp(str + slen - suflen, suffix) == 0;
}

bool lv_str_contains(const char *str, const char *substr) {
    if (!str || !substr) return false;
    return strstr(str, substr) != NULL;
}

bool lv_str_is_empty(const char *s) {
    return !s || s[0] == '\0';
}

bool lv_str_nonempty(const char *s) {
    return s && s[0] != '\0';
}

int lv_str_icmp(const char *a, const char *b) {
    if (a == b)
        return 0;
    if (!a)
        return -1;
    if (!b)
        return 1;
#if defined(_MSC_VER) || defined(_WIN32)
    return _stricmp(a, b);
#else
    return strcasecmp(a, b);
#endif
}

bool lv_str_eq(const char *a, const char *b) {
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    return strcmp(a, b) == 0;
}

bool lv_str_ne(const char *a, const char *b) {
    return !lv_str_eq(a, b);
}

int lv_str_icmp_n(const char *a, const char *b, size_t n) {
    if (a == b)
        return 0;
    if (!a)
        return -1;
    if (!b)
        return 1;
    /* 与既有 ws_header_equals/ws_header_contains_token 的手写折叠一致：
     * 仅折叠 ASCII 'A'..'Z'（+32），其余字节（含非 ASCII 高位字节）原样比较；
     * 用 unsigned char 避免有符号 char 的平台差异。 */
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = (unsigned char) a[i];
        unsigned char cb = (unsigned char) b[i];
        if (ca >= 'A' && ca <= 'Z')
            ca = (unsigned char) (ca + 32);
        if (cb >= 'A' && cb <= 'Z')
            cb = (unsigned char) (cb + 32);
        if (ca != cb)
            return ca < cb ? -1 : 1;
        if (ca == '\0')
            return 0; /* 双方同位置 NUL，剩余视为相等（strncmp 语义） */
    }
    return 0;
}

/* ===== 关键字表匹配 ===== */

int lv_str_match_any(const char *input, const char *const *keywords) {
    if (!input || !keywords)
        return -1;
    for (int i = 0; keywords[i] != NULL; i++) {
        if (strstr(input, keywords[i]) != NULL) {
            return i;
        }
    }
    return -1;
}

int lv_str_match_delimited(const char *input, const char *const *keywords) {
    if (!input || !keywords)
        return -1;
    for (int i = 0; keywords[i] != NULL; i++) {
        const char *found = strstr(input, keywords[i]);
        if (!found)
            continue;
        /* 命中后必须为分隔符结尾：'\0'、空白、'(' 或 '{' */
        char next = found[strlen(keywords[i])];
        if (next == '\0' || isspace((unsigned char) next) || next == '(' || next == '{') {
            return i;
        }
    }
    return -1;
}

/* ===== 字符串裁剪 ===== */

char *lv_str_ltrim(char *str) {
    if (!str) return NULL;
    while (*str && (unsigned char)*str <= ' ') str++;
    return str;
}

char *lv_str_rtrim(char *str) {
    if (!str) return str;
    size_t len = strlen(str);
    while (len > 0 && (unsigned char)str[len - 1] <= ' ') {
        str[len - 1] = '\0';
        len--;
    }
    return str;
}

char *lv_str_chomp(char *str) {
    if (!str) return str;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
    return str;
}

char *lv_str_trim(char *str) {
    if (!str) return NULL;
    str = lv_str_ltrim(str);
    lv_str_rtrim(str);
    return str;
}

/* ===== 辅助：动态数组追加 ===== */

static bool split_append(lvStrSplitResult *result, const char *seg, size_t seg_len, size_t *capacity) {
    if (result->count >= *capacity) {
        int cap_i = (int)*capacity;
        if (!lv_ensure_capacity((void **)&result->items, (int)result->count, &cap_i, sizeof(char *), 0))
            return false;
        *capacity = (size_t)cap_i;
    }
    lvStrBuf sb = {0};
    lv_strbuf_printf(&sb, "%.*s", (int)seg_len, seg);
    char *item = lv_strbuf_to_string(&sb);
    if (!item) return false;
    result->items[result->count++] = item;
    return true;
}

/* ===== 字符串分割 ===== */

lvStrSplitResult lv_str_split(const char *str, const char *delim) {
    lvStrSplitResult result = {NULL, 0};
    if (!str || !delim || !*str) return result;

    size_t delim_len = strlen(delim);
    if (delim_len == 0) return result;

    size_t capacity = 8;
    result.items = (char **)lv_malloc(sizeof(char *) * capacity);
    if (!result.items) return result;

    const char *p = str;
    while (*p) {
        const char *found = strstr(p, delim);
        if (!found) {
            /* 最后一段 */
            split_append(&result, p, strlen(p), &capacity);
            break;
        }

        size_t seg_len = (size_t)(found - p);
        /* 跳过开头的空片段，保留中间的连续分隔符产生的空片段 */
        if (seg_len > 0 || result.count > 0) {
            split_append(&result, p, seg_len, &capacity);
        }
        p = found + delim_len;
    }

    return result;
}

void lv_str_split_free(lvStrSplitResult *result) {
    if (!result) return;
    for (size_t i = 0; i < result->count; i++) {
        if (result->items[i]) {
            lv_free((void **)&result->items[i]);
        }
    }
    lv_free((void **)&result->items);
    result->items = NULL;
    result->count = 0;
}

char *lv_strtok_r(char *str, const char *delim, char **saveptr) {
    if (!delim || !saveptr) return NULL;
    /* MSVC 的 strtok_s 与 POSIX strtok_r 签名兼容（str, delim, context），
     * 平台差异由 lv_platform.h 的 strtok_s→strtok_r 宏映射统一处理。 */
    return strtok_r(str, delim, saveptr);
}

/* ===== 定界符扫描 ===== */

const char *lv_str_skip_balanced(const char *p, char open, char close) {
    if (!p || *p != open)
        return p;
    int depth = 0;
    for (; *p; p++) {
        if (*p == open) {
            depth++;
        } else if (*p == close) {
            depth--;
            if (depth == 0) {
                p++;
                return p;
            }
        } else if (*p == '"') {
            /* 跳过字符串字面量（含转义引号） */
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p + 1))
                    p++;
                p++;
            }
        }
    }
    return NULL; /* 不平衡 */
}

bool lv_str_check_balanced(const char *p, char open, char close) {
    if (!p)
        return false;
    int depth = 0;
    for (; *p; p++) {
        if (*p == open) {
            depth++;
        } else if (*p == close) {
            depth--;
            if (depth < 0)
                return false;
        } else if (*p == '"') {
            /* 跳过字符串字面量（含转义引号） */
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && *(p + 1))
                    p++;
                p++;
            }
        }
    }
    return depth == 0;
}

/* ===== 流式文本解析原语 ===== */

const char *lv_str_skip_ws(const char *p) {
    if (!p)
        return NULL;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return p;
}

const char *lv_str_skip_ws_n(const char *p, const char *end) {
    if (!p || !end)
        return p;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    return p;
}

const char *lv_str_skip_until(const char *p, const char *any_of) {
    if (!p || !any_of)
        return p;
    return p + strcspn(p, any_of);
}

bool lv_str_read_int(const char **pp, int64_t *out) {
    if (!pp || !out)
        return false;
    const char *p = lv_str_skip_ws(*pp);
    bool negative = false;
    if (*p == '-') {
        negative = true;
        p++;
    }
    if (*p < '0' || *p > '9') {
        /* 无数字：输出 0、指针不动（与历史 parse_int/read_int 语义一致） */
        *out = 0;
        return false;
    }

    /* 无符号累加避免有符号溢出未定义行为；溢出时钳位并继续消费数字 */
    uint64_t val = 0;
    bool overflow = false;
    while (*p >= '0' && *p <= '9') {
        uint64_t digit = (uint64_t) (*p - '0');
        if (!overflow) {
            if (val > (UINT64_MAX - digit) / 10)
                overflow = true;
            else
                val = val * 10 + digit;
        }
        p++;
    }
    *pp = p;

    if (overflow) {
        *out = negative ? INT64_MIN : INT64_MAX;
    } else if (negative) {
        /* val <= 2^63：直接取负；val == 2^63 恰为 INT64_MIN */
        if (val > (uint64_t) INT64_MAX + 1)
            *out = INT64_MIN;
        else
            *out = (val == (uint64_t) INT64_MAX + 1) ? INT64_MIN : -(int64_t) val;
    } else {
        *out = (val > (uint64_t) INT64_MAX) ? INT64_MAX : (int64_t) val;
    }
    return true;
}

bool lv_str_read_quoted(const char **pp, char **out) {
    if (!pp || !out)
        return false;
    const char *p = lv_str_skip_ws(*pp);
    *out = NULL;
    if (*p != '"') {
        *pp = p; /* 失败：停在跳过空白后的位置（与历史 parse_quoted_string 一致） */
        return false;
    }
    p++; /* 跳过开始引号 */
    const char *start = p;
    while (*p && *p != '"')
        p++;
    size_t len = (size_t) (p - start);
    char *result = lv_malloc(len + 1);
    if (!result) {
        *pp = p;
        return false;
    }
    lv_strlcpy_n(result, len + 1, start, len);
    if (*p == '"')
        p++; /* 跳过结束引号 */
    *out = result;
    *pp = p;
    return true;
}

const char *lv_str_read_token(const char **pp, char **out, const char *delims) {
    if (!out) {
        return pp ? *pp : NULL;
    }
    *out = NULL;
    if (!pp || !delims) {
        return pp ? *pp : NULL;
    }
    const char *p = lv_str_skip_ws(*pp);
    const char *start = p;
    while (*p && !strchr(delims, *p))
        p++;
    size_t len = (size_t) (p - start);
    char *result = lv_malloc(len + 1);
    if (!result) {
        *pp = p;
        return p;
    }
    lv_strlcpy_n(result, len + 1, start, len);
    *out = result;
    *pp = p;
    return p;
}

/* ===== 字符串替换 ===== */

char *lv_str_replace(const char *str, const char *old_str, const char *new_str) {
    if (!str || !old_str || !new_str) return NULL;
    size_t old_len = strlen(old_str);
    if (old_len == 0) return NULL;

    lvStrBuf sb = {0};
    const char *p = str;
    const char *found;

    while ((found = strstr(p, old_str)) != NULL) {
        /* 追加匹配位置之前的片段 */
        lv_strbuf_printf(&sb, "%.*s", (int)(found - p), p);
        /* 追加替换字符串 */
        lv_strbuf_printf(&sb, "%s", new_str);
        /* 移到匹配位置后面 */
        p = found + old_len;
    }
    /* 追加剩余部分 */
    lv_strbuf_printf(&sb, "%s", p);

    return lv_strbuf_to_string(&sb);
}

/* ===== 字符串拼接 ===== */

/* 共享骨架：向 lvStrBuf 追加 count 个项，项间插入 separator（首项省略分隔符） */
static void strbuf_join_items(lvStrBuf *sb, const char *const *items, size_t count, const char *separator) {
    for (size_t i = 0; i < count; i++) {
        if (i > 0)
            lv_strbuf_printf(sb, "%s", separator);
        if (items[i])
            lv_strbuf_printf(sb, "%s", items[i]);
    }
}

bool lv_strbuf_join(lvStrBuf *sb, const char *const *items, size_t count, const char *separator) {
    if (!sb)
        return false;
    if (count == 0 || !items)
        return true;
    if (!separator)
        separator = "";
    strbuf_join_items(sb, items, count, separator);
    return true;
}

char *lv_str_join(const char **items, size_t count, const char *separator) {
    if (!items || count == 0) return NULL;
    if (!separator) separator = "";

    lvStrBuf sb = {0};
    strbuf_join_items(&sb, items, count, separator);
    return lv_strbuf_to_string(&sb);
}

/* ===== 游标式缓冲追加 ===== */

bool lv_str_append_sep(char *dst, size_t size, size_t *pos, const char *sep, const char *item) {
    if (!dst || !pos || !item || *pos >= size)
        return false;
    const char *prefix = (*pos > 0) ? (sep ? sep : "") : "";
    size_t prefix_len = strlen(prefix);
    size_t item_len = strlen(item);
    /* 需要 prefix + item + '\0' 全部放下 */
    if (prefix_len + item_len + 1 > size - *pos)
        return false;
    if (prefix_len > 0)
        memcpy(dst + *pos, prefix, prefix_len);
    memcpy(dst + *pos + prefix_len, item, item_len);
    dst[*pos + prefix_len + item_len] = '\0';
    *pos += prefix_len + item_len;
    return true;
}

/* ===== 字符串转义 ===== */

/**
 * @brief JSON 转义字符 → 转义对字符串 查找表（按 ASCII 下标，NULL 表示非简单转义，走控制字符逻辑）
 *
 * lv_str_json_escape 使用此唯一查找表（含 \b/\f，避免控制字符原样输出破坏 JSON）。
 */
static const char *const s_json_escape_pairs[256] = {
    ['"']  = "\\\"",
    ['\\'] = "\\\\",
    ['\n'] = "\\n",
    ['\r'] = "\\r",
    ['\t'] = "\\t",
    ['\b'] = "\\b",
    ['\f'] = "\\f",
};

/** @brief XML 转义字符 → 实体字符串 查找表（NULL 表示原样输出） */
static const char *const s_str_escape_xml_entities[256] = {
    ['&']  = "&amp;",
    ['<']  = "&lt;",
    ['>']  = "&gt;",
    ['"']  = "&quot;",
    ['\''] = "&apos;",
};

void lv_str_escape_xml(lvStrBuf *sb, const char *str, size_t len) {
    if (!sb || !str) return;
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        /* 查找表：转义字符 → 实体字符串；未命中（NULL）走 default 原样输出 */
        const char *ent = s_str_escape_xml_entities[(unsigned char)c];
        if (ent) {
            lv_strbuf_printf(sb, "%s", ent);
        } else {
            lv_strbuf_append_n(sb, c, 1);
        }
    }
}

/* ===== JSON/HTML 转义（统一公共 API，snprintf 语义） ===== */

/** @brief HTML 转义字符 → 实体字符串 查找表（按 ASCII 下标，NULL 表示原样输出；' 用 &#39; 而非 XML 的 &apos;） */
static const char *const s_html_escape_entities[256] = {
    ['&']  = "&amp;",
    ['<']  = "&lt;",
    ['>']  = "&gt;",
    ['"']  = "&quot;",
    ['\''] = "&#39;",
};

/** @brief 将 src 的转义结果写入 dst（内部辅助：统一截断与 NUL 结尾语义） */
static void str_escape_write(char *dst, size_t dst_cap, size_t *written, const char *seg, size_t seg_len) {
    if (!dst || dst_cap == 0)
        return;
    if (*written >= dst_cap - 1)
        return;
    size_t copy = seg_len;
    if (*written + copy > dst_cap - 1)
        copy = dst_cap - 1 - *written;
    memcpy(dst + *written, seg, copy);
    *written += copy;
}

size_t lv_str_json_escape(const char *src, size_t src_len, char *dst, size_t dst_cap) {
    if (!src)
        src_len = 0;
    size_t need = 0;    /* 转义后所需总长度（不含 NUL） */
    size_t written = 0; /* 实际写入长度（<= dst_cap-1） */
    for (size_t i = 0; i < src_len; i++) {
        unsigned char c = (unsigned char) src[i];
        const char *pair = s_json_escape_pairs[c];
        char uesc[8];
        const char *seg;
        size_t seg_len;
        if (pair) {
            seg = pair;
            seg_len = 2;
        } else if (c < 0x20) {
            /* 其他控制字符编码为 \u00XX */
            seg = uesc;
            seg_len = (size_t) snprintf(uesc, sizeof(uesc), "\\u%04x", c);
        } else {
            seg = (const char *) &src[i];
            seg_len = 1;
        }
        need += seg_len;
        str_escape_write(dst, dst_cap, &written, seg, seg_len);
    }
    if (dst && dst_cap > 0)
        dst[written] = '\0';
    return need;
}

char *lv_str_json_escape_alloc(const char *src, size_t src_len, size_t *out_len) {
    size_t need = lv_str_json_escape(src, src_len, NULL, 0);
    char *buf = (char *) lv_malloc(need + 1);
    if (!buf)
        return NULL;
    lv_str_json_escape(src, src_len, buf, need + 1);
    if (out_len)
        *out_len = need;
    return buf;
}

size_t lv_str_html_escape(const char *src, size_t src_len, char *dst, size_t dst_cap) {
    if (!src)
        src_len = 0;
    size_t need = 0;
    size_t written = 0;
    for (size_t i = 0; i < src_len; i++) {
        unsigned char c = (unsigned char) src[i];
        const char *ent = s_html_escape_entities[c];
        if (ent) {
            need += strlen(ent);
            str_escape_write(dst, dst_cap, &written, ent, strlen(ent));
        } else {
            need++;
            str_escape_write(dst, dst_cap, &written, (const char *) &src[i], 1);
        }
    }
    if (dst && dst_cap > 0)
        dst[written] = '\0';
    return need;
}

char *lv_str_html_escape_alloc(const char *src) {
    size_t len = src ? strlen(src) : 0;
    size_t need = lv_str_html_escape(src, len, NULL, 0);
    char *buf = (char *) lv_malloc(need + 1);
    if (!buf)
        return NULL;
    lv_str_html_escape(src, len, buf, need + 1);
    return buf;
}

/** @brief LaTeX 特殊字符 → 转义字符串 查找表（按 ASCII 下标，NULL 表示原样输出）
 *  % → \%：LaTeX 中 \% 才是百分号转义（\\%% 会让裸 % 起注释吞行） */
static const char *const s_latex_escape_entities[256] = {
    ['\\'] = "\\textbackslash{}",
    ['{']  = "\\{",
    ['}']  = "\\}",
    ['_']  = "\\_",
    ['&']  = "\\&",
    ['#']  = "\\#",
    ['%']  = "\\%",
    ['$']  = "\\$",
    ['^']  = "\\^{}",
    ['~']  = "\\~{}",
};

size_t lv_str_latex_escape(const char *src, size_t src_len, char *dst, size_t dst_cap) {
    if (!src)
        src_len = 0;
    size_t need = 0;    /* 转义后所需总长度（不含 NUL） */
    size_t written = 0; /* 实际写入长度（<= dst_cap-1） */
    for (size_t i = 0; i < src_len; i++) {
        unsigned char c = (unsigned char) src[i];
        const char *esc = s_latex_escape_entities[c];
        if (esc) {
            need += strlen(esc);
            str_escape_write(dst, dst_cap, &written, esc, strlen(esc));
        } else {
            need++;
            str_escape_write(dst, dst_cap, &written, (const char *) &src[i], 1);
        }
    }
    if (dst && dst_cap > 0)
        dst[written] = '\0';
    return need;
}

char *lv_str_latex_escape_alloc(const char *src) {
    size_t len = src ? strlen(src) : 0;
    size_t need = lv_str_latex_escape(src, len, NULL, 0);
    char *buf = (char *) lv_malloc(need + 1);
    if (!buf)
        return NULL;
    lv_str_latex_escape(src, len, buf, need + 1);
    return buf;
}

char *lv_mpq_to_string(const mpq_t q, bool omit_unit_denominator) {
    if (!q)
        return NULL;
    char *num_str = mpz_get_str(NULL, 10, mpq_numref(q));
    if (!num_str)
        return NULL;
    if (omit_unit_denominator && mpz_cmp_ui(mpq_denref(q), 1) == 0) {
        size_t num_len = strlen(num_str);
        char *buf = (char *) lv_malloc(num_len + 1);
        if (!buf) {
            free(num_str);
            return NULL;
        }
        memcpy(buf, num_str, num_len + 1);
        free(num_str);
        return buf;
    }
    char *den_str = mpz_get_str(NULL, 10, mpq_denref(q));
    if (!den_str) {
        free(num_str);
        return NULL;
    }
    size_t need = strlen(num_str) + strlen(den_str) + 2;
    char *buf = (char *) lv_malloc(need);
    if (!buf) {
        free(num_str);
        free(den_str);
        return NULL;
    }
    snprintf(buf, need, "%s/%s", num_str, den_str);
    free(num_str);
    free(den_str);
    return buf;
}

/**
 * @brief 解析 JSON \uXXXX 转义为一个完整 Unicode 码点（含 UTF-16 代理对合并）
 *
 * src 指向 \u 之后的第一个十六进制字符。高代理（U+D800-U+DBFF）后若紧跟
 * \uXXXX 形式的低代理（U+DC00-U+DFFF）则合并为补充平面码点。
 *
 * @param src     源缓冲（\u 之后部分），可为 NULL
 * @param src_len src 剩余字节数
 * @param out_adv 输出实际消耗字节数（合法 4 位 = 4；含低代理合并 = 10；不足 = 已解析位数 0-3）
 * @return 完整码点；孤立代理或非法十六进制返回 0xFFFFFFFF（调用方应写 U+FFFD）
 */
unsigned int lv_str_json_read_codepoint(const char *src, size_t src_len, size_t *out_adv) {
    if (out_adv)
        *out_adv = 0;
    if (!src || src_len < 4)
        return 0xFFFFFFFFu;

    unsigned int cp = 0;
    size_t i = 0;
    for (; i < 4; i++) {
        char h = src[i];
        unsigned int nibble;
        if (h >= '0' && h <= '9')
            nibble = (unsigned int) (h - '0');
        else if (h >= 'a' && h <= 'f')
            nibble = (unsigned int) (h - 'a' + 10);
        else if (h >= 'A' && h <= 'F')
            nibble = (unsigned int) (h - 'A' + 10);
        else
            break; /* 非法十六进制：截断 */
        cp = (cp << 4) | nibble;
    }
    if (out_adv)
        *out_adv = i;
    if (i < 4)
        return 0xFFFFFFFFu;

    /* 高代理：尝试合并后续低代理 \uDC00-\uDFFF */
    if (cp >= 0xD800 && cp <= 0xDBFF) {
        if (src_len >= 10 && src[4] == '\\' && src[5] == 'u') {
            unsigned int lo = 0;
            size_t j = 0;
            for (; j < 4; j++) {
                char h = src[6 + j];
                unsigned int nibble;
                if (h >= '0' && h <= '9')
                    nibble = (unsigned int) (h - '0');
                else if (h >= 'a' && h <= 'f')
                    nibble = (unsigned int) (h - 'a' + 10);
                else if (h >= 'A' && h <= 'F')
                    nibble = (unsigned int) (h - 'A' + 10);
                else
                    break;
                lo = (lo << 4) | nibble;
            }
            if (j == 4 && lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                if (out_adv)
                    *out_adv = 10;
                return cp;
            }
        }
        return 0xFFFFFFFFu; /* 孤立高代理 → U+FFFD */
    }

    /* 孤立低代理 → U+FFFD */
    if (cp >= 0xDC00 && cp <= 0xDFFF)
        return 0xFFFFFFFFu;

    return cp;
}

/**
 * @brief 将 Unicode 码点编码为 UTF-8
 *
 * @param cp  码点（0x0-0x10FFFF）
 * @param dst 目标缓冲（至少 4 字节）
 * @param cap 目标容量（字节）
 * @return 写入字节数；cp 无效或容量不足返回 0
 */
size_t lv_str_codepoint_to_utf8(unsigned int cp, char *dst, size_t cap) {
    if (!dst || cap == 0)
        return 0;
    if (cp <= 0x7Fu) {
        if (cap < 1)
            return 0;
        dst[0] = (char) cp;
        return 1;
    }
    if (cp <= 0x7FFu) {
        if (cap < 2)
            return 0;
        dst[0] = (char) (0xC0u | (cp >> 6));
        dst[1] = (char) (0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp <= 0xFFFFu) {
        if (cap < 3)
            return 0;
        dst[0] = (char) (0xE0u | (cp >> 12));
        dst[1] = (char) (0x80u | ((cp >> 6) & 0x3Fu));
        dst[2] = (char) (0x80u | (cp & 0x3Fu));
        return 3;
    }
    if (cp <= 0x10FFFFu) {
        if (cap < 4)
            return 0;
        dst[0] = (char) (0xF0u | (cp >> 18));
        dst[1] = (char) (0x80u | ((cp >> 12) & 0x3Fu));
        dst[2] = (char) (0x80u | ((cp >> 6) & 0x3Fu));
        dst[3] = (char) (0x80u | (cp & 0x3Fu));
        return 4;
    }
    return 0; /* 无效码点 */
}

size_t lv_str_json_unescape(const char *src, size_t src_len, char *dst, size_t dst_cap) {
    if (!src)
        src_len = 0;
    size_t need = 0;    /* 解码后所需总长度（不含 NUL） */
    size_t written = 0; /* 实际写入长度（<= dst_cap-1） */
    size_t i = 0;
    char utf8[4];
    while (i < src_len) {
        unsigned char c = (unsigned char) src[i];
        size_t seg_len = 1;
        size_t adv = 1;
        const char *seg = (const char *) &src[i];
        if (c == '\\' && i + 1 < src_len) {
            unsigned char e = (unsigned char) src[i + 1];
            adv = 2;
            switch (e) {
                case '"':  seg = "\""; break;
                case '\\': seg = "\\"; break;
                case '/':  seg = "/";  break;
                case 'b':  seg = "\b"; break;
                case 'f':  seg = "\f"; break;
                case 'n':  seg = "\n"; break;
                case 'r':  seg = "\r"; break;
                case 't':  seg = "\t"; break;
                case 'u': {
                    /* \uXXXX → UTF-8（完整 UTF-16 代理对处理；孤立代理/非法 → U+FFFD） */
                    size_t adv_cp = 0;
                    unsigned int cp = lv_str_json_read_codepoint(src + i + 2, src_len - (i + 2), &adv_cp);
                    if (adv_cp == 4 || adv_cp == 10) {
                        if (cp == 0xFFFFFFFFu)
                            cp = 0xFFFDu;
                        size_t n = lv_str_codepoint_to_utf8(cp, utf8, sizeof(utf8));
                        if (n > 0) {
                            seg = utf8;
                            seg_len = n;
                            adv = 2 + adv_cp;
                        } else {
                            /* 无效码点：保留 'u' 原样 */
                            seg = (const char *) &src[i + 1];
                            seg_len = 1;
                        }
                    } else {
                        /* 非十六进制/不足 4 位：保留 'u' 原样 */
                        seg = (const char *) &src[i + 1];
                        seg_len = 1;
                    }
                    break;
                }
                default:
                    /* 未声明的转义：保留转义字符本身（去掉反斜杠） */
                    seg = (const char *) &src[i + 1];
                    seg_len = 1;
                    break;
            }
        }
        need += seg_len;
        str_escape_write(dst, dst_cap, &written, seg, seg_len);
        i += adv;
    }
    if (dst && dst_cap > 0)
        dst[written] = '\0';
    return need;
}

/* ===== 报告表格辅助 ===== */

void lv_strbuf_append_sep(lvStrBuf *sb, char ch, size_t count) {
    if (!sb) return;
    lv_strbuf_append_n(sb, ch, count);
}

void lv_strbuf_append_cell(lvStrBuf *sb, const char *text, size_t width) {
    if (!sb) return;
    size_t len = 0;
    if (text) {
        len = strlen(text);
        lv_strbuf_printf(sb, "%s", text);
    }
    if (len < width) {
        lv_strbuf_append_n(sb, ' ', width - len);
    }
}
