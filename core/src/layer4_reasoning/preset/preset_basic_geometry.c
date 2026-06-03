/**
 * @file preset_basic_geometry.c
 * @brief 基础几何构造预设函数块 - 实现（v5.0 升级版）
 *
 * 实现基础几何构造模块的预设函数块。
 * 涵盖点的构造、线段操作、直线和射线、圆的构造、交点计算、反射与对称等。
 *
 * @module BasicGeometry
 * @category PRESET_CATEGORY_CONSTRUCTION
 * @version 5.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_basic_geometry.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、PresetCategory 枚举
 * preset_blocks.h
 *   -> 提供 preset_blocks_register_simple() 声明
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、预设公共辅助函数
 * preset_register_helper.h
 *   -> 提供 LV00_PRESET_REGISTER_EX 等统一注册辅助宏
 * lv00_internal.h / lv00_utils.h
 *   -> 提供 lv00_malloc、lv00_free、lv00_strdup、lv00_log_* 等
 * ============================================================
 */
#include "preset_basic_geometry.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h" /* 预设公共宏与辅助函数 */
#include "preset_register_helper.h" /* 统一注册辅助宏 */

/* ==================== 预设函数块数量 ==================== */

/** 基础几何模块预设函数块总数 */
#define BASIC_GEOMETRY_PRESET_COUNT 25

/* ==================== 模块注册实现 ==================== */

bool preset_basic_geometry_register(void) {
    int success_count = 0;
    int total_count = 0;

    /* ============================================================
     * 第一部分：点的构造
     * ============================================================ */

    /* -------------------- 通过直角坐标构造点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_POINT_FROM_COORDS,
                                PRESET_TYPE_POINT, inputs, 2, "通过笛卡尔直角坐标构造点 P(x, y)",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 中点构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_MIDPOINT,
                                PRESET_TYPE_POINT, inputs, 2, "构造两点之间的中点 M = (A+B)/2",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 重心构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_CENTROID,
                                PRESET_TYPE_POINT, inputs, 3, "构造三角形的重心 G = (A+B+C)/3",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 外心构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_CIRCUMCENTER,
                                PRESET_TYPE_POINT, inputs, 3, "构造三角形的外心（外接圆圆心）",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 内心构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_INCENTER,
                                PRESET_TYPE_POINT, inputs, 3, "构造三角形的内心（内切圆圆心）",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 垂心构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_ORTHOCENTER,
                                PRESET_TYPE_POINT, inputs, 3, "构造三角形的垂心（三条高线的交点）",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* ============================================================
     * 第二部分：线段操作
     * ============================================================ */

    /* -------------------- 通过两点构造线段 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_SEGMENT_FROM_POINTS,
                                PRESET_TYPE_LINE_SEGMENT, inputs, 2, "通过两点构造线段 AB",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 垂直平分线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_PERPENDICULAR_BISECTOR,
                                PRESET_TYPE_LINE, inputs, 2, "构造线段的垂直平分线",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 中垂线上的点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_POINT_ON_PERP_BISECTOR,
                                PRESET_TYPE_POINT, inputs, 3, "在中垂线上构造距离中点为 d 的点",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* ============================================================
     * 第三部分：直线和射线
     * ============================================================ */

    /* -------------------- 通过两点构造直线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINE_FROM_POINTS,
                                PRESET_TYPE_LINE, inputs, 2, "通过两点构造无限直线",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 平行线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_PARALLEL_LINE,
                                PRESET_TYPE_LINE, inputs, 3, "过点作平行于给定线段的直线",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 垂线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_PERPENDICULAR_LINE,
                                PRESET_TYPE_LINE, inputs, 3, "过点作垂直于给定线段的直线",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 射线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_RAY_FROM_POINTS,
                                PRESET_TYPE_RAY, inputs, 2, "通过起点和方向点构造射线",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* ============================================================
     * 第四部分：圆的构造
     * ============================================================ */

    /* -------------------- 圆心和半径构造圆 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_CIRCLE_CENTER_RADIUS,
                                PRESET_TYPE_CIRCLE, inputs, 2, "通过圆心和半径点构造圆",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 三点定圆 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_CIRCLE_THREE_POINTS,
                                PRESET_TYPE_CIRCLE, inputs, 3, "通过三点构造外接圆",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 切线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_TANGENT_FROM_POINT,
                                PRESET_TYPE_LINE, inputs, 3, "从外部点向圆作切线",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* ============================================================
     * 第五部分：交点计算
     * ============================================================ */

    /* -------------------- 两直线交点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINE_INTERSECTION,
                                PRESET_TYPE_POINT, inputs, 4, "计算两条直线的交点",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 直线与圆交点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINE_CIRCLE_INTERSECTION,
                                PRESET_TYPE_POINT, inputs, 4, "计算直线与圆的交点",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 两圆交点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_CIRCLE_CIRCLE_INTERSECTION,
                                PRESET_TYPE_POINT, inputs, 4, "计算两个圆的交点",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* ============================================================
     * 第六部分：反射与对称
     * ============================================================ */

    /* -------------------- 点关于直线的反射 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_REFLECT_POINT_OVER_LINE,
                                PRESET_TYPE_POINT, inputs, 3, "点关于直线的反射（对称点）",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 点关于点的反射 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_REFLECT_POINT_OVER_POINT,
                                PRESET_TYPE_POINT, inputs, 2, "点关于点的中心对称",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* ============================================================
     * 第七部分：特殊点构造
     * ============================================================ */

    /* -------------------- 按比例分割线段 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_POINT_DIVIDE_SEGMENT,
                                PRESET_TYPE_POINT, inputs, 3, "按比例分割线段的点",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* -------------------- 调和共轭点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_HARMONIC_CONJUGATE,
                                PRESET_TYPE_POINT, inputs, 3, "构造调和共轭点",
                                PRESET_CATEGORY_CONSTRUCTION, "O(1)", false);
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == BASIC_GEOMETRY_PRESET_COUNT;
}

/**
 * @brief 获取基础几何预设函数块数量
 *
 * 返回基础几何模块的预设函数块总数，使用编译期常量直接返回，
 * 避免运行时计算，性能最优。
 *
 * @return int 基础几何模块预设函数块总数
 */
int preset_basic_geometry_count(void) {
    return BASIC_GEOMETRY_PRESET_COUNT;
}

/**
 * @brief 获取基础几何预设的类别
 *
 * 基础几何模块中所有预设均属于同一类别 PRESET_CATEGORY_CONSTRUCTION。
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_CONSTRUCTION
 */
PresetCategory preset_basic_geometry_category(void) {
    return PRESET_CATEGORY_CONSTRUCTION;
}

/**
 * @brief 获取基础几何预设名称列表
 *
 * 分配并返回基础几何模块中所有预设的名称数组。
 * 调用者负责释放每个名称字符串以及数组本身。
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出名称数量
 * @return true 成功获取名称列表
 * @return false 参数无效或内存分配失败
 */
bool preset_basic_geometry_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    /* 分配名称数组 */
    char **names = (char **) lv00_malloc(BASIC_GEOMETRY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    /* 预设名称列表（与注册顺序一致） */
    const char *preset_names[] = {
        /* 点的构造 */
        PRESET_POINT_FROM_COORDS,
        PRESET_MIDPOINT,
        PRESET_CENTROID,
        PRESET_CIRCUMCENTER,
        PRESET_INCENTER,
        PRESET_ORTHOCENTER,
        /* 线段操作 */
        PRESET_SEGMENT_FROM_POINTS,
        PRESET_PERPENDICULAR_BISECTOR,
        PRESET_POINT_ON_PERP_BISECTOR,
        /* 直线和射线 */
        PRESET_LINE_FROM_POINTS,
        PRESET_PARALLEL_LINE,
        PRESET_PERPENDICULAR_LINE,
        PRESET_RAY_FROM_POINTS,
        /* 圆的构造 */
        PRESET_CIRCLE_CENTER_RADIUS,
        PRESET_CIRCLE_THREE_POINTS,
        PRESET_TANGENT_FROM_POINT,
        /* 交点计算 */
        PRESET_LINE_INTERSECTION,
        PRESET_LINE_CIRCLE_INTERSECTION,
        /* 反射与对称 */
        PRESET_REFLECT_POINT_OVER_LINE,
        PRESET_REFLECT_POINT_OVER_POINT,
        /* 特殊点构造 */
        PRESET_POINT_DIVIDE_SEGMENT,
        PRESET_HARMONIC_CONJUGATE,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv00_free(&tmp);
            }
            {
                void *tmp = names;
                lv00_free(&tmp);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
