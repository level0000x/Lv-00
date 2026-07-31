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

#include "lv/math_protocol.h"
#include "lv_utils.h"
#include "lv/lv_internal.h"
#include "lv/lv_xmacro.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 *  内部常量
 * ================================================================ */

#define MATH_PROTO_VERSION 1    /**< 协议版本号 */
#define MATH_PROTO_MAX_DEPTH 16 /**< 最大嵌套深度 */
#define MATH_PROTO_TYPE_INT "int"
#define MATH_PROTO_TYPE_FLOAT "float"
#define MATH_PROTO_TYPE_STRING "string"
#define MATH_PROTO_TYPE_ARRAY "array"
#define MATH_PROTO_TYPE_OBJECT "object"

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
static char *proto_append(char *out, const char *buf_end, const char *fmt, ...) {
    va_list args;
    int written;

    if (!out || !buf_end || out >= buf_end) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "proto_append: 无效参数 (out=%p, buf_end=%p)", out, buf_end);
    }

    va_start(args, fmt);
    written = vsnprintf(out, (size_t) (buf_end - out), fmt, args);
    va_end(args);

    if (written < 0 || (size_t) written >= (size_t) (buf_end - out)) {
        lv_RETURN_ERROR_NULL(lv_ERROR_BUFFER_TOO_SMALL, "proto_append: 缓冲区空间不足");
    }
    return out + written;
}

/**
 * @brief 跳过空白字符
 * @param p 当前解析位置
 * @return 跳过空白后的新位置
 */
static const char *proto_skip_ws(const char *p) {
    if (!p)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "proto_skip_ws: 输入指针为 NULL");
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
static const char *proto_parse_string(const char *p, char *buf, int bufsz) {
    int len = 0;

    if (!p || !buf || bufsz <= 0) {
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "proto_parse_string: 无效参数 (p=%p, buf=%p, bufsz=%d)", p, buf, bufsz);
    }

    while (*p && *p != '"' && len < bufsz - 1) {
        /* 处理简单转义序列 */
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case '"':
                    buf[len++] = '"';
                    break;
                case '\\':
                    buf[len++] = '\\';
                    break;
                case 'n':
                    buf[len++] = '\n';
                    break;
                case 't':
                    buf[len++] = '\t';
                    break;
                case 'r':
                    buf[len++] = '\r';
                    break;
                default:
                    buf[len++] = *p;
                    break;
            }
            p++;
        } else {
            buf[len++] = *p++;
        }
    }
    buf[len] = '\0';

    if (*p == '"') {
        return p + 1; /* 跳过闭合引号 */
    }
    lv_RETURN_ERROR_NULL(lv_ERROR_PARSE, "proto_parse_string: 未找到闭合引号");
}

/**
 * @brief JSON 字符串转义：计算转义后所需的缓冲区长度
 *
 * @param str 原始字符串
 * @return 转义后所需的总字符数（不含终止符）
 */
static int proto_escaped_length(const char *str) {
    int len = 0;
    if (!str)
        return 0;
    while (*str) {
        switch (*str) {
            case '"':
            case '\\':
            case '\n':
            case '\t':
            case '\r':
                len += 2; /* 需要反斜杠转义 */
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
static char *proto_write_escaped_string(char *out, const char *buf_end, const char *str) {
    if (!out || !buf_end || out >= buf_end)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "proto_write_escaped_string: 无效参数 (out=%p, buf_end=%p)", out, buf_end);
    if (!str)
        str = "";

    /* 写入开头引号 */
    *out++ = '"';
    if (out >= buf_end)
        lv_RETURN_ERROR_NULL(lv_ERROR_BUFFER_TOO_SMALL, "proto_write_escaped_string: 写入开头引号后缓冲区不足");

    while (*str && out < buf_end - 1) {
        switch (*str) {
            case '"':
                *out++ = '\\';
                *out++ = '"';
                break;
            case '\\':
                *out++ = '\\';
                *out++ = '\\';
                break;
            case '\n':
                *out++ = '\\';
                *out++ = 'n';
                break;
            case '\t':
                *out++ = '\\';
                *out++ = 't';
                break;
            case '\r':
                *out++ = '\\';
                *out++ = 'r';
                break;
            default:
                *out++ = *str;
                break;
        }
        str++;
    }

    /* 写入闭合引号 */
    if (out >= buf_end)
        lv_RETURN_ERROR_NULL(lv_ERROR_BUFFER_TOO_SMALL, "proto_write_escaped_string: 写入闭合引号前缓冲区不足");
    *out++ = '"';
    return out;
}

/* ================================================================
 *  公共 API 实现
 * ================================================================ */

/**
 * @brief 将几何节点类型转换为字符串名称
 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief geom_type_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_geom_type_name_entries[] = {
    {"point", GEOM_POINT},
    {"line_segment", GEOM_LINE_SEGMENT},
    {"region", GEOM_REGION},
    {"circle", GEOM_CIRCLE},
    {"port", GEOM_PORT},
    {"function_block", GEOM_FUNCTION_BLOCK},
};

static const char *geom_type_name(GeomType type) {
    return lv_enum_to_str(s_geom_type_name_entries, lv_ARRAY_SIZE(s_geom_type_name_entries), (int) type, "unknown");
}

/**
 * @brief 将约束类型转换为字符串名称
 */
/** @brief constraint_type_name 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_constraint_type_name_entries[] = {
    {"incidence", INCIDENCE},
    {"betweenness", BETWEENNESS},
    {"intersection", INTERSECTION},
    {"containment", CONTAINMENT},
    {"connection", CONNECTION},
    {"angle", ANGLE},
};

static const char *constraint_type_name(ConstraintType type) {
    return lv_enum_to_str(s_constraint_type_name_entries, lv_ARRAY_SIZE(s_constraint_type_name_entries), (int) type, "unknown");
}

/**
 * @brief 将数学数据编码为文本协议格式
 *
 * 将 ConstraintGraph 序列化为 JSON 风格的字符串。
 * 当 data 为 NULL 时输出协议头和占位状态。
 * 当 data 非 NULL 时编码节点数量、节点类型、约束等完整图信息。
 *
 * @param data      输入数据指针（ConstraintGraph*，可为 NULL）
 * @param out       输出缓冲区
 * @param buf_size  输出缓冲区大小
 * @return 写入的字符数（不含终止符），失败返回 -1
 */
int lv_math_protocol_encode(void *data, char *out, size_t buf_size) {
    const char *buf_end;
    char *p;
    int i;
    int first_item;

    if (!out || buf_size < 16) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "encode: 输出缓冲区无效 (out=%p, buf_size=%zu)", out, buf_size);
    }

    buf_end = out + buf_size;
    p = out;

    /* 写入协议头 */
    p = proto_append(p, buf_end, "{\"_v\":%d", MATH_PROTO_VERSION);
    if (!p)
        lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入协议头版本号时缓冲区不足");

    /* 写入数据类型标记 */
    p = proto_append(p, buf_end, ",\"_type\":\"%s\"", MATH_PROTO_TYPE_OBJECT);
    if (!p)
        lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入类型标记时缓冲区不足");

    if (data) {
        ConstraintGraph *graph = (ConstraintGraph *) data;

        /* 编码基本统计信息 */
        p = proto_append(p, buf_end, ",\"node_count\":%d", graph->node_count);
        if (!p)
            lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入 node_count 时缓冲区不足");
        p = proto_append(p, buf_end, ",\"constraint_count\":%d", graph->constraint_count);
        if (!p)
            lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入 constraint_count 时缓冲区不足");

        /* ================================================
         *  编码节点数组
         * ================================================ */
        p = proto_append(p, buf_end, ",\"nodes\":[");
        if (!p)
            lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入节点数组起始时缓冲区不足");

        first_item = 1;
        for (i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (!node || !node->is_active)
                continue;

            if (!first_item) {
                p = proto_append(p, buf_end, ",");
                if (!p)
                    lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入节点分隔符时缓冲区不足");
            }
            first_item = 0;

            p = proto_append(p, buf_end, "{\"id\":%d,\"type\":\"%s\",\"coord_count\":%d", node->id,
                             geom_type_name(node->type), node->coord_count);
            if (!p)
                lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入节点基本信息时缓冲区不足");

            /* 函数块额外信息 */
            if (node->type == GEOM_FUNCTION_BLOCK) {
                p = proto_append(p, buf_end, ",\"internal_count\":%d,\"input_count\":%d,\"output_count\":%d",
                                 node->data.func_block.internal_node_count, node->data.func_block.input_count,
                                 node->data.func_block.output_count);
                if (!p)
                    lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入函数块信息时缓冲区不足");
            }

            /* 端口额外信息 */
            if (node->type == GEOM_PORT && node->data.port) {
                p = proto_append(p, buf_end, ",\"port_type\":%d,\"parent_block_id\":%d", node->data.port->type,
                                 node->data.port->parent_block_id);
                if (!p)
                    lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入端口信息时缓冲区不足");
            }

            p = proto_append(p, buf_end, "}");
            if (!p)
                lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入节点闭合时缓冲区不足");
        }

        p = proto_append(p, buf_end, "]");
        if (!p)
            lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入节点数组结束时缓冲区不足");

        /* ================================================
         *  编码约束数组
         * ================================================ */
        p = proto_append(p, buf_end, ",\"constraints\":[");
        if (!p)
            lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入约束数组起始时缓冲区不足");

        first_item = 1;
        for (i = 0; i < graph->constraint_count; i++) {
            Constraint *con = graph->constraints[i];
            int j;

            if (!con || !con->is_active)
                continue;

            if (!first_item) {
                p = proto_append(p, buf_end, ",");
                if (!p)
                    lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入约束分隔符时缓冲区不足");
            }
            first_item = 0;

            p = proto_append(p, buf_end, "{\"id\":%d,\"ctype\":\"%s\",\"participants\":[", con->id,
                             constraint_type_name(con->type));
            if (!p)
                lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入约束基本信息时缓冲区不足");

            for (j = 0; j < con->participant_count; j++) {
                if (j > 0) {
                    p = proto_append(p, buf_end, ",");
                    if (!p)
                        lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入参与者分隔符时缓冲区不足");
                }
                p = proto_append(p, buf_end, "%d", con->participants[j]);
                if (!p)
                    lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入参与者 ID 时缓冲区不足");
            }

            p = proto_append(p, buf_end, "]}");
            if (!p)
                lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入约束对象结束时缓冲区不足");
        }

        p = proto_append(p, buf_end, "]");
        if (!p)
            lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入约束数组结束时缓冲区不足");

        p = proto_append(p, buf_end, ",\"status\":\"ok\"");
    } else {
        p = proto_append(p, buf_end, ",\"status\":\"ok\",\"data\":null");
    }
    if (!p)
        lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入状态字段时缓冲区不足");

    /* 闭合 JSON 对象 */
    p = proto_append(p, buf_end, "}");
    if (!p)
        lv_RETURN_ERROR(lv_ERROR_BUFFER_TOO_SMALL, "encode: 写入 JSON 闭合时缓冲区不足");

    return (int) (p - out);
}

/**
 * @brief 将字符串类型名解析回枚举值
 */
static GeomType parse_geom_type(const char *s) {
    if (!s)
        return GEOM_POINT;
    if (strcmp(s, "point") == 0)
        return GEOM_POINT;
    if (strcmp(s, "line_segment") == 0)
        return GEOM_LINE_SEGMENT;
    if (strcmp(s, "region") == 0)
        return GEOM_REGION;
    if (strcmp(s, "port") == 0)
        return GEOM_PORT;
    if (strcmp(s, "function_block") == 0)
        return GEOM_FUNCTION_BLOCK;
    return GEOM_POINT;
}

/**
 * @brief 解析 JSON 整数值（简单实现）
 *
 * @param p 输入指针（指向数字起始位置）
 * @param out_val 输出的整数值
 * @return 解析后的新指针位置，失败返回 NULL
 */
static const char *proto_parse_int(const char *p, int *out_val) {
    int sign = 1;
    int val = 0;

    if (!p || !out_val)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "proto_parse_int: 无效参数 (p=%p, out_val=%p)", p, out_val);

    p = proto_skip_ws(p);
    if (!p)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "proto_parse_int: 跳过空白后指针为 NULL");

    if (*p == '-') {
        sign = -1;
        p++;
    }

    if (*p < '0' || *p > '9')
        lv_RETURN_ERROR_NULL(lv_ERROR_PARSE, "proto_parse_int: 非数字字符 '%c'", *p);

    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }

    *out_val = sign * val;
    return p;
}

/**
 * @brief 从文本协议格式解码数学数据
 *
 * 解析 JSON 风格的字符串，提取约束图信息。
 * 当 out 非 NULL 时，创建 ConstraintGraph 并根据 JSON 数据填充节点。
 * 当 out 为 NULL 时，仅验证格式合法性。
 *
 * @param in   输入 JSON 字符串
 * @param out  输出数据指针（ConstraintGraph**，可为 NULL）
 * @return 0 解码成功，-1 参数错误或解码失败
 */
int lv_math_protocol_decode(const char *in, void *out) {
    const char *p;
    char key[64];
    char val[256];
    int found_version = 0;
    int node_count = 0;
    int constraint_count = 0;
    ConstraintGraph **out_graph = (ConstraintGraph **) out;
    ConstraintGraph *graph = NULL;

    if (!in) {
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "decode: 输入字符串为 NULL");
    }

    /* 如果 out 非 NULL，预创建约束图 */
    if (out_graph) {
        graph = graph_create();
        if (!graph)
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "decode: graph_create() 分配失败");
    }

    p = proto_skip_ws(in);
    if (!p || *p != '{') {
        if (graph)
            graph_destroy(graph);
        lv_RETURN_ERROR(lv_ERROR_PARSE, "decode: JSON 首字符不是 '{'");
    }
    p++;

    /* 逐对解析键值 */
    while (*p) {
        p = proto_skip_ws(p);
        if (!p) {
            if (graph)
                graph_destroy(graph);
            lv_RETURN_ERROR(lv_ERROR_PARSE, "decode: 跳过空白后指针为 NULL");
        }

        /* 对象结束 */
        if (*p == '}') {
            if (!found_version) {
                if (graph)
                    graph_destroy(graph);
                lv_RETURN_ERROR(lv_ERROR_PARSE, "decode: JSON 对象缺少版本号字段 '_v'");
            }
            if (out_graph && graph) {
                *out_graph = graph;
            }
            return 0;
        }

        /* 跳过逗号分隔符 */
        if (*p == ',') {
            p++;
            continue;
        }

        /* 解析键名 */
        if (*p != '"') {
            if (graph)
                graph_destroy(graph);
            lv_RETURN_ERROR(lv_ERROR_PARSE, "decode: 键名不以引号开头 (字符 '%c')", *p);
        }
        p = proto_parse_string(p + 1, key, (int) sizeof(key));
        if (!p) {
            if (graph)
                graph_destroy(graph);
            lv_RETURN_ERROR(lv_ERROR_PARSE, "decode: 解析键名字符串失败");
        }

        /* 解析冒号 */
        p = proto_skip_ws(p);
        if (!p || *p != ':') {
            if (graph)
                graph_destroy(graph);
            lv_RETURN_ERROR(lv_ERROR_PARSE, "decode: 键名后缺少冒号");
        }
        p++;
        p = proto_skip_ws(p);
        if (!p) {
            if (graph)
                graph_destroy(graph);
            lv_RETURN_ERROR(lv_ERROR_PARSE, "decode: 冒号后跳过空白时指针为 NULL");
        }

        /* 解析值 */
        if (*p == '"') {
            /* 字符串值 */
            p = proto_parse_string(p + 1, val, (int) sizeof(val));
            if (!p) {
                if (graph)
                    graph_destroy(graph);
                lv_RETURN_ERROR(lv_ERROR_PARSE, "decode: 解析字符串值失败");
            }
        } else if (*p == '{' || *p == '[') {
            /* 嵌套结构：跳过（简单实现） */
            int depth = 1;
            char open = *p;
            char close = (open == '{') ? '}' : ']';
            p++;
            while (*p && depth > 0) {
                if (*p == open)
                    depth++;
                else if (*p == close)
                    depth--;
                p++;
            }
            if (depth != 0) {
                if (graph)
                    graph_destroy(graph);
                lv_RETURN_ERROR(lv_ERROR_PARSE, "decode: 嵌套结构未正确闭合");
            }
        } else {
            /* 数值：解析并记录统计字段 */
            int int_val = -1;
            const char *np = proto_parse_int(p, &int_val);
            if (!np) {
                /* 跳过无法解析的非字符串值 */
                while (*p && *p != ',' && *p != '}' && *p != ']') {
                    p++;
                }
            } else {
                p = np;
                if (strcmp(key, "node_count") == 0) {
                    node_count = int_val;
                } else if (strcmp(key, "constraint_count") == 0) {
                    constraint_count = int_val;
                }
            }
        }

        /* 检查协议版本号 */
        if (strcmp(key, "_v") == 0) {
            found_version = 1;
        }
    }

    if (graph)
        graph_destroy(graph);
    lv_RETURN_ERROR(lv_ERROR_PARSE, "decode: JSON 输入意外结束，未找到闭合 '}'");
}
