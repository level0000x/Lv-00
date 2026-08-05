/**
 * @file lv_json.c
 * @brief Lv-00 统一 JSON 解析与写入库实现
 *
 * @details 整合 graph_serialize.c 的 JsonParser/JsonBuf 与 lv_config.c 的
 *          独立 JSON 解析函数。所有内存分配使用 lv_malloc/lv_free/lv_realloc。
 *
 * @author Lv-00 Project
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_json.h"
#include "lv/lv_internal.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_utils.h"

/* ==================================================================
 * 转义查找表（ASCII 下标，替代 switch-case 反模式）
 * ================================================================== */

/** @brief 解析阶段：转义字符 → {前进字节数, 长度增量} 查找表（adv=0 表示未声明，走 default） */
typedef struct {
    unsigned char adv; /**< 输入前进字节数 */
    unsigned char inc; /**< 解码后长度增量 */
} lvJsonEscapeStep;

static const lvJsonEscapeStep s_json_escape_steps[256] = {
    ['"']  = {2, 1},
    ['\\'] = {2, 1},
    ['/']  = {2, 1},
    ['b']  = {2, 1},
    ['f']  = {2, 1},
    ['n']  = {2, 1},
    ['r']  = {2, 1},
    ['t']  = {2, 1},
    ['u']  = {6, 4}, /* \uXXXX：前进 6 字节，最多产生 4 字节 UTF-8 */
};

/** @brief 解析阶段：简单转义字符 → 解码字符 查找表（'\0' 表示未声明，走 default 原样保留） */
static const char s_json_escape_decode[256] = {
    ['"']  = '"',
    ['\\'] = '\\',
    ['/']  = '/',
    ['b']  = '\b',
    ['f']  = '\f',
    ['n']  = '\n',
    ['r']  = '\r',
    ['t']  = '\t',
};

/** @brief 写出阶段：转义字符 → 转义后长度增量 查找表（0 表示未声明，走 default 控制字符逻辑） */
static const unsigned char s_json_escape_len[256] = {
    ['"']  = 2,
    ['\\'] = 2,
    ['\n'] = 2,
    ['\t'] = 2,
    ['\r'] = 2,
    ['\b'] = 2,
    ['\f'] = 2,
};

/** @brief 写出阶段：转义字符 → 转义对字符串 查找表（NULL 表示未声明，走 default） */
static const char *const s_json_escape_pairs[256] = {
    ['"']  = "\\\"",
    ['\\'] = "\\\\",
    ['\n'] = "\\n",
    ['\t'] = "\\t",
    ['\r'] = "\\r",
    ['\b'] = "\\b",
    ['\f'] = "\\f",
};

/* ==================================================================
 * JSON 解析器实现
 * ================================================================== */

void lv_json_parser_init(lvJsonParser *p, const char *data, size_t size) {
    p->data = data;
    p->size = size;
    p->pos = 0;
}

void lv_json_skip_ws(lvJsonParser *p) {
    while (p->pos < p->size) {
        char c = p->data[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            p->pos++;
        } else {
            break;
        }
    }
}

char lv_json_peek(lvJsonParser *p) {
    lv_json_skip_ws(p);
    return p->pos < p->size ? p->data[p->pos] : '\0';
}

char lv_json_next(lvJsonParser *p) {
    lv_json_skip_ws(p);
    return p->pos < p->size ? p->data[p->pos++] : '\0';
}

bool lv_json_expect(lvJsonParser *p, char c) {
    char got = lv_json_next(p);
    return got == c;
}

char *lv_json_parse_string(lvJsonParser *p) {
    if (!lv_json_expect(p, '"'))
        lv_RETURN_ERROR_NULL(lv_ERROR_PARSE, "lv_json_parse_string: expected opening '\"'");

    const char *start = p->data + p->pos;
    size_t len = 0;

    /* 第一遍：计算转义后的字符串长度 */
    while (p->pos < p->size && p->data[p->pos] != '"') {
        if (p->data[p->pos] == '\\' && p->pos + 1 < p->size) {
            /* 查找表：按转义字符索引 {前进字节数, 长度增量}，未命中走 default */
            const lvJsonEscapeStep step = s_json_escape_steps[(unsigned char) p->data[p->pos + 1]];
            if (step.adv > 0) {
                p->pos += step.adv;
                len += step.inc;
            } else {
                /* default：未声明的转义字符按单字符处理 */
                p->pos += 2;
                len++;
            }
        } else {
            p->pos++;
            len++;
        }
    }

    if (p->pos >= p->size)
        lv_RETURN_ERROR_NULL(lv_ERROR_PARSE, "lv_json_parse_string: unterminated string");
    p->pos++; /* skip end quote */

    /* 分配结果缓冲区 */
    char *result = lv_malloc(len + 1);
    if (!result)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_json_parse_string: buffer allocation failed");

    /* 第二遍：解码转义序列 */
    const char *src = start;
    char *dst = result;
    const char *end = p->data + p->pos - 1; /* 指向结束引号 */

    while (src < end) {
        if (*src == '\\' && src + 1 < end) {
            src++;
            if (*src == 'u') {
                /* \uXXXX — 简化为原样传递（完整 UTF-16 代理对处理过于复杂） */
                if (src + 4 < end) {
                    /* 将 \uXXXX 编码为 UTF-8 */
                    unsigned int codepoint = 0;
                    for (int i = 0; i < 4; i++) {
                        src++;
                        char c = *src;
                        codepoint <<= 4;
                        if (c >= '0' && c <= '9')
                            codepoint |= (unsigned int)(c - '0');
                        else if (c >= 'a' && c <= 'f')
                            codepoint |= (unsigned int)(c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F')
                            codepoint |= (unsigned int)(c - 'A' + 10);
                        else
                            codepoint |= 0xFFFD; /* 替换字符 */
                    }
                    /* 编码为 UTF-8 */
                    if (codepoint < 0x80) {
                        *dst++ = (char)codepoint;
                    } else if (codepoint < 0x800) {
                        *dst++ = (char)(0xC0 | (codepoint >> 6));
                        *dst++ = (char)(0x80 | (codepoint & 0x3F));
                    } else {
                        *dst++ = (char)(0xE0 | (codepoint >> 12));
                        *dst++ = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                        *dst++ = (char)(0x80 | (codepoint & 0x3F));
                    }
                }
            } else {
                /* 简单转义查表解码；未命中（'\0'）走 default 原样保留 */
                char dec = s_json_escape_decode[(unsigned char)*src];
                *dst++ = dec != '\0' ? dec : *src;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return result;
}

bool lv_json_parse_int(lvJsonParser *p, int *out) {
    lv_json_skip_ws(p);
    size_t start = p->pos;
    bool negative = false;

    if (p->pos < p->size && p->data[p->pos] == '-') {
        negative = true;
        p->pos++;
    }

    if (p->pos >= p->size || p->data[p->pos] < '0' || p->data[p->pos] > '9') {
        return false;
    }

    while (p->pos < p->size && p->data[p->pos] >= '0' && p->data[p->pos] <= '9') {
        p->pos++;
    }

    if (p->pos == start || (p->pos == start + 1 && negative))
        return false;

    long long val = 0;
    for (const char *s = p->data + start + (negative ? 1 : 0); s < p->data + p->pos; s++) {
        val = val * 10 + (*s - '0');
    }
    *out = negative ? (int)(-val) : (int)val;
    return true;
}

bool lv_json_parse_double(lvJsonParser *p, double *out) {
    lv_json_skip_ws(p);
    const char *start = p->data + p->pos;

    /* 收集完整的 JSON 数字标记 */
    char buf[128];
    int i = 0;

    if (p->pos < p->size && (p->data[p->pos] == '-' || p->data[p->pos] == '+')) {
        if (i < 127)
            buf[i++] = p->data[p->pos];
        p->pos++;
    }

    while (p->pos < p->size && i < 127) {
        char c = p->data[p->pos];
        if (c >= '0' && c <= '9') {
            buf[i++] = c;
            p->pos++;
        } else if (c == '.' || c == 'e' || c == 'E') {
            buf[i++] = c;
            p->pos++;
        } else if ((c == '-' || c == '+') && i > 0 &&
                   (buf[i - 1] == 'e' || buf[i - 1] == 'E')) {
            buf[i++] = c;
            p->pos++;
        } else {
            break;
        }
    }

    if (i == 0 || (i == 1 && (buf[0] == '-' || buf[0] == '+'))) {
        p->pos = (size_t)(start - p->data);
        return false;
    }

    buf[i] = '\0';

    /* 使用 lv_parse_double 安全解析 */
    if (lv_parse_double(buf, out) != 0) {
        p->pos = (size_t)(start - p->data);
        return false;
    }
    return true;
}

bool lv_json_parse_bool(lvJsonParser *p, bool *out) {
    lv_json_skip_ws(p);
    if (p->pos + 4 <= p->size && strncmp(p->data + p->pos, "true", 4) == 0) {
        p->pos += 4;
        *out = true;
        return true;
    }
    if (p->pos + 5 <= p->size && strncmp(p->data + p->pos, "false", 5) == 0) {
        p->pos += 5;
        *out = false;
        return true;
    }
    return false;
}

const char *lv_json_find_key(const char *json, const char *key, size_t key_len) {
    if (!json || !key || key_len == 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "lv_json_find_key: NULL json/key or zero key_len");

    const char *p = json;
    const char *end = p + strlen(p);

    while (p < end) {
        /* 跳过空白 */
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            p++;
        if (p >= end || *p == '}' || *p == '\0')
            return NULL;

        if (*p == ',')
            p++;

        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            p++;
        if (p >= end)
            return NULL;

        if (*p == '"') {
            p++;
            /* 匹配 key */
            const char *ks = key;
            size_t remaining = key_len;
            while (p < end && *p != '"' && remaining > 0 && *p == *ks) {
                p++;
                ks++;
                remaining--;
            }
            if (p < end && *p == '"' && remaining == 0) {
                p++; /* skip closing quote */

                /* 跳过空白和冒号，返回值的起始位置 */
                while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
                    p++;
                if (p < end && *p == ':')
                    p++;
                while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
                    p++;
                return p;
            }

            /* key 不匹配，跳过整个字符串 */
            while (p < end && *p != '"')
                p++;
            if (p < end && *p == '"')
                p++;

            /* 跳过值部分 */
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
                p++;
            if (p < end && *p == ':')
                p++;
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
                p++;

            if (p >= end)
                return NULL;

            if (*p == '{' || *p == '[') {
                int depth = 1;
                p++;
                while (p < end && depth > 0) {
                    if (*p == '{' || *p == '[')
                        depth++;
                    else if (*p == '}' || *p == ']')
                        depth--;
                    p++;
                }
            } else if (*p == '"') {
                p++;
                while (p < end && *p != '"') {
                    if (*p == '\\')
                        p++;
                    p++;
                }
                if (p < end)
                    p++;
            } else {
                /* 数字或字面量 */
                while (p < end && *p != ',' && *p != '}' && *p != ']' &&
                       *p != ' ' && *p != '\n' && *p != '\t' && *p != '\r')
                    p++;
            }
        } else if (*p == '{') {
            /* 跳过开括号进入对象内部，继续搜索键 */
            p++;
        } else if (*p == '[') {
            /* 跳过整个数组（数组内无命名键） */
            int depth = 1;
            p++;
            while (p < end && depth > 0) {
                if (*p == '[')
                    depth++;
                else if (*p == ']')
                    depth--;
                p++;
            }
        } else {
            /* 跳过其他非字符串 token（如字面量 true/false/null） */
            while (p < end && *p != ',' && *p != '}' && *p != ']' &&
                   *p != ' ' && *p != '\n' && *p != '\t' && *p != '\r')
                p++;
        }
    }
    return NULL;
}

void lv_json_skip_value(lvJsonParser *p) {
    lv_json_skip_ws(p);
    if (p->pos >= p->size)
        return;

    char c = p->data[p->pos];
    if (c == '"') {
        /* string */
        p->pos++;
        while (p->pos < p->size && p->data[p->pos] != '"') {
            if (p->data[p->pos] == '\\')
                p->pos++;
            p->pos++;
        }
        if (p->pos < p->size)
            p->pos++;
    } else if (c == '{') {
        /* object */
        p->pos++;
        while (p->pos < p->size && p->data[p->pos] != '}') {
            if (p->data[p->pos] == '"') {
                lv_json_skip_value(p);
                lv_json_skip_ws(p);
                if (p->pos < p->size && p->data[p->pos] == ':')
                    p->pos++;
                lv_json_skip_value(p);
            } else if (p->data[p->pos] == '}') {
                break;
            } else {
                p->pos++;
            }
        }
        if (p->pos < p->size)
            p->pos++;
    } else if (c == '[') {
        /* array */
        p->pos++;
        while (p->pos < p->size && p->data[p->pos] != ']') {
            lv_json_skip_value(p);
            lv_json_skip_ws(p);
            if (p->pos < p->size && p->data[p->pos] == ',')
                p->pos++;
        }
        if (p->pos < p->size)
            p->pos++;
    } else if (c == 't' && p->pos + 4 <= p->size &&
               strncmp(p->data + p->pos, "true", 4) == 0) {
        p->pos += 4;
    } else if (c == 'f' && p->pos + 5 <= p->size &&
               strncmp(p->data + p->pos, "false", 5) == 0) {
        p->pos += 5;
    } else if (c == 'n' && p->pos + 4 <= p->size &&
               strncmp(p->data + p->pos, "null", 4) == 0) {
        p->pos += 4;
    } else {
        /* number or unknown literal */
        while (p->pos < p->size &&
               p->data[p->pos] != ',' && p->data[p->pos] != '}' && p->data[p->pos] != ']' &&
               p->data[p->pos] != ' ' && p->data[p->pos] != '\n' && p->data[p->pos] != '\t' && p->data[p->pos] != '\r') {
            p->pos++;
        }
    }
}

/* ==================================================================
 * JSON 便利查询函数实现
 * ================================================================== */

bool lv_json_get_string(const char *json, const char *key, char *out, size_t out_size) {
    if (!json || !key || !out || out_size == 0) return false;
    const char *val = lv_json_find_key(json, key, strlen(key));
    if (!val) return false;

    lvJsonParser p;
    lv_json_parser_init(&p, val, strlen(val));
    lv_json_skip_ws(&p);
    char *str = lv_json_parse_string(&p);
    if (!str) return false;

    size_t len = strlen(str);
    bool ok = len < out_size;
    if (ok) {
        memcpy(out, str, len + 1);
    }
    lv_free((void **)&str);
    return ok;
}

bool lv_json_get_int(const char *json, const char *key, int *out) {
    if (!json || !key || !out) return false;
    const char *val = lv_json_find_key(json, key, strlen(key));
    if (!val) return false;

    lvJsonParser p;
    lv_json_parser_init(&p, val, strlen(val));
    return lv_json_parse_int(&p, out);
}

bool lv_json_get_double(const char *json, const char *key, double *out) {
    if (!json || !key || !out) return false;
    const char *val = lv_json_find_key(json, key, strlen(key));
    if (!val) return false;

    lvJsonParser p;
    lv_json_parser_init(&p, val, strlen(val));
    return lv_json_parse_double(&p, out);
}

bool lv_json_get_bool(const char *json, const char *key, bool *out) {
    if (!json || !key || !out) return false;
    const char *val = lv_json_find_key(json, key, strlen(key));
    if (!val) return false;

    lvJsonParser p;
    lv_json_parser_init(&p, val, strlen(val));
    return lv_json_parse_bool(&p, out);
}

/* ==================================================================
 * JSON 写入器实现
 * ================================================================== */

bool lv_json_buf_init(lvJsonBuf *buf, size_t initial_size) {
    buf->capacity = initial_size;
    buf->pos = 0;
    buf->buffer = lv_malloc(initial_size);
    if (!buf->buffer)
        return false;
    buf->buffer[0] = '\0';
    return true;
}

static void lv_json_buf_grow(lvJsonBuf *buf) {
    size_t old_capacity = buf->capacity;
    /* 统一扩容（capacity 以 int 镜像传递，倍增溢出由内部检查兜底） */
    int cap = (int) old_capacity;
    if (!lv_ensure_capacity((void **) &buf->buffer, cap, &cap, sizeof(char), cap)) {
        buf->capacity = old_capacity; /* 失败回滚旧容量 */
        return;
    }
    buf->capacity = (size_t) cap;
}

void lv_json_buf_ensure(lvJsonBuf *buf, size_t extra) {
    while (buf->pos + extra >= buf->capacity) {
        lv_json_buf_grow(buf);
    }
}

void lv_json_buf_append_string(lvJsonBuf *buf, const char *str) {
    if (!str) {
        lv_json_buf_append_raw(buf, "null");
        return;
    }

    /* 计算转义后长度：初始 2 字节给引号 */
    size_t escaped_len = 2;
    for (const char *s = str; *s; s++) {
        /* 查找表：转义字符 → 长度增量（0 表示未命中，走 default） */
        unsigned char inc = s_json_escape_len[(unsigned char)*s];
        if (inc > 0) {
            escaped_len += inc;
        } else {
            /* default：其他控制字符 \u00XX 占 6 字节，其余原样 1 字节 */
            if ((unsigned char)*s < 0x20)
                escaped_len += 6;
            else
                escaped_len += 1;
        }
    }

    lv_json_buf_ensure(buf, escaped_len + 1);

    buf->buffer[buf->pos++] = '"';
    for (const char *s = str; *s; s++) {
        unsigned char c = (unsigned char)*s;
        /* 查找表：转义字符 → 转义对字符串（NULL 表示未命中，走 default） */
        const char *pair = s_json_escape_pairs[c];
        if (pair) {
            buf->buffer[buf->pos++] = pair[0];
            buf->buffer[buf->pos++] = pair[1];
        } else {
            /* default：其他控制字符：\u00XX，其余原样写入 */
            if (c < 0x20) {
                buf->pos += (size_t)snprintf(buf->buffer + buf->pos,
                                              buf->capacity - buf->pos,
                                              "\\u%04x", c);
            } else {
                buf->buffer[buf->pos++] = (char)c;
            }
        }
    }
    buf->buffer[buf->pos++] = '"';
    buf->buffer[buf->pos] = '\0';
}

void lv_json_buf_append_raw(lvJsonBuf *buf, const char *str) {
    size_t len = strlen(str);
    while (buf->pos + len + 1 >= buf->capacity) {
        lv_json_buf_grow(buf);
    }
    memcpy(buf->buffer + buf->pos, str, len + 1);
    buf->pos += len;
}

void lv_json_buf_append_char(lvJsonBuf *buf, char c) {
    if (buf->pos + 2 >= buf->capacity) {
        lv_json_buf_grow(buf);
    }
    buf->buffer[buf->pos++] = c;
    buf->buffer[buf->pos] = '\0';
}

void lv_json_buf_append_fmt(lvJsonBuf *buf, const char *fmt, ...) {
    /* 先计算所需大小 */
    va_list args;
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed < 0)
        return;

    size_t needed_size = (size_t)needed + 1;
    lv_json_buf_ensure(buf, needed_size);

    va_start(args, fmt);
    vsnprintf(buf->buffer + buf->pos, buf->capacity - buf->pos, fmt, args);
    va_end(args);

    buf->pos += (size_t)needed;
}

char *lv_json_buf_finalize(lvJsonBuf *buf) {
    char *result = buf->buffer;
    (void)buf;
    return result;
}

void lv_json_buf_free(lvJsonBuf *buf) {
    if (buf && buf->buffer) {
        lv_free((void **)&buf->buffer);
        buf->buffer = NULL;
        buf->capacity = 0;
        buf->pos = 0;
    }
}
