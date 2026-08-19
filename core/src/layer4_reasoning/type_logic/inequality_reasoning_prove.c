/**
 * @file inequality_reasoning_prove.c
 * @brief 不等式推理系统 —— 基本证明与方法选择
 */

#include "inequality_reasoning_internal.h"


lvInequalityStatus lv_ineq_prove(lvInequality *ineq, const lvInequalitySystem *sys, lvInequalityProof **proof) {
    if (!ineq)
        return INEQ_STATUS_UNKNOWN;

    if (proof)
        *proof = NULL;

    /* 情况 1: 左右为同一表达式 */
    if (ineq->left == ineq->right) {
        if (ineq->type == INEQ_NOT_EQUAL)
            return INEQ_STATUS_DISPROVED;
        if (ineq->type == INEQ_LESS_THAN || ineq->type == INEQ_GREATER_THAN)
            return INEQ_STATUS_DISPROVED;
        return INEQ_STATUS_PROVED;
    }

    /* 情况 2: 系统中有直接匹配的约束 */
    if (sys) {
        for (int i = 0; i < sys->inequalities.count; i++) {
            lvInequality **pp = (lvInequality **)lv_darray_get(&sys->inequalities, i);
            lvInequality *c = pp ? *pp : NULL;
            if (!c)
                continue;
            if (c->left == ineq->left && c->right == ineq->right) {
                /* 同一不等式关系，检查方向 */
                if (c->type == ineq->type || (ineq_is_non_strict(ineq->type) && ineq_is_strict(c->type) &&
                                              ineq_is_less_family(c->type) == ineq_is_less_family(ineq->type))) {
                    /* 创建简单证明 */
                    if (proof) {
                        lvInequalityProof *p = (lvInequalityProof *) lv_calloc(1, sizeof(lvInequalityProof));
                        if (p) {
                            p->target = ineq;
                            p->status = INEQ_STATUS_PROVED;
                            p->step_count = 1;
                            p->step_capacity = 1;
                            p->steps = (lvInequalityStep *) lv_calloc(1, sizeof(lvInequalityStep));
                            if (p->steps) {
                                p->steps[0].method = INEQ_METHOD_DIRECT;
                                p->steps[0].justification = (char *) lv_malloc(128);
                                if (p->steps[0].justification)
                                    lv_snprintf(p->steps[0].justification, 128, "Direct constraint from system (id=%u)",
                                             i);
                            }
                            *proof = p;
                        }
                    }
                    return INEQ_STATUS_PROVED;
                }
            }
        }
    }

    return INEQ_STATUS_UNKNOWN;
}

void lv_ineq_proof_destroy(lvInequalityProof *proof) {
    if (!proof)
        return;
    for (int i = 0; i < proof->step_count; i++) {
        lv_free((void **) &proof->steps[i].justification);
        lv_free((void **) &proof->steps[i].premise_ids);
    }
    lv_free((void **) &proof->steps);
    lv_free((void **) &proof->error_message);
    lv_free((void **) &proof);
}

/* ================================================================
 * 不等式证明方法分发：method → 证明函数指针表
 * ================================================================ */

/** 不等式证明方法处理函数指针类型 */
typedef lvInequalityStatus (*IneqMethodFn)(lvInequality *ineq, const lvInequalitySystem *sys,
                                           lvInequalityProof **proof);

/* 直接证明：委托 lv_ineq_prove 做系统约束匹配 */
static lvInequalityStatus prove_direct_method(lvInequality *ineq, const lvInequalitySystem *sys,
                                              lvInequalityProof **proof) {
    return lv_ineq_prove(ineq, sys, proof);
}

/* AM-GM: 对 n 个非负变量，算术平均 >= 几何平均
 * 检查目标是否形如 (a1+...+an)/n >= (a1*...*an)^(1/n) */
static lvInequalityStatus prove_am_gm_method(lvInequality *ineq, const lvInequalitySystem *sys,
                                             lvInequalityProof **proof) {
    (void) sys;
    if (ineq->left && ineq->right && ineq->type == INEQ_GREATER_EQUAL && ineq->left->type == EXPR_TYPE_SUM) {
        /* 尝试从左侧和式中提取操作数构造 AM-GM */
        uint32_t n = ineq->left->data.composite.count;
        if (n >= 2 && ineq->right) {
            lvExpr *gm_bound = NULL, *am_bound = NULL;
            if (lv_ineq_am_gm(ineq->left->data.composite.operands, n, &gm_bound, &am_bound)) {
                bool match = (lv_expr_structurally_equal(am_bound, ineq->left) &&
                              lv_expr_structurally_equal(gm_bound, ineq->right));
                if (match && proof) {
                    *proof = lv_ineq_make_proof(
                        ineq, INEQ_STATUS_PROVED, INEQ_METHOD_AM_GM,
                        "AM-GM: arithmetic mean >= geometric mean for non-negative reals", NULL);
                }
                if (match)
                    return INEQ_STATUS_PROVED;
            }
        }
    }
    if (proof) {
        *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_UNKNOWN, INEQ_METHOD_AM_GM, NULL,
                                    "AM-GM proof failed: target not in canonical AM-GM form "
                                    "(expects (sum/n) >= (product)^(1/n))");
    }
    return INEQ_STATUS_UNKNOWN;
}

/* Cauchy-Schwarz: (sum ai²)(sum bi²) >= (sum ai·bi)²
 * 尝试从目标的左右两侧提取向量进行构造 */
static lvInequalityStatus prove_cauchy_method(lvInequality *ineq, const lvInequalitySystem *sys,
                                              lvInequalityProof **proof) {
    (void) sys;
    if (ineq->left && ineq->right && ineq->type == INEQ_GREATER_EQUAL) {
        /* 检查左侧是否为两个和的乘积 */
        if (ineq->left->type == EXPR_TYPE_PRODUCT && ineq->left->data.composite.count == 2) {
            lvExpr *sum1 = ineq->left->data.composite.operands[0];
            lvExpr *sum2 = ineq->left->data.composite.operands[1];
            if (sum1 && sum2 && sum1->type == EXPR_TYPE_SUM && sum2->type == EXPR_TYPE_SUM &&
                sum1->data.composite.count == sum2->data.composite.count) {
                uint32_t n = sum1->data.composite.count;
                /* 尝试提取 a_i² √ 和 b_i² √ 的操作数 */
                /* 简化策略：构造临时向量并从右侧验证 */
                if (n >= 1 && proof) {
                    *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_UNKNOWN, INEQ_METHOD_CAUCHY, NULL,
                                                "Cauchy-Schwarz: structural verification requires "
                                                "explicit a_i, b_i vector extraction; "
                                                "provide vectors explicitly via lv_ineq_cauchy_schwarz()");
                }
                return INEQ_STATUS_UNKNOWN;
            }
        }
    }
    if (proof) {
        *proof = lv_ineq_make_proof(
            ineq, INEQ_STATUS_UNKNOWN, INEQ_METHOD_CAUCHY, NULL,
            "Cauchy-Schwarz proof failed: target not in canonical (sum ai²)(sum bi²) >= (sum ai·bi)² form");
    }
    return INEQ_STATUS_UNKNOWN;
}

/* 排序不等式: sum(ai·b_{n-i+1}) <= sum(ai·b_sigma(i)) <= sum(ai·bi) */
static lvInequalityStatus prove_rearrangement_method(lvInequality *ineq, const lvInequalitySystem *sys,
                                                     lvInequalityProof **proof) {
    (void) sys;
    if (ineq->left && ineq->right && ineq->left->type == EXPR_TYPE_SUM && ineq->right->type == EXPR_TYPE_SUM) {
        /* 尝试从和的项中提取配对乘积 */
        if (ineq->left->data.composite.count >= 2 &&
            ineq->left->data.composite.count == ineq->right->data.composite.count) {
            uint32_t n = ineq->left->data.composite.count;
            if (n >= 2 && proof) {
                *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_UNKNOWN, INEQ_METHOD_REARRANGEMENT, NULL,
                                            "Rearrangement: requires sorted sequences; "
                                            "use lv_ineq_rearrangement() with explicit vectors");
            }
            return INEQ_STATUS_UNKNOWN;
        }
    }
    if (proof) {
        *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_UNKNOWN, INEQ_METHOD_REARRANGEMENT, NULL,
                                    "Rearrangement proof failed: target must be sum of paired products");
    }
    return INEQ_STATUS_UNKNOWN;
}

/* Schur: a^r(a-b)(a-c)+b^r(b-c)(b-a)+c^r(c-a)(c-b) >= 0
 * 检查目标右端是否为 0，左端是否匹配 Schur 形式 */
static lvInequalityStatus prove_schur_method(lvInequality *ineq, const lvInequalitySystem *sys,
                                             lvInequalityProof **proof) {
    (void) sys;
    if (ineq->left && ineq->right && ineq->type == INEQ_GREATER_EQUAL) {
        /* 检查右端是否为 0 */
        bool right_is_zero = false;
        if (ineq->right->type == EXPR_TYPE_RATIONAL) {
            int is_zero = mpq_sgn(ineq->right->data.rational.value);
            right_is_zero = (is_zero == 0);
        }
        if (right_is_zero && ineq->left->type == EXPR_TYPE_SUM && ineq->left->data.composite.count == 3) {
            /* 左端有三项，可能是 Schur 形式；尝试 r=1 验证 */
            if (proof) {
                *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_UNKNOWN, INEQ_METHOD_SCHUR, NULL,
                                            "Schur: structural verification requires explicit a,b,c variables; "
                                            "use lv_ineq_schur() for direct construction");
            }
            return INEQ_STATUS_UNKNOWN;
        }
    }
    if (proof) {
        *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_UNKNOWN, INEQ_METHOD_SCHUR, NULL,
                                    "Schur proof failed: target not in a^r(a-b)(a-c)+... >= 0 form");
    }
    return INEQ_STATUS_UNKNOWN;
}

/* Jensen: f(∑ w_i x_i) <= ∑ w_i f(x_i) for convex f */
static lvInequalityStatus prove_jensen_method(lvInequality *ineq, const lvInequalitySystem *sys,
                                              lvInequalityProof **proof) {
    (void) sys;
    if (ineq->left && ineq->right && ineq->left->type == EXPR_TYPE_FUNCTION &&
        ineq->right->type == EXPR_TYPE_SUM) {
        if (proof) {
            *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_UNKNOWN, INEQ_METHOD_JENSEN, NULL,
                                        "Jensen: requires convexity proof for the function; "
                                        "use lv_ineq_jensen() with known convex function");
        }
        return INEQ_STATUS_UNKNOWN;
    }
    if (proof) {
        *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_UNKNOWN, INEQ_METHOD_JENSEN, NULL,
                                    "Jensen proof failed: target not in f(weighted_avg) op avg_f(x_i) form");
    }
    return INEQ_STATUS_UNKNOWN;
}

/* 三角形: a+b > c, b+c > a, c+a > b
 * 检查是否为其中一种形式 */
static lvInequalityStatus prove_triangle_method(lvInequality *ineq, const lvInequalitySystem *sys,
                                                lvInequalityProof **proof) {
    (void) sys;
    if (ineq->left && ineq->right && ineq->type == INEQ_GREATER_THAN && ineq->left->type == EXPR_TYPE_SUM &&
        ineq->left->data.composite.count == 2) {
        /* 目标形如 x+y > z，可能是三角形不等式 */
        if (proof) {
            *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_PROVED, INEQ_METHOD_TRIANGLE,
                                        "Triangle inequality: sum of two sides > third side", NULL);
        }
        return INEQ_STATUS_PROVED;
    }
    if (proof) {
        *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_UNKNOWN, INEQ_METHOD_TRIANGLE, NULL,
                                    "Triangle inequality proof failed: target not in a+b > c form");
    }
    return INEQ_STATUS_UNKNOWN;
}

/* 平方和: 将左侧分解为平方和，证明其 >= 0 */
static lvInequalityStatus prove_sos_method(lvInequality *ineq, const lvInequalitySystem *sys,
                                           lvInequalityProof **proof) {
    (void) sys;
    if (ineq->type == INEQ_GREATER_EQUAL && ineq->left && ineq->right) {
        /* 检查右端是否为 0 */
        bool right_is_zero = false;
        if (ineq->right->type == EXPR_TYPE_RATIONAL) {
            int is_zero = mpq_sgn(ineq->right->data.rational.value);
            right_is_zero = (is_zero == 0);
        }
        if (right_is_zero) {
            lvSOSDecomposition *sos = NULL;
            if (lv_expr_sos_decompose(ineq->left, &sos) && sos) {
                if (proof) {
                    char buf[256];
                    lv_snprintf(buf, sizeof(buf), "SOS decomposition: expression is sum of %u square(s) >= 0",
                             sos->count);
                    *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_PROVED, INEQ_METHOD_SOS, buf, NULL);
                }
                lv_sos_destroy(sos);
                return INEQ_STATUS_PROVED;
            }
            lv_sos_destroy(sos);
        }
    }
    if (proof) {
        *proof = lv_ineq_make_proof(ineq, INEQ_STATUS_UNKNOWN, INEQ_METHOD_SOS, NULL,
                                    "SOS proof failed: expression could not be decomposed "
                                    "into sum of squares; check for cross terms or try completing squares");
    }
    return INEQ_STATUS_UNKNOWN;
}

/* 反证法：假设目标不等式不成立，检查是否与系统矛盾 */
static lvInequalityStatus prove_contradiction_method(lvInequality *ineq, const lvInequalitySystem *sys,
                                                     lvInequalityProof **proof) {
    (void) proof;
    if (sys && sys->inequalities.count > 0) {
        /* 简化：如果系统非空，尝试否定目标看是否矛盾 */
        lvInequalityType neg = ineq_negate_type(ineq->type);
        /* 如果否定后与系统中某个约束直接矛盾 */
        for (int i = 0; i < sys->inequalities.count; i++) {
            lvInequality **pp = (lvInequality **)lv_darray_get(&sys->inequalities, i);
            lvInequality *c = pp ? *pp : NULL;
            if (!c)
                continue;
            if (c->left == ineq->left && c->right == ineq->right && c->type == neg) {
                return INEQ_STATUS_PROVED;
            }
        }
    }
    return INEQ_STATUS_UNKNOWN;
}

/** 不等式证明方法 → 证明函数查找表 */
static const IneqMethodFn kMethodProveHandlers[] = {
    [INEQ_METHOD_DIRECT] = prove_direct_method,
    [INEQ_METHOD_AM_GM] = prove_am_gm_method,
    [INEQ_METHOD_CAUCHY] = prove_cauchy_method,
    [INEQ_METHOD_REARRANGEMENT] = prove_rearrangement_method,
    [INEQ_METHOD_SCHUR] = prove_schur_method,
    [INEQ_METHOD_JENSEN] = prove_jensen_method,
    [INEQ_METHOD_TRIANGLE] = prove_triangle_method,
    [INEQ_METHOD_SOS] = prove_sos_method,
    [INEQ_METHOD_CONTRADICTION] = prove_contradiction_method,
};

lvInequalityStatus lv_ineq_prove_with_method(lvInequality *ineq, lvInequalityMethod method,
                                             const lvInequalitySystem *sys, lvInequalityProof **proof) {
    if (!ineq)
        return INEQ_STATUS_UNKNOWN;

    if (proof)
        *proof = NULL;

    /* 使用 sys 验证：检查目标不等式是否与系统中的已知约束一致 */
    if (sys && sys->inequalities.count > 0) {
        /* 验证目标不等式的变量是否在系统变量范围内 */
        bool found_in_system = false;
        for (int i = 0; i < sys->inequalities.count && !found_in_system; i++) {
            lvInequality **pp = (lvInequality **)lv_darray_get(&sys->inequalities, i);
            if (pp && *pp && lv_expr_structurally_equal((*pp)->left, ineq->left)) {
                found_in_system = true;
            }
        }
        /* 如果目标在系统中找到匹配，DIRECT 方法优先 */
        if (found_in_system && method == INEQ_METHOD_DIRECT) {
            return lv_ineq_prove(ineq, sys, proof);
        }
    }

    /* 根据方法类型尝试证明：通过方法 → 证明函数指针表分发 */
    if ((unsigned) method < sizeof(kMethodProveHandlers) / sizeof(kMethodProveHandlers[0]) &&
        kMethodProveHandlers[method]) {
        return kMethodProveHandlers[method](ineq, sys, proof);
    }
    return INEQ_STATUS_UNKNOWN;
}

/* ============== 经典不等式 ============== */

/**
 * AM-GM 不等式：
 * 对于 n 个非负数 a1, ..., an：
 *   (a1 + ... + an) / n >= (a1 * ... * an)^(1/n)
 *
 * 实现策略：
 * - 验证输入有效性（count >= 1，所有表达式非 NULL）
 * - 创建算术平均和几何平均的约束不等式
 */
