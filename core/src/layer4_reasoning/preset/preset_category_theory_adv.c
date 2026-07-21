/**
 * @file preset_category_theory_adv.c
 * @brief 范畴论进阶预设函数块 - 实现
 *
 * 实现范畴论进阶领域的预设函数块注册。
 * 涵盖伴随函子、极限与余极限、Kan扩张、Yoneda引理等。
 *
 * @module CategoryTheoryAdv
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "preset_category_theory_adv.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 范畴论进阶模块预设函数块总数 */
/* 已在头文件中定义 CATEGORY_THEORY_ADV_PRESET_COUNT = 8 */

/* ==================== 模块注册实现 ==================== */

int preset_category_theory_adv_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：伴随函子
     * ============================================================ */

    /* 伴随函子对构造 */
    if (preset_blocks_register_by_category(
            "adjunction_construct",
            "构造伴随函子对 F ⊣ G（F左伴随G）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 伴随判定 */
    if (preset_blocks_register_by_category(
            "adjunction_test",
            "判定函子对是否构成伴随关系",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 单位与余单位 */
    if (preset_blocks_register_by_category(
            "adjunction_unit_counit",
            "从伴随函子对提取单位 η: Id → GF 和余单位 ε: FG → Id",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：极限与余极限
     * ============================================================ */

    /* 极限计算 */
    if (preset_blocks_register_by_category(
            "limit_compute",
            "计算图表的极限（锥的终对象）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 余极限计算 */
    if (preset_blocks_register_by_category(
            "colimit_compute",
            "计算图表的余极限（余锥的始对象）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：Kan扩张
     * ============================================================ */

    /* 左Kan扩张 */
    if (preset_blocks_register_by_category(
            "left_kan_extension",
            "计算函子沿另一函子的左Kan扩张 Lan_K F",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* 右Kan扩张 */
    if (preset_blocks_register_by_category(
            "right_kan_extension",
            "计算函子沿另一函子的右Kan扩张 Ran_K F",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：Yoneda引理
     * ============================================================ */

    /* Yoneda嵌入 */
    if (preset_blocks_register_by_category(
            "yoneda_embedding",
            "Yoneda嵌入 C → [C^op, Set] 将对象映到Hom(-, A)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == CATEGORY_THEORY_ADV_PRESET_COUNT;
}

int preset_category_theory_adv_count(void)
{
    return CATEGORY_THEORY_ADV_PRESET_COUNT;
}
