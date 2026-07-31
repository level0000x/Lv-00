/*
 * @file prop_verifier_compare.c
 * @brief Proposition verifier module - formula comparison
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
 * 公式比较（用于记忆化和前提匹配）
 * ============================================================ */

/**
 * @brief 公式结构深度比较（递归）
 *
 * 递归比较两个命题公式的结构相等性：
 * - ATOM：比较名称字符串
 * - 二元运算符（CONJ/DISJ/IMPL）：递归比较左右子公式
 * - 一元运算符（NEG）：递归比较操作数
 * - BOTTOM/TRUE：类型匹配即相等
 *
 * @param a 第一个公式指针（可为 NULL）
 * @param b 第二个公式指针（可为 NULL）
 * @return true 表示结构相等，false 表示不同
 */
bool formula_equal(const PropFormula *a, const PropFormula *b) {
    if (!a || !b)
        return a == b;
    if (a->type != b->type)
        return false;
    switch (a->type) {
        case PROP_ATOM:
            return strcmp(a->data.atom.name, b->data.atom.name) == 0;
        case PROP_CONJUNCTION:
        case PROP_DISJUNCTION:
        case PROP_IMPLICATION:
            return formula_equal(a->data.binary.left, b->data.binary.left) &&
                   formula_equal(a->data.binary.right, b->data.binary.right);
        case PROP_NEGATION:
            return formula_equal(a->data.unary.operand, b->data.unary.operand);
        case PROP_BOTTOM:
        case PROP_TRUE:
            return true;
        default:
            break;
    }
    return false;
}

