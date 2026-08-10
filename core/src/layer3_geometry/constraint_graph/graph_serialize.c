/**
 * @file graph_serialize.c
 * @brief ConstraintGraph JSON 序列化与反序列化
 *
 * @details 实现约束图的持久化读写：
 *          - graph_serialize_json: 图 → JSON 字符串
 *            包含节点（类型、坐标、属性）和约束（类型、参与者）的完整导出
 *          - graph_deserialize_json: JSON 字符串 → 图
 *            重建节点、坐标（RATIONAL/QUADRATIC/ALGEBRAIC）、约束及邻接关系
 *          - graph_clone: 深拷贝整个约束图（委托序列化/反序列化）
 *          - 线段相交检测（segments_intersect）：用于区域闭合性验证
 *
 *          JSON 模式兼容前端 UI 编辑器，支持 .lvmod 和 .lvax 文件格式。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_xmacro.h"
#include "lv/symbolic_coord.h"

#include "graph_node_internal.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_json.h"
#include "../../layer4_reasoning/proof/trust_color_x.h"

/* graph_validate_region_closure: 实现在 graph_conflict.c 中 */

/* ============================================================
 * 图序列化与反序列化实现
 * ============================================================ */

/* 序列化错误信息存储 —— v3.3.0：使用图级 serialize_buffer 替代旧版全局变量 */
const char *graph_get_serialize_error(const ConstraintGraph *graph) {
    if (!graph || !graph->serialize_buffer) {
        return "";
    }
    return graph->serialize_buffer;
}

/**
 * @brief 设置序列化错误信息
 *
 * 使用可变参数格式化字符串，将错误信息写入约束图的序列化缓冲区。
 * 若图或缓冲区不可用，则回退到全局错误 API。
 *
 * @param graph 约束图指针（可以为 NULL）
 * @param fmt   printf 风格的格式字符串
 * @param ...   可变参数列表
 */
static void set_serialize_error(ConstraintGraph *graph, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *dst = NULL;
    size_t cap = 0;
    if (graph && graph->serialize_buffer) {
        dst = graph->serialize_buffer;
        cap = 256;
    }
    graph_fmt_error_to(dst, cap, fmt, args);
    va_end(args);
}

/* JSON 写入器辅助 —— 迁移至 lv_json.h/lv_json.c */

/* ── 字符串↔枚举 X-macro 列表（LV_GEOM_TYPE_X / LV_CONSTRAINT_TYPE_X 已提升至
 *    constraint_graph.h 作为单一事实来源，此处直接复用） ── */

static const lvStrToEnumEntry geom_type_map[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_GEOM_TYPE_X)
};

static const lvStrToEnumEntry constraint_type_map[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_CONSTRAINT_TYPE_X)
};

/** @brief 坐标类型字符串↔枚举映射表（RATIONAL/ALGEBRAIC/QUADRATIC） */
static const lvStrToEnumEntry coord_type_map[] = {
    {"RATIONAL", RATIONAL},
    {"ALGEBRAIC", ALGEBRAIC},
    {"QUADRATIC", QUADRATIC},
};

/**
 * @brief 将几何节点类型枚举转换为字符串
 *
 * 用于 JSON 序列化时输出节点类型的可读名称。
 *
 * @param type 几何节点类型枚举值
 * @return 类型名称字符串（静态常量，无需释放）
 */
static const char *geom_type_to_string(GeomType type) {
    /* 复用 geom_type_map 表 + lv_enum_to_str 二分查找（替代 switch） */
    return lv_enum_to_str(geom_type_map, lv_ARRAY_SIZE(geom_type_map), (int) type, "UNKNOWN");
}

/**
 * @brief 将约束类型枚举转换为字符串
 *
 * 用于 JSON 序列化时输出约束类型的可读名称。
 *
 * @param type 约束类型枚举值
 * @return 类型名称字符串（静态常量，无需释放）
 */
static const char *constraint_type_to_string(ConstraintType type) {
    /* 委托权威名称表 lv_constraint_type_name（meta_repr.c），保持越界回退 "UNKNOWN" 语义 */
    const char *name = lv_constraint_type_name(type);
    return name ? name : "UNKNOWN";
}

/* 序列化符号坐标 */
static void json_buf_append_coord(lvJsonBuf *buf, const SymbolicCoord *coord) {
    if (!coord) {
        lv_json_buf_append_raw(buf, "null");
        return;
    }

    char *coord_json = symbolic_coord_serialize(coord);
    if (!coord_json) {
        lv_json_buf_append_raw(buf, "null");
        return;
    }

    /* coord_json 格式: {"type":"RATIONAL","num":1,"den":2} */
    lv_json_buf_append_raw(buf, coord_json);
    lv_free((void **) &coord_json);
}

/* 序列化信任颜色 */
/* ================================================================
 * 枚举 -> 名称 映射表（数据表化，替代 switch）
 * ================================================================ */

/** @brief TrustColor 字符串↔枚举映射表（按枚举值升序，序列化/反序列化共用） */
#define LV_TRUST_COLOR_TO_SERIAL(sym, disp, ser, dot, tex) {ser, sym},
static const lvStrToEnumEntry trust_map[] = {
    LV_TRUST_COLOR_X(LV_TRUST_COLOR_TO_SERIAL)
};
#undef LV_TRUST_COLOR_TO_SERIAL

static const char *trust_color_to_string(TrustColor trust) {
    /* 复用 trust_map 表 + lv_enum_to_str 二分查找（替代 switch） */
    return lv_enum_to_str(trust_map, lv_ARRAY_SIZE(trust_map), (int) trust, "UNKNOWN");
}

/** @brief 将信任颜色字符串反序列化为 TrustColor 枚举（未命中回退 TRUST_GREEN） */
static TrustColor string_to_trust_color(const char *str) {
    return (TrustColor) lv_str_to_enum(trust_map, lv_ARRAY_SIZE(trust_map), str, TRUST_GREEN);
}

/* 序列化单个节点 */
char *graph_node_serialize_to_json(const GeomNode *node) {
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_node_serialize_to_json: node is NULL");

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 1024))
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_node_serialize_to_json: json buf init failed");

    /* 对象级 API：键/标量值自动管理逗号（紧凑输出与旧手写模板字节一致） */
    lv_json_buf_begin_object(&buf);

    /* id */
    lv_json_buf_append_key(&buf, "id");
    lv_json_buf_append_int(&buf, node->id);

    /* type */
    lv_json_buf_append_key(&buf, "type");
    lv_json_buf_append_string(&buf, geom_type_to_string(node->type));

    /* trust */
    lv_json_buf_append_key(&buf, "trust");
    lv_json_buf_append_string(&buf, trust_color_to_string(node->trust));

    /* namespace_depth */
    lv_json_buf_append_key(&buf, "namespace_depth");
    lv_json_buf_append_int(&buf, node->namespace_depth);

    /* parent_block_id */
    lv_json_buf_append_key(&buf, "parent_block_id");
    lv_json_buf_append_int(&buf, node->parent_block_id);

    /* coords：元素为预序列化的坐标 JSON（raw 写入），需手动管理数组内逗号 */
    lv_json_buf_append_key(&buf, "coords");
    lv_json_buf_begin_array(&buf);
    for (int i = 0; i < node->coord_count; i++) {
        if (i > 0)
            lv_json_buf_append_char(&buf, ',');
        json_buf_append_coord(&buf, node->symbolic_coords[i]);
    }
    lv_json_buf_end_array(&buf);

    /* 类型特定数据：vtable serialize 以 raw 追加自带分隔的 "key":value 片段，
     * 不感知对象级状态机，故 coords 数组后的分隔逗号按原行为手动写出 */
    lv_json_buf_append_char(&buf, ',');
    if (node->vtable && node->vtable->serialize) {
        node->vtable->serialize(node, &buf);
    }

    lv_json_buf_end_object(&buf);
    return lv_json_buf_finalize(&buf);
}

/* 序列化单个约束 */
char *graph_constraint_serialize_to_json(const Constraint *constraint) {
    if (!constraint)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_constraint_serialize_to_json: constraint is NULL");

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 256))
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_constraint_serialize_to_json: json buf init failed");

    /* 对象级 API：键/标量值自动管理逗号（紧凑输出与旧手写模板字节一致） */
    lv_json_buf_begin_object(&buf);

    /* id */
    lv_json_buf_append_key(&buf, "id");
    lv_json_buf_append_int(&buf, constraint->id);

    /* type */
    lv_json_buf_append_key(&buf, "constraint_type");
    lv_json_buf_append_string(&buf, constraint_type_to_string(constraint->type));

    /* participants：整型数组，append_int 自动管理逗号 */
    lv_json_buf_append_key(&buf, "participants");
    lv_json_buf_begin_array(&buf);
    for (int i = 0; i < constraint->participant_count; i++) {
        lv_json_buf_append_int(&buf, constraint->participants[i]);
    }
    lv_json_buf_end_array(&buf);

    /* template_id */
    lv_json_buf_append_key(&buf, "template_id");
    lv_json_buf_append_int(&buf, constraint->template_id);

    /* numeric_value (used by ANGLE constraints)：append_double 为 %.15g 风格，与旧格式一致 */
    lv_json_buf_append_key(&buf, "numeric_value");
    lv_json_buf_append_double(&buf, constraint->numeric_value);

    lv_json_buf_end_object(&buf);
    return lv_json_buf_finalize(&buf);
}

/* 序列化整个图 */
char *graph_serialize_to_json(const ConstraintGraph *graph) {
    if (!graph) {
        set_serialize_error(graph, "图指针为空");
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_serialize_to_json: graph is NULL");
    }

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 8192)) {
        set_serialize_error(graph, "内存分配失败");
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_serialize_to_json: json buf init failed");
    }

    /* 对象级 API：键/标量值自动管理逗号（紧凑输出与旧手写模板字节一致） */
    lv_json_buf_begin_object(&buf);

    /* 图元数据 */
    lv_json_buf_append_key(&buf, "node_count");
    lv_json_buf_append_int(&buf, graph->node_count);

    lv_json_buf_append_key(&buf, "constraint_count");
    lv_json_buf_append_int(&buf, graph->constraint_count);

    lv_json_buf_append_key(&buf, "next_node_id");
    lv_json_buf_append_int(&buf, graph->next_node_id);

    lv_json_buf_append_key(&buf, "next_constraint_id");
    lv_json_buf_append_int(&buf, graph->next_constraint_id);

    /* 节点数组：元素为预序列化的节点 JSON（raw 写入），需手动管理数组内逗号 */
    lv_json_buf_append_key(&buf, "nodes");
    lv_json_buf_begin_array(&buf);
    for (int i = 0; i < graph->node_count; i++) {
        if (i > 0)
            lv_json_buf_append_char(&buf, ',');
        char *node_json = graph_node_serialize_to_json(graph->nodes[i]);
        if (node_json) {
            lv_json_buf_append_raw(&buf, node_json);
            lv_free((void **) &node_json);
        } else {
            lv_json_buf_append_raw(&buf, "null");
        }
    }
    lv_json_buf_end_array(&buf);

    /* 约束数组：同上，元素为预序列化的约束 JSON */
    lv_json_buf_append_key(&buf, "constraints");
    lv_json_buf_begin_array(&buf);
    for (int i = 0; i < graph->constraint_count; i++) {
        if (i > 0)
            lv_json_buf_append_char(&buf, ',');
        char *constraint_json = graph_constraint_serialize_to_json(graph->constraints[i]);
        if (constraint_json) {
            lv_json_buf_append_raw(&buf, constraint_json);
            lv_free((void **) &constraint_json);
        } else {
            lv_json_buf_append_raw(&buf, "null");
        }
    }
    lv_json_buf_end_array(&buf);

    lv_json_buf_end_object(&buf);
    return lv_json_buf_finalize(&buf);
}

/* JSON 解析器 —— 迁移至 lv_json.h/lv_json.c */

/* 从字符串转换类型名称 */
static GeomType string_to_geom_type(const char *str) {
    return (GeomType)lv_str_to_enum(geom_type_map, 6, str, GEOM_POINT);
}

static ConstraintType string_to_constraint_type(const char *str) {
    return (ConstraintType)lv_str_to_enum(constraint_type_map, 6, str, INCIDENCE);
}

/** @brief 将坐标类型字符串反序列化为 CoordType 枚举（未命中返回 -1，保持原默认值语义） */
static int string_to_coord_type(const char *str) {
    return lv_str_to_enum(coord_type_map, lv_ARRAY_SIZE(coord_type_map), str, -1);
}

/* 解析数组中的整数列表 */
static int *json_parser_parse_int_array(lvJsonParser *p, int *out_count) {
    if (!lv_json_expect(p, '[')) {
        *out_count = 0;
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "json_parser_parse_int_array: expected '['");
    }

    lv_json_skip_ws(p);
    if (lv_json_peek(p) == ']') {
        p->pos++;
        *out_count = 0;
        lv_RETURN_ERROR_NULL(lv_ERROR_NOT_FOUND, "json_parser_parse_int_array: empty array");
    }

    /* 先计数 */
    int capacity = 8;
    int count = 0;
    int *result = lv_malloc((size_t) capacity * sizeof(int));
    if (!result) {
        *out_count = 0;
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "json_parser_parse_int_array: malloc failed");
    }

    while (lv_json_peek(p) != ']' && lv_json_peek(p) != '\0') {
        if (!lv_ensure_capacity((void **) &result, count, &capacity, sizeof(int), 0)) {
            lv_free((void **) &result);
            *out_count = 0;
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "json_parser_parse_int_array: realloc failed");
        }

        if (lv_json_parse_int(p, &result[count])) {
            count++;
        }

        lv_json_skip_ws(p);
        if (lv_json_peek(p) == ',') {
            p->pos++;
        }
    }

    lv_json_expect(p, ']');
    *out_count = count;
    return result;
}

/* ── 节点/约束 JSON 反序列化字段分发（查找表，替代手写 strcmp 链） ── */

/** @brief 节点反序列化上下文：收集 JSON 字段解析结果 */
typedef struct {
    int node_id;
    int coord_count;
    GeomType node_type;
    TrustColor trust;
    int ns_depth;
    int parent_block_id;
    int *boundary_segs;
    int boundary_seg_count;
    int *internal_nodes;
    int internal_node_count;
    int *input_port_ids;
    int input_port_count;
    int *output_port_ids;
    int output_port_count;
    PortType port_type;
    bool is_formal_param;
    bool is_polymorphic;
    int circle_center_id;
    int circle_radius_id;
    SymbolicCoord **coords;
} NodeDeserCtx;

typedef void (*NodeFieldHandler)(lvJsonParser *p, NodeDeserCtx *ctx);

static void node_field_id(lvJsonParser *p, NodeDeserCtx *ctx) { lv_json_parse_int(p, &ctx->node_id); }

static void node_field_type(lvJsonParser *p, NodeDeserCtx *ctx) {
    char *type_str = lv_json_parse_string(p);
    if (type_str) {
        ctx->node_type = string_to_geom_type(type_str);
        lv_free((void **) &type_str);
    }
}

static void node_field_trust(lvJsonParser *p, NodeDeserCtx *ctx) {
    char *trust_str = lv_json_parse_string(p);
    if (trust_str) {
        /* 查表反序列化（替代 10 分支 strcmp 链） */
        ctx->trust = string_to_trust_color(trust_str);
        lv_free((void **) &trust_str);
    }
}

static void node_field_ns_depth(lvJsonParser *p, NodeDeserCtx *ctx) { lv_json_parse_int(p, &ctx->ns_depth); }

static void node_field_parent_block_id(lvJsonParser *p, NodeDeserCtx *ctx) {
    lv_json_parse_int(p, &ctx->parent_block_id);
}

static void node_field_center_node_id(lvJsonParser *p, NodeDeserCtx *ctx) {
    lv_json_parse_int(p, &ctx->circle_center_id);
}

static void node_field_radius_node_id(lvJsonParser *p, NodeDeserCtx *ctx) {
    lv_json_parse_int(p, &ctx->circle_radius_id);
}

static void node_field_boundary_segments(lvJsonParser *p, NodeDeserCtx *ctx) {
    ctx->boundary_segs = json_parser_parse_int_array(p, &ctx->boundary_seg_count);
}

static void node_field_internal_nodes(lvJsonParser *p, NodeDeserCtx *ctx) {
    ctx->internal_nodes = json_parser_parse_int_array(p, &ctx->internal_node_count);
}

static void node_field_input_port_ids(lvJsonParser *p, NodeDeserCtx *ctx) {
    ctx->input_port_ids = json_parser_parse_int_array(p, &ctx->input_port_count);
}

static void node_field_output_port_ids(lvJsonParser *p, NodeDeserCtx *ctx) {
    ctx->output_port_ids = json_parser_parse_int_array(p, &ctx->output_port_count);
}

static void node_field_port_type(lvJsonParser *p, NodeDeserCtx *ctx) {
    char *pt = lv_json_parse_string(p);
    if (pt) {
        ctx->port_type = (strcmp(pt, "OUTPUT") == 0) ? PORT_OUTPUT : PORT_INPUT;
        lv_free((void **) &pt);
    }
}

static void node_field_is_formal_param(lvJsonParser *p, NodeDeserCtx *ctx) {
    lv_json_parse_bool(p, &ctx->is_formal_param);
}

static void node_field_is_polymorphic(lvJsonParser *p, NodeDeserCtx *ctx) {
    lv_json_parse_bool(p, &ctx->is_polymorphic);
}

static void node_field_coords(lvJsonParser *p, NodeDeserCtx *ctx) {
    if (!lv_json_expect(p, '['))
        return;
    /* 计数 */
    int temp_count = 0;
    lvJsonParser temp_p = *p;
    while (temp_p.pos < temp_p.size && temp_p.data[temp_p.pos] != ']') {
        if (temp_p.data[temp_p.pos] == ',') {
            temp_count++;
            temp_p.pos++;
        } else if (temp_p.data[temp_p.pos] != ' ' && temp_p.data[temp_p.pos] != '\t') {
            temp_count++;
        }
        lv_json_skip_value(&temp_p);
        lv_json_skip_ws(&temp_p);
    }
    ctx->coord_count = temp_count;

    /* 解析坐标 */
    if (ctx->coord_count > 0) {
        ctx->coords = lv_malloc((size_t) ctx->coord_count * sizeof(SymbolicCoord *));
        if (!ctx->coords) {
            ctx->coord_count = 0;
        } else {
            for (int i = 0; i < ctx->coord_count; i++) {
                ctx->coords[i] = NULL;
            }
        }
    }

    for (int i = 0; i < ctx->coord_count && lv_json_peek(p) != ']' && lv_json_peek(p) != '\0'; i++) {
        if (lv_json_peek(p) == ',')
            p->pos++;

        if (lv_json_peek(p) == 'n') {
            lv_json_skip_value(p);
            continue;
        }

        if (lv_json_peek(p) == '{') {
            p->pos++; /* skip '{' */
            char *coord_key = lv_json_parse_string(p);
            int coord_type = -1; /* 0=RATIONAL, 1=ALGEBRAIC, 2=QUADRATIC */
            if (coord_key && strcmp(coord_key, "type") == 0) {
                p->pos++; /* skip ':' */
                char *ct = lv_json_parse_string(p);
                if (ct) {
                    /* 查表反序列化（替代 3 分支 strcmp 链） */
                    coord_type = string_to_coord_type(ct);
                    lv_free((void **) &ct);
                }
                lv_free((void **) &coord_key);

                /* 继续解析值 */
                while (lv_json_peek(p) != '}' && lv_json_peek(p) != '\0') {
                    coord_key = lv_json_parse_string(p);
                    if (!coord_key)
                        break;
                    lv_json_skip_ws(p);
                    if (p->pos < p->size && p->data[p->pos] == ':')
                        p->pos++;

                    if (coord_type == 0 && strcmp(coord_key, "num") == 0) {
                        int64_t num, den = 1;
                        /* 手动解析 int64_t 值，避免 int* 截断 */
                        {
                            lv_json_skip_ws(p);
                            char *end;
                            long long val = strtoll((const char *) p->data + p->pos, &end, 10);
                            num = (int64_t) val;
                            p->pos = (size_t) (end - (const char *) p->data);
                        }
                        /* 查找 den */
                        lv_json_skip_ws(p);
                        if (lv_json_peek(p) == ',') {
                            p->pos++;
                            char *dk = lv_json_parse_string(p);
                            if (dk && strcmp(dk, "den") == 0) {
                                p->pos++;
                                if (p->pos < p->size && p->data[p->pos] == ':')
                                    p->pos++;
                                /* 手动解析 int64_t 值，避免 int* 截断 */
                                {
                                    lv_json_skip_ws(p);
                                    char *end;
                                    long long val =
                                        strtoll((const char *) p->data + p->pos, &end, 10);
                                    den = (int64_t) val;
                                    p->pos = (size_t) (end - (const char *) p->data);
                                }
                            }
                            lv_free((void **) &dk);
                        }
                        ctx->coords[i] = symbolic_coord_create_rational(num, den);
                    }
                    lv_free((void **) &coord_key);
                    lv_json_skip_ws(p);
                }
            } else {
                lv_free((void **) &coord_key);
                lv_json_skip_value(p);
                while (lv_json_peek(p) != '}')
                    lv_json_skip_value(p);
            }
            if (lv_json_peek(p) == '}')
                p->pos++;
        } else {
            /* 序列化器对 RATIONAL 坐标实际输出简写 "num/den"（rational_serialize
             * 的恒定 "分子/分母" 格式，整数亦带 /1；见 json_buf_append_coord），
             * 读取原始 token 直至 ',' 或 ']' 后用 rational_parse 还原（比手写
             * strtoll 更健壮：自动规范化 + 任意精度）。algebraic/quadratic/
             * transcendental 的简写输出含空格/特殊字符无法无损还原，保持
             * coord 为 NULL —— 与对象分支仅重建 RATIONAL 的语义一致。 */
            lv_json_skip_ws(p);
            size_t tok_start = p->pos;
            while (p->pos < p->size && p->data[p->pos] != ',' && p->data[p->pos] != ']')
                p->pos++;
            if (p->pos > tok_start) {
                size_t tok_len = p->pos - tok_start;
                char *tok = lv_malloc(tok_len + 1);
                if (tok) {
                    memcpy(tok, p->data + tok_start, tok_len);
                    tok[tok_len] = '\0';
                    Rational *rat = rational_parse(tok);
                    if (rat) {
                        ctx->coords[i] = symbolic_coord_create_rational(
                            (int64_t) mpz_get_si(mpq_numref(rat->value)),
                            (uint64_t) mpz_get_ui(mpq_denref(rat->value)));
                        rational_destroy(rat);
                    }
                    lv_free((void **) &tok);
                }
            }
        }
    }

    lv_json_expect(p, ']');
}

/** @brief 节点字段名→处理函数 查找表（替代 15 分支 strcmp 链） */
static const struct {
    const char *key;
    NodeFieldHandler handler;
} kNodeFieldTable[] = {
    {"id", node_field_id},
    {"type", node_field_type},
    {"trust", node_field_trust},
    {"namespace_depth", node_field_ns_depth},
    {"parent_block_id", node_field_parent_block_id},
    {"coords", node_field_coords},
    {"center_node_id", node_field_center_node_id},
    {"radius_node_id", node_field_radius_node_id},
    {"boundary_segments", node_field_boundary_segments},
    {"internal_nodes", node_field_internal_nodes},
    {"input_port_ids", node_field_input_port_ids},
    {"output_port_ids", node_field_output_port_ids},
    {"port_type", node_field_port_type},
    {"is_formal_param", node_field_is_formal_param},
    {"is_polymorphic", node_field_is_polymorphic},
};

/** @brief 在节点字段表中查找键名对应的 handler */
static NodeFieldHandler node_field_lookup(const char *key) {
    for (size_t i = 0; i < lv_ARRAY_SIZE(kNodeFieldTable); i++) {
        if (strcmp(kNodeFieldTable[i].key, key) == 0)
            return kNodeFieldTable[i].handler;
    }
    return NULL;
}

/** @brief 约束反序列化上下文：收集 JSON 字段解析结果 */
typedef struct {
    int constraint_id;
    int template_id;
    double numeric_value;
    ConstraintType constraint_type;
    int *participants;
    int participant_count;
} ConstraintDeserCtx;

typedef void (*ConstraintFieldHandler)(lvJsonParser *p, ConstraintDeserCtx *ctx);

static void constraint_field_id(lvJsonParser *p, ConstraintDeserCtx *ctx) {
    lv_json_parse_int(p, &ctx->constraint_id);
}

static void constraint_field_type(lvJsonParser *p, ConstraintDeserCtx *ctx) {
    char *type_str = lv_json_parse_string(p);
    if (type_str) {
        ctx->constraint_type = string_to_constraint_type(type_str);
        lv_free((void **) &type_str);
    }
}

static void constraint_field_participants(lvJsonParser *p, ConstraintDeserCtx *ctx) {
    ctx->participants = json_parser_parse_int_array(p, &ctx->participant_count);
}

static void constraint_field_template_id(lvJsonParser *p, ConstraintDeserCtx *ctx) {
    lv_json_parse_int(p, &ctx->template_id);
}

static void constraint_field_numeric_value(lvJsonParser *p, ConstraintDeserCtx *ctx) {
    lv_json_skip_ws(p);
    char *end;
    ctx->numeric_value = strtod(p->data + p->pos, &end);
    p->pos = (size_t) (end - p->data);
}

/** @brief 约束字段名→处理函数 查找表（替代 5 分支 strcmp 链） */
static const struct {
    const char *key;
    ConstraintFieldHandler handler;
} kConstraintFieldTable[] = {
    {"id", constraint_field_id},
    {"constraint_type", constraint_field_type},
    {"participants", constraint_field_participants},
    {"template_id", constraint_field_template_id},
    {"numeric_value", constraint_field_numeric_value},
};

/** @brief 在约束字段表中查找键名对应的 handler */
static ConstraintFieldHandler constraint_field_lookup(const char *key) {
    for (size_t i = 0; i < lv_ARRAY_SIZE(kConstraintFieldTable); i++) {
        if (strcmp(kConstraintFieldTable[i].key, key) == 0)
            return kConstraintFieldTable[i].handler;
    }
    return NULL;
}

/* 反序列化图 */
ConstraintGraph *graph_deserialize_from_json(const char *json) {
    if (!json) {
        set_serialize_error(NULL, "JSON 字符串为空");
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_deserialize_from_json: json is NULL");
    }

    size_t json_len = strlen(json);
    lvJsonParser p;
    lv_json_parser_init(&p, json, json_len);

    if (lv_json_peek(&p) != '{') {
        set_serialize_error(NULL, "期望 JSON 对象");
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "graph_deserialize_from_json: expected JSON object");
    }
    p.pos++; /* skip '{' */

    /* 创建图 */
    ConstraintGraph *graph = graph_create();
    if (!graph) {
        set_serialize_error(graph, "创建图失败");
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_deserialize_from_json: graph_create failed");
    }

    /* 解析元数据（可选） */
    int node_count = 0, constraint_count = 0;
    int next_node_id = 0, next_constraint_id = 0;

    /* 解析节点和约束数组 */
    char *key = NULL;
    while (lv_json_parse_field(&p, &key)) {
        if (strcmp(key, "nodes") == 0) {
            if (!lv_json_expect(&p, '[')) {
                lv_free((void **) &key);
                graph_destroy(graph);
                set_serialize_error(graph, "节点数组格式错误");
                lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "graph_deserialize_from_json: expected node array");
            }

            while (lv_json_peek(&p) != ']' && lv_json_peek(&p) != '\0') {
                if (lv_json_peek(&p) == ',') {
                    p.pos++;
                    continue;
                }

                if (lv_json_peek(&p) == 'n') {
                    /* null */
                    lv_json_skip_value(&p);
                    continue;
                }

                if (lv_json_peek(&p) != '{') {
                    lv_json_skip_value(&p);
                    continue;
                }
                p.pos++; /* skip '{' */

                /* 解析节点（字段分发查找表，替代 15 分支 strcmp 链） */
                NodeDeserCtx ctx;
                memset(&ctx, 0, sizeof(ctx));
                ctx.parent_block_id = -1;
                ctx.circle_center_id = -1;
                ctx.circle_radius_id = -1;
                ctx.port_type = PORT_INPUT;

                char *node_key = NULL;
                while (lv_json_parse_field(&p, &node_key)) {
                    /* 字段查表分发（替代 15 分支 strcmp 链） */
                    NodeFieldHandler fh = node_field_lookup(node_key);
                    if (fh)
                        fh(&p, &ctx);
                    else
                        lv_json_skip_value(&p);

                    lv_free((void **) &node_key);
                }

                if (lv_json_peek(&p) == '}')
                    p.pos++;

                /* 对于 LINE_SEGMENT，需要先确保端点节点存在 */
                /* 我们先跳过 LINE_SEGMENT 和 REGION，在第二轮处理 */
                /* 暂时存储节点信息以便后续处理 */
                GeomNode *node = graph_add_node_with_id(graph, ctx.node_id, ctx.node_type, ctx.coords, ctx.coord_count);
                if (node) {
                    node->trust = ctx.trust;
                    node->namespace_depth = ctx.ns_depth;
                    node->parent_block_id = ctx.parent_block_id;

                    /* 设置类型特定数据。
                     * REGION / PORT / FUNCTION_BLOCK 的类型特定数据（boundary_segments /
                     * Port 逐字段 / 三数组）重建改走 vtable->clone 公共入口
                     * （graph_node_alloc.c 的 region_clone / port_clone /
                     * func_block_clone），消除反序列化与节点深拷贝（node_deep_copy.c
                     * 原 kDataCopyHandlers）平行的手写分配+拷贝实现。
                     * clone 契约要求目标节点已存在于图中且 ID 与源一致：目标节点即
                     * graph_add_node_with_id 刚创建的 node，故构造承载 JSON 解析结果的
                     * 临时源节点（内部节点 ID 解析为图中引用）后调用 clone。 */
                    if (ctx.node_type == GEOM_REGION && ctx.boundary_segs && ctx.boundary_seg_count > 0) {
                        GeomNode src;
                        memset(&src, 0, sizeof(src));
                        src.id = ctx.node_id;
                        src.type = GEOM_REGION;
                        src.vtable = node->vtable;
                        src.data.region.boundary_segments =
                            lv_malloc((size_t) ctx.boundary_seg_count * sizeof(GeomNode *));
                        if (src.data.region.boundary_segments) {
                            for (int i = 0; i < ctx.boundary_seg_count; i++) {
                                src.data.region.boundary_segments[i] = graph_get_node(graph, ctx.boundary_segs[i]);
                            }
                            src.data.region.segment_count = ctx.boundary_seg_count;
                            if (!node->vtable->clone(&src, graph)) {
                                /* clone 失败（OOM）：region_clone 已重置 segment_count=0，
                                 * 目标节点保持数组/计数一致的空数据 */
                            }
                            lv_free((void **) &src.data.region.boundary_segments);
                        }
                    } else if (ctx.node_type == GEOM_CIRCLE) {
                        node->data.circle.center_node_id = ctx.circle_center_id;
                        node->data.circle.radius_node_id = ctx.circle_radius_id;
                    } else if (ctx.node_type == GEOM_PORT) {
                        /* Port 逐字段填充改走 port_clone；目标 data.port 已由
                         * graph_add_node_with_id -> port_alloc 分配（复用避免泄漏） */
                        Port src_port;
                        memset(&src_port, 0, sizeof(src_port));
                        src_port.id = ctx.node_id;
                        src_port.type = ctx.port_type;
                        src_port.namespace_depth = ctx.ns_depth;
                        src_port.parent_block_id = ctx.parent_block_id;
                        src_port.is_formal_param = ctx.is_formal_param;
                        src_port.is_polymorphic = ctx.is_polymorphic;
                        src_port.connected_to = NULL;
                        GeomNode src;
                        memset(&src, 0, sizeof(src));
                        src.id = ctx.node_id;
                        src.type = GEOM_PORT;
                        src.vtable = node->vtable;
                        src.data.port = &src_port;
                        node->vtable->clone(&src, graph);
                    } else if (ctx.node_type == GEOM_FUNCTION_BLOCK) {
                        /* 三数组重建改走 func_block_clone；counts 仅在对应数组存在时
                         * 设置，保证传入 clone 的源数据一致（数组与计数同生共死） */
                        GeomNode src;
                        memset(&src, 0, sizeof(src));
                        src.id = ctx.node_id;
                        src.type = GEOM_FUNCTION_BLOCK;
                        src.vtable = node->vtable;
                        src.data.func_block.internal_nodes = NULL;
                        if (ctx.internal_nodes && ctx.internal_node_count > 0) {
                            src.data.func_block.internal_nodes =
                                lv_malloc((size_t) ctx.internal_node_count * sizeof(GeomNode *));
                            if (src.data.func_block.internal_nodes) {
                                for (int i = 0; i < ctx.internal_node_count; i++) {
                                    src.data.func_block.internal_nodes[i] =
                                        graph_get_node(graph, ctx.internal_nodes[i]);
                                }
                                src.data.func_block.internal_node_count = ctx.internal_node_count;
                            }
                            /* 分配失败：internal_node_count 保持 0，与数组 NULL 保持一致 */
                        }
                        if (ctx.input_port_ids && ctx.input_port_count > 0) {
                            src.data.func_block.input_port_ids = ctx.input_port_ids;
                            src.data.func_block.input_count = ctx.input_port_count;
                        }
                        if (ctx.output_port_ids && ctx.output_port_count > 0) {
                            src.data.func_block.output_port_ids = ctx.output_port_ids;
                            src.data.func_block.output_count = ctx.output_port_count;
                        }
                        src.data.func_block.determinism_state = UNVERIFIED;
                        node->vtable->clone(&src, graph);
                        lv_free((void **) &src.data.func_block.internal_nodes);
                    }

                    /* 更新 next_node_id */
                    if (ctx.node_id >= graph->next_node_id) {
                        graph->next_node_id = ctx.node_id + 1;
                    }
                }

                /* 释放临时数据 */
                lv_free((void **) &ctx.boundary_segs);
                lv_free((void **) &ctx.internal_nodes);
                lv_free((void **) &ctx.input_port_ids);
                lv_free((void **) &ctx.output_port_ids);
                if (ctx.coords) {
                    for (int i = 0; i < ctx.coord_count; i++) {
                        if (ctx.coords[i])
                            symbolic_coord_destroy(ctx.coords[i]);
                    }
                    lv_free((void **) &ctx.coords);
                }
            }

            lv_json_expect(&p, ']');
        } else if (strcmp(key, "constraints") == 0) {
            if (!lv_json_expect(&p, '[')) {
                lv_free((void **) &key);
                graph_destroy(graph);
                set_serialize_error(graph, "约束数组格式错误");
                lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "graph_deserialize_from_json: expected constraint array");
            }

            while (lv_json_peek(&p) != ']' && lv_json_peek(&p) != '\0') {
                if (lv_json_peek(&p) == ',') {
                    p.pos++;
                    continue;
                }

                if (lv_json_peek(&p) == 'n') {
                    lv_json_skip_value(&p);
                    continue;
                }

                if (lv_json_peek(&p) != '{') {
                    lv_json_skip_value(&p);
                    continue;
                }
                p.pos++; /* skip '{' */

                /* 解析约束（字段分发查找表，替代 5 分支 strcmp 链） */
                ConstraintDeserCtx cctx;
                memset(&cctx, 0, sizeof(cctx));
                cctx.template_id = -1;
                cctx.constraint_type = INCIDENCE;

                char *ckey = NULL;
                while (lv_json_parse_field(&p, &ckey)) {
                    /* 字段查表分发（替代 5 分支 strcmp 链） */
                    ConstraintFieldHandler cfh = constraint_field_lookup(ckey);
                    if (cfh)
                        cfh(&p, &cctx);
                    else
                        lv_json_skip_value(&p);

                    lv_free((void **) &ckey);
                }

                if (lv_json_peek(&p) == '}')
                    p.pos++;

                /* 使用带ID的接口添加约束 */
                if (cctx.participants && cctx.participant_count > 0) {
                    Constraint *constraint =
                        graph_add_constraint_with_id(graph, cctx.constraint_id, cctx.constraint_type,
                                                     cctx.participants, cctx.participant_count);
                    if (constraint) {
                        constraint->template_id = cctx.template_id;
                        constraint->numeric_value = cctx.numeric_value;
                        if (cctx.constraint_id >= graph->next_constraint_id) {
                            graph->next_constraint_id = cctx.constraint_id + 1;
                        }
                    }
                }

                lv_free((void **) &cctx.participants);
            }

            lv_json_expect(&p, ']');
        } else {
            lv_json_skip_value(&p);
        }

        lv_free((void **) &key);
    }

    return graph;
}
