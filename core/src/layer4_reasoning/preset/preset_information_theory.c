/**
 * @file preset_information_theory.c
 * @brief 信息论预设函数块 - 实现
 *
 * 实现信息论领域的预设函数块注册。
 * 涵盖信息熵、互信息、信道容量、率失真理论及数据压缩。
 *
 * @module InformationTheory
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "preset_information_theory.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 信息论模块预设函数块总数 */
/* 已在头文件中定义 INFORMATION_THEORY_PRESET_COUNT = 10 */

/* ==================== 模块注册实现 ==================== */

int preset_information_theory_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：信息熵
     * ============================================================ */

    /* Shannon熵 */
    if (preset_blocks_register_by_category(
            "shannon_entropy",
            "计算离散随机变量的Shannon熵 H(X) = -Σ p(x) log p(x)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1) == 0) {
        success_count++;
    }

    /* 条件熵 */
    if (preset_blocks_register_by_category(
            "conditional_entropy",
            "计算条件熵 H(Y|X) = -Σ p(x,y) log p(y|x)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 联合熵 */
    if (preset_blocks_register_by_category(
            "joint_entropy",
            "计算联合熵 H(X,Y) = -Σ p(x,y) log p(x,y)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：互信息与信道
     * ============================================================ */

    /* 互信息 */
    if (preset_blocks_register_by_category(
            "mutual_information",
            "计算互信息 I(X;Y) = H(X) - H(X|Y)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 信道容量 */
    if (preset_blocks_register_by_category(
            "channel_capacity",
            "计算离散无记忆信道的信道容量 C = max I(X;Y)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：编码定理
     * ============================================================ */

    /* 香农第一定理（无噪声编码） */
    if (preset_blocks_register_by_category(
            "shannon_source_coding",
            "香农源编码定理：最优码长 l* >= H(X)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* 香农第二定理（信道编码） */
    if (preset_blocks_register_by_category(
            "shannon_channel_coding",
            "香农信道编码定理：可靠传输率 R <= C",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：率失真与数据压缩
     * ============================================================ */

    /* 率失真函数 */
    if (preset_blocks_register_by_category(
            "rate_distortion",
            "计算率失真函数 R(D) = min I(X;X') subject to E[d(X,X')] <= D",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1) == 0) {
        success_count++;
    }

    /* KL散度 */
    if (preset_blocks_register_by_category(
            "kl_divergence",
            "计算Kullback-Leibler散度 D_KL(P||Q) = Σ p(x) log(p(x)/q(x))",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1) == 0) {
        success_count++;
    }

    /* Huffman编码 */
    if (preset_blocks_register_by_category(
            "huffman_coding",
            "构造最优Huffman编码（最小冗余编码）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1) == 0) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == INFORMATION_THEORY_PRESET_COUNT;
}

int preset_information_theory_count(void)
{
    return INFORMATION_THEORY_PRESET_COUNT;
}
