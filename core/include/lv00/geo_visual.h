/**
 * @file geo_visual.h
 * @brief 几何可视化抽象层 - 基于 Manim Mobject 设计
 * 
 * 提供统一的视觉对象抽象，支持从符号表示到视觉渲染的透明转换
 * 
 * @author Lv-00 Project
 * @version 1.0
 */

#ifndef LV00_GEO_VISUAL_H
#define LV00_GEO_VISUAL_H

#include <lv00.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============ 前向声明 ============ */
typedef struct Lv00GeometryEntity Lv00GeometryEntity;
typedef struct Lv00VisualObject Lv00VisualObject;
typedef struct Lv00VisualScene Lv00VisualScene;
typedef struct Lv00VisualRenderer Lv00VisualRenderer;

/* ============ 可视化对象类型 ============ */
typedef enum {
    LV00_VISUAL_POINT,
    LV00_VISUAL_LINE,
    LV00_VISUAL_SEGMENT,
    LV00_VISUAL_CIRCLE,
    LV00_VISUAL_ARC,
    LV00_VISUAL_POLYGON,
    LV00_VISUAL_CURVE,
    LV00_VISUAL_VECTOR,
    LV00_VISUAL_MATHTEX,  /* LaTeX 公式 */
    LV00_VISUAL_TEXT,
    LV00_VISUAL_MOBJECT_GROUP  /* 组合对象 */
} Lv00VisualType;

/* ============ 样式属性 ============ */
typedef struct {
    float stroke_width;      /* 线条宽度 */
    float stroke_color[4];   /* RGBA 颜色 */
    float fill_color[4];     /* 填充颜色 */
    float opacity;          /* 透明度 */
    int dashed;             /* 是否虚线 */
} Lv00VisualStyle;

/* ============ 可视化对象 (Mobject 风格) ============ */
struct Lv00VisualObject {
    Lv00VisualType type;
    Lv00VisualStyle style;
    
    /* 几何数据（与底层几何实体关联） */
    Lv00GeometryEntity* entity;
    
    /* 可选：预计算的渲染数据 */
    void* render_cache;
    
    /* 组合支持 */
    struct Lv00VisualObject** children;
    size_t children_count;
    
    /* 变换矩阵 (4x4) */
    float transform[16];
};

/* ============ 证明场景（对应 Manim 的 Scene） ============ */
struct Lv00VisualScene {
    /* 场景中的对象列表 */
    Lv00VisualObject** objects;
    size_t object_count;
    
    /* 相机/视角设置 */
    float camera_center[3];
    float camera_zoom;
    int is_3d;
    
    /* 时间轴（用于动画） */
    float current_time;
    float total_duration;
};

/* ============ 渲染器后端 ============ */
typedef enum {
    LV00_RENDER_CAIRO,      /* 2D 矢量渲染 */
    LV00_RENDER_SVG,        /* SVG 导出 */
    LV00_RENDER_THREEJS,    /* Web 3D 渲染 */
    LV00_RENDER_TIKZ,       /* TikZ/LaTeX 渲染 */
    LV00_RENDER_PNG         /* 位图渲染 */
} Lv00RenderBackend;

struct Lv00VisualRenderer {
    Lv00RenderBackend backend;
    void* backend_ctx;  /* 后端特定上下文 */
    float dpi;
    int width;
    int height;
};

/* ============ API 声明 ============ */

/* 构造器 */
Lv00VisualObject* lv00_visual_point_create(float x, float y);
Lv00VisualObject* lv00_visual_point_create_3d(float x, float y, float z);
Lv00VisualObject* lv00_visual_line_create(float x1, float y1, float x2, float y2);
Lv00VisualObject* lv00_visual_circle_create(float cx, float cy, float r);
Lv00VisualObject* lv00_visual_polygon_create(float** coords, size_t n);
Lv00VisualObject* lv00_visual_mathexpr_create(const char* latex);
Lv00VisualObject* lv00_visual_group_create(Lv00VisualObject** objs, size_t n);

/* 样式设置 */
void lv00_visual_set_style(Lv00VisualObject* obj, const Lv00VisualStyle* style);
void lv00_visual_set_color(Lv00VisualObject* obj, float r, float g, float b, float a);
void lv00_visual_set_dashed(Lv00VisualObject* obj, int dashed);

/* 变换 */
void lv00_visual_transform(Lv00VisualObject* obj, float matrix[16]);
void lv00_visual_rotate(Lv00VisualObject* obj, float angle, float axis[3]);
void lv00_visual_scale(Lv00VisualObject* obj, float sx, float sy);
void lv00_visual_translate(Lv00VisualObject* obj, float dx, float dy, float dz);

/* 几何实体绑定 */
void lv00_visual_bind_entity(Lv00VisualObject* obj, Lv00GeometryEntity* entity);

/* 场景管理 */
Lv00VisualScene* lv00_visual_scene_create(void);
void lv00_visual_scene_add(Lv00VisualScene* scene, Lv00VisualObject* obj);
void lv00_visual_scene_remove(Lv00VisualScene* scene, Lv00VisualObject* obj);
void lv00_visual_scene_clear(Lv00VisualScene* scene);
void lv00_visual_scene_set_camera(Lv00VisualScene* scene, float cx, float cy, float cz, float zoom);

/* 渲染 */
Lv00VisualRenderer* lv00_visual_renderer_create(Lv00RenderBackend backend, int width, int height);
void lv00_visual_render(Lv00VisualRenderer* renderer, Lv00VisualScene* scene, const char* output_path);
void lv00_visual_render_frame(Lv00VisualRenderer* renderer, Lv00VisualScene* scene);

/* 清理 */
void lv00_visual_object_destroy(Lv00VisualObject* obj);
void lv00_visual_scene_destroy(Lv00VisualScene* scene);
void lv00_visual_renderer_destroy(Lv00VisualRenderer* renderer);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_VISUAL_H */
