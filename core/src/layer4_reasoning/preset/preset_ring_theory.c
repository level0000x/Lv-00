/**
 * @file preset_ring_theory.c
 * @brief 环论预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/ring_theory.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的环论运算预设函数块。
 * 涵盖环基础运算、理想理论、环同态、特殊环、多项式环及环结构分析。
 * 共31个预设函数块，均遵循模块化、确定性原则。
 *
 * @module RingTheory
 * @category PRESET_CATEGORY_RING_THEORY
 * @version 3.3.0
 * @author Lv-00 开发团队
 */

#include "preset_ring_theory.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 环论模块预设函数块总数：31（与头文件中 RING_THEORY_PRESET_COUNT 一致） */


/**
 * @brief 获取环论预设函数块数量
 *
 * @return int 环论模块预设函数块总数
 */
int preset_ring_theory_count(void) {
    return RING_THEORY_PRESET_COUNT;
}

PresetCategory preset_ring_theory_category(void) {
    return PRESET_CATEGORY_RING_THEORY;
}

bool preset_ring_theory_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv_malloc(RING_THEORY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* 环基础运算 */
        PRESET_RING_ADDITION,
        PRESET_RING_MULTIPLICATION,
        PRESET_RING_ADDITIVE_INVERSE,
        PRESET_RING_MULTIPLICATIVE_INVERSE,
        PRESET_RING_ZERO_ELEMENT,
        PRESET_RING_IDENTITY_ELEMENT,
        /* 理想相关 */
        PRESET_RING_IDEAL_TEST,
        PRESET_PRINCIPAL_IDEAL,
        PRESET_RING_IDEAL_SUM,
        PRESET_RING_IDEAL_INTERSECTION,
        PRESET_RING_QUOTIENT_RING,
        PRESET_RING_MAXIMAL_IDEAL_TEST,
        PRESET_RING_PRIME_IDEAL_TEST,
        /* 环同态 */
        PRESET_RING_HOMOMORPHISM_TEST,
        PRESET_RING_HOMOMORPHISM_KERNEL,
        PRESET_RING_HOMOMORPHISM_IMAGE,
        PRESET_RING_ISOMORPHISM_TEST,
        /* 特殊环 */
        PRESET_RING_INTEGRAL_DOMAIN_TEST,
        PRESET_RING_FIELD_TEST,
        PRESET_RING_EUCLIDEAN_DOMAIN_TEST,
        PRESET_RING_PID_TEST,
        PRESET_RING_UFD_TEST,
        /* 多项式环 */
        PRESET_RING_POLY_ADDITION,
        PRESET_RING_POLY_MULTIPLICATION,
        PRESET_RING_POLY_GCD,
        PRESET_RING_POLY_EVALUATION,
        PRESET_RING_POLY_IRREDUCIBLE_TEST,
        /* 环结构 */
        PRESET_RING_CHARACTERISTIC,
        PRESET_RING_NILPOTENT_TEST,
        PRESET_RING_IDEMPOTENT_TEST,
        PRESET_RING_UNIT_GROUP,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv_free(&tmp);
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
}
