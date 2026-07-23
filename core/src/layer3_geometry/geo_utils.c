/**
 * @file geo_utils.c
 * @brief 几何工具函数实现
 *
 * 聚合符号坐标操作与约束图查询的工具函数。
 * 提供坐标比较、距离计算、几何谓词等便捷操作。
 *
 * @version 3.5.0
 */

#include "lv/geo_utils.h"

#include <float.h>
#include <math.h>
#include <string.h>

/* ========================================================================
 * 数值容差常量
 * ======================================================================== */
#define GEO_EPSILON 1e-12      /**< 浮点比较容差 */
#define GEO_ANGLE_EPSILON 1e-9 /**< 角度比较容差 */

/* ========================================================================
 * 坐标转换工具
 * ======================================================================== */

/**
 * @brief 将符号坐标转换为双精度浮点值
 * @param coord 符号坐标指针
 * @return 浮点数值，coord 为 NULL 时返回 0.0
 */
double geo_coord_to_double(const SymbolicCoord *coord) {
    if (coord == NULL)
        return 0.0;

    /* 简化实现：依赖 symbolic_coord_to_double */
    extern double symbolic_coord_to_double(const SymbolicCoord *c);
    return symbolic_coord_to_double(coord);
}

/**
 * @brief 计算两个双精度点之间的欧几里得距离
 * @param x1, y1  第一个点坐标
 * @param x2, y2  第二个点坐标
 * @return 距离值
 */
double geo_distance_2d(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

/**
 * @brief 判断两个双精度值是否近似相等
 * @param a, b  待比较的值
 * @param eps   容差
 * @return 近似相等返回 1，否则返回 0
 */
int geo_approx_equal(double a, double b, double eps) {
    if (eps < GEO_EPSILON) {
        eps = GEO_EPSILON;
    }
    return fabs(a - b) < eps ? 1 : 0;
}

/* ========================================================================
 * 几何谓词
 * ======================================================================== */

/**
 * @brief 判断点 (px, py) 是否在线段 (x1,y1)-(x2,y2) 上
 *
 * 使用叉积判断共线性，并检查点是否在线段范围之内。
 *
 * @return 在线段上返回 1，否则返回 0
 */
int geo_point_on_segment(double px, double py, double x1, double y1, double x2, double y2) {
    /* 叉积判断共线 */
    double cross = (px - x1) * (y2 - y1) - (py - y1) * (x2 - x1);
    if (fabs(cross) > GEO_EPSILON)
        return 0;

    /* 范围检查 */
    double dot = (px - x1) * (px - x2) + (py - y1) * (py - y2);
    return dot <= GEO_EPSILON ? 1 : 0;
}

/**
 * @brief 计算三角形面积（带符号）
 *
 * 使用叉积公式，正面积表示逆时针方向，负面积表示顺时针方向。
 *
 * @return 有符号面积的两倍
 */
double geo_signed_area_2x(double x1, double y1, double x2, double y2, double x3, double y3) {
    return (x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1);
}

/**
 * @brief 判断点是否在三角形内部（重心坐标法）
 * @return 在内部返回 1，在边界返回 0，在外部返回 -1
 */
int geo_point_in_triangle(double px, double py, double x1, double y1, double x2, double y2, double x3, double y3) {
    double d1 = geo_signed_area_2x(x1, y1, x2, y2, px, py);
    double d2 = geo_signed_area_2x(x2, y2, x3, y3, px, py);
    double d3 = geo_signed_area_2x(x3, y3, x1, y1, px, py);

    int has_neg = (d1 < -GEO_EPSILON) || (d2 < -GEO_EPSILON) || (d3 < -GEO_EPSILON);
    int has_pos = (d1 > GEO_EPSILON) || (d2 > GEO_EPSILON) || (d3 > GEO_EPSILON);

    if (has_neg && has_pos)
        return -1; /* 外部 */

    /* 检查是否在边界上 */
    if (fabs(d1) < GEO_EPSILON || fabs(d2) < GEO_EPSILON || fabs(d3) < GEO_EPSILON) {
        return 0; /* 边界 */
    }
    return 1; /* 内部 */
}

/**
 * @brief 计算两点连线的角度（弧度）
 * @return 角度值 [-PI, PI]
 */
double geo_angle(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    if (dx == 0.0 && dy == 0.0)
        return 0.0; /* 两点重合时返回 0 而非 NaN */
    return atan2(dy, dx);
}

/**
 * @brief 判断两条线段是否相交
 * @return 相交返回 1，不相交返回 0
 */
int geo_segments_intersect(double ax1, double ay1, double ax2, double ay2, double bx1, double by1, double bx2,
                           double by2) {
    double d1 = geo_signed_area_2x(bx1, by1, bx2, by2, ax1, ay1);
    double d2 = geo_signed_area_2x(bx1, by1, bx2, by2, ax2, ay2);
    double d3 = geo_signed_area_2x(ax1, ay1, ax2, ay2, bx1, by1);
    double d4 = geo_signed_area_2x(ax1, ay1, ax2, ay2, bx2, by2);

    /* 一般相交 */
    if (((d1 > GEO_EPSILON && d2 < -GEO_EPSILON) || (d1 < -GEO_EPSILON && d2 > GEO_EPSILON)) &&
        ((d3 > GEO_EPSILON && d4 < -GEO_EPSILON) || (d3 < -GEO_EPSILON && d4 > GEO_EPSILON))) {
        return 1;
    }

    /* 退化情况：检查端点是否在另一条线段上 */
    if (fabs(d1) < GEO_EPSILON && geo_point_on_segment(ax1, ay1, bx1, by1, bx2, by2))
        return 1;
    if (fabs(d2) < GEO_EPSILON && geo_point_on_segment(ax2, ay2, bx1, by1, bx2, by2))
        return 1;
    if (fabs(d3) < GEO_EPSILON && geo_point_on_segment(bx1, by1, ax1, ay1, ax2, ay2))
        return 1;
    if (fabs(d4) < GEO_EPSILON && geo_point_on_segment(bx2, by2, ax1, ay1, ax2, ay2))
        return 1;

    return 0;
}
