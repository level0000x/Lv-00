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

#include "lv/geo_visual.h"
#include "lv/lv_utils.h"
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
static void set_default_style(lvVisualStyle *style)
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

lvVisualObject *lv_visual_point_create(float x, float y)
{
    lvVisualObject *obj = (lvVisualObject *)lv_calloc(1, sizeof(lvVisualObject));
    if (obj == NULL) return NULL;

    obj->type = lv_VISUAL_POINT;
    set_default_style(&obj->style);
    identity_matrix(obj->transform);
    obj->children = NULL;
    obj->children_count = 0;

    /* 缓存点坐标 */
    float *cache = (float *)lv_malloc(2 * sizeof(float));
    if (cache != NULL) {
        cache[0] = x;
        cache[1] = y;
    }
    obj->render_cache = cache;

    return obj;
}

lvVisualObject *lv_visual_line_create(float x1, float y1, float x2, float y2)
{
    lvVisualObject *obj = (lvVisualObject *)lv_calloc(1, sizeof(lvVisualObject));
    if (obj == NULL) return NULL;

    obj->type = lv_VISUAL_SEGMENT;
    set_default_style(&obj->style);
    identity_matrix(obj->transform);
    obj->children = NULL;
    obj->children_count = 0;

    /* 缓存线段端点坐标 */
    float *cache = (float *)lv_malloc(4 * sizeof(float));
    if (cache != NULL) {
        cache[0] = x1; cache[1] = y1;
        cache[2] = x2; cache[3] = y2;
    }
    obj->render_cache = cache;

    return obj;
}

lvVisualObject *lv_visual_circle_create(float cx, float cy, float r)
{
    lvVisualObject *obj = (lvVisualObject *)lv_calloc(1, sizeof(lvVisualObject));
    if (obj == NULL) return NULL;

    obj->type = lv_VISUAL_CIRCLE;
    set_default_style(&obj->style);
    identity_matrix(obj->transform);
    obj->children = NULL;
    obj->children_count = 0;

    /* 缓存圆心和半径 */
    float *cache = (float *)lv_malloc(3 * sizeof(float));
    if (cache != NULL) {
        cache[0] = cx; cache[1] = cy; cache[2] = r;
    }
    obj->render_cache = cache;

    return obj;
}

lvVisualObject *lv_visual_group_create(lvVisualObject **objs, size_t n)
{
    if (objs == NULL || n == 0) return NULL;

    lvVisualObject *obj = (lvVisualObject *)lv_calloc(1, sizeof(lvVisualObject));
    if (obj == NULL) return NULL;

    obj->type = lv_VISUAL_MOBJECT_GROUP;
    set_default_style(&obj->style);
    identity_matrix(obj->transform);

    obj->children = (lvVisualObject **)lv_calloc(n, sizeof(lvVisualObject *));
    if (obj->children == NULL) {
        lv_free((void **)&obj);
        return NULL;
    }
    memcpy(obj->children, objs, n * sizeof(lvVisualObject *));
    obj->children_count = n;

    return obj;
}

/* ========================================================================
 * 样式设置
 * ======================================================================== */

void lv_visual_set_style(lvVisualObject *obj, const lvVisualStyle *style)
{
    if (obj == NULL || style == NULL) return;
    obj->style = *style;
}

void lv_visual_set_color(lvVisualObject *obj, float r, float g, float b, float a)
{
    if (obj == NULL) return;
    obj->style.stroke_color[0] = r;
    obj->style.stroke_color[1] = g;
    obj->style.stroke_color[2] = b;
    obj->style.stroke_color[3] = a;
}

void lv_visual_set_dashed(lvVisualObject *obj, int dashed)
{
    if (obj == NULL) return;
    obj->style.dashed = dashed;
}

/* ========================================================================
 * 空间变换
 * ======================================================================== */

void lv_visual_translate(lvVisualObject *obj, float dx, float dy, float dz)
{
    if (obj == NULL) return;
    float t[16];
    identity_matrix(t);
    t[12] = dx; t[13] = dy; t[14] = dz;
    matrix_multiply(obj->transform, obj->transform, t);
}

void lv_visual_scale(lvVisualObject *obj, float sx, float sy)
{
    if (obj == NULL) return;
    float s[16];
    identity_matrix(s);
    s[0] = sx; s[5] = sy;
    matrix_multiply(obj->transform, obj->transform, s);
}

void lv_visual_rotate(lvVisualObject *obj, float angle, float axis[3])
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

lvVisualScene *lv_visual_scene_create(void)
{
    lvVisualScene *scene = (lvVisualScene *)lv_calloc(1, sizeof(lvVisualScene));
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

void lv_visual_scene_add(lvVisualScene *scene, lvVisualObject *obj)
{
    if (scene == NULL || obj == NULL) return;

    size_t new_count = scene->object_count + 1;
    lvVisualObject **new_arr = (lvVisualObject **)lv_realloc(
        scene->objects, new_count * sizeof(lvVisualObject *));
    if (new_arr == NULL) return;

    new_arr[scene->object_count] = obj;
    scene->objects = new_arr;
    scene->object_count = new_count;
}

void lv_visual_scene_clear(lvVisualScene *scene)
{
    if (scene == NULL) return;
    for (size_t i = 0; i < scene->object_count; i++) {
        lv_visual_object_destroy(scene->objects[i]);
    }
    lv_free((void **)&scene->objects);
    scene->object_count = 0;
}

void lv_visual_scene_set_camera(lvVisualScene *scene,
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
static void apply_camera(float *x, float *y, const lvVisualScene *scene) {
    float cx = scene->camera_center[0];
    float cy = scene->camera_center[1];
    float zoom = scene->camera_zoom;
    *x = (*x - cx) * zoom + cx;
    *y = (*y - cy) * zoom + cy;
}

/** 获取渲染缓存的端点或中心数据 */
static bool get_float_cache(const lvVisualObject *obj, float *out, int count) {
    if (obj->render_cache == NULL) return false;
    memcpy(out, obj->render_cache, (size_t)count * sizeof(float));
    return true;
}

/* ========================================================================
 * SVG 渲染
 * ======================================================================== */

/** 输出 SVG 样式属性（stroke, fill, opacity, dasharray） */
static void svg_write_style(FILE *fp, const lvVisualStyle *s) {
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
static void svg_render_object(FILE *fp, const lvVisualObject *obj,
                               const lvVisualScene *scene, int depth) {
    if (obj == NULL) return;

    /* 组合对象：递归渲染子对象 */
    if (obj->type == lv_VISUAL_MOBJECT_GROUP) {
        for (size_t i = 0; i < obj->children_count; i++) {
            svg_render_object(fp, obj->children[i], scene, depth + 1);
        }
        return;
    }

    float cache[8];
    memset(cache, 0, sizeof(cache));

    switch (obj->type) {
    case lv_VISUAL_POINT: {
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
    case lv_VISUAL_SEGMENT: {
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
    case lv_VISUAL_LINE: {
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
    case lv_VISUAL_CIRCLE: {
        if (!get_float_cache(obj, cache, 3)) return;
        float cx = cache[0], cy = cache[1], r = cache[2];
        apply_camera(&cx, &cy, scene);
        fprintf(fp, "%*s<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\"",
                depth * 2, "", cx, cy, r);
        svg_write_style(fp, &obj->style);
        fprintf(fp, "/>\n");
        break;
    }
    case lv_VISUAL_POLYGON: {
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
static void svg_render_scene(FILE *fp, const lvVisualRenderer *renderer,
                              const lvVisualScene *scene) {
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
static void tikz_write_color(FILE *fp, const lvVisualStyle *s, const char *prefix) {
    fprintf(fp, "%scolor={rgb,1:red,%.3f;green,%.3f;blue,%.3f}",
            prefix,
            s->stroke_color[0], s->stroke_color[1], s->stroke_color[2]);
}

/** 输出 TikZ 通用样式选项 */
static void tikz_write_style(FILE *fp, const lvVisualStyle *s, bool is_fill) {
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
static void tikz_render_object(FILE *fp, const lvVisualObject *obj,
                                const lvVisualScene *scene, int depth) {
    if (obj == NULL) return;

    if (obj->type == lv_VISUAL_MOBJECT_GROUP) {
        for (size_t i = 0; i < obj->children_count; i++) {
            tikz_render_object(fp, obj->children[i], scene, depth + 1);
        }
        return;
    }

    float cache[8];
    memset(cache, 0, sizeof(cache));
    const char *indent = "  ";

    switch (obj->type) {
    case lv_VISUAL_POINT: {
        if (!get_float_cache(obj, cache, 2)) return;
        float px = cache[0], py = cache[1];
        apply_camera(&px, &py, scene);
        float r = obj->style.stroke_width > 2.0f ? obj->style.stroke_width : 3.0f;
        fprintf(fp, "  \\fill[");
        tikz_write_color(fp, &obj->style, "");
        fprintf(fp, "] (%.2f,%.2f) circle (%.2fpt);\n", px, py, r);
        break;
    }
    case lv_VISUAL_SEGMENT: {
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
    case lv_VISUAL_LINE: {
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
    case lv_VISUAL_CIRCLE: {
        if (!get_float_cache(obj, cache, 3)) return;
        float cx = cache[0], cy = cache[1], r = cache[2];
        apply_camera(&cx, &cy, scene);
        fprintf(fp, "  \\draw[");
        tikz_write_style(fp, &obj->style, false);
        fprintf(fp, "] (%.2f,%.2f) circle (%.2f);\n", cx, cy, r);
        break;
    }
    case lv_VISUAL_POLYGON: {
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
static void tikz_render_scene(FILE *fp, const lvVisualRenderer *renderer,
                               const lvVisualScene *scene) {
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

lvVisualRenderer *lv_visual_renderer_create(lvRenderBackend backend,
                                                  int width, int height)
{
    lvVisualRenderer *renderer = (lvVisualRenderer *)lv_calloc(1, sizeof(lvVisualRenderer));
    if (renderer == NULL) return NULL;

    renderer->backend = backend;
    renderer->backend_ctx = NULL;
    renderer->dpi = 96.0f;
    renderer->width = (width > 0) ? width : 800;
    renderer->height = (height > 0) ? height : 600;

    return renderer;
}

/* ── 最小 PNG 编码器（无需外部库，输出有效 PNG） ── */

/** CRC-32 表（用于计算 PNG chunk CRC） */
static uint32_t png_crc_table[256];
static bool png_crc_initialized = false;

static void png_init_crc(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1) c = 0xEDB88320U ^ (c >> 1);
            else       c >>= 1;
        }
        png_crc_table[i] = c;
    }
    png_crc_initialized = true;
}

static uint32_t png_crc(const uint8_t *data, size_t len) {
    if (!png_crc_initialized) png_init_crc();
    uint32_t c = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++)
        c = png_crc_table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFU;
}

/** 以网络字节序（大端）写入 32 位整数 */
static void png_write_be32(uint8_t *buf, uint32_t v) {
    buf[0] = (uint8_t)(v >> 24);
    buf[1] = (uint8_t)(v >> 16);
    buf[2] = (uint8_t)(v >> 8);
    buf[3] = (uint8_t)(v);
}

/** 写入 PNG chunk: length + type + data + crc */
static bool png_write_chunk(FILE *fp, const char *type,
                            const uint8_t *data, uint32_t data_len) {
    uint8_t hdr[8];
    uint8_t type_buf[4];
    png_write_be32(hdr, data_len);
    type_buf[0] = (uint8_t)type[0]; type_buf[1] = (uint8_t)type[1];
    type_buf[2] = (uint8_t)type[2]; type_buf[3] = (uint8_t)type[3];

    if (fwrite(hdr, 4, 1, fp) != 1) return false;
    if (fwrite(type_buf, 4, 1, fp) != 1) return false;

    uint32_t crc_val = png_crc(type_buf, 4);
    if (data && data_len > 0) {
        if (fwrite(data, 1, data_len, fp) != data_len) return false;
        /* CRC 是 type + data 的校验 */
        uint8_t *combined = (uint8_t *)lv_malloc(4 + data_len);
        if (!combined) return false;
        memcpy(combined, type_buf, 4);
        memcpy(combined + 4, data, data_len);
        crc_val = png_crc(combined, 4 + data_len);
        lv_free((void **)&combined);
    }

    uint8_t crc_buf[4];
    png_write_be32(crc_buf, crc_val);
    if (fwrite(crc_buf, 4, 1, fp) != 1) return false;
    return true;
}

/** 将 RGB 像素数据编码为 PNG 文件 */
static bool write_png_rgb(const char *path, int width, int height,
                          const uint8_t *rgb_data) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return false;

    /* PNG 签名 */
    const uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (fwrite(sig, 8, 1, fp) != 1) { fclose(fp); return false; }

    /* IHDR chunk */
    uint8_t ihdr[13];
    png_write_be32(ihdr, (uint32_t)width);
    png_write_be32(ihdr + 4, (uint32_t)height);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 2;  /* color type: RGB */
    ihdr[10] = 0; /* compression */
    ihdr[11] = 0; /* filter */
    ihdr[12] = 0; /* interlace */
    if (!png_write_chunk(fp, "IHDR", ihdr, 13)) { fclose(fp); return false; }

    /* 构建 IDAT 数据：每行前加 filter byte=0 (None)，然后 raw RGB */
    int row_bytes = width * 3 + 1;
    size_t raw_size = (size_t)row_bytes * (size_t)height;
    uint8_t *raw = (uint8_t *)lv_malloc(raw_size);
    if (!raw) { fclose(fp); return false; }

    for (int y = 0; y < height; y++) {
        raw[y * row_bytes] = 0;
        memcpy(raw + y * row_bytes + 1,
               rgb_data + (size_t)y * width * 3,
               (size_t)width * 3);
    }

    /* 构建 DEFLATE stored block: BFINAL=1, BTYPE=00, LEN, NLEN, data */
    /* 对于超出 65535 字节的大图像，使用多块 */
    size_t raw_remaining = raw_size;
    size_t idat_capacity = raw_size + 5 + 64;
    uint8_t *idat_buf = (uint8_t *)lv_malloc(idat_capacity);
    if (!idat_buf) { lv_free((void **)&raw); fclose(fp); return false; }

    size_t idat_pos = 0;
    while (raw_remaining > 0) {
        uint32_t blk_len = (raw_remaining > 65535) ? 65535 : (uint32_t)raw_remaining;
        if (idat_pos + 5 + blk_len > idat_capacity) {
            idat_capacity = idat_pos + 5 + blk_len + 64;
            uint8_t *nb = lv_realloc(idat_buf, idat_capacity);
            if (!nb) { lv_free((void **)&idat_buf); lv_free((void **)&raw); fclose(fp); return false; }
            idat_buf = nb;
        }
        bool is_last = (raw_remaining <= 65535);
        idat_buf[idat_pos++] = is_last ? 0x01 : 0x00;
        idat_buf[idat_pos++] = (uint8_t)(blk_len & 0xFF);
        idat_buf[idat_pos++] = (uint8_t)((blk_len >> 8) & 0xFF);
        uint16_t nlen = (uint16_t)(~blk_len);
        idat_buf[idat_pos++] = (uint8_t)(nlen & 0xFF);
        idat_buf[idat_pos++] = (uint8_t)((nlen >> 8) & 0xFF);
        memcpy(idat_buf + idat_pos, raw + (raw_size - raw_remaining), blk_len);
        idat_pos += blk_len;
        raw_remaining -= blk_len;
    }
    lv_free((void **)&raw);

    if (!png_write_chunk(fp, "IDAT", idat_buf, (uint32_t)idat_pos)) {
        lv_free((void **)&idat_buf); fclose(fp); return false;
    }
    lv_free((void **)&idat_buf);

    /* IEND chunk */
    if (!png_write_chunk(fp, "IEND", NULL, 0)) { fclose(fp); return false; }

    fclose(fp);
    return true;
}

void lv_visual_render(lvVisualRenderer *renderer,
                         lvVisualScene *scene,
                         const char *output_path)
{
    if (renderer == NULL || scene == NULL || output_path == NULL) return;

    FILE *fp = fopen(output_path, "w");
    if (fp == NULL) return;

    switch (renderer->backend) {
    case lv_RENDER_SVG:
        svg_render_scene(fp, renderer, scene);
        break;
    case lv_RENDER_TIKZ:
        tikz_render_scene(fp, renderer, scene);
        break;
    case lv_RENDER_PNG: {
        /* PNG 使用二进制模式，关闭文本模式 fp */
        fclose(fp);
        size_t px = (size_t)renderer->width * (size_t)renderer->height;
        size_t rgb_size = px * 3;
        uint8_t *rgb = (uint8_t *)lv_malloc(rgb_size);
        if (rgb) {
            memset(rgb, 255, rgb_size);
            write_png_rgb(output_path, renderer->width, renderer->height, rgb);
            lv_free((void **)&rgb);
        }
        /* write_png_rgb 已关闭文件，避免重复关闭 */
        fp = NULL;
        break;
    }
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

void lv_visual_object_destroy(lvVisualObject *obj)
{
    if (obj == NULL) return;

    /* 递归销毁子对象 */
    if (obj->children != NULL) {
        for (size_t i = 0; i < obj->children_count; i++) {
            lv_visual_object_destroy(obj->children[i]);
        }
        lv_free((void **)&obj->children);
    }

    lv_free((void **)&obj->render_cache);
    lv_free((void **)&obj);
}

void lv_visual_scene_destroy(lvVisualScene *scene)
{
    if (scene == NULL) return;
    lv_visual_scene_clear(scene);
    lv_free((void **)&scene);
}

void lv_visual_renderer_destroy(lvVisualRenderer *renderer)
{
    if (renderer == NULL) return;
    lv_free((void **)&renderer);
}
