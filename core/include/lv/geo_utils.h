/**
 * @file geo_utils.h
 * @brief 几何工具函数 —— 便捷聚合头文件
 *
 * @details 将几何相关的符号坐标操作函数聚合到一个头文件中，
 * 方便其他模块统一包含。当前聚合的模块包括：
 * - symbolic_coord.h：符号坐标类型定义与操作（比较、转换等）
 * - constraint_graph.h：约束图数据结构（几何节点、约束等）
 *
 * @version 1.1.0
 */

#ifndef lv_GEO_UTILS_H
#define lv_GEO_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 符号坐标操作函数（symbolic_coord_compare, symbolic_coord_to_double 等）
 * 均声明在 symbolic_coord.h 中，此处通过包含该头文件提供统一入口。
 */
#include "symbolic_coord.h"

/*
 * 约束图数据结构和几何节点类型（ConstraintGraph, GeomNode, Constraint 等）
 * 均声明在 constraint_graph.h 中。
 */
#include "constraint_graph.h"

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ========================================================================
 * 几何计算工具函数（实现位于 layer3_geometry/geo_utils.c）
 * ======================================================================== */

lv_PUBLIC_API double geo_distance_2d(double x1, double y1, double x2, double y2);
lv_PUBLIC_API int geo_approx_equal(double a, double b, double eps);
lv_PUBLIC_API int geo_point_on_segment(double px, double py, double x1, double y1, double x2, double y2);
lv_PUBLIC_API double geo_signed_area_2x(double x1, double y1, double x2, double y2, double x3, double y3);
lv_PUBLIC_API double geo_angle(double x1, double y1, double x2, double y2);
lv_PUBLIC_API int geo_segments_intersect(double ax1, double ay1, double ax2, double ay2, double bx1, double by1,
                                         double bx2, double by2);

#ifdef __cplusplus
}
#endif

#endif /* lv_GEO_UTILS_H */
