/**
 * @file preset_coding_theory.c
 * @brief 编码理论预设函数块 - 实现
 *
 * 实现编码理论领域的预设函数块注册。
 * 涵盖线性码、Hamming码、Reed-Solomon码、循环码等。
 *
 * @module CodingTheory
 * @category PRESET_EXT_ALGEBRA_ADVANCED
 */

#include "preset_coding_theory.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 编码理论模块预设函数块总数 */
/* 已在头文件中定义 CODING_THEORY_PRESET_COUNT = 10 */

/* ==================== 模块注册实现 ==================== */

bool preset_coding_theory_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：线性码
     * ============================================================ */

    /* 线性码构造 */
    if (preset_blocks_register_by_category(
            "linear_code_construct",
            "由生成矩阵G构造 [n,k] 线性码 C",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* 校验矩阵 */
    if (preset_blocks_register_by_category(
            "parity_check_matrix",
            "由生成矩阵计算校验矩阵 H（GH^T = 0）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1)) {
        success_count++;
    }

    /* 最小距离 */
    if (preset_blocks_register_by_category(
            "minimum_distance",
            "计算线性码的最小汉明距离 d(C)",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1)) {
        success_count++;
    }

    /* 编码操作 */
    if (preset_blocks_register_by_category(
            "encode_message",
            "将信息字编码为码字 c = mG",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第二部分：Hamming码
     * ============================================================ */

    /* Hamming码构造 */
    if (preset_blocks_register_by_category(
            "hamming_code_construct",
            "构造 [2^r-1, 2^r-1-r] Hamming码",
            PRESET_EXT_ALGEBRA_ADVANCED,
            1, 1)) {
        success_count++;
    }

    /* Hamming码译码 */
    if (preset_blocks_register_by_category(
            "hamming_decode",
            "Hamming码伴随式译码（纠正单比特错误）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第三部分：Reed-Solomon码
     * ============================================================ */

    /* RS码构造 */
    if (preset_blocks_register_by_category(
            "reed_solomon_construct",
            "构造 [n, k, n-k+1] Reed-Solomon码（最大距离可分码）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1)) {
        success_count++;
    }

    /* RS码编码 */
    if (preset_blocks_register_by_category(
            "reed_solomon_encode",
            "Reed-Solomon编码（多项式求值）",
            PRESET_EXT_ALGEBRA_ADVANCED,
            3, 1)) {
        success_count++;
    }

    /* ============================================================
     * 第四部分：循环码
     * ============================================================ */

    /* 循环码构造 */
    if (preset_blocks_register_by_category(
            "cyclic_code_construct",
            "由生成多项式g(x)构造循环码",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* CRC校验 */
    if (preset_blocks_register_by_category(
            "crc_check",
            "循环冗余校验（CRC）检测传输错误",
            PRESET_EXT_ALGEBRA_ADVANCED,
            2, 1)) {
        success_count++;
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == CODING_THEORY_PRESET_COUNT;
}

int preset_coding_theory_count(void)
{
    return CODING_THEORY_PRESET_COUNT;
}
