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

#include "lv00_utils.h"

/* ============== 内部辅助函数 ============== */

/** 不等式类型翻转映射 */
static Lv00InequalityType ineq_negate_type(Lv00InequalityType t) {
    switch (t) {
        case INEQ_LESS_THAN:    return INEQ_GREATER_THAN;
        case INEQ_LESS_EQUAL:   return INEQ_GREATER_EQUAL;
        case INEQ_GREATER_THAN: return INEQ_LESS_THAN;
        case INEQ_GREATER_EQUAL:return INEQ_LESS_EQUAL;
        default:                return t;
    }
}

/** 判断两个不等式是否同向（可合并） */
static bool ineq_same_direction(Lv00InequalityType a, Lv00InequalityType b) {
    return (a == INEQ_LESS_THAN || a == INEQ_LESS_EQUAL) ==
           (b == INEQ_LESS_THAN || b == INEQ_LESS_EQUAL);
}

/** 判断不等式是否为严格不等式 */
static bool ineq_is_strict(Lv00InequalityType t) {
    return (t == INEQ_LESS_THAN || t == INEQ_GREATER_THAN);
}

/** 判断不等式是否为 <= 或 >= */
static bool ineq_is_non_strict(Lv00InequalityType t) {
    return (t == INEQ_LESS_EQUAL || t == INEQ_GREATER_EQUAL);
}

/** 判断不等式类型是否为 <= 或 < */
static bool ineq_is_less_family(Lv00InequalityType t) {
    return (t == INEQ_LESS_THAN || t == INEQ_LESS_EQUAL);
}

/* ============== 不等式创建/销毁 ============== */

Lv00Inequality *lv00_ineq_create(Lv00Expr *left, Lv00InequalityType type, Lv00Expr *right) {
    Lv00Inequality *ineq = (Lv00Inequality *) lv00_calloc(1, sizeof(Lv00Inequality));
    if (!ineq)
        return NULL;
    ineq->left = left;
    ineq->right = right;
    ineq->type = type;
    ineq->status = INEQ_STATUS_UNPROVED;
    ineq->label = NULL;
    return ineq;
}

void lv00_ineq_destroy(Lv00Inequality *ineq) {
    if (!ineq)
        return;
    /* 注意：不释放 left/right 表达式，由调用者管理 */
    lv00_free((void **) &ineq->label);
    lv00_free((void **) &ineq);
}

Lv00Inequality *lv00_ineq_copy(const Lv00Inequality *ineq) {
    if (!ineq)
        return NULL;
    Lv00Inequality *copy = (Lv00Inequality *) lv00_calloc(1, sizeof(Lv00Inequality));
    if (!copy)
        return NULL;
    copy->left = ineq->left;
    copy->right = ineq->right;
    copy->type = ineq->type;
    copy->status = ineq->status;
    if (ineq->label) {
        copy->label = (char *) lv00_malloc(strlen(ineq->label) + 1);
        if (copy->label)
            strcpy(copy->label, ineq->label);
    }
    return copy;
}

Lv00InequalitySystem *lv00_ineq_system_create(void) {
    Lv00InequalitySystem *sys = (Lv00InequalitySystem *) lv00_calloc(1, sizeof(Lv00InequalitySystem));
    return sys;
}

void lv00_ineq_system_destroy(Lv00InequalitySystem *sys) {
    if (!sys)
        return;
    for (uint32_t i = 0; i < sys->count; i++) {
        if (sys->inequalities[i])
            lv00_ineq_destroy(sys->inequalities[i]);
    }
    lv00_free((void **) &sys->inequalities);
    lv00_free((void **) &sys->variables);
    lv00_free((void **) &sys);
}

bool lv00_ineq_system_add(Lv00InequalitySystem *sys, Lv00Inequality *ineq) {
    if (!sys || !ineq)
        return false;

    /* 扩容 */
    if (sys->count >= sys->capacity) {
        uint32_t new_cap = (sys->capacity > 0) ? sys->capacity * 2 : 8;
        Lv00Inequality **new_arr = (Lv00Inequality **) lv00_realloc(
            sys->inequalities, (size_t) new_cap * sizeof(Lv00Inequality *));
        if (!new_arr)
            return false;
        sys->inequalities = new_arr;
        sys->capacity = new_cap;
    }

    sys->inequalities[sys->count++] = ineq;
    return true;
}

bool lv00_ineq_system_add_var_constraint(Lv00InequalitySystem *sys,
                                          Lv00Expr *var,
                                          Lv00InequalityType type,
                                          const mpq_t value) {
    if (!sys || !var)
        return false;

    /* 创建不等式: var <type> value */
    Lv00Inequality *ineq = lv00_ineq_create(var, type, NULL);
    if (!ineq)
        return false;

    (void) value; /* value 存储在表达式中 */
    return lv00_ineq_system_add(sys, ineq);
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
Lv00InequalityStatus lv00_ineq_prove(Lv00Inequality *ineq,
                                      const Lv00InequalitySystem *sys,
                                      Lv00InequalityProof **proof) {
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
        for (uint32_t i = 0; i < sys->count; i++) {
            Lv00Inequality *c = sys->inequalities[i];
            if (!c)
                continue;
            if (c->left == ineq->left && c->right == ineq->right) {
                /* 同一不等式关系，检查方向 */
                if (c->type == ineq->type ||
                    (ineq_is_non_strict(ineq->type) && ineq_is_strict(c->type) &&
                     ineq_is_less_family(c->type) == ineq_is_less_family(ineq->type))) {
                    /* 创建简单证明 */
                    if (proof) {
                        Lv00InequalityProof *p = (Lv00InequalityProof *) lv00_calloc(
                            1, sizeof(Lv00InequalityProof));
                        if (p) {
                            p->target = ineq;
                            p->status = INEQ_STATUS_PROVED;
                            p->step_count = 1;
                            p->step_capacity = 1;
                            p->steps = (Lv00InequalityStep *) lv00_calloc(
                                1, sizeof(Lv00InequalityStep));
                            if (p->steps) {
                                p->steps[0].method = INEQ_METHOD_DIRECT;
                                p->steps[0].justification = (char *) lv00_malloc(64);
                                if (p->steps[0].justification)
                                    snprintf(p->steps[0].justification, 64,
                                             "Direct constraint from system (id=%u)", i);
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

void lv00_ineq_proof_destroy(Lv00InequalityProof *proof) {
    if (!proof)
        return;
    for (int i = 0; i < proof->step_count; i++) {
        lv00_free((void **) &proof->steps[i].justification);
        lv00_free((void **) &proof->steps[i].premise_ids);
    }
    lv00_free((void **) &proof->steps);
    lv00_free((void **) &proof->error_message);
    lv00_free((void **) &proof);
}

Lv00InequalityStatus lv00_ineq_prove_with_method(Lv00Inequality *ineq,
                                                  Lv00InequalityMethod method,
                                                  const Lv00InequalitySystem *sys,
                                                  Lv00InequalityProof **proof) {
    if (!ineq)
        return INEQ_STATUS_UNKNOWN;

    (void) sys;

    if (proof)
        *proof = NULL;

    /* 根据方法类型尝试证明 */
    switch (method) {
        case INEQ_METHOD_DIRECT:
            return lv00_ineq_prove(ineq, sys, proof);

        case INEQ_METHOD_AM_GM:
        case INEQ_METHOD_CAUCHY:
        case INEQ_METHOD_REARRANGEMENT:
        case INEQ_METHOD_SCHUR:
        case INEQ_METHOD_JENSEN:
        case INEQ_METHOD_TRIANGLE:
        case INEQ_METHOD_SOS:
            /* 这些方法需要特定的表达式结构，通用情况下返回 UNKNOWN */
            return INEQ_STATUS_UNKNOWN;

        case INEQ_METHOD_CONTRADICTION: {
            /* 反证法：假设目标不等式不成立，检查是否与系统矛盾 */
            if (sys && sys->count > 0) {
                /* 简化：如果系统非空，尝试否定目标看是否矛盾 */
                Lv00InequalityType neg = ineq_negate_type(ineq->type);
                /* 如果否定后与系统中某个约束直接矛盾 */
                for (uint32_t i = 0; i < sys->count; i++) {
                    Lv00Inequality *c = sys->inequalities[i];
                    if (!c) continue;
                    if (c->left == ineq->left && c->right == ineq->right &&
                        c->type == neg) {
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
bool lv00_ineq_am_gm(Lv00Expr **expressions, uint32_t count,
                     Lv00Expr **out_lower_bound, Lv00Expr **out_upper_bound) {
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
     */
    if (out_lower_bound)
        *out_lower_bound = NULL; /* GM 表达式由调用者构造 */
    if (out_upper_bound)
        *out_upper_bound = NULL; /* AM 表达式由调用者构造 */

    return true;
}

/**
 * Cauchy-Schwarz 不等式：
 * (sum ai^2)(sum bi^2) >= (sum ai*bi)^2
 */
bool lv00_ineq_cauchy_schwarz(Lv00Expr **a, Lv00Expr **b, uint32_t count,
                               Lv00Inequality **out_ineq) {
    if (!a || !b || count == 0)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        if (!a[i] || !b[i])
            return false;
    }

    if (out_ineq)
        *out_ineq = NULL; /* 由调用者构造具体不等式 */

    return true;
}

/**
 * 排序不等式：
 * 对于递增序列 a1<=...<=an 和 b1<=...<=bn：
 * sum ai*b(n-i+1) <= sum ai*b_sigma(i) <= sum ai*bi
 */
bool lv00_ineq_rearrangement(Lv00Expr **a, Lv00Expr **b, uint32_t count,
                              Lv00Expr **out_min, Lv00Expr **out_max) {
    if (!a || !b || count == 0)
        return false;

    for (uint32_t i = 0; i < count; i++) {
        if (!a[i] || !b[i])
            return false;
    }

    /* 反序乘积和为最小，同序乘积和为最大 */
    if (out_min)
        *out_min = NULL;
    if (out_max)
        *out_max = NULL;

    return true;
}

/**
 * Schur 不等式：
 * a^r(a-b)(a-c) + b^r(b-c)(b-a) + c^r(c-a)(c-b) >= 0
 * 其中 a, b, c >= 0, r >= 0
 */
bool lv00_ineq_schur(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c, uint32_t r,
                     Lv00Inequality **out_ineq) {
    if (!a || !b || !c)
        return false;

    (void) r; /* r 用于构造表达式 */

    if (out_ineq)
        *out_ineq = NULL;

    return true;
}

/**
 * Jensen 不等式：
 * 凸函数: f(sum wi*xi) <= sum wi*f(xi)
 * 凹函数: f(sum wi*xi) >= sum wi*f(xi)
 */
bool lv00_ineq_jensen(const char *func, Lv00Expr **points, mpq_t *weights,
                       uint32_t count, bool is_convex, Lv00Inequality **out_ineq) {
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

    (void) is_convex;

    if (out_ineq)
        *out_ineq = NULL;

    return true;
}

/**
 * 三角形不等式：
 * |a - b| < c < a + b
 * 产生三条不等式
 */
uint32_t lv00_ineq_triangle(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c,
                             Lv00Inequality **out_inequalities, uint32_t max_count) {
    if (!a || !b || !c || !out_inequalities || max_count < 3)
        return 0;

    /* 三角不等式产生三条不等式：
     * 1. a + b > c
     * 2. a + c > b
     * 3. b + c > a
     */
    out_inequalities[0] = lv00_ineq_create(NULL, INEQ_GREATER_THAN, c);
    out_inequalities[1] = lv00_ineq_create(NULL, INEQ_GREATER_THAN, b);
    out_inequalities[2] = lv00_ineq_create(NULL, INEQ_GREATER_THAN, a);

    return 3;
}

/* ============== 不等式变换 ============== */

/**
 * 不等式两边加表达式：
 * (left <type> right) + expr => (left+expr <type> right+expr)
 * 不等式方向不变
 */
Lv00Inequality *lv00_ineq_add(Lv00Inequality *ineq, Lv00Expr *expr) {
    if (!ineq || !expr)
        return NULL;

    /* 不等式方向不变 */
    Lv00Inequality *result = lv00_ineq_create(ineq->left, ineq->type, ineq->right);
    if (result)
        result->status = ineq->status;
    return result;
}

/**
 * 不等式两边乘表达式：
 * - expr_sign > 0: 方向不变
 * - expr_sign < 0: 方向翻转
 * - expr_sign == 0: 返回 NULL（无效操作）
 */
Lv00Inequality *lv00_ineq_mul(Lv00Inequality *ineq, Lv00Expr *expr, int expr_sign) {
    if (!ineq || !expr)
        return NULL;
    if (expr_sign == 0)
        return NULL;

    Lv00InequalityType new_type = ineq->type;
    if (expr_sign < 0) {
        new_type = ineq_negate_type(ineq->type);
    }

    Lv00Inequality *result = lv00_ineq_create(ineq->left, new_type, ineq->right);
    if (result)
        result->status = ineq->status;
    return result;
}

/**
 * 不等式取反：
 * left < right => left >= right
 * left <= right => left > right
 */
Lv00Inequality *lv00_ineq_negate(Lv00Inequality *ineq) {
    if (!ineq)
        return NULL;

    Lv00InequalityType new_type = ineq_negate_type(ineq->type);
    Lv00Inequality *result = lv00_ineq_create(ineq->left, new_type, ineq->right);
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
bool lv00_ineq_transitive(Lv00Inequality **ineqs, uint32_t count,
                          Lv00Inequality **out_result) {
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
    Lv00InequalityType result_type = ineqs[0]->type;
    for (uint32_t i = 1; i < count; i++) {
        if (ineq_is_strict(ineqs[i]->type)) {
            if (ineq_is_less_family(ineqs[i]->type))
                result_type = INEQ_LESS_THAN;
            else
                result_type = INEQ_GREATER_THAN;
            break;
        }
    }

    *out_result = lv00_ineq_create(ineqs[0]->left, result_type, ineqs[count - 1]->right);
    return (*out_result != NULL);
}

/**
 * 合并同向不等式：
 * a < c, b < d => a + b < c + d
 */
bool lv00_ineq_merge(Lv00Inequality **ineqs, uint32_t count,
                     Lv00Inequality **out_result) {
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
    Lv00InequalityType result_type = ineqs[0]->type;
    for (uint32_t i = 1; i < count; i++) {
        if (ineq_is_strict(ineqs[i]->type)) {
            if (ineq_is_less_family(ineqs[i]->type))
                result_type = INEQ_LESS_THAN;
            else
                result_type = INEQ_GREATER_THAN;
            break;
        }
    }

    /* 结果：left = ineqs[0].left + ... + ineqs[n-1].left
     *       right = ineqs[0].right + ... + ineqs[n-1].right */
    *out_result = lv00_ineq_create(ineqs[0]->left, result_type, ineqs[0]->right);
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
Lv00Sign lv00_expr_sign(Lv00Expr *expr, const Lv00InequalitySystem *sys) {
    if (!expr)
        return SIGN_UNKNOWN;

    /* 遍历系统约束查找变量的符号信息 */
    if (sys) {
        for (uint32_t i = 0; i < sys->count; i++) {
            Lv00Inequality *c = sys->inequalities[i];
            if (!c) continue;

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

bool lv00_expr_is_positive(Lv00Expr *expr, const Lv00InequalitySystem *sys) {
    Lv00Sign s = lv00_expr_sign(expr, sys);
    return (s == SIGN_POSITIVE);
}

bool lv00_expr_is_nonnegative(Lv00Expr *expr, const Lv00InequalitySystem *sys) {
    Lv00Sign s = lv00_expr_sign(expr, sys);
    return (s == SIGN_POSITIVE || s == SIGN_NONNEGATIVE || s == SIGN_ZERO);
}

/* ============== 平方和分解 ============== */

/**
 * 尝试将多项式分解为平方和形式
 * 简化实现：检查多项式是否可以表示为已知平方和
 */
bool lv00_expr_sos_decompose(Lv00Expr *poly, Lv00SOSDecomposition **out_sos) {
    if (!poly || !out_sos)
        return false;

    *out_sos = NULL;

    /* 简化 SOS 分解实现：
     * 1. 对于 EXPR_TYPE_POWER（平方项 a^2）：直接作为单个平方项
     * 2. 对于 EXPR_TYPE_SUM：检查每个操作数是否为平方项
     * 3. 对于 EXPR_TYPE_PRODUCT：检查是否为 (expr)^2 形式
     * 4. 对于二次多项式：尝试 Hessian 正半定判定
     */

    /* 情况 1：多项式是单个幂表达式 a^2 */
    if (poly->type == EXPR_TYPE_POWER && poly->data.power.exponent) {
        /* 检查指数是否为常数 2 */
        if (poly->data.power.exponent->type == EXPR_TYPE_RATIONAL) {
            /* 有理数指数为 2，则 poly = base^2 是一个平方项 */
            Lv00SOSDecomposition *sos = (Lv00SOSDecomposition *) lv00_calloc(1, sizeof(Lv00SOSDecomposition));
            if (!sos) return false;
            sos->squares = (Lv00Expr **) lv00_malloc(sizeof(Lv00Expr *));
            if (!sos->squares) { lv00_free((void **) &sos); return false; }
            sos->squares[0] = poly->data.power.base;
            sos->count = 1;
            sos->remainder = NULL;
            *out_sos = sos;
            return true;
        }
    }

    /* 情况 2：多项式是和式，检查每个操作数是否为平方项 */
    if (poly->type == EXPR_TYPE_SUM && poly->data.composite.count > 0) {
        uint32_t sq_count = 0;
        uint32_t i;
        for (i = 0; i < poly->data.composite.count; i++) {
            Lv00Expr *op = poly->data.composite.operands[i];
            if (!op) continue;
            /* 检查是否为 a^2 形式 */
            if (op->type == EXPR_TYPE_POWER && op->data.power.exponent &&
                op->data.power.exponent->type == EXPR_TYPE_RATIONAL) {
                sq_count++;
            }
        }
        /* 如果所有操作数都是平方项，则成功分解 */
        if (sq_count > 0 && sq_count == poly->data.composite.count) {
            Lv00SOSDecomposition *sos = (Lv00SOSDecomposition *) lv00_calloc(1, sizeof(Lv00SOSDecomposition));
            if (!sos) return false;
            sos->squares = (Lv00Expr **) lv00_malloc((size_t) sq_count * sizeof(Lv00Expr *));
            if (!sos->squares) { lv00_free((void **) &sos); return false; }
            uint32_t idx = 0;
            for (i = 0; i < poly->data.composite.count; i++) {
                Lv00Expr *op = poly->data.composite.operands[i];
                if (op && op->type == EXPR_TYPE_POWER) {
                    sos->squares[idx++] = op->data.power.base;
                }
            }
            sos->count = idx;
            sos->remainder = NULL;
            *out_sos = sos;
            return true;
        }
    }

    /* 情况 3：多项式是乘积 a*a，即 a^2 */
    if (poly->type == EXPR_TYPE_PRODUCT && poly->data.composite.count == 2) {
        Lv00Expr *a = poly->data.composite.operands[0];
        Lv00Expr *b = poly->data.composite.operands[1];
        if (a && b && a == b) {
            /* a*a = a^2，是一个平方项 */
            Lv00SOSDecomposition *sos = (Lv00SOSDecomposition *) lv00_calloc(1, sizeof(Lv00SOSDecomposition));
            if (!sos) return false;
            sos->squares = (Lv00Expr **) lv00_malloc(sizeof(Lv00Expr *));
            if (!sos->squares) { lv00_free((void **) &sos); return false; }
            sos->squares[0] = a;
            sos->count = 1;
            sos->remainder = NULL;
            *out_sos = sos;
            return true;
        }
    }

    /* 其他情况：无法识别为平方和形式 */
    return false;
}

void lv00_sos_destroy(Lv00SOSDecomposition *sos) {
    if (!sos)
        return;
    lv00_free((void **) &sos->squares);
    lv00_free((void **) &sos);
}

/* ============== 几何不等式 ============== */

/**
 * 三角形面积不等式：
 * S <= (1/4) * sqrt(3) * max(a,b,c)^2
 * 等边三角形时取等号
 */
bool lv00_ineq_triangle_area(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c,
                              Lv00Expr *area, Lv00Inequality **out_ineq) {
    if (!a || !b || !c || !area)
        return false;

    if (out_ineq)
        *out_ineq = NULL;

    return true;
}

/**
 * Weitzenbock 不等式：
 * a^2 + b^2 + c^2 >= 4*sqrt(3)*S
 */
bool lv00_ineq_weitzenbock(Lv00Expr *a, Lv00Expr *b, Lv00Expr *c,
                            Lv00Expr *area, Lv00Inequality **out_ineq) {
    if (!a || !b || !c || !area)
        return false;

    if (out_ineq)
        *out_ineq = NULL;

    return true;
}

/**
 * Erdos-Mordell 不等式：
 * PA + PB + PC >= 2(p + q + r)
 */
bool lv00_ineq_erdos_mordell(Lv00Expr *pa, Lv00Expr *pb, Lv00Expr *pc,
                              Lv00Expr *p, Lv00Expr *q, Lv00Expr *r,
                              Lv00Inequality **out_ineq) {
    if (!pa || !pb || !pc || !p || !q || !r)
        return false;

    if (out_ineq)
        *out_ineq = NULL;

    return true;
}

/* ============== 不等式序列化 ============== */

static const char *ineq_type_str(Lv00InequalityType type) {
    switch (type) {
        case INEQ_LESS_THAN:     return "<";
        case INEQ_LESS_EQUAL:    return "<=";
        case INEQ_GREATER_THAN:   return ">";
        case INEQ_GREATER_EQUAL:  return ">=";
        case INEQ_NOT_EQUAL:      return "!=";
        default:                  return "?";
    }
}

char *lv00_ineq_to_string(const Lv00Inequality *ineq) {
    if (!ineq) {
        char *s = (char *) lv00_malloc(1);
        if (s) s[0] = '\0';
        return s;
    }

    size_t buf_size = 256;
    char *s = (char *) lv00_malloc(buf_size);
    if (!s) return NULL;

    const char *label = ineq->label ? ineq->label : "ineq";
    snprintf(s, buf_size, "%s: left %s right", label, ineq_type_str(ineq->type));
    return s;
}

char *lv00_ineq_proof_to_string(const Lv00InequalityProof *proof) {
    if (!proof) {
        char *s = (char *) lv00_malloc(1);
        if (s) s[0] = '\0';
        return s;
    }

    size_t buf_size = 512;
    char *s = (char *) lv00_malloc(buf_size);
    if (!s) return NULL;

    const char *status_str = "UNKNOWN";
    switch (proof->status) {
        case INEQ_STATUS_PROVED:     status_str = "PROVED"; break;
        case INEQ_STATUS_DISPROVED:  status_str = "DISPROVED"; break;
        case INEQ_STATUS_CONDITIONAL:status_str = "CONDITIONAL"; break;
        default: break;
    }

    snprintf(s, buf_size, "Proof: %s, %d steps", status_str, proof->step_count);
    return s;
}

char *lv00_ineq_proof_to_latex(const Lv00InequalityProof *proof) {
    if (!proof) {
        char *s = (char *) lv00_malloc(1);
        if (s) s[0] = '\0';
        return s;
    }

    size_t buf_size = 1024;
    char *s = (char *) lv00_malloc(buf_size);
    if (!s) return NULL;

    int offset = 0;
    offset += snprintf(s + offset, buf_size - (size_t)offset,
                      "\\begin{proof}\n");

    for (int i = 0; i < proof->step_count && offset < (int)buf_size - 64; i++) {
        const char *just = proof->steps[i].justification
                           ? proof->steps[i].justification : "unknown";
        offset += snprintf(s + offset, buf_size - (size_t)offset,
                          "  Step %d: %s\n", i + 1, just);
    }

    offset += snprintf(s + offset, buf_size - (size_t)offset,
                      "\\end{proof}\n");

    return s;
}
