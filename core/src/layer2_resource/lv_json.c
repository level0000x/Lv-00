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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_json.h"
#include "lv/lv_internal.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_str_utils.h"
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

bool lv_json_parse_int64(lvJsonParser *p, int64_t *out) {
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

    /* 用无符号累加避免有符号溢出未定义行为 */
    uint64_t val = 0;
    for (const char *s = p->data + start + (negative ? 1 : 0); s < p->data + p->pos; s++) {
        uint64_t digit = (uint64_t)(*s - '0');
        if (val > (UINT64_MAX - digit) / 10)
            return false; /* 溢出 */
        val = val * 10 + digit;
    }

    if (negative) {
        if (val > (uint64_t)INT64_MAX + 1)
            return false; /* 小于 INT64_MIN */
        *out = (val == (uint64_t)INT64_MAX + 1) ? INT64_MIN : -(int64_t)val;
    } else {
        if (val > (uint64_t)INT64_MAX)
            return false; /* 大于 INT64_MAX */
        *out = (int64_t)val;
    }
    return true;
}

bool lv_json_parse_uint64(lvJsonParser *p, uint64_t *out) {
    lv_json_skip_ws(p);
    size_t start = p->pos;

    if (p->pos < p->size && p->data[p->pos] == '-') {
        return false; /* 无符号数不接受负号 */
    }

    if (p->pos >= p->size || p->data[p->pos] < '0' || p->data[p->pos] > '9') {
        return false;
    }

    while (p->pos < p->size && p->data[p->pos] >= '0' && p->data[p->pos] <= '9') {
        p->pos++;
    }

    if (p->pos == start)
        return false;

    uint64_t val = 0;
    for (const char *s = p->data + start; s < p->data + p->pos; s++) {
        uint64_t digit = (uint64_t)(*s - '0');
        if (val > (UINT64_MAX - digit) / 10)
            return false; /* 溢出 */
        val = val * 10 + digit;
    }
    *out = val;
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

bool lv_json_parse_field(lvJsonParser *p, char **key) {
    *key = NULL;
    lv_json_skip_ws(p);
    if (p->pos >= p->size || p->data[p->pos] == '}')
        return false; /* 对象结束或输入末尾 */

    if (p->data[p->pos] == ',') {
        p->pos++; /* 消费上一字段后的逗号（尾部逗号在下一轮被 '}' 检查兜住） */
        lv_json_skip_ws(p);
        if (p->pos >= p->size || p->data[p->pos] == '}')
            return false; /* 尾部逗号容错 */
    }

    *key = lv_json_parse_string(p);
    if (!*key)
        return false; /* 键不是字符串：调用方 break，与历史行为一致 */

    lv_json_skip_ws(p);
    if (p->pos >= p->size || p->data[p->pos] != ':') {
        /* 缺冒号：释放 key，返回 false（调用方 break，与历史行为一致） */
        lv_free((void **) key);
        return false;
    }
    p->pos++; /* skip ':' */
    return true;
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

bool lv_json_parse_int_array(lvJsonParser *p, int *out, size_t max_count, size_t *out_count) {
    lv_json_skip_ws(p);
    if (out_count)
        *out_count = 0;
    if (p->pos >= p->size || p->data[p->pos] != '[')
        return false;
    p->pos++;

    size_t count = 0;
    for (;;) {
        lv_json_skip_ws(p);
        if (p->pos >= p->size)
            return false; /* 数组未闭合 */
        if (p->data[p->pos] == ']') {
            p->pos++;
            break; /* 正常结束 */
        }

        if (count < max_count) {
            int v = 0;
            if (!lv_json_parse_int(p, &v))
                return false;
            out[count++] = v;
        } else {
            /* 缓冲区已满（越界）：跳过剩余元素，不收集 */
            lv_json_skip_value(p);
        }

        lv_json_skip_ws(p);
        if (p->pos < p->size && p->data[p->pos] == ',') {
            p->pos++;
        } else if (p->pos < p->size && p->data[p->pos] == ']') {
            p->pos++;
            break;
        } else {
            return false; /* 期望 ',' 或 ']' */
        }
    }

    if (out_count)
        *out_count = count;
    return true;
}

bool lv_json_parse_double_array(lvJsonParser *p, double *out, size_t max_count, size_t *out_count) {
    lv_json_skip_ws(p);
    if (out_count)
        *out_count = 0;
    if (p->pos >= p->size || p->data[p->pos] != '[')
        return false;
    p->pos++;

    size_t count = 0;
    for (;;) {
        lv_json_skip_ws(p);
        if (p->pos >= p->size)
            return false; /* 数组未闭合 */
        if (p->data[p->pos] == ']') {
            p->pos++;
            break; /* 正常结束 */
        }

        if (count < max_count) {
            double v = 0.0;
            if (!lv_json_parse_double(p, &v))
                return false;
            out[count++] = v;
        } else {
            /* 缓冲区已满（越界）：跳过剩余元素，不收集 */
            lv_json_skip_value(p);
        }

        lv_json_skip_ws(p);
        if (p->pos < p->size && p->data[p->pos] == ',') {
            p->pos++;
        } else if (p->pos < p->size && p->data[p->pos] == ']') {
            p->pos++;
            break;
        } else {
            return false; /* 期望 ',' 或 ']' */
        }
    }

    if (out_count)
        *out_count = count;
    return true;
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
    /* 对象级 API 状态：根容器为空、紧凑模式、无待定键 */
    buf->depth = 0;
    buf->has_elem = false;
    buf->key_pending = false;
    buf->pretty = false;
    buf->key_space = false;
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

/* 写入带引号且已转义的 JSON 字符串（str 非 NULL；供 append_string / append_key 复用） */
static void json_buf_append_quoted(lvJsonBuf *buf, const char *str) {
    /* 转义统一走公共 API lv_str_json_escape（两遍法；转义表唯一收敛于 lv_str_utils.c，
     * 与 lv_str_json_escape_alloc 等输出格式一致：\" \\ \n \t \r \b \f 及 \u00XX 控制字符） */
    size_t need = lv_str_json_escape(str, strlen(str), NULL, 0);

    /* 2 个引号 + 转义内容 + NUL */
    lv_json_buf_ensure(buf, need + 3);

    buf->buffer[buf->pos++] = '"';
    lv_str_json_escape(str, strlen(str), buf->buffer + buf->pos, buf->capacity - buf->pos);
    buf->pos += need;
    buf->buffer[buf->pos++] = '"';
    buf->buffer[buf->pos] = '\0';
}

void lv_json_buf_append_string(lvJsonBuf *buf, const char *str) {
    if (buf && buf->key_pending)
        buf->key_pending = false; /* 作为 append_key 的紧邻值：消费键状态（旧调用中恒为 false，零影响） */
    if (!str) {
        lv_json_buf_append_raw(buf, "null");
        return;
    }
    json_buf_append_quoted(buf, str);
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
        buf->depth = 0;
        buf->has_elem = false;
        buf->key_pending = false;
        buf->pretty = false;
    }
}

/* ==================================================================
 * JSON 对象级写入 API 实现
 *
 * 状态机约定：
 *   - depth      当前打开容器数（begin 增 / end 减），缩进输出上限 64 级；
 *   - has_elem   当前容器是否已写入元素（决定元素前是否加逗号）；
 *   - key_pending append_key 后等待值写入：值/begin 时仅清除标志，
 *                 不再追加逗号/换行（该工作已由 append_key 完成）。
 * 所有"值/键/begin"入口都先经 json_buf_begin_value 统一处理分隔符。
 * ================================================================== */

void lv_json_buf_set_pretty(lvJsonBuf *buf, bool pretty) {
    if (buf)
        buf->pretty = pretty;
}

void lv_json_buf_set_key_space(lvJsonBuf *buf, bool on) {
    if (buf)
        buf->key_space = on;
}

/* 按嵌套深度输出缩进（2 空格/级，超过 64 层按 64 层处理防溢出） */
static void json_buf_write_indent(lvJsonBuf *buf, unsigned depth) {
    if (depth > 64u)
        depth = 64u;
    for (unsigned i = 0; i < depth; i++)
        lv_json_buf_append_raw(buf, "  ");
}

/* 元素写入前的分隔处理：逗号、换行、缩进与状态维护 */
static void json_buf_begin_value(lvJsonBuf *buf) {
    if (buf->key_pending) {
        /* 值为 append_key 的紧邻值：分隔符已由 append_key 输出 */
        buf->key_pending = false;
        return;
    }
    if (buf->has_elem) {
        lv_json_buf_append_char(buf, ',');
        if (buf->pretty) {
            lv_json_buf_append_char(buf, '\n');
            json_buf_write_indent(buf, buf->depth);
        }
    } else if (buf->pretty && buf->depth > 0u) {
        /* 容器内第一个元素：换行 + 缩进；顶层根（depth==0）不换行 */
        lv_json_buf_append_char(buf, '\n');
        json_buf_write_indent(buf, buf->depth);
    }
    buf->has_elem = true;
}

bool lv_json_buf_begin_object(lvJsonBuf *buf) {
    if (!buf)
        return false;
    json_buf_begin_value(buf);
    lv_json_buf_append_char(buf, '{');
    buf->depth++;
    buf->has_elem = false; /* 新容器为空 */
    return true;
}

bool lv_json_buf_end_object(lvJsonBuf *buf) {
    if (!buf)
        return false;
    if (buf->pretty && buf->has_elem) {
        lv_json_buf_append_char(buf, '\n');
        json_buf_write_indent(buf, buf->depth > 0u ? buf->depth - 1u : 0u);
    }
    lv_json_buf_append_char(buf, '}');
    if (buf->depth > 0u)
        buf->depth--;
    /* 回到父容器：该容器本身就是一个元素 */
    buf->has_elem = true;
    buf->key_pending = false;
    return true;
}

bool lv_json_buf_begin_array(lvJsonBuf *buf) {
    if (!buf)
        return false;
    json_buf_begin_value(buf);
    lv_json_buf_append_char(buf, '[');
    buf->depth++;
    buf->has_elem = false; /* 新容器为空 */
    return true;
}

bool lv_json_buf_end_array(lvJsonBuf *buf) {
    if (!buf)
        return false;
    if (buf->pretty && buf->has_elem) {
        lv_json_buf_append_char(buf, '\n');
        json_buf_write_indent(buf, buf->depth > 0u ? buf->depth - 1u : 0u);
    }
    lv_json_buf_append_char(buf, ']');
    if (buf->depth > 0u)
        buf->depth--;
    buf->has_elem = true;
    buf->key_pending = false;
    return true;
}

bool lv_json_buf_append_key(lvJsonBuf *buf, const char *key) {
    if (!buf || !key)
        return false;
    json_buf_begin_value(buf);
    json_buf_append_quoted(buf, key);
    lv_json_buf_append_char(buf, ':');
    if (buf->key_space)
        lv_json_buf_append_char(buf, ' ');
    buf->key_pending = true;
    return true;
}

bool lv_json_buf_append_int(lvJsonBuf *buf, long long v) {
    if (!buf)
        return false;
    json_buf_begin_value(buf);
    char num[32];
    snprintf(num, sizeof(num), "%lld", v);
    lv_json_buf_append_raw(buf, num);
    return true;
}

bool lv_json_buf_append_double(lvJsonBuf *buf, double v) {
    if (!buf)
        return false;
    json_buf_begin_value(buf);
    /* 与 graph_serialize.c numeric_value 序列化风格一致 */
    char num[64];
    snprintf(num, sizeof(num), "%.15g", v);
    lv_json_buf_append_raw(buf, num);
    return true;
}

bool lv_json_buf_append_bool(lvJsonBuf *buf, bool v) {
    if (!buf)
        return false;
    json_buf_begin_value(buf);
    lv_json_buf_append_raw(buf, v ? "true" : "false");
    return true;
}

bool lv_json_buf_append_null(lvJsonBuf *buf) {
    if (!buf)
        return false;
    json_buf_begin_value(buf);
    lv_json_buf_append_raw(buf, "null");
    return true;
}
