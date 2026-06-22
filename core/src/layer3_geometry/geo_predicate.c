/**
 * @file geo_predicate.c
 * @brief 精确几何谓词实现 —— 借鉴 CGAL Exact Predicate Paradigm
 *
 * 实现所有在 geo_predicate.h 中声明的几何谓词函数。
 * 支持三种精度模式：
 *   - LV00_PREDICATE_EXACT：区间算术保证精确判定
 *   - LV00_PREDICATE_APPROX：浮点运算快速判定
 *   - LV00_PREDICATE_ADAPTIVE：自适应精度（先浮点，不确定时切换精确）
 *
 * 借鉴来源：
 *   - CGAL (github.com/CGAL/cgal) 精确谓词范式
 *   - Jonathan Shewchuk, "Adaptive Precision Floating-Point Arithmetic and
 *     Fast Robust Predicates for Computational Geometry"
 *   - Boost.Geometry 策略模式
 *
 * @version 3.6.0
 */

#include "lv00/geo_predicate.h"
#include "lv00/geometry_config.h"
#include "lv00/interval_arithmetic.h"

#include <math.h>
#include <string.h>
#include <float.h>

/* 确保 LV00_PUBLIC_API 已定义 */
#ifndef LV00_PUBLIC_API
#define LV00_PUBLIC_API
#endif

/* ========================================================================
 * 内部辅助宏与常量
 * ======================================================================== */

/**
 * @brief 自适应模式中浮点结果"接近零"的阈值。
 *
 * 当浮点结果的绝对值小于此阈值时，回退到精确（区间）计算。
 * 该值取为 DBL_EPSILON 的 256 倍，覆盖大多数退化情况下的浮点误差。
 */
#define ADAPTIVE_THRESHOLD (256.0 * DBL_EPSILON)

/**
 * @brief 近似模式中使用的默认容差。
 * 当浮点结果绝对值小于此阈值时判定为零。
 */
#define APPROX_EPSILON (1e-9)

/* ========================================================================
 * 全局状态
 * ======================================================================== */

/** @brief 全局默认精度模式，初始为自适应 */
static Lv00PredicateMode g_predicate_mode = LV00_PREDICATE_ADAPTIVE;

/** @brief 全局谓词统计信息 */
static Lv00PredicateStats g_predicate_stats = {0, 0, 0};

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 使用区间算术精确计算 2D 方向谓词（有符号面积的两倍）
 *
 * 计算结果为区间 [lo, hi]：
 *   - lo > 0 → LEFT
 *   - hi < 0 → RIGHT
 *   - lo <= 0 && hi >= 0 → COLLINEAR（无法排除零）
 *
 * @return 区间表示的有符号面积
 */
static Lv00Interval orientation_2d_exact_interval(
    double p1x, double p1y,
    double p2x, double p2y,
    double p3x, double p3y)
{
    /* (p2 - p1) x (p3 - p1) = (p2x-p1x)*(p3y-p1y) - (p3x-p1x)*(p2y-p1y) */
    Lv00Interval dx2 = interval_sub(interval_point(p2x), interval_point(p1x));
    Lv00Interval dy2 = interval_sub(interval_point(p2y), interval_point(p1y));
    Lv00Interval dx3 = interval_sub(interval_point(p3x), interval_point(p1x));
    Lv00Interval dy3 = interval_sub(interval_point(p3y), interval_point(p1y));

    /* cross = dx2 * dy3 - dx3 * dy2 */
    Lv00Interval term1 = interval_mul(dx2, dy3);
    Lv00Interval term2 = interval_mul(dx3, dy2);
    Lv00Interval result = interval_sub(term1, term2);

    return result;
}

/**
 * @brief 使用区间算术精确计算 3D 方向谓词（四面体有符号体积的六倍）
 *
 * 计算行列式：
 *   | p2x-p1x  p3x-p1x  p4x-p1x |
 *   | p2y-p1y  p3y-p1y  p4y-p1y |
 *   | p2z-p1z  p3z-p1z  p4z-p1z |
 *
 * 使用 Sarrus / Laplace 展开计算 3x3 行列式。
 */
static Lv00Interval orientation_3d_exact_interval(
    double p1x, double p1y, double p1z,
    double p2x, double p2y, double p2z,
    double p3x, double p3y, double p3z,
    double p4x, double p4y, double p4z)
{
    /* 构造三个列向量（区间） */
    Lv00Interval ax = interval_sub(interval_point(p2x), interval_point(p1x));
    Lv00Interval ay = interval_sub(interval_point(p2y), interval_point(p1y));
    Lv00Interval az = interval_sub(interval_point(p2z), interval_point(p1z));

    Lv00Interval bx = interval_sub(interval_point(p3x), interval_point(p1x));
    Lv00Interval by = interval_sub(interval_point(p3y), interval_point(p1y));
    Lv00Interval bz = interval_sub(interval_point(p3z), interval_point(p1z));

    Lv00Interval cx = interval_sub(interval_point(p4x), interval_point(p1x));
    Lv00Interval cy = interval_sub(interval_point(p4y), interval_point(p1y));
    Lv00Interval cz = interval_sub(interval_point(p4z), interval_point(p1z));

    /* det = a*(b x c) */
    /* b x c = (by*cz - bz*cy, bz*cx - bx*cz, bx*cy - by*cx) */
    /* dot(a, bxc) = ax*(by*cz - bz*cy) - ay*(bx*cz - bz*cx) + az*(bx*cy - by*cx) */

    Lv00Interval bycz = interval_mul(by, cz);
    Lv00Interval bzcy = interval_mul(bz, cy);
    Lv00Interval bxcy = interval_mul(bx, cy);
    Lv00Interval bycx = interval_mul(by, cx);
    Lv00Interval bxcz = interval_mul(bx, cz);
    Lv00Interval bzcx = interval_mul(bz, cx);

    Lv00Interval term1 = interval_mul(ax, interval_sub(bycz, bzcy));
    Lv00Interval term2 = interval_mul(ay, interval_sub(bxcz, bzcx));
    Lv00Interval term3 = interval_mul(az, interval_sub(bxcy, bycx));

    return interval_add(interval_sub(term1, term2), term3);
}

/**
 * @brief 使用区间算术精确计算点与圆的位置关系
 *
 * 计算 |p - c|^2 - r^2 的区间。
 */
static Lv00Interval side_of_circle_exact_interval(
    double px, double py,
    double cx, double cy,
    double r)
{
    Lv00Interval dx = interval_sub(interval_point(px), interval_point(cx));
    Lv00Interval dy = interval_sub(interval_point(py), interval_point(cy));
    Lv00Interval dist_sq = interval_add(interval_mul(dx, dx), interval_mul(dy, dy));
    Lv00Interval r_sq = interval_mul(interval_point(r), interval_point(r));
    return interval_sub(dist_sq, r_sq);
}

/**
 * @brief 使用区间算术精确计算四点共圆行列式
 *
 * 行列式：
 *   | ax  ay  ax^2+ay^2  1 |
 *   | bx  by  bx^2+by^2  1 |
 *   | cx  cy  cx^2+cy^2  1 |
 *   | dx  dy  dx^2+dy^2  1 |
 *
 * 使用 Laplace 展开沿最后一列展开。
 */
static Lv00Interval four_points_concyclic_exact_interval(
    double ax, double ay,
    double bx, double by,
    double cx, double cy,
    double dx, double dy)
{
    /* 计算 ax^2+ay^2 等 */
    Lv00Interval a2 = interval_add(interval_mul(interval_point(ax), interval_point(ax)),
                                   interval_mul(interval_point(ay), interval_point(ay)));
    Lv00Interval b2 = interval_add(interval_mul(interval_point(bx), interval_point(bx)),
                                   interval_mul(interval_point(by), interval_point(by)));
    Lv00Interval c2 = interval_add(interval_mul(interval_point(cx), interval_point(cx)),
                                   interval_mul(interval_point(cy), interval_point(cy)));
    Lv00Interval d2 = interval_add(interval_mul(interval_point(dx), interval_point(dx)),
                                   interval_mul(interval_point(dy), interval_point(dy)));

    /*
     * Laplace 展开沿最后一列（全为 1）：
     * det = M11 - M21 + M31 - M41
     *
     * 其中 Mij 是去掉第 i 行、第 j 列的 3x3 子行列式。
     *
     * M11 = | by  b2  1 |  = by*(c2*1 - 1*dy) - b2*(cy*1 - 1*dy) + 1*(cy*dy - c2*dy)
     *       | cy  c2  1 |
     *       | dy  d2  1 |
     *
     * 简化 3x3 行列式（最后一列全为 1）：
     * M11 = by*(c2 - d2) + b2*(dy - cy) + (cy*d2 - c2*dy)
     * M21 = bx*(c2 - d2) + a2*(dy - cy) + (cy*d2 - c2*dy)  -- 不对，重新推导
     *
     * 实际上对于 3x3 行列式（最后一列为 1）：
     * | a b 1 |
     * | c d 1 | = a*(d - f) - b*(c - e) + (c*f - d*e)
     * | e f 1 |
     *
     * M11: a=by, b=b2, c=cy, d=c2, e=dy, f=d2
     *     = by*(c2 - d2) - b2*(cy - dy) + (cy*d2 - c2*dy)
     *
     * M21: a=bx, b=a2, c=cx, d=c2, e=dx, f=d2
     *     = bx*(c2 - d2) - a2*(cx - dx) + (cx*d2 - c2*dx)
     *
     * M31: a=ax, b=a2, c=bx, d=b2, e=dx, f=d2
     *     = ax*(b2 - d2) - a2*(bx - dx) + (bx*d2 - b2*dx)
     *
     * M41: a=ax, b=a2, c=bx, d=b2, e=cx, f=c2
     *     = ax*(b2 - c2) - a2*(bx - cx) + (bx*c2 - b2*cx)
     */

    /* M11 */
    Lv00Interval m11_t1 = interval_mul(interval_point(by),
                                       interval_sub(c2, d2));
    Lv00Interval m11_t2 = interval_mul(b2,
                                       interval_sub(interval_point(cy), interval_point(dy)));
    Lv00Interval m11_t3 = interval_sub(
        interval_mul(interval_point(cy), d2),
        interval_mul(c2, interval_point(dy)));
    Lv00Interval m11 = interval_add(interval_sub(m11_t1, m11_t2), m11_t3);

    /* M21 */
    Lv00Interval m21_t1 = interval_mul(interval_point(bx),
                                       interval_sub(c2, d2));
    Lv00Interval m21_t2 = interval_mul(a2,
                                       interval_sub(interval_point(cx), interval_point(dx)));
    Lv00Interval m21_t3 = interval_sub(
        interval_mul(interval_point(cx), d2),
        interval_mul(c2, interval_point(dx)));
    Lv00Interval m21 = interval_add(interval_sub(m21_t1, m21_t2), m21_t3);

    /* M31 */
    Lv00Interval m31_t1 = interval_mul(interval_point(ax),
                                       interval_sub(b2, d2));
    Lv00Interval m31_t2 = interval_mul(a2,
                                       interval_sub(interval_point(bx), interval_point(dx)));
    Lv00Interval m31_t3 = interval_sub(
        interval_mul(interval_point(bx), d2),
        interval_mul(b2, interval_point(dx)));
    Lv00Interval m31 = interval_add(interval_sub(m31_t1, m31_t2), m31_t3);

    /* M41 */
    Lv00Interval m41_t1 = interval_mul(interval_point(ax),
                                       interval_sub(b2, c2));
    Lv00Interval m41_t2 = interval_mul(a2,
                                       interval_sub(interval_point(bx), interval_point(cx)));
    Lv00Interval m41_t3 = interval_sub(
        interval_mul(interval_point(bx), c2),
        interval_mul(b2, interval_point(cx)));
    Lv00Interval m41 = interval_add(interval_sub(m41_t1, m41_t2), m41_t3);

    /* det = M11 - M21 + M31 - M41 */
    Lv00Interval det = interval_sub(
        interval_add(interval_sub(m11, m21), m31),
        m41);

    return det;
}

/* ========================================================================
 * 第二部分：2D 精确谓词 API 实现
 * ======================================================================== */

/**
 * @brief 判定三点方向（2D orientation test）
 *
 * 计算有符号面积的两倍：(p2-p1) x (p3-p1)
 *   > 0 -> LEFT（逆时针）
 *   < 0 -> RIGHT（顺时针）
 *   = 0 -> COLLINEAR（共线）
 */
LV00_PUBLIC_API Lv00Orientation lv00_orientation_2d(
    double p1x, double p1y,
    double p2x, double p2y,
    double p3x, double p3y,
    Lv00PredicateMode mode)
{
    /*
     * 检查退化情况：如果任意两点重合，则无法判定方向。
     */
    double d12_sq = (p2x - p1x) * (p2x - p1x) + (p2y - p1y) * (p2y - p1y);
    double d13_sq = (p3x - p1x) * (p3x - p1x) + (p3y - p1y) * (p3y - p1y);
    double d23_sq = (p3x - p2x) * (p3x - p2x) + (p3y - p2y) * (p3y - p2y);

    const Lv00GeometryConfig *cfg = lv00_geometry_get_config();
    double eps = cfg ? cfg->collinear_epsilon : 1e-9;

    if (d12_sq < eps * eps && d13_sq < eps * eps) {
        return LV00_ORIENTATION_DEGENERATE;
    }

    if (mode == LV00_PREDICATE_SYMBOLIC) {
        /* 符号模式暂回退到精确模式 */
        mode = LV00_PREDICATE_EXACT;
    }

    if (mode == LV00_PREDICATE_APPROX) {
        /* 近似模式：直接浮点运算 */
        g_predicate_stats.approx_count++;

        double cross = (p2x - p1x) * (p3y - p1y) - (p3x - p1x) * (p2y - p1y);

        if (cross > eps) {
            return LV00_ORIENTATION_LEFT;
        } else if (cross < -eps) {
            return LV00_ORIENTATION_RIGHT;
        } else {
            return LV00_ORIENTATION_COLLINEAR;
        }
    }

    if (mode == LV00_PREDICATE_EXACT) {
        /* 精确模式：区间算术 */
        g_predicate_stats.exact_count++;

        Lv00Interval result = orientation_2d_exact_interval(
            p1x, p1y, p2x, p2y, p3x, p3y);

        if (result.lo > 0.0) {
            return LV00_ORIENTATION_LEFT;
        } else if (result.hi < 0.0) {
            return LV00_ORIENTATION_RIGHT;
        } else {
            return LV00_ORIENTATION_COLLINEAR;
        }
    }

    /* 自适应模式：先浮点，不确定时切换精确 */
    {
        double cross = (p2x - p1x) * (p3y - p1y) - (p3x - p1x) * (p2y - p1y);

        /*
         * 自适应阈值：当 |cross| 相对于输入坐标的量级足够大时，
         * 浮点结果可信。否则回退到区间算术。
         */
        double max_coord = fmax(fmax(fabs(p1x), fabs(p1y)),
                                fmax(fmax(fabs(p2x), fabs(p2y)),
                                     fmax(fabs(p3x), fabs(p3y))));
        double threshold = ADAPTIVE_THRESHOLD * max_coord * max_coord * max_coord;

        if (threshold == 0.0) {
            threshold = ADAPTIVE_THRESHOLD;
        }

        if (cross > threshold) {
            g_predicate_stats.approx_count++;
            return LV00_ORIENTATION_LEFT;
        } else if (cross < -threshold) {
            g_predicate_stats.approx_count++;
            return LV00_ORIENTATION_RIGHT;
        } else {
            /* 回退到精确模式 */
            g_predicate_stats.adaptive_fallback++;
            g_predicate_stats.exact_count++;

            Lv00Interval result = orientation_2d_exact_interval(
                p1x, p1y, p2x, p2y, p3x, p3y);

            if (result.lo > 0.0) {
                return LV00_ORIENTATION_LEFT;
            } else if (result.hi < 0.0) {
                return LV00_ORIENTATION_RIGHT;
            } else {
                return LV00_ORIENTATION_COLLINEAR;
            }
        }
    }
}

/**
 * @brief 判定四点方向（3D orientation test）
 *
 * 计算四面体有符号体积的六倍（3x3 行列式）。
 */
LV00_PUBLIC_API Lv00Orientation lv00_orientation_3d(
    double p1x, double p1y, double p1z,
    double p2x, double p2y, double p2z,
    double p3x, double p3y, double p3z,
    double p4x, double p4y, double p4z,
    Lv00PredicateMode mode)
{
    /* 检查退化情况 */
    double d12_sq = (p2x-p1x)*(p2x-p1x) + (p2y-p1y)*(p2y-p1y) + (p2z-p1z)*(p2z-p1z);
    double d13_sq = (p3x-p1x)*(p3x-p1x) + (p3y-p1y)*(p3y-p1y) + (p3z-p1z)*(p3z-p1z);
    double d14_sq = (p4x-p1x)*(p4x-p1x) + (p4y-p1y)*(p4y-p1y) + (p4z-p1z)*(p4z-p1z);

    const Lv00GeometryConfig *cfg = lv00_geometry_get_config();
    double eps = cfg ? cfg->collinear_epsilon : 1e-9;

    if (d12_sq < eps * eps && d13_sq < eps * eps && d14_sq < eps * eps) {
        return LV00_ORIENTATION_DEGENERATE;
    }

    if (mode == LV00_PREDICATE_SYMBOLIC) {
        mode = LV00_PREDICATE_EXACT;
    }

    if (mode == LV00_PREDICATE_APPROX) {
        g_predicate_stats.approx_count++;

        /* 3x3 行列式：det(a, b, c) = a . (b x c) */
        double ax = p2x - p1x, ay = p2y - p1y, az = p2z - p1z;
        double bx = p3x - p1x, by = p3y - p1y, bz = p3z - p1z;
        double cx = p4x - p1x, cy = p4y - p1y, cz = p4z - p1z;

        double det = ax * (by * cz - bz * cy)
                   - ay * (bx * cz - bz * cx)
                   + az * (bx * cy - by * cx);

        if (det > eps) {
            return LV00_ORIENTATION_LEFT;
        } else if (det < -eps) {
            return LV00_ORIENTATION_RIGHT;
        } else {
            return LV00_ORIENTATION_COPLANAR;
        }
    }

    if (mode == LV00_PREDICATE_EXACT) {
        g_predicate_stats.exact_count++;

        Lv00Interval result = orientation_3d_exact_interval(
            p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z);

        if (result.lo > 0.0) {
            return LV00_ORIENTATION_LEFT;
        } else if (result.hi < 0.0) {
            return LV00_ORIENTATION_RIGHT;
        } else {
            return LV00_ORIENTATION_COPLANAR;
        }
    }

    /* 自适应模式 */
    {
        double ax = p2x - p1x, ay = p2y - p1y, az = p2z - p1z;
        double bx = p3x - p1x, by = p3y - p1y, bz = p3z - p1z;
        double cx = p4x - p1x, cy = p4y - p1y, cz = p4z - p1z;

        double det = ax * (by * cz - bz * cy)
                   - ay * (bx * cz - bz * cx)
                   + az * (bx * cy - by * cx);

        double max_coord = fmax(fmax(fmax(fabs(p1x), fabs(p1y)), fabs(p1z)),
                                fmax(fmax(fmax(fabs(p2x), fabs(p2y)), fabs(p2z)),
                                     fmax(fmax(fmax(fabs(p3x), fabs(p3y)), fabs(p3z)),
                                          fmax(fmax(fabs(p4x), fabs(p4y)), fabs(p4z)))));
        double threshold = ADAPTIVE_THRESHOLD * max_coord * max_coord * max_coord;

        if (threshold == 0.0) {
            threshold = ADAPTIVE_THRESHOLD;
        }

        if (det > threshold) {
            g_predicate_stats.approx_count++;
            return LV00_ORIENTATION_LEFT;
        } else if (det < -threshold) {
            g_predicate_stats.approx_count++;
            return LV00_ORIENTATION_RIGHT;
        } else {
            g_predicate_stats.adaptive_fallback++;
            g_predicate_stats.exact_count++;

            Lv00Interval result = orientation_3d_exact_interval(
                p1x, p1y, p1z, p2x, p2y, p2z, p3x, p3y, p3z, p4x, p4y, p4z);

            if (result.lo > 0.0) {
                return LV00_ORIENTATION_LEFT;
            } else if (result.hi < 0.0) {
                return LV00_ORIENTATION_RIGHT;
            } else {
                return LV00_ORIENTATION_COPLANAR;
            }
        }
    }
}

/**
 * @brief 判定点相对于直线的位置
 *
 * 委托给 lv00_orientation_2d，将直线方向映射为 Lv00LineSide。
 */
LV00_PUBLIC_API Lv00LineSide lv00_line_side(
    double px, double py,
    double lx1, double ly1,
    double lx2, double ly2,
    Lv00PredicateMode mode)
{
    /* 检查退化情况：直线的两个定义点重合 */
    const Lv00GeometryConfig *cfg = lv00_geometry_get_config();
    double eps = cfg ? cfg->distance_epsilon : 1e-9;

    double d_sq = (lx2 - lx1) * (lx2 - lx1) + (ly2 - ly1) * (ly2 - ly1);
    if (d_sq < eps * eps) {
        return LV00_LINE_SIDE_DEGENERATE;
    }

    Lv00Orientation orient = lv00_orientation_2d(lx1, ly1, lx2, ly2, px, py, mode);

    switch (orient) {
        case LV00_ORIENTATION_LEFT:
            return LV00_LINE_SIDE_LEFT;
        case LV00_ORIENTATION_RIGHT:
            return LV00_LINE_SIDE_RIGHT;
        case LV00_ORIENTATION_COLLINEAR:
            return LV00_LINE_SIDE_ON;
        default:
            return LV00_LINE_SIDE_DEGENERATE;
    }
}

/**
 * @brief 判定点相对于有向线段的位置
 *
 * 与 lv00_line_side 相同的判定逻辑，语义上针对有向线段。
 */
LV00_PUBLIC_API Lv00LineSide lv00_segment_side(
    double px, double py,
    double sx1, double sy1,
    double sx2, double sy2,
    Lv00PredicateMode mode)
{
    /* 检查退化情况：线段长度为零 */
    const Lv00GeometryConfig *cfg = lv00_geometry_get_config();
    double eps = cfg ? cfg->distance_epsilon : 1e-9;

    double d_sq = (sx2 - sx1) * (sx2 - sx1) + (sy2 - sy1) * (sy2 - sy1);
    if (d_sq < eps * eps) {
        return LV00_LINE_SIDE_DEGENERATE;
    }

    Lv00Orientation orient = lv00_orientation_2d(sx1, sy1, sx2, sy2, px, py, mode);

    switch (orient) {
        case LV00_ORIENTATION_LEFT:
            return LV00_LINE_SIDE_LEFT;
        case LV00_ORIENTATION_RIGHT:
            return LV00_LINE_SIDE_RIGHT;
        case LV00_ORIENTATION_COLLINEAR:
            return LV00_LINE_SIDE_ON;
        default:
            return LV00_LINE_SIDE_DEGENERATE;
    }
}

/**
 * @brief 判定点相对于圆的位置
 *
 * 使用有符号距离的平方：|p - c|^2 - r^2
 *   < 0 -> INSIDE
 *   = 0 -> ON
 *   > 0 -> OUTSIDE
 */
LV00_PUBLIC_API Lv00SideOfCircle lv00_side_of_circle(
    double px, double py,
    double cx, double cy,
    double r,
    Lv00PredicateMode mode)
{
    /* 检查退化情况：半径为负或零 */
    if (r < 0.0) {
        return LV00_SIDE_DEGENERATE;
    }

    const Lv00GeometryConfig *cfg = lv00_geometry_get_config();
    double eps = cfg ? cfg->distance_epsilon : 1e-9;

    if (mode == LV00_PREDICATE_SYMBOLIC) {
        mode = LV00_PREDICATE_EXACT;
    }

    if (mode == LV00_PREDICATE_APPROX) {
        g_predicate_stats.approx_count++;

        double dx = px - cx;
        double dy = py - cy;
        double dist_sq = dx * dx + dy * dy;
        double r_sq = r * r;
        double diff = dist_sq - r_sq;

        if (diff < -eps) {
            return LV00_SIDE_INSIDE;
        } else if (diff > eps) {
            return LV00_SIDE_OUTSIDE;
        } else {
            return LV00_SIDE_ON;
        }
    }

    if (mode == LV00_PREDICATE_EXACT) {
        g_predicate_stats.exact_count++;

        Lv00Interval result = side_of_circle_exact_interval(px, py, cx, cy, r);

        if (result.hi < 0.0) {
            return LV00_SIDE_INSIDE;
        } else if (result.lo > 0.0) {
            return LV00_SIDE_OUTSIDE;
        } else {
            return LV00_SIDE_ON;
        }
    }

    /* 自适应模式 */
    {
        double dx = px - cx;
        double dy = py - cy;
        double dist_sq = dx * dx + dy * dy;
        double r_sq = r * r;
        double diff = dist_sq - r_sq;

        double max_coord = fmax(fmax(fabs(px), fabs(py)),
                                fmax(fmax(fabs(cx), fabs(cy)), fabs(r)));
        double threshold = ADAPTIVE_THRESHOLD * max_coord * max_coord * max_coord;

        if (threshold == 0.0) {
            threshold = ADAPTIVE_THRESHOLD;
        }

        if (diff < -threshold) {
            g_predicate_stats.approx_count++;
            return LV00_SIDE_INSIDE;
        } else if (diff > threshold) {
            g_predicate_stats.approx_count++;
            return LV00_SIDE_OUTSIDE;
        } else {
            g_predicate_stats.adaptive_fallback++;
            g_predicate_stats.exact_count++;

            Lv00Interval result = side_of_circle_exact_interval(px, py, cx, cy, r);

            if (result.hi < 0.0) {
                return LV00_SIDE_INSIDE;
            } else if (result.lo > 0.0) {
                return LV00_SIDE_OUTSIDE;
            } else {
                return LV00_SIDE_ON;
            }
        }
    }
}

/**
 * @brief 判定两点是否在直线同侧
 *
 * 使用两个 line_side 判定：如果两个点都在直线的同一侧（同为 LEFT 或同为 RIGHT），
 * 则返回 true。如果任一点在直线上，也视为同侧。
 */
LV00_PUBLIC_API bool lv00_same_side_of_line(
    double ax, double ay,
    double bx, double by,
    double lx1, double ly1,
    double lx2, double ly2,
    Lv00PredicateMode mode)
{
    Lv00LineSide side_a = lv00_line_side(ax, ay, lx1, ly1, lx2, ly2, mode);
    Lv00LineSide side_b = lv00_line_side(bx, by, lx1, ly1, lx2, ly2, mode);

    /* 退化情况 */
    if (side_a == LV00_LINE_SIDE_DEGENERATE || side_b == LV00_LINE_SIDE_DEGENERATE) {
        return false;
    }

    /* 如果两点都在直线上，视为同侧 */
    if (side_a == LV00_LINE_SIDE_ON && side_b == LV00_LINE_SIDE_ON) {
        return true;
    }

    /* 如果其中一个点在直线上，视为同侧 */
    if (side_a == LV00_LINE_SIDE_ON || side_b == LV00_LINE_SIDE_ON) {
        return true;
    }

    /* 两点在同一侧 */
    return (side_a == side_b);
}

/**
 * @brief 判定两点是否在圆同侧
 *
 * 使用两个 side_of_circle 判定。
 */
LV00_PUBLIC_API bool lv00_same_side_of_circle(
    double ax, double ay,
    double bx, double by,
    double cx, double cy,
    double r,
    Lv00PredicateMode mode)
{
    Lv00SideOfCircle side_a = lv00_side_of_circle(ax, ay, cx, cy, r, mode);
    Lv00SideOfCircle side_b = lv00_side_of_circle(bx, by, cx, cy, r, mode);

    /* 退化情况 */
    if (side_a == LV00_SIDE_DEGENERATE || side_b == LV00_SIDE_DEGENERATE) {
        return false;
    }

    /* 两点都在圆上，视为同侧 */
    if (side_a == LV00_SIDE_ON && side_b == LV00_SIDE_ON) {
        return true;
    }

    /* 其中一个在圆上，视为同侧 */
    if (side_a == LV00_SIDE_ON || side_b == LV00_SIDE_ON) {
        return true;
    }

    /* 两点在同一侧 */
    return (side_a == side_b);
}

/**
 * @brief 判定两条线段是否相交（精确判定）
 *
 * 使用方向谓词判定：
 *   线段 AB 和线段 CD 相交，当且仅当：
 *   1. C 和 D 在 AB 的两侧（或至少一个在 AB 上），且
 *   2. A 和 B 在 CD 的两侧（或至少一个在 CD 上）
 *
 * 特殊处理端点重合的情况。
 */
LV00_PUBLIC_API bool lv00_segments_intersect(
    double ax, double ay,
    double bx, double by,
    double cx, double cy,
    double dx, double dy,
    Lv00PredicateMode mode)
{
    /*
     * 方向谓词：
     *   d1 = orientation(A, B, C)  -- C 相对于 AB 的方向
     *   d2 = orientation(A, B, D)  -- D 相对于 AB 的方向
     *   d3 = orientation(C, D, A)  -- A 相对于 CD 的方向
     *   d4 = orientation(C, D, B)  -- B 相对于 CD 的方向
     *
     * 一般相交：d1 和 d2 异号，且 d3 和 d4 异号
     * 共线相交：d1==0 且 C 在 AB 上，或类似情况
     */
    Lv00Orientation d1 = lv00_orientation_2d(ax, ay, bx, by, cx, cy, mode);
    Lv00Orientation d2 = lv00_orientation_2d(ax, ay, bx, by, dx, dy, mode);
    Lv00Orientation d3 = lv00_orientation_2d(cx, cy, dx, dy, ax, ay, mode);
    Lv00Orientation d4 = lv00_orientation_2d(cx, cy, dx, dy, bx, by, mode);

    /* 一般相交情况：C 和 D 在 AB 两侧，A 和 B 在 CD 两侧 */
    if (((d1 == LV00_ORIENTATION_LEFT && d2 == LV00_ORIENTATION_RIGHT) ||
         (d1 == LV00_ORIENTATION_RIGHT && d2 == LV00_ORIENTATION_LEFT)) &&
        ((d3 == LV00_ORIENTATION_LEFT && d4 == LV00_ORIENTATION_RIGHT) ||
         (d3 == LV00_ORIENTATION_RIGHT && d4 == LV00_ORIENTATION_LEFT))) {
        return true;
    }

    /*
     * 共线情况：如果所有点共线，需要检查线段是否重叠。
     * 使用 bounding box 检查。
     */
    if (d1 == LV00_ORIENTATION_COLLINEAR && d2 == LV00_ORIENTATION_COLLINEAR &&
        d3 == LV00_ORIENTATION_COLLINEAR && d4 == LV00_ORIENTATION_COLLINEAR) {
        /*
         * 检查 C 或 D 是否在 AB 的 bounding box 内，
         * 或 A 或 B 是否在 CD 的 bounding box 内。
         */
        /* C 在 AB 上的 bounding box 检查 */
        int c_on_ab = (cx >= fmin(ax, bx) - 1e-9 && cx <= fmax(ax, bx) + 1e-9 &&
                       cy >= fmin(ay, by) - 1e-9 && cy <= fmax(ay, by) + 1e-9);
        /* D 在 AB 上的 bounding box 检查 */
        int d_on_ab = (dx >= fmin(ax, bx) - 1e-9 && dx <= fmax(ax, bx) + 1e-9 &&
                       dy >= fmin(ay, by) - 1e-9 && dy <= fmax(ay, by) + 1e-9);
        /* A 在 CD 上的 bounding box 检查 */
        int a_on_cd = (ax >= fmin(cx, dx) - 1e-9 && ax <= fmax(cx, dx) + 1e-9 &&
                       ay >= fmin(cy, dy) - 1e-9 && ay <= fmax(cy, dy) + 1e-9);
        /* B 在 CD 上的 bounding box 检查 */
        int b_on_cd = (bx >= fmin(cx, dx) - 1e-9 && bx <= fmax(cx, dx) + 1e-9 &&
                       by >= fmin(cy, dy) - 1e-9 && by <= fmax(cy, dy) + 1e-9);

        return (c_on_ab || d_on_ab || a_on_cd || b_on_cd);
    }

    /*
     * 端点在另一线段上的情况：
     * 如果 C 在 AB 上（d1 == COLLINEAR），检查 C 是否在 AB 的 bounding box 内
     */
    if (d1 == LV00_ORIENTATION_COLLINEAR) {
        int c_on_ab = (cx >= fmin(ax, bx) - 1e-9 && cx <= fmax(ax, bx) + 1e-9 &&
                       cy >= fmin(ay, by) - 1e-9 && cy <= fmax(ay, by) + 1e-9);
        if (c_on_ab && ((d3 == LV00_ORIENTATION_LEFT && d4 == LV00_ORIENTATION_RIGHT) ||
                        (d3 == LV00_ORIENTATION_RIGHT && d4 == LV00_ORIENTATION_LEFT))) {
            return true;
        }
    }

    if (d2 == LV00_ORIENTATION_COLLINEAR) {
        int d_on_ab = (dx >= fmin(ax, bx) - 1e-9 && dx <= fmax(ax, bx) + 1e-9 &&
                       dy >= fmin(ay, by) - 1e-9 && dy <= fmax(ay, by) + 1e-9);
        if (d_on_ab && ((d3 == LV00_ORIENTATION_LEFT && d4 == LV00_ORIENTATION_RIGHT) ||
                        (d3 == LV00_ORIENTATION_RIGHT && d4 == LV00_ORIENTATION_LEFT))) {
            return true;
        }
    }

    if (d3 == LV00_ORIENTATION_COLLINEAR) {
        int a_on_cd = (ax >= fmin(cx, dx) - 1e-9 && ax <= fmax(cx, dx) + 1e-9 &&
                       ay >= fmin(cy, dy) - 1e-9 && ay <= fmax(cy, dy) + 1e-9);
        if (a_on_cd && ((d1 == LV00_ORIENTATION_LEFT && d2 == LV00_ORIENTATION_RIGHT) ||
                        (d1 == LV00_ORIENTATION_RIGHT && d2 == LV00_ORIENTATION_LEFT))) {
            return true;
        }
    }

    if (d4 == LV00_ORIENTATION_COLLINEAR) {
        int b_on_cd = (bx >= fmin(cx, dx) - 1e-9 && bx <= fmax(cx, dx) + 1e-9 &&
                       by >= fmin(cy, dy) - 1e-9 && by <= fmax(cy, dy) + 1e-9);
        if (b_on_cd && ((d1 == LV00_ORIENTATION_LEFT && d2 == LV00_ORIENTATION_RIGHT) ||
                        (d1 == LV00_ORIENTATION_RIGHT && d2 == LV00_ORIENTATION_LEFT))) {
            return true;
        }
    }

    return false;
}

/**
 * @brief 判定点是否在三角形内部（含边界）
 *
 * 使用三个方向谓词判定：
 *   - orientation(A, B, P)
 *   - orientation(B, C, P)
 *   - orientation(C, A, P)
 *
 * 如果三个方向一致（全部同向或包含共线），则点在三角形内部（含边界）。
 */
LV00_PUBLIC_API bool lv00_point_in_triangle(
    double px, double py,
    double ax, double ay,
    double bx, double by,
    double cx, double cy,
    Lv00PredicateMode mode)
{
    Lv00Orientation o1 = lv00_orientation_2d(ax, ay, bx, by, px, py, mode);
    Lv00Orientation o2 = lv00_orientation_2d(bx, by, cx, cy, px, py, mode);
    Lv00Orientation o3 = lv00_orientation_2d(cx, cy, ax, ay, px, py, mode);

    /*
     * 点在三角形内部（含边界）的条件：
     * 三个方向谓词全部非负（逆时针三角形）或全部非正（顺时针三角形）。
     *
     * 即：不存在一个为 LEFT 而另一个为 RIGHT 的情况。
     */
    int has_left  = (o1 == LV00_ORIENTATION_LEFT)  || (o2 == LV00_ORIENTATION_LEFT)  || (o3 == LV00_ORIENTATION_LEFT);
    int has_right = (o1 == LV00_ORIENTATION_RIGHT) || (o2 == LV00_ORIENTATION_RIGHT) || (o3 == LV00_ORIENTATION_RIGHT);

    /* 如果同时存在 LEFT 和 RIGHT，则点在三角形外部 */
    if (has_left && has_right) {
        return false;
    }

    return true;
}

/**
 * @brief 判定四点是否共圆
 *
 * 使用 4x4 行列式判定：
 *   | ax  ay  ax^2+ay^2  1 |
 *   | bx  by  bx^2+by^2  1 | = 0
 *   | cx  cy  cx^2+cy^2  1 |
 *   | dx  dy  dx^2+dy^2  1 |
 */
LV00_PUBLIC_API bool lv00_four_points_concyclic(
    double ax, double ay,
    double bx, double by,
    double cx, double cy,
    double dx, double dy,
    Lv00PredicateMode mode)
{
    if (mode == LV00_PREDICATE_SYMBOLIC) {
        mode = LV00_PREDICATE_EXACT;
    }

    const Lv00GeometryConfig *cfg = lv00_geometry_get_config();
    double eps = cfg ? cfg->collinear_epsilon : 1e-9;

    if (mode == LV00_PREDICATE_APPROX) {
        g_predicate_stats.approx_count++;

        double a2 = ax * ax + ay * ay;
        double b2 = bx * bx + by * by;
        double c2 = cx * cx + cy * cy;
        double d2 = dx * dx + dy * dy;

        /*
         * Laplace 展开沿最后一列：
         * det = M11 - M21 + M31 - M41
         *
         * M11 = by*(c2-d2) - b2*(cy-dy) + (cy*d2 - c2*dy)
         * M21 = bx*(c2-d2) - a2*(cx-dx) + (cx*d2 - c2*dx)
         * M31 = ax*(b2-d2) - a2*(bx-dx) + (bx*d2 - b2*dx)
         * M41 = ax*(b2-c2) - a2*(bx-cx) + (bx*c2 - b2*cx)
         */
        double m11 = by * (c2 - d2) - b2 * (cy - dy) + (cy * d2 - c2 * dy);
        double m21 = bx * (c2 - d2) - a2 * (cx - dx) + (cx * d2 - c2 * dx);
        double m31 = ax * (b2 - d2) - a2 * (bx - dx) + (bx * d2 - b2 * dx);
        double m41 = ax * (b2 - c2) - a2 * (bx - cx) + (bx * c2 - b2 * cx);

        double det = m11 - m21 + m31 - m41;

        /* 归一化：除以坐标量级 */
        double max_coord = fmax(fmax(fmax(fabs(ax), fabs(ay)),
                                     fmax(fabs(bx), fabs(by))),
                                fmax(fmax(fabs(cx), fabs(cy)),
                                     fmax(fabs(dx), fabs(dy))));
        if (max_coord > 1.0) {
            det /= (max_coord * max_coord * max_coord);
        }

        return fabs(det) < eps;
    }

    if (mode == LV00_PREDICATE_EXACT) {
        g_predicate_stats.exact_count++;

        Lv00Interval result = four_points_concyclic_exact_interval(
            ax, ay, bx, by, cx, cy, dx, dy);

        /* 如果区间包含零，则可能共圆 */
        return (result.lo <= 0.0 && result.hi >= 0.0);
    }

    /* 自适应模式 */
    {
        double a2 = ax * ax + ay * ay;
        double b2 = bx * bx + by * by;
        double c2 = cx * cx + cy * cy;
        double d2 = dx * dx + dy * dy;

        double m11 = by * (c2 - d2) - b2 * (cy - dy) + (cy * d2 - c2 * dy);
        double m21 = bx * (c2 - d2) - a2 * (cx - dx) + (cx * d2 - c2 * dx);
        double m31 = ax * (b2 - d2) - a2 * (bx - dx) + (bx * d2 - b2 * dx);
        double m41 = ax * (b2 - c2) - a2 * (bx - cx) + (bx * c2 - b2 * cx);

        double det = m11 - m21 + m31 - m41;

        double max_coord = fmax(fmax(fmax(fabs(ax), fabs(ay)),
                                     fmax(fabs(bx), fabs(by))),
                                fmax(fmax(fabs(cx), fabs(cy)),
                                     fmax(fabs(dx), fabs(dy))));
        double threshold = ADAPTIVE_THRESHOLD * max_coord * max_coord * max_coord;

        if (threshold == 0.0) {
            threshold = ADAPTIVE_THRESHOLD;
        }

        if (fabs(det) > threshold) {
            g_predicate_stats.approx_count++;
            return false; /* 明确不为零，不共圆 */
        } else {
            g_predicate_stats.adaptive_fallback++;
            g_predicate_stats.exact_count++;

            Lv00Interval result = four_points_concyclic_exact_interval(
                ax, ay, bx, by, cx, cy, dx, dy);

            return (result.lo <= 0.0 && result.hi >= 0.0);
        }
    }
}

/**
 * @brief 判定多边形是否为凸多边形
 *
 * 检查所有连续三顶点的方向是否一致（全部同向或全部反向）。
 * 允许共线顶点（退化边）。
 *
 * @param xs, ys 顶点坐标数组
 * @param n       顶点数量（必须 >= 3）
 * @param mode    精度模式
 * @return true 多边形为凸
 */
LV00_PUBLIC_API bool lv00_polygon_is_convex(
    const double *xs, const double *ys, int n,
    Lv00PredicateMode mode)
{
    if (n < 3 || xs == NULL || ys == NULL) {
        return false;
    }

    /*
     * 检查所有连续三顶点的方向。
     * first_non_collinear 记录第一个非共线方向，后续所有方向必须与之一致。
     */
    int first_sign = 0; /* 0 = 未确定, 1 = LEFT, -1 = RIGHT */

    for (int i = 0; i < n; i++) {
        int i1 = (i + 1) % n;
        int i2 = (i + 2) % n;

        Lv00Orientation orient = lv00_orientation_2d(
            xs[i], ys[i], xs[i1], ys[i1], xs[i2], ys[i2], mode);

        if (orient == LV00_ORIENTATION_LEFT) {
            if (first_sign == 0) {
                first_sign = 1;
            } else if (first_sign == -1) {
                return false; /* 方向不一致 */
            }
        } else if (orient == LV00_ORIENTATION_RIGHT) {
            if (first_sign == 0) {
                first_sign = -1;
            } else if (first_sign == 1) {
                return false; /* 方向不一致 */
            }
        }
        /* COLLINEAR 和 DEGENERATE 不影响判定 */
    }

    return true;
}

/**
 * @brief 判定点是否在凸多边形内部
 *
 * 使用二分法判定，O(log n) 复杂度。
 * 要求多边形顶点按逆时针或顺时针顺序排列。
 *
 * 算法：
 *   1. 确定多边形的绕行方向（通过第一个非共线三顶点）
 *   2. 使用二分法找到点所在的扇形区域
 *   3. 检查点是否在该扇形区域的三角形内
 */
LV00_PUBLIC_API bool lv00_point_in_convex_polygon(
    double px, double py,
    const double *xs, const double *ys, int n,
    Lv00PredicateMode mode)
{
    if (n < 3 || xs == NULL || ys == NULL) {
        return false;
    }

    /*
     * 确定多边形绕行方向。
     * 找到第一个非共线的连续三顶点。
     */
    int ccw = 0; /* 0 = 未知, 1 = 逆时针, -1 = 顺时针 */
    for (int i = 0; i < n; i++) {
        int i1 = (i + 1) % n;
        int i2 = (i + 2) % n;
        Lv00Orientation orient = lv00_orientation_2d(
            xs[i], ys[i], xs[i1], ys[i1], xs[i2], ys[i2], mode);
        if (orient == LV00_ORIENTATION_LEFT) {
            ccw = 1;
            break;
        } else if (orient == LV00_ORIENTATION_RIGHT) {
            ccw = -1;
            break;
        }
    }

    if (ccw == 0) {
        /* 所有三顶点共线，退化多边形 */
        return false;
    }

    /*
     * 二分法搜索。
     * 在 [1, n-1] 范围内搜索，使得点在三角形 (v0, v[lo], v[hi]) 内。
     */
    int lo = 1;
    int hi = n - 1;

    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;

        /*
         * 检查点相对于边 (v0, v[mid]) 的方向。
         * 逆时针多边形：点应在左侧
         * 顺时针多边形：点应在右侧
         */
        Lv00Orientation orient = lv00_orientation_2d(
            xs[0], ys[0], xs[mid], ys[mid], px, py, mode);

        if (ccw == 1) {
            /* 逆时针：点在左侧则缩小上界 */
            if (orient == LV00_ORIENTATION_LEFT) {
                hi = mid;
            } else {
                lo = mid;
            }
        } else {
            /* 顺时针：点在右侧则缩小上界 */
            if (orient == LV00_ORIENTATION_RIGHT) {
                hi = mid;
            } else {
                lo = mid;
            }
        }
    }

    /* 检查点是否在三角形 (v0, v[lo], v[hi]) 内 */
    return lv00_point_in_triangle(
        px, py, xs[0], ys[0], xs[lo], ys[lo], xs[hi], ys[hi], mode);
}

/**
 * @brief 判定点是否在任意多边形内部（射线法）
 *
 * 从点发出水平向右的射线，统计与多边形边的交点数。
 * 奇数交点 -> 内部，偶数交点 -> 外部。
 *
 * 注意处理射线经过顶点的退化情况。
 */
LV00_PUBLIC_API bool lv00_point_in_polygon(
    double px, double py,
    const double *xs, const double *ys, int n,
    Lv00PredicateMode mode)
{
    if (n < 3 || xs == NULL || ys == NULL) {
        return false;
    }

    const Lv00GeometryConfig *cfg = lv00_geometry_get_config();
    double eps = cfg ? cfg->collinear_epsilon : 1e-9;

    int crossings = 0;

    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;

        double xi = xs[i], yi = ys[i];
        double xj = xs[j], yj = ys[j];

        /*
         * 射线法标准实现：
         * 只考虑边跨越射线所在的水平线（y = py）的情况。
         *
         * 策略：当边的两个端点一个在射线上方、一个在射线下方（或恰好在射线上）时，
         * 计算交点的 x 坐标，如果交点在点的右侧则计数。
         *
         * 使用 "half-open" 区间避免顶点被重复计数：
         *   - 上端点包含（yi >= py），下端点不包含（yj < py）
         *   - 这样每个顶点恰好被一条边"拥有"
         */
        if (((yi > py) != (yj > py))) {
            /*
             * 计算射线与边的交点的 x 坐标。
             * 使用线性插值：x_intersect = xi + (py - yi) * (xj - xi) / (yj - yi)
             */
            double x_intersect = xi + (py - yi) * (xj - xi) / (yj - yi);

            if (x_intersect > px - eps) {
                crossings++;
            }
        } else if (fabs(yi - py) < eps && fabs(yj - py) < eps) {
            /*
             * 边与射线共线（水平边）。
             * 如果点在线段的 bounding box 内，则点在多边形边界上。
             * 直接返回 true（含边界）。
             */
            double x_min = fmin(xi, xj);
            double x_max = fmax(xi, xj);
            if (px >= x_min - eps && px <= x_max + eps) {
                return true;
            }
        }
    }

    /* 奇数交点 -> 内部 */
    return (crossings % 2) != 0;
}

/* ========================================================================
 * 第三部分：谓词统计与配置
 * ======================================================================== */

/**
 * @brief 获取谓词统计信息
 */
LV00_PUBLIC_API void lv00_predicate_get_stats(Lv00PredicateStats *stats)
{
    if (stats != NULL) {
        memcpy(stats, &g_predicate_stats, sizeof(Lv00PredicateStats));
    }
}

/**
 * @brief 重置谓词统计信息
 */
LV00_PUBLIC_API void lv00_predicate_reset_stats(void)
{
    memset(&g_predicate_stats, 0, sizeof(Lv00PredicateStats));
}

/**
 * @brief 设置全局谓词精度模式
 */
LV00_PUBLIC_API void lv00_predicate_set_mode(Lv00PredicateMode mode)
{
    g_predicate_mode = mode;
}

/**
 * @brief 获取全局谓词精度模式
 */
LV00_PUBLIC_API Lv00PredicateMode lv00_predicate_get_mode(void)
{
    return g_predicate_mode;
}
