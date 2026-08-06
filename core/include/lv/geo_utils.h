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
 * 数值容差常量（分级汇总）
 *
 * 统一引用 lv_utils.h / config.h 的权威常量，禁止在实现文件中本地再定义数值。
 * 收敛说明（几何层收敛任务）：
 *   - GEO_EPSILON（1e-12）引用 lv_EPSILON_DOUBLE（原 geo_utils.c 已是引用，上移汇总）；
 *   - GEO_ANGLE_EPSILON 原 geo_utils.c 本地定义 1e-9，与 config.h 的
 *     lv_GEO_ANGLE_EPSILON（1e-10）不一致（代码注释曾自曝）——已统一引用 config 权威值；
 *     该宏无使用点，实际判定行为不受影响。
 *   - 私有 epsilon（geo_predicate.c APPROX_EPSILON / ADAPTIVE_THRESHOLD、
 *     geo_constraint_solver_internal.h NUMERICAL_DIFF_EPSILON 等）按语义
 *     分别引用 lv_EPSILON_MEDIUM 或保留本地定义，见各文件。
 * ======================================================================== */
#include "lv_utils.h" /* lv_EPSILON_DOUBLE / lv_EPSILON_NEWTON 等 */
#include "config.h"   /* lv_GEO_ANGLE_EPSILON / lv_EPSILON_MEDIUM 等 */

#define GEO_EPSILON lv_EPSILON_DOUBLE          /**< 浮点比较容差（1e-12，= lv_EPSILON_DOUBLE） */
#define GEO_ANGLE_EPSILON lv_GEO_ANGLE_EPSILON /**< 角度比较容差（1e-10，= config.h 权威值） */

/* ========================================================================
 * 几何计算工具函数（实现位于 layer3_geometry/geo_utils.c）
 * ======================================================================== */

lv_PUBLIC_API double geo_distance_2d(double x1, double y1, double x2, double y2);
lv_PUBLIC_API double geo_distance_3d(double x1, double y1, double z1, double x2, double y2, double z2);
lv_PUBLIC_API double geo_norm_2d(double dx, double dy);
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
