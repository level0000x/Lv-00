/**
 * @file preset_representation_theory.c
 * @brief 表示论预设函数块 - 实现
 *
 * 实现理论数学研究中常用的表示论运算预设函数块。
 * 涵盖群表示、特征标理论、不可约表示、诱导表示和李代数表示五大领域。
 * 共18个预设函数块，均遵循模块化、确定性原则。
 *
 * 采用统一的 preset_blocks_register_simple 注册接口。
 *
 * @module RepresentationTheory
 * @category PRESET_CATEGORY_GROUP_THEORY
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "lv/preset_representation_theory.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 表示论模块预设函数块总数（与头文件中 REPRESENTATION_THEORY_PRESET_COUNT 一致） */
#define RT_PRESET_COUNT REPRESENTATION_THEORY_PRESET_COUNT

/**
 * @brief 获取表示论预设函数块数量
 *
 * @return int 表示论模块预设函数块总数（18）
 */
int preset_representation_theory_count(void) {
    return RT_PRESET_COUNT;
}

/**
 * @brief 获取表示论预设的类别
 *
 * @return PresetCategory 预设类别（PRESET_CATEGORY_GROUP_THEORY）
 */
PresetCategory preset_representation_theory_category(void) {
    return PRESET_CATEGORY_GROUP_THEORY;
}

/**
 * @brief 获取表示论预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_representation_theory_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 群表示 */
        PRESET_RT_LINEAR_REPRESENTATION,
        PRESET_RT_PERMUTATION_REP,
        PRESET_RT_REGULAR_REPRESENTATION,
        PRESET_RT_UNITARY_REPRESENTATION,
        PRESET_RT_REPRESENTATION_DIMENSION,
        PRESET_RT_REP_DIRECT_SUM_TENSOR,
        /* 特征标理论 */
        PRESET_RT_CHARACTER_CALCULATION,
        PRESET_RT_CHARACTER_TABLE,
        PRESET_RT_CHARACTER_INNER_PRODUCT,
        PRESET_RT_CHARACTER_ORTHOGONALITY,
        /* 不可约表示 */
        PRESET_RT_IRREDUCIBILITY_TEST,
        PRESET_RT_MASCHKE_THEOREM,
        PRESET_RT_IRREDUCIBLE_DECOMPOSITION,
        PRESET_RT_SCHURS_LEMMA,
        /* 诱导表示 */
        PRESET_RT_FROBENIUS_RECIPROCITY,
        PRESET_RT_INDUCED_CHARACTER,
        /* 李代数表示 */
        PRESET_RT_ADJOINT_REPRESENTATION,
        PRESET_RT_HIGHEST_WEIGHT_REP,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
