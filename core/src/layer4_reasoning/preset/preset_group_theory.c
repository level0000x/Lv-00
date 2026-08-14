/**
 * @file preset_group_theory.c
 * @brief 群论预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/group_theory.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的群论运算预设函数块。
 * 涵盖群基础运算、子群理论、同态同构、特殊群及群结构分析。
 *
 * @module GroupTheory
 * @category PRESET_CATEGORY_GROUP_THEORY
 * @version 5.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_group_theory.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_GROUP_THEORY 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv_internal.h / lv_utils.h
 *   -> 提供 lv_malloc、lv_free、lv_strdup、lv_log_* 等
 * ============================================================
 */
#include "lv/preset_group_theory.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h" /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等） */

/* ==================== 预设函数块数量 ==================== */

/** 群论模块预设函数块总数：38（与头文件中 GROUP_THEORY_PRESET_COUNT 一致） */


/**
 * @brief 获取群论预设函数块数量
 *
 * @return int 群论模块预设函数块总数
 */
int preset_group_theory_count(void) {
    return GROUP_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取群论预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_group_theory_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        PRESET_GROUP_OPERATION,
        PRESET_GROUP_INVERSE,
        PRESET_GROUP_POWER,
        PRESET_GROUP_IDENTITY_TEST,
        PRESET_ELEMENT_ORDER,
        PRESET_SUBGROUP_TEST,
        PRESET_GENERATED_SUBGROUP,
        PRESET_LEFT_COSET,
        PRESET_RIGHT_COSET,
        PRESET_COSET_DECOMPOSITION,
        PRESET_NORMAL_SUBGROUP_TEST,
        PRESET_QUOTIENT_GROUP,
        PRESET_GROUP_HOMOMORPHISM_TEST,
        PRESET_HOMOMORPHISM_KERNEL,
        PRESET_HOMOMORPHISM_IMAGE,
        PRESET_GROUP_ISOMORPHISM_TEST,
        PRESET_AUTOMORPHISM_GROUP,
        PRESET_INNER_AUTOMORPHISM,
        PRESET_CYCLIC_GROUP_TEST,
        PRESET_CYCLIC_GENERATORS,
        PRESET_ABELIAN_GROUP_TEST,
        PRESET_PERMUTATION_MULTIPLY,
        PRESET_PERMUTATION_DECOMPOSE,
        PRESET_SYMMETRIC_GROUP,
        PRESET_ALTERNATING_GROUP,
        PRESET_DIHEDRAL_GROUP,
        PRESET_QUATERNION_GROUP,
        PRESET_GROUP_ORDER,
        PRESET_CONJUGACY_CLASS,
        PRESET_CLASS_EQUATION,
        PRESET_GROUP_CENTER,
        PRESET_COMMUTATOR_SUBGROUP,
        PRESET_DERIVED_SERIES,
        PRESET_SOLVABLE_GROUP_TEST,
        PRESET_NILPOTENT_GROUP_TEST,
        PRESET_SYLOW_P_SUBGROUP,
        PRESET_SYLOW_SUBGROUP_COUNT,
        PRESET_LOWER_CENTRAL_SERIES,
        PRESET_UPPER_CENTRAL_SERIES,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}

/**
 * @brief 获取群论预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_group_theory_category(void) {
    return PRESET_CATEGORY_GROUP_THEORY;
}

/**
 * @brief 获取群论模块所有预设名称列表
 * @return 以 NULL 结尾的名称数组，调用者需使用 lv_free 释放
 */
static __attribute__((unused)) char **get_group_theory_names(void) {
    static const char *names[] = {
        PRESET_GROUP_OPERATION,
        PRESET_GROUP_INVERSE,
        PRESET_GROUP_POWER,
        PRESET_GROUP_IDENTITY_TEST,
        PRESET_ELEMENT_ORDER,
        PRESET_SUBGROUP_TEST,
        PRESET_GENERATED_SUBGROUP,
        PRESET_LEFT_COSET,
        PRESET_RIGHT_COSET,
        PRESET_COSET_DECOMPOSITION,
        PRESET_NORMAL_SUBGROUP_TEST,
        PRESET_QUOTIENT_GROUP,
        PRESET_GROUP_HOMOMORPHISM_TEST,
        PRESET_HOMOMORPHISM_KERNEL,
        PRESET_HOMOMORPHISM_IMAGE,
        PRESET_GROUP_ISOMORPHISM_TEST,
        PRESET_AUTOMORPHISM_GROUP,
        PRESET_INNER_AUTOMORPHISM,
        PRESET_CYCLIC_GROUP_TEST,
        PRESET_CYCLIC_GENERATORS,
        PRESET_ABELIAN_GROUP_TEST,
        PRESET_PERMUTATION_MULTIPLY,
        PRESET_PERMUTATION_DECOMPOSE,
        PRESET_SYMMETRIC_GROUP,
        PRESET_ALTERNATING_GROUP,
        PRESET_DIHEDRAL_GROUP,
        PRESET_QUATERNION_GROUP,
        PRESET_GROUP_ORDER,
        PRESET_CONJUGACY_CLASS,
        PRESET_CLASS_EQUATION,
        PRESET_GROUP_CENTER,
        PRESET_COMMUTATOR_SUBGROUP,
        PRESET_DERIVED_SERIES,
        PRESET_SOLVABLE_GROUP_TEST,
        PRESET_NILPOTENT_GROUP_TEST,
        PRESET_SYLOW_P_SUBGROUP,
        PRESET_SYLOW_SUBGROUP_COUNT,
        PRESET_LOWER_CENTRAL_SERIES,
        PRESET_UPPER_CENTRAL_SERIES,
    };
    const int count = sizeof(names) / sizeof(names[0]);
    char **result = (char **) lv_malloc((count + 1) * sizeof(char *));
    if (!result)
        return NULL;
    for (int i = 0; i < count; i++) {
        result[i] = lv_strdup(names[i]);
        if (!result[i]) {
            for (int j = 0; j < i; j++) {
                void *tmp = result[j];
                lv_free(&tmp);
            }
            {
                void *tmp = result;
                lv_free(&tmp);
            }
            return NULL;
        }
    }
    result[count] = NULL;
    return result;
}
