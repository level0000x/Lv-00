/*
 * @file prop_verifier_hash.c
 * @brief Proposition verifier module - hashing
 * @details Split from prop_verifier.c
 */

#include "lv/prop_verifier.h"
#include "prop_verifier_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/stream.h"
#include "lv/stream_context_util.h"

/* ============================================================
 * 哈希函数（用于记忆化）
 * ============================================================ */

/**
 * @brief 简单的指针哈希函数
 *
 * 使用指针地址生成 64 位哈希值，通过位移和乘法混合。
 *
 * @param p 待哈希的指针
 * @return 64 位哈希值
 */
/**
 * @brief 计算公式结构的哈希值（递归）
 *
 * 对公式结构生成 64 位哈希值：
 * - ATOM：基于名称字符串的字符串哈希
 * - 二元运算符：递归哈希左右子公式
 * - 一元运算符：递归哈希操作数
 * - BOTTOM/TRUE：仅类型哈希
 * 使用黄金比例乘数区分不同类型以避免冲突。
 *
 * @param f 公式指针（可为 NULL）
 * @return 64 位哈希值，NULL 公式返回 0
 */
uint64_t formula_hash(const PropFormula *f) {
    if (!f)
        return 0;
    uint64_t h = (uint64_t) f->type * PROP_HASH_TYPE_MULTIPLIER;
    switch (f->type) {
        case PROP_ATOM: {
            for (const char *s = f->data.atom.name; *s; s++)
                h = h * PROP_HASH_STRING_MULTIPLIER + (uint64_t) (unsigned char) *s;
            break;
        }
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            h ^= formula_hash(f->data.binary.left) * PROP_HASH_LEFT_MULTIPLIER;
            h ^= formula_hash(f->data.binary.right) * PROP_HASH_RIGHT_MULTIPLIER;
            break;
        case PROP_NEGATION:
            h ^= formula_hash(f->data.unary.operand) * PROP_HASH_RIGHT_MULTIPLIER;
            break;
        case PROP_BOTTOM:
        case PROP_TRUE:
            break;
        default:
            break;
    }
    return h;
}

/**
 * @brief 计算前提集合的哈希值
 *
 * 对每个前提公式的哈希值进行 64 位聚合哈希。
 *
 * @param premises 前提公式数组
 * @param count    前提数量
 * @return 64 位哈希值
 */
uint64_t premises_hash(const PropFormula **premises, int count) {
    uint64_t h = 0;
    for (int i = 0; i < count; i++) {
        h = h * PROP_HASH_PREMISES_MULTIPLIER + formula_hash(premises[i]);
    }
    return h;
}

