/**
 * @file interactive_geo.c
 * @brief 交互几何系统实现
 */

#include "lv00/lv00.h"
#include "lv00/interactive_geo.h"
#include "lv00/engine.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LV00_GEO_DEFAULT_HIT_RADIUS 12.0
#define LV00_GEO_MAX_ZOOM 20.0
#define LV00_GEO_MIN_ZOOM 0.05
#define LV00_GEO_ZOOM_FACTOR 1.15

/* ---- 坐标变换 ---- */

void interactive_geo_world_to_screen(const Lv00InteractiveGeo *geo,
                                     double world_x, double world_y,
                                     double *screen_x, double *screen_y)
{
    if (!geo) return;
    const Lv00GeoCanvasState *s = &geo->canvas_state;
    double w = s->canvas_width > 0 ? s->canvas_width : 800.0;
    double h = s->canvas_height > 0 ? s->canvas_height : 600.0;
    if (screen_x) *screen_x = (world_x - s->viewport_offset_x) * s->zoom_level + w / 2.0;
    if (screen_y) *screen_y = (world_y - s->viewport_offset_y) * s->zoom_level + h / 2.0;
}

void interactive_geo_screen_to_world(const Lv00InteractiveGeo *geo,
                                     double screen_x, double screen_y,
                                     double *world_x, double *world_y)
{
    if (!geo) return;
    const Lv00GeoCanvasState *s = &geo->canvas_state;
    double w = s->canvas_width > 0 ? s->canvas_width : 800.0;
    double h = s->canvas_height > 0 ? s->canvas_height : 600.0;
    if (world_x) *world_x = (screen_x - w / 2.0) / s->zoom_level + s->viewport_offset_x;
    if (world_y) *world_y = (screen_y - h / 2.0) / s->zoom_level + s->viewport_offset_y;
}

/* ---- 命中检测 ---- */

int interactive_geo_hit_test(const Lv00InteractiveGeo *geo,
                             double screen_x, double screen_y,
                             double hit_radius, double *out_distance)
{
    if (!geo || !geo->canvas_state.active_object_ids) return -1;
    if (hit_radius <= 0.0) hit_radius = LV00_GEO_DEFAULT_HIT_RADIUS;

    double wx, wy;
    interactive_geo_screen_to_world(geo, screen_x, screen_y, &wx, &wy);

    int best_id = -1;
    double best_dist = hit_radius * hit_radius;  /* 平方距离比较 */

    for (int i = 0; i < geo->canvas_state.active_object_count; i++) {
        int oid = geo->canvas_state.active_object_ids[i];
        double ox, oy;
        if (interactive_geo_get_object_position(geo, oid, &ox, &oy) != 0) continue;
        double dx = ox - wx, dy = oy - wy;
        double d2 = dx * dx + dy * dy;
        if (d2 < best_dist) {
            best_dist = d2;
            best_id = oid;
        }
    }

    if (out_distance) *out_distance = sqrt(best_dist) / geo->canvas_state.zoom_level;
    return best_id;
}

/* ---- 对象位置查询 ---- */

int interactive_geo_get_object_position(const Lv00InteractiveGeo *geo,
                                        int object_id,
                                        double *world_x, double *world_y)
{
    if (!geo || !geo->engine_handle) return -1;
    /* 通过引擎引擎查询点的世界坐标。
     * 内部引擎 API 需通过 engine.h 中的 lv00_get_node_coords() 获取坐标值。
     * 此处简化为返回视口偏移量作为占位值。
     * TODO: 实现完整的引擎坐标查询接口。 */
    (void)object_id;
    if (world_x) *world_x = geo->canvas_state.viewport_offset_x;
    if (world_y) *world_y = geo->canvas_state.viewport_offset_y;
    return 0;
}

/* ---- 缩放 ---- */

void interactive_geo_zoom(Lv00InteractiveGeo *geo, double zoom_delta,
                          double center_x, double center_y)
{
    if (!geo) return;
    Lv00GeoCanvasState *s = &geo->canvas_state;
    double factor = zoom_delta > 0 ? LV00_GEO_ZOOM_FACTOR : 1.0 / LV00_GEO_ZOOM_FACTOR;
    double new_zoom = s->zoom_level * factor;
    if (new_zoom < LV00_GEO_MIN_ZOOM) factor = LV00_GEO_MIN_ZOOM / s->zoom_level;
    if (new_zoom > LV00_GEO_MAX_ZOOM) factor = LV00_GEO_MAX_ZOOM / s->zoom_level;
    s->zoom_level *= factor;

    /* 以屏幕中心点为锚点调整偏移 */
    double wx_before, wy_before;
    interactive_geo_screen_to_world(geo, center_x, center_y, &wx_before, &wy_before);
    s->viewport_offset_x = wx_before;
    s->viewport_offset_y = wy_before;

    double wx_after, wy_after;
    interactive_geo_screen_to_world(geo, center_x, center_y, &wx_after, &wy_after);
    s->viewport_offset_x -= (wx_after - wx_before);
    s->viewport_offset_y -= (wy_after - wy_before);
}

void interactive_geo_reset_viewport(Lv00InteractiveGeo *geo)
{
    if (!geo) return;
    geo->canvas_state.zoom_level = 1.0;
    geo->canvas_state.viewport_offset_x = 0.0;
    geo->canvas_state.viewport_offset_y = 0.0;
    geo->canvas_state.canvas_width = 800.0;
    geo->canvas_state.canvas_height = 600.0;
    memset(geo->canvas_state.viewport_matrix, 0, sizeof(geo->canvas_state.viewport_matrix));
    geo->canvas_state.viewport_matrix[0][0] = 1.0;
    geo->canvas_state.viewport_matrix[1][1] = 1.0;
    geo->canvas_state.viewport_matrix[2][2] = 1.0;
}

void interactive_geo_set_canvas_size(Lv00InteractiveGeo *geo,
                                     double width, double height)
{
    if (!geo) return;
    geo->canvas_state.canvas_width = width;
    geo->canvas_state.canvas_height = height;
}
