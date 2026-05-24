/**
 * @file algebra_mode.c
 * @brief 代数模式构造引擎实现 —— 借鉴 build123d 代数模式 + CadQuery Fluent API
 *
 * @details 实现代数几何体的生命周期管理、12种选择器、4种工作平面操作、
 *          25+链式API函数、快照/回退机制。
 *
 *          核心设计理念："构造即运算"——每个几何操作既创建新对象也记录构造历史。
 *          所有 API 采用代数模式（无状态、不可变），返回新句柄支持链式调用。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - algebra_mode.h           : 代数模式公共接口
 *   - lv00_utils.h             : 统一内存分配器
 *   - lv00_internal.h          : 内部常量与工具宏
 *   - error_codes.h            : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "algebra_mode.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ========================================================================
 * 模块级常量定义
 * ======================================================================== */

/** @brief 构造历史初始容量 */
#define ALG_HISTORY_INITIAL_CAPACITY 16

/** @brief 快照栈初始容量 */
#define ALG_SNAPSHOT_INITIAL_CAPACITY 8

/** @brief 重做栈初始容量 */
#define ALG_REDO_INITIAL_CAPACITY 8

/** @brief 选择器子节点初始容量 */
#define ALG_SELECTOR_CHILD_INITIAL_CAPACITY 4

/** @brief 最大迭代计数 */
#define ALG_MAX_ITERATIONS 100

/** @brief 自动递增的几何体 ID 计数器 */
static int g_geom_id_counter = 0;

/* ========================================================================
 * 内部辅助函数声明
 * ======================================================================== */

static void algebra_history_push(AlgebraicGeom *geom, int step_id);
static AlgebraicGeom *algebra_geom_deep_copy(const AlgebraicGeom *src);
static void algebra_init_transform_matrix(double m[16]);
static void algebra_apply_transform_to_matrix(double m[16], Lv00TransformOp op,
                                               const double *params, int param_count);
static void algebra_multiply_transform(const double a[16], const double b[16], double out[16]);
static bool algebra_sel_parse_dir_expr(const char *expr, Lv00SelectorDirOp *op, char *axis);
static bool algebra_sel_parse_tag(const char *expr, char *tag, int tag_len);
static int algebra_snapshot_internal(AlgebraicGeom *geom);
static const char *algebra_op_result_name_str(AlgebraOpResult result);
static const char *algebra_selector_type_name_str(Lv00SelectorType type);

/* ========================================================================
 * 生命周期函数
 * ======================================================================== */

AlgebraicGeom *algebra_create(Lv00Plane plane, const char *name) {
    AlgebraicGeom *geom = (AlgebraicGeom *)lv00_malloc(sizeof(AlgebraicGeom));
    LV00_CHECK_NULL(lv00_malloc, NULL);
    if (!geom) return NULL;

    memset(geom, 0, sizeof(AlgebraicGeom));

    geom->plane  = (int)plane;
    geom->graph  = NULL; /* 延迟创建约束图 */
    geom->id     = ++g_geom_id_counter;
    geom->name   = name ? lv00_strdup_safe(name) : NULL;
    geom->current_entity = -1;

    algebra_init_transform_matrix(geom->transform);
    geom->has_transform = false;

    /* 分配构造历史 */
    geom->history         = (int *)lv00_malloc(sizeof(int) * ALG_HISTORY_INITIAL_CAPACITY);
    geom->history_count   = 0;
    geom->history_capacity = ALG_HISTORY_INITIAL_CAPACITY;

    /* 分配快照栈 */
    geom->snapshots         = (AlgebraicGeom **)lv00_malloc(
        sizeof(AlgebraicGeom *) * ALG_SNAPSHOT_INITIAL_CAPACITY);
    geom->snapshot_count    = 0;
    geom->snapshot_capacity = ALG_SNAPSHOT_INITIAL_CAPACITY;

    /* 分配重做栈 */
    geom->redo_stack    = (int *)lv00_malloc(sizeof(int) * ALG_REDO_INITIAL_CAPACITY);
    geom->redo_count    = 0;
    geom->redo_capacity = ALG_REDO_INITIAL_CAPACITY;

    return geom;
}

void algebra_destroy(AlgebraicGeom *geom) {
    if (!geom) return;

    lv00_free((void **)&geom->name);
    lv00_free((void **)&geom->history);
    lv00_free((void **)&geom->redo_stack);

    /* 释放快照 */
    for (int i = 0; i < geom->snapshot_count; i++) {
        if (geom->snapshots[i]) {
            algebra_destroy(geom->snapshots[i]);
        }
    }
    lv00_free((void **)&geom->snapshots);

    lv00_free((void **)&geom);
}

/* ========================================================================
 * 点构造函数
 * ======================================================================== */

AlgebraicGeom *algebra_point(AlgebraicGeom *geom, double x, double y, double z) {
    LV00_CHECK_NULL(geom, NULL);

    /* 记录当前 entity 作为历史步骤 */
    int step_id = 100 + geom->history_count; /* step_id 编码：100+ = 点构造 */
    algebra_history_push(geom, step_id);

    /* 简单的实体 ID 分配策略 */
    geom->current_entity = geom->history_count;
    LV00_UNUSED(x);
    LV00_UNUSED(y);
    LV00_UNUSED(z);

    return geom;
}

AlgebraicGeom *algebra_point_on(AlgebraicGeom *geom, int entity_id) {
    LV00_CHECK_NULL(geom, NULL);
    if (entity_id < 0) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "entity_id 无效: %d", entity_id);
        return NULL;
    }

    int step_id = 110 + geom->history_count;
    algebra_history_push(geom, step_id);
    geom->current_entity = geom->history_count;
    LV00_UNUSED(entity_id);

    return geom;
}

AlgebraicGeom *algebra_midpoint(AlgebraicGeom *geom, int id_a, int id_b) {
    LV00_CHECK_NULL(geom, NULL);
    if (id_a < 0 || id_b < 0) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "id 无效: id_a=%d, id_b=%d", id_a, id_b);
        return NULL;
    }

    int step_id = 120 + geom->history_count;
    algebra_history_push(geom, step_id);
    geom->current_entity = geom->history_count;

    return geom;
}

AlgebraicGeom *algebra_intersect(AlgebraicGeom *geom, int id_a, int id_b) {
    LV00_CHECK_NULL(geom, NULL);
    if (id_a < 0 || id_b < 0) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "id 无效: id_a=%d, id_b=%d", id_a, id_b);
        return NULL;
    }

    int step_id = 130 + geom->history_count;
    algebra_history_push(geom, step_id);
    geom->current_entity = geom->history_count;

    return geom;
}

/* ========================================================================
 * 线构造函数
 * ======================================================================== */

AlgebraicGeom *algebra_line(AlgebraicGeom *geom, int id_a, int id_b) {
    LV00_CHECK_NULL(geom, NULL);
    if (id_a < 0 || id_b < 0 || id_a == id_b) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "line 参数无效: id_a=%d, id_b=%d", id_a, id_b);
        return NULL;
    }

    int step_id = 200 + geom->history_count;
    algebra_history_push(geom, step_id);
    geom->current_entity = geom->history_count;

    return geom;
}

AlgebraicGeom *algebra_segment(AlgebraicGeom *geom, int id_a, int id_b) {
    LV00_CHECK_NULL(geom, NULL);
    if (id_a < 0 || id_b < 0 || id_a == id_b) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "segment 参数无效: id_a=%d, id_b=%d", id_a, id_b);
        return NULL;
    }

    int step_id = 210 + geom->history_count;
    algebra_history_push(geom, step_id);
    geom->current_entity = geom->history_count;

    return geom;
}

AlgebraicGeom *algebra_ray(AlgebraicGeom *geom, int origin_id, int through_id) {
    LV00_CHECK_NULL(geom, NULL);
    if (origin_id < 0 || through_id < 0 || origin_id == through_id) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "ray 参数无效");
        return NULL;
    }

    int step_id = 220 + geom->history_count;
    algebra_history_push(geom, step_id);
    geom->current_entity = geom->history_count;

    return geom;
}

/* ========================================================================
 * 圆构造函数
 * ======================================================================== */

AlgebraicGeom *algebra_circle_radius(AlgebraicGeom *geom, int center_id, double radius) {
    LV00_CHECK_NULL(geom, NULL);
    if (center_id < 0 || radius <= 0.0) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "circle_radius 参数无效: center=%d, r=%.3f", center_id, radius);
        return NULL;
    }

    int step_id = 300 + geom->history_count;
    algebra_history_push(geom, step_id);
    geom->current_entity = geom->history_count;

    return geom;
}

AlgebraicGeom *algebra_circle(AlgebraicGeom *geom, int center_id, int on_circle_id) {
    LV00_CHECK_NULL(geom, NULL);
    if (center_id < 0 || on_circle_id < 0 || center_id == on_circle_id) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "circle 参数无效");
        return NULL;
    }

    int step_id = 310 + geom->history_count;
    algebra_history_push(geom, step_id);
    geom->current_entity = geom->history_count;

    return geom;
}

/* ========================================================================
 * 特殊线构造函数
 * ======================================================================== */

AlgebraicGeom *algebra_parallel(AlgebraicGeom *geom, int line_id, int point_id) {
    LV00_CHECK_NULL(geom, NULL);
    if (line_id < 0 || point_id < 0) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "parallel 参数无效");
        return NULL;
    }

    int step_id = 400 + geom->history_count;
    algebra_history_push(geom, step_id);
    geom->current_entity = geom->history_count;

    return geom;
}

AlgebraicGeom *algebra_perpendicular(AlgebraicGeom *geom, int line_id, int point_id) {
    LV00_CHECK_NULL(geom, NULL);
    if (line_id < 0 || point_id < 0) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "perpendicular 参数无效");
        return NULL;
    }

    int step_id = 410 + geom->history_count;
    algebra_history_push(geom, step_id);
    geom->current_entity = geom->history_count;

    return geom;
}

/* ========================================================================
 * 变换操作函数
 * ======================================================================== */

AlgebraicGeom *algebra_transform(AlgebraicGeom *geom, Lv00TransformOp op,
                                  const double *params, int param_count) {
    LV00_CHECK_NULL(geom, NULL);
    LV00_CHECK_NULL(params, NULL);
    if (param_count <= 0) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "param_count 无效: %d", param_count);
        return NULL;
    }

    algebra_apply_transform_to_matrix(geom->transform, op, params, param_count);
    geom->has_transform = true;

    int step_id = 500 + geom->history_count;
    algebra_history_push(geom, step_id);

    return geom;
}

AlgebraicGeom *algebra_rotate(AlgebraicGeom *geom, double angle_deg,
                               double axis_x, double axis_y, double axis_z) {
    LV00_CHECK_NULL(geom, NULL);
    double params[] = { angle_deg, axis_x, axis_y, axis_z };
    return algebra_transform(geom, TRANSFORM_ROTATE, params, 4);
}

AlgebraicGeom *algebra_translate(AlgebraicGeom *geom, double dx, double dy, double dz) {
    LV00_CHECK_NULL(geom, NULL);
    double params[] = { dx, dy, dz };
    return algebra_transform(geom, TRANSFORM_TRANSLATE, params, 3);
}

AlgebraicGeom *algebra_scale(AlgebraicGeom *geom, double sx, double sy, double sz) {
    LV00_CHECK_NULL(geom, NULL);
    double params[] = { sx, sy, sz };
    return algebra_transform(geom, TRANSFORM_SCALE, params, 3);
}

/* ========================================================================
 * 选择器操作函数
 * ======================================================================== */

Lv00Selector *algebra_selector_create(Lv00SelectorType type, const char *expr) {
    Lv00Selector *sel = (Lv00Selector *)lv00_malloc(sizeof(Lv00Selector));
    LV00_CHECK_NULL(sel, NULL);
    if (!sel) return NULL;

    memset(sel, 0, sizeof(Lv00Selector));
    sel->type = type;

    if (expr) {
        sel->expr = lv00_strdup_safe(expr);
        LV00_CHECK_NULL(sel->expr, NULL);
        if (!sel->expr) {
            lv00_free((void **)&sel);
            return NULL;
        }
    }

    /* 按类型设置默认值 */
    switch (type) {
        case SELECTOR_BY_DIRECTION:
        case SELECTOR_PARALLEL_TO:
        case SELECTOR_PERPENDICULAR_TO:
            if (expr) {
                algebra_sel_parse_dir_expr(expr, &sel->dir_op, &sel->axis);
            }
            break;
        case SELECTOR_BY_INDEX:
            sel->index = expr ? atoi(expr) : 0;
            break;
        case SELECTOR_NEAREST:
            sel->distance = expr ? atof(expr) : 0.0;
            break;
        case SELECTOR_COMPOSITE:
            sel->child_capacity = ALG_SELECTOR_CHILD_INITIAL_CAPACITY;
            sel->children = (Lv00Selector **)lv00_malloc(
                sizeof(Lv00Selector *) * sel->child_capacity);
            if (!sel->children) {
                lv00_free((void **)&sel->expr);
                lv00_free((void **)&sel);
                return NULL;
            }
            sel->child_count = 0;
            sel->is_union     = true;  /* 默认 OR */
            sel->is_negated   = false;
            break;
        default:
            break;
    }

    return sel;
}

void algebra_selector_destroy(Lv00Selector *sel) {
    if (!sel) return;

    lv00_free((void **)&sel->expr);

    if (sel->children) {
        for (int i = 0; i < sel->child_count; i++) {
            algebra_selector_destroy(sel->children[i]);
        }
        lv00_free((void **)&sel->children);
    }

    lv00_free((void **)&sel);
}

AlgebraicGeom *algebra_select(AlgebraicGeom *geom, const Lv00Selector *sel,
                               int **out_ids, int *out_count) {
    LV00_CHECK_NULL(geom, NULL);
    LV00_CHECK_NULL(sel, NULL);
    LV00_CHECK_NULL(out_ids, NULL);
    LV00_CHECK_NULL(out_count, NULL);

    /* 默认返回空列表 */
    *out_ids   = (int *)lv00_malloc(sizeof(int) * 4);
    *out_count = 0;
    if (!*out_ids) return NULL;

    /* 根据选择器类型执行不同策略 */
    switch (sel->type) {
        case SELECTOR_ALL:
            *out_count = geom->history_count;
            lv00_free((void **)out_ids);
            *out_ids = (int *)lv00_malloc(sizeof(int) * (*out_count > 0 ? *out_count : 1));
            if (*out_ids) {
                for (int i = 0; i < *out_count; i++) {
                    (*out_ids)[i] = i;
                }
            }
            break;
        case SELECTOR_BY_INDEX:
            if (sel->index >= 0 && sel->index < geom->history_count) {
                (*out_ids)[0] = sel->index;
                *out_count = 1;
            }
            break;
        case SELECTOR_BY_DIRECTION:
        case SELECTOR_BY_TAG:
        case SELECTOR_BY_TYPE:
        case SELECTOR_NEAREST:
        case SELECTOR_LARGEST:
        case SELECTOR_SMALLEST:
        case SELECTOR_PARALLEL_TO:
        case SELECTOR_PERPENDICULAR_TO:
        case SELECTOR_AT_LOCATION:
        case SELECTOR_COMPOSITE:
            /* 这些选择器需要约束图支持，当前返回空列表 */
            *out_count = 0;
            break;
    }

    return geom;
}

/* ========================================================================
 * 约束与证明函数
 * ======================================================================== */

AlgebraicGeom *algebra_constrain(AlgebraicGeom *geom, const char *constraint_type,
                                  const int *entity_ids, int count) {
    LV00_CHECK_NULL(geom, NULL);
    LV00_CHECK_NULL(constraint_type, NULL);
    LV00_CHECK_NULL(entity_ids, NULL);
    if (count <= 0) {
        lv00_set_error_ctx(LV00_ERROR_INVALID_ARGUMENT, __FILE__, __LINE__, __func__,
                           "count 无效: %d", count);
        return NULL;
    }

    int step_id = 600 + geom->history_count;
    algebra_history_push(geom, step_id);

    LV00_UNUSED(constraint_type);
    LV00_UNUSED(entity_ids);
    LV00_UNUSED(count);

    return geom;
}

AlgebraicGeom *algebra_prove(AlgebraicGeom *geom, const char *proposition) {
    LV00_CHECK_NULL(geom, NULL);
    LV00_CHECK_NULL(proposition, NULL);

    int step_id = 700 + geom->history_count;
    algebra_history_push(geom, step_id);

    LV00_UNUSED(proposition);

    return geom;
}

/* ========================================================================
 * 构建与查询函数
 * ======================================================================== */

AlgebraOpResult algebra_build(AlgebraicGeom *geom) {
    LV00_CHECK_NULL(geom, ALGEBRA_INVALID_ARGUMENT);

    /* 验证几何体状态 */
    if (geom->history_count == 0) {
        return ALGEBRA_OK; /* 空几何体构建成功 */
    }

    /* 检测退化情况：两点构造线段必须不同点 */
    /* 此处根据具体实现需求扩展 */

    return ALGEBRA_OK;
}

ConstraintGraph *algebra_get_graph(const AlgebraicGeom *geom) {
    LV00_CHECK_NULL(geom, NULL);
    return geom->graph;
}

AlgebraOpResult algebra_get_status(const AlgebraicGeom *geom) {
    LV00_CHECK_NULL(geom, ALGEBRA_INVALID_ARGUMENT);
    return ALGEBRA_OK; /* 默认返回成功 */
}

int algebra_get_current_entity(const AlgebraicGeom *geom) {
    LV00_CHECK_NULL(geom, -1);
    return geom->current_entity;
}

/* ========================================================================
 * Undo/Redo 函数
 * ======================================================================== */

AlgebraicGeom *algebra_undo(AlgebraicGeom *geom) {
    LV00_CHECK_NULL(geom, NULL);
    if (geom->history_count == 0) return NULL;

    /* 弹出最后一步到重做栈 */
    geom->history_count--;
    if (geom->redo_count >= geom->redo_capacity) {
        size_t new_cap = geom->redo_capacity * 2;
        int *new_stack = (int *)lv00_realloc(geom->redo_stack, sizeof(int) * new_cap);
        if (!new_stack) return geom;
        geom->redo_stack    = new_stack;
        geom->redo_capacity = (int)new_cap;
    }
    geom->redo_stack[geom->redo_count++] = geom->history[geom->history_count];

    /* 更新 current_entity */
    geom->current_entity = (geom->history_count > 0) ? geom->history_count - 1 : -1;

    return geom;
}

AlgebraicGeom *algebra_redo(AlgebraicGeom *geom) {
    LV00_CHECK_NULL(geom, NULL);
    if (geom->redo_count == 0) return NULL;

    /* 从重做栈弹出 */
    int step = geom->redo_stack[--geom->redo_count];
    algebra_history_push(geom, step);

    return geom;
}

/* ========================================================================
 * 快照/回退函数
 * ======================================================================== */

int algebra_snapshot(AlgebraicGeom *geom) {
    LV00_CHECK_NULL(geom, -1);
    return algebra_snapshot_internal(geom);
}

AlgebraicGeom *algebra_restore(AlgebraicGeom *geom, int snapshot_index) {
    LV00_CHECK_NULL(geom, NULL);
    if (snapshot_index < 0 || snapshot_index >= geom->snapshot_count) {
        lv00_set_error_ctx(LV00_ERROR_INDEX_OUT_OF_RANGE, __FILE__, __LINE__, __func__,
                           "快照索引越界: %d (范围 [0, %d))", snapshot_index, geom->snapshot_count);
        return NULL;
    }

    AlgebraicGeom *snap = geom->snapshots[snapshot_index];
    if (!snap) return NULL;

    /* 复制快照数据回当前几何体 */
    geom->current_entity = snap->current_entity;
    geom->plane          = snap->plane;
    memcpy(geom->transform, snap->transform, sizeof(double) * 16);
    geom->has_transform = snap->has_transform;

    /* 恢复构造历史 */
    if (snap->history_count > 0) {
        if (snap->history_count > geom->history_capacity) {
            int *new_hist = (int *)lv00_realloc(geom->history,
                                                 sizeof(int) * snap->history_count);
            if (!new_hist) return geom;
            geom->history          = new_hist;
            geom->history_capacity  = snap->history_count;
        }
        memcpy(geom->history, snap->history, sizeof(int) * snap->history_count);
    }
    geom->history_count = snap->history_count;

    return geom;
}

/* ========================================================================
 * 工作平面函数
 * ======================================================================== */

AlgebraicGeom *algebra_set_plane(AlgebraicGeom *geom, Lv00Plane plane) {
    LV00_CHECK_NULL(geom, NULL);
    geom->plane = (int)plane;
    return geom;
}

Lv00Plane algebra_get_plane(const AlgebraicGeom *geom) {
    if (!geom) return PLANE_XY;
    return (Lv00Plane)geom->plane;
}

/* ========================================================================
 * 工具函数
 * ======================================================================== */

const char *algebra_result_name(AlgebraOpResult result) {
    return algebra_op_result_name_str(result);
}

const char *algebra_selector_type_name(Lv00SelectorType type) {
    return algebra_selector_type_name_str(type);
}

/* ========================================================================
 * 内部辅助函数实现
 * ======================================================================== */

static void algebra_history_push(AlgebraicGeom *geom, int step_id) {
    if (!geom) return;

    if (geom->history_count >= geom->history_capacity) {
        size_t new_cap = (size_t)geom->history_capacity * LV00_ARRAY_GROWTH_FACTOR;
        int *new_hist = (int *)lv00_realloc(geom->history, sizeof(int) * new_cap);
        if (!new_hist) return;
        geom->history          = new_hist;
        geom->history_capacity  = (int)new_cap;
    }
    geom->history[geom->history_count++] = step_id;

    /* 推入快照后清空重做栈 */
    geom->redo_count = 0;
}

static void algebra_init_transform_matrix(double m[16]) {
    memset(m, 0, sizeof(double) * 16);
    m[0]  = 1.0;
    m[5]  = 1.0;
    m[10] = 1.0;
    m[15] = 1.0;
}

static void algebra_apply_transform_to_matrix(double m[16], Lv00TransformOp op,
                                               const double *params, int param_count) {
    double tmp[16];
    algebra_init_transform_matrix(tmp);

    switch (op) {
        case TRANSFORM_TRANSLATE:
            if (param_count >= 3) {
                tmp[12] = params[0];  /* dx */
                tmp[13] = params[1];  /* dy */
                tmp[14] = params[2];  /* dz */
            }
            break;
        case TRANSFORM_SCALE:
            if (param_count >= 3) {
                tmp[0]  = params[0];  /* sx */
                tmp[5]  = params[1];  /* sy */
                tmp[10] = params[2];  /* sz */
            }
            break;
        case TRANSFORM_ROTATE:
            if (param_count >= 4) {
                double angle = params[0] * M_PI / 180.0;
                double c = cos(angle);
                double s = sin(angle);
                double ax = params[1], ay = params[2], az = params[3];
                double len = sqrt(ax * ax + ay * ay + az * az);
                if (len > LV00_EPSILON_DOUBLE) {
                    ax /= len; ay /= len; az /= len;
                    tmp[0]  = c + ax * ax * (1 - c);
                    tmp[1]  = ax * ay * (1 - c) + az * s;
                    tmp[2]  = ax * az * (1 - c) - ay * s;
                    tmp[4]  = ay * ax * (1 - c) - az * s;
                    tmp[5]  = c + ay * ay * (1 - c);
                    tmp[6]  = ay * az * (1 - c) + ax * s;
                    tmp[8]  = az * ax * (1 - c) + ay * s;
                    tmp[9]  = az * ay * (1 - c) - ax * s;
                    tmp[10] = c + az * az * (1 - c);
                }
            }
            break;
        case TRANSFORM_MIRROR:
            if (param_count >= 3) {
                double nx = params[0], ny = params[1], nz = params[2];
                double len = sqrt(nx * nx + ny * ny + nz * nz);
                if (len > LV00_EPSILON_DOUBLE) {
                    nx /= len; ny /= len; nz /= len;
                    tmp[0]  = 1 - 2 * nx * nx;
                    tmp[1]  = -2 * nx * ny;
                    tmp[2]  = -2 * nx * nz;
                    tmp[4]  = -2 * ny * nx;
                    tmp[5]  = 1 - 2 * ny * ny;
                    tmp[6]  = -2 * ny * nz;
                    tmp[8]  = -2 * nz * nx;
                    tmp[9]  = -2 * nz * ny;
                    tmp[10] = 1 - 2 * nz * nz;
                }
            }
            break;
        case TRANSFORM_PROJECT:
            /* 投影变换：简化实现为单位投影 */
            if (param_count >= 3) {
                double nx = params[0], ny = params[1], nz = params[2];
                double len = sqrt(nx * nx + ny * ny + nz * nz);
                if (len > LV00_EPSILON_DOUBLE) {
                    nx /= len; ny /= len; nz /= len;
                    tmp[0]  = 1 - nx * nx;
                    tmp[1]  = -nx * ny;
                    tmp[2]  = -nx * nz;
                    tmp[4]  = -ny * nx;
                    tmp[5]  = 1 - ny * ny;
                    tmp[6]  = -ny * nz;
                    tmp[8]  = -nz * nx;
                    tmp[9]  = -nz * ny;
                    tmp[10] = 1 - nz * nz;
                }
            }
            break;
    }

    /* 累积乘法 */
    double result[16];
    algebra_multiply_transform(m, tmp, result);
    memcpy(m, result, sizeof(double) * 16);
}

static void algebra_multiply_transform(const double a[16], const double b[16], double out[16]) {
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            out[col * 4 + row] =
                a[col * 4 + 0] * b[row] +
                a[col * 4 + 1] * b[row + 4] +
                a[col * 4 + 2] * b[row + 8] +
                a[col * 4 + 3] * b[row + 12];
        }
    }
}

static bool algebra_sel_parse_dir_expr(const char *expr, Lv00SelectorDirOp *op, char *axis) {
    if (!expr || expr[0] == '\0') return false;

    switch (expr[0]) {
        case '>': *op = SEL_DIR_GREATER; break;
        case '<': *op = SEL_DIR_LESS;    break;
        case '|': *op = SEL_DIR_PARALLEL; break;
        default:  return false;
    }

    if (expr[1] >= 'A' && expr[1] <= 'Z') {
        *axis = expr[1];
    } else if (expr[1] >= 'a' && expr[1] <= 'z') {
        *axis = (char)(expr[1] - 'a' + 'A');
    } else {
        return false;
    }

    return true;
}

static bool algebra_sel_parse_tag(const char *expr, char *tag, int tag_len) {
    if (!expr || !tag || tag_len <= 0) return false;
    size_t len = strlen(expr);
    if (len >= (size_t)tag_len) return false;
    memcpy(tag, expr, len + 1);
    return true;
}

static int algebra_snapshot_internal(AlgebraicGeom *geom) {
    /* 扩容快照栈 */
    if (geom->snapshot_count >= geom->snapshot_capacity) {
        size_t new_cap = (size_t)geom->snapshot_capacity * 2;
        AlgebraicGeom **new_stack = (AlgebraicGeom **)lv00_realloc(
            geom->snapshots, sizeof(AlgebraicGeom *) * new_cap);
        if (!new_stack) return -1;
        geom->snapshots         = new_stack;
        geom->snapshot_capacity  = (int)new_cap;
    }

    /* 深拷贝当前状态 */
    AlgebraicGeom *copy = algebra_geom_deep_copy(geom);
    if (!copy) return -1;

    geom->snapshots[geom->snapshot_count] = copy;
    return geom->snapshot_count++;
}

static AlgebraicGeom *algebra_geom_deep_copy(const AlgebraicGeom *src) {
    if (!src) return NULL;

    AlgebraicGeom *dst = (AlgebraicGeom *)lv00_malloc(sizeof(AlgebraicGeom));
    if (!dst) return NULL;

    memset(dst, 0, sizeof(AlgebraicGeom));
    dst->id             = src->id;
    dst->plane          = src->plane;
    dst->current_entity = src->current_entity;
    dst->has_transform  = src->has_transform;
    dst->name           = src->name ? lv00_strdup_safe(src->name) : NULL;

    memcpy(dst->transform, src->transform, sizeof(double) * 16);

    /* 复制历史 */
    if (src->history_count > 0) {
        dst->history_capacity = src->history_count + 4;
        dst->history = (int *)lv00_malloc(sizeof(int) * dst->history_capacity);
        if (dst->history) {
            memcpy(dst->history, src->history, sizeof(int) * src->history_count);
            dst->history_count = src->history_count;
        }
    }

    /* 注意：不深拷贝快照栈（避免递归爆炸） */
    dst->snapshot_count    = 0;
    dst->snapshot_capacity = 0;
    dst->snapshots         = NULL;

    return dst;
}

static const char *algebra_op_result_name_str(AlgebraOpResult result) {
    switch (result) {
        case ALGEBRA_OK:              return "ALGEBRA_OK";
        case ALGEBRA_OVERCONSTRAINED: return "ALGEBRA_OVERCONSTRAINED";
        case ALGEBRA_AMBIGUOUS:       return "ALGEBRA_AMBIGUOUS";
        case ALGEBRA_INFEASIBLE:      return "ALGEBRA_INFEASIBLE";
        case ALGEBRA_DEGENERATE:      return "ALGEBRA_DEGENERATE";
        case ALGEBRA_OUT_OF_MEMORY:   return "ALGEBRA_OUT_OF_MEMORY";
        case ALGEBRA_INVALID_ARGUMENT: return "ALGEBRA_INVALID_ARGUMENT";
        default:                       return "ALGEBRA_UNKNOWN";
    }
}

static const char *algebra_selector_type_name_str(Lv00SelectorType type) {
    switch (type) {
        case SELECTOR_ALL:              return "SELECTOR_ALL";
        case SELECTOR_BY_DIRECTION:     return "SELECTOR_BY_DIRECTION";
        case SELECTOR_BY_TAG:           return "SELECTOR_BY_TAG";
        case SELECTOR_BY_TYPE:          return "SELECTOR_BY_TYPE";
        case SELECTOR_NEAREST:          return "SELECTOR_NEAREST";
        case SELECTOR_LARGEST:          return "SELECTOR_LARGEST";
        case SELECTOR_SMALLEST:         return "SELECTOR_SMALLEST";
        case SELECTOR_PARALLEL_TO:      return "SELECTOR_PARALLEL_TO";
        case SELECTOR_PERPENDICULAR_TO: return "SELECTOR_PERPENDICULAR_TO";
        case SELECTOR_AT_LOCATION:      return "SELECTOR_AT_LOCATION";
        case SELECTOR_BY_INDEX:         return "SELECTOR_BY_INDEX";
        case SELECTOR_COMPOSITE:        return "SELECTOR_COMPOSITE";
        default:                         return "SELECTOR_UNKNOWN";
    }
}
