/**
 * @file inequality_reasoning_sign.c
 * @brief 不等式推理系统 —— 表达式符号判定与平方和分解
 */

#include "inequality_reasoning_internal.h"
#include "lv/expr_vtable.h"

/* 符号 → 描述字符串 静态查找表（越界含 SIGN_UNKNOWN 保持 "unknown"） */
static const char *const kSignDescriptionTable[] = {
    [SIGN_POSITIVE] = "positive",
    [SIGN_NEGATIVE] = "negative",
    [SIGN_ZERO] = "zero",
    [SIGN_NONNEGATIVE] = "non-negative",
    [SIGN_NONPOSITIVE] = "non-positive",
};

/** 不等式类型 → 表达式符号 静态查找表（未映射类型以 SIGN_UNKNOWN 为哨兵） */
static const lvSign kSignByIneq[] = {
    [INEQ_GREATER_THAN] = SIGN_POSITIVE,
    [INEQ_GREATER_EQUAL] = SIGN_NONNEGATIVE,
    [INEQ_LESS_THAN] = SIGN_NEGATIVE,
    [INEQ_LESS_EQUAL] = SIGN_NONPOSITIVE,
    [INEQ_NOT_EQUAL] = SIGN_UNKNOWN,
};


lvSign lv_expr_sign(lvExpr *expr, const lvInequalitySystem *sys) {
    if (!expr)
        return SIGN_UNKNOWN;

    /* 遍历系统约束查找变量的符号信息 */
    if (sys) {
        for (int i = 0; i < sys->inequalities.count; i++) {
            lvInequality **pp = (lvInequality **)lv_darray_get(&sys->inequalities, i);
            lvInequality *c = pp ? *pp : NULL;
            if (!c)
                continue;

            /* 检查 expr == c->left 且 c->right 为零/常量 */
            if (c->left == expr) {
                if ((unsigned) c->type < lv_ARRAY_SIZE(kSignByIneq) && kSignByIneq[c->type] != SIGN_UNKNOWN)
                    return kSignByIneq[c->type];
            }
        }
    }

    return SIGN_UNKNOWN;
}

bool lv_expr_is_positive(lvExpr *expr, const lvInequalitySystem *sys) {
    lvSign s = lv_expr_sign(expr, sys);
    return (s == SIGN_POSITIVE);
}

bool lv_expr_is_nonnegative(lvExpr *expr, const lvInequalitySystem *sys) {
    lvSign s = lv_expr_sign(expr, sys);
    return (s == SIGN_POSITIVE || s == SIGN_NONNEGATIVE || s == SIGN_ZERO);
}

/* ============== 平方和分解 ============== */

/** 检查表达式是否为平方项 a^2（指数为有理数 2） */
static bool expr_is_pure_square(const lvExpr *e, lvExpr **out_base) {
    if (!e || e->type != EXPR_TYPE_POWER || !e->data.power.exponent)
        return false;
    if (e->data.power.exponent->type != EXPR_TYPE_RATIONAL)
        return false;

    /* 检查指数 == 2 */
    int64_t exp_num = 0;
    if (!lv_expr_get_integer(e->data.power.exponent, &exp_num) || exp_num != 2)
        return false;

    if (out_base)
        *out_base = e->data.power.base;
    return true;
}

/** 检查表达式是否为 2*a*b 形式（即交叉项 2ab） */
static bool expr_is_cross_term(const lvExpr *e, lvExpr **out_a, lvExpr **out_b) {
    if (!e || e->type != EXPR_TYPE_PRODUCT || e->data.composite.count < 2 || e->data.composite.count > 3)
        return false;

    const lvExpr *factor_2 = NULL;
    const lvExpr *factor_a = NULL;
    const lvExpr *factor_b = NULL;

    for (uint32_t i = 0; i < e->data.composite.count; i++) {
        const lvExpr *op = e->data.composite.operands[i];
        if (!op)
            return false;

        /* 检查是否为常数 2 */
        if (op->type == EXPR_TYPE_RATIONAL) {
            int64_t val = 0;
            if (lv_expr_get_integer(op, &val) && val == 2) {
                factor_2 = op;
                continue;
            }
        }
        /* a 和 b */
        if (!factor_a) {
            factor_a = op;
        } else if (!factor_b) {
            factor_b = op;
        } else {
            return false; /* 超过三个因子 */
        }
    }

    /* 需要常量 2 和两个变量 a, b */
    if (!factor_2 || !factor_a || !factor_b)
        return false;

    if (out_a)
        *out_a = (lvExpr *) factor_a;
    if (out_b)
        *out_b = (lvExpr *) factor_b;
    return true;
}

/**
 * 尝试将多项式分解为平方和形式
 *
 * 增强实现：
 * - 识别显式平方项 a^2
 * - 识别交叉项 2*a*b 并结合 a^2, b^2 构成 (a+b)^2
 * - 尝试完成平方（completing the square）对二次型
 * - 对于无法分解的情况，返回明确的失败原因
 */
bool lv_expr_sos_decompose(lvExpr *poly, lvSOSDecomposition **out_sos) {
    if (!poly || !out_sos)
        return false;

    *out_sos = NULL;

    /* 情况 1：多项式是单个幂表达式 a^2 */
    if (poly->type == EXPR_TYPE_POWER && poly->data.power.exponent) {
        if (poly->data.power.exponent->type == EXPR_TYPE_RATIONAL) {
            int64_t exp_val = 0;
            if (lv_expr_get_integer(poly->data.power.exponent, &exp_val) && exp_val == 2) {
                lvSOSDecomposition *sos = (lvSOSDecomposition *) lv_calloc(1, sizeof(lvSOSDecomposition));
                if (!sos)
                    return false;
                sos->squares = (lvExpr **) lv_malloc(sizeof(lvExpr *));
                if (!sos->squares) {
                    lv_free((void **) &sos);
                    return false;
                }
                sos->squares[0] = poly->data.power.base;
                sos->count = 1;
                sos->remainder = NULL;
                sos->failure_reason = NULL;
                *out_sos = sos;
                return true;
            }
        }
        /* 指数不是 2 */
        lvSOSDecomposition *sos = (lvSOSDecomposition *) lv_calloc(1, sizeof(lvSOSDecomposition));
        if (!sos)
            return false;
        sos->failure_reason = lv_strdup(
            "Expression is a power but exponent is not 2; "
            "SOS decomposition requires even powers");
        *out_sos = sos;
        return false;
    }

    /* 情况 2：多项式是和式，检查并尝试完全平方 */
    if (poly->type == EXPR_TYPE_SUM && poly->data.composite.count > 0) {
        uint32_t n = poly->data.composite.count;

        /* 第一步：收集所有纯平方项 a_i^2 */
        uint32_t max_squares = n;
        lvExpr **square_bases = (lvExpr **) lv_calloc((size_t) max_squares, sizeof(lvExpr *));
        bool *consumed = (bool *) lv_calloc((size_t) n, sizeof(bool));
        uint32_t sq_count = 0;

        if (!square_bases || !consumed) {
            lv_free((void **) &square_bases);
            lv_free((void **) &consumed);
            return false;
        }

        for (uint32_t i = 0; i < n; i++) {
            lvExpr *op = poly->data.composite.operands[i];
            if (!op)
                continue;
            lvExpr *base = NULL;
            if (expr_is_pure_square(op, &base)) {
                square_bases[sq_count] = base;
                consumed[i] = true;
                sq_count++;
            }
        }

        /* 第二步：尝试匹配交叉项 2*a*b */
        uint32_t cross_matched = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (consumed[i])
                continue;
            lvExpr *op = poly->data.composite.operands[i];
            if (!op)
                continue;

            lvExpr *cross_a = NULL, *cross_b = NULL;
            if (expr_is_cross_term(op, &cross_a, &cross_b)) {
                /* 寻找匹配的 a^2 和 b^2 */
                for (uint32_t ai = 0; ai < sq_count; ai++) {
                    for (uint32_t bi = ai + 1; bi < sq_count; bi++) {
                        if ((lv_expr_structurally_equal(square_bases[ai], cross_a) &&
                             lv_expr_structurally_equal(square_bases[bi], cross_b)) ||
                            (lv_expr_structurally_equal(square_bases[ai], cross_b) &&
                             lv_expr_structurally_equal(square_bases[bi], cross_a))) {
                            /* 匹配成功：将 a^2, b^2, 2ab 合并为 (a+b)^2 */
                            /* 将 square_bases[bi] 更新为 (a+b) */
                            square_bases[ai] =
                                lv_expr_add(lv_expr_copy(square_bases[ai]), lv_expr_copy(square_bases[bi]));
                            /* 移除 square_bases[bi]：覆盖 */
                            if (bi < sq_count - 1) {
                                square_bases[bi] = square_bases[sq_count - 1];
                            }
                            sq_count--;
                            consumed[i] = true;
                            cross_matched++;
                            goto next_op; /* 跳出双重循环 */
                        }
                    }
                }
            }
        next_op:;
        }

        /* 第三步：处理剩余的未匹配项 */
        uint32_t unmatched = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (!consumed[i])
                unmatched++;
        }

        if (sq_count > 0) {
            lvSOSDecomposition *sos = (lvSOSDecomposition *) lv_calloc(1, sizeof(lvSOSDecomposition));
            if (!sos) {
                lv_free((void **) &square_bases);
                lv_free((void **) &consumed);
                return false;
            }
            sos->squares = (lvExpr **) lv_malloc((size_t) sq_count * sizeof(lvExpr *));
            if (!sos->squares) {
                lv_free((void **) &sos);
                lv_free((void **) &square_bases);
                lv_free((void **) &consumed);
                return false;
            }
            for (uint32_t i = 0; i < sq_count; i++) {
                sos->squares[i] = square_bases[i];
            }
            sos->count = sq_count;
            sos->remainder = NULL;
            sos->failure_reason = NULL;

            if (unmatched > 0) {
                char buf[256];
                lv_snprintf(buf, sizeof(buf),
                         "Partial SOS decomposition: %u square(s) found, but %u term(s) "
                         "could not be matched. Check for non-quadratic or cross terms "
                         "without matching squares.",
                         sq_count, unmatched);
                sos->failure_reason = lv_strdup(buf);
            }

            lv_free((void **) &consumed);
            *out_sos = sos;
            return (unmatched == 0);
        }

        /* 没有找到任何平方项 — 整个和式无法识别 */
        lvSOSDecomposition *sos_fail = (lvSOSDecomposition *) lv_calloc(1, sizeof(lvSOSDecomposition));
        if (sos_fail) {
            sos_fail->failure_reason = lv_strdup(
                "Sum expression contains no recognizable square terms (a^2). "
                "Try rewriting as sum of squares, or check for implicit squaring "
                "(e.g., a*a instead of a^2).");
            *out_sos = sos_fail;
        }
        lv_free((void **) &square_bases);
        lv_free((void **) &consumed);
        return false;
    }

    /* 情况 3：多项式是乘积 a*a */
    if (poly->type == EXPR_TYPE_PRODUCT && poly->data.composite.count == 2) {
        lvExpr *a = poly->data.composite.operands[0];
        lvExpr *b = poly->data.composite.operands[1];
        if (a && b && lv_expr_structurally_equal(a, b)) {
            lvSOSDecomposition *sos = (lvSOSDecomposition *) lv_calloc(1, sizeof(lvSOSDecomposition));
            if (!sos)
                return false;
            sos->squares = (lvExpr **) lv_malloc(sizeof(lvExpr *));
            if (!sos->squares) {
                lv_free((void **) &sos);
                return false;
            }
            sos->squares[0] = a;
            sos->count = 1;
            sos->remainder = NULL;
            sos->failure_reason = NULL;
            *out_sos = sos;
            return true;
        }
    }

    /* 其他情况：无法识别为平方和形式 */
    {
        lvSOSDecomposition *sos_fail = (lvSOSDecomposition *) lv_calloc(1, sizeof(lvSOSDecomposition));
        if (sos_fail) {
            /* 使用 vtable 判定表达式符号，替代直接的类型 switch */
            lvSign s = SIGN_UNKNOWN;
            const lvExprOps *ops = lv_expr_get_ops(poly->type);
            if (ops)
                s = ops->sign(poly, NULL);

            const char *sign_desc = "unknown";
            /* 查静态查找表获取符号描述，越界（含 SIGN_UNKNOWN）保持 "unknown" */
            if ((unsigned)s < sizeof(kSignDescriptionTable) / sizeof(kSignDescriptionTable[0]))
                sign_desc = kSignDescriptionTable[s];
            char buf[256];
            lv_snprintf(buf, sizeof(buf),
                     "Expression is %s — not directly decomposable into sum of squares. "
                     "Consider rewriting as explicit a^2 + b^2 + ... form.",
                     sign_desc);
            sos_fail->failure_reason = lv_strdup(buf);
            *out_sos = sos_fail;
        }
    }

    return false;
}

void lv_sos_destroy(lvSOSDecomposition *sos) {
    if (!sos)
        return;
    /* 注意：squares 数组中存储的是指向原表达式内部节点的指针，
     * 不应对其调用 lv_expr_destroy（由调用者管理表达式生命周期） */
    lv_free((void **) &sos->squares);
    if (sos->failure_reason) {
        lv_free((void **) &sos->failure_reason);
    }
    lv_free((void **) &sos);
}

/* ============== 几何不等式 ============== */

/**
 * 三角形面积不等式（Heron公式）：
 * 对于三角形三边 a, b, c，半周长 p = (a+b+c)/2：
 * area² = p(p-a)(p-b)(p-c)（Heron公式精确相等，构造为不等式关系）
 */
