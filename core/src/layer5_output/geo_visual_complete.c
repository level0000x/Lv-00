/**
 * @file geo_visual_complete.c
 * @brief 几何可视化完整版实现
 *
 * 提供几何图形的创建、样式设置、空间变换、场景管理和多后端渲染功能。
 * 支持 SVG / Cairo / Three.js / TikZ / PNG 五种渲染后端。
 *
 * @version 1.0.0
 * @author Lv-00 Project
 */

#include "lv/lv_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_file.h"
#include "lv/lv_lifecycle.h"

#include "lv/geo_visual.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/lv_render_visitor.h"
#include "lv/simd_ops.h" /* lv_mat4_identity_f / lv_mat4_mul_f（4x4 列主序 float，收敛共享） */
#include "lv/lv_numeric.h"
#include "lv/lv_xmacro.h"




/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/** 设置默认样式 */
static void set_default_style(lvVisualStyle *style) {
    /* 默认黑色描边，无填充 */
    style->stroke_color[0] = 0.0f;
    style->stroke_color[1] = 0.0f;
    style->stroke_color[2] = 0.0f;
    style->stroke_color[3] = 1.0f;
    style->fill_color[0] = 0.0f;
    style->fill_color[1] = 0.0f;
    style->fill_color[2] = 0.0f;
    style->fill_color[3] = 0.0f;
    style->stroke_width = 1.0f;
    style->opacity = 1.0f;
    style->dashed = 0;
}

/* ========================================================================
 * 对象构造器
 * ======================================================================== */

lvVisualObject *lv_visual_point_create(float x, float y) {
    lvVisualObject *obj = (lvVisualObject *) lv_calloc(1, sizeof(lvVisualObject));
    if (obj == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_visual_point_create: lv_calloc failed");

    obj->type = lv_VISUAL_POINT;
    set_default_style(&obj->style);
    lv_mat4_identity_f(obj->transform);
    obj->children = NULL;
    obj->children_count = 0;

    /* 缓存点坐标 */
    float *cache = (float *) lv_malloc(2 * sizeof(float));
    if (cache != NULL) {
        cache[0] = x;
        cache[1] = y;
    }
    obj->render_cache = cache;

    return obj;
}

lvVisualObject *lv_visual_line_create(float x1, float y1, float x2, float y2) {
    lvVisualObject *obj = (lvVisualObject *) lv_calloc(1, sizeof(lvVisualObject));
    if (obj == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_visual_line_create: lv_calloc failed");

    obj->type = lv_VISUAL_SEGMENT;
    set_default_style(&obj->style);
    lv_mat4_identity_f(obj->transform);
    obj->children = NULL;
    obj->children_count = 0;

    /* 缓存线段端点坐标 */
    float *cache = (float *) lv_malloc(4 * sizeof(float));
    if (cache != NULL) {
        cache[0] = x1;
        cache[1] = y1;
        cache[2] = x2;
        cache[3] = y2;
    }
    obj->render_cache = cache;

    return obj;
}

lvVisualObject *lv_visual_circle_create(float cx, float cy, float r) {
    lvVisualObject *obj = (lvVisualObject *) lv_calloc(1, sizeof(lvVisualObject));
    if (obj == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_visual_circle_create: lv_calloc failed");

    obj->type = lv_VISUAL_CIRCLE;
    set_default_style(&obj->style);
    lv_mat4_identity_f(obj->transform);
    obj->children = NULL;
    obj->children_count = 0;

    /* 缓存圆心和半径 */
    float *cache = (float *) lv_malloc(3 * sizeof(float));
    if (cache != NULL) {
        cache[0] = cx;
        cache[1] = cy;
        cache[2] = r;
    }
    obj->render_cache = cache;

    return obj;
}

lvVisualObject *lv_visual_group_create(lvVisualObject **objs, size_t n) {
    if (objs == NULL || n == 0)
        return NULL;

    lvVisualObject *obj = (lvVisualObject *) lv_calloc(1, sizeof(lvVisualObject));
    if (obj == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_visual_group_create: obj calloc failed");

    obj->type = lv_VISUAL_MOBJECT_GROUP;
    set_default_style(&obj->style);
    lv_mat4_identity_f(obj->transform);

    obj->children = (lvVisualObject **) lv_calloc(n, sizeof(lvVisualObject *));
    if (obj->children == NULL) {
        lv_free((void **) &obj);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_visual_group_create: children calloc failed");
    }
    memcpy(obj->children, objs, n * sizeof(lvVisualObject *));
    obj->children_count = n;

    return obj;
}

/* ========================================================================
 * 样式设置
 * ======================================================================== */

void lv_visual_set_style(lvVisualObject *obj, const lvVisualStyle *style) {
    if (obj == NULL || style == NULL)
        return;
    obj->style = *style;
}

void lv_visual_set_color(lvVisualObject *obj, float r, float g, float b, float a) {
    if (obj == NULL)
        return;
    obj->style.stroke_color[0] = r;
    obj->style.stroke_color[1] = g;
    obj->style.stroke_color[2] = b;
    obj->style.stroke_color[3] = a;
}

void lv_visual_set_dashed(lvVisualObject *obj, int dashed) {
    if (obj == NULL)
        return;
    obj->style.dashed = dashed;
}

/* ========================================================================
 * 空间变换
 * ======================================================================== */

void lv_visual_translate(lvVisualObject *obj, float dx, float dy, float dz) {
    if (obj == NULL)
        return;
    float t[16];
    lv_mat4_identity_f(t);
    t[12] = dx;
    t[13] = dy;
    t[14] = dz;
    lv_mat4_mul_f(obj->transform, obj->transform, t);
}

void lv_visual_scale(lvVisualObject *obj, float sx, float sy) {
    if (obj == NULL)
        return;
    float s[16];
    lv_mat4_identity_f(s);
    s[0] = sx;
    s[5] = sy;
    lv_mat4_mul_f(obj->transform, obj->transform, s);
}

void lv_visual_rotate(lvVisualObject *obj, float angle, float axis[3]) {
    if (obj == NULL)
        return;

    /* 默认绕 Z 轴旋转 */
    float ax = 0.0f, ay = 0.0f, az = 1.0f;
    /* exempt: float 渲染精度 3D 归一化（sqrtf/1e-6f），与 double 域
       lv_normalize_3d 语义不同（float 运算 + len>1e-6f 判定），保持原样；
       零长度分支（保留默认轴）留在调用点。 */
    if (axis != NULL) {
        float len = sqrtf(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
        if (len > 1e-6f) {
            ax = axis[0] / len;
            ay = axis[1] / len;
            az = axis[2] / len;
        }
    }

    float c = cosf(angle);
    float s = sinf(angle);
    float t[16];
    lv_mat4_identity_f(t);

    /* Rodrigues 旋转矩阵 */
    t[0] = c + ax * ax * (1 - c);
    t[1] = ay * ax * (1 - c) + az * s;
    t[2] = az * ax * (1 - c) - ay * s;
    t[4] = ax * ay * (1 - c) - az * s;
    t[5] = c + ay * ay * (1 - c);
    t[6] = az * ay * (1 - c) + ax * s;
    t[8] = ax * az * (1 - c) + ay * s;
    t[9] = ay * az * (1 - c) - ax * s;
    t[10] = c + az * az * (1 - c);

    lv_mat4_mul_f(obj->transform, obj->transform, t);
}

/* ========================================================================
 * 场景管理
 * ======================================================================== */

lvVisualScene *lv_visual_scene_create(void) {
    lvVisualScene *scene = (lvVisualScene *) lv_calloc(1, sizeof(lvVisualScene));
    if (scene == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_visual_scene_create: lv_calloc failed");

    scene->objects = NULL;
    scene->object_count = 0;
    scene->object_capacity = 0;
    scene->camera_center[0] = 0.0f;
    scene->camera_center[1] = 0.0f;
    scene->camera_center[2] = 0.0f;
    scene->camera_zoom = 1.0f;
    scene->is_3d = 0;
    scene->current_time = 0.0f;
    scene->total_duration = 0.0f;

    return scene;
}

void lv_visual_scene_add(lvVisualScene *scene, lvVisualObject *obj) {
    if (scene == NULL || obj == NULL)
        return;

    if (!lv_ensure_capacity((void **) &scene->objects, (int) scene->object_count, (int *) &scene->object_capacity,
                            sizeof(lvVisualObject *), 0))
        return;

    scene->objects[scene->object_count++] = obj;
}

void lv_visual_scene_clear(lvVisualScene *scene) {
    if (scene == NULL)
        return;
    for (size_t i = 0; i < scene->object_count; i++) {
        lv_visual_object_destroy(scene->objects[i]);
    }
    lv_free((void **) &scene->objects);
    scene->object_count = 0;
    scene->object_capacity = 0;
}

void lv_visual_scene_set_camera(lvVisualScene *scene, float cx, float cy, float cz, float zoom) {
    if (scene == NULL)
        return;
    scene->camera_center[0] = cx;
    scene->camera_center[1] = cy;
    scene->camera_center[2] = cz;
    scene->camera_zoom = zoom;
}

/* ========================================================================
 * 渲染辅助函数
 * ======================================================================== */

/** 将线性 RGBA 转为 0-255 整数（钳位） */
static int to_byte(float c) {
    int v = (int) (c * 255.0f);
    if (v < 0)
        return 0;
    if (v > 255)
        return 255;
    return v;
}

/** 应用相机变换：将模型坐标映射到屏幕坐标 */
static void apply_camera(float *x, float *y, const lvVisualScene *scene) {
    float cx = scene->camera_center[0];
    float cy = scene->camera_center[1];
    float zoom = scene->camera_zoom;
    *x = (*x - cx) * zoom + cx;
    *y = (*y - cy) * zoom + cy;
}

/** 获取渲染缓存的端点或中心数据 */
static bool get_float_cache(const lvVisualObject *obj, float *out, int count) {
    if (obj->render_cache == NULL)
        return false;
    memcpy(out, obj->render_cache, (size_t) count * sizeof(float));
    return true;
}

/* ========================================================================
 * SVG 渲染
 * ======================================================================== */

/** 输出 SVG 样式属性（stroke, fill, opacity, dasharray） */
static void svg_write_style(FILE *fp, const lvVisualStyle *s) {
    fprintf(fp, " stroke=\"rgb(%d,%d,%d)\"", to_byte(s->stroke_color[0]), to_byte(s->stroke_color[1]),
            to_byte(s->stroke_color[2]));
    fprintf(fp, " stroke-width=\"%.2f\"", s->stroke_width);
    fprintf(fp, " stroke-opacity=\"%.2f\"", s->stroke_color[3]);

    if (s->fill_color[3] > 0.0f) {
        fprintf(fp, " fill=\"rgb(%d,%d,%d)\"", to_byte(s->fill_color[0]), to_byte(s->fill_color[1]),
                to_byte(s->fill_color[2]));
        fprintf(fp, " fill-opacity=\"%.2f\"", s->fill_color[3]);
    } else {
        fprintf(fp, " fill=\"none\"");
    }

    if (s->opacity < 1.0f) {
        fprintf(fp, " opacity=\"%.2f\"", s->opacity);
    }
    if (s->dashed) {
        fprintf(fp, " stroke-dasharray=\"5,5\"");
    }
}

/* ── SVG 渲染处理器函数 ── */
typedef void (*SvgObjHandler)(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth);

static void svg_render_point(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    float cache[8];
    memset(cache, 0, sizeof(cache));
    if (!get_float_cache(obj, cache, 2))
        return;
    float px = cache[0], py = cache[1];
    apply_camera(&px, &py, scene);
    fprintf(fp, "%*s<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\"", depth * 2, "", px, py,
            obj->style.stroke_width > 2.0f ? obj->style.stroke_width : 3.0f);
    svg_write_style(fp, &obj->style);
    fprintf(fp, " fill=\"rgb(%d,%d,%d)\"", to_byte(obj->style.stroke_color[0]),
            to_byte(obj->style.stroke_color[1]), to_byte(obj->style.stroke_color[2]));
    fprintf(fp, "/>\n");
}

static void svg_render_segment(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    float cache[8];
    memset(cache, 0, sizeof(cache));
    if (!get_float_cache(obj, cache, 4))
        return;
    float x1 = cache[0], y1 = cache[1];
    float x2 = cache[2], y2 = cache[3];
    apply_camera(&x1, &y1, scene);
    apply_camera(&x2, &y2, scene);
    fprintf(fp, "%*s<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"",
            depth * 2, "", x1, y1, x2, y2);
    svg_write_style(fp, &obj->style);
    fprintf(fp, "/>\n");
}

static void svg_render_line(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    float cache[8];
    memset(cache, 0, sizeof(cache));
    if (!get_float_cache(obj, cache, 4))
        return;
    float x1 = cache[0], y1 = cache[1];
    float x2 = cache[2], y2 = cache[3];
    float w = 800.0f, h = 600.0f;
    float dx = x2 - x1, dy = y2 - y1;
    /* exempt: float 渲染精度 2D 归一化（sqrtf/1e-6f），渲染输出层 float 语义，
       与 double 域 lv_normalize_3d 不一致，保持原样（零长度分支提前 return）。 */
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f)
        return;
    float ux = dx / len, uy = dy / len;
    float t_max = sqrtf(w * w + h * h);
    float lx1 = x1 - ux * t_max, ly1 = y1 - uy * t_max;
    float lx2 = x1 + ux * t_max, ly2 = y1 + uy * t_max;
    apply_camera(&lx1, &ly1, scene);
    apply_camera(&lx2, &ly2, scene);
    fprintf(fp, "%*s<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\"",
            depth * 2, "", lx1, ly1, lx2, ly2);
    svg_write_style(fp, &obj->style);
    fprintf(fp, "/>\n");
}

static void svg_render_circle(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    float cache[8];
    memset(cache, 0, sizeof(cache));
    if (!get_float_cache(obj, cache, 3))
        return;
    float cx = cache[0], cy = cache[1], r = cache[2];
    apply_camera(&cx, &cy, scene);
    fprintf(fp, "%*s<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\"", depth * 2, "", cx, cy, r);
    svg_write_style(fp, &obj->style);
    fprintf(fp, "/>\n");
}

static void svg_render_polygon(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    if (obj->render_cache == NULL)
        return;
    int pcount = ((int *) obj->render_cache)[0];
    float *verts = (float *) ((int *) obj->render_cache + 1);
    if (pcount < 3)
        return;
    fprintf(fp, "%*s<polygon points=\"", depth * 2, "");
    for (int j = 0; j < pcount; j++) {
        float vx = verts[j * 2], vy = verts[j * 2 + 1];
        apply_camera(&vx, &vy, scene);
        fprintf(fp, "%.2f,%.2f ", vx, vy);
    }
    fprintf(fp, "\"");
    svg_write_style(fp, &obj->style);
    fprintf(fp, "/>\n");
}

static const SvgObjHandler svg_render_table[lv_VISUAL_MOBJECT_GROUP] = {
    [lv_VISUAL_POINT]   = svg_render_point,
    [lv_VISUAL_SEGMENT] = svg_render_segment,
    [lv_VISUAL_LINE]    = svg_render_line,
    [lv_VISUAL_CIRCLE]  = svg_render_circle,
    [lv_VISUAL_POLYGON] = svg_render_polygon,
};

/** 递归渲染单个对象为 SVG */
static void svg_render_object(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    if (obj == NULL)
        return;

    /* 组合对象：递归渲染子对象 */
    if (obj->type == lv_VISUAL_MOBJECT_GROUP) {
        for (size_t i = 0; i < obj->children_count; i++) {
            svg_render_object(fp, obj->children[i], scene, depth + 1);
        }
        return;
    }

    LV_DISPATCH_VOID(svg_render_table, obj->type, fp, obj, scene, depth);
}

/** SVG 渲染入口 */
static void svg_render_scene(FILE *fp, const lvVisualRenderer *renderer, const lvVisualScene *scene) {
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" "
            "width=\"%d\" height=\"%d\" "
            "viewBox=\"0 0 %d %d\">\n",
            renderer->width, renderer->height, renderer->width, renderer->height);
    fprintf(fp, "  <rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n");

    for (size_t i = 0; i < scene->object_count; i++) {
        svg_render_object(fp, scene->objects[i], scene, 1);
    }

    fprintf(fp, "</svg>\n");
}

/* ========================================================================
 * Cairo 渲染（生成可编译的 Cairo C 脚本）
 * ======================================================================== */

/** 生成 Cairo 颜色设置代码 */
static void cairo_write_color(FILE *fp, const float *color, float opacity, const char *indent, const char *call) {
    fprintf(fp, "%scairo_set_source_rgba(%s, %.3f, %.3f, %.3f, %.3f);\n", indent, call, (double) color[0],
            (double) color[1], (double) color[2], (double) (color[3] * opacity));
}

/** 生成 Cairo 样式设置代码 */
static void cairo_write_style(FILE *fp, const lvVisualStyle *s, const char *indent) {
    fprintf(fp, "%scairo_set_line_width(cr, %.2f);\n", indent, (double) s->stroke_width);
    if (s->dashed) {
        fprintf(fp, "%sconst double dashes[] = {5.0, 5.0};\n", indent);
        fprintf(fp, "%scairo_set_dash(cr, dashes, 2, 0.0);\n", indent);
    } else {
        fprintf(fp, "%scairo_set_dash(cr, NULL, 0, 0.0);\n", indent);
    }
}

/* ── Cairo 渲染处理器函数 ── */
typedef void (*CairoObjHandler)(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth);

static void cairo_render_point(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    (void)depth;
    float cache[8];
    memset(cache, 0, sizeof(cache));
    const char *ind = "    ";
    if (!get_float_cache(obj, cache, 2))
        return;
    float px = cache[0], py = cache[1];
    apply_camera(&px, &py, scene);
    float r = obj->style.stroke_width > 2.0f ? obj->style.stroke_width : 3.0f;
    cairo_write_color(fp, obj->style.stroke_color, obj->style.opacity, ind, "cr");
    fprintf(fp, "%scairo_arc(cr, %.2f, %.2f, %.2f, 0, 2 * M_PI);\n", ind, (double) px, (double) py, (double) r);
    fprintf(fp, "%scairo_fill(cr);\n", ind);
}

static void cairo_render_segment(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    (void)depth;
    float cache[8];
    memset(cache, 0, sizeof(cache));
    const char *ind = "    ";
    if (!get_float_cache(obj, cache, 4))
        return;
    float x1 = cache[0], y1 = cache[1];
    float x2 = cache[2], y2 = cache[3];
    apply_camera(&x1, &y1, scene);
    apply_camera(&x2, &y2, scene);
    cairo_write_color(fp, obj->style.stroke_color, obj->style.opacity, ind, "cr");
    cairo_write_style(fp, &obj->style, ind);
    fprintf(fp, "%scairo_move_to(cr, %.2f, %.2f);\n", ind, (double) x1, (double) y1);
    fprintf(fp, "%scairo_line_to(cr, %.2f, %.2f);\n", ind, (double) x2, (double) y2);
    fprintf(fp, "%scairo_stroke(cr);\n", ind);
}

static void cairo_render_line(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    (void)depth;
    float cache[8];
    memset(cache, 0, sizeof(cache));
    const char *ind = "    ";
    if (!get_float_cache(obj, cache, 4))
        return;
    float x1 = cache[0], y1 = cache[1];
    float x2 = cache[2], y2 = cache[3];
    float dx = x2 - x1, dy = y2 - y1;
    /* exempt: float 渲染精度 2D 归一化（sqrtf/1e-6f），渲染输出层 float 语义，保持原样。 */
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f)
        return;
    float ux = dx / len, uy = dy / len;
    float t_max = 1000.0f;
    float lx1 = x1 - ux * t_max, ly1 = y1 - uy * t_max;
    float lx2 = x1 + ux * t_max, ly2 = y1 + uy * t_max;
    apply_camera(&lx1, &ly1, scene);
    apply_camera(&lx2, &ly2, scene);
    cairo_write_color(fp, obj->style.stroke_color, obj->style.opacity, ind, "cr");
    cairo_write_style(fp, &obj->style, ind);
    fprintf(fp, "%scairo_move_to(cr, %.2f, %.2f);\n", ind, (double) lx1, (double) ly1);
    fprintf(fp, "%scairo_line_to(cr, %.2f, %.2f);\n", ind, (double) lx2, (double) ly2);
    fprintf(fp, "%scairo_stroke(cr);\n", ind);
}

static void cairo_render_circle(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    (void)depth;
    float cache[8];
    memset(cache, 0, sizeof(cache));
    const char *ind = "    ";
    if (!get_float_cache(obj, cache, 3))
        return;
    float cx = cache[0], cy = cache[1], r = cache[2];
    apply_camera(&cx, &cy, scene);
    cairo_write_color(fp, obj->style.stroke_color, obj->style.opacity, ind, "cr");
    cairo_write_style(fp, &obj->style, ind);
    fprintf(fp, "%scairo_arc(cr, %.2f, %.2f, %.2f, 0, 2 * M_PI);\n", ind, (double) cx, (double) cy, (double) r);
    fprintf(fp, "%scairo_stroke_preserve(cr);\n", ind);
    if (obj->style.fill_color[3] > 0.0f) {
        cairo_write_color(fp, obj->style.fill_color, obj->style.opacity, ind, "cr");
        fprintf(fp, "%scairo_fill(cr);\n", ind);
    } else {
        fprintf(fp, "%scairo_new_path(cr);\n", ind);
    }
}

static void cairo_render_polygon(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    (void)depth;
    const char *ind = "    ";
    if (obj->render_cache == NULL)
        return;
    int pcount = ((int *) obj->render_cache)[0];
    float *verts = (float *) ((int *) obj->render_cache + 1);
    if (pcount < 3)
        return;
    cairo_write_color(fp, obj->style.stroke_color, obj->style.opacity, ind, "cr");
    cairo_write_style(fp, &obj->style, ind);
    float vx0 = verts[0], vy0 = verts[1];
    apply_camera(&vx0, &vy0, scene);
    fprintf(fp, "%scairo_move_to(cr, %.2f, %.2f);\n", ind, (double) vx0, (double) vy0);
    for (int j = 1; j < pcount; j++) {
        float vx = verts[j * 2], vy = verts[j * 2 + 1];
        apply_camera(&vx, &vy, scene);
        fprintf(fp, "%scairo_line_to(cr, %.2f, %.2f);\n", ind, (double) vx, (double) vy);
    }
    fprintf(fp, "%scairo_close_path(cr);\n", ind);
    fprintf(fp, "%scairo_stroke_preserve(cr);\n", ind);
    if (obj->style.fill_color[3] > 0.0f) {
        cairo_write_color(fp, obj->style.fill_color, obj->style.opacity, ind, "cr");
        fprintf(fp, "%scairo_fill(cr);\n", ind);
    } else {
        fprintf(fp, "%scairo_new_path(cr);\n", ind);
    }
}

static const CairoObjHandler cairo_render_table[lv_VISUAL_MOBJECT_GROUP] = {
    [lv_VISUAL_POINT]   = cairo_render_point,
    [lv_VISUAL_SEGMENT] = cairo_render_segment,
    [lv_VISUAL_LINE]    = cairo_render_line,
    [lv_VISUAL_CIRCLE]  = cairo_render_circle,
    [lv_VISUAL_POLYGON] = cairo_render_polygon,
};

/** 递归生成单个对象的 Cairo 绘制代码 */
static void cairo_render_object(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth) {
    if (obj == NULL)
        return;

    if (obj->type == lv_VISUAL_MOBJECT_GROUP) {
        for (size_t i = 0; i < obj->children_count; i++) {
            cairo_render_object(fp, obj->children[i], scene, depth + 1);
        }
        return;
    }

    LV_DISPATCH_VOID(cairo_render_table, obj->type, fp, obj, scene, depth);
}

/** Cairo 渲染入口：生成可编译的 `.c` 脚本 */
static void cairo_render_scene(FILE *fp, const lvVisualRenderer *renderer, const lvVisualScene *scene) {
    fprintf(fp, "/* Generated by Lv-00 Geometry Visualizer -- Cairo script */\n");
    fprintf(fp, "#include <cairo.h>\n");
    fprintf(fp, "#include <math.h>\n");
    fprintf(fp, "#include <stdio.h>\n\n");

    fprintf(fp, "int main(void) {\n");
    fprintf(fp,
            "    cairo_surface_t *surface = "
            "cairo_image_surface_create(CAIRO_FORMAT_ARGB32, %d, %d);\n",
            renderer->width, renderer->height);
    fprintf(fp, "    cairo_t *cr = cairo_create(surface);\n\n");
    /* 白色背景 */
    fprintf(fp, "    /* 白色背景 */\n");
    fprintf(fp, "    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);\n");
    fprintf(fp, "    cairo_paint(cr);\n\n");
    /* 相机平移 + 缩放 */
    fprintf(fp, "    /* 相机变换 */\n");
    fprintf(fp, "    cairo_translate(cr, %.2f, %.2f);\n", (double) -scene->camera_center[0],
            (double) -scene->camera_center[1]);
    fprintf(fp, "    cairo_scale(cr, %.2f, %.2f);\n", (double) scene->camera_zoom, (double) scene->camera_zoom);
    fprintf(fp, "    cairo_translate(cr, %.2f, %.2f);\n\n", (double) scene->camera_center[0],
            (double) scene->camera_center[1]);
    /* 渲染对象 */
    fprintf(fp, "    /* 场景对象 */\n");
    for (size_t i = 0; i < scene->object_count; i++) {
        cairo_render_object(fp, scene->objects[i], scene, 1);
    }
    fprintf(fp, "\n");
    /* 输出并清理 */
    fprintf(fp, "    cairo_surface_write_to_png(surface, \"output.png\");\n");
    fprintf(fp, "    cairo_destroy(cr);\n");
    fprintf(fp, "    cairo_surface_destroy(surface);\n");
    fprintf(fp, "    return 0;\n");
    fprintf(fp, "}\n");
}

/* ========================================================================
 * Three.js 渲染（生成交互式 HTML / JavaScript）
 * ======================================================================== */

/* ── Three.js 渲染处理器函数 ── */
typedef void (*ThreejsObjHandler)(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth,
                                  const char *parent_var);

static void threejs_render_point(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth,
                                 const char *parent_var) {
    (void)depth;
    const char *ind = "        ";
    float cache[8];
    memset(cache, 0, sizeof(cache));
    if (!get_float_cache(obj, cache, 2))
        return;
    float px = cache[0], py = cache[1];
    apply_camera(&px, &py, scene);
    float r = obj->style.stroke_width > 2.0f ? obj->style.stroke_width : 3.0f;
    fprintf(fp, "%s{\n", ind);
    fprintf(fp, "%s    const geo = new THREE.SphereGeometry(%.2f, 16, 16);\n", ind, (double) (r * 0.5f));
    fprintf(fp, "%s    const mat = new THREE.MeshBasicMaterial({ color: new THREE.Color(%.3f, %.3f, %.3f) });\n",
            ind, (double) obj->style.stroke_color[0], (double) obj->style.stroke_color[1],
            (double) obj->style.stroke_color[2]);
    fprintf(fp, "%s    const mesh = new THREE.Mesh(geo, mat);\n", ind);
    fprintf(fp, "%s    mesh.position.set(%.2f, %.2f, 0);\n", ind, (double) px, (double) py);
    fprintf(fp, "%s    %s.add(mesh);\n", ind, parent_var);
    fprintf(fp, "%s}\n", ind);
}

static void threejs_render_segment(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth,
                                   const char *parent_var) {
    (void)depth;
    const char *ind = "        ";
    float cache[8];
    memset(cache, 0, sizeof(cache));
    if (!get_float_cache(obj, cache, 4))
        return;
    float x1 = cache[0], y1 = cache[1];
    float x2 = cache[2], y2 = cache[3];
    apply_camera(&x1, &y1, scene);
    apply_camera(&x2, &y2, scene);
    float mx = (x1 + x2) * 0.5f, my = (y1 + y2) * 0.5f;
    float dx = x2 - x1, dy = y2 - y1;
    /* exempt: float 渲染精度 2D 归一化（sqrtf/1e-6f），渲染输出层 float 语义，保持原样。 */
    float length = sqrtf(dx * dx + dy * dy);
    if (length < 1e-6f)
        return;
    float angle = atan2f(dy, dx);
    fprintf(fp, "%s{\n", ind);
    fprintf(fp, "%s    const geo = new THREE.PlaneGeometry(%.2f, %.2f);\n", ind, (double) length,
            (double) obj->style.stroke_width);
    fprintf(fp, "%s    const mat = new THREE.MeshBasicMaterial({ color: new THREE.Color(%.3f, %.3f, %.3f), "
            "side: THREE.DoubleSide, transparent: true, opacity: %.2f });\n",
            ind, (double) obj->style.stroke_color[0], (double) obj->style.stroke_color[1],
            (double) obj->style.stroke_color[2], (double) (obj->style.stroke_color[3] * obj->style.opacity));
    fprintf(fp, "%s    const mesh = new THREE.Mesh(geo, mat);\n", ind);
    fprintf(fp, "%s    mesh.position.set(%.2f, %.2f, 0);\n", ind, (double) mx, (double) my);
    fprintf(fp, "%s    mesh.rotation.z = %.6f;\n", ind, (double) angle);
    fprintf(fp, "%s    %s.add(mesh);\n", ind, parent_var);
    fprintf(fp, "%s}\n", ind);
}

static void threejs_render_line(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth,
                                const char *parent_var) {
    (void)depth;
    const char *ind = "        ";
    float cache[8];
    memset(cache, 0, sizeof(cache));
    if (!get_float_cache(obj, cache, 4))
        return;
    float x1 = cache[0], y1 = cache[1];
    float x2 = cache[2], y2 = cache[3];
    float dx = x2 - x1, dy = y2 - y1;
    /* exempt: float 渲染精度 2D 归一化（sqrtf/1e-6f），渲染输出层 float 语义，保持原样。 */
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f)
        return;
    float ux = dx / len, uy = dy / len;
    float t_max = 500.0f;
    float lx1 = x1 - ux * t_max, ly1 = y1 - uy * t_max;
    float lx2 = x1 + ux * t_max, ly2 = y1 + uy * t_max;
    apply_camera(&lx1, &ly1, scene);
    apply_camera(&lx2, &ly2, scene);
    float mx = (lx1 + lx2) * 0.5f, my = (ly1 + ly2) * 0.5f;
    float ldx = lx2 - lx1, ldy = ly2 - ly1;
    float llength = sqrtf(ldx * ldx + ldy * ldy);
    float lang = atan2f(ldy, ldx);
    fprintf(fp, "%s{\n", ind);
    fprintf(fp, "%s    const geo = new THREE.PlaneGeometry(%.2f, %.2f);\n", ind, (double) llength,
            (double) obj->style.stroke_width);
    fprintf(fp, "%s    const mat = new THREE.MeshBasicMaterial({ color: new THREE.Color(%.3f, %.3f, %.3f), "
            "side: THREE.DoubleSide, transparent: true, opacity: %.2f });\n",
            ind, (double) obj->style.stroke_color[0], (double) obj->style.stroke_color[1],
            (double) obj->style.stroke_color[2], (double) (obj->style.stroke_color[3] * obj->style.opacity));
    fprintf(fp, "%s    const mesh = new THREE.Mesh(geo, mat);\n", ind);
    fprintf(fp, "%s    mesh.position.set(%.2f, %.2f, 0);\n", ind, (double) mx, (double) my);
    fprintf(fp, "%s    mesh.rotation.z = %.6f;\n", ind, (double) lang);
    fprintf(fp, "%s    %s.add(mesh);\n", ind, parent_var);
    fprintf(fp, "%s}\n", ind);
}

static void threejs_render_circle(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth,
                                  const char *parent_var) {
    (void)depth;
    const char *ind = "        ";
    float cache[8];
    memset(cache, 0, sizeof(cache));
    if (!get_float_cache(obj, cache, 3))
        return;
    float cx = cache[0], cy = cache[1], r = cache[2];
    apply_camera(&cx, &cy, scene);
    fprintf(fp, "%s{\n", ind);
    float inner_r = (obj->style.fill_color[3] > 0.0f) ? 0.0f : (r - 1.0f);
    if (inner_r < 0.0f)
        inner_r = 0.0f;
    fprintf(fp, "%s    const geo = new THREE.RingGeometry(%.2f, %.2f, 64);\n", ind, (double) inner_r, (double) r);
    float sr = obj->style.stroke_color[0];
    float sg = obj->style.stroke_color[1];
    float sb = obj->style.stroke_color[2];
    float sa = obj->style.stroke_color[3] * obj->style.opacity;
    fprintf(fp, "%s    const mat = new THREE.MeshBasicMaterial({ color: new THREE.Color(%.3f, %.3f, %.3f), "
            "side: THREE.DoubleSide, transparent: true, opacity: %.2f });\n",
            ind, (double) sr, (double) sg, (double) sb, (double) sa);
    if (obj->style.fill_color[3] > 0.0f) {
        fprintf(fp, "%s    // 填充色: (%.3f, %.3f, %.3f)\n", ind, (double) obj->style.fill_color[0],
                (double) obj->style.fill_color[1], (double) obj->style.fill_color[2]);
    }
    fprintf(fp, "%s    const mesh = new THREE.Mesh(geo, mat);\n", ind);
    fprintf(fp, "%s    mesh.position.set(%.2f, %.2f, 0);\n", ind, (double) cx, (double) cy);
    fprintf(fp, "%s    %s.add(mesh);\n", ind, parent_var);
    fprintf(fp, "%s}\n", ind);
}

static void threejs_render_polygon(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth,
                                   const char *parent_var) {
    (void)depth;
    const char *ind = "        ";
    if (obj->render_cache == NULL)
        return;
    int pcount = ((int *) obj->render_cache)[0];
    float *verts = (float *) ((int *) obj->render_cache + 1);
    if (pcount < 3)
        return;
    fprintf(fp, "%s{\n", ind);
    fprintf(fp, "%s    const shape = new THREE.Shape();\n", ind);
    float vx0 = verts[0], vy0 = verts[1];
    apply_camera(&vx0, &vy0, scene);
    fprintf(fp, "%s    shape.moveTo(%.2f, %.2f);\n", ind, (double) vx0, (double) vy0);
    for (int j = 1; j < pcount; j++) {
        float vx = verts[j * 2], vy = verts[j * 2 + 1];
        apply_camera(&vx, &vy, scene);
        fprintf(fp, "%s    shape.lineTo(%.2f, %.2f);\n", ind, (double) vx, (double) vy);
    }
    fprintf(fp, "%s    shape.closePath();\n", ind);
    fprintf(fp, "%s    const geo = new THREE.ShapeGeometry(shape);\n", ind);
    float sr = obj->style.stroke_color[0];
    float sg = obj->style.stroke_color[1];
    float sb = obj->style.stroke_color[2];
    float sa = obj->style.stroke_color[3] * obj->style.opacity;
    fprintf(fp, "%s    const mat = new THREE.MeshBasicMaterial({ color: new THREE.Color(%.3f, %.3f, %.3f), "
            "side: THREE.DoubleSide, transparent: true, opacity: %.2f });\n",
            ind, (double) sr, (double) sg, (double) sb, (double) sa);
    fprintf(fp, "%s    const mesh = new THREE.Mesh(geo, mat);\n", ind);
    fprintf(fp, "%s    mesh.position.z = 0;\n", ind);
    fprintf(fp, "%s    %s.add(mesh);\n", ind, parent_var);
    fprintf(fp, "%s}\n", ind);
}

static const ThreejsObjHandler threejs_render_table[lv_VISUAL_MOBJECT_GROUP] = {
    [lv_VISUAL_POINT]   = threejs_render_point,
    [lv_VISUAL_SEGMENT] = threejs_render_segment,
    [lv_VISUAL_LINE]    = threejs_render_line,
    [lv_VISUAL_CIRCLE]  = threejs_render_circle,
    [lv_VISUAL_POLYGON] = threejs_render_polygon,
};

/** 递归生成单个对象的 Three.js JavaScript 代码 */
static void threejs_render_object(FILE *fp, const lvVisualObject *obj, const lvVisualScene *scene, int depth,
                                  const char *parent_var) {
    if (obj == NULL)
        return;
    const char *ind = "        ";

    if (obj->type == lv_VISUAL_MOBJECT_GROUP) {
        fprintf(fp, "%s{\n", ind);
        fprintf(fp, "%s    const grp = new THREE.Group();\n", ind);
        for (size_t i = 0; i < obj->children_count; i++) {
            threejs_render_object(fp, obj->children[i], scene, depth + 1, "grp");
        }
        fprintf(fp, "%s    %s.add(grp);\n", ind, parent_var);
        fprintf(fp, "%s}\n", ind);
        return;
    }

    LV_DISPATCH_VOID(threejs_render_table, obj->type, fp, obj, scene, depth, parent_var);
}

/** Three.js 渲染入口：生成交互式 HTML 文件 */
static void threejs_render_scene(FILE *fp, const lvVisualRenderer *renderer, const lvVisualScene *scene) {
    fprintf(fp, "<!DOCTYPE html>\n");
    fprintf(fp, "<html lang=\"zh-CN\">\n");
    fprintf(fp, "<head>\n");
    fprintf(fp, "<meta charset=\"UTF-8\">\n");
    fprintf(fp, "<title>Lv-00 几何可视化 - Three.js</title>\n");
    fprintf(fp, "<style>\n");
    fprintf(fp, "  body { margin: 0; overflow: hidden; background: #fff; }\n");
    fprintf(fp, "</style>\n");
    fprintf(fp, "</head>\n");
    fprintf(fp, "<body>\n");
    /* Three.js 和 OrbitControls 从 CDN 加载 */
    fprintf(fp, "<script type=\"importmap\">\n");
    fprintf(fp, "{\n");
    fprintf(fp, "  \"imports\": {\n");
    fprintf(fp, "    \"three\": \"https://cdn.jsdelivr.net/npm/three@0.170.0/build/three.module.js\",\n");
    fprintf(fp, "    \"three/addons/\": \"https://cdn.jsdelivr.net/npm/three@0.170.0/examples/jsm/\"\n");
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");
    fprintf(fp, "</script>\n");
    fprintf(fp, "\n");
    fprintf(fp, "<script type=\"module\">\n");
    fprintf(fp, "import * as THREE from 'three';\n");
    fprintf(fp, "import { OrbitControls } from 'three/addons/controls/OrbitControls.js';\n");
    fprintf(fp, "\n");
    /* 场景设置 */
    fprintf(fp, "const scene = new THREE.Scene();\n");
    fprintf(fp, "scene.background = new THREE.Color(0xffffff);\n");
    fprintf(fp, "\n");
    fprintf(fp, "const camera = new THREE.OrthographicCamera(\n");
    fprintf(fp, "    %d / -2.0, %d / 2.0, %d / 2.0, %d / -2.0, 0.1, 5000);\n", renderer->width, renderer->width,
            renderer->height, renderer->height);
    /* 根据场景 is_3d 选择视角 */
    if (scene->is_3d) {
        fprintf(fp, "camera.position.set(%d, %d, %d);\n", renderer->width / 2, renderer->height / 2, 500);
    } else {
        fprintf(fp, "camera.position.set(0, 0, 1000);\n");
    }
    fprintf(fp, "camera.lookAt(0, 0, 0);\n");
    fprintf(fp, "\n");
    fprintf(fp, "const renderer = new THREE.WebGLRenderer({ antialias: true });\n");
    fprintf(fp, "renderer.setSize(%d, %d);\n", renderer->width, renderer->height);
    fprintf(fp, "renderer.setPixelRatio(window.devicePixelRatio);\n");
    fprintf(fp, "document.body.appendChild(renderer.domElement);\n");
    fprintf(fp, "\n");
    fprintf(fp, "const controls = new OrbitControls(camera, renderer.domElement);\n");
    fprintf(fp, "controls.enableDamping = true;\n");
    fprintf(fp, "controls.dampingFactor = 0.05;\n");
    fprintf(fp, "\n");
    /* 坐标辅助线（仅在 2D 时显示） */
    if (!scene->is_3d) {
        fprintf(fp, "// 坐标轴辅助\n");
        fprintf(fp, "const axesHelper = new THREE.AxesHelper(200);\n");
        fprintf(fp, "scene.add(axesHelper);\n");
        fprintf(fp, "\n");
    }
    /* 场景根节点 */
    fprintf(fp, "const root = new THREE.Group();\n");
    fprintf(fp, "scene.add(root);\n");
    fprintf(fp, "\n");
    /* 渲染所有对象 */
    fprintf(fp, "// === 几何对象 ===\n");
    for (size_t i = 0; i < scene->object_count; i++) {
        threejs_render_object(fp, scene->objects[i], scene, 1, "root");
    }
    fprintf(fp, "\n");
    /* 动画循环 */
    fprintf(fp, "function animate() {\n");
    fprintf(fp, "    requestAnimationFrame(animate);\n");
    fprintf(fp, "    controls.update();\n");
    fprintf(fp, "    renderer.render(scene, camera);\n");
    fprintf(fp, "}\n");
    fprintf(fp, "animate();\n");
    fprintf(fp, "</script>\n");
    fprintf(fp, "</body>\n");
    fprintf(fp, "</html>\n");
}

/* ========================================================================
 * 渲染器（公共 API）
 * ======================================================================== */

lvVisualRenderer *lv_visual_renderer_create(lvRenderBackend backend, int width, int height) {
    lvVisualRenderer *renderer = (lvVisualRenderer *) lv_calloc(1, sizeof(lvVisualRenderer));
    if (renderer == NULL)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_visual_renderer_create: lv_calloc failed");

    renderer->backend = backend;
    renderer->backend_ctx = NULL;
    renderer->dpi = 96.0f;
    renderer->width = (width > 0) ? width : 800;
    renderer->height = (height > 0) ? height : 600;

    return renderer;
}

/* ── 最小 PNG 编码器（无需外部库，输出有效 PNG） ── */

/** @brief PNG CRC 编码器状态 */
typedef struct {
    uint32_t table[256]; /**< CRC-32 查找表 */
    bool initialized;    /**< 表是否已初始化 */
} PngCrcState;

/** @brief PNG CRC 编码器全局单例 */
static PngCrcState s_png_crc = {0};

static void png_init_crc(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1)
                c = 0xEDB88320U ^ (c >> 1);
            else
                c >>= 1;
        }
        s_png_crc.table[i] = c;
    }
    s_png_crc.initialized = true;
}

static uint32_t png_crc(const uint8_t *data, size_t len) {
    if (!s_png_crc.initialized)
        png_init_crc();
    uint32_t c = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++)
        c = s_png_crc.table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFU;
}

/** 以网络字节序（大端）写入 32 位整数 */
static void png_write_be32(uint8_t *buf, uint32_t v) {
    buf[0] = (uint8_t) (v >> 24);
    buf[1] = (uint8_t) (v >> 16);
    buf[2] = (uint8_t) (v >> 8);
    buf[3] = (uint8_t) (v);
}

/** 写入 PNG chunk: length + type + data + crc */
static bool png_write_chunk(FILE *fp, const char *type, const uint8_t *data, uint32_t data_len) {
    uint8_t hdr[8];
    uint8_t type_buf[4];
    png_write_be32(hdr, data_len);
    type_buf[0] = (uint8_t) type[0];
    type_buf[1] = (uint8_t) type[1];
    type_buf[2] = (uint8_t) type[2];
    type_buf[3] = (uint8_t) type[3];

    if (fwrite(hdr, 4, 1, fp) != 1)
        return false;
    if (fwrite(type_buf, 4, 1, fp) != 1)
        return false;

    uint32_t crc_val = png_crc(type_buf, 4);
    if (data && data_len > 0) {
        if (fwrite(data, 1, data_len, fp) != data_len)
            return false;
        /* CRC 是 type + data 的校验 */
        uint8_t *combined = (uint8_t *) lv_malloc(4 + data_len);
        if (!combined)
            return false;
        memcpy(combined, type_buf, 4);
        memcpy(combined + 4, data, data_len);
        crc_val = png_crc(combined, 4 + data_len);
        lv_free((void **) &combined);
    }

    uint8_t crc_buf[4];
    png_write_be32(crc_buf, crc_val);
    if (fwrite(crc_buf, 4, 1, fp) != 1)
        return false;
    return true;
}

/** 将 RGB 像素数据编码为 PNG 文件 */
static bool write_png_rgb(const char *path, int width, int height, const uint8_t *rgb_data) {
    FILE *fp = lv_file_open(path, "wb");
    if (!fp)
        return false;

    /* PNG 签名 */
    const uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (fwrite(sig, 8, 1, fp) != 1) {
        lv_file_close(fp);
        return false;
    }

    /* IHDR chunk */
    uint8_t ihdr[13];
    png_write_be32(ihdr, (uint32_t) width);
    png_write_be32(ihdr + 4, (uint32_t) height);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 2;  /* color type: RGB */
    ihdr[10] = 0; /* compression */
    ihdr[11] = 0; /* filter */
    ihdr[12] = 0; /* interlace */
    if (!png_write_chunk(fp, "IHDR", ihdr, 13)) {
        lv_file_close(fp);
        return false;
    }

    /* 构建 IDAT 数据：每行前加 filter byte=0 (None)，然后 raw RGB */
    int row_bytes = width * 3 + 1;
    size_t raw_size = (size_t) row_bytes * (size_t) height;
    uint8_t *raw = (uint8_t *) lv_malloc(raw_size);
    if (!raw) {
        lv_file_close(fp);
        return false;
    }

    for (int y = 0; y < height; y++) {
        raw[y * row_bytes] = 0;
        memcpy(raw + y * row_bytes + 1, rgb_data + (size_t) y * width * 3, (size_t) width * 3);
    }

    /* 构建 DEFLATE stored block: BFINAL=1, BTYPE=00, LEN, NLEN, data */
    /* 对于超出 65535 字节的大图像，使用多块 */
    /* overflow check: raw_size + 5 + 64 must not wrap */
    if (raw_size > SIZE_MAX - 69) {
        lv_free((void **) &raw);
        lv_file_close(fp);
        return false;
    }
    size_t raw_remaining = raw_size;
    size_t idat_capacity = raw_size + 69;
    uint8_t *idat_buf = (uint8_t *) lv_malloc(idat_capacity);
    if (!idat_buf) {
        lv_free((void **) &raw);
        lv_file_close(fp);
        return false;
    }

    size_t idat_pos = 0;
    while (raw_remaining > 0) {
        uint32_t blk_len = (raw_remaining > 65535) ? 65535 : (uint32_t) raw_remaining;
        if (idat_pos + 5 + blk_len > idat_capacity) {
            idat_capacity = idat_pos + 5 + blk_len + 64;
            uint8_t *nb = lv_realloc(idat_buf, idat_capacity);
            if (!nb) {
                lv_free((void **) &idat_buf);
                lv_free((void **) &raw);
                lv_file_close(fp);
                return false;
            }
            idat_buf = nb;
        }
        bool is_last = (raw_remaining <= 65535);
        idat_buf[idat_pos++] = is_last ? 0x01 : 0x00;
        /* zlib 块头为小端序 16 位长度字段 */
        lv_store_le16(idat_buf + idat_pos, (uint16_t) blk_len);
        idat_pos += 2;
        uint16_t nlen = (uint16_t) (~blk_len);
        lv_store_le16(idat_buf + idat_pos, nlen);
        idat_pos += 2;
        memcpy(idat_buf + idat_pos, raw + (raw_size - raw_remaining), blk_len);
        idat_pos += blk_len;
        raw_remaining -= blk_len;
    }
    lv_free((void **) &raw);

    if (!png_write_chunk(fp, "IDAT", idat_buf, (uint32_t) idat_pos)) {
        lv_free((void **) &idat_buf);
        lv_file_close(fp);
        return false;
    }
    lv_free((void **) &idat_buf);

    /* IEND chunk */
    if (!png_write_chunk(fp, "IEND", NULL, 0)) {
        lv_file_close(fp);
        return false;
    }

    lv_file_close(fp);
    return true;
}

/* ── 后端场景渲染函数表 ── */
typedef void (*SceneRenderFn)(FILE *fp, const lvVisualRenderer *renderer, const lvVisualScene *scene);

static void tikz_render_visitor(const lvVisualRenderer *renderer, const lvVisualScene *scene, const char *output_path) {
    lvRenderVisitor visitor;
    if (lv_render_visitor_tikz_create(output_path, &visitor)) {
        lv_render_scene(&visitor, scene);
        lv_render_visitor_tikz_destroy(&visitor);
    }
}

static void png_render_fallback(const lvVisualRenderer *renderer, const char *output_path) {
    size_t px = (size_t) renderer->width * (size_t) renderer->height;
    size_t rgb_size = px * 3;
    uint8_t *rgb = (uint8_t *) lv_malloc(rgb_size);
    if (rgb) {
        memset(rgb, 255, rgb_size);
        write_png_rgb(output_path, renderer->width, renderer->height, rgb);
        lv_free((void **) &rgb);
    }
}

static const SceneRenderFn scene_render_table[lv_RENDER_PNG] = {
    [lv_RENDER_SVG]    = svg_render_scene,
    [lv_RENDER_CAIRO]  = cairo_render_scene,
    [lv_RENDER_THREEJS] = threejs_render_scene,
};

void lv_visual_render(lvVisualRenderer *renderer, lvVisualScene *scene, const char *output_path) {
    if (renderer == NULL || scene == NULL || output_path == NULL)
        return;

    if (renderer->backend == lv_RENDER_TIKZ) {
        tikz_render_visitor(renderer, scene, output_path);
        return;
    }

    if (renderer->backend == lv_RENDER_PNG) {
        png_render_fallback(renderer, output_path);
        return;
    }

    FILE *fp = lv_file_open(output_path, "w");
    if (fp == NULL)
        return;

    if (lv_index_in_range(renderer->backend, lv_RENDER_PNG)) {
        SceneRenderFn fn = scene_render_table[renderer->backend];
        if (fn)
            fn(fp, renderer, scene);
        else
            fprintf(fp, "// Lv-00 render output (backend=%d, %dx%d)\n", (int) renderer->backend, renderer->width,
                    renderer->height);
    }

    lv_file_close(fp);
}

/* ========================================================================
 * 资源释放
 * ======================================================================== */

/* ── lv_visual_object_destroy / lv_visual_scene_destroy 子资源销毁适配 ── */

/* 子对象元素销毁：递归委托 lv_visual_object_destroy */
static void destroy_visual_object_elem(void *elem) {
    lv_visual_object_destroy((lvVisualObject *) elem);
}

/* lv_visual_object_destroy 字段描述表：children 逐元素递归销毁后释放数组，
 * render_cache 纯指针释放；entity 不拥有（外部所有权），不入表 */
static const lvFieldDesc s_visual_object_destroy_fields[] = {
    lv_FIELD_ARRAY(lvVisualObject, children, children_count, destroy_visual_object_elem),
    lv_FIELD_PLAIN(lvVisualObject, render_cache),
};

void lv_visual_object_destroy(lvVisualObject *obj) {
    if (obj == NULL)
        return;
    lv_obj_destroy_fields(obj, s_visual_object_destroy_fields,
                          sizeof(s_visual_object_destroy_fields) / sizeof(s_visual_object_destroy_fields[0]));
    lv_free((void **) &obj);
}

/* lv_visual_scene_destroy 字段描述表：objects 逐元素递归销毁后释放数组
 * （与 lv_visual_scene_clear 行为一致，全部置 NULL 安全） */
static const lvFieldDesc s_visual_scene_destroy_fields[] = {
    lv_FIELD_ARRAY(lvVisualScene, objects, object_count, destroy_visual_object_elem),
};

void lv_visual_scene_destroy(lvVisualScene *scene) {
    if (scene == NULL)
        return;
    lv_obj_destroy_fields(scene, s_visual_scene_destroy_fields,
                          sizeof(s_visual_scene_destroy_fields) / sizeof(s_visual_scene_destroy_fields[0]));
    lv_free((void **) &scene);
}

/* lv_visual_renderer_destroy 字段描述表：backend_ctx 预留指针释放 */
static const lvFieldDesc s_visual_renderer_destroy_fields[] = {
    lv_FIELD_PLAIN(lvVisualRenderer, backend_ctx),
};

void lv_visual_renderer_destroy(lvVisualRenderer *renderer) {
    if (renderer == NULL)
        return;
    lv_obj_destroy_fields(renderer, s_visual_renderer_destroy_fields,
                          sizeof(s_visual_renderer_destroy_fields) / sizeof(s_visual_renderer_destroy_fields[0]));
    lv_free((void **) &renderer);
}
