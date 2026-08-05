/**
 * @file geometry_canvas.c
 * @brief 几何画布视图实现
 *
 * @details 实现几何画布视图，管理几何实体（点、线、圆、多边形）和约束可视化。
 *          支持实体的添加/删除、约束的添加、包围盒计算、视图适配以及 SVG 输出渲染。
 *          实体和约束使用动态数组管理，支持自动扩容。
 *
 * @author Lv-00 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_check.h"
#include "lv/lv_utils.h"
#include "lv/visual_editor.h"
#include "lv/lv_internal.h"

/* 几何画布视图 - 完整实现 */

/** @brief 几何实体类型枚举 */
typedef enum {
    lv_GEOM_POINT,  /**< 点 */
    lv_GEOM_LINE,   /**< 线 */
    lv_GEOM_CIRCLE, /**< 圆 */
    lv_GEOM_POLYGON /**< 多边形 */
} lvGeomEntityType;

/** @brief 几何实体结构 */
typedef struct lvGeomEntity {
    int id;                /**< 实体唯一标识 */
    lvGeomEntityType type; /**< 实体类型 */
    char label[128];       /**< 实体标签 */

    /** 坐标数据（统一存储）
     *  point: [x, y]
     *  line: [x1, y1, x2, y2]
     *  circle: [cx, cy, r]
     *  polygon: [x1, y1, x2, y2, ...] */
    double *coords;
    int coord_count; /**< 坐标数量 */

    /* 颜色 */
    char stroke_color[32]; /**< 描边颜色 */
    char fill_color[32];   /**< 填充颜色 */
    double stroke_width;   /**< 描边宽度 */
} lvGeomEntity;

/** @brief 约束可视化结构 */
typedef struct lvGeomConstraint {
    int id;          /**< 约束唯一标识 */
    int entity_a_id; /**< 实体A的ID */
    int entity_b_id; /**< 实体B的ID */
    char label[128]; /**< 约束标签 */
    char color[32];  /**< 约束颜色 */
} lvGeomConstraint;

/** @brief 视图边界结构 */
typedef struct lvViewBounds {
    double min_x, min_y, max_x, max_y; /**< 边界范围 */
    int valid;                         /**< 边界是否有效 */
} lvViewBounds;

/** @brief 几何画布内部结构 */
typedef struct lvGeometryCanvas {
    int view_type;                 /**< 视图类型标识 */
    lvGeomEntity *entities;        /**< 实体数组 */
    int entity_count;              /**< 实体数量 */
    int entity_capacity;           /**< 实体数组容量 */
    lvGeomConstraint *constraints; /**< 约束数组 */
    int constraint_count;          /**< 约束数量 */
    int constraint_capacity;       /**< 约束数组容量 */
    lvViewBounds bounds;           /**< 视图边界缓存 */
    void *proof_overlay;           /**< 证明覆盖层（保留扩展） */
    int next_entity_id;            /**< 下一个实体ID */
    int next_constraint_id;        /**< 下一个约束ID */
} lvGeometryCanvas;

/**
 * @brief 创建几何画布视图
 *
 * 分配并初始化几何画布，预分配实体和约束数组的初始容量。
 *
 * @return 成功返回几何画布指针，失败返回NULL
 */
lvGeometryCanvas *lv_geometry_canvas_create(void) {
    lvGeometryCanvas *canvas = lv_calloc(1, sizeof(lvGeometryCanvas));
    if (!canvas)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate geometry canvas");
    canvas->view_type = lv_VIEW_GEOMETRY_CANVAS;
    canvas->entity_capacity = 16;
    canvas->entities = lv_calloc(canvas->entity_capacity, sizeof(lvGeomEntity));
    if (!canvas->entities) {
        lv_free((void **) &canvas);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate entities array");
    }
    canvas->constraint_capacity = 16;
    canvas->constraints = lv_calloc(canvas->constraint_capacity, sizeof(lvGeomConstraint));
    if (!canvas->constraints) {
        lv_free((void **) &canvas->entities);
        lv_free((void **) &canvas);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate constraints array");
    }
    canvas->next_entity_id = 1;
    canvas->next_constraint_id = 1;
    /* 画布结构体无全局颜色字段；实体和约束在添加时各自设置默认样式 */
    return canvas;
}

/**
 * @brief 销毁几何画布视图
 *
 * 释放所有实体的坐标数据、实体数组、约束数组和画布结构体。
 *
 * @param canvas 几何画布指针
 */
void lv_geometry_canvas_destroy(lvGeometryCanvas *canvas) {
    if (!canvas)
        return;
    for (int i = 0; i < canvas->entity_count; i++) {
        lv_free((void **) &canvas->entities[i].coords);
    }
    lv_free((void **) &canvas->entities);
    lv_free((void **) &canvas->constraints);
    lv_free((void **) &canvas);
}

/**
 * @brief 添加几何实体
 *
 * 向画布中添加一个几何实体，自动复制坐标数据并设置默认样式。
 * 如果实体数组已满，自动扩容为当前容量的2倍。
 *
 * @param canvas      几何画布指针
 * @param type        实体类型
 * @param label       实体标签（可为NULL）
 * @param coords      坐标数组
 * @param coord_count 坐标数量
 * @return 成功返回实体ID，失败返回-1
 */
int lv_geometry_canvas_add_entity(lvGeometryCanvas *canvas, int type, const char *label, const double *coords,
                                  int coord_count) {
    if (!canvas || !coords || coord_count <= 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL canvas, coords, or invalid coord_count");

    /* 自动扩容 */
    if (canvas->entity_count >= canvas->entity_capacity) {
        /* [安全] 防止 entity_capacity * 2 整数溢出 */
        if (canvas->entity_capacity > INT_MAX / 2)
            lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "entity capacity overflow");
        int new_cap = canvas->entity_capacity * 2;
        lvGeomEntity *new_arr = lv_realloc(canvas->entities, new_cap * sizeof(lvGeomEntity));
        if (!new_arr)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to realloc entities");
        canvas->entities = new_arr;
        canvas->entity_capacity = new_cap;
    }

    lvGeomEntity *ent = &canvas->entities[canvas->entity_count];
    ent->id = canvas->next_entity_id++;
    ent->type = (lvGeomEntityType) type;
    if (label) {
        strncpy(ent->label, label, sizeof(ent->label) - 1);
        ent->label[sizeof(ent->label) - 1] = '\0';
    } else {
        ent->label[0] = '\0';
    }

    /* 复制坐标 */
    if (coord_count < 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "coord_count is negative");
    ent->coords = lv_calloc(coord_count, sizeof(double));
    if (!ent->coords) {
        /* calloc失败，清零该实体槽位防止半初始化数据残留 */
        memset(ent, 0, sizeof(lvGeomEntity));
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to allocate entity coords");
    }
    if ((size_t) coord_count > SIZE_MAX / sizeof(double))
        lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "coord_count overflow");
    memcpy(ent->coords, coords, (size_t) coord_count * sizeof(double));
    ent->coord_count = coord_count;

    /* 默认样式 */
    strncpy(ent->stroke_color, "#333333", sizeof(ent->stroke_color) - 1);
    ent->stroke_color[sizeof(ent->stroke_color) - 1] = '\0';
    strncpy(ent->fill_color, "none", sizeof(ent->fill_color) - 1);
    ent->fill_color[sizeof(ent->fill_color) - 1] = '\0';
    ent->stroke_width = 2.0;

    canvas->entity_count++;
    /* 标记边界失效 */
    canvas->bounds.valid = 0;
    return ent->id;
}

/**
 * @brief 移除几何实体
 *
 * 删除指定ID的实体，同时移除所有关联的约束。
 * 释放实体的坐标数据，用最后一个元素填充空位。
 *
 * @param canvas 几何画布指针
 * @param id     要移除的实体ID
 * @return 成功返回0，失败返回-1
 */
int lv_geometry_canvas_remove_entity(lvGeometryCanvas *canvas, int id) {
    if (!canvas || id <= 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL canvas or invalid entity id");
    int found = -1;
    for (int i = 0; i < canvas->entity_count; i++) {
        if (canvas->entities[i].id == id) {
            found = i;
            break;
        }
    }
    if (found < 0)
        lv_RETURN_ERROR(lv_ERROR_NOT_FOUND, "entity not found");

    /* 释放坐标 */
    lv_free((void **) &canvas->entities[found].coords);

    /* 移除相关约束 */
    int new_c = 0;
    for (int i = 0; i < canvas->constraint_count; i++) {
        if (canvas->constraints[i].entity_a_id != id && canvas->constraints[i].entity_b_id != id) {
            if (new_c != i) {
                canvas->constraints[new_c] = canvas->constraints[i];
            }
            new_c++;
        }
    }
    canvas->constraint_count = new_c;

    /* 用最后一个元素填充空位 */
    canvas->entities[found] = canvas->entities[canvas->entity_count - 1];
    canvas->entity_count--;
    canvas->bounds.valid = 0;
    return 0;
}

/**
 * @brief 添加约束可视化
 *
 * 在两个几何实体之间添加一条约束可视化线。如果约束数组已满，自动扩容。
 *
 * @param canvas       几何画布指针
 * @param entity_a_id  实体A的ID
 * @param entity_b_id  实体B的ID
 * @param label        约束标签（可为NULL）
 * @return 成功返回约束ID，失败返回-1
 */
int lv_geometry_canvas_add_constraint(lvGeometryCanvas *canvas, int entity_a_id, int entity_b_id, const char *label) {
    if (!canvas || entity_a_id <= 0 || entity_b_id <= 0)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "NULL canvas or invalid entity ids");

    /* 自动扩容 */
    if (canvas->constraint_count >= canvas->constraint_capacity) {
        /* [安全] 防止 constraint_capacity * 2 整数溢出 */
        if (canvas->constraint_capacity > INT_MAX / 2)
            lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "constraint capacity overflow");
        int new_cap = canvas->constraint_capacity * 2;
        lvGeomConstraint *new_arr = lv_realloc(canvas->constraints, new_cap * sizeof(lvGeomConstraint));
        if (!new_arr)
            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "failed to realloc constraints");
        canvas->constraints = new_arr;
        canvas->constraint_capacity = new_cap;
    }

    lvGeomConstraint *c = &canvas->constraints[canvas->constraint_count];
    c->id = canvas->next_constraint_id++;
    c->entity_a_id = entity_a_id;
    c->entity_b_id = entity_b_id;
    if (label) {
        strncpy(c->label, label, sizeof(c->label) - 1);
        c->label[sizeof(c->label) - 1] = '\0';
    } else {
        c->label[0] = '\0';
    }
    strncpy(c->color, "#888888", sizeof(c->color) - 1);
    c->color[sizeof(c->color) - 1] = '\0';

    canvas->constraint_count++;
    return c->id;
}

/* ================================================================
 * 几何实体 vtable（按实体类型分派，消除 3 处重复 switch/if-else）
 * ================================================================ */

/** @brief 几何实体操作 vtable：按实体类型分派 */
typedef struct {
    /** 扩展包围盒（圆需额外考虑半径，其余类型为空操作） */
    void (*extend_bounds)(const lvGeomEntity *e, double *min_x, double *min_y, double *max_x, double *max_y);
    /** 计算中心坐标，成功返回 0，失败返回 -1 */
    int (*center)(const lvGeomEntity *e, double *cx, double *cy);
    /** 渲染为 SVG 片段 */
    void (*render_svg)(const lvGeomEntity *e, char *buf, int buf_size, int *pos);
} lvGeomEntityVtbl;

/** @brief 计算顶点平均位置（多边形默认中心，也作为特例不满足时的兜底） */
static int geom_avg_center(const lvGeomEntity *e, double *cx, double *cy) {
    if (e->coord_count >= 2) {
        double sx = 0, sy = 0;
        for (int j = 0; j < e->coord_count; j += 2) {
            sx += e->coords[j];
            if (j + 1 < e->coord_count)
                sy += e->coords[j + 1];
        }
        int npts = (e->coord_count + 1) / 2;
        *cx = sx / npts;
        *cy = sy / npts;
        return 0;
    }
    return -1;
}

/* ---- 包围盒扩展（默认空操作；圆需额外考虑半径） ---- */

static void geom_default_extend_bounds(const lvGeomEntity *e, double *min_x, double *min_y, double *max_x,
                                       double *max_y) {
    (void) e;
    (void) min_x;
    (void) min_y;
    (void) max_x;
    (void) max_y;
}

static void geom_circle_extend_bounds(const lvGeomEntity *e, double *min_x, double *min_y, double *max_x,
                                      double *max_y) {
    /* 圆需要考虑半径 */
    if (e->coord_count >= 3) {
        double r = e->coords[2];
        if (e->coords[0] - r < *min_x)
            *min_x = e->coords[0] - r;
        if (e->coords[1] - r < *min_y)
            *min_y = e->coords[1] - r;
        if (e->coords[0] + r > *max_x)
            *max_x = e->coords[0] + r;
        if (e->coords[1] + r > *max_y)
            *max_y = e->coords[1] + r;
    }
}

/* ---- 中心计算 ---- */

/** @brief 点：直接使用坐标 */
static int geom_point_center(const lvGeomEntity *e, double *cx, double *cy) {
    if (e->coord_count >= 2) {
        *cx = e->coords[0];
        *cy = e->coords[1];
        return 0;
    }
    return -1;
}

/** @brief 线：两端点中点 */
static int geom_line_center(const lvGeomEntity *e, double *cx, double *cy) {
    if (e->coord_count >= 4) {
        *cx = (e->coords[0] + e->coords[2]) / 2.0;
        *cy = (e->coords[1] + e->coords[3]) / 2.0;
        return 0;
    }
    return geom_avg_center(e, cx, cy);
}

/** @brief 圆：圆心 */
static int geom_circle_center(const lvGeomEntity *e, double *cx, double *cy) {
    if (e->coord_count >= 2) {
        *cx = e->coords[0];
        *cy = e->coords[1];
        return 0;
    }
    return geom_avg_center(e, cx, cy);
}

/** @brief 多边形：顶点平均位置 */
static int geom_polygon_center(const lvGeomEntity *e, double *cx, double *cy) {
    return geom_avg_center(e, cx, cy);
}

/* ---- SVG 渲染 ---- */

/** @brief 点渲染：小圆 + 标签 */
static void geom_point_render(const lvGeomEntity *e, char *buf, int buf_size, int *pos) {
    if (e->coord_count >= 2) {
        lv_SVG_WRITE(buf, *pos, buf_size,
                     "  <circle cx=\"%g\" cy=\"%g\" r=\"4\" "
                  "fill=\"%s\" stroke=\"%s\" stroke-width=\"%g\"/>\n",
                  e->coords[0], e->coords[1], e->fill_color, e->stroke_color, e->stroke_width);
        if (e->label[0] != '\0') {
            lv_SVG_WRITE(buf, *pos, buf_size,
                         "  <text x=\"%g\" y=\"%g\" font-size=\"12\" "
                         "text-anchor=\"middle\" dy=\"-8\">%s</text>\n",
                      e->coords[0], e->coords[1], e->label);
        }
    }
}

/** @brief 线渲染：线段 + 中点标签 */
static void geom_line_render(const lvGeomEntity *e, char *buf, int buf_size, int *pos) {
    if (e->coord_count >= 4) {
        lv_SVG_WRITE(buf, *pos, buf_size,
                     "  <line x1=\"%g\" y1=\"%g\" x2=\"%g\" y2=\"%g\" "
                  "stroke=\"%s\" stroke-width=\"%g\"/>\n",
                  e->coords[0], e->coords[1], e->coords[2], e->coords[3], e->stroke_color, e->stroke_width);
        if (e->label[0] != '\0') {
            double mx = (e->coords[0] + e->coords[2]) / 2.0;
            double my = (e->coords[1] + e->coords[3]) / 2.0;
            lv_SVG_WRITE(buf, *pos, buf_size,
                         "  <text x=\"%g\" y=\"%g\" font-size=\"12\" "
                         "text-anchor=\"middle\" dy=\"-6\">%s</text>\n",
                      mx, my, e->label);
        }
    }
}

/** @brief 圆渲染：圆 + 半径偏移标签 */
static void geom_circle_render(const lvGeomEntity *e, char *buf, int buf_size, int *pos) {
    if (e->coord_count >= 3) {
        lv_SVG_WRITE(buf, *pos, buf_size,
                     "  <circle cx=\"%g\" cy=\"%g\" r=\"%g\" "
                  "fill=\"%s\" stroke=\"%s\" stroke-width=\"%g\"/>\n",
                  e->coords[0], e->coords[1], e->coords[2], e->fill_color, e->stroke_color, e->stroke_width);
        if (e->label[0] != '\0') {
            lv_SVG_WRITE(buf, *pos, buf_size,
                         "  <text x=\"%g\" y=\"%g\" font-size=\"12\" "
                         "text-anchor=\"middle\" dy=\"-%g\">%s</text>\n",
                      e->coords[0], e->coords[1], e->coords[2] + 4, e->label);
        }
    }
}

/** @brief 多边形渲染：顶点序列 + 平均中心标签 */
static void geom_polygon_render(const lvGeomEntity *e, char *buf, int buf_size, int *pos) {
    if (e->coord_count >= 6) {
        lv_SVG_WRITE(buf, *pos, buf_size, "  <polygon points=\"");
        for (int j = 0; j < e->coord_count; j += 2) {
            if (j + 1 < e->coord_count) {
                lv_SVG_WRITE(buf, *pos, buf_size, "%g,%g ", e->coords[j], e->coords[j + 1]);
            }
        }
        lv_SVG_WRITE(buf, *pos, buf_size, "\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%g\"/>\n", e->fill_color,
                  e->stroke_color, e->stroke_width);
        if (e->label[0] != '\0') {
            double cx = 0, cy = 0;
            int npts = e->coord_count / 2;
            for (int j = 0; j < e->coord_count; j += 2) {
                cx += e->coords[j];
                if (j + 1 < e->coord_count)
                    cy += e->coords[j + 1];
            }
            cx /= npts;
            cy /= npts;
            lv_SVG_WRITE(buf, *pos, buf_size,
                         "  <text x=\"%g\" y=\"%g\" font-size=\"12\" "
                         "text-anchor=\"middle\">%s</text>\n",
                      cx, cy, e->label);
        }
    }
}

/** @brief 实体类型 vtable 查找表（指定初始化器，编译器校验 lvGeomEntityType 对齐） */
static const lvGeomEntityVtbl kGeomEntityVtbls[] = {
    [lv_GEOM_POINT] = {geom_default_extend_bounds, geom_point_center, geom_point_render},
    [lv_GEOM_LINE] = {geom_default_extend_bounds, geom_line_center, geom_line_render},
    [lv_GEOM_CIRCLE] = {geom_circle_extend_bounds, geom_circle_center, geom_circle_render},
    [lv_GEOM_POLYGON] = {geom_default_extend_bounds, geom_polygon_center, geom_polygon_render},
};

/* ================================================================
 * 包围盒 / 中心 / SVG 输出
 * ================================================================ */

/**
 * @brief 计算包围盒
 *
 * 遍历所有实体，计算包含所有几何元素的最小包围盒。
 * 对于圆形，额外考虑半径的扩展范围（经 vtable 分派）。
 * 计算结果缓存到 bounds 字段。
 *
 * @param canvas 几何画布指针（内部函数，入参非空由调用者保证）
 */
static void compute_bounds(lvGeometryCanvas *canvas) {
    if (canvas->entity_count == 0) {
        canvas->bounds.valid = 0;
        return;
    }
    double min_x = 1e18, min_y = 1e18, max_x = -1e18, max_y = -1e18;
    for (int i = 0; i < canvas->entity_count; i++) {
        lvGeomEntity *e = &canvas->entities[i];
        for (int j = 0; j < e->coord_count; j += 2) {
            double x = e->coords[j];
            double y = (j + 1 < e->coord_count) ? e->coords[j + 1] : 0;
            if (x < min_x)
                min_x = x;
            if (y < min_y)
                min_y = y;
            if (x > max_x)
                max_x = x;
            if (y > max_y)
                max_y = y;
        }
        /* 按实体类型扩展包围盒（圆需额外考虑半径） */
        if ((unsigned) e->type < lv_ARRAY_SIZE(kGeomEntityVtbls)) {
            kGeomEntityVtbls[e->type].extend_bounds(e, &min_x, &min_y, &max_x, &max_y);
        }
    }
    /* 添加边距 */
    double margin = 20.0;
    canvas->bounds.min_x = min_x - margin;
    canvas->bounds.min_y = min_y - margin;
    canvas->bounds.max_x = max_x + margin;
    canvas->bounds.max_y = max_y + margin;
    canvas->bounds.valid = 1;
}

/**
 * @brief 调整视图到包围盒
 *
 * 计算当前所有实体的包围盒，使视图适配到包围盒范围。
 *
 * @param canvas 几何画布指针
 * @return 成功返回0，边界无效返回-1
 */
int lv_geometry_canvas_fit_view(lvGeometryCanvas *canvas) {
    lv_CHECK_NOT_NULL(canvas);
    compute_bounds(canvas);
    if (!canvas->bounds.valid)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "no entities to fit view");
    return 0;
}

/**
 * @brief 查找实体中心坐标
 *
 * 根据实体ID查找几何实体的中心点坐标。
 * 不同类型的实体有不同的中心计算方式（经 vtable 分派）：
 *   - 点：直接使用坐标
 *   - 线：两端点中点
 *   - 圆：圆心
 *   - 多边形：顶点平均位置
 *
 * @param canvas 几何画布指针
 * @param id     实体ID
 * @param cx     输出中心X坐标
 * @param cy     输出中心Y坐标
 * @return 成功返回0，失败返回-1
 */
static int find_entity_center(lvGeometryCanvas *canvas, int id, double *cx, double *cy) {
    for (int i = 0; i < canvas->entity_count; i++) {
        lvGeomEntity *e = &canvas->entities[i];
        if (e->id == id) {
            if ((unsigned) e->type < lv_ARRAY_SIZE(kGeomEntityVtbls)) {
                return kGeomEntityVtbls[e->type].center(e, cx, cy);
            }
            /* 未知类型：退化为顶点平均（保持原 else 分支语义） */
            return geom_avg_center(e, cx, cy);
        }
    }
    return -1;
}

/**
 * @brief 生成 SVG 输出
 *
 * 将几何画布中的实体和约束渲染为 SVG 格式的 XML 字符串。
 * 调用者负责释放返回的字符串。
 *
 * @param canvas 几何画布指针
 * @return 成功返回分配的SVG字符串，失败返回NULL
 */
char *lv_geometry_canvas_render_svg(lvGeometryCanvas *canvas) {
    if (!canvas)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "NULL canvas");

    /* 确保边界已计算 */
    if (!canvas->bounds.valid)
        compute_bounds(canvas);
    if (!canvas->bounds.valid) {
        /* 空画布，使用默认边界 */
        canvas->bounds.min_x = 0;
        canvas->bounds.min_y = 0;
        canvas->bounds.max_x = 800;
        canvas->bounds.max_y = 600;
        canvas->bounds.valid = 1;
    }

    /* [安全] 估算输出缓冲区大小，防止整数溢出 */
    size_t est_size = (size_t) canvas->entity_count * 512 + (size_t) canvas->constraint_count * 256 + 4096;
    /* 限制最大缓冲区大小防止过度分配 */
    if (est_size > 1024 * 1024 * 16)
        est_size = 1024 * 1024 * 16;
    int buf_size = (int) est_size;
    char *buf = lv_calloc(buf_size, sizeof(char));
    if (!buf)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate SVG buffer");

    int pos = 0;

    lv_SVG_WRITE(buf, pos, buf_size,
                 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                 "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                 "viewBox=\"%g %g %g %g\" width=\"800\" height=\"600\">\n",
              canvas->bounds.min_x, canvas->bounds.min_y, canvas->bounds.max_x - canvas->bounds.min_x,
              canvas->bounds.max_y - canvas->bounds.min_y);

    /* 绘制实体（按类型 vtable 分派渲染） */
    for (int i = 0; i < canvas->entity_count; i++) {
        lvGeomEntity *e = &canvas->entities[i];
        if ((unsigned) e->type < lv_ARRAY_SIZE(kGeomEntityVtbls)) {
            kGeomEntityVtbls[e->type].render_svg(e, buf, buf_size, &pos);
        }
        /* 未知类型不渲染（原 switch default 分支为空） */
    }

    /* 绘制约束（虚线） */
    for (int i = 0; i < canvas->constraint_count; i++) {
        lvGeomConstraint *c = &canvas->constraints[i];
        double ax, ay, bx, by;
        if (find_entity_center(canvas, c->entity_a_id, &ax, &ay) != 0)
            continue;
        if (find_entity_center(canvas, c->entity_b_id, &bx, &by) != 0)
            continue;

        lv_SVG_WRITE(buf, pos, buf_size,
                     "  <line x1=\"%g\" y1=\"%g\" x2=\"%g\" y2=\"%g\" "
                     "stroke=\"%s\" stroke-width=\"1\" stroke-dasharray=\"5,5\"/>\n",
                  ax, ay, bx, by, c->color);

        if (c->label[0] != '\0') {
            double mx = (ax + bx) / 2.0;
            double my = (ay + by) / 2.0;
            lv_SVG_WRITE(buf, pos, buf_size,
                         "  <text x=\"%g\" y=\"%g\" font-size=\"10\" "
                         "fill=\"%s\" text-anchor=\"middle\" dy=\"-4\">%s</text>\n",
                      mx, my, c->color, c->label);
        }
    }

    lv_SVG_WRITE(buf, pos, buf_size, "</svg>\n");
    return buf;
}