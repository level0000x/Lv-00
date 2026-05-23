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
 * lv00_internal.h / lv00_utils.h
 *   -> 提供 lv00_malloc、lv00_free、lv00_strdup、lv00_log_* 等
 * ============================================================
 */
#include "preset_basic_geometry.h"
#include "preset_blocks.h"
#include "preset_common.h"     /* 预设公共宏与辅助函数 */
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 基础几何模块预设函数块总数 */
#define BASIC_GEOMETRY_PRESET_COUNT 25

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个基础几何预设
 *
 * 包装 preset_blocks_register_simple()，自动填充类别为 PRESET_CATEGORY_CONSTRUCTION。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义（LaTeX 格式）
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_geometry_preset(
    const char *name,
    const char *description,
    const PresetType *input_types,
    int input_count,
    PresetType output_type,
    const char *math_def,
    const char *complexity,
    bool is_constructive,
    bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description,
        PRESET_CATEGORY_CONSTRUCTION,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_basic_geometry_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：点的构造
     * ============================================================ */

    /* -------------------- 通过直角坐标构造点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_geometry_preset(
                PRESET_POINT_FROM_COORDS,
                "通过笛卡尔直角坐标构造点 P(x, y)",
                inputs, 2, PRESET_TYPE_POINT,
                "P = (x, y)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 中点构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_MIDPOINT,
                "构造两点之间的中点 M = (A+B)/2",
                inputs, 2, PRESET_TYPE_POINT,
                "M = \\frac{A + B}{2}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 重心构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_CENTROID,
                "构造三角形的重心 G = (A+B+C)/3",
                inputs, 3, PRESET_TYPE_POINT,
                "G = \\frac{A + B + C}{3}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 外心构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_CIRCUMCENTER,
                "构造三角形的外心（外接圆圆心）",
                inputs, 3, PRESET_TYPE_POINT,
                "O = \\text{外心}(\\triangle ABC): OA = OB = OC",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 内心构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_INCENTER,
                "构造三角形的内心（内切圆圆心）",
                inputs, 3, PRESET_TYPE_POINT,
                "I = \\frac{aA + bB + cC}{a + b + c}, \\ a = |BC|, b = |CA|, c = |AB|",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 垂心构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_ORTHOCENTER,
                "构造三角形的垂心（三条高线的交点）",
                inputs, 3, PRESET_TYPE_POINT,
                "H = \\text{垂心}(\\triangle ABC): \\overrightarrow{AH} \\perp \\overrightarrow{BC}, "
                "\\overrightarrow{BH} \\perp \\overrightarrow{CA}, \\overrightarrow{CH} \\perp \\overrightarrow{AB}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：线段操作
     * ============================================================ */

    /* -------------------- 通过两点构造线段 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_SEGMENT_FROM_POINTS,
                "通过两点构造线段 AB",
                inputs, 2, PRESET_TYPE_LINE_SEGMENT,
                "\\overline{AB} = \\{A + t(B - A) \\mid t \\in [0, 1]\\}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 垂直平分线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_PERPENDICULAR_BISECTOR,
                "构造线段的垂直平分线",
                inputs, 2, PRESET_TYPE_LINE,
                "\\ell: (X - M) \\cdot (B - A) = 0, \\quad M = \\frac{A + B}{2}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 中垂线上的点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        if (register_geometry_preset(
                PRESET_POINT_ON_PERP_BISECTOR,
                "在中垂线上构造距离中点为 d 的点",
                inputs, 3, PRESET_TYPE_POINT,
                "P = M \\pm d \\cdot \\hat{n}, \\quad M = \\frac{A + B}{2}, \\ \\hat{n} \\perp \\overrightarrow{AB}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：直线和射线
     * ============================================================ */

    /* -------------------- 通过两点构造直线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_LINE_FROM_POINTS,
                "通过两点构造无限直线",
                inputs, 2, PRESET_TYPE_LINE,
                "\\ell: A + t(B - A), \\quad t \\in \\mathbb{R}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 平行线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_PARALLEL_LINE,
                "过点作平行于给定线段的直线",
                inputs, 3, PRESET_TYPE_LINE,
                "\\ell': P + t(B - A), \\quad t \\in \\mathbb{R}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 垂线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_PERPENDICULAR_LINE,
                "过点作垂直于给定线段的直线",
                inputs, 3, PRESET_TYPE_LINE,
                "\\ell': (X - P) \\cdot (B - A) = 0",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 射线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_RAY_FROM_POINTS,
                "通过起点和方向点构造射线",
                inputs, 2, PRESET_TYPE_RAY,
                "\\overrightarrow{AB} = \\{A + t(B - A) \\mid t \\ge 0\\}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：圆的构造
     * ============================================================ */

    /* -------------------- 圆心和半径构造圆 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_CIRCLE_CENTER_RADIUS,
                "通过圆心和半径点构造圆",
                inputs, 2, PRESET_TYPE_CIRCLE,
                "\\odot(O, P): |X - O| = |P - O|",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 三点定圆 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_CIRCLE_THREE_POINTS,
                "通过三点构造外接圆",
                inputs, 3, PRESET_TYPE_CIRCLE,
                "\\odot(ABC): |X - O| = R, \\quad O = \\text{外心}(\\triangle ABC)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 切线 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_TANGENT_FROM_POINT,
                "从外部点向圆作切线",
                inputs, 3, PRESET_TYPE_LINE,
                "\\ell: (X - T) \\cdot (T - O) = 0, \\quad |T - O| = |P - O|, \\ "
                "(O - T) \\perp (P - T)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：交点计算
     * ============================================================ */

    /* -------------------- 两直线交点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT,
                                PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_LINE_INTERSECTION,
                "计算两条直线的交点",
                inputs, 4, PRESET_TYPE_POINT,
                "\\ell_1 \\cap \\ell_2 = \\{P\\}, \\quad "
                "\\ell_1 = \\overline{A_1A_2}, \\ \\ell_2 = \\overline{B_1B_2}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 直线与圆交点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT,
                                PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_LINE_CIRCLE_INTERSECTION,
                "计算直线与圆的交点",
                inputs, 4, PRESET_TYPE_POINT,
                "\\ell \\cap \\odot(O, r) = \\{P_1, P_2\\}, \\quad "
                "\\ell = \\overline{A_1A_2}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 两圆交点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT,
                                PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_CIRCLE_CIRCLE_INTERSECTION,
                "计算两个圆的交点",
                inputs, 4, PRESET_TYPE_POINT,
                "\\odot_1(O_1, r_1) \\cap \\odot_2(O_2, r_2) = \\{P_1, P_2\\}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：反射与对称
     * ============================================================ */

    /* -------------------- 点关于直线的反射 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_REFLECT_POINT_OVER_LINE,
                "点关于直线的反射（对称点）",
                inputs, 3, PRESET_TYPE_POINT,
                "P' = P - 2\\frac{(P - A) \\cdot n}{|n|^2}n, \\quad "
                "n \\perp \\overrightarrow{AB}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 点关于点的反射 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_REFLECT_POINT_OVER_POINT,
                "点关于点的中心对称",
                inputs, 2, PRESET_TYPE_POINT,
                "P' = 2C - P",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第七部分：特殊点构造
     * ============================================================ */

    /* -------------------- 按比例分割线段 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        if (register_geometry_preset(
                PRESET_POINT_DIVIDE_SEGMENT,
                "按比例分割线段的点",
                inputs, 3, PRESET_TYPE_POINT,
                "P = A + t(B - A), \\quad t \\in \\mathbb{R}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 调和共轭点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_geometry_preset(
                PRESET_HARMONIC_CONJUGATE,
                "构造调和共轭点",
                inputs, 3, PRESET_TYPE_POINT,
                "(A, B; C, D) = -1, \\quad D = \\text{调和共轭}(A, B, C)",
                "O(1)", true, false)) {
            success_count++;
        }
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
int preset_basic_geometry_count(void)
{
    return BASIC_GEOMETRY_PRESET_COUNT;
}

/**
 * @brief 获取基础几何预设的类别
 *
 * 基础几何模块中所有预设均属于同一类别 PRESET_CATEGORY_CONSTRUCTION。
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_CONSTRUCTION
 */
PresetCategory preset_basic_geometry_category(void)
{
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
bool preset_basic_geometry_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    /* 分配名称数组 */
    char **names = (char**)lv00_malloc(BASIC_GEOMETRY_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

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
        PRESET_CIRCLE_CIRCLE_INTERSECTION,
        /* 反射与对称 */
        PRESET_REFLECT_POINT_OVER_LINE,
        PRESET_REFLECT_POINT_OVER_POINT,
        /* 特殊点构造 */
        PRESET_POINT_DIVIDE_SEGMENT,
        PRESET_HARMONIC_CONJUGATE,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) { void *tmp = names[j]; lv00_free(&tmp); }
            { void *tmp = names; lv00_free(&tmp); }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
