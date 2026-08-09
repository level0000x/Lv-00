/**
 * @file geo_visual.h
 * @brief 几何可视化抽象层 -- 类型声明与公共 API
 *
 * 提供几何对象（点、线段、圆、多边形、组合对象）的创建、样式设置、
 * 空间变换以及多后端渲染（SVG / Cairo / Three.js / TikZ / PPM-PNG）能力。
 */

#ifndef lv_GEO_VISUAL_H
#define lv_GEO_VISUAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============ 枚举类型 ============ */

/** 可视化对象类型 */
typedef enum {
    lv_VISUAL_POINT = 0,    /**< 点 */
    lv_VISUAL_LINE,         /**< 直线（无限延伸） */
    lv_VISUAL_SEGMENT,      /**< 线段（有限端点） */
    lv_VISUAL_CIRCLE,       /**< 圆 */
    lv_VISUAL_POLYGON,      /**< 多边形 */
    lv_VISUAL_MOBJECT_GROUP /**< 组合对象（含子对象） */
} lvVisualType;

/** 渲染后端类型 */
typedef enum {
    lv_RENDER_SVG = 0, /**< SVG 矢量图 */
    lv_RENDER_CAIRO,   /**< Cairo 图形库脚本 */
    lv_RENDER_THREEJS, /**< Three.js (HTML/JavaScript) */
    lv_RENDER_TIKZ,    /**< TikZ/LaTeX */
    lv_RENDER_PNG      /**< PNG 位图（PPM 回退） */
} lvRenderBackend;

/* ============ 结构体类型 ============ */

/**
 * @brief 可视化样式
 *
 * 描述一个可视化对象的视觉属性：描边颜色、填充颜色、
 * 线宽、透明度以及是否虚线绘制。
 */
typedef struct {
    float stroke_color[4]; /**< 描边颜色 RGBA，各分量取值 [0, 1] */
    float fill_color[4];   /**< 填充颜色 RGBA，各分量取值 [0, 1] */
    float stroke_width;    /**< 描边线宽 */
    float opacity;         /**< 整体不透明度 [0, 1] */
    int dashed;            /**< 是否使用虚线 (0 = 实线, 1 = 虚线) */
} lvVisualStyle;

/**
 * @brief 可视化几何对象
 *
 * 场景图中的基本节点，可携带样式、4x4 变换矩阵、
 * 渲染缓存以及子对象列表（用于组合对象）。
 */
typedef struct lvVisualObject {
    lvVisualType type;                /**< 对象类型 */
    lvVisualStyle style;              /**< 视觉样式 */
    float transform[16];              /**< 4x4 列主序变换矩阵 */
    void *entity;                     /**< 关联的数学实体指针（可选） */
    void *render_cache;               /**< 渲染缓存（端点、半径等几何数据） */
    struct lvVisualObject **children; /**< 子对象数组（组合对象用） */
    size_t children_count;            /**< 子对象数量 */
} lvVisualObject;

/**
 * @brief 可视化场景
 *
 * 管理一组可视化对象，维护相机参数和动画时间线。
 */
typedef struct {
    lvVisualObject **objects; /**< 场景中的对象数组 */
    size_t object_count;      /**< 对象数量 */
    size_t object_capacity;   /**< 对象数组容量 */
    float camera_center[3];   /**< 相机中心坐标 (x, y, z) */
    float camera_zoom;        /**< 相机缩放倍率 */
    int is_3d;                /**< 是否为三维场景 */
    float current_time;       /**< 当前动画时间 */
    float total_duration;     /**< 动画总时长 */
} lvVisualScene;

/**
 * @brief 可视化渲染器
 *
 * 封装渲染后端选择、输出尺寸和 DPI 等参数。
 */
typedef struct {
    lvRenderBackend backend; /**< 渲染后端类型 */
    void *backend_ctx;       /**< 后端上下文（预留） */
    float dpi;               /**< 输出 DPI */
    int width;               /**< 输出宽度（像素） */
    int height;              /**< 输出高度（像素） */
} lvVisualRenderer;

/* ============ 对象构造器 ============ */

/** 创建一个点对象 */
lvVisualObject *lv_visual_point_create(float x, float y);

/** 创建一条线段对象（有限端点） */
lvVisualObject *lv_visual_line_create(float x1, float y1, float x2, float y2);

/** 创建一个圆对象 */
lvVisualObject *lv_visual_circle_create(float cx, float cy, float r);

/** 创建一个组合对象，将 objs 中的 n 个对象归为一组 */
lvVisualObject *lv_visual_group_create(lvVisualObject **objs, size_t n);

/* ============ 样式设置 ============ */

/** 将完整的样式应用到对象 */
void lv_visual_set_style(lvVisualObject *obj, const lvVisualStyle *style);

/** 设置对象描边颜色 (RGBA, 各分量 [0, 1]) */
void lv_visual_set_color(lvVisualObject *obj, float r, float g, float b, float a);

/** 设置对象是否使用虚线 */
void lv_visual_set_dashed(lvVisualObject *obj, int dashed);

/* ============ 空间变换 ============ */

/** 平移对象 */
void lv_visual_translate(lvVisualObject *obj, float dx, float dy, float dz);

/** 缩放对象 */
void lv_visual_scale(lvVisualObject *obj, float sx, float sy);

/** 旋转对象；axis 为 NULL 或零向量时默认绕 Z 轴旋转 */
void lv_visual_rotate(lvVisualObject *obj, float angle, float axis[3]);

/* ============ 场景管理 ============ */

/** 创建一个空场景 */
lvVisualScene *lv_visual_scene_create(void);

/** 向场景中添加一个对象 */
void lv_visual_scene_add(lvVisualScene *scene, lvVisualObject *obj);

/** 清空场景并销毁其中所有对象 */
void lv_visual_scene_clear(lvVisualScene *scene);

/** 设置场景相机参数 */
void lv_visual_scene_set_camera(lvVisualScene *scene, float cx, float cy, float cz, float zoom);

/* ============ 渲染器 ============ */

/** 创建渲染器 */
lvVisualRenderer *lv_visual_renderer_create(lvRenderBackend backend, int width, int height);

/** 执行渲染，将场景输出到指定路径 */
void lv_visual_render(lvVisualRenderer *renderer, lvVisualScene *scene, const char *output_path);

/* ============ 资源释放 ============ */

/** 销毁可视化对象及其子对象 */
void lv_visual_object_destroy(lvVisualObject *obj);

/** 销毁场景及其所有对象 */
void lv_visual_scene_destroy(lvVisualScene *scene);

/** 销毁渲染器 */
void lv_visual_renderer_destroy(lvVisualRenderer *renderer);

#endif /* lv_GEO_VISUAL_H */
