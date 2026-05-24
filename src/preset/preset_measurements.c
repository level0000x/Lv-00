/**
 * @file preset_measurements.c
 * @brief 几何度量计算预设函数块 - 实现
 *
 * @details 实现几何度量计算模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共20个预设，涵盖距离、角度、面积、长度、向量运算等。
 *
 * @module Measurements
 * @category PRESET_CATEGORY_MEASUREMENT
 * @version 1.0.0
 */

#include "preset_measurements.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 几何度量模块预设函数块总数 */
#define MEASUREMENTS_PRESET_COUNT 20

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 注册单个几何度量预设
 *
 * 辅助函数，用于简化预设注册过程。
 * 所有几何度量预设都属于 PRESET_CATEGORY_MEASUREMENT 类别。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义（LaTeX格式）
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_measure_preset(const char *name, const char *description, const PresetType *input_types,
                                    int input_count, PresetType output_type, const char *math_def,
                                    const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_MEASUREMENT, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ============================================================
 * 模块注册实现
 * ============================================================ */

bool preset_measurements_register(void) {
    int success_count = 0;

    /* ============================================================
     * 距离度量 (6个)
     * ============================================================ */

    /* 欧几里得距离 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_DISTANCE_EUCLIDEAN, "欧几里得距离：计算两点间的直线距离", inputs, 2,
                                    PRESET_TYPE_DISTANCE, "d(A,B) = \\sqrt{(x_B-x_A)^2 + (y_B-y_A)^2}", "O(1)", true,
                                    false)) {
            success_count++;
        }
    }

    /* 欧几里得距离平方 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_DISTANCE_SQUARED, "欧几里得距离平方：避免开方运算，用于距离比较", inputs, 2,
                                    PRESET_TYPE_SCALAR, "d^2(A,B) = (x_B-x_A)^2 + (y_B-y_A)^2", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 曼哈顿距离 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_DISTANCE_MANHATTAN, "曼哈顿距离：两点在坐标轴上的距离之和（L1范数）", inputs,
                                    2, PRESET_TYPE_DISTANCE, "d_1(A,B) = |x_B-x_A| + |y_B-y_A|", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 切比雪夫距离 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_DISTANCE_CHEBYSHEV, "切比雪夫距离：两点在坐标轴上的最大距离差（L∞范数）",
                                    inputs, 2, PRESET_TYPE_DISTANCE, "d_\\infty(A,B) = \\max(|x_B-x_A|, |y_B-y_A|)",
                                    "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 点到直线的距离 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_DISTANCE_POINT_TO_LINE, "点到直线的距离：计算点到直线的最短距离", inputs, 3,
                                    PRESET_TYPE_DISTANCE, "d(P, l) = \\frac{|ax_0 + by_0 + c|}{\\sqrt{a^2+b^2}}",
                                    "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 点到线段的距离 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_DISTANCE_POINT_TO_SEGMENT, "点到线段的距离：计算点到线段的最短距离", inputs,
                                    3, PRESET_TYPE_DISTANCE, "d(P, \\overline{AB})", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 角度度量 (3个)
     * ============================================================ */

    /* 三点形成的角度 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(
                PRESET_ANGLE_THREE_POINTS, "三点形成的角度：计算以中间点为顶点的角度（弧度）", inputs, 3,
                PRESET_TYPE_ANGLE,
                "\\angle ABC = \\arccos\\left(\\frac{\\vec{BA} \\cdot \\vec{BC}}{|\\vec{BA}||\\vec{BC}|}\\right)",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 两直线夹角 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(
                PRESET_ANGLE_TWO_LINES, "两直线夹角：计算两条直线的夹角（弧度）", inputs, 4, PRESET_TYPE_ANGLE,
                "\\theta = \\arccos\\left(\\frac{|m_1 - m_2|}{1 + m_1 m_2}\\right)", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 有向角 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_DIRECTED_ANGLE, "有向角：从射线BA到射线BC的有向角（带符号）", inputs, 3,
                                    PRESET_TYPE_ANGLE, "\\angle(\\vec{BA}, \\vec{BC}) \\in (-\\pi, \\pi]", "O(1)", true,
                                    false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 面积计算 (4个)
     * ============================================================ */

    /* 三角形面积（坐标公式） */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_TRIANGLE_AREA, "三角形面积：使用坐标公式计算三角形面积", inputs, 3,
                                    PRESET_TYPE_AREA, "S = \\frac{1}{2}|(x_B-x_A)(y_C-y_A) - (x_C-x_A)(y_B-y_A)|",
                                    "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 三角形面积（海伦公式） */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_TRIANGLE_AREA_HERON, "三角形面积：使用海伦公式计算三角形面积", inputs, 3,
                                    PRESET_TYPE_AREA, "S = \\sqrt{s(s-a)(s-b)(s-c)}, \\quad s = \\frac{a+b+c}{2}",
                                    "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 圆面积 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_CIRCLE_AREA, "圆面积：计算圆的面积", inputs, 2, PRESET_TYPE_AREA,
                                    "S = \\pi r^2", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 扇形面积 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        if (register_measure_preset(PRESET_SECTOR_AREA, "扇形面积：计算扇形的面积", inputs, 3, PRESET_TYPE_AREA,
                                    "S = \\frac{1}{2}r^2\\theta", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 长度计算 (2个)
     * ============================================================ */

    /* 线段长度 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_SEGMENT_LENGTH, "线段长度：计算线段的长度", inputs, 2, PRESET_TYPE_DISTANCE,
                                    "|AB| = \\sqrt{(x_B-x_A)^2 + (y_B-y_A)^2}", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 圆周长 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_CIRCLE_CIRCUMFERENCE, "圆周长：计算圆的周长", inputs, 2,
                                    PRESET_TYPE_DISTANCE, "C = 2\\pi r", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 向量运算 (4个)
     * ============================================================ */

    /* 向量模长 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_VECTOR_MAGNITUDE, "向量模长：计算向量的长度", inputs, 2, PRESET_TYPE_SCALAR,
                                    "|\\vec{v}| = \\sqrt{v_x^2 + v_y^2}", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 向量点积 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_VECTOR_DOT_PRODUCT, "向量点积：计算两个向量的点积（内积）", inputs, 4,
                                    PRESET_TYPE_SCALAR, "\\vec{a} \\cdot \\vec{b} = a_x b_x + a_y b_y", "O(1)", true,
                                    false)) {
            success_count++;
        }
    }

    /* 向量叉积（二维） */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_VECTOR_CROSS_PRODUCT, "向量叉积：计算两个向量的叉积（二维，返回标量）",
                                    inputs, 4, PRESET_TYPE_SCALAR, "\\vec{a} \\times \\vec{b} = a_x b_y - a_y b_x",
                                    "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 向量夹角 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(
                PRESET_VECTOR_ANGLE, "向量夹角：计算两个向量之间的夹角（弧度）", inputs, 4, PRESET_TYPE_ANGLE,
                "\\theta = \\arccos\\left(\\frac{\\vec{a} \\cdot \\vec{b}}{|\\vec{a}||\\vec{b}|}\\right)", "O(1)", true,
                false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 曲率计算 (1个)
     * ============================================================ */

    /* 圆的曲率 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_measure_preset(PRESET_CIRCLE_CURVATURE, "圆的曲率：计算圆的曲率（半径的倒数）", inputs, 2,
                                    PRESET_TYPE_SCALAR, "\\kappa = \\frac{1}{r}", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == MEASUREMENTS_PRESET_COUNT;
}

/**
 * @brief 获取度量模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_measurements_count(void) {
    return MEASUREMENTS_PRESET_COUNT;
}

/**
 * @brief 获取度量模块的类别
 *
 * @return 预设类别枚举值
 */
PresetCategory preset_measurements_category(void) {
    return PRESET_CATEGORY_MEASUREMENT;
}

/**
 * @brief 获取度量模块所有预设名称
 *
 * @param[out] out_names 输出名称数组指针
 * @param[out] out_count 输出名称数量
 * @return true 成功获取
 * @return false 参数无效或内存分配失败
 */
bool preset_measurements_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    *out_count = MEASUREMENTS_PRESET_COUNT;
    *out_names = (char **) lv00_malloc((size_t) MEASUREMENTS_PRESET_COUNT * sizeof(char *));
    if (!*out_names)
        return false;

    int idx = 0;
    (*out_names)[idx++] = lv00_strdup(PRESET_DISTANCE_EUCLIDEAN);
    (*out_names)[idx++] = lv00_strdup(PRESET_DISTANCE_SQUARED);
    (*out_names)[idx++] = lv00_strdup(PRESET_DISTANCE_MANHATTAN);
    (*out_names)[idx++] = lv00_strdup(PRESET_DISTANCE_CHEBYSHEV);
    (*out_names)[idx++] = lv00_strdup(PRESET_DISTANCE_POINT_TO_LINE);
    (*out_names)[idx++] = lv00_strdup(PRESET_DISTANCE_POINT_TO_SEGMENT);
    (*out_names)[idx++] = lv00_strdup(PRESET_ANGLE_THREE_POINTS);
    (*out_names)[idx++] = lv00_strdup(PRESET_ANGLE_TWO_LINES);
    (*out_names)[idx++] = lv00_strdup(PRESET_DIRECTED_ANGLE);
    (*out_names)[idx++] = lv00_strdup(PRESET_TRIANGLE_AREA);
    (*out_names)[idx++] = lv00_strdup(PRESET_TRIANGLE_AREA_HERON);
    (*out_names)[idx++] = lv00_strdup(PRESET_CIRCLE_AREA);
    (*out_names)[idx++] = lv00_strdup(PRESET_SECTOR_AREA);
    (*out_names)[idx++] = lv00_strdup(PRESET_SEGMENT_LENGTH);
    (*out_names)[idx++] = lv00_strdup(PRESET_CIRCLE_CIRCUMFERENCE);
    (*out_names)[idx++] = lv00_strdup(PRESET_VECTOR_MAGNITUDE);
    (*out_names)[idx++] = lv00_strdup(PRESET_VECTOR_DOT_PRODUCT);
    (*out_names)[idx++] = lv00_strdup(PRESET_VECTOR_CROSS_PRODUCT);
    (*out_names)[idx++] = lv00_strdup(PRESET_VECTOR_ANGLE);
    (*out_names)[idx++] = lv00_strdup(PRESET_CIRCLE_CURVATURE);

    /* 检查是否有分配失败 */
    for (int i = 0; i < idx; i++) {
        if (!(*out_names)[i]) {
            /* 回滚已分配的内存 */
            for (int j = 0; j < i; j++) {
                lv00_free((void **) &(*out_names)[j]);
            }
            lv00_free((void **) out_names);
            return false;
        }
    }

    return true;
}
