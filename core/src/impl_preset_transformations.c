/* ============================================================================
 * 模块名称:预设变换 (impl_preset_transformations)
 *
 * 说明:
 *   本文件是从 lv_impl_upper.c 中提取的"第4部分:预设变换 --
 *   preset_transformations(17函数)"的独立实现文件。
 *   包含平移、旋转、缩放、反射、仿射、投影等几何变换包装函数。
 *
 * 提取自: lv_impl_upper.c (第1122行-第2457行)
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
 * 第4部分:预设变换 -- preset_transformations(17函数)
 * ============================================================ */

/** 平移变换 */
int64_t preset_translate(lvEngine *ctx, int64_t obj_id, int64_t dx, int64_t dy) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    if (!obj)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_translate: NULL obj node");

    /* 如果 obj 没有坐标，回退到复制行为 */
    if (obj->coord_count == 0) {
        graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
        int result_id = graph_get_last_added_node_id(graph);
        if (result_id < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_translate: graph_get_last_added_node_id failed (fallback)");
        graph_add_line_segment(graph, (int) obj_id, result_id);
        return (int64_t) result_id;
    }

    /* 创建平移向量 */
    SymbolicCoord *dx_r = symbolic_coord_create_rational(dx, 1);
    SymbolicCoord *dy_r = symbolic_coord_create_rational(dy, 1);
    if (!dx_r || !dy_r) {
        if (dx_r)
            symbolic_coord_destroy(dx_r);
        if (dy_r)
            symbolic_coord_destroy(dy_r);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_translate: create rational failed");
    }

    /* 计算新坐标: (x+dx, y+dy) */
    int n = obj->coord_count;
    SymbolicCoord **new_coords = lv_malloc((size_t) n * sizeof(SymbolicCoord *));
    if (!new_coords) {
        symbolic_coord_destroy(dx_r);
        symbolic_coord_destroy(dy_r);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_translate: new_coords allocation failed");
    }

    for (int i = 0; i < n; i++) {
        SymbolicCoord *delta = (i % 2 == 0) ? dx_r : dy_r;
        new_coords[i] = symbolic_coord_add(obj->symbolic_coords[i], delta);
        if (!new_coords[i]) {
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_translate: symbolic_coord_add failed");
        }
    }

    symbolic_coord_destroy(dx_r);
    symbolic_coord_destroy(dy_r);

    /* 创建新点（graph_add_point 会深拷贝坐标，故可释放临时数组） */
    graph_add_point(graph, new_coords, n);
    for (int i = 0; i < n; i++)
        symbolic_coord_destroy(new_coords[i]);
    lv_free((void **) &new_coords);

    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_translate: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    return (int64_t) result_id;
}

/** 旋转变换(绕原点) */
int64_t preset_rotate(lvEngine *ctx, int64_t obj_id, int64_t angle_mrad) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    if (!obj)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_rotate: NULL obj node");

    /* 如果 obj 没有坐标，回退到复制行为 */
    if (obj->coord_count == 0) {
        graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
        int result_id = graph_get_last_added_node_id(graph);
        if (result_id < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_rotate: graph_get_last_added_node_id failed (fallback)");
        graph_add_line_segment(graph, (int) obj_id, result_id);
        return (int64_t) result_id;
    }

    /* 创建 sinθ 和 cosθ 超越数 */
    SymbolicCoord *sin_theta = symbolic_coord_create_transcendental("sinθ_mrad");
    SymbolicCoord *cos_theta = symbolic_coord_create_transcendental("cosθ_mrad");
    if (!sin_theta || !cos_theta) {
        if (sin_theta)
            symbolic_coord_destroy(sin_theta);
        if (cos_theta)
            symbolic_coord_destroy(cos_theta);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotate: create transcendental failed");
    }

    /* 旋转公式: x' = x*cosθ - y*sinθ, y' = x*sinθ + y*cosθ */
    int n = obj->coord_count;
    SymbolicCoord **new_coords = lv_malloc((size_t) n * sizeof(SymbolicCoord *));
    if (!new_coords) {
        symbolic_coord_destroy(sin_theta);
        symbolic_coord_destroy(cos_theta);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotate: new_coords allocation failed");
    }

    for (int i = 0; i + 1 < n; i += 2) {
        /* x' = x*cosθ - y*sinθ */
        SymbolicCoord *x_cos = symbolic_coord_multiply(obj->symbolic_coords[i], cos_theta);
        SymbolicCoord *y_sin = symbolic_coord_multiply(obj->symbolic_coords[i + 1], sin_theta);
        if (!x_cos || !y_sin) {
            if (x_cos)
                symbolic_coord_destroy(x_cos);
            if (y_sin)
                symbolic_coord_destroy(y_sin);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotate: multiply failed (x_cos/y_sin)");
        }
        new_coords[i] = symbolic_coord_subtract(x_cos, y_sin);
        symbolic_coord_destroy(x_cos);
        symbolic_coord_destroy(y_sin);
        if (!new_coords[i]) {
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotate: subtract failed (new_coords[i])");
        }

        /* y' = x*sinθ + y*cosθ */
        SymbolicCoord *x_sin = symbolic_coord_multiply(obj->symbolic_coords[i], sin_theta);
        SymbolicCoord *y_cos = symbolic_coord_multiply(obj->symbolic_coords[i + 1], cos_theta);
        if (!x_sin || !y_cos) {
            if (x_sin)
                symbolic_coord_destroy(x_sin);
            if (y_cos)
                symbolic_coord_destroy(y_cos);
            symbolic_coord_destroy(new_coords[i]);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotate: multiply failed (x_sin/y_cos)");
        }
        new_coords[i + 1] = symbolic_coord_add(x_sin, y_cos);
        symbolic_coord_destroy(x_sin);
        symbolic_coord_destroy(y_cos);
        if (!new_coords[i + 1]) {
            symbolic_coord_destroy(new_coords[i]);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotate: add failed (new_coords[i+1])");
        }
    }

    /* 处理奇数个坐标的情况（复制最后一个） */
    if (n % 2 != 0) {
        new_coords[n - 1] = symbolic_coord_copy(obj->symbolic_coords[n - 1]);
        if (!new_coords[n - 1]) {
            for (int j = 0; j < n - 1; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotate: symbolic_coord_copy failed");
        }
    }

    symbolic_coord_destroy(sin_theta);
    symbolic_coord_destroy(cos_theta);

    graph_add_point(graph, new_coords, n);
    for (int i = 0; i < n; i++)
        symbolic_coord_destroy(new_coords[i]);
    lv_free((void **) &new_coords);

    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_rotate: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    return (int64_t) result_id;
}

/** 关于点的反射 */
int64_t preset_reflect_point(lvEngine *ctx, int64_t obj_id, int64_t center_id) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    GeomNode *center = graph_get_node(graph, (int) center_id);
    if (!obj || !center)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_reflect_point: NULL node input");
    graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_reflect_point: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    graph_add_line_segment(graph, (int) center_id, result_id);
    return (int64_t) result_id;
}

/** 关于直线的反射 */
int64_t preset_reflect_line(lvEngine *ctx, int64_t obj_id, int64_t line_id) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    GeomNode *line = graph_get_node(graph, (int) line_id);
    if (!obj || !line)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_reflect_line: NULL node input");
    graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_reflect_line: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    graph_add_incidence(graph, result_id, (int) line_id);
    return (int64_t) result_id;
}

/** 缩放变换 */
int64_t preset_scale(lvEngine *ctx, int64_t obj_id, int64_t sx, int64_t sy, int64_t denom) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    if (!obj)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_scale: NULL obj node");

    /* 如果 obj 没有坐标，回退到复制行为 */
    if (obj->coord_count == 0) {
        graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
        int result_id = graph_get_last_added_node_id(graph);
        if (result_id < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_scale: graph_get_last_added_node_id failed (fallback)");
        graph_add_line_segment(graph, (int) obj_id, result_id);
        return (int64_t) result_id;
    }

    /* 创建缩放因子 */
    SymbolicCoord *sx_r = symbolic_coord_create_rational(sx, (uint64_t) denom);
    SymbolicCoord *sy_r = symbolic_coord_create_rational(sy, (uint64_t) denom);
    if (!sx_r || !sy_r) {
        if (sx_r)
            symbolic_coord_destroy(sx_r);
        if (sy_r)
            symbolic_coord_destroy(sy_r);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_scale: create rational failed");
    }

    /* 计算新坐标: (x*sx/denom, y*sy/denom) */
    int n = obj->coord_count;
    SymbolicCoord **new_coords = lv_malloc((size_t) n * sizeof(SymbolicCoord *));
    if (!new_coords) {
        symbolic_coord_destroy(sx_r);
        symbolic_coord_destroy(sy_r);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_scale: new_coords allocation failed");
    }

    for (int i = 0; i < n; i++) {
        SymbolicCoord *scale = (i % 2 == 0) ? sx_r : sy_r;
        new_coords[i] = symbolic_coord_multiply(obj->symbolic_coords[i], scale);
        if (!new_coords[i]) {
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sx_r);
            symbolic_coord_destroy(sy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_scale: symbolic_coord_multiply failed");
        }
    }

    symbolic_coord_destroy(sx_r);
    symbolic_coord_destroy(sy_r);

    graph_add_point(graph, new_coords, n);
    for (int i = 0; i < n; i++)
        symbolic_coord_destroy(new_coords[i]);
    lv_free((void **) &new_coords);

    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_scale: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    return (int64_t) result_id;
}

/** X方向剪切变换 */
int64_t preset_shear_x(lvEngine *ctx, int64_t obj_id, int64_t factor, int64_t denom) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    if (!obj)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_shear_x: NULL obj node");

    /* 如果 obj 没有坐标，回退到复制行为 */
    if (obj->coord_count == 0) {
        graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
        int result_id = graph_get_last_added_node_id(graph);
        if (result_id < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_shear_x: graph_get_last_added_node_id failed (fallback)");
        graph_add_line_segment(graph, (int) obj_id, result_id);
        return (int64_t) result_id;
    }

    /* 创建剪切因子: factor/denom */
    SymbolicCoord *ratio = symbolic_coord_create_rational(factor, (uint64_t) denom);
    if (!ratio)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_x: create rational failed");

    /* x' = x + y*factor/denom, y' = y */
    int n = obj->coord_count;
    SymbolicCoord **new_coords = lv_malloc((size_t) n * sizeof(SymbolicCoord *));
    if (!new_coords) {
        symbolic_coord_destroy(ratio);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_x: new_coords allocation failed");
    }

    for (int i = 0; i + 1 < n; i += 2) {
        /* x' = x + y * ratio */
        SymbolicCoord *y_ratio = symbolic_coord_multiply(obj->symbolic_coords[i + 1], ratio);
        if (!y_ratio) {
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_x: multiply failed (y_ratio)");
        }
        new_coords[i] = symbolic_coord_add(obj->symbolic_coords[i], y_ratio);
        symbolic_coord_destroy(y_ratio);
        if (!new_coords[i]) {
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_x: add failed (new_coords[i])");
        }

        /* y' = y (不变) */
        new_coords[i + 1] = symbolic_coord_copy(obj->symbolic_coords[i + 1]);
        if (!new_coords[i + 1]) {
            symbolic_coord_destroy(new_coords[i]);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_x: copy failed (new_coords[i+1])");
        }
    }

    /* 处理奇数个坐标的情况 */
    if (n % 2 != 0) {
        new_coords[n - 1] = symbolic_coord_copy(obj->symbolic_coords[n - 1]);
        if (!new_coords[n - 1]) {
            for (int j = 0; j < n - 1; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_x: copy failed (odd coord)");
        }
    }

    symbolic_coord_destroy(ratio);

    graph_add_point(graph, new_coords, n);
    for (int i = 0; i < n; i++)
        symbolic_coord_destroy(new_coords[i]);
    lv_free((void **) &new_coords);

    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_shear_x: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    return (int64_t) result_id;
}

/** Y方向剪切变换 */
int64_t preset_shear_y(lvEngine *ctx, int64_t obj_id, int64_t factor, int64_t denom) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    if (!obj)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_shear_y: NULL obj node");

    /* 如果 obj 没有坐标，回退到复制行为 */
    if (obj->coord_count == 0) {
        graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
        int result_id = graph_get_last_added_node_id(graph);
        if (result_id < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_shear_y: graph_get_last_added_node_id failed (fallback)");
        graph_add_line_segment(graph, (int) obj_id, result_id);
        return (int64_t) result_id;
    }

    /* 创建剪切因子: factor/denom */
    SymbolicCoord *ratio = symbolic_coord_create_rational(factor, (uint64_t) denom);
    if (!ratio)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_y: create rational failed");

    /* x' = x, y' = y + x*factor/denom */
    int n = obj->coord_count;
    SymbolicCoord **new_coords = lv_malloc((size_t) n * sizeof(SymbolicCoord *));
    if (!new_coords) {
        symbolic_coord_destroy(ratio);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_y: new_coords allocation failed");
    }

    for (int i = 0; i + 1 < n; i += 2) {
        /* x' = x (不变) */
        new_coords[i] = symbolic_coord_copy(obj->symbolic_coords[i]);
        if (!new_coords[i]) {
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_y: copy failed (new_coords[i])");
        }

        /* y' = y + x * ratio */
        SymbolicCoord *x_ratio = symbolic_coord_multiply(obj->symbolic_coords[i], ratio);
        if (!x_ratio) {
            symbolic_coord_destroy(new_coords[i]);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_y: multiply failed (x_ratio)");
        }
        new_coords[i + 1] = symbolic_coord_add(obj->symbolic_coords[i + 1], x_ratio);
        symbolic_coord_destroy(x_ratio);
        if (!new_coords[i + 1]) {
            symbolic_coord_destroy(new_coords[i]);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_y: add failed (new_coords[i+1])");
        }
    }

    /* 处理奇数个坐标的情况 */
    if (n % 2 != 0) {
        new_coords[n - 1] = symbolic_coord_copy(obj->symbolic_coords[n - 1]);
        if (!new_coords[n - 1]) {
            for (int j = 0; j < n - 1; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_shear_y: copy failed (odd coord)");
        }
    }

    symbolic_coord_destroy(ratio);

    graph_add_point(graph, new_coords, n);
    for (int i = 0; i < n; i++)
        symbolic_coord_destroy(new_coords[i]);
    lv_free((void **) &new_coords);

    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_shear_y: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    return (int64_t) result_id;
}

/** 仿射变换(6参数矩阵) */
int64_t preset_affine(lvEngine *ctx, int64_t obj_id, int64_t a11, int64_t a12, int64_t a21, int64_t a22, int64_t tx,
                      int64_t ty, int64_t denom) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    if (!obj)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_affine: NULL obj node");

    /* 如果 obj 没有坐标，回退到复制行为 */
    if (obj->coord_count == 0) {
        graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
        int result_id = graph_get_last_added_node_id(graph);
        if (result_id < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_affine: graph_get_last_added_node_id failed (fallback)");
        graph_add_line_segment(graph, (int) obj_id, result_id);
        return (int64_t) result_id;
    }

    /* 创建矩阵元素和偏移量 */
    SymbolicCoord *s_a11 = symbolic_coord_create_rational(a11, (uint64_t) denom);
    SymbolicCoord *s_a12 = symbolic_coord_create_rational(a12, (uint64_t) denom);
    SymbolicCoord *s_a21 = symbolic_coord_create_rational(a21, (uint64_t) denom);
    SymbolicCoord *s_a22 = symbolic_coord_create_rational(a22, (uint64_t) denom);
    SymbolicCoord *s_tx = symbolic_coord_create_rational(tx, (uint64_t) denom);
    SymbolicCoord *s_ty = symbolic_coord_create_rational(ty, (uint64_t) denom);

    if (!s_a11 || !s_a12 || !s_a21 || !s_a22 || !s_tx || !s_ty) {
        if (s_a11)
            symbolic_coord_destroy(s_a11);
        if (s_a12)
            symbolic_coord_destroy(s_a12);
        if (s_a21)
            symbolic_coord_destroy(s_a21);
        if (s_a22)
            symbolic_coord_destroy(s_a22);
        if (s_tx)
            symbolic_coord_destroy(s_tx);
        if (s_ty)
            symbolic_coord_destroy(s_ty);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_affine: create rational failed");
    }

    /* x' = (a11*x + a12*y + tx) / denom
     * y' = (a21*x + a22*y + ty) / denom */
    int n = obj->coord_count;
    SymbolicCoord **new_coords = lv_malloc((size_t) n * sizeof(SymbolicCoord *));
    if (!new_coords) {
        symbolic_coord_destroy(s_a11);
        symbolic_coord_destroy(s_a12);
        symbolic_coord_destroy(s_a21);
        symbolic_coord_destroy(s_a22);
        symbolic_coord_destroy(s_tx);
        symbolic_coord_destroy(s_ty);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_affine: new_coords allocation failed");
    }

    for (int i = 0; i + 1 < n; i += 2) {
        /* x' = a11*x + a12*y + tx */
        SymbolicCoord *a11x = symbolic_coord_multiply(obj->symbolic_coords[i], s_a11);
        SymbolicCoord *a12y = symbolic_coord_multiply(obj->symbolic_coords[i + 1], s_a12);
        if (!a11x || !a12y) {
            if (a11x)
                symbolic_coord_destroy(a11x);
            if (a12y)
                symbolic_coord_destroy(a12y);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(s_a11);
            symbolic_coord_destroy(s_a12);
            symbolic_coord_destroy(s_a21);
            symbolic_coord_destroy(s_a22);
            symbolic_coord_destroy(s_tx);
            symbolic_coord_destroy(s_ty);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_affine: multiply failed (a11x/a12y)");
        }
    }

    /* 处理奇数个坐标的情况 */
    if (n % 2 != 0) {
        new_coords[n - 1] = symbolic_coord_copy(obj->symbolic_coords[n - 1]);
        if (!new_coords[n - 1]) {
            for (int j = 0; j < n - 1; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(s_a11);
            symbolic_coord_destroy(s_a12);
            symbolic_coord_destroy(s_a21);
            symbolic_coord_destroy(s_a22);
            symbolic_coord_destroy(s_tx);
            symbolic_coord_destroy(s_ty);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_affine: copy failed (odd coord)");
        }
    }

    symbolic_coord_destroy(s_a11);
    symbolic_coord_destroy(s_a12);
    symbolic_coord_destroy(s_a21);
    symbolic_coord_destroy(s_a22);
    symbolic_coord_destroy(s_tx);
    symbolic_coord_destroy(s_ty);

    graph_add_point(graph, new_coords, n);
    for (int i = 0; i < n; i++)
        symbolic_coord_destroy(new_coords[i]);
    lv_free((void **) &new_coords);

    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_affine: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    return (int64_t) result_id;
}

/** 逆变换 */
int64_t preset_inverse_transform(lvEngine *ctx, int64_t transform_id) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *t = graph_get_node(graph, (int) transform_id);
    if (!t)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_inverse_transform: NULL transform node");
    graph_add_point(graph, t->symbolic_coords, t->coord_count);
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_inverse_transform: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) transform_id, result_id);
    return (int64_t) result_id;
}

/** 组合两个变换 */
int64_t preset_compose_transforms(lvEngine *ctx, int64_t t1_id, int64_t t2_id) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *t1 = graph_get_node(graph, (int) t1_id);
    GeomNode *t2 = graph_get_node(graph, (int) t2_id);
    if (!t1 || !t2)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_compose_transforms: NULL transform node");
    graph_add_point(graph, t1->symbolic_coords, t1->coord_count);
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_compose_transforms: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) t1_id, result_id);
    graph_add_line_segment(graph, (int) t2_id, result_id);
    return (int64_t) result_id;
}

/** 恒等变换 */
int64_t preset_identity_transform(lvEngine *ctx) {
    ConstraintGraph *graph = ctx->main_graph;
    graph_add_point(graph, NULL, 0);
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_identity_transform: graph_get_last_added_node_id failed");
    return (int64_t) result_id;
}

/** 位似变换(dilatation) */
int64_t preset_dilate(lvEngine *ctx, int64_t obj_id, int64_t center_id, int64_t ratio_num, int64_t ratio_den) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    GeomNode *center = graph_get_node(graph, (int) center_id);
    if (!obj || !center)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_dilate: NULL node input");

    /* 如果 obj 或 center 没有坐标，回退到复制行为 */
    if (obj->coord_count == 0 || center->coord_count < 2) {
        graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
        int result_id = graph_get_last_added_node_id(graph);
        if (result_id < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_dilate: graph_get_last_added_node_id failed (fallback)");
        graph_add_line_segment(graph, (int) obj_id, result_id);
        graph_add_line_segment(graph, (int) center_id, result_id);
        return (int64_t) result_id;
    }

    /* 创建缩放比率: ratio = ratio_num/ratio_den */
    SymbolicCoord *ratio = symbolic_coord_create_rational(ratio_num, (uint64_t) ratio_den);
    if (!ratio)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_dilate: create rational failed");

    /* P' = C + (P-C) * ratio
     * x' = cx + (x - cx) * ratio
     * y' = cy + (y - cy) * ratio */
    SymbolicCoord *cx = center->symbolic_coords[0];
    SymbolicCoord *cy = center->symbolic_coords[1];
    int n = obj->coord_count;
    SymbolicCoord **new_coords = lv_malloc((size_t) n * sizeof(SymbolicCoord *));
    if (!new_coords) {
        symbolic_coord_destroy(ratio);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_dilate: new_coords allocation failed");
    }

    for (int i = 0; i + 1 < n; i += 2) {
        /* offset_x = x - cx */
        SymbolicCoord *off_x = symbolic_coord_subtract(obj->symbolic_coords[i], cx);
        if (!off_x) {
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_dilate: subtract failed (off_x)");
        }
        /* offset_y = y - cy */
        SymbolicCoord *off_y = symbolic_coord_subtract(obj->symbolic_coords[i + 1], cy);
        if (!off_y) {
            symbolic_coord_destroy(off_x);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_dilate: subtract failed (off_y)");
        }

        /* off_x * ratio, off_y * ratio */
        SymbolicCoord *scaled_x = symbolic_coord_multiply(off_x, ratio);
        SymbolicCoord *scaled_y = symbolic_coord_multiply(off_y, ratio);
        symbolic_coord_destroy(off_x);
        symbolic_coord_destroy(off_y);
        if (!scaled_x || !scaled_y) {
            if (scaled_x)
                symbolic_coord_destroy(scaled_x);
            if (scaled_y)
                symbolic_coord_destroy(scaled_y);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_dilate: multiply failed (scaled)");
        }

        /* x' = cx + scaled_x */
        new_coords[i] = symbolic_coord_add(cx, scaled_x);
        symbolic_coord_destroy(scaled_x);
        if (!new_coords[i]) {
            symbolic_coord_destroy(scaled_y);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_dilate: add failed (new_coords[i])");
        }

        /* y' = cy + scaled_y */
        new_coords[i + 1] = symbolic_coord_add(cy, scaled_y);
        symbolic_coord_destroy(scaled_y);
        if (!new_coords[i + 1]) {
            symbolic_coord_destroy(new_coords[i]);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_dilate: add failed (new_coords[i+1])");
        }
    }

    /* 处理奇数个坐标的情况 */
    if (n % 2 != 0) {
        new_coords[n - 1] = symbolic_coord_copy(obj->symbolic_coords[n - 1]);
        if (!new_coords[n - 1]) {
            for (int j = 0; j < n - 1; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(ratio);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_dilate: copy failed (odd coord)");
        }
    }

    symbolic_coord_destroy(ratio);

    graph_add_point(graph, new_coords, n);
    for (int i = 0; i < n; i++)
        symbolic_coord_destroy(new_coords[i]);
    lv_free((void **) &new_coords);

    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_dilate: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    graph_add_line_segment(graph, (int) center_id, result_id);
    return (int64_t) result_id;
}

/** 滑移反射 */
int64_t preset_glide_reflect(lvEngine *ctx, int64_t obj_id, int64_t line_id, int64_t dx, int64_t dy) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    GeomNode *line = graph_get_node(graph, (int) line_id);
    if (!obj || !line)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_glide_reflect: NULL node input");

    /* 如果 obj 或 line 没有足够坐标，回退到复制+关联行为 */
    if (obj->coord_count == 0 || line->coord_count < 4) {
        graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
        int result_id = graph_get_last_added_node_id(graph);
        if (result_id < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_glide_reflect: graph_get_last_added_node_id failed (fallback)");
        graph_add_line_segment(graph, (int) obj_id, result_id);
        graph_add_incidence(graph, result_id, (int) line_id);
        return (int64_t) result_id;
    }

    /* 获取直线端点坐标 */
    SymbolicCoord *lx1 = line->symbolic_coords[0];
    SymbolicCoord *ly1 = line->symbolic_coords[1];
    SymbolicCoord *lx2 = line->symbolic_coords[2];
    SymbolicCoord *ly2 = line->symbolic_coords[3];

    /* 创建平移向量 */
    SymbolicCoord *dx_r = symbolic_coord_create_rational(dx, 1);
    SymbolicCoord *dy_r = symbolic_coord_create_rational(dy, 1);
    if (!dx_r || !dy_r) {
        if (dx_r)
            symbolic_coord_destroy(dx_r);
        if (dy_r)
            symbolic_coord_destroy(dy_r);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: create rational failed");
    }

    /* 计算方向向量 v = (lx2-lx1, ly2-ly1) */
    SymbolicCoord *vx = symbolic_coord_subtract(lx2, lx1);
    SymbolicCoord *vy = symbolic_coord_subtract(ly2, ly1);
    if (!vx || !vy) {
        if (vx)
            symbolic_coord_destroy(vx);
        if (vy)
            symbolic_coord_destroy(vy);
        symbolic_coord_destroy(dx_r);
        symbolic_coord_destroy(dy_r);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: subtract failed (vx/vy)");
    }

    /* 计算 v·v = vx*vx + vy*vy */
    SymbolicCoord *vx_sq = symbolic_coord_multiply(vx, vx);
    SymbolicCoord *vy_sq = symbolic_coord_multiply(vy, vy);
    if (!vx_sq || !vy_sq) {
        if (vx_sq)
            symbolic_coord_destroy(vx_sq);
        if (vy_sq)
            symbolic_coord_destroy(vy_sq);
        symbolic_coord_destroy(vx);
        symbolic_coord_destroy(vy);
        symbolic_coord_destroy(dx_r);
        symbolic_coord_destroy(dy_r);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: multiply failed (vx_sq/vy_sq)");
    }
    SymbolicCoord *v_dot_v = symbolic_coord_add(vx_sq, vy_sq);
    symbolic_coord_destroy(vx_sq);
    symbolic_coord_destroy(vy_sq);
    if (!v_dot_v) {
        symbolic_coord_destroy(vx);
        symbolic_coord_destroy(vy);
        symbolic_coord_destroy(dx_r);
        symbolic_coord_destroy(dy_r);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: add failed (v_dot_v)");
    }

    int n = obj->coord_count;
    SymbolicCoord **new_coords = lv_malloc((size_t) n * sizeof(SymbolicCoord *));
    if (!new_coords) {
        symbolic_coord_destroy(v_dot_v);
        symbolic_coord_destroy(vx);
        symbolic_coord_destroy(vy);
        symbolic_coord_destroy(dx_r);
        symbolic_coord_destroy(dy_r);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: new_coords allocation failed");
    }

    for (int i = 0; i + 1 < n; i += 2) {
        /* w = (x - lx1, y - ly1) */
        SymbolicCoord *wx = symbolic_coord_subtract(obj->symbolic_coords[i], lx1);
        SymbolicCoord *wy = symbolic_coord_subtract(obj->symbolic_coords[i + 1], ly1);
        if (!wx || !wy) {
            if (wx)
                symbolic_coord_destroy(wx);
            if (wy)
                symbolic_coord_destroy(wy);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(v_dot_v);
            symbolic_coord_destroy(vx);
            symbolic_coord_destroy(vy);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: subtract failed (wx/wy)");
        }

        /* w·v = wx*vx + wy*vy */
        SymbolicCoord *wx_vx = symbolic_coord_multiply(wx, vx);
        SymbolicCoord *wy_vy = symbolic_coord_multiply(wy, vy);
        symbolic_coord_destroy(wx);
        symbolic_coord_destroy(wy);
        if (!wx_vx || !wy_vy) {
            if (wx_vx)
                symbolic_coord_destroy(wx_vx);
            if (wy_vy)
                symbolic_coord_destroy(wy_vy);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(v_dot_v);
            symbolic_coord_destroy(vx);
            symbolic_coord_destroy(vy);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: multiply failed (wx_vx/wy_vy)");
        }
        SymbolicCoord *w_dot_v = symbolic_coord_add(wx_vx, wy_vy);
        symbolic_coord_destroy(wx_vx);
        symbolic_coord_destroy(wy_vy);
        if (!w_dot_v) {
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(v_dot_v);
            symbolic_coord_destroy(vx);
            symbolic_coord_destroy(vy);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: add failed (w_dot_v)");
        }

        /* t = (w·v) / (v·v) */
        SymbolicCoord *t = symbolic_coord_divide(w_dot_v, v_dot_v);
        symbolic_coord_destroy(w_dot_v);
        if (!t) {
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(v_dot_v);
            symbolic_coord_destroy(vx);
            symbolic_coord_destroy(vy);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: divide failed (t)");
        }

        /* proj_x = lx1 + t*vx */
        SymbolicCoord *t_vx = symbolic_coord_multiply(t, vx);
        if (!t_vx) {
            symbolic_coord_destroy(t);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(v_dot_v);
            symbolic_coord_destroy(vx);
            symbolic_coord_destroy(vy);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: multiply failed (t_vx)");
        }
        SymbolicCoord *proj_x = symbolic_coord_add(lx1, t_vx);
        symbolic_coord_destroy(t_vx);
        if (!proj_x) {
            symbolic_coord_destroy(t);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(v_dot_v);
            symbolic_coord_destroy(vx);
            symbolic_coord_destroy(vy);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: add failed (proj_x)");
        }

        /* proj_y = ly1 + t*vy */
        SymbolicCoord *t_vy = symbolic_coord_multiply(t, vy);
        symbolic_coord_destroy(t);
        if (!t_vy) {
            symbolic_coord_destroy(proj_x);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(v_dot_v);
            symbolic_coord_destroy(vx);
            symbolic_coord_destroy(vy);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: multiply failed (t_vy)");
        }
        SymbolicCoord *proj_y = symbolic_coord_add(ly1, t_vy);
        symbolic_coord_destroy(t_vy);
        if (!proj_y) {
            symbolic_coord_destroy(proj_x);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(v_dot_v);
            symbolic_coord_destroy(vx);
            symbolic_coord_destroy(vy);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: add failed (proj_y)");
        }

        /* new_x = proj_x + dx */
        new_coords[i] = symbolic_coord_add(proj_x, dx_r);
        symbolic_coord_destroy(proj_x);
        if (!new_coords[i]) {
            symbolic_coord_destroy(proj_y);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(v_dot_v);
            symbolic_coord_destroy(vx);
            symbolic_coord_destroy(vy);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: add failed (new_coords[i])");
        }

        /* new_y = proj_y + dy */
        new_coords[i + 1] = symbolic_coord_add(proj_y, dy_r);
        symbolic_coord_destroy(proj_y);
        if (!new_coords[i + 1]) {
            symbolic_coord_destroy(new_coords[i]);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(v_dot_v);
            symbolic_coord_destroy(vx);
            symbolic_coord_destroy(vy);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: add failed (new_coords[i+1])");
        }
    }

    /* 处理奇数个坐标的情况 */
    if (n % 2 != 0) {
        new_coords[n - 1] = symbolic_coord_copy(obj->symbolic_coords[n - 1]);
        if (!new_coords[n - 1]) {
            for (int j = 0; j < n - 1; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(v_dot_v);
            symbolic_coord_destroy(vx);
            symbolic_coord_destroy(vy);
            symbolic_coord_destroy(dx_r);
            symbolic_coord_destroy(dy_r);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_glide_reflect: copy failed (odd coord)");
        }
    }

    symbolic_coord_destroy(v_dot_v);
    symbolic_coord_destroy(vx);
    symbolic_coord_destroy(vy);
    symbolic_coord_destroy(dx_r);
    symbolic_coord_destroy(dy_r);

    graph_add_point(graph, new_coords, n);
    for (int i = 0; i < n; i++)
        symbolic_coord_destroy(new_coords[i]);
    lv_free((void **) &new_coords);

    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_glide_reflect: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    graph_add_incidence(graph, result_id, (int) line_id);
    return (int64_t) result_id;
}

/** 绕指定点旋转 */
int64_t preset_rotation_about(lvEngine *ctx, int64_t obj_id, int64_t center_id, int64_t angle_mrad) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    GeomNode *center = graph_get_node(graph, (int) center_id);
    if (!obj || !center)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_rotation_about: NULL node input");

    /* 如果 obj 或 center 没有坐标，回退到复制行为 */
    if (obj->coord_count == 0 || center->coord_count < 2) {
        graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
        int result_id = graph_get_last_added_node_id(graph);
        if (result_id < 0)
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_rotation_about: graph_get_last_added_node_id failed (fallback)");
        graph_add_line_segment(graph, (int) obj_id, result_id);
        graph_add_line_segment(graph, (int) center_id, result_id);
        return (int64_t) result_id;
    }

    /* 创建 sinθ 和 cosθ 超越数 */
    SymbolicCoord *sin_theta = symbolic_coord_create_transcendental("sinθ_mrad");
    SymbolicCoord *cos_theta = symbolic_coord_create_transcendental("cosθ_mrad");
    if (!sin_theta || !cos_theta) {
        if (sin_theta)
            symbolic_coord_destroy(sin_theta);
        if (cos_theta)
            symbolic_coord_destroy(cos_theta);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotation_about: create transcendental failed");
    }

    SymbolicCoord *cx = center->symbolic_coords[0];
    SymbolicCoord *cy = center->symbolic_coords[1];
    int n = obj->coord_count;
    SymbolicCoord **new_coords = lv_malloc((size_t) n * sizeof(SymbolicCoord *));
    if (!new_coords) {
        symbolic_coord_destroy(sin_theta);
        symbolic_coord_destroy(cos_theta);
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotation_about: new_coords allocation failed");
    }

    /* P' = C + rotate(P-C)
     * 1. offset = P - C
     * 2. rotated_x = offset_x*cosθ - offset_y*sinθ
     *    rotated_y = offset_x*sinθ + offset_y*cosθ
     * 3. new_x = cx + rotated_x, new_y = cy + rotated_y */
    for (int i = 0; i + 1 < n; i += 2) {
        /* offset_x = x - cx, offset_y = y - cy */
        SymbolicCoord *off_x = symbolic_coord_subtract(obj->symbolic_coords[i], cx);
        SymbolicCoord *off_y = symbolic_coord_subtract(obj->symbolic_coords[i + 1], cy);
        if (!off_x || !off_y) {
            if (off_x)
                symbolic_coord_destroy(off_x);
            if (off_y)
                symbolic_coord_destroy(off_y);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotation_about: subtract failed (off_x/off_y)");
        }

        /* rotated_x = off_x*cosθ - off_y*sinθ */
        SymbolicCoord *ox_cos = symbolic_coord_multiply(off_x, cos_theta);
        SymbolicCoord *oy_sin = symbolic_coord_multiply(off_y, sin_theta);
        if (!ox_cos || !oy_sin) {
            if (ox_cos)
                symbolic_coord_destroy(ox_cos);
            if (oy_sin)
                symbolic_coord_destroy(oy_sin);
            symbolic_coord_destroy(off_x);
            symbolic_coord_destroy(off_y);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotation_about: multiply failed (ox_cos/oy_sin)");
        }
        SymbolicCoord *rot_x = symbolic_coord_subtract(ox_cos, oy_sin);
        symbolic_coord_destroy(ox_cos);
        symbolic_coord_destroy(oy_sin);
        if (!rot_x) {
            symbolic_coord_destroy(off_x);
            symbolic_coord_destroy(off_y);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotation_about: subtract failed (rot_x)");
        }

        /* rotated_y = off_x*sinθ + off_y*cosθ */
        SymbolicCoord *ox_sin = symbolic_coord_multiply(off_x, sin_theta);
        SymbolicCoord *oy_cos = symbolic_coord_multiply(off_y, cos_theta);
        symbolic_coord_destroy(off_x);
        symbolic_coord_destroy(off_y);
        if (!ox_sin || !oy_cos) {
            if (ox_sin)
                symbolic_coord_destroy(ox_sin);
            if (oy_cos)
                symbolic_coord_destroy(oy_cos);
            symbolic_coord_destroy(rot_x);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotation_about: multiply failed (ox_sin/oy_cos)");
        }
        SymbolicCoord *rot_y = symbolic_coord_add(ox_sin, oy_cos);
        symbolic_coord_destroy(ox_sin);
        symbolic_coord_destroy(oy_cos);
        if (!rot_y) {
            symbolic_coord_destroy(rot_x);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotation_about: add failed (rot_y)");
        }

        /* new_x = cx + rot_x */
        new_coords[i] = symbolic_coord_add(cx, rot_x);
        symbolic_coord_destroy(rot_x);
        if (!new_coords[i]) {
            symbolic_coord_destroy(rot_y);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotation_about: add failed (new_coords[i])");
        }

        /* new_y = cy + rot_y */
        new_coords[i + 1] = symbolic_coord_add(cy, rot_y);
        symbolic_coord_destroy(rot_y);
        if (!new_coords[i + 1]) {
            symbolic_coord_destroy(new_coords[i]);
            for (int j = 0; j < i; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotation_about: add failed (new_coords[i+1])");
        }
    }

    /* 处理奇数个坐标的情况 */
    if (n % 2 != 0) {
        new_coords[n - 1] = symbolic_coord_copy(obj->symbolic_coords[n - 1]);
        if (!new_coords[n - 1]) {
            for (int j = 0; j < n - 1; j++)
                symbolic_coord_destroy(new_coords[j]);
            lv_free((void **) &new_coords);
            symbolic_coord_destroy(sin_theta);
            symbolic_coord_destroy(cos_theta);
            lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "preset_rotation_about: copy failed (odd coord)");
        }
    }

    symbolic_coord_destroy(sin_theta);
    symbolic_coord_destroy(cos_theta);

    graph_add_point(graph, new_coords, n);
    for (int i = 0; i < n; i++)
        symbolic_coord_destroy(new_coords[i]);
    lv_free((void **) &new_coords);

    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_rotation_about: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    graph_add_line_segment(graph, (int) center_id, result_id);
    return (int64_t) result_id;
}

/** 关于指定直线的反射 */
int64_t preset_reflection_about(lvEngine *ctx, int64_t obj_id, int64_t line_id) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    GeomNode *line = graph_get_node(graph, (int) line_id);
    if (!obj || !line)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_reflection_about: NULL node input");
    graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_reflection_about: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    graph_add_incidence(graph, result_id, (int) line_id);
    return (int64_t) result_id;
}

/** 投影变换 */
int64_t preset_projection(lvEngine *ctx, int64_t obj_id, int64_t line_id) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    GeomNode *line = graph_get_node(graph, (int) line_id);
    if (!obj || !line)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_projection: NULL node input");
    graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_projection: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    graph_add_incidence(graph, result_id, (int) line_id);
    return (int64_t) result_id;
}

/** 反演变换 */
int64_t preset_inversion(lvEngine *ctx, int64_t obj_id, int64_t circle_id) {
    ConstraintGraph *graph = ctx->main_graph;
    GeomNode *obj = graph_get_node(graph, (int) obj_id);
    GeomNode *circle = graph_get_node(graph, (int) circle_id);
    if (!obj || !circle)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "preset_inversion: NULL node input");
    graph_add_point(graph, obj->symbolic_coords, obj->coord_count);
    int result_id = graph_get_last_added_node_id(graph);
    if (result_id < 0)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "preset_inversion: graph_get_last_added_node_id failed");
    graph_add_line_segment(graph, (int) obj_id, result_id);
    graph_add_line_segment(graph, (int) circle_id, result_id);
    return (int64_t) result_id;
}
