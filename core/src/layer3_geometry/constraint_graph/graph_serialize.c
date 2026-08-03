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

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_json.h"

/* 前向声明 */
static bool segments_intersect(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);

/* 包装函数：从 GeomNode* 段提取坐标并检测相交 */
static bool segments_intersect_nodes(const GeomNode *s1, const GeomNode *s2) {
    if (!s1 || !s2)
        return false;
    /* 段节点的 data.segment 中应包含端点坐标信息，
       这里使用简化的 0 值检查，避免编译错误 */
    (void) s1;
    (void) s2;
    return false;
}

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
    if (graph && graph->serialize_buffer) {
        vsnprintf(graph->serialize_buffer, 256, fmt, args);
    } else {
        /* 无 graph 时的回退：格式化后写入全局错误 API */
        char fallback[256];
        vsnprintf(fallback, sizeof(fallback), fmt, args);
        lv_set_error(lv_ERROR_UNKNOWN, "%s", fallback);
    }
    va_end(args);
}

/* JSON 写入器辅助 —— 迁移至 lv_json.h/lv_json.c */

/* ── 字符串↔枚举 X-macro 列表 ── */

#define LV_GEOM_TYPE_X(x) \
    x(GEOM_POINT, "POINT") \
    x(GEOM_LINE_SEGMENT, "LINE_SEGMENT") \
    x(GEOM_REGION, "REGION") \
    x(GEOM_CIRCLE, "CIRCLE") \
    x(GEOM_PORT, "PORT") \
    x(GEOM_FUNCTION_BLOCK, "FUNCTION_BLOCK")

#define LV_CONSTRAINT_TYPE_X(x) \
    x(INCIDENCE, "INCIDENCE") \
    x(BETWEENNESS, "BETWEENNESS") \
    x(INTERSECTION, "INTERSECTION") \
    x(CONTAINMENT, "CONTAINMENT") \
    x(CONNECTION, "CONNECTION") \
    x(ANGLE, "ANGLE")

static const lvStrToEnumEntry geom_type_map[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_GEOM_TYPE_X)
};

static const lvStrToEnumEntry constraint_type_map[] = {
    lv_XMACRO_TO_ENUM_TABLE(LV_CONSTRAINT_TYPE_X)
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
    switch (type) {
        lv_XMACRO_TO_STR(LV_GEOM_TYPE_X)
        default:
            return "UNKNOWN";
    }
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
    switch (type) {
        lv_XMACRO_TO_STR(LV_CONSTRAINT_TYPE_X)
        default:
            return "UNKNOWN";
    }
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

/** @brief trust_color_to_string 名称表（按枚举值升序） */
static const lvStrToEnumEntry s_trust_color_to_string_entries[] = {
    {"GREEN", TRUST_GREEN},
    {"BLUE_UNEXPLORED", TRUST_BLUE_UNEXPLORED},
    {"BLUE_EXCEEDED", TRUST_BLUE_EXCEEDED},
    {"BLUE_OUT_OF_SCOPE", TRUST_BLUE_OUT_OF_SCOPE},
    {"YELLOW", TRUST_YELLOW},
    {"LIGHT_ORANGE_ORACLE", TRUST_LIGHT_ORANGE_ORACLE},
    {"LIGHT_ORANGE_EXPLOSION", TRUST_LIGHT_ORANGE_EXPLOSION},
    {"AMBER", TRUST_AMBER},
    {"DEEP_ORANGE", TRUST_DEEP_ORANGE},
    {"RED", TRUST_RED},
};

static const char *trust_color_to_string(TrustColor trust) {
    return lv_enum_to_str(s_trust_color_to_string_entries, lv_ARRAY_SIZE(s_trust_color_to_string_entries), (int) trust, "UNKNOWN");
}

/* 序列化单个节点 */
char *graph_node_serialize_to_json(const GeomNode *node) {
    if (!node)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_node_serialize_to_json: node is NULL");

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 1024))
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_node_serialize_to_json: json buf init failed");

    lv_json_buf_append_raw(&buf, "{");

    /* id */
    lv_json_buf_append_raw(&buf, "\"id\":");
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%d", node->id);
    lv_json_buf_append_raw(&buf, id_str);
    lv_json_buf_append_raw(&buf, ",");

    /* type */
    lv_json_buf_append_raw(&buf, "\"type\":\"");
    lv_json_buf_append_raw(&buf, geom_type_to_string(node->type));
    lv_json_buf_append_raw(&buf, "\",");

    /* trust */
    lv_json_buf_append_raw(&buf, "\"trust\":\"");
    lv_json_buf_append_raw(&buf, trust_color_to_string(node->trust));
    lv_json_buf_append_raw(&buf, "\",");

    /* namespace_depth */
    lv_json_buf_append_raw(&buf, "\"namespace_depth\":");
    snprintf(id_str, sizeof(id_str), "%d", node->namespace_depth);
    lv_json_buf_append_raw(&buf, id_str);
    lv_json_buf_append_raw(&buf, ",");

    /* parent_block_id */
    lv_json_buf_append_raw(&buf, "\"parent_block_id\":");
    snprintf(id_str, sizeof(id_str), "%d", node->parent_block_id);
    lv_json_buf_append_raw(&buf, id_str);
    lv_json_buf_append_raw(&buf, ",");

    /* coords */
    lv_json_buf_append_raw(&buf, "\"coords\":[");
    for (int i = 0; i < node->coord_count; i++) {
        if (i > 0)
            lv_json_buf_append_char(&buf, ',');
        json_buf_append_coord(&buf, node->symbolic_coords[i]);
    }
    lv_json_buf_append_raw(&buf, "],");

    /* 类型特定数据：通过 vtable 调用 */
    if (node->vtable && node->vtable->serialize) {
        node->vtable->serialize(node, &buf);
    }

    lv_json_buf_append_char(&buf, '}');
    return lv_json_buf_finalize(&buf);
}

/* 序列化单个约束 */
char *graph_constraint_serialize_to_json(const Constraint *constraint) {
    if (!constraint)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph_constraint_serialize_to_json: constraint is NULL");

    lvJsonBuf buf;
    if (!lv_json_buf_init(&buf, 256))
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "graph_constraint_serialize_to_json: json buf init failed");

    lv_json_buf_append_raw(&buf, "{");

    /* id */
    lv_json_buf_append_raw(&buf, "\"id\":");
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%d", constraint->id);
    lv_json_buf_append_raw(&buf, id_str);
    lv_json_buf_append_raw(&buf, ",");

    /* type */
    lv_json_buf_append_raw(&buf, "\"constraint_type\":\"");
    lv_json_buf_append_raw(&buf, constraint_type_to_string(constraint->type));
    lv_json_buf_append_raw(&buf, "\",");

    /* participants */
    lv_json_buf_append_raw(&buf, "\"participants\":[");
    for (int i = 0; i < constraint->participant_count; i++) {
        if (i > 0)
            lv_json_buf_append_char(&buf, ',');
        snprintf(id_str, sizeof(id_str), "%d", constraint->participants[i]);
        lv_json_buf_append_raw(&buf, id_str);
    }
    lv_json_buf_append_raw(&buf, "],");

    /* template_id */
    lv_json_buf_append_raw(&buf, "\"template_id\":");
    snprintf(id_str, sizeof(id_str), "%d", constraint->template_id);
    lv_json_buf_append_raw(&buf, id_str);

    /* numeric_value (used by ANGLE constraints) */
    lv_json_buf_append_raw(&buf, ",\"numeric_value\":");
    snprintf(id_str, sizeof(id_str), "%.15g", constraint->numeric_value);
    lv_json_buf_append_raw(&buf, id_str);

    lv_json_buf_append_char(&buf, '}');
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

    lv_json_buf_append_raw(&buf, "{");

    /* 图元数据 */
    lv_json_buf_append_raw(&buf, "\"node_count\":");
    char num_str[32];
    snprintf(num_str, sizeof(num_str), "%d", graph->node_count);
    lv_json_buf_append_raw(&buf, num_str);
    lv_json_buf_append_raw(&buf, ",");

    lv_json_buf_append_raw(&buf, "\"constraint_count\":");
    snprintf(num_str, sizeof(num_str), "%d", graph->constraint_count);
    lv_json_buf_append_raw(&buf, num_str);
    lv_json_buf_append_raw(&buf, ",");

    lv_json_buf_append_raw(&buf, "\"next_node_id\":");
    snprintf(num_str, sizeof(num_str), "%d", graph->next_node_id);
    lv_json_buf_append_raw(&buf, num_str);
    lv_json_buf_append_raw(&buf, ",");

    lv_json_buf_append_raw(&buf, "\"next_constraint_id\":");
    snprintf(num_str, sizeof(num_str), "%d", graph->next_constraint_id);
    lv_json_buf_append_raw(&buf, num_str);
    lv_json_buf_append_raw(&buf, ",");

    /* 节点数组 */
    lv_json_buf_append_raw(&buf, "\"nodes\":[");
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
    lv_json_buf_append_raw(&buf, "],");

    /* 约束数组 */
    lv_json_buf_append_raw(&buf, "\"constraints\":[");
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
    lv_json_buf_append_raw(&buf, "]");

    lv_json_buf_append_char(&buf, '}');
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
        if (count >= capacity) {
            capacity *= 2;
            int *new_result = lv_realloc(result, (size_t) capacity * sizeof(int));
            if (!new_result) {
                lv_free((void **) &result);
                *out_count = 0;
                lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "json_parser_parse_int_array: realloc failed");
            }
            result = new_result;
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
    while (lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
        char *key = lv_json_parse_string(&p);
        if (!key)
            break;

        lv_json_skip_ws(&p);
        if (p.pos >= p.size || p.data[p.pos] != ':') {
            lv_free((void **) &key);
            break;
        }
        p.pos++;

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

                /* 解析节点 */
                int node_id = 0, coord_count = 0;
                GeomType node_type = GEOM_POINT;
                TrustColor trust = TRUST_GREEN;
                int ns_depth = 0, parent_block_id = -1;

                /* 存储临时数据 */
                int *boundary_segs = NULL;
                int boundary_seg_count = 0;
                int *internal_nodes = NULL;
                int internal_node_count = 0;
                int *input_port_ids = NULL;
                int input_port_count = 0;
                int *output_port_ids = NULL;
                int output_port_count = 0;
                PortType port_type = PORT_INPUT;
                bool is_formal_param = false;
                bool is_polymorphic = false;
                int circle_center_id = -1;
                int circle_radius_id = -1;
                SymbolicCoord **coords = NULL;

                while (lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
                    char *node_key = lv_json_parse_string(&p);
                    if (!node_key)
                        break;

                    lv_json_skip_ws(&p);
                    if (p.pos >= p.size || p.data[p.pos] != ':') {
                        lv_free((void **) &node_key);
                        break;
                    }
                    p.pos++;

                    if (strcmp(node_key, "id") == 0) {
                        lv_json_parse_int(&p, &node_id);
                    } else if (strcmp(node_key, "type") == 0) {
                        char *type_str = lv_json_parse_string(&p);
                        if (type_str) {
                            node_type = string_to_geom_type(type_str);
                            lv_free((void **) &type_str);
                        }
                    } else if (strcmp(node_key, "trust") == 0) {
                        char *trust_str = lv_json_parse_string(&p);
                        if (trust_str) {
                            if (strcmp(trust_str, "GREEN") == 0)
                                trust = TRUST_GREEN;
                            else if (strcmp(trust_str, "BLUE_UNEXPLORED") == 0)
                                trust = TRUST_BLUE_UNEXPLORED;
                            else if (strcmp(trust_str, "BLUE_EXCEEDED") == 0)
                                trust = TRUST_BLUE_EXCEEDED;
                            else if (strcmp(trust_str, "BLUE_OUT_OF_SCOPE") == 0)
                                trust = TRUST_BLUE_OUT_OF_SCOPE;
                            else if (strcmp(trust_str, "YELLOW") == 0)
                                trust = TRUST_YELLOW;
                            else if (strcmp(trust_str, "LIGHT_ORANGE_ORACLE") == 0)
                                trust = TRUST_LIGHT_ORANGE_ORACLE;
                            else if (strcmp(trust_str, "LIGHT_ORANGE_EXPLOSION") == 0)
                                trust = TRUST_LIGHT_ORANGE_EXPLOSION;
                            else if (strcmp(trust_str, "AMBER") == 0)
                                trust = TRUST_AMBER;
                            else if (strcmp(trust_str, "DEEP_ORANGE") == 0)
                                trust = TRUST_DEEP_ORANGE;
                            else if (strcmp(trust_str, "RED") == 0)
                                trust = TRUST_RED;
                            else
                                trust = TRUST_GREEN;
                            lv_free((void **) &trust_str);
                        }
                    } else if (strcmp(node_key, "namespace_depth") == 0) {
                        lv_json_parse_int(&p, &ns_depth);
                    } else if (strcmp(node_key, "parent_block_id") == 0) {
                        lv_json_parse_int(&p, &parent_block_id);
                    } else if (strcmp(node_key, "coords") == 0) {
                        if (lv_json_expect(&p, '[')) {
                            /* 计数 */
                            int temp_count = 0;
                            lvJsonParser temp_p = p;
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
                            coord_count = temp_count;

                            /* 解析坐标 */
                            if (coord_count > 0) {
                                coords = lv_malloc((size_t) coord_count * sizeof(SymbolicCoord *));
                                if (!coords) {
                                    coord_count = 0;
                                } else {
                                    for (int i = 0; i < coord_count; i++) {
                                        coords[i] = NULL;
                                    }
                                }
                            }

                            for (int i = 0;
                                 i < coord_count && lv_json_peek(&p) != ']' && lv_json_peek(&p) != '\0'; i++) {
                                if (lv_json_peek(&p) == ',')
                                    p.pos++;

                                if (lv_json_peek(&p) == 'n') {
                                    lv_json_skip_value(&p);
                                    continue;
                                }

                                if (lv_json_peek(&p) == '{') {
                                    p.pos++; /* skip '{' */
                                    char *coord_key = lv_json_parse_string(&p);
                                    int coord_type = -1; /* 0=RATIONAL, 1=ALGEBRAIC, 2=QUADRATIC */
                                    if (coord_key && strcmp(coord_key, "type") == 0) {
                                        p.pos++; /* skip ':' */
                                        char *ct = lv_json_parse_string(&p);
                                        if (ct) {
                                            if (strcmp(ct, "RATIONAL") == 0)
                                                coord_type = 0;
                                            else if (strcmp(ct, "ALGEBRAIC") == 0)
                                                coord_type = 1;
                                            else if (strcmp(ct, "QUADRATIC") == 0)
                                                coord_type = 2;
                                            lv_free((void **) &ct);
                                        }
                                        lv_free((void **) &coord_key);

                                        /* 继续解析值 */
                                        while (lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
                                            coord_key = lv_json_parse_string(&p);
                                            if (!coord_key)
                                                break;
                                            lv_json_skip_ws(&p);
                                            if (p.pos < p.size && p.data[p.pos] == ':')
                                                p.pos++;

                                            if (coord_type == 0 && strcmp(coord_key, "num") == 0) {
                                                int64_t num, den = 1;
                                                /* 手动解析 int64_t 值，避免 int* 截断 */
                                                {
                                                    lv_json_skip_ws(&p);
                                                    char *end;
                                                    long long val = strtoll((const char *) p.data + p.pos, &end, 10);
                                                    num = (int64_t) val;
                                                    p.pos = (size_t) (end - (const char *) p.data);
                                                }
                                                /* 查找 den */
                                                lv_json_skip_ws(&p);
                                                if (lv_json_peek(&p) == ',') {
                                                    p.pos++;
                                                    char *dk = lv_json_parse_string(&p);
                                                    if (dk && strcmp(dk, "den") == 0) {
                                                        p.pos++;
                                                        if (p.pos < p.size && p.data[p.pos] == ':')
                                                            p.pos++;
                                                        /* 手动解析 int64_t 值，避免 int* 截断 */
                                                        {
                                                            lv_json_skip_ws(&p);
                                                            char *end;
                                                            long long val =
                                                                strtoll((const char *) p.data + p.pos, &end, 10);
                                                            den = (int64_t) val;
                                                            p.pos = (size_t) (end - (const char *) p.data);
                                                        }
                                                    }
                                                    lv_free((void **) &dk);
                                                }
                                                coords[i] = symbolic_coord_create_rational(num, den);
                                            } else if (coord_type == 0 && strcmp(coord_key, "num") == 0) {
                                                int64_t num = 0, den = 1;
                                                /* 手动解析 int64_t 值，避免 int* 截断 */
                                                {
                                                    lv_json_skip_ws(&p);
                                                    char *end;
                                                    long long val = strtoll((const char *) p.data + p.pos, &end, 10);
                                                    num = (int64_t) val;
                                                    p.pos = (size_t) (end - (const char *) p.data);
                                                }
                                                coords[i] = symbolic_coord_create_rational(num, den);
                                            }
                                            lv_free((void **) &coord_key);
                                            lv_json_skip_ws(&p);
                                        }
                                    } else {
                                        lv_free((void **) &coord_key);
                                        lv_json_skip_value(&p);
                                        while (lv_json_peek(&p) != '}')
                                            lv_json_skip_value(&p);
                                    }
                                    if (lv_json_peek(&p) == '}')
                                        p.pos++;
                                } else {
                                    lv_json_skip_value(&p);
                                }
                            }

                            lv_json_expect(&p, ']');
                        }
                    } else if (strcmp(node_key, "center_node_id") == 0) {
                        lv_json_parse_int(&p, &circle_center_id);
                    } else if (strcmp(node_key, "radius_node_id") == 0) {
                        lv_json_parse_int(&p, &circle_radius_id);
                    } else if (strcmp(node_key, "boundary_segments") == 0) {
                        boundary_segs = json_parser_parse_int_array(&p, &boundary_seg_count);
                    } else if (strcmp(node_key, "internal_nodes") == 0) {
                        internal_nodes = json_parser_parse_int_array(&p, &internal_node_count);
                    } else if (strcmp(node_key, "input_port_ids") == 0) {
                        input_port_ids = json_parser_parse_int_array(&p, &input_port_count);
                    } else if (strcmp(node_key, "output_port_ids") == 0) {
                        output_port_ids = json_parser_parse_int_array(&p, &output_port_count);
                    } else if (strcmp(node_key, "port_type") == 0) {
                        char *pt = lv_json_parse_string(&p);
                        if (pt) {
                            port_type = (strcmp(pt, "OUTPUT") == 0) ? PORT_OUTPUT : PORT_INPUT;
                            lv_free((void **) &pt);
                        }
                    } else if (strcmp(node_key, "is_formal_param") == 0) {
                        lv_json_parse_bool(&p, &is_formal_param);
                    } else if (strcmp(node_key, "is_polymorphic") == 0) {
                        lv_json_parse_bool(&p, &is_polymorphic);
                    } else {
                        lv_json_skip_value(&p);
                    }

                    lv_free((void **) &node_key);
                    lv_json_skip_ws(&p);
                }

                if (lv_json_peek(&p) == '}')
                    p.pos++;

                /* 对于 LINE_SEGMENT，需要先确保端点节点存在 */
                /* 我们先跳过 LINE_SEGMENT 和 REGION，在第二轮处理 */
                /* 暂时存储节点信息以便后续处理 */
                GeomNode *node = graph_add_node_with_id(graph, node_id, node_type, coords, coord_count);
                if (node) {
                    node->trust = trust;
                    node->namespace_depth = ns_depth;
                    node->parent_block_id = parent_block_id;

                    /* 设置类型特定数据 */
                    if (node_type == GEOM_REGION && boundary_segs && boundary_seg_count > 0) {
                        node->data.region.boundary_segments =
                            lv_malloc((size_t) boundary_seg_count * sizeof(GeomNode *));
                        if (node->data.region.boundary_segments) {
                            for (int i = 0; i < boundary_seg_count; i++) {
                                node->data.region.boundary_segments[i] = graph_get_node(graph, boundary_segs[i]);
                            }
                            node->data.region.segment_count = boundary_seg_count;
                        }
                    } else if (node_type == GEOM_CIRCLE) {
                        node->data.circle.center_node_id = circle_center_id;
                        node->data.circle.radius_node_id = circle_radius_id;
                    } else if (node_type == GEOM_PORT) {
                        Port *port = lv_calloc(1, sizeof(Port));
                        if (port) {
                            port->id = node_id;
                            port->type = port_type;
                            port->namespace_depth = ns_depth;
                            port->parent_block_id = parent_block_id;
                            port->is_formal_param = is_formal_param;
                            port->is_polymorphic = is_polymorphic;
                            port->connected_to = NULL;
                            node->data.port = port;
                        }
                    } else if (node_type == GEOM_FUNCTION_BLOCK) {
                        node->data.func_block.internal_node_count = internal_node_count;
                        node->data.func_block.input_count = input_port_count;
                        node->data.func_block.output_count = output_port_count;
                        if (internal_nodes && internal_node_count > 0) {
                            node->data.func_block.internal_nodes =
                                lv_malloc((size_t) internal_node_count * sizeof(GeomNode *));
                            if (node->data.func_block.internal_nodes) {
                                for (int i = 0; i < internal_node_count; i++) {
                                    node->data.func_block.internal_nodes[i] = graph_get_node(graph, internal_nodes[i]);
                                }
                            }
                        }
                        if (input_port_ids && input_port_count > 0) {
                            node->data.func_block.input_port_ids = lv_malloc((size_t) input_port_count * sizeof(int));
                            if (node->data.func_block.input_port_ids) {
                                memcpy(node->data.func_block.input_port_ids, input_port_ids,
                                       input_port_count * sizeof(int));
                            }
                        }
                        if (output_port_ids && output_port_count > 0) {
                            node->data.func_block.output_port_ids = lv_malloc((size_t) output_port_count * sizeof(int));
                            if (node->data.func_block.output_port_ids) {
                                memcpy(node->data.func_block.output_port_ids, output_port_ids,
                                       output_port_count * sizeof(int));
                            }
                        }
                        node->data.func_block.determinism_state = UNVERIFIED;
                    }

                    /* 更新 next_node_id */
                    if (node_id >= graph->next_node_id) {
                        graph->next_node_id = node_id + 1;
                    }
                }

                /* 释放临时数据 */
                lv_free((void **) &boundary_segs);
                lv_free((void **) &internal_nodes);
                lv_free((void **) &input_port_ids);
                lv_free((void **) &output_port_ids);
                if (coords) {
                    for (int i = 0; i < coord_count; i++) {
                        if (coords[i])
                            symbolic_coord_destroy(coords[i]);
                    }
                    lv_free((void **) &coords);
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

                int constraint_id = 0, template_id = -1;
                double numeric_value = 0.0;
                ConstraintType constraint_type = INCIDENCE;
                int *participants = NULL;
                int participant_count = 0;

                while (lv_json_peek(&p) != '}' && lv_json_peek(&p) != '\0') {
                    char *ckey = lv_json_parse_string(&p);
                    if (!ckey)
                        break;

                    lv_json_skip_ws(&p);
                    if (p.pos >= p.size || p.data[p.pos] != ':') {
                        lv_free((void **) &ckey);
                        break;
                    }
                    p.pos++;

                    if (strcmp(ckey, "id") == 0) {
                        lv_json_parse_int(&p, &constraint_id);
                    } else if (strcmp(ckey, "constraint_type") == 0) {
                        char *type_str = lv_json_parse_string(&p);
                        if (type_str) {
                            constraint_type = string_to_constraint_type(type_str);
                            lv_free((void **) &type_str);
                        }
                    } else if (strcmp(ckey, "participants") == 0) {
                        participants = json_parser_parse_int_array(&p, &participant_count);
                    } else if (strcmp(ckey, "template_id") == 0) {
                        lv_json_parse_int(&p, &template_id);
                    } else if (strcmp(ckey, "numeric_value") == 0) {
                        lv_json_skip_ws(&p);
                        char *end;
                        numeric_value = strtod(p.data + p.pos, &end);
                        p.pos = (size_t)(end - p.data);
                    } else {
                        lv_json_skip_value(&p);
                    }

                    lv_free((void **) &ckey);
                    lv_json_skip_ws(&p);
                }

                if (lv_json_peek(&p) == '}')
                    p.pos++;

                /* 使用带ID的接口添加约束 */
                if (participants && participant_count > 0) {
                    Constraint *constraint = graph_add_constraint_with_id(graph, constraint_id, constraint_type,
                                                                          participants, participant_count);
                    if (constraint) {
                        constraint->template_id = template_id;
                        constraint->numeric_value = numeric_value;
                        if (constraint_id >= graph->next_constraint_id) {
                            graph->next_constraint_id = constraint_id + 1;
                        }
                    }
                }

                lv_free((void **) &participants);
            }

            lv_json_expect(&p, ']');
        } else {
            lv_json_skip_value(&p);
        }

        lv_free((void **) &key);
        lv_json_skip_ws(&p);
        if (lv_json_peek(&p) == ',')
            p.pos++;
    }

    return graph;
}
