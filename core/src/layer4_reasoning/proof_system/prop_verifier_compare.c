/*
 * @file prop_verifier_compare.c
 * @brief Proposition verifier module - formula comparison
 * @details Split from prop_verifier.c
 */

#include "lv/prop_verifier.h"
#include "lv/prop_formula_ops.h"
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
    const PropFormulaOps *ops = prop_formula_get_ops(a->type);
    if (ops && ops->equal)
        return ops->equal(a, b);
    return false;
}

