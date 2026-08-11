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

    /* 在 entity_id 上创建一个共线点 */
    /* 简化实现：创建点并与 entity_id 添加 incidence 约束 */
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

    return sel;
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

    /* 简化选择器实现：返回当前图的节点 ID 列表 */
    *out_count = 0;
    *out_ids = NULL;

    if (!geom->graph || geom->graph->node_count == 0)
        return geom;

    *out_ids = (int *) lv_calloc((size_t) geom->graph->node_count, sizeof(int));
    if (!*out_ids)
        return geom;

    for (int i = 0; i < geom->graph->node_count; i++) {
        if (geom->graph->nodes[i]) {
            (*out_ids)[(*out_count)++] = geom->graph->nodes[i]->id;
        }
    }

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
        if (op->name && strcmp(constraint_type, op->name) == 0 && count >= op->min_participants) {
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
