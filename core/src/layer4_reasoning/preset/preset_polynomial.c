/**
 * @file preset_polynomial.c
 * @brief 多项式理论预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/polynomial.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的多项式运算预设函数块。
 * 包括多项式算术运算、多项式分析、多项式求根和特殊多项式等。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module Polynomial
 * @category PRESET_CATEGORY_ALGEBRA
 * @version 4.0.0
 */

#include "preset_polynomial.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 多项式理论模块预设函数块总数：18（与头文件中 POLYNOMIAL_PRESET_COUNT 一致） */


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
    char **names = (char **) lv_malloc(POLYNOMIAL_PRESET_COUNT * sizeof(char *));
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
        names[i] = lv_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                {
                    void *tmp = names[j];
                    lv_free(&tmp);
                }
            }
            {
                void *tmp = names;
                lv_free(&tmp);
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
