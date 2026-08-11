/**
 * @file formula_converter_geom.c
 * @brief 公式转换器实现 —— 基本几何对象转换：点/线段/圆/三角形
 *
 * @details 由 formula_converter.c 按功能边界拆分而来，
 *          属于公式 AST 与约束图双向转换的一部分。
 *
 * @author Lv-00 Project
 * @version 3.0.1
 */

#include "lv/lv_platform.h"
#include "formula_converter.h"
#include "formula_converter_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formula_renderer.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"

/* ============================================================
 * 几何对象转换
 * ============================================================ */

/**
 * @brief 将点 AST 节点转换为约束图节点
 *
 * @param point_node 点 AST 节点
 * @param graph      约束图指针
 * @param out_node_id 输出参数，接收创建的节点 ID
 * @return true 成功，false 失败
 */
bool formula_convert_point(const FormulaNode *point_node, ConstraintGraph *graph, int *out_node_id) {
    if (!point_node || point_node->type != NODE_GEOM_POINT || !graph || !out_node_id) {
        return false;
    }

    /* 获取坐标 */
    int coord_count = 0;
    SymbolicCoord **coords = NULL;

    if (point_node->data.geom_point.coords) {
        coords = formula_coords_to_symbolic(point_node->data.geom_point.coords, &coord_count);
    }

    /* 如果没有坐标，创建默认坐标 (0, 0) */
    if (!coords || coord_count < 2) {
        if (coords) {
            for (int i = 0; i < coord_count; i++) {
                symbolic_coord_destroy(coords[i]);
            }
            lv_free((void **) &coords); /* 统一内存释放器 */
        }
        coords = (SymbolicCoord **) lv_calloc(2, sizeof(SymbolicCoord *));
        if (!coords) {
            lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate default coords");
        }
        coords[0] = symbolic_coord_create_rational(0, 1);
        coords[1] = symbolic_coord_create_rational(0, 1);
        if (!coords[0] || !coords[1]) {
            if (coords[0])
                symbolic_coord_destroy(coords[0]);
            if (coords[1])
                symbolic_coord_destroy(coords[1]);
            lv_free((void **) &coords);
            lv_RETURN_ERROR_BOOL(lv_ERROR_ALLOCATION_FAILED, "failed to allocate default coords");
        }
        coord_count = 2;
    }

    /* 添加点到图 */
    AddNodeResult result = graph_add_point(graph, coords, coord_count);

    /* 释放坐标数组（节点已复制） */
    for (int i = 0; i < coord_count; i++) {
        symbolic_coord_destroy(coords[i]);
    }
    lv_free((void **) &coords); /* 统一内存释放器 */

    if (result != ADD_NODE_OK) {
        return false;
    }

    /* 获取新创建的节点 ID */
    *out_node_id = graph_get_node_count(graph) - 1;
    GeomNode *new_node = graph_get_node(graph, *out_node_id);
    if (new_node) {
        *out_node_id = new_node->id;
    }

    /* 记录变量名映射 */
    if (point_node->data.geom_point.name) {
        formula_set_node_id(point_node->data.geom_point.name, *out_node_id);
    }

    return true;
}

/**
 * @brief 将线段 AST 节点转换为约束图节点
 *
 * @param segment_node 线段 AST 节点
 * @param graph        约束图指针
 * @param out_node_id  输出参数，接收创建的节点 ID
 * @return true 成功，false 失败
 */
bool formula_convert_segment(const FormulaNode *segment_node, ConstraintGraph *graph, int *out_node_id) {
    if (!segment_node || segment_node->type != NODE_GEOM_SEGMENT || !graph || !out_node_id) {
        return false;
    }

    /* 获取端点 ID */
    int ep1_id = -1, ep2_id = -1;

    if (segment_node->data.geom_segment.endpoint1) {
        if (segment_node->data.geom_segment.endpoint1->type == NODE_IDENTIFIER) {
            ep1_id = formula_get_node_id(segment_node->data.geom_segment.endpoint1->data.identifier.name);
        }
        /* 如果端点是点定义，先创建点 */
        else if (segment_node->data.geom_segment.endpoint1->type == NODE_GEOM_POINT) {
            if (!formula_convert_point(segment_node->data.geom_segment.endpoint1, graph, &ep1_id)) {
                ep1_id = -1;
            }
        }
    }

    if (segment_node->data.geom_segment.endpoint2) {
        if (segment_node->data.geom_segment.endpoint2->type == NODE_IDENTIFIER) {
            ep2_id = formula_get_node_id(segment_node->data.geom_segment.endpoint2->data.identifier.name);
        } else if (segment_node->data.geom_segment.endpoint2->type == NODE_GEOM_POINT) {
            if (!formula_convert_point(segment_node->data.geom_segment.endpoint2, graph, &ep2_id)) {
                ep2_id = -1;
            }
        }
    }

    if (ep1_id < 0 || ep2_id < 0) {
        return false;
    }

    /* 添加线段到图 */
    AddNodeResult result = graph_add_line_segment(graph, ep1_id, ep2_id);

    if (result != ADD_NODE_OK) {
        return false;
    }

    /* 获取新创建的节点 ID */
    *out_node_id = graph_get_node_count(graph) - 1;
    GeomNode *new_node = graph_get_node(graph, *out_node_id);
    if (new_node) {
        *out_node_id = new_node->id;
    }

    /* 记录变量名映射 */
    if (segment_node->data.geom_segment.name) {
        formula_set_node_id(segment_node->data.geom_segment.name, *out_node_id);
    }

    return true;
}

/**
 * @brief 将圆 AST 节点转换为约束图节点
 *
 * @param circle_node 圆 AST 节点
 * @param graph       约束图指针
 * @param out_node_id 输出参数，接收创建的节点 ID
 * @return true 成功，false 失败
 */
bool formula_convert_circle(const FormulaNode *circle_node, ConstraintGraph *graph, int *out_node_id) {
    if (!circle_node || circle_node->type != NODE_GEOM_CIRCLE || !graph || !out_node_id) {
        return false;
    }

    /* 获取圆心 ID */
    int center_id = -1;

    if (circle_node->data.geom_circle.center) {
        if (circle_node->data.geom_circle.center->type == NODE_IDENTIFIER) {
            center_id = formula_get_node_id(circle_node->data.geom_circle.center->data.identifier.name);
        } else if (circle_node->data.geom_circle.center->type == NODE_GEOM_POINT) {
            if (!formula_convert_point(circle_node->data.geom_circle.center, graph, &center_id)) {
                center_id = -1;
            }
        }
    }

    /* 如果没有圆心，创建一个默认圆心点 */
    if (center_id < 0) {
        SymbolicCoord *coords[2];
        if (!symbolic_coord_pair_create_rational(0, 1, 0, 1, &coords[0], &coords[1])) {
            return false;
        }

        AddNodeResult result = graph_add_point(graph, coords, 2);
        symbolic_coord_destroy(coords[0]);
        symbolic_coord_destroy(coords[1]);

        if (result != ADD_NODE_OK) {
            return false;
        }

        center_id = graph_get_node_count(graph) - 1;
        GeomNode *center_node = graph_get_node(graph, center_id);
        if (center_node) {
            center_id = center_node->id;
        }
    }

    /* 获取半径 */
    double radius = 1.0;
    if (circle_node->data.geom_circle.radius) {
        if (circle_node->data.geom_circle.radius->type == NODE_NUMBER) {
            if (circle_node->data.geom_circle.radius->data.number.is_integer) {
                radius = (double) circle_node->data.geom_circle.radius->data.number.numerator;
            } else {
                radius = (double) circle_node->data.geom_circle.radius->data.number.numerator /
                         (double) circle_node->data.geom_circle.radius->data.number.denominator;
            }
        }
    }

    /* 创建圆周上的一个点（用于表示半径） */
    SymbolicCoord *radius_coords[2];
    radius_coords[0] = symbolic_coord_from_double_scaled(radius, lv_RATIONAL_SCALE_LOW); /* 圆心 x + radius */
    radius_coords[1] = symbolic_coord_create_rational(0, 1);                            /* 圆心 y */

    AddNodeResult result = graph_add_point(graph, radius_coords, 2);
    symbolic_coord_destroy(radius_coords[0]);
    symbolic_coord_destroy(radius_coords[1]);

    if (result != ADD_NODE_OK) {
        *out_node_id = center_id; /* 至少返回圆心 */
        return true;
    }

    int radius_point_id = graph_get_node_count(graph) - 1;
    GeomNode *radius_node = graph_get_node(graph, radius_point_id);
    if (radius_node) {
        radius_point_id = radius_node->id;
    }

    /* 创建半径线段 */
    result = graph_add_line_segment(graph, center_id, radius_point_id);

    *out_node_id = center_id; /* 返回圆心作为圆的标识 */

    /* 记录变量名映射 */
    if (circle_node->data.geom_circle.name) {
        formula_set_node_id(circle_node->data.geom_circle.name, center_id);
    }

    return true;
}

/**
 * @brief 将三角形 AST 节点转换为约束图节点
 *
 * @param triangle_node 三角形 AST 节点
 * @param graph         约束图指针
 * @param[out] out_node_ids 输出参数，接收创建的节点 ID 数组
 * @param[out] out_count    输出参数，接收节点数量
 * @return true 成功，false 失败
 */
bool formula_convert_triangle(const FormulaNode *triangle_node, ConstraintGraph *graph, int *out_node_ids,
                              int *out_count) {
    if (!triangle_node || triangle_node->type != NODE_GEOM_TRIANGLE || !graph || !out_node_ids || !out_count) {
        return false;
    }

    /* 获取三个顶点 ID */
    int v1_id = -1, v2_id = -1, v3_id = -1;

    if (triangle_node->data.geom_triangle.vertex1) {
        if (triangle_node->data.geom_triangle.vertex1->type == NODE_IDENTIFIER) {
            v1_id = formula_get_node_id(triangle_node->data.geom_triangle.vertex1->data.identifier.name);
        } else if (triangle_node->data.geom_triangle.vertex1->type == NODE_GEOM_POINT) {
            formula_convert_point(triangle_node->data.geom_triangle.vertex1, graph, &v1_id);
        }
    }

    if (triangle_node->data.geom_triangle.vertex2) {
        if (triangle_node->data.geom_triangle.vertex2->type == NODE_IDENTIFIER) {
            v2_id = formula_get_node_id(triangle_node->data.geom_triangle.vertex2->data.identifier.name);
        } else if (triangle_node->data.geom_triangle.vertex2->type == NODE_GEOM_POINT) {
            formula_convert_point(triangle_node->data.geom_triangle.vertex2, graph, &v2_id);
        }
    }

    if (triangle_node->data.geom_triangle.vertex3) {
        if (triangle_node->data.geom_triangle.vertex3->type == NODE_IDENTIFIER) {
            v3_id = formula_get_node_id(triangle_node->data.geom_triangle.vertex3->data.identifier.name);
        } else if (triangle_node->data.geom_triangle.vertex3->type == NODE_GEOM_POINT) {
            formula_convert_point(triangle_node->data.geom_triangle.vertex3, graph, &v3_id);
        }
    }

    if (v1_id < 0 || v2_id < 0 || v3_id < 0) {
        return false;
    }

    /* 记录顶点 */
    out_node_ids[0] = v1_id;
    out_node_ids[1] = v2_id;
    out_node_ids[2] = v3_id;
    *out_count = 3;

    /* 创建三条边 */
    AddNodeResult result;

    result = graph_add_line_segment(graph, v1_id, v2_id);
    if (result == ADD_NODE_OK) {
        out_node_ids[(*out_count)++] = graph_get_node_count(graph) - 1;
    }

    result = graph_add_line_segment(graph, v2_id, v3_id);
    if (result == ADD_NODE_OK) {
        out_node_ids[(*out_count)++] = graph_get_node_count(graph) - 1;
    }

    result = graph_add_line_segment(graph, v3_id, v1_id);
    if (result == ADD_NODE_OK) {
        out_node_ids[(*out_count)++] = graph_get_node_count(graph) - 1;
    }

    return true;
}
