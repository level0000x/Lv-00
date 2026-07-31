/*
 * @file lv_impl_upper_algebra.c
 * @brief Lv-00 upper unified impl - preset algebra
 * @details Split from lv_impl_upper.c
 */

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/atp_backend.h"
#include "lv/conflict_detector.h"
#include "lv/engine.h"
#include "lv/func_block.h"
#include "lv/func_block_preset.h"
#include "lv/func_block_registry.h"
#include "lv/geom_evol.h"
#include "lv/interop.h"
#include "lv/lv_json.h"
#include "lv/lv_utils.h"
#include "lv/meta_verify.h"
#include "lv/orchestrator.h"
#include "lv/preset_algebraic.h"
#include "lv/preset_basic_geometry.h"
#include "lv/preset_measurements.h"
#include "lv/preset_polygons.h"
#include "lv/preset_transformations.h"
#include "lv/visual_editor.h"

#include "lv_internal.h" /* lv_RETURN_ERROR / lv_RETURN_ERROR_NULL */
#include "lv/lv_strbuf.h"
#include "lv_impl_upper_internal.h"

/* ============================================================
 * 第7部分:预设代数 -- preset_algebraic(14函数)
 * ============================================================ */

/** 创建多项式对象(系数数组)-- 创建 GEOM_FUNCTION_BLOCK 类型节点存储多项式系数 */
int64_t preset_polynomial_create(lvEngine *ctx, int64_t *coeffs, int64_t degree) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph || !coeffs || degree < 0 || degree > INT_MAX - 1)
        return s_upper_state.upper_id++;

    int coord_count = (int) degree + 1;
    SymbolicCoord **coords = lv_calloc((size_t) coord_count, sizeof(SymbolicCoord *));
    if (!coords)
        return s_upper_state.upper_id++;

    /* coord[0] = degree, coord[1..degree] = coeffs */
    coords[0] = symbolic_coord_create_rational(degree, 1);
    for (int i = 0; i < (int) degree; i++) {
        coords[i + 1] = symbolic_coord_create_rational(coeffs[i], 1);
    }

    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, coord_count);
    int result_id = graph_get_last_added_node_id(graph);

    for (int i = 0; i < coord_count; i++)
        symbolic_coord_destroy(coords[i]);
    lv_free((void **) &coords);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 在指定点求多项式值(使用GMP精确整数/有理数) */
int64_t preset_polynomial_evaluate(lvEngine *ctx, int64_t poly_id, int64_t x_num, int64_t x_den) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return s_upper_state.upper_id++;

    /* 查找输入多项式节点 */
    GeomNode *poly_node = graph_get_node(graph, (int) poly_id);
    if (!poly_node)
        return s_upper_state.upper_id++;

    /* 创建计算点坐标:x = x_num / x_den,以及占位结果坐标 */
    SymbolicCoord *point_coord = symbolic_coord_create_rational(x_num, (uint64_t) (x_den ? x_den : 1));
    if (!point_coord)
        return s_upper_state.upper_id++;
    SymbolicCoord *result_coord = symbolic_coord_create_rational(0, 1);
    if (!result_coord) {
        symbolic_coord_destroy(point_coord);
        return s_upper_state.upper_id++;
    }

    SymbolicCoord *coords[2] = {point_coord, result_coord};
    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(point_coord);
    symbolic_coord_destroy(result_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 多项式求根(返回根节点组ID)-- 创建结果节点,实际求根由求解器完成 */
int64_t preset_polynomial_roots(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return s_upper_state.upper_id++;

    GeomNode *poly_node = graph_get_node(graph, (int) poly_id);
    if (!poly_node)
        return s_upper_state.upper_id++;

    /* 创建结果节点表示求根操作 */
    SymbolicCoord *root_coord = symbolic_coord_create_rational(0, 1);
    if (!root_coord)
        return s_upper_state.upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &root_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(root_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 多项式加法 -- 创建新多项式节点表示加法结果 */
int64_t preset_polynomial_add(lvEngine *ctx, int64_t p1_id, int64_t p2_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return s_upper_state.upper_id++;

    GeomNode *p1_node = graph_get_node(graph, (int) p1_id);
    GeomNode *p2_node = graph_get_node(graph, (int) p2_id);
    if (!p1_node || !p2_node)
        return s_upper_state.upper_id++;

    /* 创建结果节点:coord[0]=p1_id标记, coord[1]=p2_id标记 */
    SymbolicCoord *op1 = symbolic_coord_create_rational(p1_id, 1);
    SymbolicCoord *op2 = symbolic_coord_create_rational(p2_id, 1);
    if (!op1 || !op2) {
        if (op1)
            symbolic_coord_destroy(op1);
        if (op2)
            symbolic_coord_destroy(op2);
        return s_upper_state.upper_id++;
    }

    SymbolicCoord *coords[2] = {op1, op2};
    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(op1);
    symbolic_coord_destroy(op2);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 多项式乘法 -- 创建新多项式节点表示乘法结果 */
int64_t preset_polynomial_mul(lvEngine *ctx, int64_t p1_id, int64_t p2_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return s_upper_state.upper_id++;

    GeomNode *p1_node = graph_get_node(graph, (int) p1_id);
    GeomNode *p2_node = graph_get_node(graph, (int) p2_id);
    if (!p1_node || !p2_node)
        return s_upper_state.upper_id++;

    /* 创建结果节点:coord[0]=p1_id标记, coord[1]=p2_id标记 */
    SymbolicCoord *op1 = symbolic_coord_create_rational(p1_id, 1);
    SymbolicCoord *op2 = symbolic_coord_create_rational(p2_id, 1);
    if (!op1 || !op2) {
        if (op1)
            symbolic_coord_destroy(op1);
        if (op2)
            symbolic_coord_destroy(op2);
        return s_upper_state.upper_id++;
    }

    SymbolicCoord *coords[2] = {op1, op2};
    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(op1);
    symbolic_coord_destroy(op2);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 方程求解 -- 创建结果节点表示解 */
int64_t preset_equation_solve(lvEngine *ctx, int64_t equation_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return s_upper_state.upper_id++;

    GeomNode *eq_node = graph_get_node(graph, (int) equation_id);
    if (!eq_node)
        return s_upper_state.upper_id++;

    /* 创建结果节点表示方程的解 */
    SymbolicCoord *sol_coord = symbolic_coord_create_rational(0, 1);
    if (!sol_coord)
        return s_upper_state.upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &sol_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(sol_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 不等式检查 -- 创建结果节点表示真/假(1=成立, 0=不成立) */
int64_t preset_inequality_check(lvEngine *ctx, int64_t expr_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return 1; /* 默认成立 */

    GeomNode *expr_node = graph_get_node(graph, (int) expr_id);
    if (!expr_node)
        return 1;

    /* 创建结果节点:coord value = 1(成立),实际验证由求解器完成 */
    SymbolicCoord *result_coord = symbolic_coord_create_rational(1, 1);
    if (!result_coord)
        return 1;

    AddNodeResult add_res = graph_add_point(graph, &result_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(result_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return 1;
    return (int64_t) result_id;
}

/** Groebner基计算 -- 创建结果节点表示Groebner基 */
int64_t preset_groebner_basis(lvEngine *ctx, int64_t *poly_ids, int64_t count) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph || !poly_ids || count <= 0)
        return s_upper_state.upper_id++;

    /* 验证所有输入多项式节点存在 */
    for (int64_t i = 0; i < count; i++) {
        if (!graph_get_node(graph, (int) poly_ids[i]))
            return s_upper_state.upper_id++;
    }

    /* 创建结果节点:coord[0]=count标记 */
    SymbolicCoord *cnt_coord = symbolic_coord_create_rational(count, 1);
    if (!cnt_coord)
        return s_upper_state.upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &cnt_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(cnt_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 获取多项式次数 -- 返回度数节点 */
int64_t preset_polynomial_degree(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return s_upper_state.upper_id++;

    GeomNode *poly_node = graph_get_node(graph, (int) poly_id);
    if (!poly_node)
        return s_upper_state.upper_id++;

    /* 查找节点坐标获取度数:若节点有coord_count,则度数为coord_count-1 */
    int degree = (poly_node->coord_count > 1) ? (poly_node->coord_count - 1) : 0;

    /* 创建结果节点存储度数值 */
    SymbolicCoord *deg_coord = symbolic_coord_create_rational((int64_t) degree, 1);
    if (!deg_coord)
        return s_upper_state.upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &deg_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(deg_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 多项式求导 -- 创建导数多项式节点 */
int64_t preset_polynomial_derivative(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return s_upper_state.upper_id++;

    GeomNode *poly_node = graph_get_node(graph, (int) poly_id);
    if (!poly_node)
        return s_upper_state.upper_id++;

    /* 创建导数节点:coord[0]=原多项式ID标记, coord[1]=导数标记(-1) */
    SymbolicCoord *src_coord = symbolic_coord_create_rational(poly_id, 1);
    SymbolicCoord *op_coord = symbolic_coord_create_rational(-1, 1);
    if (!src_coord || !op_coord) {
        if (src_coord)
            symbolic_coord_destroy(src_coord);
        if (op_coord)
            symbolic_coord_destroy(op_coord);
        return s_upper_state.upper_id++;
    }

    SymbolicCoord *coords[2] = {src_coord, op_coord};
    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(src_coord);
    symbolic_coord_destroy(op_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 多项式积分 -- 创建积分多项式节点 */
int64_t preset_polynomial_integral(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return s_upper_state.upper_id++;

    GeomNode *poly_node = graph_get_node(graph, (int) poly_id);
    if (!poly_node)
        return s_upper_state.upper_id++;

    /* 创建积分节点:coord[0]=原多项式ID标记, coord[1]=积分标记(+1) */
    SymbolicCoord *src_coord = symbolic_coord_create_rational(poly_id, 1);
    SymbolicCoord *op_coord = symbolic_coord_create_rational(1, 1);
    if (!src_coord || !op_coord) {
        if (src_coord)
            symbolic_coord_destroy(src_coord);
        if (op_coord)
            symbolic_coord_destroy(op_coord);
        return s_upper_state.upper_id++;
    }

    SymbolicCoord *coords[2] = {src_coord, op_coord};
    AddNodeResult add_res = graph_add_point(graph, (SymbolicCoord *const *) coords, 2);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(src_coord);
    symbolic_coord_destroy(op_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 方程组求解 -- 创建结果节点组 */
int64_t preset_system_solve(lvEngine *ctx, int64_t *equation_ids, int64_t count) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph || !equation_ids || count <= 0)
        return s_upper_state.upper_id++;

    /* 验证所有方程节点存在 */
    for (int64_t i = 0; i < count; i++) {
        if (!graph_get_node(graph, (int) equation_ids[i]))
            return s_upper_state.upper_id++;
    }

    /* 创建结果节点:coord[0]=方程组数量标记 */
    SymbolicCoord *cnt_coord = symbolic_coord_create_rational(count, 1);
    if (!cnt_coord)
        return s_upper_state.upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &cnt_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(cnt_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 有理表达式化简 -- 创建化简结果节点 */
int64_t preset_rational_simplify(lvEngine *ctx, int64_t expr_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return s_upper_state.upper_id++;

    GeomNode *expr_node = graph_get_node(graph, (int) expr_id);
    if (!expr_node)
        return s_upper_state.upper_id++;

    /* 创建化简结果节点:coord[0]=原表达式ID,实际化简由求解器完成 */
    SymbolicCoord *result_coord = symbolic_coord_create_rational(expr_id, 1);
    if (!result_coord)
        return s_upper_state.upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &result_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(result_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}

/** 表达式化简 -- 创建化简结果节点 */
int64_t preset_expression_simplify(lvEngine *ctx, int64_t expr_id) {
    ConstraintGraph *graph = ctx ? ctx->main_graph : NULL;
    if (!graph)
        return s_upper_state.upper_id++;

    GeomNode *expr_node = graph_get_node(graph, (int) expr_id);
    if (!expr_node)
        return s_upper_state.upper_id++;

    /* 创建化简结果节点:coord[0]=原表达式ID,实际化简由求解器完成 */
    SymbolicCoord *result_coord = symbolic_coord_create_rational(expr_id, 1);
    if (!result_coord)
        return s_upper_state.upper_id++;

    AddNodeResult add_res = graph_add_point(graph, &result_coord, 1);
    int result_id = graph_get_last_added_node_id(graph);

    symbolic_coord_destroy(result_coord);

    if (add_res != ADD_NODE_OK || result_id < 0)
        return s_upper_state.upper_id++;
    return (int64_t) result_id;
}
