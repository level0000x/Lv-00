/**
 * @file geo_visual.c
 * @brief 几何可视化抽象层实现
 */

#include "lv00/geo_visual.h"
#include "lv00_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============ 内部工具函数 ============ */

static void identity_matrix(float m[16]) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
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
        obj->children = (Lv00VisualObject**)lv00_malloc(n * sizeof(Lv00VisualObject*));
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
    Lv00VisualObject** new_objects = (Lv00VisualObject**)lv00_malloc(new_count * sizeof(Lv00VisualObject*));
    
    if (scene->objects) {
        memcpy(new_objects, scene->objects, scene->object_count * sizeof(Lv00VisualObject*));
        lv00_free(scene->objects);
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
    
    lv00_free(scene->objects);
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

    /* 辅助宏：向缓冲区追加字符串 */
    #define SVG_APPEND(fmt, ...) do { \
        int written = snprintf(*buf + *pos, *cap - *pos, fmt, ##__VA_ARGS__); \
        if (written > 0) { \
            *pos += (size_t)written; \
            if (*pos >= *cap) { \
                *cap *= 2; \
                char* new_buf = (char*)lv00_realloc(*buf, *cap); \
                if (new_buf) *buf = new_buf; \
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
        FILE* fp = fopen(output_path, "w");
        if (fp) {
            fputs(buf, fp);
            fclose(fp);
        }

        lv00_free(buf);
        break;
    }
    case LV00_RENDER_CAIRO:
        /* Cairo 后端：暂未实现 */
        break;
    case LV00_RENDER_THREEJS:
        /* Three.js 后端：暂未实现 */
        break;
    case LV00_RENDER_TIKZ:
        /* TikZ 后端：暂未实现 */
        break;
    case LV00_RENDER_PNG:
        /* PNG 后端：暂未实现 */
        break;
    default:
        break;
    }
}

/* ============ 清理 ============ */

void lv00_visual_object_destroy(Lv00VisualObject* obj) {
    if (!obj) return;
    
    if (obj->render_cache) {
        lv00_free(obj->render_cache);
    }
    
    if (obj->children) {
        for (size_t i = 0; i < obj->children_count; i++) {
            lv00_visual_object_destroy(obj->children[i]);
        }
        lv00_free(obj->children);
    }
    
    lv00_free(obj);
}

void lv00_visual_scene_destroy(Lv00VisualScene* scene) {
    if (!scene) return;
    
    lv00_free(scene->objects);
    lv00_free(scene);
}

void lv00_visual_renderer_destroy(Lv00VisualRenderer* renderer) {
    if (!renderer) return;
    lv00_free(renderer);
}
