/**
 * @file geometry_ops.h
 * @brief 蓝图几何运算接口（TEN_LAYER_OPTIMIZED_PLAN §12.3/12.5/12.6/12.7 落地）
 *
 * 规划文档中的几何函数（lv_point_xxx / lv_segment_xxx / lv_triangle_xxx /
 * lv_intersect_xxx）以 SymbolicCoord* 单指针表示"点"，无法承载 2D 坐标；
 * 本实现按库内 SymbolicCoord 标量语义落地为**坐标分量对**接口：每个点
 * 由 (x, y) 两个 SymbolicCoord* 分量表达，输出同理。函数名与蓝图一致，
 * 语义等价（distance_sq/midpoint 为符号精度；谓词/交点含 double 容差路径）。
 *
 * 所有权：所有 out_* 输出均为 [take]（新分配），调用者负责 lv_free；
 * 失败返回 false 且不写入输出。
 */

#ifndef lv_GEOMETRY_OPS_H
#define lv_GEOMETRY_OPS_H

#include <stdbool.h>
#include <stddef.h>

#include "symbolic_coord.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/* ============ Point（§12.3 R1-R3） ============ */

/** @brief 两点距离平方（符号精度）：(bx-ax)^2 + (by-ay)^2 */
lv_PUBLIC_API bool lv_point_distance_sq(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                        const SymbolicCoord *by, SymbolicCoord **out_result);

/** @brief 中点（符号精度）：(a+b)/2，输出 (out_x, out_y) */
lv_PUBLIC_API bool lv_point_midpoint(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                     const SymbolicCoord *by, SymbolicCoord **out_x, SymbolicCoord **out_y);

/**
 * @brief 共线检测：(b-a)×(c-a) 判零
 *
 * 符号精度判零（symbolic_coord_is_zero）；坐标非精确类型时回退
 * double 叉积 + tolerance 判定。
 */
lv_PUBLIC_API bool lv_point_is_collinear(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                         const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                                         bool *out_result, double tolerance);

/* ============ Segment（§12.7 R8） ============ */

/** @brief 线段长度平方（符号精度）= 两点距离平方 */
lv_PUBLIC_API bool lv_segment_length_sq(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                        const SymbolicCoord *by, SymbolicCoord **out);

/** @brief 线段中点（符号精度），输出 (out_x, out_y) */
lv_PUBLIC_API bool lv_segment_midpoint(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                       const SymbolicCoord *by, SymbolicCoord **out_x, SymbolicCoord **out_y);

/** @brief 线段方向向量 d = b - a（符号精度），输出 (out_dx, out_dy) */
lv_PUBLIC_API bool lv_segment_direction(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                        const SymbolicCoord *by, SymbolicCoord **out_dx, SymbolicCoord **out_dy);

/** @brief 两线段平行：方向叉积 (d1)×(d2) 判零（符号/容差双路径） */
lv_PUBLIC_API bool lv_segment_is_parallel(const SymbolicCoord *a1x, const SymbolicCoord *a1y,
                                          const SymbolicCoord *a2x, const SymbolicCoord *a2y,
                                          const SymbolicCoord *b1x, const SymbolicCoord *b1y,
                                          const SymbolicCoord *b2x, const SymbolicCoord *b2y, bool *out,
                                          double tolerance);

/** @brief 两线段垂直：方向点积判零（符号/容差双路径） */
lv_PUBLIC_API bool lv_segment_is_perpendicular(const SymbolicCoord *a1x, const SymbolicCoord *a1y,
                                               const SymbolicCoord *a2x, const SymbolicCoord *a2y,
                                               const SymbolicCoord *b1x, const SymbolicCoord *b1y,
                                               const SymbolicCoord *b2x, const SymbolicCoord *b2y, bool *out,
                                               double tolerance);

/** @brief 线段相交（含端点）：参数化交点；输出 (out_x, out_y)。不相交返回 false */
lv_PUBLIC_API bool lv_segment_intersection(const SymbolicCoord *a1x, const SymbolicCoord *a1y,
                                           const SymbolicCoord *a2x, const SymbolicCoord *a2y,
                                           const SymbolicCoord *b1x, const SymbolicCoord *b1y,
                                           const SymbolicCoord *b2x, const SymbolicCoord *b2y,
                                           SymbolicCoord **out_x, SymbolicCoord **out_y);

/** @brief 点是否在线段上（含端点）：共线且坐标在包围盒内 */
lv_PUBLIC_API bool lv_segment_contains_point(const SymbolicCoord *a1x, const SymbolicCoord *a1y,
                                             const SymbolicCoord *a2x, const SymbolicCoord *a2y,
                                             const SymbolicCoord *px, const SymbolicCoord *py, bool *out,
                                             double tolerance);

/** @brief 点到线段距离（输出距离值，符号精度不可行时 double 构造） */
lv_PUBLIC_API bool lv_segment_distance_to_point(const SymbolicCoord *a1x, const SymbolicCoord *a1y,
                                                const SymbolicCoord *a2x, const SymbolicCoord *a2y,
                                                const SymbolicCoord *px, const SymbolicCoord *py,
                                                SymbolicCoord **out_distance);

/* ============ Triangle（§12.6 R7） ============ */

/** @brief 三角形有符号面积（叉积/2 的绝对值；符号精度：叉积=2S 判号后取半） */
lv_PUBLIC_API bool lv_triangle_area(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                    const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                                    SymbolicCoord **out_area);

/** @brief 外心（数值实现，double → 有理数构造；[take] out_center_x/y） */
lv_PUBLIC_API bool lv_triangle_circumcenter(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                            const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                                            SymbolicCoord **out_x, SymbolicCoord **out_y);

/** @brief 垂心（数值实现） */
lv_PUBLIC_API bool lv_triangle_orthocenter(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                           const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                                           SymbolicCoord **out_x, SymbolicCoord **out_y);

/** @brief 内心（数值实现：角平分线/边长加权） */
lv_PUBLIC_API bool lv_triangle_incenter(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                        const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                                        SymbolicCoord **out_x, SymbolicCoord **out_y);

/** @brief 重心（符号精度：(a+b+c)/3） */
lv_PUBLIC_API bool lv_triangle_centroid(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                        const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                                        SymbolicCoord **out_x, SymbolicCoord **out_y);

/** @brief 九点圆心（数值实现：外心与垂心中点） */
lv_PUBLIC_API bool lv_triangle_nine_point_center(const SymbolicCoord *ax, const SymbolicCoord *ay,
                                                 const SymbolicCoord *bx, const SymbolicCoord *by,
                                                 const SymbolicCoord *cx, const SymbolicCoord *cy,
                                                 SymbolicCoord **out_x, SymbolicCoord **out_y);

/** @brief 旁心（数值实现：A 角旁心） */
lv_PUBLIC_API bool lv_triangle_excenter(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                        const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                                        SymbolicCoord **out_x, SymbolicCoord **out_y);

/** @brief 内切圆半径（数值实现） */
lv_PUBLIC_API bool lv_triangle_inradius(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                        const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                                        SymbolicCoord **out_radius);

/** @brief 外接圆半径（数值实现） */
lv_PUBLIC_API bool lv_triangle_circumradius(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                            const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                                            SymbolicCoord **out_radius);

/* ============ Intersect（§12.5 R5-R6） ============ */

/** @brief 两直线交点（点+方向表示；符号不可行时数值）：输出 (out_x, out_y)；平行返回 false 并置 out_parallel */
lv_PUBLIC_API bool lv_intersect_lines(const SymbolicCoord *p1x, const SymbolicCoord *p1y, const SymbolicCoord *d1x,
                                      const SymbolicCoord *d1y, const SymbolicCoord *p2x, const SymbolicCoord *p2y,
                                      const SymbolicCoord *d2x, const SymbolicCoord *d2y, SymbolicCoord **out_x,
                                      SymbolicCoord **out_y, bool *out_parallel);

/** @brief 两圆交点（圆心+半径平方；数值实现）：out_count 0/1/2 */
lv_PUBLIC_API bool lv_intersect_circles(const SymbolicCoord *c1x, const SymbolicCoord *c1y, const SymbolicCoord *r1_sq,
                                        const SymbolicCoord *c2x, const SymbolicCoord *c2y, const SymbolicCoord *r2_sq,
                                        SymbolicCoord **out_p1x, SymbolicCoord **out_p1y, SymbolicCoord **out_p2x,
                                        SymbolicCoord **out_p2y, int *out_count);

/** @brief 直线与圆交点（数值实现）：out_count 0/1/2 */
lv_PUBLIC_API bool lv_intersect_line_circle(const SymbolicCoord *line_px, const SymbolicCoord *line_py,
                                            const SymbolicCoord *line_dx, const SymbolicCoord *line_dy,
                                            const SymbolicCoord *circle_cx, const SymbolicCoord *circle_cy,
                                            const SymbolicCoord *circle_r2, SymbolicCoord **out_p1x,
                                            SymbolicCoord **out_p1y, SymbolicCoord **out_p2x,
                                            SymbolicCoord **out_p2y, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* lv_GEOMETRY_OPS_H */
