/**
 * @file formula_converter_complex.c
 * @brief 公式转换器实现 —— 复合几何对象转换：多边形/区域/弧
 *
 * @details 由 formula_converter.c 按功能边界拆分而来，
 *          属于公式 AST 与约束图双向转换的一部分。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include "lv/lv_platform.h"
#include "lv/formula_converter.h"
#include "formula_converter_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/formula_renderer.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"

/* ============================================================
 * 多边形/区域/弧 转换
 * ============================================================ */

/**
 * @brief 转换多边形定义到约束图
 *
 * 多边形由顶点列表定义，转换为多条首尾相连的 LINE_SEGMENT 围成封闭区域，
 * 然后用这些线段创建 GEOM_REGION 节点。
 *
 * @param[in]  polygon_node 多边形节点（类型须为 NODE_GEOM_POLYGON）
 * @param[in]  graph        目标图
 * @param[out] out_node_ids 输出节点ID数组（顶点 + 边 + 区域）
 * @param[out] out_count    输出节点数量
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_polygon(const FormulaNode *polygon_node, ConstraintGraph *graph, int *out_node_ids,
                             int *out_count) {
    if (!polygon_node || polygon_node->type != NODE_GEOM_POLYGON || !graph || !out_node_ids || !out_count) {
        return false;
    }

    *out_count = 0;

    int vert_count = polygon_node->data.geom_polygon.vertex_count;
    if (vert_count < 3) {
        return false; /* 多边形至少需要3个顶点 */
    }

    /* 创建所有顶点 */
    int *vertex_ids = (int *) lv_calloc(vert_count, sizeof(int)); /* 统一内存分配器 */
    if (!vertex_ids)
        return false;

    for (int i = 0; i < vert_count; i++) {
        FormulaNode *v = polygon_node->data.geom_polygon.vertices[i];
        int v_id = -1;

        if (v) {
            if (v->type == NODE_IDENTIFIER) {
                v_id = formula_get_node_id(v->data.identifier.name);
            } else if (v->type == NODE_GEOM_POINT) {
                formula_convert_point(v, graph, &v_id);
            }
        }

        if (v_id < 0) {
            /* 创建默认顶点 */
            SymbolicCoord *coords[2];
            if (symbolic_coord_pair_create_rational(0, 1, 0, 1, &coords[0], &coords[1])) {
                AddNodeResult r = graph_add_point(graph, coords, 2);
                symbolic_coord_pair_destroy(coords[0], coords[1]);
                if (r == ADD_NODE_OK) {
                    v_id = graph_get_node_count(graph) - 1;
                    GeomNode *n = graph_get_node(graph, v_id);
                    if (n)
                        v_id = n->id;
                }
            }
        }

        vertex_ids[i] = v_id;
        if (v_id >= 0 && *out_count < FORMULA_VAR_MAP_SIZE) {
            out_node_ids[(*out_count)++] = v_id;
        }
    }

    /* 创建边（首尾相连） */
    int *segment_ids = (int *) lv_calloc(vert_count, sizeof(int)); /* 统一内存分配器 */
    if (!segment_ids) {
        lv_free((void **) &vertex_ids); /* 统一内存释放器 */
        return false;
    }
    int seg_count = 0;

    for (int i = 0; i < vert_count; i++) {
        int next_i = (i + 1) % vert_count;
        AddNodeResult r = graph_add_line_segment(graph, vertex_ids[i], vertex_ids[next_i]);
        if (r == ADD_NODE_OK) {
            int seg_id = graph_get_node_count(graph) - 1;
            GeomNode *n = graph_get_node(graph, seg_id);
            if (n)
                seg_id = n->id;
            segment_ids[seg_count++] = seg_id;
            if (*out_count < FORMULA_VAR_MAP_SIZE) {
                out_node_ids[(*out_count)++] = seg_id;
            }
        }
    }

    /* 第三步：用边界线段创建 GEOM_REGION */
    if (seg_count >= 3) {
        AddNodeResult r = graph_add_region(graph, segment_ids, seg_count);
        if (r == ADD_NODE_OK) {
            int region_id = graph_get_node_count(graph) - 1;
            GeomNode *n = graph_get_node(graph, region_id);
            if (n)
                region_id = n->id;
            if (*out_count < FORMULA_VAR_MAP_SIZE) {
                out_node_ids[(*out_count)++] = region_id;
            }
            /* 记录变量名映射 */
            if (polygon_node->data.geom_polygon.name) {
                formula_set_node_id(polygon_node->data.geom_polygon.name, region_id);
            }
        }
    }

    lv_free((void **) &vertex_ids);  /* 统一内存释放器 */
    lv_free((void **) &segment_ids); /* 统一内存释放器 */
    return true;
}

/**
 * @brief 转换区域定义到约束图
 *
 * 区域由边界线段列表定义，直接使用 graph_add_region 创建 GEOM_REGION 节点。
 * 子节点应为线段标识符或线段定义。
 *
 * @param[in]  region_node 区域节点（类型须为 NODE_GEOM_REGION）
 * @param[in]  graph       目标图
 * @param[out] out_node_id 输出节点ID
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_region(const FormulaNode *region_node, ConstraintGraph *graph, int *out_node_id) {
    if (!region_node || region_node->type != NODE_GEOM_REGION || !graph || !out_node_id) {
        return false;
    }

    *out_node_id = -1;

    int seg_count = region_node->data.geom_region.segment_count;
    if (seg_count < 3) {
        return false; /* 区域至少需要3条边界线段 */
    }

    /* 从子节点中提取边界线段 ID */
    int *segment_ids = (int *) lv_calloc(seg_count, sizeof(int)); /* 统一内存分配器 */
    if (!segment_ids)
        return false;

    int valid_count = 0;
    for (int i = 0; i < seg_count; i++) {
        FormulaNode *seg = region_node->data.geom_region.boundary_segments[i];
        int seg_id = -1;

        if (seg) {
            if (seg->type == NODE_IDENTIFIER) {
                seg_id = formula_get_node_id(seg->data.identifier.name);
            } else if (seg->type == NODE_GEOM_SEGMENT) {
                formula_convert_segment(seg, graph, &seg_id);
            }
        }

        if (seg_id >= 0) {
            segment_ids[valid_count++] = seg_id;
        }
    }

    if (valid_count < 3) {
        lv_free((void **) &segment_ids); /* 统一内存释放器 */
        return false;
    }

    /* 创建区域节点 */
    AddNodeResult r = graph_add_region(graph, segment_ids, valid_count);

    if (r == ADD_NODE_OK) {
        *out_node_id = graph_get_node_count(graph) - 1;
        GeomNode *n = graph_get_node(graph, *out_node_id);
        if (n) {
            *out_node_id = n->id;
        }
        /* 记录变量名映射 */
        if (region_node->data.geom_region.name) {
            formula_set_node_id(region_node->data.geom_region.name, *out_node_id);
        }
    }

    lv_free((void **) &segment_ids); /* 统一内存释放器 */
    return (*out_node_id >= 0);
}

/**
 * @brief 转换弧定义到约束图
 *
 * 弧由圆心、半径、起止角度定义。转换为：
 *   1. 圆心点
 *   2. 圆周上的半径点（用于表示半径）
 *   3. 半径线段
 * 弧的角度约束通过起止角度参数记录在变量映射中。
 *
 * @param[in]  arc_node   弧节点（类型须为 NODE_GEOM_ARC）
 * @param[in]  graph      目标图
 * @param[out] out_node_ids 输出节点ID数组（圆心 + 半径点 + 半径线段）
 * @param[out] out_count    输出节点数量
 * @return 成功返回 true，失败返回 false
 */
bool formula_convert_arc(const FormulaNode *arc_node, ConstraintGraph *graph, int *out_node_ids, int *out_count) {
    if (!arc_node || arc_node->type != NODE_GEOM_ARC || !graph || !out_node_ids || !out_count) {
        return false;
    }

    *out_count = 0;

    /* 获取圆心 */
    int center_id = -1;
    if (arc_node->data.geom_arc.center) {
        if (arc_node->data.geom_arc.center->type == NODE_IDENTIFIER) {
            center_id = formula_get_node_id(arc_node->data.geom_arc.center->data.identifier.name);
        } else if (arc_node->data.geom_arc.center->type == NODE_GEOM_POINT) {
            formula_convert_point(arc_node->data.geom_arc.center, graph, &center_id);
        }
    }

    /* 如果没有圆心，创建默认圆心 */
    if (center_id < 0) {
        SymbolicCoord *coords[2];
        if (symbolic_coord_pair_create_rational(0, 1, 0, 1, &coords[0], &coords[1])) {
            AddNodeResult r = graph_add_point(graph, coords, 2);
            symbolic_coord_pair_destroy(coords[0], coords[1]);
            if (r == ADD_NODE_OK) {
                center_id = graph_get_node_count(graph) - 1;
                GeomNode *n = graph_get_node(graph, center_id);
                if (n)
                    center_id = n->id;
            }
        }
    }

    if (center_id < 0)
        return false;
    out_node_ids[(*out_count)++] = center_id;

    /* 获取半径 */
    double radius = 1.0;
    if (arc_node->data.geom_arc.radius) {
        if (arc_node->data.geom_arc.radius->type == NODE_NUMBER) {
            if (arc_node->data.geom_arc.radius->data.number.is_integer) {
                radius = (double) arc_node->data.geom_arc.radius->data.number.numerator;
            } else {
                radius = (double) arc_node->data.geom_arc.radius->data.number.numerator /
                         (double) arc_node->data.geom_arc.radius->data.number.denominator;
            }
        }
    }

    /* 获取起止角度（弧度） */
    double start_angle = 0.0;
    double end_angle = M_PI; /* 默认半圆 */

    if (arc_node->data.geom_arc.start_angle) {
        if (arc_node->data.geom_arc.start_angle->type == NODE_NUMBER) {
            FormulaNode *sa = arc_node->data.geom_arc.start_angle;
            if (sa->data.number.is_integer) {
                start_angle = (double) sa->data.number.numerator;
            } else {
                start_angle = (double) sa->data.number.numerator / (double) sa->data.number.denominator;
            }
        }
    }

    if (arc_node->data.geom_arc.end_angle) {
        if (arc_node->data.geom_arc.end_angle->type == NODE_NUMBER) {
            FormulaNode *ea = arc_node->data.geom_arc.end_angle;
            if (ea->data.number.is_integer) {
                end_angle = (double) ea->data.number.numerator;
            } else {
                end_angle = (double) ea->data.number.numerator / (double) ea->data.number.denominator;
            }
        }
    }

    /* 创建半径端点（在起始角度方向上） */
    double rx = radius * cos(start_angle);
    double ry = radius * sin(start_angle);
    SymbolicCoord *radius_coords[2];
    radius_coords[0] = symbolic_coord_from_double_scaled(rx, 1000);
    radius_coords[1] = symbolic_coord_from_double_scaled(ry, 1000);

    AddNodeResult r = graph_add_point(graph, radius_coords, 2);
    symbolic_coord_pair_destroy(radius_coords[0], radius_coords[1]);

    if (r == ADD_NODE_OK) {
        int rp_id = graph_get_node_count(graph) - 1;
        GeomNode *n = graph_get_node(graph, rp_id);
        if (n)
            rp_id = n->id;
        out_node_ids[(*out_count)++] = rp_id;

        /* 创建半径线段 */
        r = graph_add_line_segment(graph, center_id, rp_id);
        if (r == ADD_NODE_OK) {
            int seg_id = graph_get_node_count(graph) - 1;
            n = graph_get_node(graph, seg_id);
            if (n)
                seg_id = n->id;
            out_node_ids[(*out_count)++] = seg_id;
        }
    }

    /* 记录变量名映射 */
    if (arc_node->data.geom_arc.name) {
        formula_set_node_id(arc_node->data.geom_arc.name, center_id);
    }

    return true;
}
