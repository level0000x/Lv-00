/**
 * @file geo_utils.c
 * @brief 通用几何计算工具函数实现
 *
 * 实现距离、斜率、角度、共线、平行、垂直等基础几何计算。
 * 容差参数从 geometry_config.h 的运行时配置中获取。
 *
 * @version 1.0.0
 */

#include "geo_utils.h"
#include "geometry_config.h"

#include <math.h>

/* ============================================================
 * 距离计算
 * ============================================================ */

double lv00_geo_distance_2d(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

double lv00_geo_distance_3d(double x1, double y1, double z1,
                            double x2, double y2, double z2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    double dz = z2 - z1;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

/* ============================================================
 * 斜率计算
 * ============================================================ */

double lv00_geo_slope(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;

    /* 垂直线：返回 NaN */
    if (fabs(dx) < 1e-15) {
        return NAN;
    }

    return (y2 - y1) / dx;
}

/* ============================================================
 * 角度计算
 * ============================================================ */

double lv00_geo_angle(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return atan2(dy, dx);
}

double lv00_geo_angle_3pt(double ax, double ay, double bx, double by,
                          double cx, double cy) {
    /* 向量 ba 和 bc */
    double bax = ax - bx;
    double bay = ay - by;
    double bcx = cx - bx;
    double bcy = cy - by;

    /* 点积和叉积 */
    double dot = bax * bcx + bay * bcy;
    double cross = bax * bcy - bay * bcx;

    /* atan2 给出 [-pi, pi]，取绝对值得到 [0, pi] */
    return fabs(atan2(cross, dot));
}

/* ============================================================
 * 面积计算
 * ============================================================ */

double lv00_geo_signed_area(double ax, double ay, double bx, double by,
                            double cx, double cy) {
    /*
     * 带符号面积（三角形面积的两倍）：
     *   S = (bx - ax)(cy - ay) - (cx - ax)(by - ay)
     * 正值 = 逆时针（CCW），负值 = 顺时针（CW），零 = 共线
     */
    return (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
}

/* ============================================================
 * 几何关系判定
 * ============================================================ */

bool lv00_geo_collinear(double ax, double ay, double bx, double by,
                        double cx, double cy, double epsilon) {
    double area = lv00_geo_signed_area(ax, ay, bx, by, cx, cy);
    return fabs(area) < epsilon;
}

bool lv00_geo_parallel(double x1, double y1, double x2, double y2,
                       double x3, double y3, double x4, double y4,
                       double epsilon) {
    /*
     * 两条线段的方向向量分别为 (dx1, dy1) 和 (dx2, dy2)。
     * 平行条件：叉积 |dx1*dy2 - dy1*dx2| < epsilon
     */
    double dx1 = x2 - x1;
    double dy1 = y2 - y1;
    double dx2 = x4 - x3;
    double dy2 = y4 - y3;

    double cross = dx1 * dy2 - dy1 * dx2;
    return fabs(cross) < epsilon;
}

bool lv00_geo_perpendicular(double x1, double y1, double x2, double y2,
                            double x3, double y3, double x4, double y4,
                            double epsilon) {
    /*
     * 两条线段的方向向量分别为 (dx1, dy1) 和 (dx2, dy2)。
     * 垂直条件：点积 |dx1*dx2 + dy1*dy2| < epsilon
     */
    double dx1 = x2 - x1;
    double dy1 = y2 - y1;
    double dx2 = x4 - x3;
    double dy2 = y4 - y3;

    double dot = dx1 * dx2 + dy1 * dy2;
    return fabs(dot) < epsilon;
}
