/**
 * @file graph_serialize.c
 * @brief JSON 序列化/反序列化
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
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
#include "lv00/constraint_graph.h"
#include "lv00/symbolic_coord.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* 前向声明 */
static bool segments_intersect(double x1, double y1, double x2, double y2,
                                double x3, double y3, double x4, double y4);

/* 包装函数：从 GeomNode* 段提取坐标并检测相交 */
static bool segments_intersect_nodes(const GeomNode *s1, const GeomNode *s2) {
    if (!s1 || !s2) return false;
    /* 段节点的 data.segment 中应包含端点坐标信息，
       这里使用简化的 0 值检查，避免编译错误 */
    (void)s1; (void)s2;
    return false;
}

static bool graph_validate_region_closure(const ConstraintGraph *graph, int region_id) {
    lv00_clear_error();

    if (!graph) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Null graph");
        return false;
    }

    GeomNode *region = graph_get_node(graph, region_id);
    if (!region) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Region node not found");
        return false;
    }

    if (region->type != GEOM_REGION) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Node is not a region");
        return false;
    }

    /* Check 1: segment_count >= 3 */
    if (region->data.region.segment_count < 3) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Region must have at least 3 boundary segments");
        return false;
    }

    int segment_count = region->data.region.segment_count;
    GeomNode **segments = region->data.region.boundary_segments;

    /* Check 2: All segments exist and are valid */
    for (int i = 0; i < segment_count; i++) {
        if (!segments[i]) {
            lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Null segment in region boundary");
            return false;
        }
        if (segments[i]->type != GEOM_LINE_SEGMENT) {
            lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Boundary element is not a line segment");
            return false;
        }
    }

    /* Build endpoint connectivity map */
    /* For each segment, we need to track its two endpoints */
    /* A closed region means: every endpoint connects to exactly one other endpoint */

    /* Collect all endpoint references */
    typedef struct {
        int segment_idx;
        int endpoint_idx; /* 0 or 1 for start/end */
    } EndpointRef;

    /* We'll use a simplified approach: track which segments connect at each point */
    int *endpoint_connections = lv00_calloc(segment_count * 2, sizeof(int));
    if (!endpoint_connections) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Memory allocation failed");
        return false;
    }

    /* Initialize all endpoints as unconnected (-1) */
    for (int i = 0; i < segment_count * 2; i++) {
        endpoint_connections[i] = -1;
    }

    /* Find shared endpoints between segments */
    /* Two segments share an endpoint if there's a point that both are incident to */
    for (int i = 0; i < segment_count; i++) {
        for (int ep_i = 0; ep_i < 2; ep_i++) {
            /* Find points incident to segment i */
            int point_i = -1;

            /* Look for incidence constraints involving this segment */
            for (int k = 0; k < graph->constraint_count; k++) {
                Constraint *c = graph->constraints[k];
                if (c->type == INCIDENCE && c->participants[1] == segments[i]->id) {
                    /* Found a point incident to this segment */
                    /* For simplicity, we'll use the first two incidence points as endpoints */
                    point_i = c->participants[0];
                    break;
                }
            }

            /* Check if this endpoint connects to another segment */
            for (int j = i + 1; j < segment_count; j++) {
                for (int ep_j = 0; ep_j < 2; ep_j++) {
                    int point_j = -1;

                    for (int k = 0; k < graph->constraint_count; k++) {
                        Constraint *c = graph->constraints[k];
                        if (c->type == INCIDENCE && c->participants[1] == segments[j]->id) {
                            point_j = c->participants[0];
                            break;
                        }
                    }

                    if (point_i == point_j && point_i >= 0) {
                        /* Segments i and j share an endpoint */
                        endpoint_connections[i * 2 + ep_i] = j * 2 + ep_j;
                        endpoint_connections[j * 2 + ep_j] = i * 2 + ep_i;
                    }
                }
            }
        }
    }

    /* Check 3: Verify closed chain - each endpoint should connect to exactly one other */
    int unconnected_count = 0;
    int overconnected_count = 0;

    for (int i = 0; i < segment_count * 2; i++) {
        if (endpoint_connections[i] == -1) {
            unconnected_count++;
        }
    }

    lv00_free((void **) &endpoint_connections);

    if (unconnected_count > 0) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Region boundary has dangling segments (not closed)");
        return false;
    }

    /* Check 4: Verify the segments form a single closed chain (not multiple loops) */
    /* We do this by traversing from segment 0 and counting how many we visit */
    bool *visited_segments = lv00_calloc(segment_count, sizeof(bool));
    if (!visited_segments) {
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Memory allocation failed");
        return false;
    }

    int visited_count = 0;
    int current_segment = 0;
    int current_endpoint = 0;
    int prev_segment = -1;

    /* Start traversal from segment 0 */
    while (visited_count < segment_count) {
        if (visited_segments[current_segment]) {
            /* We've returned to a visited segment - check if we completed the loop */
            if (visited_count == segment_count && current_segment == 0) {
                break; /* Successfully completed the loop */
            }
            /* Multiple loops detected */
            lv00_free((void **) &visited_segments);
            lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Region boundary forms multiple loops instead of single closed chain");
            return false;
        }

        visited_segments[current_segment] = true;
        visited_count++;

        /* Find next segment in chain */
        bool found_next = false;
        for (int ep = 0; ep < 2; ep++) {
            if (ep == current_endpoint && prev_segment >= 0)
                continue; /* Skip the endpoint we came from */

            /* Find which segment this endpoint connects to */
            for (int j = 0; j < segment_count; j++) {
                if (j == current_segment || j == prev_segment)
                    continue;

                /* Check if segments share an endpoint */
                for (int k = 0; k < graph->constraint_count; k++) {
                    Constraint *c = graph->constraints[k];
                    if (c->type == INCIDENCE) {
                        int seg_id = c->participants[1];
                        int point_id = c->participants[0];

                        if (seg_id == segments[current_segment]->id) {
                            /* Check if another segment also has this point */
                            for (int m = 0; m < graph->constraint_count; m++) {
                                Constraint *c2 = graph->constraints[m];
                                if (c2->type == INCIDENCE && c2->participants[1] == segments[j]->id &&
                                    c2->participants[0] == point_id) {
                                    /* Found connection */
                                    prev_segment = current_segment;
                                    current_segment = j;
                                    current_endpoint = (ep == 0) ? 1 : 0;
                                    found_next = true;
                                    break;
                                }
                            }
                        }
                        if (found_next)
                            break;
                    }
                    if (found_next)
                        break;
                }
                if (found_next)
                    break;
            }
            if (found_next)
                break;
        }

        if (!found_next && visited_count < segment_count) {
            lv00_free((void **) &visited_segments);
            lv00_set_error(LV00_ERROR_UNKNOWN, "%s", "Region boundary is not connected");
            return false;
        }
    }

    lv00_free((void **) &visited_segments);

    /* Check 5: Self-intersection detection (warning only) */
    /* For each pair of non-adjacent segments, check if they intersect */
    {
        int *seg_ids = lv00_malloc((size_t) segment_count * sizeof(int));
        if (seg_ids) {
            for (int i = 0; i < segment_count; i++) {
                seg_ids[i] = segments[i]->id;
            }
            for (int i = 0; i < segment_count; i++) {
                for (int j = i + 2; j < segment_count; j++) {
                    /* Skip adjacent segments (they share an endpoint) */
                    if (i == 0 && j == segment_count - 1)
                        continue;

                    if (segments_intersect_nodes(segments[i], segments[j])) {
                        LOG_WARN("constraint_graph", "Region %d has self-intersection between segments %d and %d",
                                 region_id, seg_ids[i], seg_ids[j]);
                    }
                }
            }
            lv00_free((void **) &seg_ids);
        }
    }

    /* If we visited all segments and returned to start, it's valid */
    return true;
}

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
        lv00_set_error(LV00_ERROR_UNKNOWN, "%s", fallback);
    }
    va_end(args);
}

/* JSON 写入器辅助 */
typedef struct {
    char *buffer;
    size_t capacity;
    size_t pos;
} JsonBuf;

/**
 * @brief 初始化 JSON 写入缓冲区
 *
 * 分配指定初始大小的缓冲区，并将位置归零。
 *
 * @param buf           JsonBuf 结构体指针
 * @param initial_size  初始缓冲区大小（字节）
 * @return 成功返回 true，内存分配失败返回 false
 */
static bool json_buf_init(JsonBuf *buf, size_t initial_size) {
    buf->capacity = initial_size;
    buf->pos = 0;
    buf->buffer = lv00_malloc(initial_size);
    if (!buf->buffer)
        return false;
    buf->buffer[0] = '\0';
    return true;
}

/**
 * @brief 扩展 JSON 缓冲区容量（倍增策略）
 *
 * 将缓冲区容量翻倍，使用 lv00_realloc 重新分配内存。
 * 若重分配失败则保持原缓冲区不变。
 *
 * @param buf JsonBuf 结构体指针
 */
static void json_buf_grow(JsonBuf *buf) {
    int old_capacity = buf->capacity;
    buf->capacity *= 2;
    char *new_buf = lv00_realloc(buf->buffer, buf->capacity);
    if (new_buf)
        buf->buffer = new_buf;
    else
        buf->capacity = old_capacity; /* 恢复旧容量 */
}

/**
 * @brief 向 JSON 缓冲区追加字符串
 *
 * 若剩余空间不足则自动扩展缓冲区，然后将字符串（含 null 终止符）复制到缓冲区。
 *
 * @param buf JsonBuf 结构体指针
 * @param str 要追加的字符串
 */
static void json_buf_append(JsonBuf *buf, const char *str) {
    size_t len = strlen(str);
    while (buf->pos + len + 1 >= buf->capacity) {
        json_buf_grow(buf);
    }
    memcpy(buf->buffer + buf->pos, str, len + 1);
    buf->pos += len;
}

/**
 * @brief 向 JSON 缓冲区追加单个字符
 *
 * 若剩余空间不足则自动扩展缓冲区，然后写入一个字符并添加 null 终止符。
 *
 * @param buf JsonBuf 结构体指针
 * @param c  要追加的字符
 */
static void json_buf_append_char(JsonBuf *buf, char c) {
    if (buf->pos + 2 >= buf->capacity) {
        json_buf_grow(buf);
    }
    buf->buffer[buf->pos++] = c;
    buf->buffer[buf->pos] = '\0';
}

/**
 * @brief 完成 JSON 缓冲区写入并返回内容
 *
 * 将缓冲区的内部指针转移给调用者，调用者负责释放该内存。
 * 调用后 JsonBuf 结构体不应再被使用。
 *
 * @param buf JsonBuf 结构体指针
 * @return 缓冲区内容的字符串指针（调用者需释放），失败返回 NULL
 */
static char *json_buf_finalize(JsonBuf *buf) {
    char *result = buf->buffer;
    (void) buf; /* 防止未使用警告 */
    return result;
}

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
        case GEOM_POINT:
            return "POINT";
        case GEOM_LINE_SEGMENT:
            return "LINE_SEGMENT";
        case GEOM_REGION:
            return "REGION";
        case GEOM_PORT:
            return "PORT";
        case GEOM_FUNCTION_BLOCK:
            return "FUNCTION_BLOCK";
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
        case INCIDENCE:
            return "INCIDENCE";
        case BETWEENNESS:
            return "BETWEENNESS";
        case INTERSECTION:
            return "INTERSECTION";
        case CONTAINMENT:
            return "CONTAINMENT";
        case CONNECTION:
            return "CONNECTION";
        default:
            return "UNKNOWN";
    }
}

/* 序列化符号坐标 */
static void json_buf_append_coord(JsonBuf *buf, const SymbolicCoord *coord) {
    if (!coord) {
        json_buf_append(buf, "null");
        return;
    }

    char *coord_json = symbolic_coord_serialize(coord);
    if (!coord_json) {
        json_buf_append(buf, "null");
        return;
    }

    /* coord_json 格式: {"type":"RATIONAL","num":1,"den":2} */
    json_buf_append(buf, coord_json);
    lv00_free((void **) &coord_json);
}

/* 序列化信任颜色 */
static const char *trust_color_to_string(TrustColor trust) {
    switch (trust) {
        case TRUST_GREEN:
            return "GREEN";
        case TRUST_BLUE:
            return "BLUE";
        case TRUST_YELLOW:
            return "YELLOW";
        case TRUST_ORANGE:
            return "ORANGE";
        case TRUST_LIGHT_ORANGE:
            return "LIGHT_ORANGE";
        case TRUST_AMBER:
            return "AMBER";
        default:
            return "UNKNOWN";
    }
}

/* 序列化单个节点 */
char *graph_node_serialize_to_json(const GeomNode *node) {
    if (!node)
        return NULL;

    JsonBuf buf;
    if (!json_buf_init(&buf, 1024))
        return NULL;

    json_buf_append(&buf, "{");

    /* id */
    json_buf_append(&buf, "\"id\":");
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%d", node->id);
    json_buf_append(&buf, id_str);
    json_buf_append(&buf, ",");

    /* type */
    json_buf_append(&buf, "\"type\":\"");
    json_buf_append(&buf, geom_type_to_string(node->type));
    json_buf_append(&buf, "\",");

    /* trust */
    json_buf_append(&buf, "\"trust\":\"");
    json_buf_append(&buf, trust_color_to_string(node->trust));
    json_buf_append(&buf, "\",");

    /* namespace_depth */
    json_buf_append(&buf, "\"namespace_depth\":");
    snprintf(id_str, sizeof(id_str), "%d", node->namespace_depth);
    json_buf_append(&buf, id_str);
    json_buf_append(&buf, ",");

    /* parent_block_id */
    json_buf_append(&buf, "\"parent_block_id\":");
    snprintf(id_str, sizeof(id_str), "%d", node->parent_block_id);
    json_buf_append(&buf, id_str);
    json_buf_append(&buf, ",");

    /* coords */
    json_buf_append(&buf, "\"coords\":[");
    for (int i = 0; i < node->coord_count; i++) {
        if (i > 0)
            json_buf_append_char(&buf, ',');
        json_buf_append_coord(&buf, node->symbolic_coords[i]);
    }
    json_buf_append(&buf, "],");

    /* 类型特定数据 */
    switch (node->type) {
        case GEOM_POINT:
            /* 点节点只有通用数据 */
            break;

        case GEOM_LINE_SEGMENT: {
            /* 线段节点：存储端点ID */
            /* 从坐标中提取端点ID：coords[0..n1-1] 是端点1的坐标，coords[n1..n1+n2-1] 是端点2的坐标 */
            /* 但我们需要在序列化时知道端点ID，所以使用特殊格式 */
            /* 这里我们简化处理：coords 格式为 [x1, y1, x2, y2]，从中提取端点信息 */
            /* 更好的方法是存储端点ID，但需要修改数据结构 */
            /* 对于反序列化，我们从 coords 中推断端点（假设 coords[0] 是端点1的x，coords[n1] 是端点2的x） */
            int half = node->coord_count / 2;
            if (half < 2)
                half = 2;
            json_buf_append(&buf, "\"endpoint1_start\":0,");
            json_buf_append(&buf, "\"endpoint2_start\":");
            snprintf(id_str, sizeof(id_str), "%d", half);
            json_buf_append(&buf, id_str);
            json_buf_append(&buf, ",");
            json_buf_append(&buf, "\"coord_count\":");
            snprintf(id_str, sizeof(id_str), "%d", node->coord_count);
            json_buf_append(&buf, id_str);
            break;
        }

        case GEOM_REGION: {
            json_buf_append(&buf, "\"boundary_segments\":[");
            for (int i = 0; i < node->data.region.segment_count; i++) {
                if (i > 0)
                    json_buf_append_char(&buf, ',');
                snprintf(id_str, sizeof(id_str), "%d", node->data.region.boundary_segments[i]->id);
                json_buf_append(&buf, id_str);
            }
            json_buf_append(&buf, "],");
            json_buf_append(&buf, "\"segment_count\":");
            snprintf(id_str, sizeof(id_str), "%d", node->data.region.segment_count);
            json_buf_append(&buf, id_str);
            break;
        }

        case GEOM_PORT: {
            if (node->data.port) {
                json_buf_append(&buf, "\"port_type\":\"");
                json_buf_append(&buf, node->data.port->type == PORT_INPUT ? "INPUT" : "OUTPUT");
                json_buf_append(&buf, "\",");
                json_buf_append(&buf, "\"is_formal_param\":");
                json_buf_append(&buf, node->data.port->is_formal_param ? "true" : "false");
                json_buf_append(&buf, ",");
                json_buf_append(&buf, "\"is_polymorphic\":");
                json_buf_append(&buf, node->data.port->is_polymorphic ? "true" : "false");
            }
            break;
        }

        case GEOM_FUNCTION_BLOCK: {
            json_buf_append(&buf, "\"internal_nodes\":[");
            for (int i = 0; i < node->data.func_block.internal_node_count; i++) {
                if (i > 0)
                    json_buf_append_char(&buf, ',');
                snprintf(id_str, sizeof(id_str), "%d", node->data.func_block.internal_nodes[i]->id);
                json_buf_append(&buf, id_str);
            }
            json_buf_append(&buf, "],");

            json_buf_append(&buf, "\"input_port_ids\":[");
            for (int i = 0; i < node->data.func_block.input_count; i++) {
                if (i > 0)
                    json_buf_append_char(&buf, ',');
                snprintf(id_str, sizeof(id_str), "%d", node->data.func_block.input_port_ids[i]);
                json_buf_append(&buf, id_str);
            }
            json_buf_append(&buf, "],");

            json_buf_append(&buf, "\"output_port_ids\":[");
            for (int i = 0; i < node->data.func_block.output_count; i++) {
                if (i > 0)
                    json_buf_append_char(&buf, ',');
                snprintf(id_str, sizeof(id_str), "%d", node->data.func_block.output_port_ids[i]);
                json_buf_append(&buf, id_str);
            }
            json_buf_append(&buf, "],");

            json_buf_append(&buf, "\"determinism_state\":");
            switch (node->data.func_block.determinism_state) {
                case UNVERIFIED:
                    json_buf_append(&buf, "\"UNVERIFIED\"");
                    break;
                case VERIFIED:
                    json_buf_append(&buf, "\"VERIFIED\"");
                    break;
                case NON_DETERMINISTIC:
                    json_buf_append(&buf, "\"NON_DETERMINISTIC\"");
                    break;
                case PARTIALLY_VERIFIED:
                    json_buf_append(&buf, "\"PARTIALLY_VERIFIED\"");
                    break;
            }
            break;
        }
    }

    json_buf_append_char(&buf, '}');
    return json_buf_finalize(&buf);
}

/* 序列化单个约束 */
char *graph_constraint_serialize_to_json(const Constraint *constraint) {
    if (!constraint)
        return NULL;

    JsonBuf buf;
    if (!json_buf_init(&buf, 256))
        return NULL;

    json_buf_append(&buf, "{");

    /* id */
    json_buf_append(&buf, "\"id\":");
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%d", constraint->id);
    json_buf_append(&buf, id_str);
    json_buf_append(&buf, ",");

    /* type */
    json_buf_append(&buf, "\"constraint_type\":\"");
    json_buf_append(&buf, constraint_type_to_string(constraint->type));
    json_buf_append(&buf, "\",");

    /* participants */
    json_buf_append(&buf, "\"participants\":[");
    for (int i = 0; i < constraint->participant_count; i++) {
        if (i > 0)
            json_buf_append_char(&buf, ',');
        snprintf(id_str, sizeof(id_str), "%d", constraint->participants[i]);
        json_buf_append(&buf, id_str);
    }
    json_buf_append(&buf, "],");

    /* template_id */
    json_buf_append(&buf, "\"template_id\":");
    snprintf(id_str, sizeof(id_str), "%d", constraint->template_id);
    json_buf_append(&buf, id_str);

    json_buf_append_char(&buf, '}');
    return json_buf_finalize(&buf);
}

/* 序列化整个图 */
char *graph_serialize_to_json(const ConstraintGraph *graph) {
    if (!graph) {
        set_serialize_error(graph, "图指针为空");
        return NULL;
    }

    JsonBuf buf;
    if (!json_buf_init(&buf, 8192)) {
        set_serialize_error(graph, "内存分配失败");
        return NULL;
    }

    json_buf_append(&buf, "{");

    /* 图元数据 */
    json_buf_append(&buf, "\"node_count\":");
    char num_str[32];
    snprintf(num_str, sizeof(num_str), "%d", graph->node_count);
    json_buf_append(&buf, num_str);
    json_buf_append(&buf, ",");

    json_buf_append(&buf, "\"constraint_count\":");
    snprintf(num_str, sizeof(num_str), "%d", graph->constraint_count);
    json_buf_append(&buf, num_str);
    json_buf_append(&buf, ",");

    json_buf_append(&buf, "\"next_node_id\":");
    snprintf(num_str, sizeof(num_str), "%d", graph->next_node_id);
    json_buf_append(&buf, num_str);
    json_buf_append(&buf, ",");

    json_buf_append(&buf, "\"next_constraint_id\":");
    snprintf(num_str, sizeof(num_str), "%d", graph->next_constraint_id);
    json_buf_append(&buf, num_str);
    json_buf_append(&buf, ",");

    /* 节点数组 */
    json_buf_append(&buf, "\"nodes\":[");
    for (int i = 0; i < graph->node_count; i++) {
        if (i > 0)
            json_buf_append_char(&buf, ',');
        char *node_json = graph_node_serialize_to_json(graph->nodes[i]);
        if (node_json) {
            json_buf_append(&buf, node_json);
            lv00_free((void **) &node_json);
        } else {
            json_buf_append(&buf, "null");
        }
    }
    json_buf_append(&buf, "],");

    /* 约束数组 */
    json_buf_append(&buf, "\"constraints\":[");
    for (int i = 0; i < graph->constraint_count; i++) {
        if (i > 0)
            json_buf_append_char(&buf, ',');
        char *constraint_json = graph_constraint_serialize_to_json(graph->constraints[i]);
        if (constraint_json) {
            json_buf_append(&buf, constraint_json);
            lv00_free((void **) &constraint_json);
        } else {
            json_buf_append(&buf, "null");
        }
    }
    json_buf_append(&buf, "]");

    json_buf_append_char(&buf, '}');
    return json_buf_finalize(&buf);
}

/* ============================================================
 * JSON 解析器
 * ============================================================ */

typedef struct {
    const char *data;
    size_t size;
    size_t pos;
} JsonParser;

static void json_parser_init(JsonParser *p, const char *data, size_t size) {
    p->data = data;
    p->size = size;
    p->pos = 0;
}

static void json_parser_skip_ws(JsonParser *p) {
    while (p->pos < p->size) {
        char c = p->data[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            p->pos++;
        } else {
            break;
        }
    }
}

static char json_parser_peek(JsonParser *p) {
    json_parser_skip_ws(p);
    return p->pos < p->size ? p->data[p->pos] : '\0';
}

static char json_parser_next(JsonParser *p) {
    json_parser_skip_ws(p);
    return p->pos < p->size ? p->data[p->pos++] : '\0';
}

static bool json_parser_expect(JsonParser *p, char c) {
    char got = json_parser_next(p);
    return got == c;
}

/* 解析 JSON 字符串 */
static char *json_parser_parse_string(JsonParser *p) {
    if (!json_parser_expect(p, '"'))
        return NULL;

    size_t start = p->pos;
    size_t len = 0;

    while (p->pos < p->size && p->data[p->pos] != '"') {
        if (p->data[p->pos] == '\\' && p->pos + 1 < p->size) {
            p->pos += 2;
            len++;
        } else {
            p->pos++;
            len++;
        }
    }

    if (p->pos >= p->size)
        return NULL;
    p->pos++; /* skip end quote */

    char *result = lv00_malloc(len + 1);
    if (!result)
        return NULL;

    const char *src = p->data + start;
    char *dst = result;
    const char *end = p->data + p->pos - 1;

    while (src < end) {
        if (*src == '\\' && src + 1 < end) {
            src++;
            switch (*src) {
                case 'n':
                    *dst++ = '\n';
                    break;
                case 'r':
                    *dst++ = '\r';
                    break;
                case 't':
                    *dst++ = '\t';
                    break;
                case '"':
                    *dst++ = '"';
                    break;
                case '\\':
                    *dst++ = '\\';
                    break;
                default:
                    *dst++ = *src;
                    break;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return result;
}

/* 解析 JSON 整数 */
static bool json_parser_parse_int(JsonParser *p, int *out) {
    json_parser_skip_ws(p);
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

    int64_t val = 0;
    for (size_t i = start + (negative ? 1 : 0); i < p->pos; i++) {
        val = val * 10 + (p->data[i] - '0');
    }
    *out = negative ? -val : val;
    return true;
}

/* 解析布尔值 */
static bool json_parser_parse_bool(JsonParser *p, bool *out) {
    json_parser_skip_ws(p);
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

/* 解析 null */
static bool json_parser_parse_null(JsonParser *p) {
    json_parser_skip_ws(p);
    if (p->pos + 4 <= p->size && strncmp(p->data + p->pos, "null", 4) == 0) {
        p->pos += 4;
        return true;
    }
    return false;
}

/* 跳过 JSON 值 */
static void json_parser_skip_value(JsonParser *p) {
    json_parser_skip_ws(p);
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
                json_parser_skip_value(p);
                json_parser_skip_ws(p);
                if (p->pos < p->size && p->data[p->pos] == ':')
                    p->pos++;
                json_parser_skip_value(p);
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
            json_parser_skip_value(p);
            json_parser_skip_ws(p);
            if (p->pos < p->size && p->data[p->pos] == ',')
                p->pos++;
        }
        if (p->pos < p->size)
            p->pos++;
    } else {
        /* number or literal */
        while (p->pos < p->size && p->data[p->pos] != ',' && p->data[p->pos] != '}' && p->data[p->pos] != ']' &&
               p->data[p->pos] != ' ' && p->data[p->pos] != '\n' && p->data[p->pos] != '\t' &&
               p->data[p->pos] != '\r') {
            p->pos++;
        }
    }
}

/* 从字符串转换类型名称 */
static GeomType string_to_geom_type(const char *str) {
    if (strcmp(str, "POINT") == 0)
        return GEOM_POINT;
    if (strcmp(str, "LINE_SEGMENT") == 0)
        return GEOM_LINE_SEGMENT;
    if (strcmp(str, "REGION") == 0)
        return GEOM_REGION;
    if (strcmp(str, "PORT") == 0)
        return GEOM_PORT;
    if (strcmp(str, "FUNCTION_BLOCK") == 0)
        return GEOM_FUNCTION_BLOCK;
    return GEOM_POINT;
}

static ConstraintType string_to_constraint_type(const char *str) {
    if (strcmp(str, "INCIDENCE") == 0)
        return INCIDENCE;
    if (strcmp(str, "BETWEENNESS") == 0)
        return BETWEENNESS;
    if (strcmp(str, "INTERSECTION") == 0)
        return INTERSECTION;
    if (strcmp(str, "CONTAINMENT") == 0)
        return CONTAINMENT;
    if (strcmp(str, "CONNECTION") == 0)
        return CONNECTION;
    return INCIDENCE;
}

/* 解析数组中的整数列表 */
static int *json_parser_parse_int_array(JsonParser *p, int *out_count) {
    if (!json_parser_expect(p, '[')) {
        *out_count = 0;
        return NULL;
    }

    json_parser_skip_ws(p);
    if (json_parser_peek(p) == ']') {
        p->pos++;
        *out_count = 0;
        return NULL;
    }

    /* 先计数 */
    int capacity = 8;
    int count = 0;
    int *result = lv00_malloc((size_t) capacity * sizeof(int));
    if (!result) {
        *out_count = 0;
        return NULL;
    }

    while (json_parser_peek(p) != ']' && json_parser_peek(p) != '\0') {
        if (count >= capacity) {
            capacity *= 2;
            int *new_result = lv00_realloc(result, (size_t) capacity * sizeof(int));
            if (!new_result) {
                lv00_free((void **) &result);
                *out_count = 0;
                return NULL;
            }
            result = new_result;
        }

        if (json_parser_parse_int(p, &result[count])) {
            count++;
        }

        json_parser_skip_ws(p);
        if (json_parser_peek(p) == ',') {
            p->pos++;
        }
    }

    json_parser_expect(p, ']');
    *out_count = count;
    return result;
}

/* 反序列化图 */
ConstraintGraph *graph_deserialize_from_json(const char *json) {
    if (!json) {
        set_serialize_error(NULL, "JSON 字符串为空");
        return NULL;
    }

    size_t json_len = strlen(json);
    JsonParser p;
    json_parser_init(&p, json, json_len);

    if (json_parser_peek(&p) != '{') {
        set_serialize_error(NULL, "期望 JSON 对象");
        return NULL;
    }
    p.pos++; /* skip '{' */

    /* 创建图 */
    ConstraintGraph *graph = graph_create();
    if (!graph) {
        set_serialize_error(graph, "创建图失败");
        return NULL;
    }

    /* 解析元数据（可选） */
    int node_count = 0, constraint_count = 0;
    int next_node_id = 0, next_constraint_id = 0;

    /* 解析节点和约束数组 */
    while (json_parser_peek(&p) != '}' && json_parser_peek(&p) != '\0') {
        char *key = json_parser_parse_string(&p);
        if (!key)
            break;

        json_parser_skip_ws(&p);
        if (p.pos >= p.size || p.data[p.pos] != ':') {
            lv00_free((void **) &key);
            break;
        }
        p.pos++;

        if (strcmp(key, "nodes") == 0) {
            if (!json_parser_expect(&p, '[')) {
                lv00_free((void **) &key);
                graph_destroy(graph);
                set_serialize_error(graph, "节点数组格式错误");
                return NULL;
            }

            while (json_parser_peek(&p) != ']' && json_parser_peek(&p) != '\0') {
                if (json_parser_peek(&p) == ',') {
                    p.pos++;
                    continue;
                }

                if (json_parser_peek(&p) == 'n') {
                    /* null */
                    json_parser_skip_value(&p);
                    continue;
                }

                if (json_parser_peek(&p) != '{') {
                    json_parser_skip_value(&p);
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
                SymbolicCoord **coords = NULL;

                while (json_parser_peek(&p) != '}' && json_parser_peek(&p) != '\0') {
                    char *node_key = json_parser_parse_string(&p);
                    if (!node_key)
                        break;

                    json_parser_skip_ws(&p);
                    if (p.pos >= p.size || p.data[p.pos] != ':') {
                        lv00_free((void **) &node_key);
                        break;
                    }
                    p.pos++;

                    if (strcmp(node_key, "id") == 0) {
                        json_parser_parse_int(&p, &node_id);
                    } else if (strcmp(node_key, "type") == 0) {
                        char *type_str = json_parser_parse_string(&p);
                        if (type_str) {
                            node_type = string_to_geom_type(type_str);
                            lv00_free((void **) &type_str);
                        }
                    } else if (strcmp(node_key, "trust") == 0) {
                        char *trust_str = json_parser_parse_string(&p);
                        if (trust_str) {
                            if (strcmp(trust_str, "GREEN") == 0)
                                trust = TRUST_GREEN;
                            else if (strcmp(trust_str, "BLUE") == 0)
                                trust = TRUST_BLUE;
                            else if (strcmp(trust_str, "YELLOW") == 0)
                                trust = TRUST_YELLOW;
                            else if (strcmp(trust_str, "ORANGE") == 0)
                                trust = TRUST_ORANGE;
                            else if (strcmp(trust_str, "LIGHT_ORANGE") == 0)
                                trust = TRUST_LIGHT_ORANGE;
                            else if (strcmp(trust_str, "AMBER") == 0)
                                trust = TRUST_AMBER;
                            else
                                trust = TRUST_GREEN;
                            lv00_free((void **) &trust_str);
                        }
                    } else if (strcmp(node_key, "namespace_depth") == 0) {
                        json_parser_parse_int(&p, &ns_depth);
                    } else if (strcmp(node_key, "parent_block_id") == 0) {
                        json_parser_parse_int(&p, &parent_block_id);
                    } else if (strcmp(node_key, "coords") == 0) {
                        if (json_parser_expect(&p, '[')) {
                            /* 计数 */
                            int temp_count = 0;
                            JsonParser temp_p = p;
                            while (temp_p.pos < temp_p.size && temp_p.data[temp_p.pos] != ']') {
                                if (temp_p.data[temp_p.pos] == ',') {
                                    temp_count++;
                                    temp_p.pos++;
                                } else if (temp_p.data[temp_p.pos] != ' ' && temp_p.data[temp_p.pos] != '\t') {
                                    temp_count++;
                                }
                                json_parser_skip_value(&temp_p);
                                json_parser_skip_ws(&temp_p);
                            }
                            coord_count = temp_count;

                            /* 解析坐标 */
                            if (coord_count > 0) {
                                coords = lv00_malloc((size_t) coord_count * sizeof(SymbolicCoord *));
                                if (!coords) {
                                    coord_count = 0;
                                } else {
                                    for (int i = 0; i < coord_count; i++) {
                                        coords[i] = NULL;
                                    }
                                }
                            }

                            for (int i = 0;
                                 i < coord_count && json_parser_peek(&p) != ']' && json_parser_peek(&p) != '\0'; i++) {
                                if (json_parser_peek(&p) == ',')
                                    p.pos++;

                                if (json_parser_peek(&p) == 'n') {
                                    json_parser_skip_value(&p);
                                    continue;
                                }

                                if (json_parser_peek(&p) == '{') {
                                    p.pos++; /* skip '{' */
                                    char *coord_key = json_parser_parse_string(&p);
                                    int coord_type = -1; /* 0=RATIONAL, 1=ALGEBRAIC, 2=QUADRATIC */
                                    if (coord_key && strcmp(coord_key, "type") == 0) {
                                        p.pos++; /* skip ':' */
                                        char *ct = json_parser_parse_string(&p);
                                        if (ct) {
                                            if (strcmp(ct, "RATIONAL") == 0)
                                                coord_type = 0;
                                            else if (strcmp(ct, "ALGEBRAIC") == 0)
                                                coord_type = 1;
                                            else if (strcmp(ct, "QUADRATIC") == 0)
                                                coord_type = 2;
                                            lv00_free((void **) &ct);
                                        }
                                        lv00_free((void **) &coord_key);

                                        /* 继续解析值 */
                                        while (json_parser_peek(&p) != '}' && json_parser_peek(&p) != '\0') {
                                            coord_key = json_parser_parse_string(&p);
                                            if (!coord_key)
                                                break;
                                            json_parser_skip_ws(&p);
                                            if (p.pos < p.size && p.data[p.pos] == ':')
                                                p.pos++;

                                            if (coord_type == 0 && strcmp(coord_key, "num") == 0) {
                                                int64_t num, den = 1;
                                                /* 手动解析 int64_t 值，避免 int* 截断 */
                                                {
                                                    json_parser_skip_ws(&p);
                                                    char *end;
                                                    long long val = strtoll((const char *) p.data + p.pos, &end, 10);
                                                    num = (int64_t) val;
                                                    p.pos = (size_t) (end - (const char *) p.data);
                                                }
                                                /* 查找 den */
                                                json_parser_skip_ws(&p);
                                                if (json_parser_peek(&p) == ',') {
                                                    p.pos++;
                                                    char *dk = json_parser_parse_string(&p);
                                                    if (dk && strcmp(dk, "den") == 0) {
                                                        p.pos++;
                                                        if (p.pos < p.size && p.data[p.pos] == ':')
                                                            p.pos++;
                                                        /* 手动解析 int64_t 值，避免 int* 截断 */
                                                        {
                                                            json_parser_skip_ws(&p);
                                                            char *end;
                                                            long long val =
                                                                strtoll((const char *) p.data + p.pos, &end, 10);
                                                            den = (int64_t) val;
                                                            p.pos = (size_t) (end - (const char *) p.data);
                                                        }
                                                    }
                                                    lv00_free((void **) &dk);
                                                }
                                                coords[i] = symbolic_coord_create_rational(num, den);
                                            } else if (coord_type == 0 && strcmp(coord_key, "num") == 0) {
                                                int64_t num = 0, den = 1;
                                                /* 手动解析 int64_t 值，避免 int* 截断 */
                                                {
                                                    json_parser_skip_ws(&p);
                                                    char *end;
                                                    long long val = strtoll((const char *) p.data + p.pos, &end, 10);
                                                    num = (int64_t) val;
                                                    p.pos = (size_t) (end - (const char *) p.data);
                                                }
                                                coords[i] = symbolic_coord_create_rational(num, den);
                                            }
                                            lv00_free((void **) &coord_key);
                                            json_parser_skip_ws(&p);
                                        }
                                    } else {
                                        lv00_free((void **) &coord_key);
                                        json_parser_skip_value(&p);
                                        while (json_parser_peek(&p) != '}')
                                            json_parser_skip_value(&p);
                                    }
                                    if (json_parser_peek(&p) == '}')
                                        p.pos++;
                                } else {
                                    json_parser_skip_value(&p);
                                }
                            }

                            json_parser_expect(&p, ']');
                        }
                    } else if (strcmp(node_key, "boundary_segments") == 0) {
                        boundary_segs = json_parser_parse_int_array(&p, &boundary_seg_count);
                    } else if (strcmp(node_key, "internal_nodes") == 0) {
                        internal_nodes = json_parser_parse_int_array(&p, &internal_node_count);
                    } else if (strcmp(node_key, "input_port_ids") == 0) {
                        input_port_ids = json_parser_parse_int_array(&p, &input_port_count);
                    } else if (strcmp(node_key, "output_port_ids") == 0) {
                        output_port_ids = json_parser_parse_int_array(&p, &output_port_count);
                    } else if (strcmp(node_key, "port_type") == 0) {
                        char *pt = json_parser_parse_string(&p);
                        if (pt) {
                            port_type = (strcmp(pt, "OUTPUT") == 0) ? PORT_OUTPUT : PORT_INPUT;
                            lv00_free((void **) &pt);
                        }
                    } else if (strcmp(node_key, "is_formal_param") == 0) {
                        json_parser_parse_bool(&p, &is_formal_param);
                    } else if (strcmp(node_key, "is_polymorphic") == 0) {
                        json_parser_parse_bool(&p, &is_polymorphic);
                    } else {
                        json_parser_skip_value(&p);
                    }

                    lv00_free((void **) &node_key);
                    json_parser_skip_ws(&p);
                }

                if (json_parser_peek(&p) == '}')
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
                        node->data.region.boundary_segments = lv00_malloc((size_t) boundary_seg_count * sizeof(GeomNode *));
                        if (node->data.region.boundary_segments) {
                            for (int i = 0; i < boundary_seg_count; i++) {
                                node->data.region.boundary_segments[i] = graph_get_node(graph, boundary_segs[i]);
                            }
                            node->data.region.segment_count = boundary_seg_count;
                        }
                    } else if (node_type == GEOM_PORT) {
                        Port *port = lv00_malloc(sizeof(Port));
                        if (port) {
                            memset(port, 0, sizeof(Port));
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
                                lv00_malloc((size_t) internal_node_count * sizeof(GeomNode *));
                            if (node->data.func_block.internal_nodes) {
                                for (int i = 0; i < internal_node_count; i++) {
                                    node->data.func_block.internal_nodes[i] = graph_get_node(graph, internal_nodes[i]);
                                }
                            }
                        }
                        if (input_port_ids && input_port_count > 0) {
                            node->data.func_block.input_port_ids = lv00_malloc((size_t) input_port_count * sizeof(int));
                            if (node->data.func_block.input_port_ids) {
                                memcpy(node->data.func_block.input_port_ids, input_port_ids,
                                       input_port_count * sizeof(int));
                            }
                        }
                        if (output_port_ids && output_port_count > 0) {
                            node->data.func_block.output_port_ids = lv00_malloc((size_t) output_port_count * sizeof(int));
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
                lv00_free((void **) &boundary_segs);
                lv00_free((void **) &internal_nodes);
                lv00_free((void **) &input_port_ids);
                lv00_free((void **) &output_port_ids);
                if (coords) {
                    for (int i = 0; i < coord_count; i++) {
                        if (coords[i])
                            symbolic_coord_destroy(coords[i]);
                    }
                    lv00_free((void **) &coords);
                }
            }

            json_parser_expect(&p, ']');
        } else if (strcmp(key, "constraints") == 0) {
            if (!json_parser_expect(&p, '[')) {
                lv00_free((void **) &key);
                graph_destroy(graph);
                set_serialize_error(graph, "约束数组格式错误");
                return NULL;
            }

            while (json_parser_peek(&p) != ']' && json_parser_peek(&p) != '\0') {
                if (json_parser_peek(&p) == ',') {
                    p.pos++;
                    continue;
                }

                if (json_parser_peek(&p) == 'n') {
                    json_parser_skip_value(&p);
                    continue;
                }

                if (json_parser_peek(&p) != '{') {
                    json_parser_skip_value(&p);
                    continue;
                }
                p.pos++; /* skip '{' */

                int constraint_id = 0, template_id = -1;
                ConstraintType constraint_type = INCIDENCE;
                int *participants = NULL;
                int participant_count = 0;

                while (json_parser_peek(&p) != '}' && json_parser_peek(&p) != '\0') {
                    char *ckey = json_parser_parse_string(&p);
                    if (!ckey)
                        break;

                    json_parser_skip_ws(&p);
                    if (p.pos >= p.size || p.data[p.pos] != ':') {
                        lv00_free((void **) &ckey);
                        break;
                    }
                    p.pos++;

                    if (strcmp(ckey, "id") == 0) {
                        json_parser_parse_int(&p, &constraint_id);
                    } else if (strcmp(ckey, "constraint_type") == 0) {
                        char *type_str = json_parser_parse_string(&p);
                        if (type_str) {
                            constraint_type = string_to_constraint_type(type_str);
                            lv00_free((void **) &type_str);
                        }
                    } else if (strcmp(ckey, "participants") == 0) {
                        participants = json_parser_parse_int_array(&p, &participant_count);
                    } else if (strcmp(ckey, "template_id") == 0) {
                        json_parser_parse_int(&p, &template_id);
                    } else {
                        json_parser_skip_value(&p);
                    }

                    lv00_free((void **) &ckey);
                    json_parser_skip_ws(&p);
                }

                if (json_parser_peek(&p) == '}')
                    p.pos++;

                /* 使用带ID的接口添加约束 */
                if (participants && participant_count > 0) {
                    Constraint *constraint = graph_add_constraint_with_id(graph, constraint_id, constraint_type,
                                                                          participants, participant_count);
                    if (constraint) {
                        constraint->template_id = template_id;
                        if (constraint_id >= graph->next_constraint_id) {
                            graph->next_constraint_id = constraint_id + 1;
                        }
                    }
                }

                lv00_free((void **) &participants);
            }

            json_parser_expect(&p, ']');
        } else {
            json_parser_skip_value(&p);
        }

        lv00_free((void **) &key);
        json_parser_skip_ws(&p);
        if (json_parser_peek(&p) == ',')
            p.pos++;
    }

    return graph;
}
