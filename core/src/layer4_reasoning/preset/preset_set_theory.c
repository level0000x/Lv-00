/**
 * @file preset_set_theory.c
 * @brief 集合论预设函数块模块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/set_theory.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究项目Lv-00中集合论领域的预设函数块。
 * 采用v2统一宏模式，使用 REGISTER_SET 宏简化注册流程。
 *
 * 模块包含35个预设，分为五大类别：
 *   - 集合基本运算（10个）：并集、交集、差集、补集、对称差、
 *     笛卡尔积、幂集、子集判定、集合相等判定、空集判定
 *   - 关系与函数（8个）：关系复合、逆关系、自反性判定、
 *     对称性判定、传递性判定、等价关系判定、等价类、商集
 *   - 映射理论（8个）：函数复合、逆函数判定、单射判定、
 *     满射判定、双射判定、像、原像、不动点
 *   - 序理论（5个）：偏序关系判定、最小元、最大元、上确界、下确界
 *   - 公理集合论（4个）：ZFC配对公理、ZFC并集公理、
 *     ZFC幂集公理、ZFC替换公理
 *
 * @module SetTheory
 * @category PRESET_CATEGORY_SET_THEORY
 * @version 2.0.0
 * @author Lv-00 开发团队
 */

#include "lv/preset_set_theory.h"

#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/preset_blocks.h"
#include "lv/preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 集合论模块预设函数块总数：35（与头文件中 SET_THEORY_PRESET_COUNT 一致） */


/**
 * @brief 获取集合论预设函数块的类别
 * @return 预设类别枚举值
 */
PresetCategory preset_set_theory_category(void) {
    return PRESET_CATEGORY_LOGIC;
}

/**
 * @brief 获取集合论预设函数块的名称列表
 * @param out_names 输出：名称数组（调用者需先释放每个元素再释放数组本身）
 * @param out_count 输出：名称数量
 * @return true 成功，false 内存分配失败
 */
bool preset_set_theory_get_names(char ***out_names, int *out_count) {
    static const char *const preset_names[] = {
        /* 集合基本运算 */
        PRESET_SET_UNION,
        PRESET_SET_INTERSECTION,
        PRESET_SET_DIFFERENCE,
        PRESET_SET_COMPLEMENT,
        PRESET_SET_SYMMETRIC_DIFFERENCE,
        PRESET_SET_CARTESIAN_PRODUCT,
        PRESET_SET_POWER_SET,
        PRESET_SET_SUBSET_CHECK,
        PRESET_SET_EQUALITY_CHECK,
        PRESET_SET_EMPTY_CHECK,
        /* 关系与函数 */
        PRESET_RELATION_COMPOSE,
        PRESET_RELATION_INVERSE,
        PRESET_RELATION_REFLEXIVE_CHECK,
        PRESET_RELATION_SYMMETRIC_CHECK,
        PRESET_RELATION_TRANSITIVE_CHECK,
        PRESET_RELATION_EQUIVALENCE_CHECK,
        PRESET_EQUIVALENCE_CLASS,
        PRESET_QUOTIENT_SET,
        /* 映射理论 */
        PRESET_FUNCTION_COMPOSE,
        PRESET_FUNCTION_INVERSE_CHECK,
        PRESET_FUNCTION_INJECTIVE_CHECK,
        PRESET_FUNCTION_SURJECTIVE_CHECK,
        PRESET_FUNCTION_BIJECTIVE_CHECK,
        PRESET_FUNCTION_IMAGE,
        PRESET_FUNCTION_PREIMAGE,
        PRESET_FUNCTION_FIXPOINT,
        /* 序理论 */
        PRESET_ORDER_CHECK,
        PRESET_ORDER_MIN,
        PRESET_ORDER_MAX,
        PRESET_ORDER_SUPREMUM,
        PRESET_ORDER_INFIMUM,
        /* 公理集合论 */
        PRESET_ZFC_PAIRING,
        PRESET_ZFC_UNION,
        PRESET_ZFC_POWER_SET,
        PRESET_ZFC_REPLACEMENT,
    };

    return preset_module_get_names(preset_names,
        (int) (sizeof(preset_names) / sizeof(preset_names[0])), out_names, out_count);
}
