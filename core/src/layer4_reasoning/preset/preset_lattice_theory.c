/**
 * @file preset_lattice_theory.c
 * @brief 格论预设函数块模块 - 实现
 *
 * 实现理论数学研究中常用的格论运算预设函数块。
 * 涵盖格基础运算、特殊格、格同态与表示、格与序。
 * 共30个预设函数块，均遵循模块化、确定性原则。
 *
 * @module LatticeTheory
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 5.0.0
 */

#include "preset_lattice_theory.h"

#include <stdlib.h>
#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 格论模块预设函数块总数 */

/**
 * @brief 获取格论预设函数块数量
 *
 * @return int 格论模块预设函数块总数
 */
int preset_lattice_theory_count(void) {
    return LATTICE_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取格论预设函数块名称列表
 *
 * @param out_names 输出名称数组指针（调用者负责释放）
 * @param out_count 输出预设数量
 * @return true 获取成功
 * @return false 获取失败
 */
bool preset_lattice_theory_get_names(char ***out_names, int *out_count) {
    if (out_names == NULL || out_count == NULL) {
        return false;
    }

    *out_count = LATTICE_THEORY_PRESET_COUNT;

    /* 分配名称数组 */
    char **names = (char **) lv_malloc(LATTICE_THEORY_PRESET_COUNT * sizeof(char *));
    if (names == NULL) {
        return false;
    }

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 格基础运算 */
        "lattice_join", "lattice_meet", "lattice_top", "lattice_bottom", "lattice_complement", "lattice_partial_order",
        "lattice_check", "lattice_bounded_check", "lattice_distributive_check", "lattice_modular_check",
        /* 特殊格 */
        "boolean_algebra_check", "boolean_algebra_operations", "heyting_algebra_check", "heyting_implication",
        "complete_lattice_check", "complete_lattice_sup", "complete_lattice_inf", "lattice_ideal",
        /* 格同态与表示 */
        "lattice_homomorphism", "lattice_embedding", "lattice_isomorphism_check", "lattice_sublattice_check",
        "lattice_product", "lattice_duality", "stone_representation",
        /* 格与序 */
        "hasse_diagram", "chain_check", "antichain_check", "lattice_height", "lattice_width"};

    for (int i = 0; i < LATTICE_THEORY_PRESET_COUNT; i++) {
        size_t len = strlen(preset_names[i]) + 1;
        names[i] = (char *) lv_malloc(len);
        if (names[i] == NULL) {
            /* 分配失败时释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                lv_free((void **) &names[j]);
            }
            lv_free((void **) &names);
            return false;
        }
        memcpy(names[i], preset_names[i], len);
    }

    *out_names = names;
    return true;
}

/**
 * @brief 获取格论模块类别名称
 *
 * @return 类别名称字符串
 */
const char *preset_lattice_theory_category(void) {
    return "格论";
}
