/**
 * @file preset_algebraic.c
 * @brief 代数运算预设函数块 - 实现
 *
 * 实现理论数学研究中常用的代数运算预设函数块。
 * 涵盖向量代数、坐标系变换、复数运算及圆锥曲线。
 *
 * @module Algebraic
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 5.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_algebraic.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_ALGEBRAIC 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv00_internal.h / lv00_utils.h
 *   -> 提供 lv00_malloc、lv00_free、lv00_strdup、lv00_log_* 等
 * ============================================================
 */
#include "preset_algebraic.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 代数模块预设函数块总数 ==================== */

/** 代数模块预设函数块总数 */
#define ALGEBRAIC_PRESET_COUNT 15

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个代数运算预设
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def LaTeX 数学定义
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_algebraic_preset(const char *name, const char *description, const PresetType *input_types,
                                      int input_count, PresetType output_type, const char *math_def,
                                      const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ALGEBRAIC, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== 注册辅助宏 ==================== */

/**
 * @brief 代数模块预设注册宏
 *
 * 封装单个代数学预设的注册调用，自动累加 success_count，
 * 并在失败时记录错误日志。
 */
#define REGISTER_ALGEBRAIC(name, desc, inputs, input_count, output, math_def, complexity, constructive, reversible) \
    do {                                                                                                            \
        if (register_algebraic_preset((name), (desc), (inputs), (input_count), (output), (math_def), (complexity),  \
                                      (constructive), (reversible))) {                                              \
            success_count++;                                                                                        \
        } else {                                                                                                    \
            PRESET_ERROR_LOG("代数预设注册失败: %s", (name));                                                       \
        }                                                                                                           \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_algebraic_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：向量代数
     * ============================================================ */

    /* -------------------- 向量加法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ALGEBRAIC(PRESET_VECTOR_ADD, "向量加法：给定向量OA和OB，构造和向量OC = OA + OB", inputs, 3,
                           PRESET_TYPE_POINT, "\\vec{OC} = \\vec{OA} + \\vec{OB}", "O(1)", true, true);
    }

    /* -------------------- 向量减法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ALGEBRAIC(PRESET_VECTOR_SUB, "向量减法：给定向量OA和OB，构造差向量OC = OA - OB", inputs, 3,
                           PRESET_TYPE_POINT, "\\vec{OC} = \\vec{OA} - \\vec{OB}", "O(1)", true, true);
    }

    /* -------------------- 向量数乘 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_ALGEBRAIC(PRESET_VECTOR_SCALE, "向量数乘：给定向量OA和标量k，构造数乘向量OB = k·OA", inputs, 3,
                           PRESET_TYPE_POINT, "\\vec{OB} = k \\cdot \\vec{OA}", "O(1)", true, true);
    }

    /* -------------------- 向量线性组合 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR, PRESET_TYPE_POINT,
                               PRESET_TYPE_SCALAR};
        REGISTER_ALGEBRAIC(PRESET_VECTOR_LINEAR_COMB, "向量线性组合：k1·OA1 + k2·OA2 = OB", inputs, 5,
                           PRESET_TYPE_POINT, "\\vec{OB} = k_1 \\vec{OA_1} + k_2 \\vec{OA_2}", "O(1)", true, false);
    }

    /* -------------------- 向量归一化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ALGEBRAIC(PRESET_VECTOR_NORMALIZE, "构造单位向量：将向量OA归一化为单位向量OB = OA/|OA|", inputs, 2,
                           PRESET_TYPE_POINT, "\\vec{OB} = \\frac{\\vec{OA}}{|\\vec{OA}|}", "O(1)", true, false);
    }

    /* -------------------- 向量投影 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ALGEBRAIC(PRESET_VECTOR_PROJECT, "向量投影：计算向量OA在向量OB方向上的投影向量OP", inputs, 3,
                           PRESET_TYPE_POINT,
                           "\\vec{OP} = \\text{proj}_{\\vec{OB}} \\vec{OA} = "
                           "\\frac{\\vec{OA} \\cdot \\vec{OB}}{|\\vec{OB}|^2} \\vec{OB}",
                           "O(1)", true, false);
    }

    /* ============================================================
     * 第二部分：坐标系与基底
     * ============================================================ */

    /* -------------------- 标准正交基 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ALGEBRAIC(PRESET_STANDARD_BASIS, "构造标准正交基：给定原点O和x轴方向点X，构造y轴单位向量OY", inputs, 2,
                           PRESET_TYPE_POINT, "\\vec{OY} \\perp \\vec{OX}, \\quad |\\vec{OY}| = 1", "O(1)", true,
                           false);
    }

    /* -------------------- 坐标变换 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT,
                               PRESET_TYPE_POINT};
        REGISTER_ALGEBRAIC(PRESET_COORDINATE_TRANSFORM, "坐标变换：将点P从旧坐标系变换到新坐标系下，得到P_new", inputs,
                           5, PRESET_TYPE_POINT, "P' = T(P), \\quad T: (x, y) \\mapsto (x', y')", "O(1)", true, true);
    }

    /* -------------------- 极坐标转直角坐标 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_ALGEBRAIC(PRESET_POLAR_TO_CARTESIAN, "极坐标转直角坐标：(r, θ) → (r·cosθ, r·sinθ)", inputs, 3,
                           PRESET_TYPE_POINT, "(r \\cos \\theta, \\, r \\sin \\theta)", "O(1)", true, false);
    }

    /* ============================================================
     * 第三部分：复数运算（几何表示）
     * ============================================================ */

    /* -------------------- 复数乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ALGEBRAIC(PRESET_COMPLEX_MULTIPLY, "复数乘法的几何表示：z1·z2 的几何构造", inputs, 3,
                           PRESET_TYPE_POINT, "z_1 \\cdot z_2 = (x_1 x_2 - y_1 y_2) + i(x_1 y_2 + x_2 y_1)", "O(1)",
                           true, true);
    }

    /* -------------------- 复数除法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        REGISTER_ALGEBRAIC(PRESET_COMPLEX_DIVIDE, "复数除法的几何表示：z1/z2 的几何构造", inputs, 3, PRESET_TYPE_POINT,
                           "z_1 / z_2 = \\frac{z_1 \\overline{z_2}}{|z_2|^2}", "O(1)", true, true);
    }

    /* -------------------- 复数幂运算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_INTEGER};
        REGISTER_ALGEBRAIC(PRESET_COMPLEX_POWER, "复数的n次幂：zⁿ 的几何构造", inputs, 3, PRESET_TYPE_POINT,
                           "z^n = r^n (\\cos n\\theta + i\\sin n\\theta)", "O(\\log n)", true, false);
    }

    /* -------------------- 复数开方 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_INTEGER};
        REGISTER_ALGEBRAIC(PRESET_COMPLEX_ROOT, "复数的n次方根：ⁿ√z 的几何构造（主值支）", inputs, 3, PRESET_TYPE_POINT,
                           "\\sqrt[n]{z} = \\sqrt[n]{r}\\left(\\cos\\frac{\\theta+2k\\pi}{n} + "
                           "i\\sin\\frac{\\theta+2k\\pi}{n}\\right)",
                           "O(1)", true, false);
    }

    /* ============================================================
     * 第四部分：圆锥曲线
     * ============================================================ */

    /* -------------------- 抛物线上的点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        REGISTER_ALGEBRAIC(PRESET_PARABOLA_POINT, "抛物线上的点：给定焦点和准线，构造抛物线上参数t对应的点", inputs, 4,
                           PRESET_TYPE_POINT, "|PF| = |Pl|, \\quad P \\in \\text{抛物线}", "O(1)", true, false);
    }

    /* -------------------- 椭圆上的点 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_ALGEBRAIC(PRESET_ELLIPSE_POINT, "椭圆上的点：给定两焦点和长轴长度，构造椭圆上参数t对应的点", inputs, 4,
                           PRESET_TYPE_POINT, "|PF_1| + |PF_2| = 2a, \\quad P \\in \\text{椭圆}", "O(1)", true, false);
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == ALGEBRAIC_PRESET_COUNT;
}

/**
 * @brief 获取代数运算预设函数块数量
 *
 * @return int 代数模块预设函数块总数
 */
int preset_algebraic_count(void) {
    return ALGEBRAIC_PRESET_COUNT;
}

/**
 * @brief 获取代数模块的预设类别
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ALGEBRAIC
 */
PresetCategory preset_algebraic_category(void) {
    return PRESET_CATEGORY_ALGEBRAIC;
}

/**
 * @brief 获取代数模块所有预设的名称列表
 *
 * 分配并返回包含所有代数预设名称的字符串数组。
 * 调用者负责释放 out_names 中的每个字符串和数组本身。
 *
 * @param out_names 输出：预设名称字符串数组的指针
 * @param out_count 输出：预设名称数量
 * @return true 成功获取
 * @return false 参数为空或内存分配失败
 */
bool preset_algebraic_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    /* 分配名称数组 */
    char **names = (char **) lv00_malloc(ALGEBRAIC_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 向量代数 */
        PRESET_VECTOR_ADD,
        PRESET_VECTOR_SUB,
        PRESET_VECTOR_SCALE,
        PRESET_VECTOR_LINEAR_COMB,
        PRESET_VECTOR_NORMALIZE,
        PRESET_VECTOR_PROJECT,
        /* 坐标系与基底 */
        PRESET_STANDARD_BASIS,
        PRESET_COORDINATE_TRANSFORM,
        PRESET_POLAR_TO_CARTESIAN,
        /* 复数运算（几何表示） */
        PRESET_COMPLEX_MULTIPLY,
        PRESET_COMPLEX_DIVIDE,
        PRESET_COMPLEX_POWER,
        PRESET_COMPLEX_ROOT,
        /* 圆锥曲线 */
        PRESET_PARABOLA_POINT,
        PRESET_ELLIPSE_POINT,
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
