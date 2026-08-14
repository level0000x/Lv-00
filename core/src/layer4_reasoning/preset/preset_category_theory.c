/**
 * @file preset_category_theory.c
 * @brief 范畴论预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/category_theory.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的范畴论运算预设函数块。
 * 涵盖范畴基本概念（恒等态射、复合、同构）、函子作用、
 * 自然变换、极限与余极限（积、余积、拉回、推出、等化子、余等化子）、
 * 泛性质（指数对象、初始对象、终止对象、伴随函子）。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module CategoryTheory
 * @category PRESET_CATEGORY_CATEGORY_THEORY
 * @version 4.0.0
 */

#include "lv/preset_category_theory.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 范畴论模块预设函数块总数：15（与头文件中 CATEGORY_THEORY_PRESET_COUNT 一致） */


/**
 * @brief 获取范畴论预设函数块的类别
 * @return 预设类别枚举值
 */
PresetCategory preset_category_theory_category(void) {
    return PRESET_CATEGORY_CATEGORY_THEORY;
}

/**
 * @brief 获取范畴论预设函数块的名称列表
 * @param out_names 输出：名称数组（调用者需先释放每个元素再释放数组本身）
 * @param out_count 输出：名称数量
 * @return true 成功，false 内存分配失败
 */
bool preset_category_theory_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        PRESET_CAT_IDENTITY_MORPHISM,
        PRESET_CAT_COMPOSITION,
        PRESET_CAT_ISOMORPHISM_TEST,
        PRESET_CAT_FUNCTOR_APPLY,
        PRESET_CAT_NATURAL_TRANSFORMATION,
        PRESET_CAT_PRODUCT,
        PRESET_CAT_COPRODUCT,
        PRESET_CAT_PULLBACK,
        PRESET_CAT_PUSHOUT,
        PRESET_CAT_EQUALIZER,
        PRESET_CAT_COEQUALIZER,
        PRESET_CAT_EXPONENTIAL,
        PRESET_CAT_INITIAL_OBJECT,
        PRESET_CAT_TERMINAL_OBJECT,
        PRESET_CAT_ADJOINT,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
