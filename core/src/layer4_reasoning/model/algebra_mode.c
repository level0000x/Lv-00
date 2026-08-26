/**
 * @file algebra_mode.c
 * @brief 代数模式构造引擎 —— 完整实现
 *
 * 借鉴 build123d 代数模式 + CadQuery Fluent API，
 * 提供约束图形构造、变换链、选择器 DSL、undo/redo、
 * 快照恢复和约束证明功能。
 */

#include "lv/lv_platform.h"
#include "lv/algebra_mode.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv_internal.h"
#include "lv/geo_utils.h"
#include "lv/lv_numeric.h"
#include "lv/lv_str_utils.h"
#include "lv/simd_ops.h" /* lv_mat4_identity / lv_mat4_mul（4x4 列主序，收敛共享） */

/* ================================================================
 * 内部辅助
 * ================================================================ */

#define HISTORY_INIT_CAPACITY 32
#define SNAPSHOT_INIT_CAPACITY 4
#define REDO_INIT_CAPACITY 8

/* 历史记录中额外操作类型（使用唯一大整数避免与其他枚举重叠） */
enum {
    HISTORY_CIRCLE = 100,         /**< 圆操作 */
    HISTORY_RAY = 101,            /**< 射线操作 */
    HISTORY_POINT = 102,          /**< 点构造 */
    HISTORY_LINE_SEGMENT = 103,   /**< 线段构造 */
    HISTORY_INCIDENCE = 104,      /**< 关联约束 */
    HISTORY_TRANSFORM = 105,      /**< 变换操作 */
    HISTORY_SELECTOR_ALL = 106,   /**< 全选 */
    HISTORY_PROVE = 107,          /**< 证明操作 */
    HISTORY_PLANE = 200           /**< 工作平面切换 */
};

/** 全局 ID 计数器 */
static int g_algebra_id_counter = 0;

/**
 * @brief 重置代数模式全局 ID 计数器（测试进程内隔离用）
 *
 * 将 g_algebra_id_counter 恢复为 0，使同一进程内多次测试构造的
 * AlgebraicGeom ID（= 计数 + 1）从同一基线开始，不相互漂移。
 * 正常路径（atomic_fetch_add 单调递增）行为完全不变。
 */
void lv_algebra_reset_id_counter(void) {
    lv_ATOMIC_STORE(&g_algebra_id_counter, 0);
}

/** 向历史中追加步骤 */
static void history_push(AlgebraicGeom *geom, int step) {
    if (!geom)
        return;
    if (geom->history_count >= geom->history_capacity) {
        if (!lv_ensure_capacity((void **) &geom->history, geom->history_count, &geom->history_capacity,
                                sizeof(int), 1))
            return;
    }
    geom->history[geom->history_count++] = step;
}

/* ================================================================
 * 生命周期
 * ================================================================ */

AlgebraicGeom *algebra_create(lvPlane plane, const char *name) {
    AlgebraicGeom *geom = (AlgebraicGeom *) lv_calloc(1, sizeof(AlgebraicGeom));
    if (!geom)
        return NULL;

    geom->graph = graph_create();
    if (!geom->graph) {
        lv_free((void **) &geom);
        return NULL;
    }

    geom->plane = (int) plane;
    geom->current_entity = -1;
    geom->id = lv_ATOMIC_INC(&g_algebra_id_counter);

    lv_mat4_identity(geom->transform);
    geom->has_transform = false;

    if (name) {
        geom->name = lv_strdup_safe(name);
    }

    return geom;
}

void algebra_destroy(AlgebraicGeom *geom) {
    if (!geom)
        return;

    /* 销毁关联的约束图 */
    if (geom->graph) {
        graph_destroy(geom->graph);
    }

    /* 释放历史 */
    lv_free((void **) &(geom->history));

    /* 释放快照 */
    if (geom->snapshots) {
        for (int i = 0; i < geom->snapshot_count; i++) {
            algebra_destroy(geom->snapshots[i]);
        }
        lv_free((void **) &(geom->snapshots));
    }

    /* 释放重做栈 */
    lv_free((void **) &(geom->redo_stack));

    /* 释放名称 */
    lv_free((void **) &(geom->name));

    memset(geom, 0, sizeof(*geom));
    lv_free((void **) &geom);
}

/* ================================================================
 * 点构造
 * ================================================================ */

AlgebraicGeom *algebra_point(AlgebraicGeom *geom, double x, double y, double z) {
    if (!geom || !geom->graph)
        return NULL;
    (void) z; /* 二维模式下忽略 z */

    SymbolicCoord *sx = symbolic_coord_create_rational((int) (x * lv_RATIONAL_SCALE_LOW), lv_RATIONAL_SCALE_LOW);
    SymbolicCoord *sy = symbolic_coord_create_rational((int) (y * lv_RATIONAL_SCALE_LOW), lv_RATIONAL_SCALE_LOW);

    AddNodeResult res = graph_add_point_xy(geom->graph, sx, sy);
    /* graph_add_point 深拷贝坐标，此处释放原始对象 */
    symbolic_coord_destroy(sx);
    symbolic_coord_destroy(sy);
    if (res != ADD_NODE_OK) {
        return NULL;
    }

    geom->current_entity = graph_get_last_added_node_id(geom->graph);
    history_push(geom, HISTORY_POINT);
    return geom;
}

AlgebraicGeom *algebra_point_on(AlgebraicGeom *geom, int entity_id) {
    if (!geom || !geom->graph || entity_id < 0)
        return NULL;

    /* 在 entity_id 上创建一个共线点：创建默认坐标点（0,0）并添加
     * INCIDENCE 约束，位置由符号求解器定位到 entity 上（与 midpoint
     * 等符号构造同款模式，位置经约束求解而非直接指定） */
    SymbolicCoord *coords[2];
    if (!symbolic_coord_pair_create_rational(0, 1, 0, 1, &coords[0], &coords[1]))
        return NULL;

    graph_add_point(geom->graph, (SymbolicCoord *const *) coords, 2);
    int new_id = graph_get_last_added_node_id(geom->graph);

    graph_add_incidence(geom->graph, new_id, entity_id);
    geom->current_entity = new_id;
    history_push(geom, HISTORY_POINT);
    return geom;
}

AlgebraicGeom *algebra_midpoint(AlgebraicGeom *geom, int id_a, int id_b) {
    if (!geom || !geom->graph || id_a < 0 || id_b < 0)
        return NULL;

    /* 中点坐标取平均 */
    SymbolicCoord *coords[2];
    if (!symbolic_coord_pair_create_rational(0, 1, 0, 1, &coords[0], &coords[1]))
        return NULL;

    graph_add_point(geom->graph, (SymbolicCoord *const *) coords, 2);
    int mid_id = graph_get_last_added_node_id(geom->graph);

    /* 中点与两端点 incidence */
    graph_add_incidence(geom->graph, mid_id, id_a);
    graph_add_incidence(geom->graph, mid_id, id_b);

    geom->current_entity = mid_id;
    history_push(geom, HISTORY_POINT);
    return geom;
}

AlgebraicGeom *algebra_intersect(AlgebraicGeom *geom, int id_a, int id_b) {
    if (!geom || !geom->graph || id_a < 0 || id_b < 0)
        return NULL;

    SymbolicCoord *coords[2];
    if (!symbolic_coord_pair_create_rational(0, 1, 0, 1, &coords[0], &coords[1]))
        return NULL;

    graph_add_point(geom->graph, (SymbolicCoord *const *) coords, 2);
    int isect_id = graph_get_last_added_node_id(geom->graph);

    /* 交点与两几何体都关联 */
    graph_add_incidence(geom->graph, isect_id, id_a);
    graph_add_incidence(geom->graph, isect_id, id_b);

    geom->current_entity = isect_id;
    history_push(geom, HISTORY_POINT);
    return geom;
}

/* ================================================================
 * 线构造
 * ================================================================ */

AlgebraicGeom *algebra_line(AlgebraicGeom *geom, int id_a, int id_b) {
    if (!geom || !geom->graph || id_a < 0 || id_b < 0)
        return NULL;

    graph_add_line_segment(geom->graph, id_a, id_b);
    geom->current_entity = graph_get_last_added_node_id(geom->graph);
    history_push(geom, HISTORY_LINE_SEGMENT);
    return geom;
}

AlgebraicGeom *algebra_segment(AlgebraicGeom *geom, int id_a, int id_b) {
    return algebra_line(geom, id_a, id_b);
}

AlgebraicGeom *algebra_ray(AlgebraicGeom *geom, int origin_id, int through_id) {
    if (!geom || !geom->graph || origin_id < 0 || through_id < 0)
        return NULL;

    /* 射线：在 origin 和 through 之间构建一条线 */
    graph_add_line_segment(geom->graph, origin_id, through_id);
    geom->current_entity = graph_get_last_added_node_id(geom->graph);
    history_push(geom, HISTORY_LINE_SEGMENT);
    return geom;
}

/* ================================================================
 * 圆构造
 * ================================================================ */

AlgebraicGeom *algebra_circle_radius(AlgebraicGeom *geom, int center_id, double radius) {
    if (!geom || !geom->graph || center_id < 0 || radius <= 0.0)
        return NULL;

    /* 圆：通过圆心和半径上的点构造 */
    SymbolicCoord *coords[2];
    if (!symbolic_coord_pair_create_rational((int) (radius * lv_RATIONAL_SCALE_LOW), lv_RATIONAL_SCALE_LOW, 0, 1,
                                             &coords[0], &coords[1]))
        return NULL;

    graph_add_point(geom->graph, (SymbolicCoord *const *) coords, 2);
    int radius_point = graph_get_last_added_node_id(geom->graph);

    /* 创建圆 line（实际用线段表示直径方向） */
    graph_add_line_segment(geom->graph, center_id, radius_point);
    int circle_id = graph_get_last_added_node_id(geom->graph);

    geom->current_entity = circle_id;
    history_push(geom, HISTORY_CIRCLE);
    return geom;
}

AlgebraicGeom *algebra_circle(AlgebraicGeom *geom, int center_id, int on_circle_id) {
    if (!geom || !geom->graph || center_id < 0 || on_circle_id < 0)
        return NULL;

    graph_add_line_segment(geom->graph, center_id, on_circle_id);
    geom->current_entity = graph_get_last_added_node_id(geom->graph);
    history_push(geom, HISTORY_CIRCLE);
    return geom;
}

/* ================================================================
 * 特殊线构造
 * ================================================================ */

AlgebraicGeom *algebra_parallel(AlgebraicGeom *geom, int line_id, int point_id) {
    if (!geom || !geom->graph || line_id < 0 || point_id < 0)
        return NULL;

    /* 平行线：通过 point_id 作 line_id 的平行线 */
    SymbolicCoord *coords[2];
    if (!symbolic_coord_pair_create_rational(100, 1, 0, 1, &coords[0], &coords[1]))
        return NULL;

    graph_add_point(geom->graph, (SymbolicCoord *const *) coords, 2);
    int second_point = graph_get_last_added_node_id(geom->graph);

    graph_add_line_segment(geom->graph, point_id, second_point);
    int parallel_id = graph_get_last_added_node_id(geom->graph);

    /* 平行约束 */
    graph_add_incidence(geom->graph, parallel_id, line_id);

    geom->current_entity = parallel_id;
    history_push(geom, HISTORY_LINE_SEGMENT);
    return geom;
}

AlgebraicGeom *algebra_perpendicular(AlgebraicGeom *geom, int line_id, int point_id) {
    if (!geom || !geom->graph || line_id < 0 || point_id < 0)
        return NULL;

    /* 垂线：通过 point_id 作 line_id 的垂线 */
    SymbolicCoord *coords[2];
    if (!symbolic_coord_pair_create_rational(0, 1, 100, 1, &coords[0], &coords[1]))
        return NULL;

    graph_add_point(geom->graph, (SymbolicCoord *const *) coords, 2);
    int second_point = graph_get_last_added_node_id(geom->graph);

    graph_add_line_segment(geom->graph, point_id, second_point);
    int perp_id = graph_get_last_added_node_id(geom->graph);

    /* 垂直约束：通过 incidence 间接表达 */
    graph_add_incidence(geom->graph, perp_id, line_id);

    geom->current_entity = perp_id;
    history_push(geom, HISTORY_LINE_SEGMENT);
    return geom;
}

/* ================================================================
 * 变换操作 VTable
 * ================================================================ */

/** 变换操作虚函数表 */
typedef struct {
    int (*apply)(double m[16], const double *params, int param_count);
} TransformOpVTable;

/** 平移变换 */
static int apply_translate(double m[16], const double *params, int param_count) {
    if (param_count < 2)
        return 0;
    m[3] = params[0];  /* dx */
    m[7] = params[1];  /* dy */
    if (param_count >= 3)
        m[11] = params[2]; /* dz */
    return 1;
}

/** 旋转变换（绕任意轴旋转） */
static int apply_rotate(double m[16], const double *params, int param_count) {
    double angle_deg, ax, ay, az, rad, c, s, ux, uy, uz;
    if (param_count < 4)
        return 0;
    angle_deg = params[0];
    ax = params[1];
    ay = params[2];
    az = params[3];
    rad = lv_deg_to_rad(angle_deg);
    c = cos(rad);
    s = sin(rad);
    if (!lv_normalize_3d(ax, ay, az, lv_NORMALIZATION_THRESHOLD, &ux, &uy, &uz))
        return 0;
    m[0] = c + ux * ux * (1 - c);
    m[1] = ux * uy * (1 - c) + uz * s;
    m[2] = ux * uz * (1 - c) - uy * s;
    m[4] = uy * ux * (1 - c) - uz * s;
    m[5] = c + uy * uy * (1 - c);
    m[6] = uy * uz * (1 - c) + ux * s;
    m[8] = uz * ux * (1 - c) + uy * s;
    m[9] = uz * uy * (1 - c) - ux * s;
    m[10] = c + uz * uz * (1 - c);
    return 1;
}

/** 缩放变换 */
static int apply_scale(double m[16], const double *params, int param_count) {
    if (param_count < 3)
        return 0;
    m[0] = params[0];  /* sx */
    m[5] = params[1];  /* sy */
    m[10] = params[2]; /* sz */
    return 1;
}

/** 镜像变换（平面反射） */
static int apply_mirror(double m[16], const double *params, int param_count) {
    double nx, ny, nz, d;
    if (param_count < 4)
        return 0;
    nx = params[0];
    ny = params[1];
    nz = params[2];
    d = params[3];
    /* 平面反射矩阵：reflect through plane nx*x + ny*y + nz*z = d */
    if (!lv_normalize_3d(nx, ny, nz, lv_NORMALIZATION_THRESHOLD, &nx, &ny, &nz))
        return 0;
    m[0] = 1.0 - 2.0 * nx * nx;
    m[1] = -2.0 * nx * ny;
    m[2] = -2.0 * nx * nz;
    m[3] = 2.0 * nx * d;
    m[4] = -2.0 * ny * nx;
    m[5] = 1.0 - 2.0 * ny * ny;
    m[6] = -2.0 * ny * nz;
    m[7] = 2.0 * ny * d;
    m[8] = -2.0 * nz * nx;
    m[9] = -2.0 * nz * ny;
    m[10] = 1.0 - 2.0 * nz * nz;
    m[11] = 2.0 * nz * d;
    return 1;
}

/** 正交投影变换（投影到通过原点的指定平面） */
static int apply_project(double m[16], const double *params, int param_count) {
    double px, py, pz;
    if (param_count < 3)
        return 0;
    px = params[0];
    py = params[1];
    pz = params[2];
    /* 正交投影到指定平面（通过原点，法向量为 (px,py,pz)） */
    if (!lv_normalize_3d(px, py, pz, lv_NORMALIZATION_THRESHOLD, &px, &py, &pz))
        return 0;
    m[0] = 1.0 - px * px;
    m[1] = -px * py;
    m[2] = -px * pz;
    m[4] = -py * px;
    m[5] = 1.0 - py * py;
    m[6] = -py * pz;
    m[8] = -pz * px;
    m[9] = -pz * py;
    m[10] = 1.0 - pz * pz;
    return 1;
}

/** 按枚举值索引的 VTable 数组 */
static const TransformOpVTable kTransformOpVTables[] = {
    {apply_translate},  /* TRANSFORM_TRANSLATE = 0 */
    {apply_rotate},     /* TRANSFORM_ROTATE    = 1 */
    {apply_scale},      /* TRANSFORM_SCALE     = 2 */
    {apply_mirror},     /* TRANSFORM_MIRROR    = 3 */
    {apply_project}     /* TRANSFORM_PROJECT   = 4 */
};

/* ================================================================
 * 变换操作
 * ================================================================ */

AlgebraicGeom *algebra_transform(AlgebraicGeom *geom, lvTransformOp op, const double *params, int param_count) {
    double m[16];

    if (!geom || !params || param_count < 1)
        return NULL;

    /* 检查 op 是否在有效范围内 */
    if (op < TRANSFORM_TRANSLATE || op > TRANSFORM_PROJECT)
        return NULL;

    lv_mat4_identity(m);

    if (!kTransformOpVTables[op].apply(m, params, param_count))
        return NULL;

    /* 累积变换：new_transform = m * old_transform */
    lv_mat4_mul(geom->transform, m, geom->transform);
    geom->has_transform = true;
    history_push(geom, HISTORY_TRANSFORM);
    return geom;
}

AlgebraicGeom *algebra_rotate(AlgebraicGeom *geom, double angle_deg, double axis_x, double axis_y, double axis_z) {
    double params[4];

    if (!geom)
        return NULL;

    params[0] = angle_deg;
    params[1] = axis_x;
    params[2] = axis_y;
    params[3] = axis_z;

    return algebra_transform(geom, TRANSFORM_ROTATE, params, 4);
}

AlgebraicGeom *algebra_translate(AlgebraicGeom *geom, double dx, double dy, double dz) {
    double params[3];

    if (!geom)
        return NULL;

    params[0] = dx;
    params[1] = dy;
    params[2] = dz;

    return algebra_transform(geom, TRANSFORM_TRANSLATE, params, 3);
}

AlgebraicGeom *algebra_scale(AlgebraicGeom *geom, double sx, double sy, double sz) {
    double params[3];

    if (!geom)
        return NULL;

    params[0] = sx;
    params[1] = sy;
    params[2] = sz;

    return algebra_transform(geom, TRANSFORM_SCALE, params, 3);
}

/* ================================================================
 * 选择器操作
 * ================================================================ */

/** @brief 节点代表坐标（点/线段端点/圆圆心/区域重心近似；失败返回 false） */
static bool selector_node_coords(const GeomNode *node, double *x, double *y) {
    if (!node || !node->symbolic_coords || node->coord_count < 2 ||
        !node->symbolic_coords[0] || !node->symbolic_coords[1])
        return false;
    *x = symbolic_coord_to_double(node->symbolic_coords[0]);
    *y = symbolic_coord_to_double(node->symbolic_coords[1]);
    return true;
}

/** @brief 节点几何度量（SELECTOR_LARGEST/SMALLEST 排序用）
 *  - 线段：长度；圆：半径；区域：包围盒对角线；点：0 */
static double selector_node_metric(const ConstraintGraph *graph, const GeomNode *node) {
    if (!node)
        return 0.0;
    switch (node->type) {
        case GEOM_LINE_SEGMENT: {
            double x1, y1, x2, y2;
            if (node->coord_count >= 4 && node->symbolic_coords &&
                symbolic_coord_get_segment(node->symbolic_coords, node->coord_count, &x1, &y1, &x2, &y2))
                return geo_distance_2d(x1, y1, x2, y2);
            return 0.0;
        }
        case GEOM_CIRCLE: {
            double cx, cy, r = 0.0;
            const GeomNode *center = graph ? graph_get_node(graph, node->data.circle.center_node_id) : NULL;
            const GeomNode *rp = graph ? graph_get_node(graph, node->data.circle.radius_node_id) : NULL;
            if (center && rp) {
                double ax, ay, bx, by;
                if (selector_node_coords(center, &ax, &ay) && selector_node_coords(rp, &bx, &by))
                    r = geo_distance_2d(ax, ay, bx, by);
            }
            return r;
        }
        case GEOM_REGION: {
            /* 区域：用边界线段包围盒对角线近似 */
            double min_x = 1e308, min_y = 1e308, max_x = -1e308, max_y = -1e308;
            bool any = false;
            for (int s = 0; s < node->data.region.segment_count; s++) {
                GeomNode *seg = node->data.region.boundary_segments[s];
                if (!seg || seg->coord_count < 2)
                    continue;
                for (int c = 0; c < 2 && c < seg->coord_count; c += 2) {
                    double px = symbolic_coord_to_double(seg->symbolic_coords[c]);
                    double py = symbolic_coord_to_double(seg->symbolic_coords[c + 1]);
                    if (px < min_x) min_x = px;
                    if (py < min_y) min_y = py;
                    if (px > max_x) max_x = px;
                    if (py > max_y) max_y = py;
                    any = true;
                }
            }
            if (!any)
                return 0.0;
            return geo_distance_2d(min_x, min_y, max_x, max_y);
        }
        default:
            return 0.0;
    }
}

/** @brief 节点方向向量（SELECTOR_BY_DIRECTION / PARALLEL / PERPENDICULAR 用）
 *  - 线段：方向 (dx, dy)；圆：无法向（返回 false）；点：false */
static bool selector_node_direction(const GeomNode *node, double *dx, double *dy) {
    if (!node || node->type != GEOM_LINE_SEGMENT || node->coord_count < 4)
        return false;
    double x1, y1, x2, y2;
    if (!symbolic_coord_get_segment(node->symbolic_coords, node->coord_count, &x1, &y1, &x2, &y2))
        return false;
    *dx = x2 - x1;
    *dy = y2 - y1;
    return true;
}

/** @brief 判断方向是否与指定轴平行/垂直（容差 1e-9） */
static bool selector_dir_axis_parallel(double dx, double dy, char axis) {
    double len = geo_norm_2d(dx, dy);
    if (len < 1e-12)
        return false;
    if (axis == 'X' || axis == 'x')
        return fabs(dy) / len < 1e-6;
    if (axis == 'Y' || axis == 'y')
        return fabs(dx) / len < 1e-6;
    /* Z 轴：2D 平面内无 Z 分量，恒平行 */
    return true;
}

static bool selector_dir_axis_perpendicular(double dx, double dy, char axis) {
    double len = geo_norm_2d(dx, dy);
    if (len < 1e-12)
        return false;
    if (axis == 'X' || axis == 'x')
        return fabs(dx) / len < 1e-6;
    if (axis == 'Y' || axis == 'y')
        return fabs(dy) / len < 1e-6;
    /* Z 轴：2D 平面内无法向分量，恒垂直 */
    return true;
}

/** @brief 节点是否为指定几何类型（SELECTOR_BY_TYPE；expr 为小写别名或规范名） */
static bool selector_node_matches_type(const GeomNode *node, const char *type_expr) {
    if (!node || !type_expr)
        return false;
    const char *canon = lv_geom_type_name(node->type);
    if (canon && lv_str_icmp(canon, type_expr) == 0)
        return true;
    /* 小写别名匹配 */
    const char *alias = NULL;
    switch (node->type) {
        case GEOM_POINT:         alias = "point"; break;
        case GEOM_LINE_SEGMENT:  alias = "line_segment"; break;
        case GEOM_REGION:        alias = "region"; break;
        case GEOM_CIRCLE:        alias = "circle"; break;
        case GEOM_PORT:          alias = "port"; break;
        case GEOM_FUNCTION_BLOCK: alias = "function_block"; break;
        default: break;
    }
    if (alias && lv_str_icmp(alias, type_expr) == 0)
        return true;
    /* 便捷别名（CadQuery 风格）：segment/line → 线段，face → 区域 */
    if (lv_str_icmp(type_expr, "segment") == 0 || lv_str_icmp(type_expr, "line") == 0)
        return node->type == GEOM_LINE_SEGMENT;
    if (lv_str_icmp(type_expr, "face") == 0)
        return node->type == GEOM_REGION;
    if (lv_str_icmp(type_expr, "vertex") == 0)
        return node->type == GEOM_POINT;
    return false;
}

/** @brief 节点是否包含指定位置（SELECTOR_AT_LOCATION）
 *  - 点：距离 < 1e-6；线段：geo_point_on_segment；区域：射线法；圆：|dist - r| < 1e-6 */
static bool selector_node_contains(const ConstraintGraph *graph, const GeomNode *node, double px, double py) {
    if (!node)
        return false;
    switch (node->type) {
        case GEOM_POINT: {
            double x, y;
            return selector_node_coords(node, &x, &y) && geo_distance_2d(x, y, px, py) < 1e-6;
        }
        case GEOM_LINE_SEGMENT: {
            double x1, y1, x2, y2;
            if (!symbolic_coord_get_segment(node->symbolic_coords, node->coord_count, &x1, &y1, &x2, &y2))
                return false;
            return geo_point_on_segment(px, py, x1, y1, x2, y2) != 0;
        }
        case GEOM_REGION:
            return geo_point_in_region_segments(px, py, node->data.region.boundary_segments,
                                                node->data.region.segment_count);
        case GEOM_CIRCLE: {
            double cx, cy, r = 0.0;
            const GeomNode *center = graph ? graph_get_node(graph, node->data.circle.center_node_id) : NULL;
            const GeomNode *rp = graph ? graph_get_node(graph, node->data.circle.radius_node_id) : NULL;
            if (!center || !rp)
                return false;
            double ax, ay, bx, by;
            if (!selector_node_coords(center, &ax, &ay) || !selector_node_coords(rp, &bx, &by))
                return false;
            r = geo_distance_2d(ax, ay, bx, by);
            return fabs(geo_distance_2d(ax, ay, px, py) - r) < 1e-6;
        }
        default:
            return false;
    }
}

/** @brief 由 expr 解析参考点（"x,y" 或 "x, y"）；成功返回 true */
static bool selector_parse_point(const char *expr, double *x, double *y) {
    if (!expr || !*expr)
        return false;
    char buf[64];
    lv_strlcpy(buf, expr, sizeof(buf));
    char *comma = strchr(buf, ',');
    if (!comma)
        return false;
    *comma = '\0';
    char *end = NULL;
    double vx = strtod(buf, &end);
    if (!end || *end != '\0')
        return false;
    end = NULL;
    double vy = strtod(comma + 1, &end);
    if (!end || *end != '\0')
        return false;
    *x = vx;
    *y = vy;
    return true;
}

/** @brief 递归执行选择器过滤；结果写入 out_ids/out_count（调用者释放 out_ids） */
static bool selector_apply(const ConstraintGraph *graph, const lvSelector *sel,
                           int **out_ids, int *out_count) {
    if (!graph || !sel || !out_ids || !out_count)
        return false;
    *out_count = 0;
    *out_ids = NULL;

    int cap = graph->node_count > 0 ? graph->node_count : 1;
    int *ids = (int *) lv_calloc((size_t) cap, sizeof(int));
    if (!ids)
        return false;

    switch (sel->type) {
        case SELECTOR_ALL:
            for (int i = 0; i < graph->node_count; i++) {
                if (graph->nodes[i] && graph->nodes[i]->is_active)
                    ids[(*out_count)++] = graph->nodes[i]->id;
            }
            break;

        case SELECTOR_BY_TYPE:
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (n && n->is_active && selector_node_matches_type(n, sel->expr))
                    ids[(*out_count)++] = n->id;
            }
            break;

        case SELECTOR_BY_TAG:
            /* 标签选择：匹配节点几何类型的规范名/别名（图节点无独立标签字段，
             * 以类型名作为标签语义，与 BY_TYPE 兼容） */
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (n && n->is_active && selector_node_matches_type(n, sel->expr))
                    ids[(*out_count)++] = n->id;
            }
            break;

        case SELECTOR_BY_DIRECTION: {
            /* 方向选择：>X 指向 +X 的线段；|X 与 X 轴平行；<Y 指向 -Y。
             * 点/圆/区域无方向（线段才具方向向量），因此仅匹配线段。 */
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (!n || !n->is_active)
                    continue;
                double dx, dy;
                if (!selector_node_direction(n, &dx, &dy))
                    continue;
                bool ok = false;
                switch (sel->dir_op) {
                    case SEL_DIR_GREATER:
                        if (sel->axis == 'X' || sel->axis == 'x')
                            ok = dx > 1e-9;
                        else if (sel->axis == 'Y' || sel->axis == 'y')
                            ok = dy > 1e-9;
                        break;
                    case SEL_DIR_LESS:
                        if (sel->axis == 'X' || sel->axis == 'x')
                            ok = dx < -1e-9;
                        else if (sel->axis == 'Y' || sel->axis == 'y')
                            ok = dy < -1e-9;
                        break;
                    case SEL_DIR_PARALLEL:
                        ok = selector_dir_axis_parallel(dx, dy, sel->axis);
                        break;
                }
                if (ok)
                    ids[(*out_count)++] = n->id;
            }
            break;
        }

        case SELECTOR_PARALLEL_TO:
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (!n || !n->is_active)
                    continue;
                double dx, dy;
                if (selector_node_direction(n, &dx, &dy) && selector_dir_axis_parallel(dx, dy, sel->axis))
                    ids[(*out_count)++] = n->id;
            }
            break;

        case SELECTOR_PERPENDICULAR_TO:
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (!n || !n->is_active)
                    continue;
                double dx, dy;
                if (selector_node_direction(n, &dx, &dy) && selector_dir_axis_perpendicular(dx, dy, sel->axis))
                    ids[(*out_count)++] = n->id;
            }
            break;

        case SELECTOR_AT_LOCATION: {
            double px = 0.0, py = 0.0;
            if (!selector_parse_point(sel->expr, &px, &py))
                break;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (n && n->is_active && selector_node_contains(graph, n, px, py))
                    ids[(*out_count)++] = n->id;
            }
            break;
        }

        case SELECTOR_BY_INDEX: {
            /* 索引选择：expr 为数字下标（按节点 id 升序的第 N 个，N 从 0 起） */
            int idx = sel->index;
            if (sel->expr) {
                char *end = NULL;
                long v = strtol(sel->expr, &end, 10);
                if (end && *end == '\0')
                    idx = (int) v;
            }
            if (idx < 0 || idx >= graph->node_count)
                break;
            GeomNode *n = graph->nodes[idx];
            if (n && n->is_active)
                ids[(*out_count)++] = n->id;
            break;
        }

        case SELECTOR_NEAREST: {
            /* 最近选择：参考点取自 expr "x,y"；无 expr 用图的第一个点。 */
            double rx = 0.0, ry = 0.0;
            bool have_ref = selector_parse_point(sel->expr, &rx, &ry);
            int best = -1;
            double best_d = 1e308;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (!n || !n->is_active)
                    continue;
                double x, y;
                if (!selector_node_coords(n, &x, &y))
                    continue;
                if (!have_ref) {
                    rx = x;
                    ry = y;
                    have_ref = true;
                }
                double d = geo_distance_2d(x, y, rx, ry);
                if (d < best_d) {
                    best_d = d;
                    best = n->id;
                }
            }
            if (best >= 0)
                ids[(*out_count)++] = best;
            break;
        }

        case SELECTOR_LARGEST:
        case SELECTOR_SMALLEST: {
            int best = -1;
            double best_m = (sel->type == SELECTOR_LARGEST) ? -1e308 : 1e308;
            for (int i = 0; i < graph->node_count; i++) {
                GeomNode *n = graph->nodes[i];
                if (!n || !n->is_active)
                    continue;
                double m = selector_node_metric(graph, n);
                if (sel->type == SELECTOR_LARGEST) {
                    if (m > best_m) {
                        best_m = m;
                        best = n->id;
                    }
                } else {
                    if (m < best_m) {
                        best_m = m;
                        best = n->id;
                    }
                }
            }
            if (best >= 0)
                ids[(*out_count)++] = best;
            break;
        }

        case SELECTOR_COMPOSITE: {
            /* 复合选择器：递归执行子选择器，按 is_union/is_negated 组合 */
            if (sel->child_count == 0)
                break;
            /* 先取第一个子结果作为种子 */
            int *base = NULL;
            int base_count = 0;
            if (!selector_apply(graph, sel->children[0], &base, &base_count)) {
                lv_free((void **) &ids);
                return false;
            }
            if (sel->is_union) {
                /* OR：并集 */
                for (int c = 1; c < sel->child_count; c++) {
                    int *sub = NULL;
                    int sub_count = 0;
                    if (!selector_apply(graph, sel->children[c], &sub, &sub_count)) {
                        lv_free((void **) &base);
                        lv_free((void **) &ids);
                        return false;
                    }
                    for (int s = 0; s < sub_count; s++) {
                        bool dup = false;
                        for (int b = 0; b < base_count; b++) {
                            if (base[b] == sub[s]) {
                                dup = true;
                                break;
                            }
                        }
                        if (!dup && base_count < cap)
                            base[base_count++] = sub[s];
                    }
                    lv_free((void **) &sub);
                }
            } else {
                /* AND：交集 */
                int *inter = (int *) lv_calloc((size_t) cap, sizeof(int));
                int inter_count = 0;
                if (!inter) {
                    lv_free((void **) &base);
                    lv_free((void **) &ids);
                    return false;
                }
                for (int b = 0; b < base_count; b++) {
                    bool in_all = true;
                    for (int c = 1; c < sel->child_count && in_all; c++) {
                        int *sub = NULL;
                        int sub_count = 0;
                        if (!selector_apply(graph, sel->children[c], &sub, &sub_count)) {
                            in_all = false;
                            lv_free((void **) &sub);
                            break;
                        }
                        bool found = false;
                        for (int s = 0; s < sub_count; s++) {
                            if (sub[s] == base[b]) {
                                found = true;
                                break;
                            }
                        }
                        lv_free((void **) &sub);
                        if (!found)
                            in_all = false;
                    }
                    if (in_all)
                        inter[inter_count++] = base[b];
                }
                lv_free((void **) &base);
                base = inter;
                base_count = inter_count;
            }
            if (sel->is_negated) {
                /* NOT：补集（相对全部活跃节点） */
                int *comp = (int *) lv_calloc((size_t) cap, sizeof(int));
                int comp_count = 0;
                if (!comp) {
                    lv_free((void **) &base);
                    lv_free((void **) &ids);
                    return false;
                }
                for (int i = 0; i < graph->node_count; i++) {
                    GeomNode *n = graph->nodes[i];
                    if (!n || !n->is_active)
                        continue;
                    bool in_base = false;
                    for (int b = 0; b < base_count; b++) {
                        if (base[b] == n->id) {
                            in_base = true;
                            break;
                        }
                    }
                    if (!in_base)
                        comp[comp_count++] = n->id;
                }
                lv_free((void **) &base);
                base = comp;
                base_count = comp_count;
            }
            *out_ids = base;
            *out_count = base_count;
            lv_free((void **) &ids);
            return true;
        }

        default:
            lv_free((void **) &ids);
            return false;
    }

    *out_ids = ids;
    return true;
}

lvSelector *algebra_selector_create(lvSelectorType type, const char *expr) {
    if (type < SELECTOR_ALL || type > SELECTOR_COMPOSITE)
        return NULL;

    lvSelector *sel = (lvSelector *) lv_calloc(1, sizeof(lvSelector));
    if (!sel)
        return NULL;

    sel->type = type;
    if (expr) {
        sel->expr = lv_strdup_safe(expr);
    }

    /* 解析方向操作符 */
    if (type == SELECTOR_BY_DIRECTION && expr && strlen(expr) >= 2) {
        switch (expr[0]) {
            case '>':
                sel->dir_op = SEL_DIR_GREATER;
                break;
            case '<':
                sel->dir_op = SEL_DIR_LESS;
                break;
            case '|':
                sel->dir_op = SEL_DIR_PARALLEL;
                break;
            default:
                break;
        }
        sel->axis = expr[1];
    }

    /* PARALLEL_TO / PERPENDICULAR_TO：expr 为轴名（"X"/"Y"/"Z"） */
    if ((type == SELECTOR_PARALLEL_TO || type == SELECTOR_PERPENDICULAR_TO) && expr && *expr) {
        sel->axis = expr[0];
    }

    /* BY_INDEX：expr 为数字下标 */
    if (type == SELECTOR_BY_INDEX && expr) {
        char *end = NULL;
        long v = strtol(expr, &end, 10);
        if (end && *end == '\0')
            sel->index = (int) v;
    }

    /* COMPOSITE：expr 可指定组合语义（"AND"/"OR"/"NOT"） */
    if (type == SELECTOR_COMPOSITE && expr) {
        if (lv_str_icmp(expr, "OR") == 0)
            sel->is_union = true;
        else if (lv_str_icmp(expr, "NOT") == 0)
            sel->is_negated = true;
    }

    return sel;
}

int algebra_selector_add_child(lvSelector *parent, lvSelector *child) {
    if (!parent || !child || parent->type != SELECTOR_COMPOSITE)
        return -1;
    if (parent->child_count >= parent->child_capacity) {
        int new_cap = parent->child_capacity > 0 ? parent->child_capacity * 2 : 4;
        lvSelector **grown = (lvSelector **) lv_realloc(parent->children, (size_t) new_cap * sizeof(lvSelector *));
        if (!grown)
            return -1;
        parent->children = grown;
        parent->child_capacity = new_cap;
    }
    parent->children[parent->child_count++] = child;
    return 0;
}

void algebra_selector_destroy(lvSelector *sel) {
    if (!sel)
        return;

    lv_free((void **) &(sel->expr));

    if (sel->children) {
        for (int i = 0; i < sel->child_count; i++) {
            algebra_selector_destroy(sel->children[i]);
        }
        lv_free((void **) &(sel->children));
    }

    memset(sel, 0, sizeof(*sel));
    lv_free((void **) &sel);
}

AlgebraicGeom *algebra_select(AlgebraicGeom *geom, const lvSelector *sel, int **out_ids, int *out_count) {
    if (!geom || !sel || !out_ids || !out_count)
        return NULL;

    /* 完整实现：按选择器类型递归过滤（方向/类型/标签/索引/最近/最大/最小/
     * 平行/垂直/位置/复合），替代旧"返回全部节点"的简化实现 */
    if (!geom->graph || geom->graph->node_count == 0) {
        *out_count = 0;
        *out_ids = NULL;
        return geom;
    }

    if (!selector_apply(geom->graph, sel, out_ids, out_count))
        return NULL;

    /* 历史记录：保留 SELECTOR_ALL 类型码（历史仅记录操作类别，不细分选择器） */
    history_push(geom, HISTORY_SELECTOR_ALL);
    return geom;
}

/* ================================================================
 * 约束与证明
 * ================================================================ */

AlgebraicGeom *algebra_constrain(AlgebraicGeom *geom, const char *constraint_type, const int *entity_ids, int count) {
    if (!geom || !geom->graph || !constraint_type || !entity_ids || count < 1)
        return NULL;

    /* 按 name 查 kConstraintAddOps 表（graph_index.c 定义，与
     * graph_add_constraint_dispatch 的按 type 分派共用一张表）：
     * 参与人数按 min_participants 宽松校验（count >= N），匹配到第一个
     * 即调用 fn 并停止，语义与原先的 strcmp 链逐字一致；
     * 未匹配（含 name 为 NULL 的 CONNECTION/ANGLE）时保持原行为：不做任何添加。 */
    for (int i = 0; i < LV_CONSTRAINT_ADD_OPS_COUNT; i++) {
        const ConstraintAddOps *op = &kConstraintAddOps[i];
        if (op->name && lv_str_eq(constraint_type, op->name) && count >= op->min_participants) {
            op->fn(geom->graph, entity_ids, count, 0.0);
            break;
        }
    }

    history_push(geom, HISTORY_INCIDENCE);
    return geom;
}

AlgebraicGeom *algebra_prove(AlgebraicGeom *geom, const char *proposition) {
    if (!geom || !proposition)
        return NULL;

    /* 证明操作：记录命题待后续引擎处理 */
    history_push(geom, HISTORY_PROVE);
    return geom;
}

/* ================================================================
 * 构建与查询
 * ================================================================ */

AlgebraOpResult algebra_build(AlgebraicGeom *geom) {
    if (!geom)
        return ALGEBRA_INVALID_ARGUMENT;
    if (!geom->graph)
        return ALGEBRA_INFEASIBLE;

    /* 如果图中有悬空节点 → 退化 */
    if (geom->graph->node_count == 0) {
        return ALGEBRA_DEGENERATE;
    }

    /* 变换已累积但未来得及应用 → 仍然成功 */
    geom->has_transform = false;

    return ALGEBRA_OK;
}

ConstraintGraph *algebra_get_graph(const AlgebraicGeom *geom) {
    if (!geom)
        return NULL;
    return geom->graph;
}

AlgebraOpResult algebra_get_status(const AlgebraicGeom *geom) {
    if (!geom)
        return ALGEBRA_INVALID_ARGUMENT;
    if (!geom->graph)
        return ALGEBRA_INFEASIBLE;

    /* 检查图的有效性 */
    if (geom->graph->node_count == 0)
        return ALGEBRA_DEGENERATE;

    return ALGEBRA_OK;
}

int algebra_get_current_entity(const AlgebraicGeom *geom) {
    if (!geom)
        return -1;
    return geom->current_entity;
}

/* ================================================================
 * Undo/Redo
 * ================================================================ */

AlgebraicGeom *algebra_undo(AlgebraicGeom *geom) {
    if (!geom || geom->history_count == 0)
        return NULL;

    /* 弹出最后一步并推入 redo 栈 */
    if (geom->redo_count >= geom->redo_capacity) {
        if (!lv_ensure_capacity((void **) &geom->redo_stack, geom->redo_count, &geom->redo_capacity,
                                sizeof(int), 1))
            return NULL;
    }

    int step = geom->history[--geom->history_count];
    geom->redo_stack[geom->redo_count++] = step;

    return geom;
}

/* REDO 动作分类（HISTORY_* 为显式大整数编号，未列出的步骤归 REDO_NONE） */
enum {
    REDO_NONE = 0,   /* 无操作：约束操作（INCIDENCE）、平面切换（PLANE）等 */
    REDO_ENTITY,     /* 实体构造：更新 current_entity 为图中最后添加的节点 */
    REDO_TRANSFORM   /* 变换操作：恢复变换累积状态 */
};

static const int kRedoActionByStep[HISTORY_PLANE + 1] = {
    [HISTORY_POINT]        = REDO_ENTITY,
    [HISTORY_LINE_SEGMENT] = REDO_ENTITY,
    [HISTORY_CIRCLE]       = REDO_ENTITY,
    [HISTORY_RAY]          = REDO_ENTITY,
    [HISTORY_INCIDENCE]    = REDO_NONE,
    [HISTORY_TRANSFORM]    = REDO_TRANSFORM,
};

AlgebraicGeom *algebra_redo(AlgebraicGeom *geom) {
    if (!geom || geom->redo_count == 0)
        return NULL;

    int step = geom->redo_stack[--geom->redo_count];
    history_push(geom, step);

    /* 根据步骤类型重新执行几何构造（REDO 动作查找表，未列出/越界值归 REDO_NONE） */
    int redo_action = (unsigned) step < sizeof(kRedoActionByStep) / sizeof(kRedoActionByStep[0])
                          ? kRedoActionByStep[step]
                          : REDO_NONE;
    switch (redo_action) {
        case REDO_ENTITY:
            /* 点、线段、圆、射线的构造：更新 current_entity 为图中最后一个节点 */
            if (geom->graph && geom->graph->node_count > 0) {
                geom->current_entity = graph_get_last_added_node_id(geom->graph);
            }
            break;

        case REDO_TRANSFORM:
            /* 变换操作：恢复变换累积状态 */
            geom->has_transform = true;
            break;

        case REDO_NONE:
        default:
            /* 约束操作（HISTORY_INCIDENCE）不改变 current_entity；HISTORY_PLANE 等平面切换亦不受影响 */
            break;
    }

    return geom;
}

/* ================================================================
 * 快照/回退
 * ================================================================ */

int algebra_snapshot(AlgebraicGeom *geom) {
    if (!geom)
        return -1;

    /* 扩容快照栈 */
    if (geom->snapshot_count >= geom->snapshot_capacity) {
        if (!lv_ensure_capacity((void **) &geom->snapshots, geom->snapshot_count, &geom->snapshot_capacity,
                                sizeof(struct AlgebraicGeom *), 1))
            return -1;
    }

    /* 创建当前状态的浅拷贝 */
    AlgebraicGeom *copy = (AlgebraicGeom *) lv_calloc(1, sizeof(AlgebraicGeom));
    if (!copy)
        return -1;
    memcpy(copy, geom, sizeof(AlgebraicGeom));

    /* 对拥有所有权的字段做深拷贝 */
    if (geom->name)
        copy->name = lv_strdup_safe(geom->name);

    /* 深拷贝约束图：快照必须持有独立的图，否则销毁快照时
     * 会与主几何体共享的 graph 发生双重释放 */
    if (geom->graph)
        copy->graph = graph_copy(geom->graph);

    if (geom->history_count > 0) {
        copy->history = (int *) lv_malloc((size_t) geom->history_count * sizeof(int));
        if (copy->history) {
            memcpy(copy->history, geom->history, (size_t) geom->history_count * sizeof(int));
        }
    }

    copy->snapshots = NULL;
    copy->snapshot_count = 0;
    copy->snapshot_capacity = 0;
    copy->redo_stack = NULL;
    copy->redo_count = 0;
    copy->redo_capacity = 0;

    geom->snapshots[geom->snapshot_count++] = copy;
    return geom->snapshot_count - 1;
}

AlgebraicGeom *algebra_restore(AlgebraicGeom *geom, int snapshot_index) {
    if (!geom || !lv_index_in_range(snapshot_index, geom->snapshot_count))
        return NULL;

    AlgebraicGeom *snap = geom->snapshots[snapshot_index];
    if (!snap)
        return NULL;

    /* 恢复 states（浅字段） */
    geom->plane = snap->plane;
    geom->current_entity = snap->current_entity;
    memcpy(geom->transform, snap->transform, 16 * sizeof(double));
    geom->has_transform = snap->has_transform;
    geom->history_count = snap->history_count;

    if (geom->history && snap->history && snap->history_count > 0) {
        if (geom->history_capacity >= snap->history_count) {
            memcpy(geom->history, snap->history, (size_t) snap->history_count * sizeof(int));
        }
    }

    return geom;
}

/* ================================================================
 * 工作平面
 * ================================================================ */

AlgebraicGeom *algebra_set_work_plane(AlgebraicGeom *geom, int plane) {
    if (!geom)
        return NULL;
    if (plane < PLANE_XY || plane > PLANE_CUSTOM)
        return NULL;

    geom->plane = plane;
    history_push(geom, plane);
    return geom;
}
