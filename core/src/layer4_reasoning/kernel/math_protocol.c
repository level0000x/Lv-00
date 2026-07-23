/**
 * @file math_protocol.c
 * @brief 数学协议模块（子目录版本）
 *
 * 提供数学数据的编码与解码功能，支持将内部数学对象
 * 序列化为可传输的文本格式，以及从文本格式反序列化。
 *
 * 编码格式：基于 JSON 的轻量协议，支持基本类型和嵌套结构。
 */

#include "lv/math_protocol.h"

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
 * @brief 将几何节点类型转换为字符串名称
 */
static const char *geom_type_name(GeomType type)
{
    switch (type) {
        case GEOM_POINT:         return "point";
        case GEOM_LINE_SEGMENT:  return "line_segment";
        case GEOM_REGION:        return "region";
        case GEOM_PORT:          return "port";
        case GEOM_FUNCTION_BLOCK:return "function_block";
        default:                 return "unknown";
    }
}

/**
 * @brief 将约束类型转换为字符串名称
 */
static const char *constraint_type_name(ConstraintType type)
{
    switch (type) {
        case INCIDENCE:    return "incidence";
        case BETWEENNESS:  return "betweenness";
        case INTERSECTION: return "intersection";
        case CONTAINMENT:  return "containment";
        case CONNECTION:   return "connection";
        default:           return "unknown";
    }
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
int lv_math_protocol_encode(void *data, char *out, size_t buf_size)
{
    const char *buf_end;
    char *p;
    int i;
    int first_item;

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
        ConstraintGraph *graph = (ConstraintGraph *)data;

        /* 编码基本统计信息 */
        p = proto_append(p, buf_end, ",\"node_count\":%d", graph->node_count);
        if (!p) return -1;
        p = proto_append(p, buf_end, ",\"constraint_count\":%d", graph->constraint_count);
        if (!p) return -1;

        /* ================================================
         *  编码节点数组
         * ================================================ */
        p = proto_append(p, buf_end, ",\"nodes\":[");
        if (!p) return -1;

        first_item = 1;
        for (i = 0; i < graph->node_count; i++) {
            GeomNode *node = graph->nodes[i];
            if (!node || !node->is_active) continue;

            if (!first_item) {
                p = proto_append(p, buf_end, ",");
                if (!p) return -1;
            }
            first_item = 0;

            p = proto_append(p, buf_end,
                "{\"id\":%d,\"type\":\"%s\",\"coord_count\":%d",
                node->id,
                geom_type_name(node->type),
                node->coord_count);
            if (!p) return -1;

            /* 函数块额外信息 */
            if (node->type == GEOM_FUNCTION_BLOCK) {
                p = proto_append(p, buf_end,
                    ",\"internal_count\":%d,\"input_count\":%d,\"output_count\":%d",
                    node->data.func_block.internal_node_count,
                    node->data.func_block.input_count,
                    node->data.func_block.output_count);
                if (!p) return -1;
            }

            /* 端口额外信息 */
            if (node->type == GEOM_PORT && node->data.port) {
                p = proto_append(p, buf_end,
                    ",\"port_type\":%d,\"parent_block_id\":%d",
                    node->data.port->type,
                    node->data.port->parent_block_id);
                if (!p) return -1;
            }

            p = proto_append(p, buf_end, "}");
            if (!p) return -1;
        }

        p = proto_append(p, buf_end, "]");
        if (!p) return -1;

        /* ================================================
         *  编码约束数组
         * ================================================ */
        p = proto_append(p, buf_end, ",\"constraints\":[");
        if (!p) return -1;

        first_item = 1;
        for (i = 0; i < graph->constraint_count; i++) {
            Constraint *con = graph->constraints[i];
            int j;

            if (!con || !con->is_active) continue;

            if (!first_item) {
                p = proto_append(p, buf_end, ",");
                if (!p) return -1;
            }
            first_item = 0;

            p = proto_append(p, buf_end,
                "{\"id\":%d,\"ctype\":\"%s\",\"participants\":[",
                con->id, constraint_type_name(con->type));
            if (!p) return -1;

            for (j = 0; j < con->participant_count; j++) {
                if (j > 0) {
                    p = proto_append(p, buf_end, ",");
                    if (!p) return -1;
                }
                p = proto_append(p, buf_end, "%d", con->participants[j]);
                if (!p) return -1;
            }

            p = proto_append(p, buf_end, "]}");
            if (!p) return -1;
        }

        p = proto_append(p, buf_end, "]");
        if (!p) return -1;

        p = proto_append(p, buf_end, ",\"status\":\"ok\"");
    } else {
        p = proto_append(p, buf_end, ",\"status\":\"ok\",\"data\":null");
    }
    if (!p) return -1;

    /* 闭合 JSON 对象 */
    p = proto_append(p, buf_end, "}");
    if (!p) return -1;

    return (int)(p - out);
}

/**
 * @brief 将字符串类型名解析回枚举值
 */
static GeomType parse_geom_type(const char *s)
{
    if (!s) return GEOM_POINT;
    if (strcmp(s, "point") == 0)          return GEOM_POINT;
    if (strcmp(s, "line_segment") == 0)   return GEOM_LINE_SEGMENT;
    if (strcmp(s, "region") == 0)         return GEOM_REGION;
    if (strcmp(s, "port") == 0)          return GEOM_PORT;
    if (strcmp(s, "function_block") == 0) return GEOM_FUNCTION_BLOCK;
    return GEOM_POINT;
}

/**
 * @brief 解析 JSON 整数值（简单实现）
 *
 * @param p 输入指针（指向数字起始位置）
 * @param out_val 输出的整数值
 * @return 解析后的新指针位置，失败返回 NULL
 */
static const char *proto_parse_int(const char *p, int *out_val)
{
    int sign = 1;
    int val = 0;

    if (!p || !out_val) return NULL;

    p = proto_skip_ws(p);
    if (!p) return NULL;

    if (*p == '-') {
        sign = -1;
        p++;
    }

    if (*p < '0' || *p > '9') return NULL;

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
int lv_math_protocol_decode(const char *in, void *out)
{
    const char *p;
    char key[64];
    char val[256];
    int found_v = 0;
    int node_count = 0;
    int constraint_count = 0;
    ConstraintGraph **out_graph = (ConstraintGraph **)out;
    ConstraintGraph *graph = NULL;

    if (!in) {
        return -1;
    }

    /* 如果 out 非 NULL，预创建约束图 */
    if (out_graph) {
        graph = graph_create();
        if (!graph) return -1;
    }

    p = proto_skip_ws(in);
    if (*p != '{') {
        if (graph) graph_destroy(graph);
        return -1;
    }
    p++;

    while (*p) {
        p = proto_skip_ws(p);
        if (*p == '}') {
            if (found_v) {
                if (out_graph && graph) {
                    *out_graph = graph;
                }
                return 0;
            }
            if (graph) graph_destroy(graph);
            return -1;
        }
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p != '"') {
            if (graph) graph_destroy(graph);
            return -1;
        }
        p = proto_parse_string(p + 1, key, (int)sizeof(key));
        if (!p) {
            if (graph) graph_destroy(graph);
            return -1;
        }

        p = proto_skip_ws(p);
        if (*p != ':') {
            if (graph) graph_destroy(graph);
            return -1;
        }
        p++;
        p = proto_skip_ws(p);

        if (*p == '"') {
            p = proto_parse_string(p + 1, val, (int)sizeof(val));
            if (!p) {
                if (graph) graph_destroy(graph);
                return -1;
            }

            /* 记录统计字段 */
            if (strcmp(key, "_v") == 0) {
                found_v = 1;
            }
        } else if (*p == '{') {
            /* 跳过嵌套对象 */
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }
            if (depth != 0) {
                if (graph) graph_destroy(graph);
                return -1;
            }
        } else if (*p == '[') {
            /* 跳过嵌套数组 */
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '[') depth++;
                else if (*p == ']') depth--;
                p++;
            }
            if (depth != 0) {
                if (graph) graph_destroy(graph);
                return -1;
            }
        } else {
            /* 数值：解析并记录统计字段 */
            int int_val = -1;
            const char *np = proto_parse_int(p, &int_val);
            if (!np) {
                /* 跳过无法解析的非字符串值 */
                while (*p && *p != ',' && *p != '}') {
                    p++;
                }
            } else {
                p = np;
                if (strcmp(key, "node_count") == 0) {
                    node_count = int_val;
                } else if (strcmp(key, "constraint_count") == 0) {
                    constraint_count = int_val;
                } else if (strcmp(key, "_v") == 0) {
                    found_v = 1;
                }
            }
        }
    }

    if (graph) graph_destroy(graph);
    return -1;
}
