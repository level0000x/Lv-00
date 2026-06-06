#include "lv00/visual_editor.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* 几何画布视图 - 完整实现 */

/* 几何实体类型 */
typedef enum {
    LV00_GEOM_POINT,
    LV00_GEOM_LINE,
    LV00_GEOM_CIRCLE,
    LV00_GEOM_POLYGON
} Lv00GeomEntityType;

/* 几何实体 */
typedef struct Lv00GeomEntity {
    int id;
    Lv00GeomEntityType type;
    char label[128];

    /* 坐标数据（统一存储） */
    double *coords;   /* point: [x,y]; line: [x1,y1,x2,y2]; circle: [cx,cy,r]; polygon: [x1,y1,x2,y2,...] */
    int coord_count;

    /* 颜色 */
    char stroke_color[32];
    char fill_color[32];
    double stroke_width;
} Lv00GeomEntity;

/* 约束可视化 */
typedef struct Lv00GeomConstraint {
    int id;
    int entity_a_id;
    int entity_b_id;
    char label[128];
    char color[32];
} Lv00GeomConstraint;

/* 视图边界 */
typedef struct Lv00ViewBounds {
    double min_x, min_y, max_x, max_y;
    int valid;
} Lv00ViewBounds;

typedef struct Lv00GeometryCanvas {
    int view_type;
    Lv00GeomEntity *entities;
    int entity_count;
    int entity_capacity;
    Lv00GeomConstraint *constraints;
    int constraint_count;
    int constraint_capacity;
    Lv00ViewBounds bounds;
    void *proof_overlay;
    int next_entity_id;
    int next_constraint_id;
} Lv00GeometryCanvas;

Lv00GeometryCanvas *lv00_geometry_canvas_create(void) {
    Lv00GeometryCanvas *canvas = lv00_calloc(1, sizeof(Lv00GeometryCanvas));
    if (!canvas) return NULL;
    canvas->view_type = LV00_VIEW_GEOMETRY_CANVAS;
    canvas->entity_capacity = 16;
    canvas->entities = lv00_calloc(canvas->entity_capacity, sizeof(Lv00GeomEntity));
    if (!canvas->entities) { lv00_free((void **)&canvas); return NULL; }
    canvas->constraint_capacity = 16;
    canvas->constraints = lv00_calloc(canvas->constraint_capacity, sizeof(Lv00GeomConstraint));
    if (!canvas->constraints) { lv00_free((void **)&canvas->entities); lv00_free((void **)&canvas); return NULL; }
    canvas->next_entity_id = 1;
    canvas->next_constraint_id = 1;
    /* 默认颜色 */
    strncpy(canvas->entities ? "" : "", "", 0); /* 占位 */
    return canvas;
}

void lv00_geometry_canvas_destroy(Lv00GeometryCanvas *canvas) {
    if (!canvas) return;
    for (int i = 0; i < canvas->entity_count; i++) {
        lv00_free((void **)&canvas->entities[i].coords);
    }
    lv00_free((void **)&canvas->entities);
    lv00_free((void **)&canvas->constraints);
    lv00_free((void **)&canvas);
}

/* 添加几何实体 */
int lv00_geometry_canvas_add_entity(Lv00GeometryCanvas *canvas, int type,
                                     const char *label, const double *coords,
                                     int coord_count) {
    if (!canvas || !coords || coord_count <= 0) return -1;

    /* 自动扩容 */
    if (canvas->entity_count >= canvas->entity_capacity) {
        int new_cap = canvas->entity_capacity * 2;
        Lv00GeomEntity *new_arr = lv00_realloc(canvas->entities, new_cap * sizeof(Lv00GeomEntity));
        if (!new_arr) return -1;
        canvas->entities = new_arr;
        canvas->entity_capacity = new_cap;
    }

    Lv00GeomEntity *ent = &canvas->entities[canvas->entity_count];
    ent->id = canvas->next_entity_id++;
    ent->type = (Lv00GeomEntityType)type;
    if (label) {
        strncpy(ent->label, label, sizeof(ent->label) - 1);
        ent->label[sizeof(ent->label) - 1] = '\0';
    } else {
        ent->label[0] = '\0';
    }

    /* 复制坐标 */
    ent->coords = lv00_calloc(coord_count, sizeof(double));
    if (!ent->coords) {
        /* calloc失败，清零该实体槽位防止半初始化数据残留 */
        memset(ent, 0, sizeof(Lv00GeomEntity));
        return -1;
    }
    memcpy(ent->coords, coords, coord_count * sizeof(double));
    ent->coord_count = coord_count;

    /* 默认样式 */
    strncpy(ent->stroke_color, "#333333", sizeof(ent->stroke_color) - 1);
    strncpy(ent->fill_color, "none", sizeof(ent->fill_color) - 1);
    ent->stroke_width = 2.0;

    canvas->entity_count++;
    /* 标记边界失效 */
    canvas->bounds.valid = 0;
    return ent->id;
}

/* 移除几何实体 */
int lv00_geometry_canvas_remove_entity(Lv00GeometryCanvas *canvas, int id) {
    if (!canvas || id <= 0) return -1;
    int found = -1;
    for (int i = 0; i < canvas->entity_count; i++) {
        if (canvas->entities[i].id == id) { found = i; break; }
    }
    if (found < 0) return -1;

    /* 释放坐标 */
    lv00_free((void **)&canvas->entities[found].coords);

    /* 移除相关约束 */
    int new_c = 0;
    for (int i = 0; i < canvas->constraint_count; i++) {
        if (canvas->constraints[i].entity_a_id != id &&
            canvas->constraints[i].entity_b_id != id) {
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

/* 添加约束可视化 */
int lv00_geometry_canvas_add_constraint(Lv00GeometryCanvas *canvas,
                                        int entity_a_id, int entity_b_id,
                                        const char *label) {
    if (!canvas || entity_a_id <= 0 || entity_b_id <= 0) return -1;

    /* 自动扩容 */
    if (canvas->constraint_count >= canvas->constraint_capacity) {
        int new_cap = canvas->constraint_capacity * 2;
        Lv00GeomConstraint *new_arr = lv00_realloc(canvas->constraints,
                                               new_cap * sizeof(Lv00GeomConstraint));
        if (!new_arr) return -1;
        canvas->constraints = new_arr;
        canvas->constraint_capacity = new_cap;
    }

    Lv00GeomConstraint *c = &canvas->constraints[canvas->constraint_count];
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

    canvas->constraint_count++;
    return c->id;
}

/* 计算包围盒 */
static void compute_bounds(Lv00GeometryCanvas *canvas) {
    if (canvas->entity_count == 0) {
        canvas->bounds.valid = 0;
        return;
    }
    double min_x = 1e18, min_y = 1e18, max_x = -1e18, max_y = -1e18;
    for (int i = 0; i < canvas->entity_count; i++) {
        Lv00GeomEntity *e = &canvas->entities[i];
        for (int j = 0; j < e->coord_count; j += 2) {
            double x = e->coords[j];
            double y = (j + 1 < e->coord_count) ? e->coords[j + 1] : 0;
            if (x < min_x) min_x = x;
            if (y < min_y) min_y = y;
            if (x > max_x) max_x = x;
            if (y > max_y) max_y = y;
        }
        /* 圆需要考虑半径 */
        if (e->type == LV00_GEOM_CIRCLE && e->coord_count >= 3) {
            double r = e->coords[2];
            if (e->coords[0] - r < min_x) min_x = e->coords[0] - r;
            if (e->coords[1] - r < min_y) min_y = e->coords[1] - r;
            if (e->coords[0] + r > max_x) max_x = e->coords[0] + r;
            if (e->coords[1] + r > max_y) max_y = e->coords[1] + r;
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

/* 调整视图到包围盒 */
int lv00_geometry_canvas_fit_view(Lv00GeometryCanvas *canvas) {
    if (!canvas) return -1;
    compute_bounds(canvas);
    return canvas->bounds.valid ? 0 : -1;
}

/* 根据实体ID查找实体中心坐标 */
static int find_entity_center(Lv00GeometryCanvas *canvas, int id,
                                double *cx, double *cy) {
    for (int i = 0; i < canvas->entity_count; i++) {
        Lv00GeomEntity *e = &canvas->entities[i];
        if (e->id == id) {
            if (e->type == LV00_GEOM_POINT && e->coord_count >= 2) {
                *cx = e->coords[0]; *cy = e->coords[1];
            } else if (e->type == LV00_GEOM_LINE && e->coord_count >= 4) {
                *cx = (e->coords[0] + e->coords[2]) / 2.0;
                *cy = (e->coords[1] + e->coords[3]) / 2.0;
            } else if (e->type == LV00_GEOM_CIRCLE && e->coord_count >= 2) {
                *cx = e->coords[0]; *cy = e->coords[1];
            } else if (e->coord_count >= 2) {
                double sx = 0, sy = 0;
                for (int j = 0; j < e->coord_count; j += 2) {
                    sx += e->coords[j];
                    if (j + 1 < e->coord_count) sy += e->coords[j + 1];
                }
                int npts = (e->coord_count + 1) / 2;
                *cx = sx / npts; *cy = sy / npts;
            } else {
                return -1;
            }
            return 0;
        }
    }
    return -1;
}

/* 生成 SVG 输出 */
char *lv00_geometry_canvas_render_svg(Lv00GeometryCanvas *canvas) {
    if (!canvas) return NULL;

    /* 确保边界已计算 */
    if (!canvas->bounds.valid) compute_bounds(canvas);
    if (!canvas->bounds.valid) {
        /* 空画布，使用默认边界 */
        canvas->bounds.min_x = 0; canvas->bounds.min_y = 0;
        canvas->bounds.max_x = 800; canvas->bounds.max_y = 600;
        canvas->bounds.valid = 1;
    }

    /* 估算输出缓冲区大小 */
    int buf_size = 4096 + canvas->entity_count * 512 + canvas->constraint_count * 256;
    char *buf = lv00_calloc(buf_size, sizeof(char));
    if (!buf) return NULL;

    int pos = 0;
    /* 辅助宏：安全写入 snprintf 链，防止 pos 溢出 buf_size */
    #define SVG_SAFE_WRITE(...) do { \
        if (pos < buf_size) { \
            int _w = snprintf(buf + pos, (size_t)(buf_size - pos), __VA_ARGS__); \
            if (_w > 0) { \
                pos += _w; \
                if (pos >= buf_size) pos = buf_size - 1; \
            } \
        } \
    } while(0)

    SVG_SAFE_WRITE(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "viewBox=\"%g %g %g %g\" width=\"800\" height=\"600\">\n",
        canvas->bounds.min_x, canvas->bounds.min_y,
        canvas->bounds.max_x - canvas->bounds.min_x,
        canvas->bounds.max_y - canvas->bounds.min_y);

    /* 绘制实体 */
    for (int i = 0; i < canvas->entity_count; i++) {
        Lv00GeomEntity *e = &canvas->entities[i];
        switch (e->type) {
        case LV00_GEOM_POINT:
            if (e->coord_count >= 2) {
                SVG_SAFE_WRITE(
                    "  <circle cx=\"%g\" cy=\"%g\" r=\"4\" "
                    "fill=\"%s\" stroke=\"%s\" stroke-width=\"%g\"/>\n",
                    e->coords[0], e->coords[1],
                    e->fill_color, e->stroke_color, e->stroke_width);
                if (e->label[0] != '\0') {
                    SVG_SAFE_WRITE(
                        "  <text x=\"%g\" y=\"%g\" font-size=\"12\" "
                        "text-anchor=\"middle\" dy=\"-8\">%s</text>\n",
                        e->coords[0], e->coords[1], e->label);
                }
            }
            break;
        case LV00_GEOM_LINE:
            if (e->coord_count >= 4) {
                SVG_SAFE_WRITE(
                    "  <line x1=\"%g\" y1=\"%g\" x2=\"%g\" y2=\"%g\" "
                    "stroke=\"%s\" stroke-width=\"%g\"/>\n",
                    e->coords[0], e->coords[1],
                    e->coords[2], e->coords[3],
                    e->stroke_color, e->stroke_width);
                if (e->label[0] != '\0') {
                    double mx = (e->coords[0] + e->coords[2]) / 2.0;
                    double my = (e->coords[1] + e->coords[3]) / 2.0;
                    SVG_SAFE_WRITE(
                        "  <text x=\"%g\" y=\"%g\" font-size=\"12\" "
                        "text-anchor=\"middle\" dy=\"-6\">%s</text>\n",
                        mx, my, e->label);
                }
            }
            break;
        case LV00_GEOM_CIRCLE:
            if (e->coord_count >= 3) {
                SVG_SAFE_WRITE(
                    "  <circle cx=\"%g\" cy=\"%g\" r=\"%g\" "
                    "fill=\"%s\" stroke=\"%s\" stroke-width=\"%g\"/>\n",
                    e->coords[0], e->coords[1], e->coords[2],
                    e->fill_color, e->stroke_color, e->stroke_width);
                if (e->label[0] != '\0') {
                    SVG_SAFE_WRITE(
                        "  <text x=\"%g\" y=\"%g\" font-size=\"12\" "
                        "text-anchor=\"middle\" dy=\"-%g\">%s</text>\n",
                        e->coords[0], e->coords[1], e->coords[2] + 4, e->label);
                }
            }
            break;
        case LV00_GEOM_POLYGON:
            if (e->coord_count >= 6) {
                SVG_SAFE_WRITE(
                    "  <polygon points=\"");
                for (int j = 0; j < e->coord_count; j += 2) {
                    if (j + 1 < e->coord_count) {
                        SVG_SAFE_WRITE(
                            "%g,%g ", e->coords[j], e->coords[j + 1]);
                    }
                }
                SVG_SAFE_WRITE(
                    "\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%g\"/>\n",
                    e->fill_color, e->stroke_color, e->stroke_width);
                if (e->label[0] != '\0') {
                    double cx = 0, cy = 0;
                    int npts = e->coord_count / 2;
                    for (int j = 0; j < e->coord_count; j += 2) {
                        cx += e->coords[j];
                        if (j + 1 < e->coord_count) cy += e->coords[j + 1];
                    }
                    cx /= npts; cy /= npts;
                    SVG_SAFE_WRITE(
                        "  <text x=\"%g\" y=\"%g\" font-size=\"12\" "
                        "text-anchor=\"middle\">%s</text>\n",
                        cx, cy, e->label);
                }
            }
            break;
        default:
            break;
        }
    }

    /* 绘制约束（虚线） */
    for (int i = 0; i < canvas->constraint_count; i++) {
        Lv00GeomConstraint *c = &canvas->constraints[i];
        double ax, ay, bx, by;
        if (find_entity_center(canvas, c->entity_a_id, &ax, &ay) != 0) continue;
        if (find_entity_center(canvas, c->entity_b_id, &bx, &by) != 0) continue;

        SVG_SAFE_WRITE(
            "  <line x1=\"%g\" y1=\"%g\" x2=\"%g\" y2=\"%g\" "
            "stroke=\"%s\" stroke-width=\"1\" stroke-dasharray=\"5,5\"/>\n",
            ax, ay, bx, by, c->color);

        if (c->label[0] != '\0') {
            double mx = (ax + bx) / 2.0;
            double my = (ay + by) / 2.0;
            SVG_SAFE_WRITE(
                "  <text x=\"%g\" y=\"%g\" font-size=\"10\" "
                "fill=\"%s\" text-anchor=\"middle\" dy=\"-4\">%s</text>\n",
                mx, my, c->color, c->label);
        }
    }

    SVG_SAFE_WRITE("</svg>\n");
    #undef SVG_SAFE_WRITE
    return buf;
}
