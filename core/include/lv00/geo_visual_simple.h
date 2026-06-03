/**
 * @file geo_visual_simple.h
 * @brief 简化版几何可视化抽象层（独立测试用）
 */

#ifndef LV00_GEO_VISUAL_SIMPLE_H
#define LV00_GEO_VISUAL_SIMPLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 基础类型定义 */
typedef struct Lv00GeometryEntity Lv00GeometryEntity;
typedef struct Lv00VisualObject Lv00VisualObject;
typedef struct Lv00VisualScene Lv00VisualScene;
typedef struct Lv00VisualRenderer Lv00VisualRenderer;

/* 可视化对象类型 */
typedef enum {
    LV00_VISUAL_POINT,
    LV00_VISUAL_LINE,
    LV00_VISUAL_SEGMENT,
    LV00_VISUAL_CIRCLE,
    LV00_VISUAL_ARC,
    LV00_VISUAL_POLYGON,
    LV00_VISUAL_CURVE,
    LV00_VISUAL_VECTOR,
    LV00_VISUAL_MATHTEX,
    LV00_VISUAL_TEXT,
    LV00_VISUAL_MOBJECT_GROUP
} Lv00VisualType;

/* 样式属性 */
typedef struct {
    float stroke_width;
    float stroke_color[4];
    float fill_color[4];
    float opacity;
    int dashed;
} Lv00VisualStyle;

/* 可视化对象 */
struct Lv00VisualObject {
    Lv00VisualType type;
    Lv00VisualStyle style;
    Lv00GeometryEntity* entity;
    void* render_cache;
    Lv00VisualObject** children;
    size_t children_count;
    float transform[16];
};

/* 场景 */
struct Lv00VisualScene {
    Lv00VisualObject** objects;
    size_t object_count;
    float camera_center[3];
    float camera_zoom;
    int is_3d;
    float current_time;
    float total_duration;
};

/* 渲染器后端 */
typedef enum {
    LV00_RENDER_CAIRO,
    LV00_RENDER_SVG,
    LV00_RENDER_THREEJS,
    LV00_RENDER_TIKZ,
    LV00_RENDER_PNG
} Lv00RenderBackend;

struct Lv00VisualRenderer {
    Lv00RenderBackend backend;
    void* backend_ctx;
    float dpi;
    int width;
    int height;
};

/* API */
Lv00VisualObject* lv00_visual_point_create(float x, float y);
Lv00VisualObject* lv00_visual_line_create(float x1, float y1, float x2, float y2);
Lv00VisualObject* lv00_visual_circle_create(float cx, float cy, float r);
Lv00VisualObject* lv00_visual_group_create(Lv00VisualObject** objs, size_t n);

void lv00_visual_set_style(Lv00VisualObject* obj, const Lv00VisualStyle* style);
void lv00_visual_set_color(Lv00VisualObject* obj, float r, float g, float b, float a);
void lv00_visual_set_dashed(Lv00VisualObject* obj, int dashed);

void lv00_visual_translate(Lv00VisualObject* obj, float dx, float dy, float dz);
void lv00_visual_scale(Lv00VisualObject* obj, float sx, float sy);
void lv00_visual_rotate(Lv00VisualObject* obj, float angle, float axis[3]);

Lv00VisualScene* lv00_visual_scene_create(void);
void lv00_visual_scene_add(Lv00VisualScene* scene, Lv00VisualObject* obj);
void lv00_visual_scene_clear(Lv00VisualScene* scene);
void lv00_visual_scene_set_camera(Lv00VisualScene* scene, float cx, float cy, float cz, float zoom);

Lv00VisualRenderer* lv00_visual_renderer_create(Lv00RenderBackend backend, int width, int height);

void lv00_visual_object_destroy(Lv00VisualObject* obj);
void lv00_visual_scene_destroy(Lv00VisualScene* scene);
void lv00_visual_renderer_destroy(Lv00VisualRenderer* renderer);

#ifdef __cplusplus
}
#endif

#endif
