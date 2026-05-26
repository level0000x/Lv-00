/**
 * @file geo_utils.h
 * @brief 通用几何计算工具函数库
 *
 * 提供几何计算中常用的基础函数，消除各模块间的代码重复。
 * 所有容差参数均从 geometry_config.h 的 Lv00GeometryConfig 获取，
 * 确保全局一致性。
 *
 * @version 1.0.0
 */

#ifndef LV00_GEO_UTILS_H
#define LV00_GEO_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 计算两个 2D 点之间的欧几里得距离
 *
 * @param x1 第一个点的 X 坐标
 * @param y1 第一个点的 Y 坐标
 * @param x2 第二个点的 X 坐标
 * @param y2 第二个点的 Y 坐标
 * @return 两点之间的距离
 */
double lv00_geo_distance_2d(double x1, double y1, double x2, double y2);

/**
 * @brief 计算两个 3D 点之间的欧几里得距离
 *
 * @param x1 第一个点的 X 坐标
 * @param y1 第一个点的 Y 坐标
 * @param z1 第一个点的 Z 坐标
 * @param x2 第二个点的 X 坐标
 * @param y2 第二个点的 Y 坐标
 * @param z2 第二个点的 Z 坐标
 * @return 两点之间的距离
 */
double lv00_geo_distance_3d(double x1, double y1, double z1,
                            double x2, double y2, double z2);

/**
 * @brief 计算两点之间的斜率
 *
 * @param x1 第一个点的 X 坐标
 * @param y1 第一个点的 Y 坐标
 * @param x2 第二个点的 X 坐标
 * @param y2 第二个点的 Y 坐标
 * @return 斜率值；垂直线返回 NaN
 */
double lv00_geo_slope(double x1, double y1, double x2, double y2);

/**
 * @brief 计算从 (x1,y1) 到 (x2,y2) 的方向角（弧度）
 *
 * 角度范围 [0, 2*pi)，从正 X 轴逆时针测量。
 *
 * @param x1 起点坐标 X
 * @param y1 起点坐标 Y
 * @param x2 终点坐标 X
 * @param y2 终点坐标 Y
 * @return 方向角（弧度）
 */
double lv00_geo_angle(double x1, double y1, double x2, double y2);

/**
 * @brief 计算三点构成的角度（顶点在 b 处），单位弧度
 *
 * @param ax 顶点 a 的 X 坐标
 * @param ay 顶点 a 的 Y 坐标
 * @param bx 顶点 b（角度顶点）的 X 坐标
 * @param by 顶点 b（角度顶点）的 Y 坐标
 * @param cx 顶点 c 的 X 坐标
 * @param cy 顶点 c 的 Y 坐标
 * @return 角度值（弧度），范围 [0, pi]
 */
double lv00_geo_angle_3pt(double ax, double ay, double bx, double by,
                          double cx, double cy);

/**
 * @brief 计算三角形的带符号面积
 *
 * 正值表示逆时针排列（CCW），负值表示顺时针排列（CW），零表示共线。
 *
 * @param ax 点 a 的 X 坐标
 * @param ay 点 a 的 Y 坐标
 * @param bx 点 b 的 X 坐标
 * @param by 点 b 的 Y 坐标
 * @param cx 点 c 的 X 坐标
 * @param cy 点 c 的 Y 坐标
 * @return 带符号面积（三角形面积的两倍）
 */
double lv00_geo_signed_area(double ax, double ay, double bx, double by,
                            double cx, double cy);

/**
 * @brief 判断三个点是否共线
 *
 * 使用带符号面积与 epsilon 容差进行比较。
 *
 * @param ax 点 a 的 X 坐标
 * @param ay 点 a 的 Y 坐标
 * @param bx 点 b 的 X 坐标
 * @param by 点 b 的 Y 坐标
 * @param cx 点 c 的 X 坐标
 * @param cy 点 c 的 Y 坐标
 * @param epsilon 容差阈值
 * @return true 共线，false 不共线
 */
bool lv00_geo_collinear(double ax, double ay, double bx, double by,
                        double cx, double cy, double epsilon);

/**
 * @brief 判断两条线段是否平行
 *
 * 线段 1: (x1,y1) -> (x2,y2)，线段 2: (x3,y3) -> (x4,y4)
 * 通过叉积的绝对值与 epsilon 比较来判断。
 *
 * @param x1 线段1起点 X
 * @param y1 线段1起点 Y
 * @param x2 线段1终点 X
 * @param y2 线段1终点 Y
 * @param x3 线段2起点 X
 * @param y3 线段2起点 Y
 * @param x4 线段2终点 X
 * @param y4 线段2终点 Y
 * @param epsilon 容差阈值
 * @return true 平行，false 不平行
 */
bool lv00_geo_parallel(double x1, double y1, double x2, double y2,
                       double x3, double y3, double x4, double y4,
                       double epsilon);

/**
 * @brief 判断两条线段是否垂直
 *
 * 线段 1: (x1,y1) -> (x2,y2)，线段 2: (x3,y3) -> (x4,y4)
 * 通过点积的绝对值与 epsilon 比较来判断。
 *
 * @param x1 线段1起点 X
 * @param y1 线段1起点 Y
 * @param x2 线段1终点 X
 * @param y2 线段1终点 Y
 * @param x3 线段2起点 X
 * @param y3 线段2起点 Y
 * @param x4 线段2终点 X
 * @param y4 线段2终点 Y
 * @param epsilon 容差阈值
 * @return true 垂直，false 不垂直
 */
bool lv00_geo_perpendicular(double x1, double y1, double x2, double y2,
                            double x3, double y3, double x4, double y4,
                            double epsilon);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEO_UTILS_H */
