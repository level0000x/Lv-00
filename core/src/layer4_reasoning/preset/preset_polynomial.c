/**
 * @file preset_polynomial.c
 * @brief 多项式理论预设函数块 - 实现
 *
 * 实现理论数学研究中常用的多项式运算预设函数块。
 * 包括多项式算术运算、多项式分析、多项式求根和特殊多项式等。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module Polynomial
 * @category PRESET_CATEGORY_ALGEBRA
 * @version 3.3.0
 */

#include "preset_polynomial.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 多项式理论模块预设函数块总数 */
#define POLYNOMIAL_PRESET_COUNT 18

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个多项式预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有多项式预设都属于 PRESET_CATEGORY_ALGEBRA 类别。
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
static bool register_polynomial_preset(const char *name, const char *description, const PresetType *input_types,
                                       int input_count, PresetType output_type, const char *math_def,
                                       const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ALGEBRA, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/**
 * @brief 简化预设注册的宏
 *
 * 减少重复代码，提高可维护性。
 */
#define REGISTER_POLY(name, desc, inputs, in_count, output, math, comp, cons, rev)                             \
    do {                                                                                                       \
        if (register_polynomial_preset((name), (desc), (inputs), (in_count), (output), (math), (comp), (cons), \
                                       (rev))) {                                                               \
            success_count++;                                                                                   \
        } else {                                                                                               \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */                                                \
        }                                                                                                      \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_polynomial_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：多项式运算 (6个)
     * ============================================================ */

    /* -------------------- 多项式加法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_add", "多项式加法：计算两个多项式的和，对应系数相加", inputs, 2,
                      PRESET_TYPE_POLYNOMIAL, "(f + g)(x) = f(x) + g(x)", "O(n)", true, true);
    }

    /* -------------------- 多项式减法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_subtract", "多项式减法：计算两个多项式的差，对应系数相减", inputs, 2,
                      PRESET_TYPE_POLYNOMIAL, "(f - g)(x) = f(x) - g(x)", "O(n)", true, true);
    }

    /* -------------------- 多项式乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_multiply", "多项式乘法：计算两个多项式的积，卷积运算", inputs, 2,
                      PRESET_TYPE_POLYNOMIAL, "(f \\cdot g)(x) = f(x) \\cdot g(x)", "O(n^2)", true, false);
    }

    /* -------------------- 多项式除法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_divide", "多项式除法（带余）：计算 f = g*q + r，返回商 q 和余式 r", inputs, 2,
                      PRESET_TYPE_TUPLE, "f = g \\cdot q + r, \\quad \\deg(r) < \\deg(g)", "O(n^2)", true, false);
    }

    /* -------------------- 多项式GCD -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_gcd", "多项式GCD：使用欧几里得算法计算两个多项式的最大公因式", inputs, 2,
                      PRESET_TYPE_POLYNOMIAL, "\\gcd(f, g) = \\max\\{d : d|f \\land d|g\\}", "O(n^2)", true, false);
    }

    /* -------------------- 多项式LCM -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_lcm", "多项式LCM：计算两个多项式的最小公倍式", inputs, 2, PRESET_TYPE_POLYNOMIAL,
                      "\\text{lcm}(f, g) = \\frac{f \\cdot g}{\\gcd(f, g)}", "O(n^2)", true, false);
    }

    /* ============================================================
     * 第二部分：多项式分析 (5个)
     * ============================================================ */

    /* -------------------- 多项式次数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_degree", "多项式次数：返回多项式的最高次项的次数 deg(f)", inputs, 1,
                      PRESET_TYPE_INTEGER, "\\deg(f) = \\max\\{i : a_i \\neq 0\\}", "O(1)", false, false);
    }

    /* -------------------- 多项式求值 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_SCALAR};
        REGISTER_POLY("polynomial_evaluate", "多项式求值：使用秦九韶算法（Horner法则）计算 f(x_0) 的值", inputs, 2,
                      PRESET_TYPE_SCALAR, "f(x_0) = a_n x_0^n + a_{n-1} x_0^{n-1} + \\cdots + a_1 x_0 + a_0", "O(n)",
                      false, false);
    }

    /* -------------------- 多项式求导 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_derivative", "多项式求导：计算多项式的形式导数 f'(x)", inputs, 1,
                      PRESET_TYPE_POLYNOMIAL, "f'(x) = \\sum_{i=1}^{n} i \\cdot a_i \\cdot x^{i-1}", "O(n)", true,
                      false);
    }

    /* -------------------- 多项式积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_integral", "多项式积分：计算多项式的不定积分（忽略常数项）", inputs, 1,
                      PRESET_TYPE_POLYNOMIAL, "\\int f(x)\\,dx = \\sum_{i=0}^{n} \\frac{a_i}{i+1} x^{i+1} + C", "O(n)",
                      true, false);
    }

    /* -------------------- 多项式复合 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_compose", "多项式复合：计算复合多项式 f(g(x))", inputs, 2, PRESET_TYPE_POLYNOMIAL,
                      "(f \\circ g)(x) = f(g(x))", "O(n^2)", true, false);
    }

    /* ============================================================
     * 第三部分：多项式根 (4个)
     * ============================================================ */

    /* -------------------- 二次方程求根 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_POLY("polynomial_roots_quadratic", "二次方程求根：使用求根公式求解 ax^2 + bx + c = 0", inputs, 3,
                      PRESET_TYPE_LIST, "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}", "O(1)", true, false);
    }

    /* -------------------- 三次方程求根 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_POLY("polynomial_roots_cubic", "三次方程求根：使用Cardano公式求解 ax^3 + bx^2 + cx + d = 0", inputs, 4,
                      PRESET_TYPE_LIST, "x^3 + px + q = 0, \\quad \\Delta = -4p^3 - 27q^2 \\text{（Cardano公式）}",
                      "O(1)", true, false);
    }

    /* -------------------- 四次方程求根 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR,
                               PRESET_TYPE_SCALAR};
        REGISTER_POLY("polynomial_roots_quartic", "四次方程求根：使用Ferrari方法求解 ax^4 + bx^3 + cx^2 + dx + e = 0",
                      inputs, 5, PRESET_TYPE_LIST, "x^4 + px^2 + qx + r = 0 \\text{（Ferrari方法，化为三次预解方程）}",
                      "O(1)", true, false);
    }

    /* -------------------- 多项式因式分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY(
            "polynomial_factor", "多项式因式分解：将多项式分解为不可约因式的乘积", inputs, 1, PRESET_TYPE_LIST,
            "f(x) = a_n \\prod_{i=1}^{k} (x - r_i)^{e_i} \\cdot \\prod_{j=1}^{m} q_j(x)^{d_j}", "O(n^3)", true, false);
    }

    /* ============================================================
     * 第四部分：特殊多项式 (3个)
     * ============================================================ */

    /* -------------------- 结式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL, PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_resultant", "结式：计算两个多项式的结式 Res(f,g)，用于判断公共零点", inputs, 2,
                      PRESET_TYPE_SCALAR,
                      "\\text{Res}(f, g) = a_n^m \\prod_{i=1}^{n} g(r_i), \\quad r_i \\text{ 为 } f \\text{ 的根}",
                      "O(n^2)", false, false);
    }

    /* -------------------- 判别式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_POLYNOMIAL};
        REGISTER_POLY("polynomial_discriminant", "判别式：计算多项式的判别式 Delta(f)，判断根的重数", inputs, 1,
                      PRESET_TYPE_SCALAR,
                      "\\Delta(f) = (-1)^{\\frac{n(n-1)}{2}} \\cdot \\frac{\\text{Res}(f, f')}{a_n}", "O(n^2)", false,
                      false);
    }

    /* -------------------- 多项式插值 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        REGISTER_POLY(
            "polynomial_interpolation", "多项式插值：给定 n+1 个数据点，使用Lagrange或Newton插值构造过这些点的多项式",
            inputs, 2, PRESET_TYPE_POLYNOMIAL,
            "L(x) = \\sum_{i=0}^{n} y_i \\prod_{j \\neq i} \\frac{x - x_j}{x_i - x_j}", "O(n^2)", true, false);
    }

    /* 检查是否所有预设都注册成功 */
    ; /* 注册完成 */

    return success_count == POLYNOMIAL_PRESET_COUNT;
}

/* ==================== 模块信息接口 ==================== */

/**
 * @brief 获取多项式理论预设函数块数量
 *
 * @return int 多项式理论模块预设函数块总数
 */
int preset_polynomial_count(void) {
    return POLYNOMIAL_PRESET_COUNT;
}

/**
 * @brief 获取多项式理论预设的类别
 *
 * @return PresetCategory 返回 PRESET_CATEGORY_ALGEBRA
 */
PresetCategory preset_polynomial_category(void) {
    return PRESET_CATEGORY_ALGEBRA;
}

/**
 * @brief 获取多项式理论预设函数块名称列表
 *
 * 返回模块中所有预设函数块的名称列表。
 * 调用者负责释放名称数组和每个名称字符串。
 *
 * @param out_names 输出：名称数组指针（调用者释放）
 * @param out_count 输出：名称数量
 * @return true 成功
 * @return false 失败（参数为空或内存不足）
 */
bool preset_polynomial_get_names(char ***out_names, int *out_count) {
    PRESET_CHECK_NULL(out_names, error);
    PRESET_CHECK_NULL(out_count, error);

    /* 分配名称数组 */
    char **names = (char **) lv00_malloc(POLYNOMIAL_PRESET_COUNT * sizeof(char *));
    PRESET_CHECK_NULL(names, error);

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 多项式运算 */
        "polynomial_add",
        "polynomial_subtract",
        "polynomial_multiply",
        "polynomial_divide",
        "polynomial_gcd",
        "polynomial_lcm",
        /* 多项式分析 */
        "polynomial_degree",
        "polynomial_evaluate",
        "polynomial_derivative",
        "polynomial_integral",
        "polynomial_compose",
        /* 多项式根 */
        "polynomial_roots_quadratic",
        "polynomial_roots_cubic",
        "polynomial_roots_quartic",
        "polynomial_factor",
        /* 特殊多项式 */
        "polynomial_resultant",
        "polynomial_discriminant",
        "polynomial_interpolation",
    };

    int count = sizeof(preset_names) / sizeof(preset_names[0]);

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                {
                    void *tmp = names[j];
                    lv00_free(&tmp);
                }
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

error:
    return false;
}
