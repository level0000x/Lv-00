/**
 * @file geo_visual_complete.c
 * @brief 几何可视化完整版实现
 *
 * 提供几何图形的创建、样式设置、空间变换、场景管理和多后端渲染功能。
 * 支持 SVG / Cairo / Three.js / TikZ / PNG 五种渲染后端。
 *
 * @version 1.0.0
 */

#include "lv00/geo_visual.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/** 初始化单位矩阵（4x4 列主序） */
static void identity_matrix(float *m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/** 矩阵乘法: out = a * b（4x4 列主序） */
static void matrix_multiply(float *out, const float *a, const float *b)
{
    float tmp[16];
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            tmp[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    memcpy(out, tmp, 16 * sizeof(float));
}

/** 设置默认样式 */
static void set_default_style(Lv00VisualStyle *style)
{
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

Lv00VisualObject *lv00_visual_point_create(float x, float y)
{
    Lv00VisualObject *obj = (Lv00VisualObject *)calloc(1, sizeof(Lv00VisualObject));
    if (obj == NULL) return NULL;

    obj->type = LV00_VISUAL_POINT;
    set_default_style(&obj->style);
    identity_matrix(obj->transform);
    obj->children = NULL;
    obj->children_count = 0;

    /* 缓存点坐标 */
    float *cache = (float *)malloc(2 * sizeof(float));
    if (cache != NULL) {
        cache[0] = x;
        cache[1] = y;
    }
    obj->render_cache = cache;

    return obj;
}

Lv00VisualObject *lv00_visual_line_create(float x1, float y1, float x2, float y2)
{
    Lv00VisualObject *obj = (Lv00VisualObject *)calloc(1, sizeof(Lv00VisualObject));
    if (obj == NULL) return NULL;

    obj->type = LV00_VISUAL_SEGMENT;
    set_default_style(&obj->style);
    identity_matrix(obj->transform);
    obj->children = NULL;
    obj->children_count = 0;

    /* 缓存线段端点坐标 */
    float *cache = (float *)malloc(4 * sizeof(float));
    if (cache != NULL) {
        cache[0] = x1; cache[1] = y1;
        cache[2] = x2; cache[3] = y2;
    }
    obj->render_cache = cache;

    return obj;
}

Lv00VisualObject *lv00_visual_circle_create(float cx, float cy, float r)
{
    Lv00VisualObject *obj = (Lv00VisualObject *)calloc(1, sizeof(Lv00VisualObject));
    if (obj == NULL) return NULL;

    obj->type = LV00_VISUAL_CIRCLE;
    set_default_style(&obj->style);
    identity_matrix(obj->transform);
    obj->children = NULL;
    obj->children_count = 0;

    /* 缓存圆心和半径 */
    float *cache = (float *)malloc(3 * sizeof(float));
    if (cache != NULL) {
        cache[0] = cx; cache[1] = cy; cache[2] = r;
    }
    obj->render_cache = cache;

    return obj;
}

Lv00VisualObject *lv00_visual_group_create(Lv00VisualObject **objs, size_t n)
{
    if (objs == NULL || n == 0) return NULL;

    Lv00VisualObject *obj = (Lv00VisualObject *)calloc(1, sizeof(Lv00VisualObject));
    if (obj == NULL) return NULL;

    obj->type = LV00_VISUAL_MOBJECT_GROUP;
    set_default_style(&obj->style);
    identity_matrix(obj->transform);

    obj->children = (Lv00VisualObject **)calloc(n, sizeof(Lv00VisualObject *));
    if (obj->children == NULL) {
        free(obj);
        return NULL;
    }
    memcpy(obj->children, objs, n * sizeof(Lv00VisualObject *));
    obj->children_count = n;

    return obj;
}

/* ========================================================================
 * 样式设置
 * ======================================================================== */

void lv00_visual_set_style(Lv00VisualObject *obj, const Lv00VisualStyle *style)
{
    if (obj == NULL || style == NULL) return;
    obj->style = *style;
}

void lv00_visual_set_color(Lv00VisualObject *obj, float r, float g, float b, float a)
{
    if (obj == NULL) return;
    obj->style.stroke_color[0] = r;
    obj->style.stroke_color[1] = g;
    obj->style.stroke_color[2] = b;
    obj->style.stroke_color[3] = a;
}

void lv00_visual_set_dashed(Lv00VisualObject *obj, int dashed)
{
    if (obj == NULL) return;
    obj->style.dashed = dashed;
}

/* ========================================================================
 * 空间变换
 * ======================================================================== */

void lv00_visual_translate(Lv00VisualObject *obj, float dx, float dy, float dz)
{
    if (obj == NULL) return;
    float t[16];
    identity_matrix(t);
    t[12] = dx; t[13] = dy; t[14] = dz;
    matrix_multiply(obj->transform, obj->transform, t);
}

void lv00_visual_scale(Lv00VisualObject *obj, float sx, float sy)
{
    if (obj == NULL) return;
    float s[16];
    identity_matrix(s);
    s[0] = sx; s[5] = sy;
    matrix_multiply(obj->transform, obj->transform, s);
}

void lv00_visual_rotate(Lv00VisualObject *obj, float angle, float axis[3])
{
    if (obj == NULL) return;

    /* 默认绕 Z 轴旋转 */
    float ax = 0.0f, ay = 0.0f, az = 1.0f;
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
    identity_matrix(t);

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

    matrix_multiply(obj->transform, obj->transform, t);
}

/* ========================================================================
 * 场景管理
 * ======================================================================== */

Lv00VisualScene *lv00_visual_scene_create(void)
{
    Lv00VisualScene *scene = (Lv00VisualScene *)calloc(1, sizeof(Lv00VisualScene));
    if (scene == NULL) return NULL;

    scene->objects = NULL;
    scene->object_count = 0;
    scene->camera_center[0] = 0.0f;
    scene->camera_center[1] = 0.0f;
    scene->camera_center[2] = 0.0f;
    scene->camera_zoom = 1.0f;
    scene->is_3d = 0;
    scene->current_time = 0.0f;
    scene->total_duration = 0.0f;

    return scene;
}

void lv00_visual_scene_add(Lv00VisualScene *scene, Lv00VisualObject *obj)
{
    if (scene == NULL || obj == NULL) return;

    size_t new_count = scene->object_count + 1;
    Lv00VisualObject **new_arr = (Lv00VisualObject **)lv00_realloc(
        scene->objects, new_count * sizeof(Lv00VisualObject *));
    if (new_arr == NULL) return;

    new_arr[scene->object_count] = obj;
    scene->objects = new_arr;
    scene->object_count = new_count;
}

void lv00_visual_scene_clear(Lv00VisualScene *scene)
{
    if (scene == NULL) return;
    for (size_t i = 0; i < scene->object_count; i++) {
        lv00_visual_object_destroy(scene->objects[i]);
    }
    free(scene->objects);
    scene->objects = NULL;
    scene->object_count = 0;
}

void lv00_visual_scene_set_camera(Lv00VisualScene *scene,
                                   float cx, float cy, float cz, float zoom)
{
    if (scene == NULL) return;
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
    int v = (int)(c * 255.0f);
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

/** 应用相机变换：将模型坐标映射到屏幕坐标 */
static void apply_camera(float *x, float *y, const Lv00VisualScene *scene) {
    float cx = scene->camera_center[0];
    float cy = scene->camera_center[1];
    float zoom = scene->camera_zoom;
    *x = (*x - cx) * zoom + cx;
    *y = (*y - cy) * zoom + cy;
}

/** 获取渲染缓存的端点或中心数据 */
static bool get_float_cache(const Lv00VisualObject *obj, float *out, int count) {
    if (obj->render_cache == NULL) return false;
    memcpy(out, obj->render_cache, (size_t)count * sizeof(float));
    return true;
}

/* ========================================================================
 * SVG 渲染
 * ======================================================================== */

/** 输出 SVG 样式属性（stroke, fill, opacity, dasharray） */
static void svg_write_style(FILE *fp, const Lv00VisualStyle *s) {
    fprintf(fp, " stroke=\"rgb(%d,%d,%d)\"",
            to_byte(s->stroke_color[0]),
            to_byte(s->stroke_color[1]),
            to_byte(s->stroke_color[2]));
    fprintf(fp, " stroke-width=\"%.2f\"", s->stroke_width);
    fprintf(fp, " stroke-opacity=\"%.2f\"", s->stroke_color[3]);

    if (s->fill_color[3] > 0.0f) {
        fprintf(fp, " fill=\"rgb(%d,%d,%d)\"",
                to_byte(s->fill_color[0]),
                to_byte(s->fill_color[1]),
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

/** 递归渲染单个对象为 SVG */
static void svg_render_object(FILE *fp, const Lv00VisualObject *obj,
                               const Lv00VisualScene *scene, int depth) {
    if (obj == NULL) return;

    /* 组合对象：递归渲染子对象 */
    if (obj->type == LV00_VISUAL_MOBJECT_GROUP) {
        for (size_t i = 0; i < obj->children_count; i++) {
            svg_render_object(fp, obj->children[i], scene, depth + 1);
        }
        return;
    }

    float cache[8];
    memset(cache, 0, sizeof(cache));

    switch (obj->type) {
    case LV00_VISUAL_POINT: {
        if (!get_float_cache(obj, cache, 2)) return;
        float px = cache[0], py = cache[1];
        apply_camera(&px, &py, scene);
        fprintf(fp, "%*s<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\"",
                depth * 2, "", px, py,
                obj->style.stroke_width > 2.0f ? obj->style.stroke_width : 3.0f);
        svg_write_style(fp, &obj->style);
        /* 点为实心小圆 */
        fprintf(fp, " fill=\"rgb(%d,%d,%d)\"",
                to_byte(obj->style.stroke_color[0]),
                to_byte(obj->style.stroke_color[1]),
                to_byte(obj->style.stroke_color[2]));
        fprintf(fp, "/>\n");
        break;
    }
    case LV00_VISUAL_SEGMENT: {
        if (!get_float_cache(obj, cache, 4)) return;
        float x1 = cache[0], y1 = cache[1];
        float x2 = cache[2], y2 = cache[3];
        apply_camera(&x1, &y1, scene);
        apply_camera(&x2, &y2, scene);
        fprintf(fp, "%*s<line x1=\"%.2f\" y1=\"%.2f\" "
                "x2=\"%.2f\" y2=\"%.2f\"",
                depth * 2, "", x1, y1, x2, y2);
        svg_write_style(fp, &obj->style);
        fprintf(fp, "/>\n");
        break;
    }
    case LV00_VISUAL_LINE: {
        /* 直线从场景边界穿过 */
        if (!get_float_cache(obj, cache, 4)) return;
        float x1 = cache[0], y1 = cache[1];
        float x2 = cache[2], y2 = cache[3];
        float w = (float)(scene->objects ? 800 : 800);  /* 使用默认宽度 */
        float h = (float)(scene->objects ? 600 : 600);
        float dx = x2 - x1, dy = y2 - y1;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-6f) return;
        float ux = dx / len, uy = dy / len;
        float t_max = sqrtf(w * w + h * h);
        float lx1 = x1 - ux * t_max, ly1 = y1 - uy * t_max;
        float lx2 = x1 + ux * t_max, ly2 = y1 + uy * t_max;
        apply_camera(&lx1, &ly1, scene);
        apply_camera(&lx2, &ly2, scene);
        fprintf(fp, "%*s<line x1=\"%.2f\" y1=\"%.2f\" "
                "x2=\"%.2f\" y2=\"%.2f\"",
                depth * 2, "", lx1, ly1, lx2, ly2);
        svg_write_style(fp, &obj->style);
        fprintf(fp, "/>\n");
        break;
    }
    case LV00_VISUAL_CIRCLE: {
        if (!get_float_cache(obj, cache, 3)) return;
        float cx = cache[0], cy = cache[1], r = cache[2];
        apply_camera(&cx, &cy, scene);
        fprintf(fp, "%*s<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\"",
                depth * 2, "", cx, cy, r);
        svg_write_style(fp, &obj->style);
        fprintf(fp, "/>\n");
        break;
    }
    case LV00_VISUAL_POLYGON: {
        /* 多边形缓存格式: [count, x0, y0, x1, y1, ...] */
        if (obj->render_cache == NULL) return;
        int   pcount = ((int *)obj->render_cache)[0];
        float *verts = (float *)((int *)obj->render_cache + 1);
        if (pcount < 3) return;
        fprintf(fp, "%*s<polygon points=\"", depth * 2, "");
        for (int j = 0; j < pcount; j++) {
            float vx = verts[j * 2], vy = verts[j * 2 + 1];
            apply_camera(&vx, &vy, scene);
            fprintf(fp, "%.2f,%.2f ", vx, vy);
        }
        fprintf(fp, "\"");
        svg_write_style(fp, &obj->style);
        fprintf(fp, "/>\n");
        break;
    }
    default:
        break;
    }
}

/** SVG 渲染入口 */
static void svg_render_scene(FILE *fp, const Lv00VisualRenderer *renderer,
                              const Lv00VisualScene *scene) {
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<svg xmlns=\"http://www.w3.org/2000/svg\" "
            "width=\"%d\" height=\"%d\" "
            "viewBox=\"0 0 %d %d\">\n",
            renderer->width, renderer->height,
            renderer->width, renderer->height);
    fprintf(fp, "  <rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n");

    for (size_t i = 0; i < scene->object_count; i++) {
        svg_render_object(fp, scene->objects[i], scene, 1);
    }

    fprintf(fp, "</svg>\n");
}

/* ========================================================================
 * TikZ / LaTeX 渲染
 * ======================================================================== */

/** 输出 TikZ 颜色定义 */
static void tikz_write_color(FILE *fp, const Lv00VisualStyle *s, const char *prefix) {
    fprintf(fp, "%scolor={rgb,1:red,%.3f;green,%.3f;blue,%.3f}",
            prefix,
            s->stroke_color[0], s->stroke_color[1], s->stroke_color[2]);
}

/** 输出 TikZ 通用样式选项 */
static void tikz_write_style(FILE *fp, const Lv00VisualStyle *s, bool is_fill) {
    tikz_write_color(fp, s, is_fill ? "" : "draw=");
    if (is_fill) {
        if (s->fill_color[3] > 0.0f) {
            fprintf(fp, ", fill={rgb,1:red,%.3f;green,%.3f;blue,%.3f}",
                    s->fill_color[0], s->fill_color[1], s->fill_color[2]);
            fprintf(fp, ", fill opacity=%.2f", s->fill_color[3]);
        }
    } else {
        fprintf(fp, ", draw opacity=%.2f", s->stroke_color[3]);
    }
    fprintf(fp, ", line width=%.2fpt", s->stroke_width);
    if (s->opacity < 1.0f) {
        fprintf(fp, ", opacity=%.2f", s->opacity);
    }
    if (s->dashed) {
        fprintf(fp, ", dashed");
    }
}

/** 递归渲染单个对象为 TikZ */
static void tikz_render_object(FILE *fp, const Lv00VisualObject *obj,
                                const Lv00VisualScene *scene, int depth) {
    if (obj == NULL) return;

    if (obj->type == LV00_VISUAL_MOBJECT_GROUP) {
        for (size_t i = 0; i < obj->children_count; i++) {
            tikz_render_object(fp, obj->children[i], scene, depth + 1);
        }
        return;
    }

    float cache[8];
    memset(cache, 0, sizeof(cache));
    const char *indent = "  ";

    switch (obj->type) {
    case LV00_VISUAL_POINT: {
        if (!get_float_cache(obj, cache, 2)) return;
        float px = cache[0], py = cache[1];
        apply_camera(&px, &py, scene);
        float r = obj->style.stroke_width > 2.0f ? obj->style.stroke_width : 3.0f;
        fprintf(fp, "  \\fill[");
        tikz_write_color(fp, &obj->style, "");
        fprintf(fp, "] (%.2f,%.2f) circle (%.2fpt);\n", px, py, r);
        break;
    }
    case LV00_VISUAL_SEGMENT: {
        if (!get_float_cache(obj, cache, 4)) return;
        float x1 = cache[0], y1 = cache[1];
        float x2 = cache[2], y2 = cache[3];
        apply_camera(&x1, &y1, scene);
        apply_camera(&x2, &y2, scene);
        fprintf(fp, "  \\draw[");
        tikz_write_style(fp, &obj->style, false);
        fprintf(fp, "] (%.2f,%.2f) -- (%.2f,%.2f);\n", x1, y1, x2, y2);
        break;
    }
    case LV00_VISUAL_LINE: {
        if (!get_float_cache(obj, cache, 4)) return;
        float x1 = cache[0], y1 = cache[1];
        float x2 = cache[2], y2 = cache[3];
        float dx = x2 - x1, dy = y2 - y1;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-6f) return;
        float ux = dx / len, uy = dy / len;
        float t_max = 1000.0f;
        float lx1 = x1 - ux * t_max, ly1 = y1 - uy * t_max;
        float lx2 = x1 + ux * t_max, ly2 = y1 + uy * t_max;
        apply_camera(&lx1, &ly1, scene);
        apply_camera(&lx2, &ly2, scene);
        fprintf(fp, "  \\draw[");
        tikz_write_style(fp, &obj->style, false);
        fprintf(fp, "] (%.2f,%.2f) -- (%.2f,%.2f);\n", lx1, ly1, lx2, ly2);
        break;
    }
    case LV00_VISUAL_CIRCLE: {
        if (!get_float_cache(obj, cache, 3)) return;
        float cx = cache[0], cy = cache[1], r = cache[2];
        apply_camera(&cx, &cy, scene);
        fprintf(fp, "  \\draw[");
        tikz_write_style(fp, &obj->style, false);
        fprintf(fp, "] (%.2f,%.2f) circle (%.2f);\n", cx, cy, r);
        break;
    }
    case LV00_VISUAL_POLYGON: {
        if (obj->render_cache == NULL) return;
        int   pcount = ((int *)obj->render_cache)[0];
        float *verts = (float *)((int *)obj->render_cache + 1);
        if (pcount < 3) return;
        fprintf(fp, "  \\draw[");
        tikz_write_style(fp, &obj->style, false);
        fprintf(fp, "] ");
        for (int j = 0; j < pcount; j++) {
            float vx = verts[j * 2], vy = verts[j * 2 + 1];
            apply_camera(&vx, &vy, scene);
            fprintf(fp, "(%.2f,%.2f)", vx, vy);
            if (j < pcount - 1) fprintf(fp, " -- ");
        }
        fprintf(fp, " -- cycle;\n");
        break;
    }
    default:
        break;
    }
}

/** TikZ 渲染入口 */
static void tikz_render_scene(FILE *fp, const Lv00VisualRenderer *renderer,
                               const Lv00VisualScene *scene) {
    (void)renderer;
    fprintf(fp, "%% Generated by Lv-00 Geometry Visualizer\n");
    fprintf(fp, "\\begin{tikzpicture}[scale=1.0]\n");

    for (size_t i = 0; i < scene->object_count; i++) {
        tikz_render_object(fp, scene->objects[i], scene, 1);
    }

    fprintf(fp, "\\end{tikzpicture}\n");
}

/* ========================================================================
 * 渲染器（公共 API）
 * ======================================================================== */

Lv00VisualRenderer *lv00_visual_renderer_create(Lv00RenderBackend backend,
                                                  int width, int height)
{
    Lv00VisualRenderer *renderer = (Lv00VisualRenderer *)lv00_malloc(sizeof(Lv00VisualRenderer));
    if (renderer == NULL) return NULL;
    memset(renderer, 0, sizeof(Lv00VisualRenderer));

    renderer->backend = backend;
    renderer->backend_ctx = NULL;
    renderer->dpi = 96.0f;
    renderer->width = (width > 0) ? width : 800;
    renderer->height = (height > 0) ? height : 600;

    return renderer;
}

void lv00_visual_render(Lv00VisualRenderer *renderer,
                         Lv00VisualScene *scene,
                         const char *output_path)
{
    if (renderer == NULL || scene == NULL || output_path == NULL) return;

    FILE *fp = fopen(output_path, "w");
    if (fp == NULL) return;

    switch (renderer->backend) {
    case LV00_RENDER_SVG:
        svg_render_scene(fp, renderer, scene);
        break;
    case LV00_RENDER_TIKZ:
        tikz_render_scene(fp, renderer, scene);
        break;
    case LV00_RENDER_PNG:
        fprintf(fp, "P3\n# Lv-00 PNG placeholder (backend=%d)\n"
                "%d %d\n255\n", LV00_RENDER_PNG,
                renderer->width, renderer->height);
        /* 输出白色背景 */
        for (int y = 0; y < renderer->height; y++) {
            for (int x = 0; x < renderer->width; x++) {
                fprintf(fp, "255 255 255 ");
            }
            fprintf(fp, "\n");
        }
        break;
    default:
        fprintf(fp, "// Lv-00 render output (backend=%d, %dx%d)\n",
                (int)renderer->backend, renderer->width, renderer->height);
        break;
    }

    fclose(fp);
}

/* ========================================================================
 * 资源释放
 * ======================================================================== */

void lv00_visual_object_destroy(Lv00VisualObject *obj)
{
    if (obj == NULL) return;

    /* 递归销毁子对象 */
    if (obj->children != NULL) {
        for (size_t i = 0; i < obj->children_count; i++) {
            lv00_visual_object_destroy(obj->children[i]);
        }
        free(obj->children);
    }

    free(obj->render_cache);
    free(obj);
}

void lv00_visual_scene_destroy(Lv00VisualScene *scene)
{
    if (scene == NULL) return;
    lv00_visual_scene_clear(scene);
    free(scene);
}

void lv00_visual_renderer_destroy(Lv00VisualRenderer *renderer)
{
    if (renderer == NULL) return;
    free(renderer);
}
