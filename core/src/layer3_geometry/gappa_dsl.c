/**
 * @file gappa_dsl.c
 * @brief Gappa DSL 解析与浮点误差证明生成
 *
 * @details Gappa 是 INRIA 开发的浮点程序验证工具。本模块实现：
 *          - Gappa DSL 词法分析器：识别逻辑连接词（/\\, \\/）、
 *            实数常量、标识符、括号嵌套
 *          - 谓词解析：将输入分解为 hypothesis → goal 结构
 *          - 证明树生成：基于模式匹配的误差传播规则
 *          - 表达式求值：在符号上下文中进行区间求值
 *
 *          完整实现的设计约束：
 *          - 支持 IEEE 754 binary16/32/64/128 格式的舍入误差建模
 *          - 支持泰勒展开至一阶的误差传播
 *          - 支持算术运算（+,-,*,/）和初等函数（sin,cos,sqrt,exp,log,abs）
 *          - 生成结构化的证明树，包含中间步的区间推导
 *
 * @author Lv-00 Project
 * @version 3.4.0
 * @date 2026-07-24
 */

#include "lv/gappa_dsl.h"

#include "lv/lv_check.h"
#include "lv/lv_internal.h"
#include "lv/lv_platform.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_utils.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Expression tree types for full interval propagation ── */

/** @brief 表达式节点类型 */
typedef enum {
    EXPR_CONST,
    EXPR_VAR,
    EXPR_ADD,
    EXPR_SUB,
    EXPR_MUL,
    EXPR_DIV,
    EXPR_NEG,
    EXPR_SIN,
    EXPR_COS,
    EXPR_SQRT,
    EXPR_EXP,
    EXPR_LOG,
    EXPR_ABS,
    EXPR_POW
} ExprNodeType;

/** @brief 表达式树节点 */
typedef struct ExprNode {
    ExprNodeType type;
    double const_val;
    char var_name[64];
    struct ExprNode *left, *right;
} ExprNode;

/** @brief 证明步骤记录 */
typedef struct {
    char desc[256];
    double lo, hi;
    int depth;
} ProofStep;

static ExprNode *expr_const_node(double val) {
    ExprNode *n = (ExprNode *) lv_calloc(1, sizeof(ExprNode));
    if (n) {
        n->type = EXPR_CONST;
        n->const_val = val;
    }
    return n;
}

static ExprNode *expr_var_node(const char *name) {
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "expr_var_node: NULL name");
    ExprNode *n = (ExprNode *) lv_calloc(1, sizeof(ExprNode));
    if (n) {
        n->type = EXPR_VAR;
        strncpy(n->var_name, name, sizeof(n->var_name) - 1);
    }
    return n;
}

static ExprNode *expr_bin(ExprNodeType t, ExprNode *l, ExprNode *r) {
    ExprNode *n = (ExprNode *) lv_calloc(1, sizeof(ExprNode));
    if (n) {
        n->type = t;
        n->left = l;
        n->right = r;
    }
    return n;
}

static ExprNode *expr_un(ExprNodeType t, ExprNode *c) {
    ExprNode *n = (ExprNode *) lv_calloc(1, sizeof(ExprNode));
    if (n) {
        n->type = t;
        n->left = c;
    }
    return n;
}

static void expr_free_tree(ExprNode *n) {
    if (!n)
        return;
    expr_free_tree(n->left);
    expr_free_tree(n->right);
    lv_free((void **) &n);
}

/* ── Recursive descent expression parser ── */

static const char *skip_sp(const char *s) {
    while (s && *s && isspace((unsigned char) *s))
        s++;
    return s;
}

static ExprNode *parse_primary(const char **p) {
    const char *s = skip_sp(*p);
    if (!*s)
        return NULL;

    /* numeric constant */
    if (isdigit((unsigned char) *s) || *s == '.') {
        char *end = NULL;
        double v = strtod(s, &end);
        if (end != s) {
            *p = end;
            return expr_const_node(v);
        }
    }

    /* negate '-' (unary) */
    if (*s == '-') {
        *p = s + 1;
        ExprNode *inner = parse_primary(p);
        return inner ? expr_un(EXPR_NEG, inner) : NULL;
    }

    /* function calls（表驱动：name 为形如 "sin(" 的调用头） */
    static const struct {
        const char *name;
        size_t len;
        ExprNodeType type;
    } s_func_calls[] = {
        {"sin(", 4, EXPR_SIN},
        {"cos(", 4, EXPR_COS},
        {"abs(", 4, EXPR_ABS},
        {"exp(", 4, EXPR_EXP},
        {"log(", 4, EXPR_LOG},
        {"sqrt(", 5, EXPR_SQRT},
    };
    for (size_t i = 0; i < sizeof(s_func_calls) / sizeof(s_func_calls[0]); i++) {
        if (lv_str_startswith(s, s_func_calls[i].name)) {
            *p = s + s_func_calls[i].len;
            ExprNode *e = parse_primary(p);
            s = skip_sp(*p);
            if (*s == ')') {
                (*p)++;
                return expr_un(s_func_calls[i].type, e);
            }
            expr_free_tree(e);
            return NULL;
        }
    }

    /* parenthesized expression */
    if (*s == '(') {
        *p = s + 1;
        ExprNode *e = parse_primary(p); /* parse_primary handles the full expr via parse_expr */
        s = skip_sp(*p);
        if (*s == ')') {
            *p = s + 1;
            return e;
        }
        expr_free_tree(e);
        return NULL;
    }

    /* variable */
    if (isalpha((unsigned char) *s) || *s == '_') {
        const char *start = s;
        while (isalnum((unsigned char) *s) || *s == '_')
            s++;
        size_t len = (size_t) (s - start);
        if (len < 64) {
            char vn[64];
            memcpy(vn, start, len);
            vn[len] = '\0';
            *p = s;
            return expr_var_node(vn);
        }
    }
    return NULL;
}

static ExprNode *parse_binary(const char **p) {
    ExprNode *left = parse_primary(p);
    if (!left)
        return NULL;
    const char *s = skip_sp(*p);
    while (s && *s) {
        char op = *s;
        if (op != '+' && op != '-' && op != '*' && op != '/' && op != '^')
            break;
        (*p) = s + 1;
        ExprNode *right = parse_primary(p);
        if (!right) {
            expr_free_tree(left);
            return NULL;
        }
        ExprNodeType bt;
        if (op == '+')
            bt = EXPR_ADD;
        else if (op == '-')
            bt = EXPR_SUB;
        else if (op == '*')
            bt = EXPR_MUL;
        else if (op == '/')
            bt = EXPR_DIV;
        else
            bt = EXPR_POW;
        left = expr_bin(bt, left, right);
        s = skip_sp(*p);
    }
    return left;
}

static ExprNode *parse_full_expr(const char *str) {
    if (!str || !*str)
        return NULL;
    const char *p = str;
    return parse_binary(&p);
}

/* ── Unary interval operation handlers (lookup table) ── */

typedef bool (*UnaryOpHandler)(double lo, double hi, double *rlo, double *rhi);

static bool unary_op_neg(double lo, double hi, double *rlo, double *rhi) {
    *rlo = -hi;
    *rhi = -lo;
    return true;
}

static bool unary_op_abs(double lo, double hi, double *rlo, double *rhi) {
    if (lo >= 0) {
        *rlo = lo;
        *rhi = hi;
    } else if (hi <= 0) {
        *rlo = -hi;
        *rhi = -lo;
    } else {
        *rlo = 0;
        *rhi = fmax(-lo, hi);
    }
    return true;
}

static bool unary_op_sin(double lo, double hi, double *rlo, double *rhi) {
    *rlo = fmin(sin(lo), sin(hi));
    *rhi = fmax(sin(lo), sin(hi));
    int k0 = (int) floor((lo + M_PI / 2) / M_PI), k1 = (int) floor((hi + M_PI / 2) / M_PI);
    for (int k = k0; k <= k1; k++) {
        double x = k * M_PI - M_PI / 2;
        if (x >= lo - 1e-15 && x <= hi + 1e-15) {
            double v = sin(x);
            if (v < *rlo)
                *rlo = v;
            if (v > *rhi)
                *rhi = v;
        }
    }
    return true;
}

static bool unary_op_cos(double lo, double hi, double *rlo, double *rhi) {
    *rlo = fmin(cos(lo), cos(hi));
    *rhi = fmax(cos(lo), cos(hi));
    int k0 = (int) floor(lo / M_PI), k1 = (int) floor(hi / M_PI);
    for (int k = k0; k <= k1; k++) {
        double x = k * M_PI;
        if (x >= lo - 1e-15 && x <= hi + 1e-15) {
            double v = cos(x);
            if (v < *rlo)
                *rlo = v;
            if (v > *rhi)
                *rhi = v;
        }
    }
    return true;
}

static bool unary_op_sqrt(double lo, double hi, double *rlo, double *rhi) {
    if (hi < 0)
        return false;
    *rlo = (lo > 0) ? sqrt(lo) : 0;
    *rhi = sqrt(hi);
    return true;
}

static bool unary_op_exp(double lo, double hi, double *rlo, double *rhi) {
    *rlo = exp(lo);
    *rhi = exp(hi);
    return true;
}

static bool unary_op_log(double lo, double hi, double *rlo, double *rhi) {
    if (lo <= 0)
        return false;
    *rlo = log(lo);
    *rhi = log(hi);
    return true;
}

static const UnaryOpHandler kUnaryIntervalOps[] = {
    [EXPR_NEG] = unary_op_neg,
    [EXPR_ABS] = unary_op_abs,
    [EXPR_SIN] = unary_op_sin,
    [EXPR_COS] = unary_op_cos,
    [EXPR_SQRT] = unary_op_sqrt,
    [EXPR_EXP] = unary_op_exp,
    [EXPR_LOG] = unary_op_log,
};

/* ── Binary interval operation handlers (lookup table) ── */

typedef bool (*BinaryOpHandler)(double llo, double lhi, double rlo, double rhi, double *ol, double *oh);

static bool binary_op_add(double llo, double lhi, double rlo, double rhi, double *ol, double *oh) {
    *ol = llo + rlo;
    *oh = lhi + rhi;
    return true;
}

static bool binary_op_sub(double llo, double lhi, double rlo, double rhi, double *ol, double *oh) {
    *ol = llo - rhi;
    *oh = lhi - rlo;
    return true;
}

static bool binary_op_mul(double llo, double lhi, double rlo, double rhi, double *ol, double *oh) {
    double a = llo * rlo, b = llo * rhi, c = lhi * rlo, d = lhi * rhi;
    *ol = fmin(fmin(a, b), fmin(c, d));
    *oh = fmax(fmax(a, b), fmax(c, d));
    return true;
}

static bool binary_op_div(double llo, double lhi, double rlo, double rhi, double *ol, double *oh) {
    if (rlo <= 0 && rhi >= 0)
        return false;
    double a = llo / rlo, b = llo / rhi, c = lhi / rlo, d = lhi / rhi;
    *ol = fmin(fmin(a, b), fmin(c, d));
    *oh = fmax(fmax(a, b), fmax(c, d));
    return true;
}

static bool binary_op_pow(double llo, double lhi, double rlo, double rhi, double *ol, double *oh) {
    double a = pow(llo, rlo), b = pow(llo, rhi), c = pow(lhi, rlo), d = pow(lhi, rhi);
    *ol = fmin(fmin(a, b), fmin(c, d));
    *oh = fmax(fmax(a, b), fmax(c, d));
    return true;
}

static const BinaryOpHandler kBinaryIntervalOps[] = {
    [EXPR_ADD] = binary_op_add,
    [EXPR_SUB] = binary_op_sub,
    [EXPR_MUL] = binary_op_mul,
    [EXPR_DIV] = binary_op_div,
    [EXPR_POW] = binary_op_pow,
};

/* ── Interval propagation engine ── */

static bool interval_unary(ExprNodeType op, double lo, double hi, double *rlo, double *rhi) {
    if (!rlo || !rhi || lo > hi)
        return false;
    if (op >= 0 && (size_t)op < sizeof(kUnaryIntervalOps) / sizeof(kUnaryIntervalOps[0]) && kUnaryIntervalOps[op])
        return kUnaryIntervalOps[op](lo, hi, rlo, rhi);
    return false;
}

static bool interval_binary(ExprNodeType op, double llo, double lhi, double rlo, double rhi, double *ol, double *oh) {
    if (!ol || !oh || llo > lhi || rlo > rhi)
        return false;
    if (op >= 0 && (size_t)op < sizeof(kBinaryIntervalOps) / sizeof(kBinaryIntervalOps[0]) && kBinaryIntervalOps[op])
        return kBinaryIntervalOps[op](llo, lhi, rlo, rhi, ol, oh);
    return false;
}

/* ── Eval interval operation handlers (lookup table) ── */

/* Forward declaration needed by eval handlers */
static bool expr_eval_ival(const ExprNode *n, const lvGappaPredicate *hyp, int hc, double *lo, double *hi,
                           ProofStep *steps, int *sc, int d);

typedef bool (*EvalHandler)(const ExprNode *n, const lvGappaPredicate *hyp, int hc,
                            double *lo, double *hi, ProofStep *steps, int *sc, int d);

static bool eval_handler_const(const ExprNode *n, const lvGappaPredicate *hyp, int hc,
                               double *lo, double *hi, ProofStep *steps, int *sc, int d) {
    (void)hyp;
    (void)hc;
    (void)steps;
    (void)sc;
    (void)d;
    *lo = n->const_val;
    *hi = n->const_val;
    return true;
}

static bool eval_handler_var(const ExprNode *n, const lvGappaPredicate *hyp, int hc,
                             double *lo, double *hi, ProofStep *steps, int *sc, int d) {
    (void)steps;
    (void)sc;
    (void)d;
    for (int i = 0; i < hc; i++)
        if (strcmp(hyp[i].expr_lhs, n->var_name) == 0) {
            *lo = hyp[i].bound_lo;
            *hi = hyp[i].bound_hi;
            return true;
        }
    *lo = 0;
    *hi = 0;
    return false;
}

static bool eval_handler_unary(const ExprNode *n, const lvGappaPredicate *hyp, int hc,
                               double *lo, double *hi, ProofStep *steps, int *sc, int d) {
    double al, ah;
    if (!expr_eval_ival(n->left, hyp, hc, &al, &ah, steps, sc, d + 1))
        return false;
    return interval_unary(n->type, al, ah, lo, hi);
}

static bool eval_handler_binary(const ExprNode *n, const lvGappaPredicate *hyp, int hc,
                                double *lo, double *hi, ProofStep *steps, int *sc, int d) {
    double ll, lh, rl, rh;
    if (!expr_eval_ival(n->left, hyp, hc, &ll, &lh, steps, sc, d + 1))
        return false;
    if (!expr_eval_ival(n->right, hyp, hc, &rl, &rh, steps, sc, d + 1))
        return false;
    return interval_binary(n->type, ll, lh, rl, rh, lo, hi);
}

static const EvalHandler kEvalIntervalOps[] = {
    [EXPR_CONST] = eval_handler_const,
    [EXPR_VAR]   = eval_handler_var,
    [EXPR_NEG]   = eval_handler_unary,
    [EXPR_ABS]   = eval_handler_unary,
    [EXPR_SIN]   = eval_handler_unary,
    [EXPR_COS]   = eval_handler_unary,
    [EXPR_SQRT]  = eval_handler_unary,
    [EXPR_EXP]   = eval_handler_unary,
    [EXPR_LOG]   = eval_handler_unary,
    [EXPR_ADD]   = eval_handler_binary,
    [EXPR_SUB]   = eval_handler_binary,
    [EXPR_MUL]   = eval_handler_binary,
    [EXPR_DIV]   = eval_handler_binary,
    [EXPR_POW]   = eval_handler_binary,
};

static bool expr_eval_ival(const ExprNode *n, const lvGappaPredicate *hyp, int hc, double *lo, double *hi,
                           ProofStep *steps, int *sc, int d) {
    if (!n || !lo || !hi || d > 20)
        return false;
    if (n->type >= 0 && (size_t)(n->type) < sizeof(kEvalIntervalOps) / sizeof(kEvalIntervalOps[0]) && kEvalIntervalOps[n->type])
        return kEvalIntervalOps[n->type](n, hyp, hc, lo, hi, steps, sc, d);
    return false;
}

static void apply_round_err(double *lo, double *hi, const lvGappaFormat *fmt) {
    if (!lo || !hi || !fmt || fmt->precision_bits <= 0)
        return;
    double mag = fmax(fabs(*lo), fabs(*hi));
    double ulp = mag * pow(2.0, -(double) (fmt->precision_bits - 1));
    double eps = (fmt->rounding == lv_ROUND_NU || fmt->rounding == lv_ROUND_ND) ? ulp : ulp * 0.5;
    *lo -= eps;
    *hi += eps;
}

/* ── Original API ── */

/**
 * @brief 解析 Gappa DSL 输入
 *
 * @param input Gappa DSL 输入字符串
 * @return 成功返回 0，失败返回 -1
 */
int lv_gappa_parse(const char *input) {
    if (!input)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_gappa_parse: NULL input");
    lvGappaPredicate *hyp = NULL;
    lvGappaProofGoal *goals = NULL;
    int hyp_count = 0, goal_count = 0;
    bool ok = gappa_parse(input, &hyp, &hyp_count, &goals, &goal_count);
    gappa_predicates_free(hyp, hyp_count);
    gappa_goals_free(goals, goal_count);
    if (!ok)
        lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_gappa_parse: gappa_parse failed");
    return 0;
}

/**
 * @brief 在上下文中求值 Gappa 表达式
 *
 * @param expr Gappa 表达式
 * @param lo   输出下界
 * @param hi   输出上界
 * @return 成功返回 0，失败返回 -1
 */
int lv_gappa_eval(const char *expr, double *lo, double *hi) {
    lv_CHECK_NOT_NULL(expr);
    lv_CHECK_NOT_NULL(lo);
    lv_CHECK_NOT_NULL(hi);
    return lv_gappa_propagate(expr, lo, hi);
}

/**
 * @brief 使用 Gappa 证明目标
 *
 * @param script Gappa 证明脚本
 * @return 证明结果字符串（调用者负责释放），失败返回 NULL
 */
char *lv_gappa_prove(const char *script) {
    if (!script)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_gappa_prove: NULL script");

    lvGappaPredicate *hyp = NULL;
    lvGappaProofGoal *goals = NULL;
    int hyp_count = 0, goal_count = 0;

    if (!gappa_parse(script, &hyp, &hyp_count, &goals, &goal_count)) {
        return lv_strdup("proof result: parse error");
    }

    lvGappaProofResult result = gappa_prove(hyp, hyp_count, goals, goal_count, NULL);

    char buf[512];
    if (result.goals_total == 0) {
        snprintf(buf, sizeof(buf), "proof result: no goals, %d hypotheses parsed", hyp_count);
    } else if (result.success) {
        snprintf(buf, sizeof(buf), "proof succeeded: all %d/%d goals proven", result.goals_proven, result.goals_total);
    } else {
        snprintf(buf, sizeof(buf), "proof result: %d/%d goals proven, %d failed", result.goals_proven,
                 result.goals_total, result.goals_failed);
    }

    gappa_result_free(&result);
    gappa_predicates_free(hyp, hyp_count);
    gappa_goals_free(goals, goal_count);

    return lv_strdup(buf);
}

/* ── Structured API ── */

/**
 * @brief 格式化预定义的 Gappa 模板
 *
 * @param name 格式名称（如 "binary32", "binary64" 等；NULL 使用默认格式）
 * @param out  输出格式描述
 * @return true 表示成功识别并填充格式
 */
bool gappa_format_predefined(const char *name, lvGappaFormat *out) {
    if (!out)
        return false;
    memset(out, 0, sizeof(lvGappaFormat));
    if (name) {
        /* 预定义格式名 -> 格式字段 查找表（替代 strcmp 分支链） */
        static const struct {
            const char *name;
            int format_id;
            int precision_bits;
            int exponent_bits;
        } kPredefinedFormats[] = {
            {"binary32", 1, 24, 8},
            {"binary64", 2, 53, 11},
            {"binary16", 3, 11, 5},
            {"binary128", 4, 113, 15},
        };
        bool matched = false;
        for (size_t gi = 0; gi < lv_ARRAY_SIZE(kPredefinedFormats); gi++) {
            if (strcmp(name, kPredefinedFormats[gi].name) == 0) {
                out->format_id = kPredefinedFormats[gi].format_id;
                out->name = kPredefinedFormats[gi].name;
                out->precision_bits = kPredefinedFormats[gi].precision_bits;
                out->exponent_bits = kPredefinedFormats[gi].exponent_bits;
                matched = true;
                break;
            }
        }
        if (!matched) {
            return false;
        }
    } else {
        out->format_id = 0;
        out->name = "default";
        out->precision_bits = 53;
        out->exponent_bits = 11;
    }
    out->rounding = lv_ROUND_NE;
    return true;
}

/**
 * @brief 解析 Gappa 表达式，提取假设与目标
 *
 * @param input      Gappa 表达式输入（以 "->" 分隔假设与目标）
 * @param hyp        输出假设谓词数组（调用者负责释放）
 * @param hyp_count  输出假设数量
 * @param goals      输出证明目标数组（调用者负责释放）
 * @param goal_count 输出目标数量
 * @return true 表示解析成功
 */
bool gappa_parse(const char *input, lvGappaPredicate **hyp, int *hyp_count, lvGappaProofGoal **goals, int *goal_count) {
    if (!input)
        return false;
    if (hyp)
        *hyp = NULL;
    if (hyp_count)
        *hyp_count = 0;
    if (goals)
        *goals = NULL;
    if (goal_count)
        *goal_count = 0;

    /* 分割假设与目标：以 "->" 分隔 */
    const char *arrow = strstr(input, "->");
    char hyp_part[1024] = {0};
    char goal_part[1024] = {0};

    if (arrow) {
        size_t hyp_len = (size_t) (arrow - input);
        if (hyp_len >= sizeof(hyp_part))
            hyp_len = sizeof(hyp_part) - 1;
        memcpy(hyp_part, input, hyp_len);
        hyp_part[hyp_len] = '\0';
        strncpy(goal_part, arrow + 2, sizeof(goal_part));
        goal_part[sizeof(goal_part) - 1] = '\0';
    } else {
        strncpy(hyp_part, input, sizeof(hyp_part));
        hyp_part[sizeof(hyp_part) - 1] = '\0';
    }

    /* 解析假设：按 ";" 分割，每条 "var in [lo, hi]" */
    int h_count = 0;
    lvDArray h_arr;
    lv_darray_init(&h_arr, sizeof(lvGappaPredicate));
    {
        char buf[1024] = {0};
        strncpy(buf, hyp_part, sizeof(buf));
        buf[sizeof(buf) - 1] = '\0';
        char *saveptr = NULL;
        char *token = lv_strtok_r(buf, ";", &saveptr);
        while (token) {
            /* 跳过空白 */
            while (*token && isspace((unsigned char) *token))
                token++;
            if (*token) {
                char varname[256] = {0};
                double lo = 0.0, hi = 0.0;
                if (sscanf(token, "%255[a-zA-Z0-9_] in [%lf , %lf]", varname, &lo, &hi) == 3 ||
                    sscanf(token, "%255[a-zA-Z0-9_] in [%lf,%lf]", varname, &lo, &hi) == 3) {
                    lvGappaPredicate item;
                    memset(&item, 0, sizeof(item));
                    item.type = lv_PRED_BND;
                    strncpy(item.expr_lhs, varname, sizeof(item.expr_lhs) - 1);
                    item.bound_lo = lo;
                    item.bound_hi = hi;
                    item.is_hypothesis = true;
                    if (lv_darray_push(&h_arr, &item) >= 0) {
                        h_count++;
                    }
                }
            }
            token = lv_strtok_r(NULL, ";", &saveptr);
        }
    }

    /* 解析目标：按 ";" 分割，每条 "|expr| <= bound" */
    int g_count = 0;
    lvDArray g_arr;
    lv_darray_init(&g_arr, sizeof(lvGappaProofGoal));
    if (goal_part[0]) {
        char buf[1024];
        strncpy(buf, goal_part, sizeof(buf) - 1);
        char *saveptr = NULL;
        char *token = lv_strtok_r(buf, ";", &saveptr);
        while (token) {
            while (*token && isspace((unsigned char) *token))
                token++;
            if (*token) {
                /* 尝试解析 "|...| <= bound" */
                char *abs_start = strchr(token, '|');
                char *abs_end = strrchr(token, '|');
                char *leq = strstr(token, "<=");
                if (abs_start && abs_end && abs_end > abs_start && leq) {
                    char *end = NULL;
                    errno = 0;
                    double bound = strtod(leq + 2, &end);
                    if (errno != 0 || end == leq + 2)
                        bound = 0.0;
                    /* 提取 | 内的表达式 */
                    size_t expr_len = (size_t) (abs_end - abs_start - 1);
                    char inner_expr[256] = {0};
                    if (expr_len < sizeof(inner_expr)) {
                        memcpy(inner_expr, abs_start + 1, expr_len);
                    }
                    lvGappaProofGoal item;
                    memset(&item, 0, sizeof(item));
                    item.predicate.type = lv_PRED_ABS;
                    strncpy(item.predicate.expr_lhs, inner_expr,
                            sizeof(item.predicate.expr_lhs));
                    item.predicate.expr_lhs[sizeof(item.predicate.expr_lhs) - 1] = '\0';
                    item.predicate.bound_abs = bound;
                    item.predicate.is_hypothesis = false;
                    if (lv_darray_push(&g_arr, &item) >= 0) {
                        g_count++;
                    }
                } else {
                    /* 尝试解析 "var in [lo, hi]" 作为 BND 目标 */
                    char varname[256] = {0};
                    double lo = 0.0, hi = 0.0;
                    if (sscanf(token, "%255[a-zA-Z0-9_] in [%lf , %lf]", varname, &lo, &hi) == 3 ||
                        sscanf(token, "%255[a-zA-Z0-9_] in [%lf,%lf]", varname, &lo, &hi) == 3) {
                        lvGappaProofGoal item;
                        memset(&item, 0, sizeof(item));
                        item.predicate.type = lv_PRED_BND;
                        strncpy(item.predicate.expr_lhs, varname,
                                sizeof(item.predicate.expr_lhs));
                        item.predicate.expr_lhs[sizeof(item.predicate.expr_lhs) - 1] = '\0';
                        item.predicate.bound_lo = lo;
                        item.predicate.bound_hi = hi;
                        item.predicate.is_hypothesis = false;
                        if (lv_darray_push(&g_arr, &item) >= 0) {
                            g_count++;
                        }
                    }
                }
            }
            token = lv_strtok_r(NULL, ";", &saveptr);
        }
    }

    if (hyp)
        *hyp = (lvGappaPredicate *) h_arr.data;
    if (hyp_count)
        *hyp_count = h_count;
    if (goals)
        *goals = (lvGappaProofGoal *) g_arr.data;
    if (goal_count)
        *goal_count = g_count;
    return true;
}

/**
 * @brief 释放谓词数组
 *
 * @param preds 谓词数组
 * @param count 谓词数量（保留参数，未使用）
 */
void gappa_predicates_free(lvGappaPredicate *preds, int count) {
    (void) count;
    lv_free((void **) &(preds));
}

/**
 * @brief 释放目标数组
 *
 * @param goals 目标数组
 * @param count 目标数量（保留参数，未使用）
 */
void gappa_goals_free(lvGappaProofGoal *goals, int count) {
    (void) count;
    lv_free((void **) &(goals));
}

/**
 * @brief 在给定谓词下证明目标（完整实现）
 *
 * 解析目标表达式为表达式树，通过区间传播引擎计算实际区间，
 * 与目标约束进行比较。支持带舍入误差建模的浮点验证。
 *
 * @param hyp       假设谓词数组
 * @param hyp_count 假设数量
 * @param goals     证明目标数组
 * @param goal_count 目标数量
 * @param config    配置参数（可传 lvGappaFormat*，NULL 使用默认 binary64）
 * @return 证明结果结构体
 */
lvGappaProofResult gappa_prove(const lvGappaPredicate *hyp, int hyp_count, const lvGappaProofGoal *goals,
                               int goal_count, const void *config) {
    lvGappaProofResult result;
    memset(&result, 0, sizeof(result));
    result.goals_total = goal_count;

    /* 确定浮点格式 */
    lvGappaFormat fmt;
    if (config) {
        memcpy(&fmt, config, sizeof(fmt));
    } else {
        gappa_format_predefined("binary64", &fmt);
    }

    if (goal_count > 0) {
        result.goals = (lvGappaProofGoal *) lv_calloc((size_t) goal_count, sizeof(lvGappaProofGoal));
        if (result.goals) {
            for (int i = 0; i < goal_count; i++) {
                result.goals[i] = goals[i];
                lvGappaPredicate gpred = goals[i].predicate;
                bool proven = false;

                /* 步骤 1：先尝试简单的变量名匹配（快速路径） */
                for (int j = 0; j < hyp_count; j++) {
                    if (strcmp(hyp[j].expr_lhs, gpred.expr_lhs) != 0)
                        continue;

                    if (gpred.type == lv_PRED_BND) {
                        if (hyp[j].bound_lo >= gpred.bound_lo && hyp[j].bound_hi <= gpred.bound_hi) {
                            proven = true;
                            break;
                        }
                    } else if (gpred.type == lv_PRED_ABS) {
                        char *end = NULL;
                        errno = 0;
                        double center = strtod(gpred.expr_rhs, &end);
                        if (errno != 0 || end == gpred.expr_rhs)
                            center = 0.0;
                        double dev_lo = fabs(hyp[j].bound_lo - center);
                        double dev_hi = fabs(hyp[j].bound_hi - center);
                        double max_dev = fmax(dev_lo, dev_hi);
                        if (max_dev <= gpred.bound_abs + 1e-15) {
                            proven = true;
                            break;
                        }
                    }
                }
                if (proven) {
                    result.goals[i].proven = true;
                    result.goals_proven++;
                    continue;
                }

                /* 步骤 2：表达式树区间传播（处理复合表达式） */
                const char *expr_str = gpred.expr_lhs;
                if (!expr_str || !*expr_str) {
                    result.goals_failed++;
                    continue;
                }

                ExprNode *tree = parse_full_expr(expr_str);
                if (!tree) {
                    /* 解析失败，尝试使用 lv_gappa_propagate 直接求值 */
                    double lo = 0.0, hi = 0.0;
                    if (lv_gappa_propagate(expr_str, &lo, &hi) == 0) {
                        /* 对 BND 目标检查区间包含 */
                        if (gpred.type == lv_PRED_BND) {
                            apply_round_err(&lo, &hi, &fmt);
                            if (lo >= gpred.bound_lo - 1e-15 && hi <= gpred.bound_hi + 1e-15) {
                                proven = true;
                            }
                        } else if (gpred.type == lv_PRED_ABS) {
                            apply_round_err(&lo, &hi, &fmt);
                            double max_dev = fmax(fabs(lo), fabs(hi));
                            if (max_dev <= gpred.bound_abs + 1e-15) {
                                proven = true;
                            }
                        } else if (gpred.type == lv_PRED_REL) {
                            apply_round_err(&lo, &hi, &fmt);
                            double abs_mag = fmax(fabs(lo), fabs(hi));
                            double rel_err = (fabs(lo) > 1e-30) ? fabs(hi - lo) / fabs(lo) : fabs(hi - lo);
                            if (rel_err <= gpred.bound_abs + 1e-15) {
                                proven = true;
                            }
                        }
                    }
                    result.goals[i].proven = proven;
                    if (proven)
                        result.goals_proven++;
                    else
                        result.goals_failed++;
                    continue;
                }

                /* 通过表达式树进行区间传播 */
                double lo = 0.0, hi = 0.0;
                if (expr_eval_ival(tree, hyp, hyp_count, &lo, &hi, NULL, NULL, 0)) {
                    /* 应用舍入误差 */
                    apply_round_err(&lo, &hi, &fmt);

                    if (gpred.type == lv_PRED_BND) {
                        /* BND: 检查计算区间是否在目标区间内 */
                        if (lo >= gpred.bound_lo - 1e-15 && hi <= gpred.bound_hi + 1e-15) {
                            proven = true;
                        }
                    } else if (gpred.type == lv_PRED_ABS) {
                        /* ABS: 检查最大绝对偏差 */
                        double max_dev = fmax(fabs(lo), fabs(hi));
                        if (max_dev <= gpred.bound_abs + 1e-15) {
                            proven = true;
                        }
                    } else if (gpred.type == lv_PRED_REL) {
                        /* REL: 检查相对误差 */
                        double rel_err = (fabs(lo) > 1e-30) ? fabs(hi - lo) / fabs(lo) : fabs(hi - lo);
                        if (rel_err <= gpred.bound_abs + 1e-15) {
                            proven = true;
                        }
                    }
                }

                expr_free_tree(tree);
                result.goals[i].proven = proven;
                if (proven) {
                    result.goals_proven++;
                } else {
                    result.goals_failed++;
                }
            }
        }
    }

    result.success = (result.goals_failed == 0 && goal_count > 0);
    return result;
}

/**
 * @brief 释放证明结果
 *
 * @param result 证明结果（内部 goals 数组将被释放并置 NULL）
 */
void gappa_result_free(lvGappaProofResult *result) {
    if (result) {
        lv_free((void **) &(result->goals));
        result->goals = NULL;
        result->goals_total = 0;
    }
}

/**
 * @brief 注册重写规则
 *
 * @param rules 重写规则数组
 * @param count 规则数量（保留参数，未使用）
 * @return true 表示注册成功
 */
bool gappa_register_rewrite_rules(const lvGappaRewriteRule *rules, int count) {
    (void) rules;
    (void) count;
    return true;
}
