/**
 * @file math_protocol.c
 * @brief 数学协议模块（子目录版本）
 *
 * 提供数学数据的编码与解码功能，支持将内部数学对象
 * 序列化为可传输的文本格式，以及从文本格式反序列化。
 *
 * 编码格式：基于 JSON 的轻量协议，支持基本类型和嵌套结构。
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
 * @brief 安全地向缓冲区追加字符串
 *
 * @param out      输出缓冲区当前位置
 * @param buf_end  缓冲区末尾
 * @param fmt      printf 格式字符串
 * @param ...      格式参数
 * @return 追加后的指针位置，若缓冲区满则返回 NULL
 */
static char *proto_append(char *out, const char *buf_end, const char *fmt, ...)
{
    va_list args;
    int written;

    if (out >= buf_end) {
        return NULL;
    }

    va_start(args, fmt);
    written = vsnprintf(out, (size_t)(buf_end - out), fmt, args);
    va_end(args);

    if (written < 0 || out + written >= buf_end) {
        return NULL;
    }
    return out + written;
}

/**
 * @brief 跳过空白字符
 */
static const char *proto_skip_ws(const char *p)
{
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    return p;
}

/**
 * @brief 解析 JSON 字符串值（简单实现，不处理转义）
 *
 * @param p    输入指针（指向开头引号之后）
 * @param buf  输出缓冲区
 * @param bufsz 缓冲区大小
 * @return 解析后的新指针位置（指向闭合引号之后），失败返回 NULL
 */
static const char *proto_parse_string(const char *p, char *buf, int bufsz)
{
    int len = 0;
    while (*p && *p != '"' && len < bufsz - 1) {
        buf[len++] = *p++;
    }
    buf[len] = '\0';
    if (*p == '"') {
        return p + 1;
    }
    return NULL;
}

/* ================================================================
 *  公共 API 实现
 * ================================================================ */

/**
 * @brief 将数学数据编码为文本协议格式
 *
 * 将内部数据序列化为 JSON 风格的字符串。
 * 当前实现为占位编码，输出协议版本和数据类型信息。
 *
 * @param data      输入数据指针（当前未使用，预留接口）
 * @param out       输出缓冲区
 * @param buf_size  输出缓冲区大小
 * @return 写入的字符数（不含终止符），失败返回 -1
 */
int lv00_math_protocol_encode(void *data, char *out, size_t buf_size)
{
    const char *buf_end;
    char *p;

    if (!out || buf_size < 16) {
        return -1;
    }

    (void)data;  /* 预留：未来从 data 结构中提取字段 */

    buf_end = out + buf_size;
    p = out;

    /* 写入协议头 */
    p = proto_append(p, buf_end, "{\"_v\":%d", MATH_PROTO_VERSION);
    if (!p) return -1;

    /* 写入数据类型标记 */
    p = proto_append(p, buf_end, ",\"_type\":\"%s\"", MATH_PROTO_TYPE_OBJECT);
    if (!p) return -1;

    /* 写入占位内容 */
    p = proto_append(p, buf_end, ",\"status\":\"ok\"}");
    if (!p) return -1;

    return (int)(p - out);
}

/**
 * @brief 从文本协议格式解码数学数据
 *
 * 解析 JSON 风格的字符串，提取数学对象信息。
 * 当前实现为占位解码，仅验证格式合法性。
 *
 * @param in   输入字符串
 * @param out  输出数据指针（当前未使用，预留接口）
 * @return 0 解码成功，-1 参数错误或解码失败
 */
int lv00_math_protocol_decode(const char *in, void *out)
{
    const char *p;
    char key[64];
    char val[256];

    if (!in) {
        return -1;
    }

    (void)out;  /* 预留：未来填充 out 结构 */

    p = proto_skip_ws(in);
    if (*p != '{') {
        return -1;  /* 期望 JSON 对象起始 */
    }
    p++;

    /* 简单解析键值对 */
    while (*p) {
        p = proto_skip_ws(p);
        if (*p == '}') {
            return 0;  /* 正常结束 */
        }
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p != '"') {
            return -1;  /* 期望键名 */
        }
        p = proto_parse_string(p + 1, key, (int)sizeof(key));
        if (!p) return -1;

        p = proto_skip_ws(p);
        if (*p != ':') return -1;
        p++;
        p = proto_skip_ws(p);

        /* 解析值 */
        if (*p == '"') {
            p = proto_parse_string(p + 1, val, (int)sizeof(val));
            if (!p) return -1;
        } else {
            /* 跳过非字符串值（数字、布尔等） */
            while (*p && *p != ',' && *p != '}') {
                p++;
            }
        }
    }

    return -1;  /* 未找到闭合括号 */
}
