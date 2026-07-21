/**
 * @file preset_measurements.c
 * @brief 几何测量模块预设函数库 - 实现
 *
 * 实现几何测量模块的所有预设函数库。
 * 涵盖距离、角度、面积、长度等多种测量计算。
 *
 * @module Measurements
 * @category PRESET_CATEGORY_MEASUREMENT
 */

#include "preset_measurements.h"
#include "preset_blocks.h"
#include "lv00_internal.h"

#include <string.h>

/* ==================== 预设函数注册表 ==================== */

/** 测量学模块预设函数块总数 */

/* ==================== 模块注册实现 ==================== */

bool preset_measurements_register(void)
{
    int success_count = 0;

    /* -------------------- 距离测量 -------------------- */

    /* 欧几里得距离 */
    if (preset_blocks_register_by_category(
            "distance_euclidean",
            "计算两点间的欧几里得距离",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* 欧几里得距离平方 */
    if (preset_blocks_register_by_category(
            "distance_squared",
            "计算两点距离的平方（避免开根号）",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* 曼哈顿距离 */
    if (preset_blocks_register_by_category(
            "distance_manhattan",
            "计算两点间的曼哈顿距离（L1范数）",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* 切比雪夫距离 */
    if (preset_blocks_register_by_category(
            "distance_chebyshev",
            "计算两点间的切比雪夫距离（L∞范数）",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* 点到直线距离 */
    if (preset_blocks_register_by_category(
            "distance_point_to_line",
            "计算点到直线的最短距离",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* 点到线段距离 */
    if (preset_blocks_register_by_category(
            "distance_point_to_segment",
            "计算点到线段的最短距离",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* -------------------- 角度测量 -------------------- */

    /* 三点角度 */
    if (preset_blocks_register_by_category(
            "angle_three_points",
            "计算三点形成的角度",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* 两直线夹角 */
    if (preset_blocks_register_by_category(
            "angle_two_lines",
            "计算两条直线的夹角",
            PRESET_EXT_MEASUREMENT,
            4, 1)) {
        success_count++;
    }

    /* 有向角 */
    if (preset_blocks_register_by_category(
            "directed_angle",
            "计算有向角（带符号）",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* -------------------- 面积测量 -------------------- */

    /* 三角形面积（鞋带公式） */
    if (preset_blocks_register_by_category(
            "triangle_area",
            "使用鞋带公式计算三角形面积",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* 海伦公式 */
    if (preset_blocks_register_by_category(
            "triangle_area_heron",
            "使用海伦公式计算三角形面积",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* 圆面积 */
    if (preset_blocks_register_by_category(
            "circle_area",
            "计算圆的面积",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* 扇形面积 */
    if (preset_blocks_register_by_category(
            "sector_area",
            "计算扇形的面积",
            PRESET_EXT_MEASUREMENT,
            3, 1)) {
        success_count++;
    }

    /* -------------------- 长度计算 -------------------- */

    /* 线段长度 */
    if (preset_blocks_register_by_category(
            "segment_length",
            "计算线段长度",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* 圆周长 */
    if (preset_blocks_register_by_category(
            "circle_circumference",
            "计算圆的周长",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* -------------------- 向量计算 -------------------- */

    /* 向量模长 */
    if (preset_blocks_register_by_category(
            "vector_magnitude",
            "计算向量的模长",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* 向量点积 */
    if (preset_blocks_register_by_category(
            "vector_dot_product",
            "计算两个向量的点积",
            PRESET_EXT_MEASUREMENT,
            4, 1)) {
        success_count++;
    }

    /* 向量叉积 */
    if (preset_blocks_register_by_category(
            "vector_cross_product",
            "计算两个向量的叉积（二维）",
            PRESET_EXT_MEASUREMENT,
            4, 1)) {
        success_count++;
    }

    /* 向量夹角 */
    if (preset_blocks_register_by_category(
            "vector_angle",
            "计算两个向量的夹角",
            PRESET_EXT_MEASUREMENT,
            4, 1)) {
        success_count++;
    }

    /* -------------------- 曲率计算 -------------------- */

    /* 圆曲率 */
    if (preset_blocks_register_by_category(
            "circle_curvature",
            "计算圆的曲率",
            PRESET_EXT_MEASUREMENT,
            2, 1)) {
        success_count++;
    }

    /* 检查是否所有预设都注册成功 */
    return success_count == MEASUREMENTS_PRESET_COUNT;
}

int preset_measurements_count(void)
{
    return MEASUREMENTS_PRESET_COUNT;
}
