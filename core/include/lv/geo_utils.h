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
 * ========================================================================
 * geo_norm_* 家族（批次 P2 收敛，2026-08-12）契约卡：
 *   语义契约：计算向量模长（欧氏 2-范数）；geo_norm_sq_* 返回模长平方
 *              （省去 sqrt，用于距离比较 / 阈值判断），其余返回 sqrt 模长。
 *   前置条件：无（普通标量输入，不检查指针 / NaN）。
 *   失败 / 截断语义：无失败路径；float 变体（geo_norm_2df）按 float 精度
 *              计算并返回 float，与 sqrtf 语义逐位一致。
 *   边界行为：负分量与零向量均正常；零向量返回 0。
 *   扩展点：无（维度后缀 _2d/_3d 即完整家族；如需 3D float 模长，
 *              按 geo_norm_3df 同型扩展）。
 */

lv_PUBLIC_API double geo_distance_2d(double x1, double y1, double x2, double y2);
lv_PUBLIC_API double geo_distance_3d(double x1, double y1, double z1, double x2, double y2, double z2);
lv_PUBLIC_API double geo_norm_2d(double dx, double dy);
lv_PUBLIC_API double geo_norm_3d(double dx, double dy, double dz);
lv_PUBLIC_API double geo_norm_sq_2d(double dx, double dy);
lv_PUBLIC_API double geo_norm_sq_3d(double dx, double dy, double dz);
lv_PUBLIC_API float geo_norm_2df(float dx, float dy);
lv_PUBLIC_API int geo_approx_equal(double a, double b, double eps);

/**
 * @brief 判断点 (px, py) 是否落在线段 (ax,ay)-(bx,by) 的轴向包围盒内（含 epsilon 容差）
 *
 * 收敛说明（批次 Q 组⑦ P14）：收敛 lv_segments_intersect 内 8 处 +
 * recursion_selector.c 1 处手写的「fmin/fmax + eps」二维 bbox 检查样板。
 *
 * @return 在 bbox 内返回 1，否则返回 0
 */
lv_PUBLIC_API int geo_bbox_contains_2d(double px, double py, double ax, double ay, double bx, double by, double eps);

/**
 * @brief 判断值 p 是否落在区间 [min(a,b), max(a,b)] 内（含 epsilon 容差）
 *
 * 一维轴向包围盒变体，供 lv_point_in_polygon 的水平边分支使用。
 *
 * @return 在区间内返回 1，否则返回 0
 */
lv_PUBLIC_API int geo_bbox_contains_1d(double p, double a, double b, double eps);

/**
 * @brief 射线法判断点 (px, py) 是否位于线段列表围成的区域内（奇数交点）
 *
 * 收敛说明（批次 T F-2）：收敛 func_block_selector.c 的 point_in_region 与
 * recursion_selector.c 的 point_in_region_ray_casting 两份射线法副本。
 * 采用更稳健的 lv_is_zero(dy, lv_EPSILON_ULTRA) 防卫跳过近水平退化边
 * （原 recursion 版本缺少该防卫，存在近零分母风险）；半开区间条件保证
 * 顶点不被重复计数。
 *
 * @param px, py    查询点浮点坐标
 * @param segments  区域边界线段数组（GEOM_LINE_SEGMENT），可含多环 / 非闭合
 * @param seg_count 线段数量
 * @return true 点在区域内（奇数交点）；false 参数无效或偶数交点
 */
lv_PUBLIC_API bool geo_point_in_region_segments(double px, double py, GeomNode **segments, int seg_count);

lv_PUBLIC_API int geo_point_on_segment(double px, double py, double x1, double y1, double x2, double y2);
lv_PUBLIC_API double geo_signed_area_2x(double x1, double y1, double x2, double y2, double x3, double y3);
lv_PUBLIC_API double geo_angle(double x1, double y1, double x2, double y2);
lv_PUBLIC_API int geo_segments_intersect(double ax1, double ay1, double ax2, double ay2, double bx1, double by1,
                                         double bx2, double by2);

#ifdef __cplusplus
}
#endif

#endif /* lv_GEO_UTILS_H */
