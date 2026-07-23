#ifndef lv_GEO_PREDICATE_H
#define lv_GEO_PREDICATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* ========================================================================
 * 谓词精度模式
 * ======================================================================== */

/**
 * @brief 几何谓词精度模式
 *
 * 四种精度模式：
 *   - lv_PREDICATE_EXACT：区间算术保证精确判定
 *   - lv_PREDICATE_APPROX：浮点运算快速判定
 *   - lv_PREDICATE_ADAPTIVE：自适应精度（先浮点，不确定时切换精确）
 *   - lv_PREDICATE_SYMBOLIC：符号计算模式（暂回退到精确模式）
 */
typedef enum {
    lv_PREDICATE_EXACT = 0,
    lv_PREDICATE_APPROX = 1,
    lv_PREDICATE_ADAPTIVE = 2,
    lv_PREDICATE_SYMBOLIC = 3
} lvPredicateMode;

/* ========================================================================
 * 方向枚举
 * ======================================================================== */

/**
 * @brief 方向判定结果
 *
 * 用于 2D/3D 方向谓词的返回值：
 *   - lv_ORIENTATION_LEFT：左侧（逆时针 / 正体积）
 *   - lv_ORIENTATION_RIGHT：右侧（顺时针 / 负体积）
 *   - lv_ORIENTATION_COLLINEAR：共线（2D）
 *   - lv_ORIENTATION_COPLANAR：共面（3D）
 *   - lv_ORIENTATION_DEGENERATE：退化（输入点重合等）
 */
typedef enum {
    lv_ORIENTATION_LEFT = -1,
    lv_ORIENTATION_COLLINEAR = 0,
    lv_ORIENTATION_COPLANAR = 0,
    lv_ORIENTATION_RIGHT = 1,
    lv_ORIENTATION_DEGENERATE = 2
} lvOrientation;

/* 兼容旧名称 */
#define lv_ORIENT_LEFT lv_ORIENTATION_LEFT
#define lv_ORIENT_COLLINEAR lv_ORIENTATION_COLLINEAR
#define lv_ORIENT_RIGHT lv_ORIENTATION_RIGHT

/* ========================================================================
 * 直线/线段侧边枚举
 * ======================================================================== */

/**
 * @brief 点相对于有向直线/线段的位置
 */
typedef enum {
    lv_LINE_SIDE_LEFT = -1,
    lv_LINE_SIDE_ON = 0,
    lv_LINE_SIDE_RIGHT = 1,
    lv_LINE_SIDE_DEGENERATE = 2
} lvLineSide;

/* ========================================================================
 * 圆侧边枚举
 * ======================================================================== */

/**
 * @brief 点相对于圆的位置
 */
typedef enum { lv_SIDE_INSIDE = -1, lv_SIDE_ON = 0, lv_SIDE_OUTSIDE = 1, lv_SIDE_DEGENERATE = 2 } lvSideOfCircle;

/* ========================================================================
 * 谓词统计信息
 * ======================================================================== */

/**
 * @brief 谓词调用统计信息
 */
typedef struct {
    size_t approx_count;
    size_t exact_count;
    size_t adaptive_fallback;
} lvPredicateStats;

/* ========================================================================
 * 公共 API 导出宏
 * ======================================================================== */

#ifndef lv_PUBLIC_API
#define lv_PUBLIC_API
#endif

/* ========================================================================
 * 方向谓词
 * ======================================================================== */

/**
 * @brief 2D 方向谓词（三点方向判定）
 *
 * 计算有符号面积的两倍：(p2-p1) x (p3-p1)
 *   > 0 -> LEFT（逆时针）
 *   < 0 -> RIGHT（顺时针）
 *   = 0 -> COLLINEAR（共线）
 */
lv_PUBLIC_API lvOrientation lv_orientation_2d(double p1x, double p1y, double p2x, double p2y, double p3x, double p3y,
                                              lvPredicateMode mode);

/**
 * @brief 3D 方向谓词（四点方向判定）
 *
 * 计算四面体有符号体积的六倍（3x3 行列式）。
 *   > 0 -> LEFT（正体积）
 *   < 0 -> RIGHT（负体积）
 *   = 0 -> COPLANAR（共面）
 */
lv_PUBLIC_API lvOrientation lv_orientation_3d(double p1x, double p1y, double p1z, double p2x, double p2y, double p2z,
                                              double p3x, double p3y, double p3z, double p4x, double p4y, double p4z,
                                              lvPredicateMode mode);

/* 兼容旧函数名 */
#define lv_orient2d(ax, ay, bx, by, cx, cy) lv_orientation_2d(ax, ay, bx, by, cx, cy, lv_PREDICATE_ADAPTIVE)
#define lv_orient3d(ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz) \
    lv_orientation_3d(ax, ay, az, bx, by, bz, cx, cy, cz, dx, dy, dz, lv_PREDICATE_ADAPTIVE)

/* ========================================================================
 * 直线/线段谓词
 * ======================================================================== */

/**
 * @brief 判定点相对于有向直线的位置
 */
lv_PUBLIC_API lvLineSide lv_line_side(double px, double py, double lx1, double ly1, double lx2, double ly2,
                                      lvPredicateMode mode);

/**
 * @brief 判定点相对于有向线段的位置
 */
lv_PUBLIC_API lvLineSide lv_segment_side(double px, double py, double sx1, double sy1, double sx2, double sy2,
                                         lvPredicateMode mode);

/**
 * @brief 判定两点是否在直线同侧
 */
lv_PUBLIC_API bool lv_same_side_of_line(double ax, double ay, double bx, double by, double lx1, double ly1, double lx2,
                                        double ly2, lvPredicateMode mode);

/**
 * @brief 判定两条线段是否相交
 */
lv_PUBLIC_API bool lv_segments_intersect(double ax, double ay, double bx, double by, double cx, double cy, double dx,
                                         double dy, lvPredicateMode mode);

/* ========================================================================
 * 圆谓词
 * ======================================================================== */

/**
 * @brief 判定点相对于圆的位置（内/上/外）
 *
 * 使用有符号距离的平方：|p - c|^2 - r^2
 *   < 0 -> INSIDE
 *   = 0 -> ON
 *   > 0 -> OUTSIDE
 */
lv_PUBLIC_API lvSideOfCircle lv_side_of_circle(double px, double py, double cx, double cy, double r,
                                               lvPredicateMode mode);

/**
 * @brief 判定两点是否在圆同侧
 */
lv_PUBLIC_API bool lv_same_side_of_circle(double ax, double ay, double bx, double by, double cx, double cy, double r,
                                          lvPredicateMode mode);

/**
 * @brief 判定四点是否共圆
 */
lv_PUBLIC_API bool lv_four_points_concyclic(double ax, double ay, double bx, double by, double cx, double cy, double dx,
                                            double dy, lvPredicateMode mode);

/* 兼容旧函数名 */
#define lv_incircle(ax, ay, bx, by, cx, cy, dx, dy) \
    (lv_four_points_concyclic(ax, ay, bx, by, cx, cy, dx, dy, lv_PREDICATE_ADAPTIVE) ? 0.0 : 1.0)

/* ========================================================================
 * 三角形谓词
 * ======================================================================== */

/**
 * @brief 判定点是否在三角形内部（含边界）
 */
lv_PUBLIC_API bool lv_point_in_triangle(double px, double py, double ax, double ay, double bx, double by, double cx,
                                        double cy, lvPredicateMode mode);

/* ========================================================================
 * 多边形谓词
 * ======================================================================== */

/**
 * @brief 判定多边形是否为凸多边形
 */
lv_PUBLIC_API bool lv_polygon_is_convex(const double *xs, const double *ys, int n, lvPredicateMode mode);

/**
 * @brief 判定点是否在凸多边形内部（含边界）
 *
 * 使用二分法，O(log n) 复杂度。
 * 要求多边形顶点按逆时针或顺时针顺序排列。
 */
lv_PUBLIC_API bool lv_point_in_convex_polygon(double px, double py, const double *xs, const double *ys, int n,
                                              lvPredicateMode mode);

/**
 * @brief 判定点是否在任意简单多边形内部（含边界）
 *
 * 使用射线法（ray casting algorithm），O(n) 复杂度。
 */
lv_PUBLIC_API bool lv_point_in_polygon(double px, double py, const double *xs, const double *ys, int n,
                                       lvPredicateMode mode);

/* ========================================================================
 * 谓词统计与配置
 * ======================================================================== */

/**
 * @brief 获取谓词统计信息
 */
lv_PUBLIC_API void lv_predicate_get_stats(lvPredicateStats *stats);

/**
 * @brief 重置谓词统计信息
 */
lv_PUBLIC_API void lv_predicate_reset_stats(void);

/**
 * @brief 设置全局谓词精度模式
 */
lv_PUBLIC_API void lv_predicate_set_mode(lvPredicateMode mode);

/**
 * @brief 获取全局谓词精度模式
 */
lv_PUBLIC_API lvPredicateMode lv_predicate_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif
