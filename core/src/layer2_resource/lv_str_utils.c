/**
 * @file lv_str_utils.c
 * @brief 统一字符串工具函数集实现
 */

#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"

#include <string.h>

/* ===== 字符串检查 ===== */

bool lv_str_startswith(const char *str, const char *prefix) {
    if (!str || !prefix) return false;
    size_t slen = strlen(str);
    size_t plen = strlen(prefix);
    if (plen > slen) return false;
    return strncmp(str, prefix, plen) == 0;
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

char *lv_str_trim(char *str) {
    if (!str) return NULL;
    str = lv_str_ltrim(str);
    lv_str_rtrim(str);
    return str;
}

/* ===== 辅助：动态数组追加 ===== */

static bool split_append(lvStrSplitResult *result, const char *seg, size_t seg_len, size_t *capacity) {
    if (result->count >= *capacity) {
        size_t new_cap = *capacity * 2;
        char **new_items = (char **)lv_malloc(sizeof(char *) * new_cap);
        if (!new_items) return false;
        memcpy(new_items, result->items, sizeof(char *) * result->count);
        lv_free((void **)&result->items);
        result->items = new_items;
        *capacity = new_cap;
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

char *lv_str_join(const char **items, size_t count, const char *separator) {
    if (!items || count == 0) return NULL;
    if (!separator) separator = "";

    lvStrBuf sb = {0};
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            lv_strbuf_printf(&sb, "%s", separator);
        }
        if (items[i]) {
            lv_strbuf_printf(&sb, "%s", items[i]);
        }
    }
    return lv_strbuf_to_string(&sb);
}

/* ===== 字符串转义 ===== */

void lv_str_escape_json(lvStrBuf *sb, const char *str, size_t len) {
    if (!sb || !str) return;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char) str[i];
        switch (c) {
            case '"':
                lv_strbuf_printf(sb, "\\\"");
                break;
            case '\\':
                lv_strbuf_printf(sb, "\\\\");
                break;
            case '\n':
                lv_strbuf_printf(sb, "\\n");
                break;
            case '\r':
                lv_strbuf_printf(sb, "\\r");
                break;
            case '\t':
                lv_strbuf_printf(sb, "\\t");
                break;
            default:
                if (c < 0x20) {
                    lv_strbuf_printf(sb, "\\u%04x", c);
                } else {
                    lv_strbuf_append_n(sb, (char) c, 1);
                }
                break;
        }
    }
}

void lv_str_escape_xml(lvStrBuf *sb, const char *str, size_t len) {
    if (!sb || !str) return;
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        switch (c) {
            case '&':
                lv_strbuf_printf(sb, "&amp;");
                break;
            case '<':
                lv_strbuf_printf(sb, "&lt;");
                break;
            case '>':
                lv_strbuf_printf(sb, "&gt;");
                break;
            case '"':
                lv_strbuf_printf(sb, "&quot;");
                break;
            case '\'':
                lv_strbuf_printf(sb, "&apos;");
                break;
            default:
                lv_strbuf_append_n(sb, c, 1);
                break;
        }
    }
}
