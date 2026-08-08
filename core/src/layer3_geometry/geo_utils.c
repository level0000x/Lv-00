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

#include "lv_internal.h"
#include "lv/geo_predicate.h"

/* ========================================================================
 * 数值容差常量
 * ========================================================================
 * GEO_EPSILON / GEO_ANGLE_EPSILON 已收敛到 geo_utils.h 的分级常量表
 * （分别引用 lv_EPSILON_DOUBLE / lv_GEO_ANGLE_EPSILON），此处不再本地定义。
 * 注意：GEO_ANGLE_EPSILON 原先地值 1e-9 与 config.h 的 lv_GEO_ANGLE_EPSILON
 * （1e-10）不一致，已统一为引用 config 权威值（该宏无使用点，行为零影响）。 */

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
 * @brief 计算两个三维点之间的欧几里得距离
 * @param x1, y1, z1  第一个点坐标
 * @param x2, y2, z2  第二个点坐标
 * @return 距离值
 */
double geo_distance_3d(double x1, double y1, double z1, double x2, double y2, double z2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    double dz = z2 - z1;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

/**
 * @brief 计算二维向量 (dx, dy) 的模长
 * @param dx, dy  向量分量
 * @return 向量模长
 */
double geo_norm_2d(double dx, double dy) {
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
 * 收敛实现：共线性判定委托事实来源 lv_segment_side（geo_predicate.c，
 * APPROX 浮点近似模式）；线段范围检查（含端点）由本地点积保留，因为
 * 谓词层 lv_segment_side 仅判定点相对于无限直线的位置，无线段范围语义。
 *
 * 行为差异说明（迁移后以谓词层为准）：
 *   - 共线阈值由本地 GEO_EPSILON（1e-12）变为谓词层 distance_epsilon
 *     （默认 1e-9，见 geometry_config.c），判定更宽松；
 *   - 退化线段（两端点重合）：旧实现按"点等于端点"判定，谓词层返回
 *     DEGENERATE（不在线上）。调用方 graph_node_alloc.c 已先行排除退化，
 *     实际路径不受影响。
 *
 * @return 在线段上返回 1，否则返回 0
 */
int geo_point_on_segment(double px, double py, double x1, double y1, double x2, double y2) {
    if (lv_segment_side(px, py, x1, y1, x2, y2, lv_PREDICATE_APPROX) != lv_LINE_SIDE_ON)
        return 0;

    /* 范围检查：点是否落在线段两端点之间的范围内（含端点） */
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
 * @brief 判断点是否在三角形内部（含边界）
 *
 * 收敛实现：委托事实来源 lv_point_in_triangle（geo_predicate.c，
 * APPROX 浮点近似模式，三次 lv_orientation_2d 同号判定含边界）。
 *
 * 返回语义映射：内部/边界 → 1，外部 → -1。
 *
 * 边界语义差异（迁移后以谓词层为准）：旧实现用 3 个独立 GEO_EPSILON
 * 阈值严格区分边界并返回 0；谓词层"同号即内部"将边界与内部统一返回
 * 真（true）。本函数全项目无调用方、无头文件声明，无旧行为兼容负担。
 *
 * @return 在内部或边界返回 1，在外部返回 -1
 */
int geo_point_in_triangle(double px, double py, double x1, double y1, double x2, double y2, double x3, double y3) {
    return lv_point_in_triangle(px, py, x1, y1, x2, y2, x3, y3, lv_PREDICATE_APPROX) ? 1 : -1;
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
 *
 * 收敛实现：委托给事实来源 lv_segments_intersect（geo_predicate.c，
 * APPROX 浮点近似模式，含端点接触语义，与本地旧实现一致）。
 */
int geo_segments_intersect(double ax1, double ay1, double ax2, double ay2, double bx1, double by1, double bx2,
                           double by2) {
    return lv_segments_intersect(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2, lv_PREDICATE_APPROX) ? 1 : 0;
}
