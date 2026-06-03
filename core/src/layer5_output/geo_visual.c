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
    /* 简化实现：仅支持 Z 轴旋转 */
    float c = cosf(angle);
    float s = sinf(angle);
    
    float rot[16];
    identity_matrix(rot);
    rot[0] = c;  rot[1] = -s;
    rot[4] = s;  rot[5] = c;
    
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
    /* 简化实现：仅输出 SVG 占位 */
    if (!renderer || !scene || !output_path) return;
    
    /* TODO: 实现实际渲染逻辑 */
    /* 根据 backend 选择渲染方式 */
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
