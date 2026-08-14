/**
 * @file preset_combinatorics.c
 * @brief 组合数学预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/combinatorics.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的组合数学预设函数块。
 * 涵盖排列组合、生成函数、图论基础及计数方法。
 *
 * @module Combinatorics
 * @category PRESET_CATEGORY_COMBINATORICS
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "lv/preset_combinatorics.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 组合数学模块预设函数块总数：20（与头文件中 COMBINATORICS_PRESET_COUNT 一致） */


int preset_combinatorics_count(void) {
    return COMBINATORICS_PRESET_COUNT;
}

PresetCategory preset_combinatorics_category(void) {
    return PRESET_CATEGORY_COMBINATORICS;
}

bool preset_combinatorics_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 排列组合 */
        PRESET_COMB_PERMUTATION,
        PRESET_COMB_COMBINATION,
        PRESET_COMB_MULTISET_PERMUTATION,
        PRESET_COMB_MULTISET_COMBINATION,
        PRESET_COMB_STIRLING_FIRST,
        PRESET_COMB_STIRLING_SECOND,
        /* 生成函数 */
        PRESET_COMB_OGF,
        PRESET_COMB_EGF,
        PRESET_COMB_COEFFICIENT_EXTRACT,
        PRESET_COMB_COMPOSITION,
        /* 图论基础 */
        PRESET_COMB_GRAPH_CREATE,
        PRESET_COMB_GRAPH_ADD_EDGE,
        PRESET_COMB_GRAPH_DEGREE,
        PRESET_COMB_GRAPH_IS_CONNECTED,
        PRESET_COMB_GRAPH_IS_TREE,
        /* 计数方法 */
        PRESET_COMB_INCLUSION_EXCLUSION,
        PRESET_COMB_PIGEONHOLE,
        PRESET_COMB_RAMSEY_NUMBER,
        PRESET_COMB_CATALAN_NUMBER,
        PRESET_COMB_PARTITION_NUMBER,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
