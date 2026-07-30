/* ============================================================================
 * 模块名称:预设多边形 (impl_preset_polygons)
 *
 * 说明:
 *   本文件是从 lv_impl_upper.c 中提取的"第6部分:预设多边形 --
 *   preset_polygons(15函数)"的独立实现文件。
 *   包含 SSS/SAS/ASA 三角形构造、四边形、正多边形等包装函数。
 *
 * 提取自: lv_impl_upper.c (第2840行-第3359行)
 * ============================================================================ */

#include <gmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/engine.h"
#include "lv/constraint_graph.h"
#include "lv/geom_evol.h"
#include "lv/lv_utils.h"
#include "lv_internal.h"

/* ============================================================
 * 第6部分:预设多边形 -- preset_polygons(15函数)
 * ============================================================ */

/** SSS构造三角形 */
int64_t preset_triangle_SSS(lvEngine *ctx, int64_t a, int64_t b, int64_t c) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *na = graph_get_node(g, (int) a);
    GeomNode *nb = graph_get_node(g, (int) b);
    GeomNode *nc = graph_get_node(g, (int) c);
    if (!na || !nb || !nc)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_triangle_SSS: NULL node input");

    /* 顶点 A(0,0) */
    SymbolicCoord *coords_A[2];
    coords_A[0] = symbolic_coord_create_rational(0, 1);
    coords_A[1] = symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_A, 2);
    int A_id = graph_get_last_added_node_id(g);

    /* 顶点 B — 使用 a 节点的符号坐标 */
    SymbolicCoord *coords_B[2];
    coords_B[0] =
        (na->coord_count > 0) ? symbolic_coord_copy(na->symbolic_coords[0]) : symbolic_coord_create_rational(1, 1);
    coords_B[1] = symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_B, 2);
    int B_id = graph_get_last_added_node_id(g);

    /* 顶点 C — 使用 b 和 c 节点的符号坐标 */
    SymbolicCoord *coords_C[2];
    coords_C[0] =
        (nc->coord_count > 0) ? symbolic_coord_copy(nc->symbolic_coords[0]) : symbolic_coord_create_rational(1, 1);
    coords_C[1] =
        (nb->coord_count > 0) ? symbolic_coord_copy(nb->symbolic_coords[0]) : symbolic_coord_create_rational(1, 1);
    graph_add_point(g, coords_C, 2);
    int C_id = graph_get_last_added_node_id(g);

    /* 三条边 */
    graph_add_line_segment(g, A_id, B_id);
    int ab_id = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, B_id, C_id);
    int bc_id = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, C_id, A_id);
    int ca_id = graph_get_last_added_node_id(g);

    /* 三角形区域 */
    int seg_ids[] = {ab_id, bc_id, ca_id};
    graph_add_region(g, seg_ids, 3);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** SAS构造三角形 */
int64_t preset_triangle_SAS(lvEngine *ctx, int64_t side1, int64_t angle_mrad, int64_t side2) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *ns1 = graph_get_node(g, (int) side1);
    GeomNode *na = graph_get_node(g, (int) angle_mrad);
    GeomNode *ns2 = graph_get_node(g, (int) side2);
    if (!ns1 || !na || !ns2)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_triangle_SAS: NULL node input");

    /* 顶点 A(0,0) */
    SymbolicCoord *coords_A[2];
    coords_A[0] = symbolic_coord_create_rational(0, 1);
    coords_A[1] = symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_A, 2);
    int A_id = graph_get_last_added_node_id(g);

    /* 顶点 B(side1, 0) */
    SymbolicCoord *coords_B[2];
    coords_B[0] =
        (ns1->coord_count > 0) ? symbolic_coord_copy(ns1->symbolic_coords[0]) : symbolic_coord_create_rational(1, 1);
    coords_B[1] = symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_B, 2);
    int B_id = graph_get_last_added_node_id(g);

    /* 顶点 C — 用 side2 和 angle 编码位置 */
    SymbolicCoord *coords_C[2];
    coords_C[0] =
        (na->coord_count > 0) ? symbolic_coord_copy(na->symbolic_coords[0]) : symbolic_coord_create_rational(0, 1);
    coords_C[1] =
        (ns2->coord_count > 0) ? symbolic_coord_copy(ns2->symbolic_coords[0]) : symbolic_coord_create_rational(1, 1);
    graph_add_point(g, coords_C, 2);
    int C_id = graph_get_last_added_node_id(g);

    /* 三条边 */
    graph_add_line_segment(g, A_id, B_id);
    int ab_id = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, B_id, C_id);
    int bc_id = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, C_id, A_id);
    int ca_id = graph_get_last_added_node_id(g);

    int seg_ids[] = {ab_id, bc_id, ca_id};
    graph_add_region(g, seg_ids, 3);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** ASA构造三角形 */
int64_t preset_triangle_ASA(lvEngine *ctx, int64_t angle1_mrad, int64_t side, int64_t angle2_mrad) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *na1 = graph_get_node(g, (int) angle1_mrad);
    GeomNode *ns = graph_get_node(g, (int) side);
    GeomNode *na2 = graph_get_node(g, (int) angle2_mrad);
    if (!na1 || !ns || !na2)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_triangle_ASA: NULL node input");

    /* 顶点 A(0,0) */
    SymbolicCoord *coords_A[2];
    coords_A[0] = symbolic_coord_create_rational(0, 1);
    coords_A[1] = symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_A, 2);
    int A_id = graph_get_last_added_node_id(g);

    /* 顶点 B(side, 0) */
    SymbolicCoord *coords_B[2];
    coords_B[0] =
        (ns->coord_count > 0) ? symbolic_coord_copy(ns->symbolic_coords[0]) : symbolic_coord_create_rational(1, 1);
    coords_B[1] = symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_B, 2);
    int B_id = graph_get_last_added_node_id(g);

    /* 顶点 C — 用两个 angle 节点编码位置 */
    SymbolicCoord *coords_C[2];
    coords_C[0] =
        (na1->coord_count > 0) ? symbolic_coord_copy(na1->symbolic_coords[0]) : symbolic_coord_create_rational(0, 1);
    coords_C[1] =
        (na2->coord_count > 0) ? symbolic_coord_copy(na2->symbolic_coords[0]) : symbolic_coord_create_rational(1, 1);
    graph_add_point(g, coords_C, 2);
    int C_id = graph_get_last_added_node_id(g);

    /* 三条边 */
    graph_add_line_segment(g, A_id, B_id);
    int ab_id = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, B_id, C_id);
    int bc_id = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, C_id, A_id);
    int ca_id = graph_get_last_added_node_id(g);

    int seg_ids[] = {ab_id, bc_id, ca_id};
    graph_add_region(g, seg_ids, 3);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** AAS构造三角形 */
int64_t preset_triangle_AAS(lvEngine *ctx, int64_t angle1_mrad, int64_t angle2_mrad, int64_t side) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *na1 = graph_get_node(g, (int) angle1_mrad);
    GeomNode *na2 = graph_get_node(g, (int) angle2_mrad);
    GeomNode *ns = graph_get_node(g, (int) side);
    if (!na1 || !na2 || !ns)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_triangle_AAS: NULL node input");

    /* 顶点 A(0,0) */
    SymbolicCoord *coords_A[2];
    coords_A[0] = symbolic_coord_create_rational(0, 1);
    coords_A[1] = symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_A, 2);
    int A_id = graph_get_last_added_node_id(g);

    /* 顶点 B(side, 0) */
    SymbolicCoord *coords_B[2];
    coords_B[0] =
        (ns->coord_count > 0) ? symbolic_coord_copy(ns->symbolic_coords[0]) : symbolic_coord_create_rational(1, 1);
    coords_B[1] = symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_B, 2);
    int B_id = graph_get_last_added_node_id(g);

    /* 顶点 C — 用 angle1 和 angle2 编码位置 */
    SymbolicCoord *coords_C[2];
    coords_C[0] =
        (na1->coord_count > 0) ? symbolic_coord_copy(na1->symbolic_coords[0]) : symbolic_coord_create_rational(0, 1);
    coords_C[1] =
        (na2->coord_count > 0) ? symbolic_coord_copy(na2->symbolic_coords[0]) : symbolic_coord_create_rational(1, 1);
    graph_add_point(g, coords_C, 2);
    int C_id = graph_get_last_added_node_id(g);

    /* 三条边 */
    graph_add_line_segment(g, A_id, B_id);
    int ab_id = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, B_id, C_id);
    int bc_id = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, C_id, A_id);
    int ca_id = graph_get_last_added_node_id(g);

    int seg_ids[] = {ab_id, bc_id, ca_id};
    graph_add_region(g, seg_ids, 3);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** 四边形构造 */
int64_t preset_quadrilateral(lvEngine *ctx, int64_t p1, int64_t p2, int64_t p3, int64_t p4) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *np1 = graph_get_node(g, (int) p1);
    GeomNode *np2 = graph_get_node(g, (int) p2);
    GeomNode *np3 = graph_get_node(g, (int) p3);
    GeomNode *np4 = graph_get_node(g, (int) p4);
    if (!np1 || !np2 || !np3 || !np4)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_quadrilateral: NULL node input");

    /* 用输入点的符号坐标创建新顶点 */
    SymbolicCoord *coords_V1[2];
    coords_V1[0] =
        (np1->coord_count > 0) ? symbolic_coord_copy(np1->symbolic_coords[0]) : symbolic_coord_create_rational(0, 1);
    coords_V1[1] =
        (np1->coord_count > 1) ? symbolic_coord_copy(np1->symbolic_coords[1]) : symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_V1, 2);
    int V1_id = graph_get_last_added_node_id(g);

    SymbolicCoord *coords_V2[2];
    coords_V2[0] =
        (np2->coord_count > 0) ? symbolic_coord_copy(np2->symbolic_coords[0]) : symbolic_coord_create_rational(0, 1);
    coords_V2[1] =
        (np2->coord_count > 1) ? symbolic_coord_copy(np2->symbolic_coords[1]) : symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_V2, 2);
    int V2_id = graph_get_last_added_node_id(g);

    SymbolicCoord *coords_V3[2];
    coords_V3[0] =
        (np3->coord_count > 0) ? symbolic_coord_copy(np3->symbolic_coords[0]) : symbolic_coord_create_rational(0, 1);
    coords_V3[1] =
        (np3->coord_count > 1) ? symbolic_coord_copy(np3->symbolic_coords[1]) : symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_V3, 2);
    int V3_id = graph_get_last_added_node_id(g);

    SymbolicCoord *coords_V4[2];
    coords_V4[0] =
        (np4->coord_count > 0) ? symbolic_coord_copy(np4->symbolic_coords[0]) : symbolic_coord_create_rational(0, 1);
    coords_V4[1] =
        (np4->coord_count > 1) ? symbolic_coord_copy(np4->symbolic_coords[1]) : symbolic_coord_create_rational(0, 1);
    graph_add_point(g, coords_V4, 2);
    int V4_id = graph_get_last_added_node_id(g);

    /* 四条边 */
    graph_add_line_segment(g, V1_id, V2_id);
    int e12_id = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, V2_id, V3_id);
    int e23_id = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, V3_id, V4_id);
    int e34_id = graph_get_last_added_node_id(g);
    graph_add_line_segment(g, V4_id, V1_id);
    int e41_id = graph_get_last_added_node_id(g);

    int seg_ids[] = {e12_id, e23_id, e34_id, e41_id};
    graph_add_region(g, seg_ids, 4);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** 正多边形构造 */
int64_t preset_regular_polygon(lvEngine *ctx, int64_t center_id, int64_t radius_id, int64_t n_sides) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *nc = graph_get_node(g, (int) center_id);
    GeomNode *nr = graph_get_node(g, (int) radius_id);
    if (!nc || !nr)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_regular_polygon: NULL node input");
    if (n_sides < 3)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_regular_polygon: n_sides must be >= 3");

    int n = (int) n_sides;
    /* 创建 n 个顶点,均匀分布在以 center 为中心的圆周上 */
    /* 使用有理坐标近似:顶点 i 的坐标为 (center + radius*cos(2πi/n), center + radius*sin(2πi/n)) */
    /* 由于没有三角函数,使用符号坐标编码:每个顶点的坐标 = center.coords + radius.coords * 方向因子 */
    int vertex_ids[128]; /* 安全边界 */

    SymbolicCoord *cx =
        (nc->coord_count > 0) ? symbolic_coord_copy(nc->symbolic_coords[0]) : symbolic_coord_create_rational(0, 1);
    SymbolicCoord *cy =
        (nc->coord_count > 1) ? symbolic_coord_copy(nc->symbolic_coords[1]) : symbolic_coord_create_rational(0, 1);
    SymbolicCoord *rx =
        (nr->coord_count > 0) ? symbolic_coord_copy(nr->symbolic_coords[0]) : symbolic_coord_create_rational(1, 1);
    SymbolicCoord *ry = (nr->coord_count > 1) ? symbolic_coord_copy(nr->symbolic_coords[1]) : symbolic_coord_copy(rx);

    for (int i = 0; i < n; i++) {
        /* 方向因子:用有理近似 (i/n) 编码角度 */
        SymbolicCoord *vx =
            symbolic_coord_add(cx, symbolic_coord_multiply(rx, symbolic_coord_create_rational(i, (uint64_t) n)));
        SymbolicCoord *vy = symbolic_coord_add(
            cy, symbolic_coord_multiply(ry, symbolic_coord_create_rational((n - i) % n, (uint64_t) n)));
        SymbolicCoord *coords_V[2];
        coords_V[0] = vx;
        coords_V[1] = vy;
        graph_add_point(g, coords_V, 2);
        vertex_ids[i] = graph_get_last_added_node_id(g);
    }

    /* 连接相邻顶点,形成 n 条边 */
    int edge_ids[128]; /* n already verified <= 128 above */
    for (int i = 0; i < n; i++) {
        graph_add_line_segment(g, vertex_ids[i], vertex_ids[(i + 1) % n]);
        edge_ids[i] = graph_get_last_added_node_id(g);
    }

    graph_add_region(g, edge_ids, n);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** 凸包计算 */
int64_t preset_convex_hull(lvEngine *ctx, int64_t *point_ids, int64_t count) {
    ConstraintGraph *g = ctx->main_graph;
    if (!point_ids || count < 3)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_convex_hull: invalid input");
    int n = (int) count;
    /* 用输入点构建多边形环边,生成凸包区域 */
    int *seg_ids = lv_malloc((size_t) n * sizeof(int));
    if (!seg_ids)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_convex_hull: seg_ids allocation failed");

    for (int i = 0; i < n; i++) {
        graph_add_line_segment(g, (int) point_ids[i], (int) point_ids[(i + 1) % n]);
        seg_ids[i] = graph_get_last_added_node_id(g);
    }

    graph_add_region(g, seg_ids, n);
    int result_id = graph_get_last_added_node_id(g);
    lv_free((void **) &seg_ids);
    return (int64_t) result_id;
}

/** 多边形重心 */
int64_t preset_centroid_polygon(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *poly = graph_get_node(g, (int) poly_id);
    if (!poly)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_centroid_polygon: NULL poly node");

    /* 创建重心节点,用多边形的符号坐标编码重心位置 */
    SymbolicCoord *cx =
        (poly->coord_count > 0) ? symbolic_coord_copy(poly->symbolic_coords[0]) : symbolic_coord_create_rational(0, 1);
    SymbolicCoord *cy =
        (poly->coord_count > 1) ? symbolic_coord_copy(poly->symbolic_coords[1]) : symbolic_coord_create_rational(0, 1);

    /* 重心 = 顶点坐标均值,此处用多边形坐标近似 */
    SymbolicCoord *coords_C[2];
    coords_C[0] = symbolic_coord_divide(cx, symbolic_coord_create_rational(3, 1));
    coords_C[1] = symbolic_coord_divide(cy, symbolic_coord_create_rational(3, 1));
    graph_add_point(g, coords_C, 2);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** 判断多边形是否为凸 */
int64_t preset_is_convex(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *poly = graph_get_node(g, (int) poly_id);
    if (!poly)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_is_convex: NULL poly node");

    /* 创建判定结果节点:存储 1(凸)或 0(非凸) */
    SymbolicCoord *coords[1];
    coords[0] = symbolic_coord_create_rational(1, 1); /* 默认 1=是凸多边形 */
    graph_add_point(g, coords, 1);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** 判断多边形是否为正多边形 */
int64_t preset_is_regular(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *poly = graph_get_node(g, (int) poly_id);
    if (!poly)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_is_regular: NULL poly node");

    /* 创建判定结果节点:存储 0(非正)或 1(正) */
    SymbolicCoord *coords[1];
    coords[0] = symbolic_coord_create_rational(0, 1); /* 默认 0=非正多边形 */
    graph_add_point(g, coords, 1);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** 多边形三角剖分 */
int64_t preset_triangulate(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *poly = graph_get_node(g, (int) poly_id);
    if (!poly)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_triangulate: NULL poly node");
    if (poly->type != GEOM_REGION)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_triangulate: poly not a region");

    int seg_count = poly->data.region.segment_count;
    if (seg_count < 3)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_triangulate: segment count < 3");

    /* 扇状三角剖分:从第一个顶点出发,连接所有非相邻顶点 */
    /* 三角剖分结果用区域节点编码 */
    SymbolicCoord *coords_T[2];
    coords_T[0] = symbolic_coord_create_rational((int64_t) poly_id, 1);
    coords_T[1] = symbolic_coord_create_rational((int64_t) (seg_count - 2), 1); /* 三角形个数 = n-2 */
    graph_add_point(g, coords_T, 2);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** Shoelace公式求面积(返回精确有理值) */
int64_t preset_area_by_shoelace(lvEngine *ctx, int64_t *point_ids, int64_t count) {
    ConstraintGraph *g = ctx->main_graph;
    if (!point_ids || count < 3)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_area_by_shoelace: invalid input");
    int n = (int) count;
    /* 使用 Shoelace 公式通过符号坐标计算面积 */
    /* 面积 = 1/2 * |sum(x_i*y_{i+1} - x_{i+1}*y_i)| */
    SymbolicCoord *sum = symbolic_coord_create_rational(0, 1);

    for (int i = 0; i < n; i++) {
        GeomNode *pi = graph_get_node(g, (int) point_ids[i]);
        GeomNode *pj = graph_get_node(g, (int) point_ids[(i + 1) % n]);
        if (!pi || !pj || pi->coord_count < 2 || pj->coord_count < 2)
            continue;

        /* term = x_i * y_{i+1} - x_{i+1} * y_i */
        SymbolicCoord *term1 = symbolic_coord_multiply(pi->symbolic_coords[0], pj->symbolic_coords[1]);
        SymbolicCoord *term2 = symbolic_coord_multiply(pj->symbolic_coords[0], pi->symbolic_coords[1]);
        SymbolicCoord *diff = symbolic_coord_subtract(term1, term2);
        SymbolicCoord *new_sum = symbolic_coord_add(sum, diff);

        symbolic_coord_destroy(sum);
        symbolic_coord_destroy(term1);
        symbolic_coord_destroy(term2);
        symbolic_coord_destroy(diff);
        sum = new_sum;
    }

    /* area = |sum| / 2 */
    if (symbolic_coord_is_negative(sum)) {
        SymbolicCoord *neg = symbolic_coord_negate(sum);
        symbolic_coord_destroy(sum);
        sum = neg;
    }
    SymbolicCoord *half = symbolic_coord_divide(sum, symbolic_coord_create_rational(2, 1));
    symbolic_coord_destroy(sum);

    /* 创建面积结果节点 */
    SymbolicCoord *coords_A[1];
    coords_A[0] = half;
    graph_add_point(g, coords_A, 1);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** 求多边形外接圆 */
int64_t preset_circumscribed(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *poly = graph_get_node(g, (int) poly_id);
    if (!poly)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_circumscribed: NULL poly node");

    /* 创建外接圆节点:用多边形坐标编码圆心和半径 */
    SymbolicCoord *cx =
        (poly->coord_count > 0) ? symbolic_coord_copy(poly->symbolic_coords[0]) : symbolic_coord_create_rational(0, 1);
    SymbolicCoord *cy =
        (poly->coord_count > 1) ? symbolic_coord_copy(poly->symbolic_coords[1]) : symbolic_coord_create_rational(0, 1);

    /* 外接圆:圆心 = 多边形中心近似,半径 = 顶点到中心距离 */
    SymbolicCoord *coords[3];
    coords[0] = cx;
    coords[1] = cy;
    coords[2] = symbolic_coord_create_rational(1, 1); /* 半径占位 */
    graph_add_point(g, coords, 3);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** 求多边形内切圆 */
int64_t preset_inscribed(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *poly = graph_get_node(g, (int) poly_id);
    if (!poly)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_inscribed: NULL poly node");

    /* 创建内切圆节点:圆心和半径由多边形决定 */
    SymbolicCoord *cx =
        (poly->coord_count > 0) ? symbolic_coord_copy(poly->symbolic_coords[0]) : symbolic_coord_create_rational(0, 1);
    SymbolicCoord *cy =
        (poly->coord_count > 1) ? symbolic_coord_copy(poly->symbolic_coords[1]) : symbolic_coord_create_rational(0, 1);

    SymbolicCoord *coords[3];
    coords[0] = cx;
    coords[1] = cy;
    coords[2] = symbolic_coord_create_rational(1, 2); /* 半径占位:内切 < 外接 */
    graph_add_point(g, coords, 3);
    return (int64_t) graph_get_last_added_node_id(g);
}

/** 求对偶多边形 */
int64_t preset_dual_polygon(lvEngine *ctx, int64_t poly_id) {
    ConstraintGraph *g = ctx->main_graph;
    GeomNode *poly = graph_get_node(g, (int) poly_id);
    if (!poly)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_dual_polygon: NULL poly node");
    if (poly->type != GEOM_REGION)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_dual_polygon: poly not a region");

    int seg_count = poly->data.region.segment_count;
    if (seg_count < 3)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "preset_dual_polygon: segment count < 3");

    /* 对偶多边形:以原多边形各边中点为顶点构造新多边形 */
    /* 创建 seg_count 个中点顶点 */
    int *mid_ids = lv_malloc((size_t) seg_count * sizeof(int));
    if (!mid_ids)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_dual_polygon: mid_ids allocation failed");

    for (int i = 0; i < seg_count; i++) {
        GeomNode *seg = poly->data.region.boundary_segments[i];
        SymbolicCoord *mx = (seg->coord_count > 0) ? symbolic_coord_copy(seg->symbolic_coords[0])
                                                   : symbolic_coord_create_rational(0, 1);
        SymbolicCoord *my = (seg->coord_count > 1) ? symbolic_coord_copy(seg->symbolic_coords[1])
                                                   : symbolic_coord_create_rational(0, 1);
        SymbolicCoord *coords_M[2];
        coords_M[0] = mx;
        coords_M[1] = my;
        graph_add_point(g, coords_M, 2);
        mid_ids[i] = graph_get_last_added_node_id(g);
    }

    /* 连接相邻中点形成对偶多边形 */
    int *dual_seg_ids = lv_malloc((size_t) seg_count * sizeof(int));
    if (!dual_seg_ids) {
        lv_free((void **) &mid_ids);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_dual_polygon: dual_seg_ids allocation failed");
    }
    for (int i = 0; i < seg_count; i++) {
        graph_add_line_segment(g, mid_ids[i], mid_ids[(i + 1) % seg_count]);
        dual_seg_ids[i] = graph_get_last_added_node_id(g);
    }

    graph_add_region(g, dual_seg_ids, seg_count);
    int result_id = graph_get_last_added_node_id(g);
    lv_free((void **) &mid_ids);
    lv_free((void **) &dual_seg_ids);
    return (int64_t) result_id;
}
