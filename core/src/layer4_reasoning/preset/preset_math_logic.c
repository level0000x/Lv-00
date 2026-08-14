/**
 * @file preset_math_logic.c
 * @brief 数理逻辑预设函数块 - 实现
 *
 * 实现理论数学研究中常用的数理逻辑运算预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module MathLogic
 * @category PRESET_CATEGORY_MATH_LOGIC
 * @version 4.0.0
 */

#include "lv/preset_math_logic.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* 内部别名：与 preset_math_logic.h 中 MATH_LOGIC_PRESET_COUNT 一致 */
#define ADVANCED_MATH_LOGIC_PRESET_COUNT MATH_LOGIC_PRESET_COUNT

/* ==================== 预设函数块数量 ==================== */

/** 数理逻辑模块预设函数块总数 */


/**
 * @brief 获取数理逻辑预设函数块的类别
 * @return 预设类别枚举值
 */
PresetCategory preset_math_logic_category(void) {
    return PRESET_CATEGORY_LOGIC;
}

/**
 * @brief 获取数理逻辑预设函数块的名称列表
 * @param out_names 输出：名称数组（调用者需先释放每个元素再释放数组本身）
 * @param out_count 输出：名称数量
 * @return true 成功，false 内存分配失败
 */
bool preset_math_logic_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 命题逻辑 */
        PRESET_LOGIC_CONJUNCTION,
        PRESET_LOGIC_DISJUNCTION,
        PRESET_LOGIC_NEGATION,
        PRESET_LOGIC_IMPLICATION,
        PRESET_LOGIC_BICONDITIONAL,
        PRESET_LOGIC_TAUTOLOGY_TEST,
        PRESET_LOGIC_SATISFIABILITY_TEST,
        /* 一阶逻辑 */
        PRESET_LOGIC_UNIVERSAL_INSTANTIATION,
        PRESET_LOGIC_EXISTENTIAL_GENERALIZATION,
        PRESET_LOGIC_UNIVERSAL_GENERALIZATION,
        PRESET_LOGIC_EXISTENTIAL_INSTANTIATION,
        /* 证明系统 */
        PRESET_LOGIC_NATURAL_DEDUCTION,
        PRESET_LOGIC_RESOLUTION,
        PRESET_LOGIC_TABLEAU_METHOD,
        /* 模型论 */
        PRESET_LOGIC_MODEL_CHECK,
        PRESET_LOGIC_VALIDITY_TEST,
        PRESET_LOGIC_MODEL_SATISFIABILITY,
        /* 递归论 */
        PRESET_LOGIC_TURING_MACHINE_CHECK,
        PRESET_LOGIC_RECURSIVE_CHECK,
        PRESET_LOGIC_HALTING_PROBLEM,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}

int preset_math_logic_count(void) {
    return ADVANCED_MATH_LOGIC_PRESET_COUNT;
}
