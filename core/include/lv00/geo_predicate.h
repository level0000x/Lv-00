/**
 * @file geo_predicate.h
 * @brief 精确几何谓词 —— 借鉴 CGAL Exact Predicate Paradigm
 *
 * 借鉴来源：
 *   - CGAL (github.com/CGAL/cgal)
 *     精确谓词范式（Predicate/Construction 分离）、Kernel 抽象层
 *   - Boost.Geometry (boost.org/libs/geometry)
 *     策略模式（可定制距离/侧边判定策略）
 *
 * 设计目标：
 *   - 几何判定（orientation/side_of_circle）保证精确性
 *   - 通过 PredicateMode 支持精确/近似/区间三种精度模式
 *   - 与现有 SymbolicCoord 系统无缝衔接
 *
 * 版本：v3.6.0（第十三梯队 CGAL + Boost.Geometry 落地）
 */

#ifndef LV00_GEO_PREDICATE_H
#define LV00_GEO_PREDICATE_H

#include <stdbool.h>
#include <stdint.h>

#include "lv00.h"
#include "geometry_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 第一部分：精度模式与 Kernel 抽象（借鉴 CGAL Kernel 层）
 *
 * CGAL 将几何操作分为两类：
 *   - Predicate（谓词）：返回离散结果（是/否），必须精确
 *   - Construction（构造）：返回几何对象，可近似
 *
 * Lv-00 适配：
 *   - LV00_PREDICATE_EXACT：使用区间算术保证精确判定
 *   - LV00_PREDICATE_APPROX：使用浮点快速判定（可能有误差）
 *   - LV00_PREDICATE_ADAPTIVE：自适应精度（先浮点，不确定时切换精确）
 * ======================================================================== */

/**
 * @brief 谓词精度模式
 */
typedef enum {
    LV00_PREDICATE_EXACT,     /**< 精确模式：使用区间算术，保证结果正确 */
    LV00_PREDICATE_APPROX,    /**< 近似模式：使用浮点运算，速度快 */
    LV00_PREDICATE_ADAPTIVE,  /**< 自适应模式：先浮点，不确定时切换精确 */
    LV00_PREDICATE_SYMBOLIC   /**< 符号模式：使用 SymbolicCoord 精确计算 */
} Lv00PredicateMode;

/**
 * @brief 方向判定结果（借鉴 CGAL Orientation）
 */
typedef enum {
    LV00_ORIENTATION_LEFT,        /**< 逆时针方向（正方向） */
    LV00_ORIENTATION_RIGHT,       /**< 顺时针方向（负方向） */
    LV00_ORIENTATION_COLLINEAR,   /**< 三点共线 */
    LV00_ORIENTATION_COPLANAR,    /**< 四点共面（3D 专用） */
    LV00_ORIENTATION_DEGENERATE   /**< 退化情况（如两点重合） */
} Lv00Orientation;

/**
 * @brief 点与圆的位置关系（借鉴 CGAL side_of_circle）
 */
typedef enum {
    LV00_SIDE_INSIDE,     /**< 点在圆内 */
    LV00_SIDE_ON,         /**< 点在圆上 */
    LV00_SIDE_OUTSIDE,    /**< 点在圆外 */
    LV00_SIDE_DEGENERATE  /**< 退化情况（如圆半径为零） */
} Lv00SideOfCircle;

/**
 * @brief 点与直线的位置关系
 */
typedef enum {
    LV00_LINE_SIDE_LEFT,       /**< 点在直线左侧 */
    LV00_LINE_SIDE_RIGHT,      /**< 点在直线右侧 */
    LV00_LINE_SIDE_ON,         /**< 点在直线上 */
    LV00_LINE_SIDE_DEGENERATE  /**< 退化情况 */
} Lv00LineSide;

/**
 * @brief 谓词统计信息（用于性能分析）
 */
typedef struct {
    int exact_count;       /**< 精确计算次数 */
    int approx_count;      /**< 近似计算次数 */
    int adaptive_fallback; /**< 自适应回退次数 */
} Lv00PredicateStats;

/* ========================================================================
 * 第二部分：2D 精确谓词 API
 * ======================================================================== */

/**
 * @brief 判定三点方向（2D orientation test）
 *
 * 计算有符号面积的两倍：(p2-p1) × (p3-p1)
 *   > 0 → LEFT（逆时针）
 *   < 0 → RIGHT（顺时针）
 *   = 0 → COLLINEAR（共线）
 *
 * @param p1x, p1y 第一个点坐标
 * @param p2x, p2y 第二个点坐标
 * @param p3x, p3y 第三个点坐标
 * @param mode     精度模式
 * @return 方向判定结果
 *
 * @note 借鉴 CGAL 的 orientation_2_object()
 */
LV00_PUBLIC_API Lv00Orientation lv00_orientation_2d(
    double p1x, double p1y,
    double p2x, double p2y,
    double p3x, double p3y,
    Lv00PredicateMode mode);

/**
 * @brief 判定四点方向（3D orientation test）
 *
 * 计算四面体有符号体积的六倍：
 *   det | p2-p1  p3-p1  p4-p1 |
 *   > 0 → 正方向
 *   < 0 → 负方向
 *   = 0 → 共面
 *
 * @param p1x, p1y, p1z 第一个点坐标
 * @param p2x, p2y, p2z 第二个点坐标
 * @param p3x, p3y, p3z 第三个点坐标
 * @param p4x, p4y, p4z 第四个点坐标
 * @param mode           精度模式
 * @return 方向判定结果
 */
LV00_PUBLIC_API Lv00Orientation lv00_orientation_3d(
    double p1x, double p1y, double p1z,
    double p2x, double p2y, double p2z,
    double p3x, double p3y, double p3z,
    double p4x, double p4y, double p4z,
    Lv00PredicateMode mode);

/**
 * @brief 判定点相对于直线的位置
 *
 * @param px, py 点坐标
 * @param lx1, ly1 直线上第一个点
 * @param lx2, ly2 直线上第二个点
 * @param mode       精度模式
 * @return 点相对于直线的位置
 */
LV00_PUBLIC_API Lv00LineSide lv00_line_side(
    double px, double py,
    double lx1, double ly1,
    double lx2, double ly2,
    Lv00PredicateMode mode);

/**
 * @brief 判定点相对于有向线段的位置
 *
 * @param px, py 点坐标
 * @param sx1, sy1 线段起点
 * @param sx2, sy2 线段终点
 * @param mode       精度模式
 * @return 点相对于有向线段的位置
 */
LV00_PUBLIC_API Lv00LineSide lv00_segment_side(
    double px, double py,
    double sx1, double sy1,
    double sx2, double sy2,
    Lv00PredicateMode mode);

/**
 * @brief 判定点相对于圆的位置
 *
 * 使用有符号距离的平方：|p - c|^2 - r^2
 *   < 0 → INSIDE
 *   = 0 → ON
 *   > 0 → OUTSIDE
 *
 * @param px, py 点坐标
 * @param cx, cy 圆心坐标
 * @param r       圆半径
 * @param mode    精度模式
 * @return 点相对于圆的位置
 *
 * @note 借鉴 CGAL 的 side_of_oriented_circle_2_object()
 */
LV00_PUBLIC_API Lv00SideOfCircle lv00_side_of_circle(
    double px, double py,
    double cx, double cy,
    double r,
    Lv00PredicateMode mode);

/**
 * @brief 判定两点是否在直线同侧
 *
 * @param ax, ay 第一个点坐标
 * @param bx, by 第二个点坐标
 * @param lx1, ly1 直线上第一个点
 * @param lx2, ly2 直线上第二个点
 * @param mode       精度模式
 * @return true 两点在直线同侧
 */
LV00_PUBLIC_API bool lv00_same_side_of_line(
    double ax, double ay,
    double bx, double by,
    double lx1, double ly1,
    double lx2, double ly2,
    Lv00PredicateMode mode);

/**
 * @brief 判定两点是否在圆同侧
 *
 * @param ax, ay 第一个点坐标
 * @param bx, by 第二个点坐标
 * @param cx, cy 圆心坐标
 * @param r       圆半径
 * @param mode    精度模式
 * @return true 两点在圆同侧
 */
LV00_PUBLIC_API bool lv00_same_side_of_circle(
    double ax, double ay,
    double bx, double by,
    double cx, double cy,
    double r,
    Lv00PredicateMode mode);

/**
 * @brief 判定两条线段是否相交（精确判定）
 *
 * 使用方向谓词判定，避免浮点误差。
 *
 * @param ax, ay 第一条线段起点
 * @param bx, by 第一条线段终点
 * @param cx, cy 第二条线段起点
 * @param dx, dy 第二条线段终点
 * @param mode    精度模式
 * @return true 两线段相交
 */
LV00_PUBLIC_API bool lv00_segments_intersect(
    double ax, double ay,
    double bx, double by,
    double cx, double cy,
    double dx, double dy,
    Lv00PredicateMode mode);

/**
 * @brief 判定点是否在三角形内部（含边界）
 *
 * 使用三个方向谓词判定：所有方向一致则点在内部。
 *
 * @param px, py 点坐标
 * @param ax, ay 三角形顶点 A
 * @param bx, by 三角形顶点 B
 * @param cx, cy 三角形顶点 C
 * @param mode    精度模式
 * @return true 点在三角形内部（含边界）
 */
LV00_PUBLIC_API bool lv00_point_in_triangle(
    double px, double py,
    double ax, double ay,
    double bx, double by,
    double cx, double cy,
    Lv00PredicateMode mode);

/**
 * @brief 判定四点是否共圆
 *
 * 使用行列式判定：
 *   | ax  ay  ax^2+ay^2  1 |
 *   | bx  by  bx^2+by^2  1 | = 0
 *   | cx  cy  cx^2+cy^2  1 |
 *   | dx  dy  dx^2+dy^2  1 |
 *
 * @param ax, ay 点 A 坐标
 * @param bx, by 点 B 坐标
 * @param cx, cy 点 C 坐标
 * @param dx, dy 点 D 坐标
 * @param mode    精度模式
 * @return true 四点共圆
 */
LV00_PUBLIC_API bool lv00_four_points_concyclic(
    double ax, double ay,
    double bx, double by,
    double cx, double cy,
    double dx, double dy,
    Lv00PredicateMode mode);

/**
 * @brief 判定多边形是否为凸多边形
 *
 * 检查所有连续三顶点的方向是否一致（全部同向或全部反向）。
 *
 * @param xs, ys 顶点坐标数组
 * @param n       顶点数量
 * @param mode    精度模式
 * @return true 多边形为凸
 */
LV00_PUBLIC_API bool lv00_polygon_is_convex(
    const double *xs, const double *ys, int n,
    Lv00PredicateMode mode);

/**
 * @brief 判定点是否在凸多边形内部
 *
 * 使用二分法判定，O(log n) 复杂度。
 * 要求多边形顶点按逆时针或顺时针顺序排列。
 *
 * @param px, py 点坐标
 * @param xs, ys 多边形顶点坐标数组
 * @param n       顶点数量
 * @param mode    精度模式
 * @return true 点在凸多边形内部（含边界）
 */
LV00_PUBLIC_API bool lv00_point_in_convex_polygon(
    double px, double py,
    const double *xs, const double *ys, int n,
    Lv00PredicateMode mode);

/**
 * @brief 判定点是否在任意多边形内部（射线法）
 *
 * 从点发出水平射线，统计与多边形边的交点数。
 * 奇数交点 → 内部，偶数交点 → 外部。
 *
 * @param px, py 点坐标
 * @param xs, ys 多边形顶点坐标数组
 * @param n       顶点数量
 * @param mode    精度模式
 * @return true 点在多边形内部（含边界）
 */
LV00_PUBLIC_API bool lv00_point_in_polygon(
    double px, double py,
    const double *xs, const double *ys, int n,
    Lv00PredicateMode mode);

/* ========================================================================
 * 第三部分：谓词统计与配置
 * ======================================================================== */

/**
 * @brief 获取谓词统计信息
 * @param stats 输出统计信息
 */
LV00_PUBLIC_API void lv00_predicate_get_stats(Lv00PredicateStats *stats);

/**
 * @brief 重置谓词统计信息
 */
LV00_PUBLIC_API void lv00_predicate_reset_stats(void);

/**
 * @brief 设置全局谓词精度模式
 * @param mode 精度模式
 */
LV00_PUBLIC_API void lv00_predicate_set_mode(Lv00PredicateMode mode);

/**
 * @brief 获取全局谓词精度模式
 * @return 当前精度模式
 */
LV00_PUBLIC_API Lv00PredicateMode lv00_predicate_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_PREDICATE_H */
