/**
 * @file geo_visual.c
 * @brief 几何可视化抽象层实现
 */

#include "lv00/geo_visual.h"
#include "lv00/lv00_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============ 内部工具函数 ============ */

static void identity_matrix(float m[16]) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/* 将渲染内容写入输出文件（文本模式） */
static void write_output_to_file(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs(content, fp);
        fclose(fp);
    } else {
        LV00_LOG_WARNING("geo_visual: 无法打开输出文件 %s", path);
    }
}

/* ============ 构造器实现 ============ */

Lv00VisualObject* lv00_visual_point_create(float x, float y) {
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    if (!obj) return NULL;
    
    obj->type = LV00_VISUAL_POINT;
    obj->style.stroke_width = 2.0f;
    obj->style.stroke_color[0] = obj->style.stroke_color[1] = obj->style.stroke_color[2] = 0.0f;
    obj->style.stroke_color[3] = 1.0f;
    obj->style.fill_color[0] = obj->style.fill_color[1] = obj->style.fill_color[2] = 0.0f;
    obj->style.fill_color[3] = 1.0f;
    obj->style.opacity = 1.0f;
    obj->style.dashed = 0;
    
    obj->entity = NULL;
    obj->render_cache = NULL;
    obj->children = NULL;
    obj->children_count = 0;
    
    identity_matrix(obj->transform);
    /* 设置平移 */
    obj->transform[12] = x;
    obj->transform[13] = y;
    
    return obj;
}

Lv00VisualObject* lv00_visual_line_create(float x1, float y1, float x2, float y2) {
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    if (!obj) return NULL;
    
    obj->type = LV00_VISUAL_LINE;
    obj->style.stroke_width = 1.5f;
    obj->style.stroke_color[0] = obj->style.stroke_color[1] = obj->style.stroke_color[2] = 0.0f;
    obj->style.stroke_color[3] = 1.0f;
    obj->style.opacity = 1.0f;
    obj->style.dashed = 0;
    
    obj->entity = NULL;
    obj->render_cache = NULL;
    obj->children = NULL;
    obj->children_count = 0;
    
    identity_matrix(obj->transform);
    
    /* 存储端点信息在 render_cache */
    float* endpoints = (float*)lv00_malloc(4 * sizeof(float));
    if (!endpoints) {
        lv00_free_ptr(obj);
        return NULL;
    }
    endpoints[0] = x1; endpoints[1] = y1;
    endpoints[2] = x2; endpoints[3] = y2;
    obj->render_cache = endpoints;
    
    return obj;
}

Lv00VisualObject* lv00_visual_circle_create(float cx, float cy, float r) {
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    if (!obj) return NULL;
    
    obj->type = LV00_VISUAL_CIRCLE;
    obj->style.stroke_width = 1.5f;
    obj->style.stroke_color[0] = obj->style.stroke_color[1] = obj->style.stroke_color[2] = 0.0f;
    obj->style.stroke_color[3] = 1.0f;
    obj->style.fill_color[0] = obj->style.fill_color[1] = obj->style.fill_color[2] = 1.0f;
    obj->style.fill_color[3] = 0.0f; /* 透明填充 */
    obj->style.opacity = 1.0f;
    obj->style.dashed = 0;
    
    obj->entity = NULL;
    obj->render_cache = NULL;
    obj->children = NULL;
    obj->children_count = 0;
    
    identity_matrix(obj->transform);
    obj->transform[12] = cx;
    obj->transform[13] = cy;
    
    /* 存储半径 */
    float* radius = (float*)lv00_malloc(sizeof(float));
    if (!radius) {
        lv00_free_ptr(obj);
        return NULL;
    }
    *radius = r;
    obj->render_cache = radius;
    
    return obj;
}

Lv00VisualObject* lv00_visual_group_create(Lv00VisualObject** objs, size_t n) {
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    if (!obj) return NULL;
    
    obj->type = LV00_VISUAL_MOBJECT_GROUP;
    obj->style.stroke_width = 0;
    obj->style.opacity = 1.0f;
    
    obj->entity = NULL;
    obj->render_cache = NULL;
    
    if (n > 0 && objs) {
        obj->children = (Lv00VisualObject**)lv00_malloc((size_t)n * sizeof(Lv00VisualObject*));
        if (!obj->children) {
            lv00_free_ptr(obj);
            return NULL;
        }
        memcpy(obj->children, objs, n * sizeof(Lv00VisualObject*));
        obj->children_count = n;
    } else {
        obj->children = NULL;
        obj->children_count = 0;
    }
    
    identity_matrix(obj->transform);
    
    return obj;
}

/* ============ 样式设置 ============ */

void lv00_visual_set_style(Lv00VisualObject* obj, const Lv00VisualStyle* style) {
    if (obj && style) {
        obj->style = *style;
    }
}

void lv00_visual_set_color(Lv00VisualObject* obj, float r, float g, float b, float a) {
    if (obj) {
        obj->style.stroke_color[0] = r;
        obj->style.stroke_color[1] = g;
        obj->style.stroke_color[2] = b;
        obj->style.stroke_color[3] = a;
    }
}

void lv00_visual_set_dashed(Lv00VisualObject* obj, int dashed) {
    if (obj) {
        obj->style.dashed = dashed;
    }
}

/* ============ 变换 ============ */

void lv00_visual_translate(Lv00VisualObject* obj, float dx, float dy, float dz) {
    if (!obj) return;
    obj->transform[12] += dx;
    obj->transform[13] += dy;
    obj->transform[14] += dz;
}

void lv00_visual_scale(Lv00VisualObject* obj, float sx, float sy) {
    if (!obj) return;
    obj->transform[0] *= sx;
    obj->transform[5] *= sy;
}

void lv00_visual_rotate(Lv00VisualObject* obj, float angle, float axis[3]) {
    if (!obj) return;

    /* 判断是否需要使用任意轴旋转（Rodrigues 旋转公式） */
    int use_z_axis = 0;
    if (!axis) {
        use_z_axis = 1;
    } else {
        float len = sqrtf(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
        if (len < 1e-8f) {
            use_z_axis = 1; /* 零向量，默认 Z 轴旋转 */
        }
    }

    float rot[16];
    identity_matrix(rot);

    if (use_z_axis) {
        /* Z 轴旋转 */
        float c = cosf(angle);
        float s = sinf(angle);
        rot[0] = c;  rot[1] = -s;
        rot[4] = s;  rot[5] = c;
    } else {
        /* Rodrigues 旋转公式: v' = v*cos(θ) + (k×v)*sin(θ) + k*(k·v)*(1-cos(θ))
         * 其中 k 为单位旋转轴，θ 为旋转角度
         * 转换为 4x4 旋转矩阵 */
        float kx = axis[0], ky = axis[1], kz = axis[2];
        float len = sqrtf(kx*kx + ky*ky + kz*kz);
        kx /= len; ky /= len; kz /= len; /* 归一化 */

        float c = cosf(angle);
        float s = sinf(angle);
        float t = 1.0f - c; /* 1 - cos(θ) */

        /* 旋转矩阵 R:
         * R[0][0] = c + kx²*t      R[0][1] = kx*ky*t - kz*s   R[0][2] = kx*kz*t + ky*s
         * R[1][0] = ky*kx*t + kz*s  R[1][1] = c + ky²*t       R[1][2] = ky*kz*t - kx*s
         * R[2][0] = kz*kx*t - ky*s  R[2][1] = kz*ky*t + kx*s  R[2][2] = c + kz²*t
         */
        rot[0] = c + kx*kx*t;
        rot[1] = kx*ky*t - kz*s;
        rot[2] = kx*kz*t + ky*s;

        rot[4] = ky*kx*t + kz*s;
        rot[5] = c + ky*ky*t;
        rot[6] = ky*kz*t - kx*s;

        rot[8]  = kz*kx*t - ky*s;
        rot[9]  = kz*ky*t + kx*s;
        rot[10] = c + kz*kz*t;
    }

    /* 矩阵乘法: transform = transform * rot */
    float result[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result[i*4+j] = 0;
            for (int k = 0; k < 4; k++) {
                result[i*4+j] += obj->transform[i*4+k] * rot[k*4+j];
            }
        }
    }
    memcpy(obj->transform, result, 16 * sizeof(float));
}

/* ============ 场景管理 ============ */

Lv00VisualScene* lv00_visual_scene_create(void) {
    Lv00VisualScene* scene = (Lv00VisualScene*)lv00_malloc(sizeof(Lv00VisualScene));
    if (!scene) return NULL;
    
    scene->objects = NULL;
    scene->object_count = 0;
    scene->camera_center[0] = scene->camera_center[1] = scene->camera_center[2] = 0.0f;
    scene->camera_zoom = 1.0f;
    scene->is_3d = 0;
    scene->current_time = 0.0f;
    scene->total_duration = 0.0f;
    
    return scene;
}

void lv00_visual_scene_add(Lv00VisualScene* scene, Lv00VisualObject* obj) {
    if (!scene || !obj) return;
    
    size_t new_count = scene->object_count + 1;
    Lv00VisualObject** new_objects = (Lv00VisualObject**)lv00_malloc((size_t)new_count * sizeof(Lv00VisualObject*));
    if (!new_objects) return;

    if (scene->objects) {
        memcpy(new_objects, scene->objects, scene->object_count * sizeof(Lv00VisualObject*));
        lv00_free_ptr(scene->objects);
    }
    
    new_objects[scene->object_count] = obj;
    scene->objects = new_objects;
    scene->object_count = new_count;
}

void lv00_visual_scene_clear(Lv00VisualScene* scene) {
    if (!scene) return;
    
    for (size_t i = 0; i < scene->object_count; i++) {
        lv00_visual_object_destroy(scene->objects[i]);
    }
    
    lv00_free_ptr(scene->objects);
    scene->objects = NULL;
    scene->object_count = 0;
}

void lv00_visual_scene_set_camera(Lv00VisualScene* scene, float cx, float cy, float cz, float zoom) {
    if (!scene) return;
    scene->camera_center[0] = cx;
    scene->camera_center[1] = cy;
    scene->camera_center[2] = cz;
    scene->camera_zoom = zoom;
}

/* ============ SVG 辅助函数 ============ */

/* 将浮点 RGBA 颜色转换为 SVG 颜色字符串 */
static void color_to_svg(float c[4], char* buf, size_t buf_size) {
    int r = (int)(c[0] * 255.0f);
    int g = (int)(c[1] * 255.0f);
    int b = (int)(c[2] * 255.0f);
    float a = c[3];
    if (a < 0.999f) {
        snprintf(buf, buf_size, "rgba(%d,%d,%d,%.2f)", r, g, b, a);
    } else {
        snprintf(buf, buf_size, "rgb(%d,%d,%d)", r, g, b);
    }
}

/* 递归渲染单个对象为 SVG 元素 */
static void render_object_svg(Lv00VisualObject* obj, char** buf, size_t* pos, size_t* cap) {
    if (!obj) return;

    /* 辅助宏：向缓冲区追加字符串（先检查容量再写入） */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
    #define SVG_APPEND(fmt, ...) do { \
        int needed = snprintf(NULL, 0, fmt, ##__VA_ARGS__); \
        if (needed > 0) { \
            while (*pos + (size_t)needed + 1 > *cap) { \
                *cap *= 2; \
                char* _nb = (char*)lv00_realloc(*buf, *cap); \
                if (!_nb) break; \
                *buf = _nb; \
            } \
            int written = snprintf(*buf + *pos, *cap - *pos, fmt, ##__VA_ARGS__); \
            if (written > 0) { \
                *pos += (size_t)written; \
            } \
        } \
    } while(0)

    char stroke[64], fill[64];
    color_to_svg(obj->style.stroke_color, stroke, sizeof(stroke));
    color_to_svg(obj->style.fill_color, fill, sizeof(fill));

    const char* dash_attr = obj->style.dashed ? " stroke-dasharray=\"5,3\"" : "";

    switch (obj->type) {
    case LV00_VISUAL_POINT: {
        /* 点渲染为小圆 */
        float px = obj->transform[12];
        float py = obj->transform[13];
        SVG_APPEND("  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"3\" "
                   "fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\" opacity=\"%.2f\"/>\n",
                   px, py, stroke, stroke, obj->style.stroke_width, obj->style.opacity);
        break;
    }
    case LV00_VISUAL_LINE:
    case LV00_VISUAL_SEGMENT: {
        /* 线段渲染为 SVG line */
        if (obj->render_cache) {
            float* ep = (float*)obj->render_cache;
            SVG_APPEND("  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                       "stroke=\"%s\" stroke-width=\"%.1f\" opacity=\"%.2f\"%s/>\n",
                       ep[0], ep[1], ep[2], ep[3],
                       stroke, obj->style.stroke_width, obj->style.opacity, dash_attr);
        }
        break;
    }
    case LV00_VISUAL_CIRCLE: {
        /* 圆渲染为 SVG circle */
        float cx = obj->transform[12];
        float cy = obj->transform[13];
        float r = obj->render_cache ? *(float*)obj->render_cache : 10.0f;
        SVG_APPEND("  <circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" "
                   "fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\" opacity=\"%.2f\"%s/>\n",
                   cx, cy, r, fill, stroke, obj->style.stroke_width, obj->style.opacity, dash_attr);
        break;
    }
    case LV00_VISUAL_POLYGON: {
        /* 多边形渲染为 SVG polygon */
        if (obj->render_cache && obj->children_count > 0) {
            SVG_APPEND("  <polygon points=\"");
            for (size_t i = 0; i < obj->children_count; i++) {
                Lv00VisualObject* child = obj->children[i];
                if (child) {
                    float px = child->transform[12];
                    float py = child->transform[13];
                    SVG_APPEND("%.2f,%.2f ", px, py);
                }
            }
            SVG_APPEND("\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\" opacity=\"%.2f\"%s/>\n",
                       fill, stroke, obj->style.stroke_width, obj->style.opacity, dash_attr);
        }
        break;
    }
    case LV00_VISUAL_MOBJECT_GROUP: {
        /* 组合对象：递归渲染子对象 */
        for (size_t i = 0; i < obj->children_count; i++) {
            render_object_svg(obj->children[i], buf, pos, cap);
        }
        break;
    }
    default:
        /* 其他类型暂不渲染 */
        break;
    }

    #undef SVG_APPEND
}

/* ============ Cairo 辅助函数 ============ */

/* 递归渲染单个对象为 Cairo 脚本命令 */
static void render_object_cairo(Lv00VisualObject* obj, char** buf, size_t* pos, size_t* cap) {
    if (!obj) return;

    /* 辅助宏：向缓冲区追加字符串 */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
    #define CAIRO_APPEND(fmt, ...) do { \
        size_t _avail = *cap - *pos; \
        if (*pos >= *cap) { \
            *cap *= 2; \
            char* _nb = (char*)lv00_realloc(*buf, *cap); \
            if (_nb) *buf = _nb; else break; \
            _avail = *cap - *pos; \
        } \
        int written = snprintf(*buf + *pos, _avail, fmt, ##__VA_ARGS__); \
        if (written > 0) { \
            *pos += (size_t)written; \
            if (*pos >= *cap) { \
                *cap *= 2; \
                char* _nb2 = (char*)lv00_realloc(*buf, *cap); \
                if (_nb2) *buf = _nb2; \
            } \
        } \
    } while(0)

    /* 将浮点 RGBA 颜色转换为 Cairo rgba() 调用 */
    float sr = obj->style.stroke_color[0];
    float sg = obj->style.stroke_color[1];
    float sb = obj->style.stroke_color[2];
    float sa = obj->style.stroke_color[3];
    float fr = obj->style.fill_color[0];
    float fg = obj->style.fill_color[1];
    float fb_c = obj->style.fill_color[2];
    float fa = obj->style.fill_color[3];

    switch (obj->type) {
    case LV00_VISUAL_POINT: {
        /* 点渲染为小填充圆 */
        float px = obj->transform[12];
        float py = obj->transform[13];
        CAIRO_APPEND("  /* 点 (%.2f, %.2f) */\n", px, py);
        CAIRO_APPEND("  cr->save();\n");
        CAIRO_APPEND("  cr->set_source_rgba(%.3f, %.3f, %.3f, %.3f);\n", sr, sg, sb, sa);
        CAIRO_APPEND("  cr->arc(%.2f, %.2f, 3.0, 0, 2 * M_PI);\n", px, py);
        CAIRO_APPEND("  cr->fill();\n");
        CAIRO_APPEND("  cr->restore();\n\n");
        break;
    }
    case LV00_VISUAL_LINE:
    case LV00_VISUAL_SEGMENT: {
        /* 线段渲染为 move_to/line_to */
        if (obj->render_cache) {
            float* ep = (float*)obj->render_cache;
            CAIRO_APPEND("  /* 线段 (%.2f,%.2f) -> (%.2f,%.2f) */\n",
                       ep[0], ep[1], ep[2], ep[3]);
            CAIRO_APPEND("  cr->save();\n");
            CAIRO_APPEND("  cr->set_source_rgba(%.3f, %.3f, %.3f, %.3f);\n", sr, sg, sb, sa);
            CAIRO_APPEND("  cr->set_line_width(%.2f);\n", obj->style.stroke_width);
            if (obj->style.dashed) {
                CAIRO_APPEND("  cr->set_dash(dashes, 2);\n");
            }
            CAIRO_APPEND("  cr->move_to(%.2f, %.2f);\n", ep[0], ep[1]);
            CAIRO_APPEND("  cr->line_to(%.2f, %.2f);\n", ep[2], ep[3]);
            CAIRO_APPEND("  cr->stroke();\n");
            CAIRO_APPEND("  cr->restore();\n\n");
        }
        break;
    }
    case LV00_VISUAL_CIRCLE: {
        /* 圆渲染为 arc */
        float cx = obj->transform[12];
        float cy = obj->transform[13];
        float r = obj->render_cache ? *(float*)obj->render_cache : 10.0f;
        CAIRO_APPEND("  /* 圆 (%.2f, %.2f) r=%.2f */\n", cx, cy, r);
        CAIRO_APPEND("  cr->save();\n");
        CAIRO_APPEND("  cr->set_source_rgba(%.3f, %.3f, %.3f, %.3f);\n", fr, fg, fb_c, fa);
        CAIRO_APPEND("  cr->arc(%.2f, %.2f, %.2f, 0, 2 * M_PI);\n", cx, cy, r);
        CAIRO_APPEND("  cr->fill_preserve();\n");
        CAIRO_APPEND("  cr->set_source_rgba(%.3f, %.3f, %.3f, %.3f);\n", sr, sg, sb, sa);
        CAIRO_APPEND("  cr->set_line_width(%.2f);\n", obj->style.stroke_width);
        CAIRO_APPEND("  cr->stroke();\n");
        CAIRO_APPEND("  cr->restore();\n\n");
        break;
    }
    case LV00_VISUAL_POLYGON: {
        /* 多边形渲染为 move_to/line_to/close_path */
        if (obj->children_count > 0) {
            CAIRO_APPEND("  /* 多边形 (%zu 顶点) */\n", obj->children_count);
            CAIRO_APPEND("  cr->save();\n");
            CAIRO_APPEND("  cr->set_source_rgba(%.3f, %.3f, %.3f, %.3f);\n", fr, fg, fb_c, fa);
            for (size_t i = 0; i < obj->children_count; i++) {
                Lv00VisualObject* child = obj->children[i];
                if (child) {
                    float px = child->transform[12];
                    float py = child->transform[13];
                    if (i == 0) {
                        CAIRO_APPEND("  cr->move_to(%.2f, %.2f);\n", px, py);
                    } else {
                        CAIRO_APPEND("  cr->line_to(%.2f, %.2f);\n", px, py);
                    }
                }
            }
            CAIRO_APPEND("  cr->close_path();\n");
            CAIRO_APPEND("  cr->fill_preserve();\n");
            CAIRO_APPEND("  cr->set_source_rgba(%.3f, %.3f, %.3f, %.3f);\n", sr, sg, sb, sa);
            CAIRO_APPEND("  cr->set_line_width(%.2f);\n", obj->style.stroke_width);
            CAIRO_APPEND("  cr->stroke();\n");
            CAIRO_APPEND("  cr->restore();\n\n");
        }
        break;
    }
    case LV00_VISUAL_MOBJECT_GROUP: {
        /* 组合对象：递归渲染子对象 */
        for (size_t i = 0; i < obj->children_count; i++) {
            render_object_cairo(obj->children[i], buf, pos, cap);
        }
        break;
    }
    default:
        /* 其他类型暂不渲染 */
        break;
    }

    #undef CAIRO_APPEND
}

/* ============ Three.js 辅助函数 ============ */

/* 递归渲染单个对象为 Three.js JavaScript 代码 */
static void render_object_threejs(Lv00VisualObject* obj, char** buf, size_t* pos, size_t* cap) {
    if (!obj) return;

    /* 辅助宏：向缓冲区追加字符串 */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
    #define THREEJS_APPEND(fmt, ...) do { \
        size_t _avail = *cap - *pos; \
        if (*pos >= *cap) { \
            *cap *= 2; \
            char* _nb = (char*)lv00_realloc(*buf, *cap); \
            if (_nb) *buf = _nb; else break; \
            _avail = *cap - *pos; \
        } \
        int written = snprintf(*buf + *pos, _avail, fmt, ##__VA_ARGS__); \
        if (written > 0) { \
            *pos += (size_t)written; \
            if (*pos >= *cap) { \
                *cap *= 2; \
                char* _nb2 = (char*)lv00_realloc(*buf, *cap); \
                if (_nb2) *buf = _nb2; \
            } \
        } \
    } while(0)

    /* 将浮点 RGBA 颜色转换为 Three.js 十六进制颜色字符串 */
    int sr = (int)(obj->style.stroke_color[0] * 255.0f);
    int sg = (int)(obj->style.stroke_color[1] * 255.0f);
    int sb = (int)(obj->style.stroke_color[2] * 255.0f);
    int fr = (int)(obj->style.fill_color[0] * 255.0f);
    int fg = (int)(obj->style.fill_color[1] * 255.0f);
    int fb_c = (int)(obj->style.fill_color[2] * 255.0f);
    float opacity = obj->style.opacity;

    switch (obj->type) {
    case LV00_VISUAL_POINT: {
        /* 点渲染为 SphereGeometry */
        float px = obj->transform[12];
        float py = obj->transform[13];
        THREEJS_APPEND("  // 点 (%.2f, %.2f)\n", px, py);
        THREEJS_APPEND("  (function() {\n");
        THREEJS_APPEND("    var geo = new THREE.SphereGeometry(0.05, 16, 16);\n");
        THREEJS_APPEND("    var mat = new THREE.MeshBasicMaterial({ color: 0x%02x%02x%02x, opacity: %.2f });\n",
                       sr, sg, sb, opacity);
        THREEJS_APPEND("    var mesh = new THREE.Mesh(geo, mat);\n");
        THREEJS_APPEND("    mesh.position.set(%.2f, %.2f, 0);\n", px, py);
        THREEJS_APPEND("    scene.add(mesh);\n");
        THREEJS_APPEND("  })();\n\n");
        break;
    }
    case LV00_VISUAL_LINE:
    case LV00_VISUAL_SEGMENT: {
        /* 线段渲染为 BufferGeometry + LineBasicMaterial */
        if (obj->render_cache) {
            float* ep = (float*)obj->render_cache;
            THREEJS_APPEND("  // 线段 (%.2f,%.2f) -> (%.2f,%.2f)\n",
                       ep[0], ep[1], ep[2], ep[3]);
            THREEJS_APPEND("  (function() {\n");
            THREEJS_APPEND("    var points = [new THREE.Vector3(%.2f, %.2f, 0), new THREE.Vector3(%.2f, %.2f, 0)];\n",
                       ep[0], ep[1], ep[2], ep[3]);
            THREEJS_APPEND("    var geo = new THREE.BufferGeometry().setFromPoints(points);\n");
            THREEJS_APPEND("    var mat = new THREE.LineBasicMaterial({ color: 0x%02x%02x%02x, linewidth: %.1f, opacity: %.2f });\n",
                       sr, sg, sb, obj->style.stroke_width, opacity);
            THREEJS_APPEND("    var line = new THREE.Line(geo, mat);\n");
            THREEJS_APPEND("    scene.add(line);\n");
            THREEJS_APPEND("  })();\n\n");
        }
        break;
    }
    case LV00_VISUAL_CIRCLE: {
        /* 圆渲染为自定义圆形轮廓（使用 RingGeometry 近似） */
        float cx = obj->transform[12];
        float cy = obj->transform[13];
        float r = obj->render_cache ? *(float*)obj->render_cache : 10.0f;
        THREEJS_APPEND("  // 圆 (%.2f, %.2f) r=%.2f\n", cx, cy, r);
        THREEJS_APPEND("  (function() {\n");
        THREEJS_APPEND("    var segments = 64;\n");
        THREEJS_APPEND("    var pts = [];\n");
        THREEJS_APPEND("    for (var i = 0; i <= segments; i++) {\n");
        THREEJS_APPEND("      var angle = (i / segments) * 2 * Math.PI;\n");
        THREEJS_APPEND("      pts.push(new THREE.Vector3(%.2f + %.2f * Math.cos(angle), %.2f + %.2f * Math.sin(angle), 0));\n",
                       cx, r, cy, r);
        THREEJS_APPEND("    }\n");
        THREEJS_APPEND("    var geo = new THREE.BufferGeometry().setFromPoints(pts);\n");
        THREEJS_APPEND("    var mat = new THREE.LineBasicMaterial({ color: 0x%02x%02x%02x, opacity: %.2f });\n",
                       sr, sg, sb, opacity);
        THREEJS_APPEND("    var circle = new THREE.Line(geo, mat);\n");
        THREEJS_APPEND("    scene.add(circle);\n");
        THREEJS_APPEND("  })();\n\n");
        break;
    }
    case LV00_VISUAL_POLYGON: {
        /* 多边形渲染为 ShapeGeometry */
        if (obj->children_count > 0) {
            THREEJS_APPEND("  // 多边形 (%zu 顶点)\n", obj->children_count);
            THREEJS_APPEND("  (function() {\n");
            THREEJS_APPEND("    var shape = new THREE.Shape();\n");
            size_t first = 1;
            for (size_t i = 0; i < obj->children_count; i++) {
                Lv00VisualObject* child = obj->children[i];
                if (child) {
                    float px = child->transform[12];
                    float py = child->transform[13];
                    if (first) {
                        THREEJS_APPEND("    shape.moveTo(%.2f, %.2f);\n", px, py);
                        first = 0;
                    } else {
                        THREEJS_APPEND("    shape.lineTo(%.2f, %.2f);\n", px, py);
                    }
                }
            }
            THREEJS_APPEND("    var geo = new THREE.ShapeGeometry(shape);\n");
            THREEJS_APPEND("    var mat = new THREE.MeshBasicMaterial({ color: 0x%02x%02x%02x, opacity: %.2f, side: THREE.DoubleSide });\n",
                       fr, fg, fb_c, opacity);
            THREEJS_APPEND("    var mesh = new THREE.Mesh(geo, mat);\n");
            THREEJS_APPEND("    scene.add(mesh);\n");
            THREEJS_APPEND("  })();\n\n");
        }
        break;
    }
    case LV00_VISUAL_MOBJECT_GROUP: {
        /* 组合对象：递归渲染子对象 */
        for (size_t i = 0; i < obj->children_count; i++) {
            render_object_threejs(obj->children[i], buf, pos, cap);
        }
        break;
    }
    default:
        /* 其他类型暂不渲染 */
        break;
    }

    #undef THREEJS_APPEND
}

/* ============ TikZ 辅助函数 ============ */

/* 将浮点 RGBA 颜色转换为 TikZ 颜色定义 */
static void color_to_tikz(float c[4], char* buf, size_t buf_size) {
    int r = (int)(c[0] * 255.0f);
    int g = (int)(c[1] * 255.0f);
    int b = (int)(c[2] * 255.0f);
    float a = c[3];
    if (a < 0.999f) {
        snprintf(buf, buf_size, "{rgb,1:red,%d;green,%d;blue,%d},opacity=%.2f", r, g, b, a);
    } else {
        snprintf(buf, buf_size, "{rgb,1:red,%d;green,%d;blue,%d}", r, g, b);
    }
}

/* 递归渲染单个对象为 TikZ 命令 */
static void render_object_tikz(Lv00VisualObject* obj, char** buf, size_t* pos, size_t* cap) {
    if (!obj) return;

    /* 辅助宏：向缓冲区追加字符串 */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
    #define TIKZ_APPEND(fmt, ...) do { \
        size_t _avail = *cap - *pos; \
        if (*pos >= *cap) { \
            *cap *= 2; \
            char* _nb = (char*)lv00_realloc(*buf, *cap); \
            if (_nb) *buf = _nb; else break; \
            _avail = *cap - *pos; \
        } \
        int written = snprintf(*buf + *pos, _avail, fmt, ##__VA_ARGS__); \
        if (written > 0) { \
            *pos += (size_t)written; \
            if (*pos >= *cap) { \
                *cap *= 2; \
                char* _nb2 = (char*)lv00_realloc(*buf, *cap); \
                if (_nb2) *buf = _nb2; \
            } \
        } \
    } while(0)

    char stroke[128], fill[128];
    color_to_tikz(obj->style.stroke_color, stroke, sizeof(stroke));
    color_to_tikz(obj->style.fill_color, fill, sizeof(fill));

    switch (obj->type) {
    case LV00_VISUAL_POINT: {
        /* 点渲染为填充小圆节点 */
        float px = obj->transform[12];
        float py = obj->transform[13];
        TIKZ_APPEND("  %% 点 (%.2f, %.2f)\n", px, py);
        TIKZ_APPEND("  \\node[circle,fill=%s,inner sep=1.5pt] at (%.2f, %.2f) {};\n\n",
                   stroke, px, py);
        break;
    }
    case LV00_VISUAL_LINE:
    case LV00_VISUAL_SEGMENT: {
        /* 线段渲染为 \draw 命令 */
        if (obj->render_cache) {
            float* ep = (float*)obj->render_cache;
            const char* dash_opt = obj->style.dashed ? ",dashed" : "";
            TIKZ_APPEND("  %% 线段 (%.2f,%.2f) -> (%.2f,%.2f)\n",
                       ep[0], ep[1], ep[2], ep[3]);
            TIKZ_APPEND("  \\draw[thick,color=%s,line width=%.1fpt%s] (%.2f, %.2f) -- (%.2f, %.2f);\n\n",
                       stroke, obj->style.stroke_width, dash_opt,
                       ep[0], ep[1], ep[2], ep[3]);
        }
        break;
    }
    case LV00_VISUAL_CIRCLE: {
        /* 圆渲染为 \draw circle */
        float cx = obj->transform[12];
        float cy = obj->transform[13];
        float r = obj->render_cache ? *(float*)obj->render_cache : 10.0f;
        TIKZ_APPEND("  %% 圆 (%.2f, %.2f) r=%.2f\n", cx, cy, r);
        TIKZ_APPEND("  \\draw[fill=%s,draw=%s,line width=%.1fpt] (%.2f, %.2f) circle (%.2f);\n\n",
                   fill, stroke, obj->style.stroke_width, cx, cy, r);
        break;
    }
    case LV00_VISUAL_POLYGON: {
        /* 多边形渲染为 \draw -- cycle */
        if (obj->children_count > 0) {
            TIKZ_APPEND("  %% 多边形 (%zu 顶点)\n", obj->children_count);
            TIKZ_APPEND("  \\draw[fill=%s,draw=%s,line width=%.1fpt] ",
                       fill, stroke, obj->style.stroke_width);
            for (size_t i = 0; i < obj->children_count; i++) {
                Lv00VisualObject* child = obj->children[i];
                if (child) {
                    float px = child->transform[12];
                    float py = child->transform[13];
                    if (i == 0) {
                        TIKZ_APPEND("(%.2f, %.2f)", px, py);
                    } else {
                        TIKZ_APPEND(" -- (%.2f, %.2f)", px, py);
                    }
                }
            }
            TIKZ_APPEND(" -- cycle;\n\n");
        }
        break;
    }
    case LV00_VISUAL_MOBJECT_GROUP: {
        /* 组合对象：递归渲染子对象 */
        for (size_t i = 0; i < obj->children_count; i++) {
            render_object_tikz(obj->children[i], buf, pos, cap);
        }
        break;
    }
    default:
        /* 其他类型暂不渲染 */
        break;
    }

    #undef TIKZ_APPEND
}

/* ============ PPM 像素缓冲辅助函数 ============ */

/* 设置像素颜色（含边界检查） */
static void ppm_set_pixel(unsigned char* pixels, int w, int h, int x, int y,
                          unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    int idx = (y * w + x) * 3;
    pixels[idx + 0] = r;
    pixels[idx + 1] = g;
    pixels[idx + 2] = b;
}

/* Bresenham 画线算法 */
static void ppm_draw_line(unsigned char* pixels, int w, int h,
                          int x0, int y0, int x1, int y1,
                          unsigned char r, unsigned char g, unsigned char b) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        ppm_set_pixel(pixels, w, h, x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

/* 中点圆算法：画圆轮廓 */
static void ppm_draw_circle(unsigned char* pixels, int w, int h,
                            int cx, int cy, int radius,
                            unsigned char r, unsigned char g, unsigned char b) {
    int x = radius;
    int y = 0;
    int d = 1 - radius;

    while (x >= y) {
        /* 对称绘制 8 个点 */
        ppm_set_pixel(pixels, w, h, cx + x, cy + y, r, g, b);
        ppm_set_pixel(pixels, w, h, cx - x, cy + y, r, g, b);
        ppm_set_pixel(pixels, w, h, cx + x, cy - y, r, g, b);
        ppm_set_pixel(pixels, w, h, cx - x, cy - y, r, g, b);
        ppm_set_pixel(pixels, w, h, cx + y, cy + x, r, g, b);
        ppm_set_pixel(pixels, w, h, cx - y, cy + x, r, g, b);
        ppm_set_pixel(pixels, w, h, cx + y, cy - x, r, g, b);
        ppm_set_pixel(pixels, w, h, cx - y, cy - x, r, g, b);

        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

/* 填充圆（Bresenham 实心圆） */
static void ppm_fill_circle(unsigned char* pixels, int w, int h,
                            int cx, int cy, int radius,
                            unsigned char r, unsigned char g, unsigned char b) {
    int x = radius;
    int y = 0;
    int d = 1 - radius;

    while (x >= y) {
        /* 绘制水平线段填充 */
        for (int i = cx - x; i <= cx + x; i++) {
            ppm_set_pixel(pixels, w, h, i, cy + y, r, g, b);
            ppm_set_pixel(pixels, w, h, i, cy - y, r, g, b);
        }
        for (int i = cx - y; i <= cx + y; i++) {
            ppm_set_pixel(pixels, w, h, i, cy + x, r, g, b);
            ppm_set_pixel(pixels, w, h, i, cy - x, r, g, b);
        }

        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

/* 递归光栅化单个对象到像素缓冲 */
static void rasterize_object_ppm(Lv00VisualObject* obj, unsigned char* pixels, int w, int h) {
    if (!obj) return;

    /* 提取颜色分量 */
    unsigned char sr = (unsigned char)(obj->style.stroke_color[0] * 255.0f);
    unsigned char sg = (unsigned char)(obj->style.stroke_color[1] * 255.0f);
    unsigned char sb = (unsigned char)(obj->style.stroke_color[2] * 255.0f);
    unsigned char fr = (unsigned char)(obj->style.fill_color[0] * 255.0f);
    unsigned char fg = (unsigned char)(obj->style.fill_color[1] * 255.0f);
    unsigned char fb_c = (unsigned char)(obj->style.fill_color[2] * 255.0f);

    switch (obj->type) {
    case LV00_VISUAL_POINT: {
        /* 点渲染为填充小圆 */
        int px = (int)obj->transform[12];
        int py = (int)(h - obj->transform[13]); /* Y 轴翻转 */
        ppm_fill_circle(pixels, w, h, px, py, 3, sr, sg, sb);
        break;
    }
    case LV00_VISUAL_LINE:
    case LV00_VISUAL_SEGMENT: {
        /* 线段使用 Bresenham 画线 */
        if (obj->render_cache) {
            float* ep = (float*)obj->render_cache;
            int x0 = (int)ep[0];
            int y0 = (int)(h - ep[1]); /* Y 轴翻转 */
            int x1 = (int)ep[2];
            int y1 = (int)(h - ep[3]);
            ppm_draw_line(pixels, w, h, x0, y0, x1, y1, sr, sg, sb);
        }
        break;
    }
    case LV00_VISUAL_CIRCLE: {
        /* 圆使用中点圆算法 */
        int cx = (int)obj->transform[12];
        int cy = (int)(h - obj->transform[13]); /* Y 轴翻转 */
        int r = obj->render_cache ? (int)(*(float*)obj->render_cache) : 10;
        ppm_draw_circle(pixels, w, h, cx, cy, r, sr, sg, sb);
        break;
    }
    case LV00_VISUAL_POLYGON: {
        /* 多边形：逐边画线 */
        if (obj->children_count > 0) {
            for (size_t i = 0; i < obj->children_count; i++) {
                Lv00VisualObject* curr = obj->children[i];
                Lv00VisualObject* next = obj->children[(i + 1) % obj->children_count];
                if (curr && next) {
                    int x0 = (int)curr->transform[12];
                    int y0 = (int)(h - curr->transform[13]);
                    int x1 = (int)next->transform[12];
                    int y1 = (int)(h - next->transform[13]);
                    ppm_draw_line(pixels, w, h, x0, y0, x1, y1, sr, sg, sb);
                }
            }
        }
        break;
    }
    case LV00_VISUAL_MOBJECT_GROUP: {
        /* 组合对象：递归光栅化子对象 */
        for (size_t i = 0; i < obj->children_count; i++) {
            rasterize_object_ppm(obj->children[i], pixels, w, h);
        }
        break;
    }
    default:
        /* 其他类型暂不渲染 */
        break;
    }
}

/* ============ 渲染器 ============ */

Lv00VisualRenderer* lv00_visual_renderer_create(Lv00RenderBackend backend, int width, int height) {
    Lv00VisualRenderer* renderer = (Lv00VisualRenderer*)lv00_malloc(sizeof(Lv00VisualRenderer));
    if (!renderer) return NULL;
    
    renderer->backend = backend;
    renderer->backend_ctx = NULL;
    renderer->dpi = 96.0f;
    renderer->width = width;
    renderer->height = height;
    
    return renderer;
}

void lv00_visual_render(Lv00VisualRenderer* renderer, Lv00VisualScene* scene, const char* output_path) {
    if (!renderer || !scene || !output_path) return;

    /* 根据 backend 选择渲染方式 */
    switch (renderer->backend) {
    case LV00_RENDER_SVG: {
        /* SVG 渲染：生成完整 SVG 文档并写入文件 */
        size_t cap = 4096;
        char* buf = (char*)lv00_malloc(cap);
        if (!buf) return;
        size_t pos = 0;

        /* SVG 文档头 */
        pos += snprintf(buf + pos, cap - pos,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<svg xmlns=\"http://www.w3.org/2000/svg\" "
            "width=\"%d\" height=\"%d\" "
            "viewBox=\"0 0 %d %d\">\n",
            renderer->width, renderer->height,
            renderer->width, renderer->height);

        /* 背景 */
        pos += snprintf(buf + pos, cap - pos,
            "  <rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n");

        /* 遍历场景中的所有对象并生成 SVG 元素 */
        for (size_t i = 0; i < scene->object_count; i++) {
            render_object_svg(scene->objects[i], &buf, &pos, &cap);
        }

        /* SVG 文档尾 */
        pos += snprintf(buf + pos, cap - pos, "</svg>\n");

        /* 写入输出文件 */
        write_output_to_file(output_path, buf);

        lv00_free_ptr(buf);
        break;
    }
    case LV00_RENDER_CAIRO: {
        /* Cairo 后端：生成 Cairo 脚本命令（非实际 Cairo API 调用，而是脚本格式） */
        size_t cap = 4096;
        char* buf = (char*)lv00_malloc(cap);
        if (!buf) return;
        size_t pos = 0;

        /* Cairo 脚本头：创建表面和上下文 */
        pos += snprintf(buf + pos, cap - pos,
            "/* Cairo 脚本 - 由 Lv-00 几何可视化生成 */\n"
            "#include <cairo.h>\n\n"
            "int main(void) {\n"
            "  /* 创建图像表面 %dx%d */\n"
            "  cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, %d, %d);\n"
            "  cairo_t *cr = cairo_create(surface);\n\n"
            "  /* 白色背景 */\n"
            "  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);\n"
            "  cairo_paint(cr);\n\n"
            "  /* 虚线样式定义 */\n"
            "  double dashes[] = {5.0, 3.0};\n\n",
            renderer->width, renderer->height,
            renderer->width, renderer->height);

        /* 遍历场景中的所有对象并生成 Cairo 命令 */
        for (size_t i = 0; i < scene->object_count; i++) {
            render_object_cairo(scene->objects[i], &buf, &pos, &cap);
        }

        /* Cairo 脚本尾：销毁资源并输出 */
        pos += snprintf(buf + pos, cap - pos,
            "  /* 销毁资源 */\n"
            "  cairo_destroy(cr);\n"
            "  cairo_surface_write_to_png(surface, \"%s\");\n"
            "  cairo_surface_destroy(surface);\n"
            "  return 0;\n"
            "}\n",
            output_path);

        /* 写入输出文件 */
        write_output_to_file(output_path, buf);

        lv00_free_ptr(buf);
        break;
    }
    case LV00_RENDER_THREEJS:
        /* Three.js HTML 生成已迁移至 UI 层。
           内核通过 lv00_protocol.h 提供结构化数据。 */
        break;
    case LV00_RENDER_TIKZ: {
        /* TikZ 后端：生成 TikZ/LaTeX 代码 */
        size_t cap = 4096;
        char* buf = (char*)lv00_malloc(cap);
        if (!buf) return;
        size_t pos = 0;

        /* TikZ 文档头：包含必要的 LaTeX 包 */
        pos += snprintf(buf + pos, cap - pos,
            "%% TikZ 图形 - 由 Lv-00 几何可视化生成\n"
            "\\documentclass[tikz,border=10pt]{standalone}\n"
            "\\usepackage{tikz}\n"
            "\\begin{document}\n"
            "\\begin{tikzpicture}[scale=1.0]\n\n");

        /* 遍历场景中的所有对象并生成 TikZ 命令 */
        for (size_t i = 0; i < scene->object_count; i++) {
            render_object_tikz(scene->objects[i], &buf, &pos, &cap);
        }

        /* TikZ 文档尾 */
        pos += snprintf(buf + pos, cap - pos,
            "\\end{tikzpicture}\n"
            "\\end{document}\n");

        /* 写入输出文件 */
        write_output_to_file(output_path, buf);

        lv00_free_ptr(buf);
        break;
    }
    case LV00_RENDER_PNG: {
        /* PNG 后端：生成 PPM (Portable Pixmap) 作为 PNG 回退格式 */
        int w = renderer->width;
        int h = renderer->height;

        /* 创建像素缓冲区 (width x height x 3 RGB) */
        size_t pixel_size = (size_t)w * h * 3;
        unsigned char* pixels = (unsigned char*)lv00_malloc(pixel_size);
        if (!pixels) return;

        /* 设置白色背景 */
        memset(pixels, 255, pixel_size);

        /* 遍历场景中的所有对象并光栅化到像素缓冲 */
        for (size_t i = 0; i < scene->object_count; i++) {
            rasterize_object_ppm(scene->objects[i], pixels, w, h);
        }

        /* 写入 PPM 文件 (P6 二进制格式) */
        FILE* fp = fopen(output_path, "wb");
        if (fp) {
            fprintf(fp, "P6\n%d %d\n255\n", w, h);
            size_t written = fwrite(pixels, 1, pixel_size, fp);
            if (written != pixel_size) {
                LV00_LOG_WARNING("PPM导出写入不完整（期望 %zu, 实际 %zu）", pixel_size, written);
            }
            fclose(fp);
        } else {
            LV00_LOG_WARNING("geo_visual: 无法打开输出文件 %s", output_path);
        }

        lv00_free_ptr(pixels);
        break;
    }
    default:
        break;
    }
}

/* ============ 清理 ============ */

void lv00_visual_object_destroy(Lv00VisualObject* obj) {
    if (!obj) return;
    
    if (obj->render_cache) {
        lv00_free_ptr(obj->render_cache);
    }
    
    if (obj->children) {
        for (size_t i = 0; i < obj->children_count; i++) {
            lv00_visual_object_destroy(obj->children[i]);
        }
        lv00_free_ptr(obj->children);
    }
    
    lv00_free_ptr(obj);
}

void lv00_visual_scene_destroy(Lv00VisualScene* scene) {
    if (!scene) return;

    for (size_t i = 0; i < scene->object_count; i++) {
        lv00_visual_object_destroy(scene->objects[i]);
    }
    lv00_free_ptr(scene->objects);
    lv00_free_ptr(scene);
}

void lv00_visual_renderer_destroy(Lv00VisualRenderer* renderer) {
    if (!renderer) return;
    lv00_free_ptr(renderer);
}
