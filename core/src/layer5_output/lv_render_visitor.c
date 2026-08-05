/**
 * @file lv_render_visitor.c
 * @brief 渲染访问器 —— 场景图遍历与类型分发
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/lv_render_visitor.h"
#include "lv/geo_utils.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lv/lv_internal.h"

/* ========================================================================
 * 内部辅助：从 lvVisualObject 提取几何数据并应用相机变换
 * ======================================================================== */

/** 应用相机变换：将模型坐标映射到屏幕坐标 */
static void apply_camera(double *x, double *y, const lvVisualScene *scene) {
    double cx = (double)scene->camera_center[0];
    double cy = (double)scene->camera_center[1];
    double zoom = (double)scene->camera_zoom;
    *x = (*x - cx) * zoom + cx;
    *y = (*y - cy) * zoom + cy;
}

/** 从对象的 render_cache 中读取 float 数据到 double 数组 */
static bool get_cache_doubles(const lvVisualObject *obj, double *out, int count) {
    if (obj->render_cache == NULL)
        return false;
    float *fc = (float *)obj->render_cache;
    for (int i = 0; i < count; i++)
        out[i] = (double)fc[i];
    return true;
}

/* ========================================================================
 * VTable: 渲染处理器函数指针类型
 * ======================================================================== */

typedef bool (*RenderObjectHandler)(const lvVisualObject *obj, const lvVisualScene *scene, const lvRenderVisitor *visitor);

/* ── 各类型渲染处理器 ── */

static bool render_point(const lvVisualObject *obj, const lvVisualScene *scene, const lvRenderVisitor *visitor) {
    double cache[256];
    memset(cache, 0, sizeof(cache));
    if (!get_cache_doubles(obj, cache, 2))
        return true;
    apply_camera(&cache[0], &cache[1], scene);
    if (visitor->visit_point)
        return visitor->visit_point(visitor->user_data, cache[0], cache[1], &obj->style);
    return true;
}

static bool render_segment(const lvVisualObject *obj, const lvVisualScene *scene, const lvRenderVisitor *visitor) {
    double cache[256];
    memset(cache, 0, sizeof(cache));
    if (!get_cache_doubles(obj, cache, 4))
        return true;
    apply_camera(&cache[0], &cache[1], scene);
    apply_camera(&cache[2], &cache[3], scene);
    if (visitor->visit_segment)
        return visitor->visit_segment(visitor->user_data,
                                      cache[0], cache[1], cache[2], cache[3], &obj->style);
    return true;
}

static bool render_line(const lvVisualObject *obj, const lvVisualScene *scene, const lvRenderVisitor *visitor) {
    double cache[256];
    memset(cache, 0, sizeof(cache));
    if (!get_cache_doubles(obj, cache, 4))
        return true;
    /* 无限延伸直线：从端点沿方向扩展到场景范围 */
    double dx = cache[2] - cache[0];
    double dy = cache[3] - cache[1];
    double len = geo_distance_2d(cache[0], cache[1], cache[2], cache[3]);
    if (len < 1e-12)
        return true;
    double ux = dx / len, uy = dy / len;
    double t_max = 1000.0;
    cache[0] = cache[0] - ux * t_max;
    cache[1] = cache[1] - uy * t_max;
    cache[2] = cache[0] + ux * t_max * 2.0;
    cache[3] = cache[1] + uy * t_max * 2.0;
    apply_camera(&cache[0], &cache[1], scene);
    apply_camera(&cache[2], &cache[3], scene);
    if (visitor->visit_line)
        return visitor->visit_line(visitor->user_data,
                                   cache[0], cache[1], cache[2], cache[3], &obj->style);
    return true;
}

static bool render_circle(const lvVisualObject *obj, const lvVisualScene *scene, const lvRenderVisitor *visitor) {
    double cache[256];
    memset(cache, 0, sizeof(cache));
    if (!get_cache_doubles(obj, cache, 3))
        return true;
    apply_camera(&cache[0], &cache[1], scene);
    /* 半径也受相机缩放影响 */
    cache[2] *= (double)scene->camera_zoom;
    if (visitor->visit_circle)
        return visitor->visit_circle(visitor->user_data,
                                     cache[0], cache[1], cache[2], &obj->style);
    return true;
}

static bool render_polygon(const lvVisualObject *obj, const lvVisualScene *scene, const lvRenderVisitor *visitor) {
    if (obj->render_cache == NULL)
        return true;
    int pcount = ((int *)obj->render_cache)[0];
    float *verts = (float *)((int *)obj->render_cache + 1);
    if (pcount < 3 || pcount > 128)
        return true;
    /* 转换为 double 并应用相机 */
    double pts[256];
    for (int j = 0; j < pcount; j++) {
        pts[j * 2]     = (double)verts[j * 2];
        pts[j * 2 + 1] = (double)verts[j * 2 + 1];
        apply_camera(&pts[j * 2], &pts[j * 2 + 1], scene);
    }
    if (visitor->visit_polygon)
        return visitor->visit_polygon(visitor->user_data, pts, pcount, &obj->style);
    return true;
}

/* ── 渲染处理器查找表 ── */
static const RenderObjectHandler kRenderObjectHandlers[] = {
    [lv_VISUAL_POINT]    = render_point,
    [lv_VISUAL_SEGMENT]  = render_segment,
    [lv_VISUAL_LINE]     = render_line,
    [lv_VISUAL_CIRCLE]   = render_circle,
    [lv_VISUAL_POLYGON]  = render_polygon,
};

/* ========================================================================
 * 递归遍历单个对象
 * ======================================================================== */

/**
 * @brief 递归遍历一个可视化对象，调用 visitor 回调
 * @return false 表示被回调中止
 */
static bool traverse_object(const lvRenderVisitor *visitor,
                            const lvVisualObject *obj,
                            const lvVisualScene *scene) {
    if (obj == NULL)
        return true;

    /* ── 组合对象：递归遍历子对象 ── */
    if (obj->type == lv_VISUAL_MOBJECT_GROUP) {
        if (visitor->begin_group && !visitor->begin_group(visitor->user_data, "group"))
            return false;
        for (size_t i = 0; i < obj->children_count; i++) {
            if (!traverse_object(visitor, obj->children[i], scene))
                return false;
        }
        if (visitor->end_group && !visitor->end_group(visitor->user_data))
            return false;
        return true;
    }

    /* 通过 VTable 分发到具体类型的渲染处理器 */
    if (obj->type >= 0 && obj->type < (int)(sizeof(kRenderObjectHandlers)/sizeof(kRenderObjectHandlers[0])) && kRenderObjectHandlers[obj->type]) {
        return kRenderObjectHandlers[obj->type](obj, scene, visitor);
    }
    return true;
}

/* ========================================================================
 * 公共 API: lv_render_scene
 * ======================================================================== */

bool lv_render_scene(const lvRenderVisitor *visitor, const lvVisualScene *scene) {
    if (visitor == NULL || scene == NULL)
        return false;

    /* 场景标题 */
    char title[64];
    int n = snprintf(title, sizeof(title), "Lv-00 Scene (%zu objects)", scene->object_count);
    if (n < 0 || (size_t)n >= sizeof(title))
        snprintf(title, sizeof(title), "Lv-00 Scene");

    /* 开始场景 */
    if (visitor->begin_scene && !visitor->begin_scene(visitor->user_data, title,
                                                       scene->objects ? 800 : 800,
                                                       scene->objects ? 600 : 600))
        return false;

    /* 遍历场景中每个顶层对象 */
    for (size_t i = 0; i < scene->object_count; i++) {
        if (!traverse_object(visitor, scene->objects[i], scene))
            return false;
    }

    /* 结束场景 */
    if (visitor->end_scene && !visitor->end_scene(visitor->user_data))
        return false;

    return true;
}
