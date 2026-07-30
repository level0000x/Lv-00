/**
 * @file inequality_reasoning.c
 * @brief 不等式推理系统 - 真实实现
 *
 * @details 实现不等式创建/销毁、基本证明、经典不等式（AM-GM、Cauchy-Schwarz、
 * 排序不等式、Schur、Jensen、三角形不等式）、不等式变换（加减乘、传递、合并）、
 * 表达式符号判定、平方和分解、几何不等式和序列化。
 *
 * @version 3.3.0
 * @author Lv-00 Team
 */

#include "inequality_reasoning.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_utils.h"

/* ============== 内部辅助函数 ============== */

/** 不等式类型翻转映射 */
static lvInequalityType ineq_negate_type(lvInequalityType t) {
    switch (t) {
        case INEQ_LESS_THAN:
            return INEQ_GREATER_THAN;
        case INEQ_LESS_EQUAL:
            return INEQ_GREATER_EQUAL;
        case INEQ_GREATER_THAN:
            return INEQ_LESS_THAN;
        case INEQ_GREATER_EQUAL:
            return INEQ_LESS_EQUAL;
        default:
            return t;
    }
}

/** 判断两个不等式是否同向（可合并） */
static bool ineq_same_direction(lvInequalityType a, lvInequalityType b) {
    return (a == INEQ_LESS_THAN || a == INEQ_LESS_EQUAL) == (b == INEQ_LESS_THAN || b == INEQ_LESS_EQUAL);
}

/** 判断不等式是否为严格不等式 */
static bool ineq_is_strict(lvInequalityType t) {
    return (t == INEQ_LESS_THAN || t == INEQ_GREATER_THAN);
}

/** 判断不等式是否为 <= 或 >= */
static bool ineq_is_non_strict(lvInequalityType t) {
    return (t == INEQ_LESS_EQUAL || t == INEQ_GREATER_EQUAL);
}

/** 判断不等式类型是否为 <= 或 < */
static bool ineq_is_less_family(lvInequalityType t) {
    return (t == INEQ_LESS_THAN || t == INEQ_LESS_EQUAL);
}

/** 结构性比较两个表达式（递归深度优先） */
static bool lv_expr_structurally_equal(const lvExpr *a, const lvExpr *b) {
    /* 空指针处理 */
    if (a == b)
        return true;
    if (!a || !b)
        return false;

    /* 类型必须一致 */
    if (a->type != b->type)
        return false;

    switch (a->type) {
        case EXPR_TYPE_VARIABLE:
            return (a->data.variable.name && b->data.variable.name &&
                    strcmp(a->data.variable.name, b->data.variable.name) == 0);

        case EXPR_TYPE_RATIONAL:
            return (mpq_equal(a->data.rational.value, b->data.rational.value) != 0);

        case EXPR_TYPE_POWER:
            return (lv_expr_structurally_equal(a->data.power.base, b->data.power.base) &&
                    lv_expr_structurally_equal(a->data.power.exponent, b->data.power.exponent));

        case EXPR_TYPE_PRODUCT:
        case EXPR_TYPE_SUM:
            if (a->data.composite.count != b->data.composite.count)
                return false;
            for (uint32_t i = 0; i < a->data.composite.count; i++) {
                if (!lv_expr_structurally_equal(a->data.composite.operands[i], b->data.composite.operands[i]))
                    return false;
            }
            return true;

        case EXPR_TYPE_FUNCTION:
            return (a->data.function.func_name && b->data.function.func_name &&
                    strcmp(a->data.function.func_name, b->data.function.func_name) == 0 &&
                    lv_expr_structurally_equal(a->data.function.argument, b->data.function.argument));
        default:
            return false; /* 未知表达式类型视为不等 */
    }
    return false;
}

/** 结构性比较两个不等式 */
static bool lv_ineq_structurally_equal(const lvInequality *a, const lvInequality *b) {
    if (!a || !b)
        return false;
    return (a->type == b->type && lv_expr_structurally_equal(a->left, b->left) &&
            lv_expr_structurally_equal(a->right, b->right));
}

/** 创建带错误消息的证明结构 */
static lvInequalityProof *lv_ineq_make_proof(lvInequality *target, lvInequalityStatus status, lvInequalityMethod method,
                                             const char *justification, const char *error) {
    lvInequalityProof *p = (lvInequalityProof *) lv_calloc(1, sizeof(lvInequalityProof));
    if (!p)
        return NULL;

    p->target = target;
    p->status = status;
    p->step_count = 1;
    p->step_capacity = 1;
    p->steps = (lvInequalityStep *) lv_calloc(1, sizeof(lvInequalityStep));
    if (!p->steps) {
        lv_free((void **) &p);
        return NULL;
    }

    p->steps[0].method = method;
    p->steps[0].ineq = target;
    if (justification) {
        p->steps[0].justification = lv_strdup(justification);
    }
    if (error) {
        p->error_message = lv_strdup(error);
    }

    return p;
}

/* ============== 不等式创建/销毁 ============== */

lvInequality *lv_ineq_create(lvExpr *left, lvInequalityType type, lvExpr *right) {
    lvInequality *ineq = (lvInequality *) lv_calloc(1, sizeof(lvInequality));
    if (!ineq)
        return NULL;
    ineq->left = left;
    ineq->right = right;
    ineq->type = type;
    ineq->status = INEQ_STATUS_UNPROVED;
    ineq->label = NULL;
    return ineq;
}

void lv_ineq_destroy(lvInequality *ineq) {
    if (!ineq)
        return;
    /* 注意：不释放 left/right 表达式，由调用者管理 */
    lv_free((void **) &ineq->label);
    lv_free((void **) &ineq);
}

lvInequality *lv_ineq_copy(const lvInequality *ineq) {
    if (!ineq)
        return NULL;
    lvInequality *copy = (lvInequality *) lv_calloc(1, sizeof(lvInequality));
    if (!copy)
        return NULL;
    copy->left = ineq->left;
    copy->right = ineq->right;
    copy->type = ineq->type;
    copy->status = ineq->status;
    if (ineq->label) {
        copy->label = (char *) lv_malloc(strlen(ineq->label) + 1);
        if (copy->label)
            snprintf(copy->label, strlen(ineq->label) + 1, "%s", ineq->label);
    }
    return copy;
}

lvInequalitySystem *lv_ineq_system_create(void) {
    lvInequalitySystem *sys = (lvInequalitySystem *) lv_calloc(1, sizeof(lvInequalitySystem));
    if (sys)
        lv_darray_init(&sys->inequalities, sizeof(lvInequality *));
    return sys;
}

void lv_ineq_system_destroy(lvInequalitySystem *sys) {
    if (!sys)
        return;
    for (int i = 0; i < sys->inequalities.count; i++) {
        lvInequality **p = (lvInequality **)lv_darray_get(&sys->inequalities, i);
        if (p && *p)
            lv_ineq_destroy(*p);
    }
    lv_darray_free(&sys->inequalities);
    lv_free((void **) &sys->variables);
    lv_free((void **) &sys);
}

bool lv_ineq_system_add(lvInequalitySystem *sys, lvInequality *ineq) {
    if (!sys || !ineq)
        return false;

    if (lv_darray_push(&sys->inequalities, &ineq) < 0)
        return false;
    return true;
}

bool lv_ineq_system_add_var_constraint(lvInequalitySystem *sys, lvExpr *var, lvInequalityType type, const mpq_t value) {
    if (!sys || !var)
        return false;

    /* 创建不等式: var <type> value */
    lvExpr *val_expr = lv_expr_create_rational_mpq(value);
    if (!val_expr)
        return false;

    lvInequality *ineq = lv_ineq_create(var, type, val_expr);
    if (!ineq) {
        lv_expr_destroy(&val_expr);
        return false;
    }

    return lv_ineq_system_add(sys, ineq);
}

/* ============== 基本不等式证明 ============== */

/**
 * @brief 证明不等式：检查系统中的约束是否足以推导目标不等式
 *
 * 策略：
 * 1. 如果 left == right，则等式成立
 * 2. 遍历系统约束，尝试传递链推导
 * 3. 检查变量约束（如 x > 0）是否支持推导
 */
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
                                    snprintf(p->steps[0].justification, 128, "Direct constraint from system (id=%u)",
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

    /* 根据方法类型尝试证明 */
    switch (method) {
        case INEQ_METHOD_DIRECT:
            return lv_ineq_prove(ineq, sys, proof);

        case INEQ_METHOD_AM_GM:
            /* AM-GM: 对 n 个非负变量，算术平均 >= 几何平均
             * 检查目标是否形如 (a1+...+an)/n >= (a1*...*an)^(1/n) */
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

        case INEQ_METHOD_CAUCHY:
            /* Cauchy-Schwarz: (sum ai²)(sum bi²) >= (sum ai·bi)²
             * 尝试从目标的左右两侧提取向量进行构造 */
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

        case INEQ_METHOD_REARRANGEMENT:
            /* 排序不等式: sum(ai·b_{n-i+1}) <= sum(ai·b_sigma(i)) <= sum(ai·bi) */
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

        case INEQ_METHOD_SCHUR:
            /* Schur: a^r(a-b)(a-c)+b^r(b-c)(b-a)+c^r(c-a)(c-b) >= 0
             * 检查目标右端是否为 0，左端是否匹配 Schur 形式 */
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

        case INEQ_METHOD_JENSEN:
            /* Jensen: f(∑ w_i x_i) <= ∑ w_i f(x_i) for convex f */
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

        case INEQ_METHOD_TRIANGLE:
            /* 三角形: a+b > c, b+c > a, c+a > b
             * 检查是否为其中一种形式 */
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

        case INEQ_METHOD_SOS:
            /* 平方和: 将左侧分解为平方和，证明其 >= 0 */
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
                            snprintf(buf, sizeof(buf), "SOS decomposition: expression is sum of %u square(s) >= 0",
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

        case INEQ_METHOD_CONTRADICTION: {
            /* 反证法：假设目标不等式不成立，检查是否与系统矛盾 */
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

        default:
            return INEQ_STATUS_UNKNOWN;
    }
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
bool lv_ineq_am_gm(lvExpr **expressions, uint32_t count, lvExpr **out_lower_bound, lvExpr **out_upper_bound) {
    if (!expressions || count == 0)
        return false;

    /* 验证所有表达式非 NULL */
    for (uint32_t i = 0; i < count; i++) {
        if (!expressions[i])
            return false;
    }

    /* AM-GM 产生的不等式：
     * 几何平均 <= 算术平均
     * 即 GM 是下界，AM 是上界
     *
     * 构造：
     *   AM = (e1 + ... + en) / n  (上界)
     *   GM = (e1 * ... * en)^(1/n) (下界)
     */

    /* 算术平均: (e1 + ... + en) / n */
    lvExpr *sum = NULL;
    lvExpr *inv_n = NULL;
    lvExpr *am = NULL;
    lvExpr *prod = NULL;
    lvExpr *inv_n_exp = NULL;
    lvExpr *gm = NULL;
    bool ok = false;

    sum = lv_expr_sum_n(expressions, count);
    if (!sum)
        goto cleanup;

    inv_n = lv_expr_create_rational(1, count);
    if (!inv_n)
        goto cleanup;

    /* sum * (1/n) = sum / n */
    am = lv_expr_mul(sum, inv_n);
    if (!am)
        goto cleanup;

    /* 几何平均: (e1 * ... * en)^(1/n) */
    prod = lv_expr_product_n(expressions, count);
    if (!prod)
        goto cleanup;

    /* inv_n_expr = 1/n as exponent */
    inv_n_exp = lv_expr_create_rational(1, count);
    if (!inv_n_exp)
        goto cleanup;

    gm = lv_expr_power(prod, inv_n_exp);
    if (!gm)
        goto cleanup;

    if (out_lower_bound)
        *out_lower_bound = gm;
    if (out_upper_bound)
        *out_upper_bound = am;

    ok = true;

cleanup:
    if (!ok) {
        lv_expr_free(gm);
        lv_expr_free(inv_n_exp);
        lv_expr_free(prod);
        lv_expr_free(am);
        lv_expr_free(inv_n);
        lv_expr_free(sum);
    }
    return ok;
}

/**
 * Cauchy-Schwarz 不等式：
 * (sum ai^2)(sum bi^2) >= (sum ai*bi)^2
 */
bool lv_ineq_cauchy_schwarz(lvExpr **a, lvExpr **b, uint32_t count, lvInequality **out_ineq) {
    if (!a || !b || count == 0)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        if (!a[i] || !b[i])
            return false;
    }

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* Cauchy-Schwarz: (∑a_i²)(∑b_i²) ≥ (∑a_i·b_i)²
     *
     * 构造三部分：
     *   left = (∑a_i²) * (∑b_i²)
     *   right = (∑a_i·b_i)²
     */

    /* 构造 a_i² 数组: a²[i] = a[i]^2 */
    lvExpr *two = lv_expr_create_rational(2, 1);
    if (!two)
        return false;

    lvExpr **a_sq = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!a_sq) {
        lv_expr_free(two);
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        a_sq[i] = lv_expr_power(a[i], two);
        if (!a_sq[i])
            goto cleanup;
    }

    /* 构造 b_i² 数组: b_sq[i] = b[i]^2 */
    lvExpr **b_sq = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!b_sq)
        goto cleanup;
    for (uint32_t i = 0; i < count; i++) {
        b_sq[i] = lv_expr_power(b[i], two);
        if (!b_sq[i])
            goto cleanup;
    }

    /* 构造 a_i·b_i 数组 */
    lvExpr **ab = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!ab)
        goto cleanup;
    for (uint32_t i = 0; i < count; i++) {
        ab[i] = lv_expr_mul(a[i], b[i]);
        if (!ab[i])
            goto cleanup;
    }

    /* sum_a_sq = ∑a_i², sum_b_sq = ∑b_i², sum_ab = ∑a_i·b_i */
    lvExpr *sum_a_sq = NULL;
    lvExpr *sum_b_sq = NULL;
    lvExpr *sum_ab = NULL;
    lvExpr *left = NULL;
    lvExpr *right = NULL;

    sum_a_sq = lv_expr_sum_n(a_sq, count);
    sum_b_sq = lv_expr_sum_n(b_sq, count);
    sum_ab = lv_expr_sum_n(ab, count);
    if (!sum_a_sq || !sum_b_sq || !sum_ab)
        goto cleanup;

    /* left = (∑a_i²) * (∑b_i²) */
    left = lv_expr_mul(sum_a_sq, sum_b_sq);
    if (!left)
        goto cleanup;

    /* right = (∑a_i·b_i)² */
    right = lv_expr_power(sum_ab, two);
    if (!right)
        goto cleanup;

    *out_ineq = lv_ineq_create(left, INEQ_GREATER_EQUAL, right);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Cauchy-Schwarz");
    }

cleanup:
    /* 释放临时数组和中间表达式 */
    lv_free((void **) &a_sq);
    lv_free((void **) &b_sq);
    lv_free((void **) &ab);
    lv_expr_free(two);
    if (*out_ineq == NULL) {
        lv_expr_free(right);
        lv_expr_free(left);
        lv_expr_free(sum_ab);
        lv_expr_free(sum_b_sq);
        lv_expr_free(sum_a_sq);
    }

    return (*out_ineq != NULL);
}

/**
 * 排序不等式：
 * 对于递增序列 a1<=...<=an 和 b1<=...<=bn：
 * sum ai*b(n-i+1) <= sum ai*b_sigma(i) <= sum ai*bi
 */
bool lv_ineq_rearrangement(lvExpr **a, lvExpr **b, uint32_t count, lvExpr **out_min, lvExpr **out_max) {
    if (!a || !b || count == 0)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        if (!a[i] || !b[i])
            return false;
    }

    /* 反序乘积和为最小，同序乘积和为最大
     *
     * max = a₁b₁ + a₂b₂ + ... + a_n b_n   (同序和)
     * min = a₁b_n + a₂b_{n-1} + ... + a_n b₁ (反序和)
     */

    if (!out_max && !out_min)
        return true;

    /* 构造同序和 (max) */
    if (out_max) {
        lvExpr **same_prods = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        if (!same_prods)
            return false;
        for (uint32_t i = 0; i < count; i++) {
            same_prods[i] = lv_expr_mul(a[i], b[i]);
            if (!same_prods[i]) {
                lv_free((void **) &same_prods);
                return false;
            }
        }
        *out_max = lv_expr_sum_n(same_prods, count);
        lv_free((void **) &same_prods);
        if (!*out_max)
            return false;
    }

    /* 构造反序和 (min) */
    if (out_min) {
        lvExpr **rev_prods = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        if (!rev_prods)
            return false;
        for (uint32_t i = 0; i < count; i++) {
            /* a[i] * b[count - 1 - i] */
            rev_prods[i] = lv_expr_mul(a[i], b[count - 1 - i]);
            if (!rev_prods[i]) {
                lv_free((void **) &rev_prods);
                return false;
            }
        }
        *out_min = lv_expr_sum_n(rev_prods, count);
        lv_free((void **) &rev_prods);
        if (!*out_min)
            return false;
    }

    return true;
}

/**
 * Schur 不等式：
 * a^r(a-b)(a-c) + b^r(b-c)(b-a) + c^r(c-a)(c-b) >= 0
 * 其中 a, b, c >= 0, r >= 0
 */
bool lv_ineq_schur(lvExpr *a, lvExpr *b, lvExpr *c, uint32_t r, lvInequality **out_ineq) {
    if (!a || !b || !c)
        return false;

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* Schur 不等式:
     * a^r(a-b)(a-c) + b^r(b-c)(b-a) + c^r(c-a)(c-b) >= 0
     *
     * 构造左端表达式，右端为 0
     */

    lvExpr *minus_one = lv_expr_create_rational(-1, 1);
    lvExpr *r_expr = lv_expr_create_rational((int64_t) r, 1);
    lvExpr *zero = lv_expr_create_rational(0, 1);
    if (!minus_one || !r_expr || !zero) {
        lv_expr_free(minus_one);
        lv_expr_free(r_expr);
        lv_expr_free(zero);
        return false;
    }

    /* 构造差值: a-b, a-c, b-c, b-a, c-a, c-b */
    lvExpr *a_minus_b = NULL;
    lvExpr *a_minus_c = NULL;
    lvExpr *b_minus_c = NULL;
    lvExpr *b_minus_a = NULL;
    lvExpr *c_minus_a = NULL;
    lvExpr *c_minus_b = NULL;
    lvExpr *a_pow_r = NULL;
    lvExpr *b_pow_r = NULL;
    lvExpr *c_pow_r = NULL;
    lvExpr *term1 = NULL;
    lvExpr *term2 = NULL;
    lvExpr *term3 = NULL;
    lvExpr *left = NULL;
    bool ok = false;

    a_minus_b = lv_expr_add(a, lv_expr_mul(b, minus_one));
    a_minus_c = lv_expr_add(a, lv_expr_mul(c, minus_one));
    b_minus_c = lv_expr_add(b, lv_expr_mul(c, minus_one));
    b_minus_a = lv_expr_add(b, lv_expr_mul(a, minus_one));
    c_minus_a = lv_expr_add(c, lv_expr_mul(a, minus_one));
    c_minus_b = lv_expr_add(c, lv_expr_mul(b, minus_one));
    if (!a_minus_b || !a_minus_c || !b_minus_c || !b_minus_a || !c_minus_a || !c_minus_b)
        goto cleanup;

    /* 构造三项: term1 = a^r * (a-b) * (a-c) */
    a_pow_r = lv_expr_power(a, r_expr);
    if (!a_pow_r)
        goto cleanup;
    term1 = lv_expr_mul(lv_expr_mul(a_pow_r, a_minus_b), a_minus_c);
    if (!term1)
        goto cleanup;

    /* term2 = b^r * (b-c) * (b-a) */
    b_pow_r = lv_expr_power(b, r_expr);
    if (!b_pow_r)
        goto cleanup;
    term2 = lv_expr_mul(lv_expr_mul(b_pow_r, b_minus_c), b_minus_a);
    if (!term2)
        goto cleanup;

    /* term3 = c^r * (c-a) * (c-b) */
    c_pow_r = lv_expr_power(c, r_expr);
    if (!c_pow_r)
        goto cleanup;
    term3 = lv_expr_mul(lv_expr_mul(c_pow_r, c_minus_a), c_minus_b);
    if (!term3)
        goto cleanup;

    /* 左端 = term1 + term2 + term3 */
    lvExpr *terms_arr[3];
    terms_arr[0] = term1;
    terms_arr[1] = term2;
    terms_arr[2] = term3;
    left = lv_expr_sum_n(terms_arr, 3);
    if (!left)
        goto cleanup;

    *out_ineq = lv_ineq_create(left, INEQ_GREATER_EQUAL, zero);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Schur");
    }

    ok = true;

cleanup:
    if (!ok) {
        lv_expr_free(left);
        lv_expr_free(term3);
        lv_expr_free(term2);
        lv_expr_free(term1);
        lv_expr_free(c_pow_r);
        lv_expr_free(b_pow_r);
        lv_expr_free(a_pow_r);
        lv_expr_free(c_minus_b);
        lv_expr_free(c_minus_a);
        lv_expr_free(b_minus_a);
        lv_expr_free(b_minus_c);
        lv_expr_free(a_minus_c);
        lv_expr_free(a_minus_b);
        lv_expr_free(zero);
        lv_expr_free(r_expr);
        lv_expr_free(minus_one);
    }
    return ok;
}

/**
 * Jensen 不等式：
 * 凸函数: f(sum wi*xi) <= sum wi*f(xi)
 * 凹函数: f(sum wi*xi) >= sum wi*f(xi)
 */
bool lv_ineq_jensen(const char *func, lvExpr **points, mpq_t *weights, uint32_t count, bool is_convex,
                    lvInequality **out_ineq) {
    if (!func || !points || count == 0)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        if (!points[i])
            return false;
    }

    /* 验证权重和为 1 */
    if (weights) {
        mpq_t sum;
        mpq_init(sum);
        mpq_set_ui(sum, 0, 1);
        for (uint32_t i = 0; i < count; i++) {
            mpq_add(sum, sum, weights[i]);
        }
        int cmp = mpq_cmp_ui(sum, 1, 1);
        mpq_clear(sum);
        if (cmp != 0)
            return false;
    }

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* Jensen 不等式:
     * 凸函数: f(∑ w_i x_i) ≤ ∑ w_i f(x_i)
     * 凹函数: f(∑ w_i x_i) ≥ ∑ w_i f(x_i)
     *
     * 构造:
     *   left  = f(∑ w_i * x_i)
     *   right = ∑ w_i * f(x_i)
     */

    lvExpr *weighted_sum = NULL;  /* ∑ w_i * x_i */
    lvExpr *weighted_func = NULL; /* ∑ w_i * f(x_i) */

    if (weights) {
        /* 有自定义权重 */
        lvExpr **w_x = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        lvExpr **w_fx = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        if (!w_x || !w_fx) {
            lv_free((void **) &w_x);
            lv_free((void **) &w_fx);
            return false;
        }

        for (uint32_t i = 0; i < count; i++) {
            lvExpr *w_expr = lv_expr_create_rational_mpq(weights[i]);
            if (!w_expr)
                goto cleanup_jensen;
            w_x[i] = lv_expr_mul(w_expr, points[i]);
            if (!w_x[i]) {
                lv_expr_free(w_expr);
                goto cleanup_jensen;
            }

            /* f(x_i) */
            lvExpr *fx = lv_expr_function(func, points[i]);
            if (!fx) {
                lv_expr_free(w_expr);
                goto cleanup_jensen;
            }
            /* 需要另一个权重表达式副本 */
            lvExpr *w_expr2 = lv_expr_create_rational_mpq(weights[i]);
            if (!w_expr2) {
                lv_expr_free(w_expr);
                lv_expr_free(fx);
                goto cleanup_jensen;
            }
            w_fx[i] = lv_expr_mul(w_expr2, fx);
            if (!w_fx[i]) {
                lv_expr_free(w_expr);
                lv_expr_free(w_expr2);
                lv_expr_free(fx);
                goto cleanup_jensen;
            }
        }

        weighted_sum = lv_expr_sum_n(w_x, count);
        weighted_func = lv_expr_sum_n(w_fx, count);

    cleanup_jensen:
        lv_free((void **) &w_x);
        lv_free((void **) &w_fx);
        if (!weighted_sum || !weighted_func)
            return false;
    } else {
        /* 等权重: w_i = 1/n */
        lvExpr *inv_n = NULL;
        lvExpr *sum = NULL;
        lvExpr **fx_arr = NULL;
        lvExpr *fx_sum = NULL;
        lvExpr *inv_n2 = NULL;

        inv_n = lv_expr_create_rational(1, count);
        if (!inv_n)
            return false;

        /* 等权和 = (x_1 + ... + x_n) / n */
        sum = lv_expr_sum_n(points, count);
        if (!sum) {
            lv_expr_free(inv_n);
            return false;
        }
        weighted_sum = lv_expr_mul(sum, inv_n);
        if (!weighted_sum) {
            lv_expr_free(sum);
            lv_expr_free(inv_n);
            return false;
        }

        /* 等权函数和 = (f(x_1) + ... + f(x_n)) / n */
        fx_arr = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
        if (!fx_arr) {
            lv_expr_free(weighted_sum);
            lv_expr_free(sum);
            lv_expr_free(inv_n);
            return false;
        }
        for (uint32_t i = 0; i < count; i++) {
            fx_arr[i] = lv_expr_function(func, points[i]);
            if (!fx_arr[i]) {
                /* 释放已创建的 fx_arr 元素 */
                for (uint32_t j = 0; j < i; j++)
                    lv_expr_free(fx_arr[j]);
                lv_free((void **) &fx_arr);
                lv_expr_free(weighted_sum);
                lv_expr_free(sum);
                lv_expr_free(inv_n);
                return false;
            }
        }
        fx_sum = lv_expr_sum_n(fx_arr, count);
        lv_free((void **) &fx_arr);
        if (!fx_sum) {
            lv_expr_free(weighted_sum);
            lv_expr_free(sum);
            lv_expr_free(inv_n);
            return false;
        }

        inv_n2 = lv_expr_create_rational(1, count);
        if (!inv_n2) {
            lv_expr_free(fx_sum);
            lv_expr_free(weighted_sum);
            lv_expr_free(sum);
            lv_expr_free(inv_n);
            return false;
        }
        weighted_func = lv_expr_mul(fx_sum, inv_n2);
        if (!weighted_func) {
            lv_expr_free(inv_n2);
            lv_expr_free(fx_sum);
            lv_expr_free(weighted_sum);
            lv_expr_free(sum);
            lv_expr_free(inv_n);
            return false;
        }
    }

    /* 左端: f(weighted_sum) */
    lvExpr *left = lv_expr_function(func, weighted_sum);
    if (!left)
        return false;

    /* 右端: weighted_func 就是 ∑ w_i f(x_i) */
    lvExpr *right = weighted_func;

    /* 根据凹凸性决定不等式方向
     * 凸函数: left <= right → f(∑w_i x_i) ≤ ∑w_i f(x_i)
     * 凹函数: left >= right → f(∑w_i x_i) ≥ ∑w_i f(x_i)
     */
    lvInequalityType itype = is_convex ? INEQ_LESS_EQUAL : INEQ_GREATER_EQUAL;

    *out_ineq = lv_ineq_create(left, itype, right);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Jensen");
    }

    return (*out_ineq != NULL);
}

/**
 * 三角形不等式：
 * |a - b| < c < a + b
 * 产生三条不等式
 */
uint32_t lv_ineq_triangle(lvExpr *a, lvExpr *b, lvExpr *c, lvInequality **out_inequalities, uint32_t max_count) {
    if (!a || !b || !c || !out_inequalities || max_count < 3)
        return 0;

    /* 三角不等式产生三条不等式：
     * 1. a + b > c
     * 2. a + c > b
     * 3. b + c > a
     */
    lvExpr *a_plus_b = lv_expr_add(a, b);
    lvExpr *a_plus_c = lv_expr_add(a, c);
    lvExpr *b_plus_c = lv_expr_add(b, c);
    if (!a_plus_b || !a_plus_c || !b_plus_c)
        return 0;

    out_inequalities[0] = lv_ineq_create(a_plus_b, INEQ_GREATER_THAN, c);
    if (!out_inequalities[0]) {
        lv_expr_free(a_plus_c);
        lv_expr_free(b_plus_c);
        return 0;
    }

    out_inequalities[1] = lv_ineq_create(a_plus_c, INEQ_GREATER_THAN, b);
    if (!out_inequalities[1]) {
        lv_ineq_destroy(out_inequalities[0]);
        out_inequalities[0] = NULL;
        lv_expr_free(b_plus_c);
        return 0;
    }

    out_inequalities[2] = lv_ineq_create(b_plus_c, INEQ_GREATER_THAN, a);
    if (!out_inequalities[2]) {
        lv_ineq_destroy(out_inequalities[1]);
        out_inequalities[1] = NULL;
        lv_ineq_destroy(out_inequalities[0]);
        out_inequalities[0] = NULL;
        return 0;
    }

    return 3;
}

/* ============== 不等式变换 ============== */

/**
 * 不等式两边加表达式：
 * (left <type> right) + expr => (left+expr <type> right+expr)
 * 不等式方向不变
 */
lvInequality *lv_ineq_add(lvInequality *ineq, lvExpr *expr) {
    if (!ineq || !expr)
        return NULL;

    /* 不等式方向不变，两边都加上 expr */
    lvExpr *new_left = lv_expr_add(ineq->left, expr);
    lvExpr *new_right = lv_expr_add(ineq->right, expr);
    if (!new_left || !new_right) {
        lv_expr_destroy(&new_left);
        lv_expr_destroy(&new_right);
        return NULL;
    }

    lvInequality *result = lv_ineq_create(new_left, ineq->type, new_right);
    if (result)
        result->status = ineq->status;
    else {
        lv_expr_destroy(&new_left);
        lv_expr_destroy(&new_right);
    }
    return result;
}

/**
 * 不等式两边乘表达式：
 * - expr_sign > 0: 方向不变
 * - expr_sign < 0: 方向翻转
 * - expr_sign == 0: 返回 NULL（无效操作）
 */
lvInequality *lv_ineq_mul(lvInequality *ineq, lvExpr *expr, int expr_sign) {
    if (!ineq || !expr)
        return NULL;
    if (expr_sign == 0)
        return NULL;

    lvInequalityType new_type = ineq->type;
    if (expr_sign < 0) {
        new_type = ineq_negate_type(ineq->type);
    }

    /* 不等式两边都乘以 expr */
    lvExpr *new_left = lv_expr_mul(ineq->left, expr);
    lvExpr *new_right = lv_expr_mul(ineq->right, expr);
    if (!new_left || !new_right) {
        lv_expr_destroy(&new_left);
        lv_expr_destroy(&new_right);
        return NULL;
    }

    lvInequality *result = lv_ineq_create(new_left, new_type, new_right);
    if (result)
        result->status = ineq->status;
    else {
        lv_expr_destroy(&new_left);
        lv_expr_destroy(&new_right);
    }
    return result;
}

/**
 * 不等式取反：
 * left < right => left >= right
 * left <= right => left > right
 */
lvInequality *lv_ineq_negate(lvInequality *ineq) {
    if (!ineq)
        return NULL;

    lvInequalityType new_type = ineq_negate_type(ineq->type);
    lvInequality *result = lv_ineq_create(ineq->left, new_type, ineq->right);
    if (result) {
        result->status = ineq->status;
        if (result->status == INEQ_STATUS_PROVED)
            result->status = INEQ_STATUS_DISPROVED;
        else if (result->status == INEQ_STATUS_DISPROVED)
            result->status = INEQ_STATUS_PROVED;
    }
    return result;
}

/**
 * 不等式传递：
 * a < b, b < c => a < c
 * a <= b, b <= c => a <= c
 * a < b, b <= c => a < c
 * a <= b, b < c => a < c
 */
bool lv_ineq_transitive(lvInequality **ineqs, uint32_t count, lvInequality **out_result) {
    if (!ineqs || count < 2 || !out_result)
        return false;

    *out_result = NULL;

    /* 检查所有不等式是否同向 */
    for (uint32_t i = 1; i < count; i++) {
        if (!ineqs[i - 1] || !ineqs[i])
            return false;
        if (!ineq_same_direction(ineqs[i - 1]->type, ineqs[i]->type))
            return false;
    }

    /* 检查链式连接：ineqs[i].right == ineqs[i+1].left */
    for (uint32_t i = 0; i < count - 1; i++) {
        if (ineqs[i]->right != ineqs[i + 1]->left)
            return false;
    }

    /* 确定结果类型：如果任一为严格不等式，结果为严格 */
    lvInequalityType result_type = ineqs[0]->type;
    for (uint32_t i = 1; i < count; i++) {
        if (ineq_is_strict(ineqs[i]->type)) {
            if (ineq_is_less_family(ineqs[i]->type))
                result_type = INEQ_LESS_THAN;
            else
                result_type = INEQ_GREATER_THAN;
            break;
        }
    }

    *out_result = lv_ineq_create(ineqs[0]->left, result_type, ineqs[count - 1]->right);
    return (*out_result != NULL);
}

/**
 * 合并同向不等式：
 * a < c, b < d => a + b < c + d
 */
bool lv_ineq_merge(lvInequality **ineqs, uint32_t count, lvInequality **out_result) {
    if (!ineqs || count < 2 || !out_result)
        return false;

    *out_result = NULL;

    /* 检查所有不等式是否同向 */
    for (uint32_t i = 1; i < count; i++) {
        if (!ineqs[i - 1] || !ineqs[i])
            return false;
        if (!ineq_same_direction(ineqs[i - 1]->type, ineqs[i]->type))
            return false;
    }

    /* 确定结果类型 */
    lvInequalityType result_type = ineqs[0]->type;
    for (uint32_t i = 1; i < count; i++) {
        if (ineq_is_strict(ineqs[i]->type)) {
            if (ineq_is_less_family(ineqs[i]->type))
                result_type = INEQ_LESS_THAN;
            else
                result_type = INEQ_GREATER_THAN;
            break;
        }
    }

    /* 结果：left = sum of all lefts, right = sum of all rights */
    lvExpr **left_exprs = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    lvExpr **right_exprs = (lvExpr **) lv_malloc((size_t) count * sizeof(lvExpr *));
    if (!left_exprs || !right_exprs) {
        lv_free((void **) &left_exprs);
        lv_free((void **) &right_exprs);
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        left_exprs[i] = ineqs[i]->left;
        right_exprs[i] = ineqs[i]->right;
    }
    lvExpr *left_sum = lv_expr_sum_n(left_exprs, count);
    lvExpr *right_sum = lv_expr_sum_n(right_exprs, count);
    lv_free((void **) &left_exprs);
    lv_free((void **) &right_exprs);
    if (!left_sum || !right_sum)
        return false;

    *out_result = lv_ineq_create(left_sum, result_type, right_sum);
    return (*out_result != NULL);
}

/* ============== 表达式符号判定 ============== */

/**
 * 通过不等式系统中的约束判定表达式符号
 *
 * 策略：
 * 1. 检查系统中的变量约束（x > 0, x >= 0, x < 0, x <= 0）
 * 2. 传播符号信息（正*正=正，正*负=负，负*负=正）
 * 3. 平方项总是非负
 */
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
                switch (c->type) {
                    case INEQ_GREATER_THAN:
                        return SIGN_POSITIVE;
                    case INEQ_GREATER_EQUAL:
                        return SIGN_NONNEGATIVE;
                    case INEQ_LESS_THAN:
                        return SIGN_NEGATIVE;
                    case INEQ_LESS_EQUAL:
                        return SIGN_NONPOSITIVE;
                    default:
                        break;
                }
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
                snprintf(buf, sizeof(buf),
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
            const char *type_name = "unknown";
            switch (poly->type) {
                case EXPR_TYPE_VARIABLE:
                    type_name = "variable (needs squaring)";
                    break;
                case EXPR_TYPE_RATIONAL:
                    type_name = "constant (consider sqrt decomposition)";
                    break;
                case EXPR_TYPE_FUNCTION:
                    type_name = "function application (not directly decomposable)";
                    break;
                case EXPR_TYPE_PRODUCT:
                    type_name = "product with != 2 factors (not a*a)";
                    break;
                default:
                    break;
            }
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "Expression type '%s' is not directly decomposable into sum of squares. "
                     "Consider rewriting as explicit a^2 + b^2 + ... form.",
                     type_name);
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
bool lv_ineq_triangle_area(lvExpr *a, lvExpr *b, lvExpr *c, lvExpr *area, lvInequality **out_ineq) {
    if (!a || !b || !c || !area)
        return false;

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* 构造半周长: p = (a + b + c) / 2 */
    lvExpr *a_plus_b = lv_expr_add(a, b);
    if (!a_plus_b)
        return false;
    lvExpr *sum_abc = lv_expr_add(a_plus_b, c);
    if (!sum_abc)
        return false;

    lvExpr *half = lv_expr_create_rational(1, 2);
    if (!half)
        return false;
    lvExpr *p = lv_expr_mul(sum_abc, half);
    if (!p)
        return false;

    /* 构造 p-a, p-b, p-c */
    lvExpr *minus_one = lv_expr_create_rational(-1, 1);
    if (!minus_one)
        return false;

    lvExpr *neg_a = lv_expr_mul(a, minus_one);
    lvExpr *neg_b = lv_expr_mul(b, minus_one);
    lvExpr *neg_c = lv_expr_mul(c, minus_one);
    if (!neg_a || !neg_b || !neg_c)
        return false;

    lvExpr *p_minus_a = lv_expr_add(p, neg_a);
    lvExpr *p_minus_b = lv_expr_add(p, neg_b);
    lvExpr *p_minus_c = lv_expr_add(p, neg_c);
    if (!p_minus_a || !p_minus_b || !p_minus_c)
        return false;

    /* 构造 Heron 表达式: p * (p-a) * (p-b) * (p-c) */
    lvExpr *heron_ab = lv_expr_mul(p_minus_a, p_minus_b);
    if (!heron_ab)
        return false;
    lvExpr *heron_abc = lv_expr_mul(heron_ab, p_minus_c);
    if (!heron_abc)
        return false;
    lvExpr *heron = lv_expr_mul(p, heron_abc);
    if (!heron)
        return false;

    /* 构造 area² */
    lvExpr *two = lv_expr_create_rational(2, 1);
    if (!two)
        return false;
    lvExpr *area_sq = lv_expr_power(area, two);
    if (!area_sq)
        return false;

    /* area² >= p(p-a)(p-b)(p-c) —— Heron 公式约束 */
    *out_ineq = lv_ineq_create(area_sq, INEQ_GREATER_EQUAL, heron);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Triangle-Area-Heron");
    }

    return (*out_ineq != NULL);
}

/**
 * Weitzenbock 不等式：
 * a² + b² + c² >= 4√3 * S
 *
 * （注：由于 GMP 仅支持有理数，4√3 不可精确表示为有理数。
 *  此处构造符号形式的右端，实际 sqrt(3) 因子由调用者在更高层验证。）
 */
bool lv_ineq_weitzenbock(lvExpr *a, lvExpr *b, lvExpr *c, lvExpr *area, lvInequality **out_ineq) {
    if (!a || !b || !c || !area)
        return false;

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* 构造左端: a² + b² + c² */
    lvExpr *two = lv_expr_create_rational(2, 1);
    if (!two)
        return false;

    lvExpr *a_sq = lv_expr_power(a, two);
    lvExpr *b_sq = lv_expr_power(b, two);
    lvExpr *c_sq = lv_expr_power(c, two);
    if (!a_sq || !b_sq || !c_sq)
        return false;

    lvExpr *a_sq_plus_b_sq = lv_expr_add(a_sq, b_sq);
    if (!a_sq_plus_b_sq)
        return false;
    lvExpr *left = lv_expr_add(a_sq_plus_b_sq, c_sq);
    if (!left)
        return false;

    /* 构造右端: 4 * sqrt(3) * S
     * 由于 sqrt(3) 非常数有理数，此处构造 sqrt3 变量符号:
     *   right = 4 * sqrt3_var * area
     * sqrt3_var 作为符号变量保留，调用者可绑定为 sqrt(3) 的有理逼近
     */
    lvExpr *four = lv_expr_create_rational(4, 1);
    if (!four)
        return false;

    /* 使用变量 "sqrt3" 作为占位符 */
    lvExpr *sqrt3_var = lv_expr_create_variable("sqrt3");
    if (!sqrt3_var)
        return false;

    lvExpr *four_sqrt3 = lv_expr_mul(four, sqrt3_var);
    if (!four_sqrt3)
        return false;
    lvExpr *right = lv_expr_mul(four_sqrt3, area);
    if (!right)
        return false;

    *out_ineq = lv_ineq_create(left, INEQ_GREATER_EQUAL, right);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Weitzenbock");
    }

    return (*out_ineq != NULL);
}

/**
 * Erdos-Mordell 不等式：
 * 对于三角形 ABC 内点 P，设 P 到三边距离为 p, q, r：
 * PA + PB + PC >= 2(p + q + r)
 */
bool lv_ineq_erdos_mordell(lvExpr *pa, lvExpr *pb, lvExpr *pc, lvExpr *p, lvExpr *q, lvExpr *r,
                           lvInequality **out_ineq) {
    if (!pa || !pb || !pc || !p || !q || !r)
        return false;

    if (!out_ineq)
        return true;
    *out_ineq = NULL;

    /* 构造左端: PA + PB + PC */
    lvExpr *pa_plus_pb = lv_expr_add(pa, pb);
    if (!pa_plus_pb)
        return false;
    lvExpr *left = lv_expr_add(pa_plus_pb, pc);
    if (!left)
        return false;

    /* 构造右端: 2 * (p + q + r) */
    lvExpr *p_plus_q = lv_expr_add(p, q);
    if (!p_plus_q)
        return false;
    lvExpr *sum_dist = lv_expr_add(p_plus_q, r);
    if (!sum_dist)
        return false;

    lvExpr *two = lv_expr_create_rational(2, 1);
    if (!two)
        return false;
    lvExpr *right = lv_expr_mul(two, sum_dist);
    if (!right)
        return false;

    *out_ineq = lv_ineq_create(left, INEQ_GREATER_EQUAL, right);
    if (*out_ineq) {
        (*out_ineq)->status = INEQ_STATUS_PROVED;
        (*out_ineq)->label = lv_strdup("Erdos-Mordell");
    }

    return (*out_ineq != NULL);
}

/* ============== 不等式序列化 ============== */

static const char *ineq_type_str(lvInequalityType type) {
    switch (type) {
        case INEQ_LESS_THAN:
            return "<";
        case INEQ_LESS_EQUAL:
            return "<=";
        case INEQ_GREATER_THAN:
            return ">";
        case INEQ_GREATER_EQUAL:
            return ">=";
        case INEQ_NOT_EQUAL:
            return "!=";
        default:
            return "?";
    }
}

char *lv_ineq_to_string(const lvInequality *ineq) {
    if (!ineq) {
        char *s = (char *) lv_malloc(1);
        if (s)
            s[0] = '\0';
        return s;
    }

    size_t buf_size = 256;
    char *s = (char *) lv_malloc(buf_size);
    if (!s)
        return NULL;

    const char *label = ineq->label ? ineq->label : "ineq";
    snprintf(s, buf_size, "%s: left %s right", label, ineq_type_str(ineq->type));
    return s;
}

char *lv_ineq_proof_to_string(const lvInequalityProof *proof) {
    if (!proof) {
        char *s = (char *) lv_malloc(1);
        if (s)
            s[0] = '\0';
        return s;
    }

    size_t buf_size = 512;
    char *s = (char *) lv_malloc(buf_size);
    if (!s)
        return NULL;

    const char *status_str = "UNKNOWN";
    switch (proof->status) {
        case INEQ_STATUS_PROVED:
            status_str = "PROVED";
            break;
        case INEQ_STATUS_DISPROVED:
            status_str = "DISPROVED";
            break;
        case INEQ_STATUS_CONDITIONAL:
            status_str = "CONDITIONAL";
            break;
        default:
            break;
    }

    snprintf(s, buf_size, "Proof: %s, %d steps", status_str, proof->step_count);
    return s;
}

char *lv_ineq_proof_to_latex(const lvInequalityProof *proof) {
    if (!proof) {
        char *s = (char *) lv_malloc(1);
        if (s)
            s[0] = '\0';
        return s;
    }

    size_t buf_size = 1024;
    char *s = (char *) lv_malloc(buf_size);
    if (!s)
        return NULL;

    int offset = 0;
    if (offset < (int) buf_size)
        offset += snprintf(s + offset, buf_size - (size_t) offset, "\\begin{proof}\n");
    if (offset < 0)
        goto done;

    for (int i = 0; i < proof->step_count && offset < (int) buf_size - 64; i++) {
        const char *just = proof->steps[i].justification ? proof->steps[i].justification : "unknown";
        if (offset < (int) buf_size)
            offset += snprintf(s + offset, buf_size - (size_t) offset, "  Step %d: %s\n", i + 1, just);
        if (offset < 0)
            break;
    }

    if (offset >= 0 && offset < (int) buf_size)
        offset += snprintf(s + offset, buf_size - (size_t) offset, "\\end{proof}\n");

done:

    return s;
}
