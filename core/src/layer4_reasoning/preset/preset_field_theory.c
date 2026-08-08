/**
 * @file preset_field_theory.c
 * @brief 域论预设函数块模块 - 实现（v2统一宏模式）
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/field_theory.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的域论运算预设函数块。
 * 涵盖域基础运算、域扩张及伽罗瓦理论。
 * 共30个预设函数块，均遵循模块化、确定性原则。
 *
 * @module FieldTheory
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 5.0.0
 */

#include "preset_field_theory.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 域论模块预设函数块总数：30（与头文件中 FIELD_THEORY_PRESET_COUNT 一致） */


/**
 * @brief 获取域论预设函数块数量
 *
 * @return int 域论模块预设函数块总数
 */
int preset_field_theory_count(void) {
    return FIELD_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取域论预设函数块名称列表
 *
 * @param out_names 输出名称数组指针（调用者负责释放）
 * @param out_count 输出预设数量
 * @return true 获取成功
 * @return false 获取失败
 */
bool preset_field_theory_get_names(char ***out_names, int *out_count) {
    if (out_names == NULL || out_count == NULL) {
        return false;
    }

    *out_count = FIELD_THEORY_PRESET_COUNT;

    /* 分配名称数组 */
    char **names = (char **) lv_malloc(FIELD_THEORY_PRESET_COUNT * sizeof(char *));
    if (names == NULL) {
        return false;
    }

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 域基础运算 */
        PRESET_FIELD_ADD,
        PRESET_FIELD_MULTIPLY,
        PRESET_FIELD_INVERSE,
        PRESET_FIELD_DIVIDE,
        PRESET_FIELD_CHARACTERISTIC,
        PRESET_FIELD_SUBFIELD_CHECK,
        PRESET_FIELD_EXTENSION_CHECK,
        PRESET_FIELD_PRIME_SUBFIELD,
        /* 域扩张 */
        PRESET_EXTENSION_DEGREE,
        PRESET_SIMPLE_EXTENSION,
        PRESET_ALGEBRAIC_EXTENSION,
        PRESET_TRANSCENDENTAL_EXTENSION,
        PRESET_FINITE_EXTENSION,
        PRESET_ALGEBRAIC_ELEMENT_CHECK,
        PRESET_MINIMAL_POLYNOMIAL,
        PRESET_FIELD_TOWER,
        PRESET_PRIMITIVE_ELEMENT,
        PRESET_NORMAL_EXTENSION,
        /* 伽罗瓦理论 */
        PRESET_GALOIS_GROUP,
        PRESET_GALOIS_GROUP_ORDER,
        PRESET_FIXED_FIELD,
        PRESET_GALOIS_CORRESPONDENCE,
        PRESET_GALOIS_CHECK,
        PRESET_SEPARABLE_EXTENSION,
        PRESET_SPLITTING_FIELD,
        PRESET_CYCLOTOMIC_FIELD,
        PRESET_FINITE_FIELD_CONSTRUCT,
        PRESET_FROBENIUS_AUTOMORPHISM,
        PRESET_FIELD_EMBEDDING,
        PRESET_ALGEBRAIC_CLOSURE,
    };

    for (int i = 0; i < FIELD_THEORY_PRESET_COUNT; i++) {
        names[i] = lv_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 分配失败时释放已分配的内存 */
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
    return true;
}
