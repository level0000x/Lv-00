/**
 * @file math_protocol.c
 * @brief 数学协议编解码模块 —— Layer2 资源管理层
 *
 * 提供数学数据的编码与解码功能，支持将内部数学对象
 * 序列化为可传输的 JSON 文本格式，以及从 JSON 格式反序列化。
 *
 * 编码格式：基于 JSON 的轻量协议，支持基本类型和嵌套结构。
 *
 * @version 1.0.0
 */

#include "lv00/math_protocol.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 *  内部常量
 * ================================================================ */

#define MATH_PROTO_VERSION      1       /**< 协议版本号 */
#define MATH_PROTO_MAX_DEPTH    16      /**< 最大嵌套深度 */
#define MATH_PROTO_TYPE_INT     "int"
#define MATH_PROTO_TYPE_FLOAT   "float"
#define MATH_PROTO_TYPE_STRING  "string"
#define MATH_PROTO_TYPE_ARRAY   "array"
#define MATH_PROTO_TYPE_OBJECT  "object"

/* ================================================================
 *  内部辅助函数
 * ================================================================ */

/**
 * @brief 安全地向缓冲区追加格式化字符串
 *
 * @param out      输出缓冲区当前位置
 * @param buf_end  缓冲区末尾（含终止符空间）
 * @param fmt      printf 格式字符串
 * @return 追加后的指针位置，若缓冲区满则返回 NULL
 */
static char *proto_append(char *out, const char *buf_end, const char *fmt, ...)
{
    va_list args;
    int written;

    if (!out || !buf_end || out >= buf_end) {
        return NULL;
    }

    va_start(args, fmt);
    written = vsnprintf(out, (size_t)(buf_end - out), fmt, args);
    va_end(args);

    if (written < 0 || (size_t)written >= (size_t)(buf_end - out)) {
        return NULL;
    }
    return out + written;
}

/**
 * @brief 跳过空白字符
 * @param p 当前解析位置
 * @return 跳过空白后的新位置
 */
static const char *proto_skip_ws(const char *p)
{
    if (!p) return NULL;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    return p;
}

/**
 * @brief 解析 JSON 字符串值（简单实现，支持基本转义）
 *
 * @param p    输入指针（指向开头引号之后）
 * @param buf  输出缓冲区
 * @param bufsz 缓冲区大小
 * @return 解析后的新指针位置（指向闭合引号之后），失败返回 NULL
 */
static const char *proto_parse_string(const char *p, char *buf, int bufsz)
{
    int len = 0;

    if (!p || !buf || bufsz <= 0) {
        return NULL;
    }

    while (*p && *p != '"' && len < bufsz - 1) {
        /* 处理简单转义序列 */
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case '"':  buf[len++] = '"';  break;
                case '\\': buf[len++] = '\\'; break;
                case 'n':  buf[len++] = '\n'; break;
                case 't':  buf[len++] = '\t'; break;
                case 'r':  buf[len++] = '\r'; break;
                default:   buf[len++] = *p;   break;
            }
            p++;
        } else {
            buf[len++] = *p++;
        }
    }
    buf[len] = '\0';

    if (*p == '"') {
        return p + 1;  /* 跳过闭合引号 */
    }
    return NULL;  /* 未找到闭合引号 */
}

/**
 * @brief JSON 字符串转义：计算转义后所需的缓冲区长度
 *
 * @param str 原始字符串
 * @return 转义后所需的总字符数（不含终止符）
 */
static int proto_escaped_length(const char *str)
{
    int len = 0;
    if (!str) return 0;
    while (*str) {
        switch (*str) {
            case '"': case '\\': case '\n': case '\t': case '\r':
                len += 2;  /* 需要反斜杠转义 */
                break;
            default:
                len += 1;
                break;
        }
        str++;
    }
    return len;
}

/**
 * @brief 将字符串以 JSON 转义形式追加到缓冲区
 *
 * @param out     输出位置
 * @param buf_end 缓冲区末尾
 * @param str     要写入的字符串
 * @return 追加后的新位置，缓冲区不足返回 NULL
 */
static char *proto_write_escaped_string(char *out, const char *buf_end, const char *str)
{
    if (!out || !buf_end || out >= buf_end) return NULL;
    if (!str) str = "";

    /* 写入开头引号 */
    *out++ = '"';
    if (out >= buf_end) return NULL;

    while (*str && out < buf_end - 1) {
        switch (*str) {
            case '"':  *out++ = '\\'; *out++ = '"';  break;
            case '\\': *out++ = '\\'; *out++ = '\\'; break;
            case '\n': *out++ = '\\'; *out++ = 'n';  break;
            case '\t': *out++ = '\\'; *out++ = 't';  break;
            case '\r': *out++ = '\\'; *out++ = 'r';  break;
            default:   *out++ = *str; break;
        }
        str++;
    }

    /* 写入闭合引号 */
    if (out >= buf_end) return NULL;
    *out++ = '"';
    return out;
}

/* ================================================================
 *  公共 API 实现
 * ================================================================ */

/**
 * @brief 将数学数据编码为文本协议格式
 *
 * 将内部数据序列化为 JSON 风格的字符串。
 * 当 data 为 NULL 时输出协议头和状态信息。
 * 当 data 非 NULL 时（预留接口），输出协议头和数据摘要。
 *
 * @param data      输入数据指针（当前预留接口，传 NULL 有效）
 * @param out       输出缓冲区
 * @param buf_size  输出缓冲区大小
 * @return 写入的字符数（不含终止符），失败返回 -1
 */
static int lv00_math_protocol_encode(void *data, char *out, size_t buf_size)
{
    const char *buf_end;
    char *p;

    if (!out || buf_size < 16) {
        return -1;
    }

    buf_end = out + buf_size;
    p = out;

    /* 写入协议头 */
    p = proto_append(p, buf_end, "{\"_v\":%d", MATH_PROTO_VERSION);
    if (!p) return -1;

    /* 写入数据类型标记 */
    p = proto_append(p, buf_end, ",\"_type\":\"%s\"", MATH_PROTO_TYPE_OBJECT);
    if (!p) return -1;

    if (data) {
        /* data 非 NULL 时标记数据已填充（预留接口） */
        p = proto_append(p, buf_end, ",\"status\":\"ok\",\"data\":true");
    } else {
        /* data 为 NULL 时输出占位状态 */
        p = proto_append(p, buf_end, ",\"status\":\"ok\",\"data\":null");
    }
    if (!p) return -1;

    /* 闭合 JSON 对象 */
    p = proto_append(p, buf_end, "}");
    if (!p) return -1;

    return (int)(p - out);
}

/**
 * @brief 从文本协议格式解码数学数据
 *
 * 解析 JSON 风格的字符串，提取数学对象信息。
 * 验证协议版本号和基本结构合法性。
 *
 * @param in   输入 JSON 字符串
 * @param out  输出数据指针（当前预留接口，传 NULL 有效）
 * @return 0 解码成功，-1 参数错误或解码失败
 */
static int lv00_math_protocol_decode(const char *in, void *out)
{
    const char *p;
    char key[64];
    char val[256];
    int found_version = 0;

    if (!in) {
        return -1;
    }

    (void)out;  /* 预留：未来填充 out 结构 */

    p = proto_skip_ws(in);
    if (!p || *p != '{') {
        return -1;  /* 期望 JSON 对象起始 */
    }
    p++;

    /* 逐对解析键值 */
    while (*p) {
        p = proto_skip_ws(p);
        if (!p) return -1;

        /* 对象结束 */
        if (*p == '}') {
            if (!found_version) {
                return -1;  /* 缺少协议版本字段 */
            }
            return 0;  /* 正常结束 */
        }

        /* 跳过逗号分隔符 */
        if (*p == ',') {
            p++;
            continue;
        }

        /* 解析键名 */
        if (*p != '"') {
            return -1;  /* 期望键名起始引号 */
        }
        p = proto_parse_string(p + 1, key, (int)sizeof(key));
        if (!p) return -1;

        /* 解析冒号 */
        p = proto_skip_ws(p);
        if (!p || *p != ':') return -1;
        p++;
        p = proto_skip_ws(p);
        if (!p) return -1;

        /* 解析值 */
        if (*p == '"') {
            /* 字符串值 */
            p = proto_parse_string(p + 1, val, (int)sizeof(val));
            if (!p) return -1;
        } else if (*p == '{' || *p == '[') {
            /* 嵌套结构：跳过（简单实现） */
            int depth = 1;
            char open = *p;
            char close = (open == '{') ? '}' : ']';
            p++;
            while (*p && depth > 0) {
                if (*p == open) depth++;
                else if (*p == close) depth--;
                p++;
            }
            if (depth != 0) return -1;
        } else {
            /* 数字、布尔值、null：跳到下一个分隔符 */
            while (*p && *p != ',' && *p != '}' && *p != ']') {
                p++;
            }
        }

        /* 检查协议版本号 */
        if (strcmp(key, "_v") == 0) {
            found_version = 1;
        }
    }

    return -1;  /* 未找到闭合括号 */
}
