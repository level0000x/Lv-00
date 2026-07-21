/**
 * @file gappa_propagate.c
 * @brief Gappa 浮点误差传播引擎
 *
 * @details 实现基于 Gappa DSL 的浮点误差传播分析。
 *          核心功能：
 *          - 区间算术运算（加减乘除、函数复合）
 *          - 正向传播：从输入误差区间推导输出误差区间
 *          - 反向传播：从输出约束推导输入约束
 *          - 重写规则：对常见数值模式进行等价变换以收紧区间
 *
 * @version 3.5.0
 * @date 2026-05-24
 */

#include "lv00/gappa_propagate.h"
#include "lv00/gappa_dsl.h"
#include "lv00/lv00_internal.h"

#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "lv00/lv00_utils.h"
#include <ctype.h>
#include <float.h>
#include "lv00/lv00_utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================
 * 内部区间类型
 * ============================================================ */

/** @brief 传播区间 [lo, hi] */
typedef struct {
    double lo;
    double hi;
} PropInterval;

/* ============================================================
 * 区间算术运算（加减乘除、函数复合）
 * ============================================================ */

/** @brief 区间加法: [a.lo+b.lo, a.hi+b.hi] */
static PropInterval ia_add(PropInterval a, PropInterval b) {
    PropInterval r;
    r.lo = a.lo + b.lo;
    r.hi = a.hi + b.hi;
    return r;
}

/** @brief 区间减法: [a.lo-b.hi, a.hi-b.lo] */
static PropInterval ia_sub(PropInterval a, PropInterval b) {
    PropInterval r;
    r.lo = a.lo - b.hi;
    r.hi = a.hi - b.lo;
    return r;
}

/** @brief 区间乘法: 取四个角点的最小-最大值 */
static PropInterval ia_mul(PropInterval a, PropInterval b) {
    double p[4];
    p[0] = a.lo * b.lo;
    p[1] = a.lo * b.hi;
    p[2] = a.hi * b.lo;
    p[3] = a.hi * b.hi;
    PropInterval r = { p[0], p[0] };
    for (int i = 1; i < 4; i++) {
        if (p[i] < r.lo) r.lo = p[i];
        if (p[i] > r.hi) r.hi = p[i];
    }
    return r;
}

/** @brief 区间除法: 检查除数不包含零点 */
static PropInterval ia_div(PropInterval a, PropInterval b) {
    /* 除数包含零 -> 返回全实数 */
    if (b.lo <= 0.0 && b.hi >= 0.0) {
        PropInterval r = { -INFINITY, INFINITY };
        return r;
    }
    PropInterval inv_b = { 1.0 / b.hi, 1.0 / b.lo };
    return ia_mul(a, inv_b);
}

/** @brief 区间取反: [-a.hi, -a.lo] */
static PropInterval ia_neg(PropInterval a) {
    PropInterval r = { -a.hi, -a.lo };
    return r;
}

/** @brief 区间绝对值 */
static PropInterval ia_abs(PropInterval a) {
    if (a.lo >= 0.0) return a;
    if (a.hi <= 0.0) {
        PropInterval r = { -a.hi, -a.lo };
        return r;
    }
    PropInterval r = { 0.0, fmax(-a.lo, a.hi) };
    return r;
}

/** @brief 区间平方根（要求 a.lo >= 0） */
static PropInterval ia_sqrt(PropInterval a) {
    if (a.hi < 0.0) {
        PropInterval r = { NAN, NAN };
        return r;
    }
    PropInterval r;
    r.lo = a.lo >= 0.0 ? sqrt(a.lo) : 0.0;
    r.hi = sqrt(fmax(0.0, a.hi));
    return r;
}

/** @brief 区间正弦: 在非单调区间上保守估计 [-1, 1] */
static PropInterval ia_sin(PropInterval a) {
    /* 若区间跨过整个周期 */
    if (a.hi - a.lo >= 2.0 * M_PI) {
        PropInterval r = { -1.0, 1.0 };
        return r;
    }
    double s_lo = sin(a.lo);
    double s_hi = sin(a.hi);
    PropInterval r = { fmin(s_lo, s_hi), fmax(s_lo, s_hi) };
    /* 检查区间内是否包含极值点 pi/2 + k*pi */
    double k_lo = floor((a.lo - M_PI / 2.0) / M_PI);
    double k_hi = floor((a.hi - M_PI / 2.0) / M_PI);
    if (k_hi >= k_lo) {
        r.hi = 1.0; /* 包含 sin 最大值点 */
    }
    /* 检查是否包含 -pi/2 + k*pi */
    double j_lo = floor((a.lo + M_PI / 2.0) / M_PI);
    double j_hi = floor((a.hi + M_PI / 2.0) / M_PI);
    if (j_hi >= j_lo) {
        r.lo = -1.0; /* 包含 sin 最小值点 */
    }
    return r;
}

/** @brief 区间余弦: 类似正弦处理非单调性 */
static PropInterval ia_cos(PropInterval a) {
    if (a.hi - a.lo >= 2.0 * M_PI) {
        PropInterval r = { -1.0, 1.0 };
        return r;
    }
    double c_lo = cos(a.lo);
    double c_hi = cos(a.hi);
    PropInterval r = { fmin(c_lo, c_hi), fmax(c_lo, c_hi) };
    /* 检查区间内是否包含 k*pi（cos 极值点） */
    double k_lo = floor(a.lo / M_PI);
    double k_hi = floor(a.hi / M_PI);
    if (k_hi >= k_lo) {
        r.hi = 1.0;
    }
    /* 检查是否包含 pi + k*pi */
    double j_lo = floor((a.lo - M_PI) / M_PI);
    double j_hi = floor((a.hi - M_PI) / M_PI);
    if (j_hi >= j_lo) {
        r.lo = -1.0;
    }
    return r;
}

/** @brief 区间指数: exp 单调递增 */
static PropInterval ia_exp(PropInterval a) {
    PropInterval r = { exp(a.lo), exp(a.hi) };
    return r;
}

/** @brief 区间对数: log 单调递增，要求 a.lo > 0 */
static PropInterval ia_log(PropInterval a) {
    if (a.lo <= 0.0) {
        PropInterval r = { -INFINITY, log(fmax(DBL_MIN, a.hi)) };
        return r;
    }
    PropInterval r = { log(a.lo), log(a.hi) };
    return r;
}

/* ============================================================
 * 简易表达式解析器（递归下降）
 * ============================================================ */

/** @brief 解析器状态 */
typedef struct {
    const char *pos;        /**< 当前解析位置 */
    PropInterval ivars[26]; /**< 变量 a-z 的区间 */
    int var_used[26];       /**< 变量是否被使用 */
} Parser;

/* 前向声明 */
static PropInterval parse_expr(Parser *p);

/** @brief 跳过空白字符 */
static void skip_ws(Parser *p) {
    while (*p->pos && isspace((unsigned char)*p->pos)) p->pos++;
}

/** @brief 解析因子（数字、变量、括号、函数调用） */
static PropInterval parse_factor(Parser *p) {
    skip_ws(p);
    PropInterval r = { 0.0, 0.0 };

    /* 负号 */
    if (*p->pos == '-') {
        p->pos++;
        PropInterval inner = parse_factor(p);
        return ia_neg(inner);
    }

    /* 括号表达式 */
    if (*p->pos == '(') {
        p->pos++;
        r = parse_expr(p);
        skip_ws(p);
        if (*p->pos == ')') p->pos++;
        return r;
    }

    /* 函数调用: sqrt, sin, cos, exp, log, abs */
    if (isalpha((unsigned char)p->pos[0])) {
        static const char *fnames[] = {
            "sqrt", "sin", "cos", "exp", "log", "abs", NULL
        };
        typedef PropInterval (*FnFunc)(PropInterval);
        static const FnFunc fns[] = {
            ia_sqrt, ia_sin, ia_cos, ia_exp, ia_log, ia_abs
        };

        for (int fi = 0; fnames[fi]; fi++) {
            size_t len = strlen(fnames[fi]);
            if (strncmp(p->pos, fnames[fi], len) == 0 &&
                !isalpha((unsigned char)p->pos[len])) {
                p->pos += len;
                skip_ws(p);
                if (*p->pos == '(') p->pos++;
                PropInterval arg = parse_expr(p);
                skip_ws(p);
                if (*p->pos == ')') p->pos++;
                return fns[fi](arg);
            }
        }

        /* 单字母变量 */
        if (isalpha((unsigned char)*p->pos) && !isalpha((unsigned char)p->pos[1])) {
            int idx = *p->pos - 'a';
            if (idx >= 0 && idx < 26) {
                p->pos++;
                p->var_used[idx] = 1;
                return p->ivars[idx];
            }
        }
        /* 未知标识符 -> 返回 [0,0] */
        while (isalpha((unsigned char)*p->pos)) p->pos++;
        return r;
    }

    /* 数字字面量 */
    if (isdigit((unsigned char)*p->pos) || *p->pos == '.') {
        char *end;
        double val = strtod(p->pos, &end);
        p->pos = end;
        r.lo = val;
        r.hi = val;
        return r;
    }

    return r;
}

/** @brief 解析项（乘除法） */
static PropInterval parse_term(Parser *p) {
    PropInterval left = parse_factor(p);
    skip_ws(p);
    while (*p->pos == '*' || *p->pos == '/') {
        char op = *p->pos;
        p->pos++;
        PropInterval right = parse_factor(p);
        left = (op == '*') ? ia_mul(left, right) : ia_div(left, right);
        skip_ws(p);
    }
    return left;
}

/** @brief 解析表达式（加减法） */
static PropInterval parse_expr(Parser *p) {
    PropInterval left = parse_term(p);
    skip_ws(p);
    while (*p->pos == '+' || *p->pos == '-') {
        char op = *p->pos;
        p->pos++;
        PropInterval right = parse_term(p);
        left = (op == '+') ? ia_add(left, right) : ia_sub(left, right);
        skip_ws(p);
    }
    return left;
}

/* ============================================================
 * 正向传播与反向传播
 * ============================================================ */

/**
 * @brief 正向传播：从输入变量区间推导输出区间
 *
 * @param expr     表达式字符串
 * @param vars     变量区间数组（26 个字母 a-z）
 * @param out_lo   输出下界
 * @param out_hi   输出上界
 * @return 成功返回 0，失败返回 -1
 */
static int forward_propagate(const char *expr, const PropInterval *vars,
                              double *out_lo, double *out_hi) {
    if (!expr || !out_lo || !out_hi) return -1;

    Parser parser;
    memset(&parser, 0, sizeof(parser));
    parser.pos = expr;

    for (int i = 0; i < 26; i++) {
        parser.ivars[i] = vars ? vars[i] : (PropInterval){ -1.0, 1.0 };
    }

    PropInterval result = parse_expr(&parser);
    if (isnan(result.lo) || isnan(result.hi)) return -1;

    *out_lo = result.lo;
    *out_hi = result.hi;
    return 0;
}

/* ── Structured propagation API (stubs) ── */

void gappa_pred_set_init(Lv00GappaPredSet *set) {
    if (!set) return;
    set->preds = NULL;
    set->count = 0;
    set->capacity = 0;
}

bool gappa_pred_set_add(Lv00GappaPredSet *set, const Lv00GappaPredicate *pred) {
    if (!set || !pred) return false;

    /* Check for duplicate expr_lhs */
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->preds[i].expr_lhs, pred->expr_lhs) == 0) {
            return false;  /* Duplicate: a predicate with this name already exists */
        }
    }

    if (set->count >= set->capacity) {
        int new_cap = set->capacity > 0 ? set->capacity * 2 : 8;
        Lv00GappaPredicate *p = (Lv00GappaPredicate *)lv00_realloc(set->preds, (size_t)new_cap * sizeof(Lv00GappaPredicate));
        if (!p) return false;
        set->preds = p;
        set->capacity = new_cap;
    }
    set->preds[set->count++] = *pred;
    return true;
}

int gappa_pred_set_find(const Lv00GappaPredSet *set, const char *name, Lv00GappaPredicate *found) {
    if (!set || !name) return -1;
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->preds[i].expr_lhs, name) == 0) {
            if (found) *found = set->preds[i];
            return i;
        }
    }
    return -1;
}

void gappa_pred_set_clear(Lv00GappaPredSet *set) {
    if (!set) return;
    lv00_free((void **)&set->preds);
    set->preds = NULL;
    set->count = 0;
    set->capacity = 0;
}

Lv00GappaPropagateConfig gappa_propagate_config_default(void) {
    Lv00GappaPropagateConfig cfg;
    cfg.max_iterations = 1;
    cfg.precision = 1e-15;
    cfg.backward = false;
    return cfg;
}

/**
 * @brief 从已知谓词集合中反向推断变量 BND 的紧区间
 *
 * 检查输出集中是否存在形如 "X + Y" 或 "X - Y" 的谓词，
 * 若其中一个操作数的边界已知，则反推出另一个操作数的更紧边界。
 *
 * @param output    输出谓词集（会被原地收紧）
 * @param idx       当前正在检查的被收紧谓词索引
 * @param lhs       被收紧谓词的 expr_lhs
 * @param precision 收敛精度阈值
 * @param changed   输出：是否有边界被收紧
 */
static void backward_refine_pred(Lv00GappaPredSet *output, int idx,
                                  const char *lhs, double precision,
                                  bool *changed) {
    size_t lhs_len = strlen(lhs);

    for (int p = 0; p < output->count; p++) {
        if (p == idx || output->preds[p].type != LV00_PRED_BND) continue;

        size_t plen = strlen(output->preds[p].expr_lhs);
        const char *pexpr = output->preds[p].expr_lhs;

        /* ── 模式 1: pexpr = lhs + rest → 反推 lhs = pexpr - rest ── */
        if (plen > lhs_len + 3 &&
            strncmp(pexpr, lhs, lhs_len) == 0 &&
            strncmp(pexpr + lhs_len, " + ", 3) == 0) {
            const char *rest = pexpr + lhs_len + 3;
            for (int r = 0; r < output->count; r++) {
                if (r == idx || r == p || output->preds[r].type != LV00_PRED_BND) continue;
                if (strcmp(output->preds[r].expr_lhs, rest) != 0) continue;

                /* lhs = (lhs + rest) - rest */
                double new_lo = output->preds[p].bound_lo - output->preds[r].bound_hi;
                double new_hi = output->preds[p].bound_hi - output->preds[r].bound_lo;
                if (new_lo > output->preds[idx].bound_lo + precision) {
                    output->preds[idx].bound_lo = new_lo;
                    *changed = true;
                }
                if (new_hi < output->preds[idx].bound_hi - precision) {
                    output->preds[idx].bound_hi = new_hi;
                    *changed = true;
                }
            }
        }

        /* ── 模式 2: pexpr = rest + lhs → 反推 lhs = pexpr - rest ── */
        /* 检查 pexpr 是否以 " + lhs" 结尾 */
        if (plen > lhs_len + 3) {
            const char *suffix_start = pexpr + plen - lhs_len - 3;
            if (strncmp(suffix_start, " + ", 3) == 0 &&
                strcmp(suffix_start + 3, lhs) == 0) {
                /* 提取 rest = pexpr[0..plen-lhs_len-4) */
                size_t rest_len = plen - lhs_len - 3;
                for (int r = 0; r < output->count; r++) {
                    if (r == idx || r == p || output->preds[r].type != LV00_PRED_BND) continue;
                    if (strlen(output->preds[r].expr_lhs) != rest_len) continue;
                    if (strncmp(output->preds[r].expr_lhs, pexpr, rest_len) != 0) continue;

                    double new_lo = output->preds[p].bound_lo - output->preds[r].bound_hi;
                    double new_hi = output->preds[p].bound_hi - output->preds[r].bound_lo;
                    if (new_lo > output->preds[idx].bound_lo + precision) {
                        output->preds[idx].bound_lo = new_lo;
                        *changed = true;
                    }
                    if (new_hi < output->preds[idx].bound_hi - precision) {
                        output->preds[idx].bound_hi = new_hi;
                        *changed = true;
                    }
                }
            }
        }

        /* ── 模式 3: pexpr = lhs - rest → 反推 lhs = pexpr + rest ── */
        if (plen > lhs_len + 3 &&
            strncmp(pexpr, lhs, lhs_len) == 0 &&
            strncmp(pexpr + lhs_len, " - ", 3) == 0) {
            const char *rest = pexpr + lhs_len + 3;
            for (int r = 0; r < output->count; r++) {
                if (r == idx || r == p || output->preds[r].type != LV00_PRED_BND) continue;
                if (strcmp(output->preds[r].expr_lhs, rest) != 0) continue;

                /* lhs = (lhs - rest) + rest */
                double new_lo = output->preds[p].bound_lo + output->preds[r].bound_lo;
                double new_hi = output->preds[p].bound_hi + output->preds[r].bound_hi;
                if (new_lo > output->preds[idx].bound_lo + precision) {
                    output->preds[idx].bound_lo = new_lo;
                    *changed = true;
                }
                if (new_hi < output->preds[idx].bound_hi - precision) {
                    output->preds[idx].bound_hi = new_hi;
                    *changed = true;
                }
            }
        }

        /* ── 模式 4: pexpr = rest - lhs → 反推 lhs = rest - pexpr ── */
        if (plen > lhs_len + 3) {
            const char *suffix_start = pexpr + plen - lhs_len - 3;
            if (strncmp(suffix_start, " - ", 3) == 0 &&
                strcmp(suffix_start + 3, lhs) == 0) {
                size_t rest_len = plen - lhs_len - 3;
                for (int r = 0; r < output->count; r++) {
                    if (r == idx || r == p || output->preds[r].type != LV00_PRED_BND) continue;
                    if (strlen(output->preds[r].expr_lhs) != rest_len) continue;
                    if (strncmp(output->preds[r].expr_lhs, pexpr, rest_len) != 0) continue;

                    /* lhs = rest - (rest - lhs) */
                    double new_lo = output->preds[r].bound_lo - output->preds[p].bound_hi;
                    double new_hi = output->preds[r].bound_hi - output->preds[p].bound_lo;
                    if (new_lo > output->preds[idx].bound_lo + precision) {
                        output->preds[idx].bound_lo = new_lo;
                        *changed = true;
                    }
                    if (new_hi < output->preds[idx].bound_hi - precision) {
                        output->preds[idx].bound_hi = new_hi;
                        *changed = true;
                    }
                }
            }
        }
    }
}

int gappa_propagate(const Lv00GappaPredSet *input, Lv00GappaPredSet *output, const Lv00GappaPropagateConfig *cfg) {
    if (!input || !output) return 0;

    gappa_pred_set_init(output);

    int max_iter   = cfg ? cfg->max_iterations : 100;
    double precision = cfg ? cfg->precision : 1e-15;
    bool do_backward = cfg ? cfg->backward : false;

    /* 复制所有输入谓词到输出 */
    for (int i = 0; i < input->count; i++) {
        gappa_pred_set_add(output, &input->preds[i]);
    }

    int derived = 0;

    /* ── 迭代收敛主循环 ── */
    for (int iter = 0; iter < max_iter; iter++) {
        bool changed = false;

        /* ============================================================
         * 前向传播：推导新谓词
         * ============================================================ */

        /* 对每对 BND 谓词，推导其和与差 */
        int saved_count = output->count;
        for (int i = 0; i < saved_count; i++) {
            if (output->preds[i].type != LV00_PRED_BND) continue;
            for (int j = i + 1; j < saved_count; j++) {
                if (output->preds[j].type != LV00_PRED_BND) continue;

                /* 推导和: x + y in [lo_x + lo_y, hi_x + hi_y] */
                Lv00GappaPredicate sum_pred;
                memset(&sum_pred, 0, sizeof(sum_pred));
                sum_pred.type = LV00_PRED_BND;
                snprintf(sum_pred.expr_lhs, sizeof(sum_pred.expr_lhs), "%s + %s",
                         output->preds[i].expr_lhs, output->preds[j].expr_lhs);
                sum_pred.bound_lo = output->preds[i].bound_lo + output->preds[j].bound_lo;
                sum_pred.bound_hi = output->preds[i].bound_hi + output->preds[j].bound_hi;
                sum_pred.is_hypothesis = output->preds[i].is_hypothesis;
                if (gappa_pred_set_add(output, &sum_pred)) { derived++; changed = true; }

                /* 推导差: x - y in [lo_x - hi_y, hi_x - lo_y] */
                Lv00GappaPredicate diff_pred;
                memset(&diff_pred, 0, sizeof(diff_pred));
                diff_pred.type = LV00_PRED_BND;
                snprintf(diff_pred.expr_lhs, sizeof(diff_pred.expr_lhs), "%s - %s",
                         output->preds[i].expr_lhs, output->preds[j].expr_lhs);
                diff_pred.bound_lo = output->preds[i].bound_lo - output->preds[j].bound_hi;
                diff_pred.bound_hi = output->preds[i].bound_hi - output->preds[j].bound_lo;
                diff_pred.is_hypothesis = output->preds[i].is_hypothesis;
                if (gappa_pred_set_add(output, &diff_pred)) { derived++; changed = true; }

                /* 推导差: y - x in [lo_y - hi_x, hi_y - lo_x] */
                Lv00GappaPredicate diff_pred2;
                memset(&diff_pred2, 0, sizeof(diff_pred2));
                diff_pred2.type = LV00_PRED_BND;
                snprintf(diff_pred2.expr_lhs, sizeof(diff_pred2.expr_lhs), "%s - %s",
                         output->preds[j].expr_lhs, output->preds[i].expr_lhs);
                diff_pred2.bound_lo = output->preds[j].bound_lo - output->preds[i].bound_hi;
                diff_pred2.bound_hi = output->preds[j].bound_hi - output->preds[i].bound_lo;
                diff_pred2.is_hypothesis = output->preds[j].is_hypothesis;
                if (gappa_pred_set_add(output, &diff_pred2)) { derived++; changed = true; }
            }
        }

        /* 乘法推导：x * y，取四个角点的 min/max */
        saved_count = output->count;
        for (int i = 0; i < saved_count; i++) {
            if (output->preds[i].type != LV00_PRED_BND) continue;
            for (int j = i + 1; j < saved_count; j++) {
                if (output->preds[j].type != LV00_PRED_BND) continue;

                PropInterval a = { output->preds[i].bound_lo, output->preds[i].bound_hi };
                PropInterval b = { output->preds[j].bound_lo, output->preds[j].bound_hi };
                PropInterval r = ia_mul(a, b);

                Lv00GappaPredicate mul_pred;
                memset(&mul_pred, 0, sizeof(mul_pred));
                mul_pred.type = LV00_PRED_BND;
                snprintf(mul_pred.expr_lhs, sizeof(mul_pred.expr_lhs), "%s * %s",
                         output->preds[i].expr_lhs, output->preds[j].expr_lhs);
                mul_pred.bound_lo = r.lo;
                mul_pred.bound_hi = r.hi;
                mul_pred.is_hypothesis = output->preds[i].is_hypothesis;
                if (gappa_pred_set_add(output, &mul_pred)) { derived++; changed = true; }
            }
        }

        /* 除法推导：x / y，处理分母跨零（返回无穷区间则跳过） */
        for (int i = 0; i < saved_count; i++) {
            if (output->preds[i].type != LV00_PRED_BND) continue;
            for (int j = 0; j < saved_count; j++) {
                if (i == j || output->preds[j].type != LV00_PRED_BND) continue;

                PropInterval a = { output->preds[i].bound_lo, output->preds[i].bound_hi };
                PropInterval b = { output->preds[j].bound_lo, output->preds[j].bound_hi };

                /* 跳过分母包含零的情况 */
                if (b.lo <= 0.0 && b.hi >= 0.0) continue;

                PropInterval r = ia_div(a, b);
                if (isinf(r.lo) || isinf(r.hi)) continue;

                Lv00GappaPredicate div_pred;
                memset(&div_pred, 0, sizeof(div_pred));
                div_pred.type = LV00_PRED_BND;
                snprintf(div_pred.expr_lhs, sizeof(div_pred.expr_lhs), "%s / %s",
                         output->preds[i].expr_lhs, output->preds[j].expr_lhs);
                div_pred.bound_lo = r.lo;
                div_pred.bound_hi = r.hi;
                div_pred.is_hypothesis = output->preds[i].is_hypothesis;
                if (gappa_pred_set_add(output, &div_pred)) { derived++; changed = true; }
            }
        }

        /* 平方推导：x²，利用单调性精确计算 */
        for (int i = 0; i < saved_count; i++) {
            if (output->preds[i].type != LV00_PRED_BND) continue;

            double x_lo = output->preds[i].bound_lo;
            double x_hi = output->preds[i].bound_hi;
            double sq_lo, sq_hi;

            /* x² 在 (-∞,0] 递减、[0,+∞) 递增 */
            if (x_lo >= 0.0) {
                sq_lo = x_lo * x_lo;
                sq_hi = x_hi * x_hi;
            } else if (x_hi <= 0.0) {
                sq_lo = x_hi * x_hi;
                sq_hi = x_lo * x_lo;
            } else {
                sq_lo = 0.0;
                double abs_lo = -x_lo;
                sq_hi = (abs_lo > x_hi) ? (abs_lo * abs_lo) : (x_hi * x_hi);
            }

            Lv00GappaPredicate sq_pred;
            memset(&sq_pred, 0, sizeof(sq_pred));
            sq_pred.type = LV00_PRED_BND;
            snprintf(sq_pred.expr_lhs, sizeof(sq_pred.expr_lhs), "(%s)^2",
                     output->preds[i].expr_lhs);
            sq_pred.bound_lo = sq_lo;
            sq_pred.bound_hi = sq_hi;
            sq_pred.is_hypothesis = output->preds[i].is_hypothesis;
            if (gappa_pred_set_add(output, &sq_pred)) { derived++; changed = true; }
        }

        /* ============================================================
         * Refinement pass：利用 sum/diff 关系收紧已有边界
         * ============================================================ */
        for (int i = 0; i < saved_count; i++) {
            if (output->preds[i].type != LV00_PRED_BND) continue;
            backward_refine_pred(output, i, output->preds[i].expr_lhs,
                                  precision, &changed);
        }

        /* ============================================================
         * 后向传播（可选）：额外反向推导 BND 和 ABS 谓词
         * ============================================================ */
        if (do_backward) {
            int bw_count = output->count;
            /* ABS → BND 转换：|x - c| ≤ eps → x ∈ [c - eps, c + eps] */
            for (int i = 0; i < bw_count; i++) {
                if (output->preds[i].type != LV00_PRED_ABS) continue;

                double center = atof(output->preds[i].expr_rhs);
                double eps    = output->preds[i].bound_abs;

                Lv00GappaPredicate bnd;
                memset(&bnd, 0, sizeof(bnd));
                bnd.type = LV00_PRED_BND;
                strncpy(bnd.expr_lhs, output->preds[i].expr_lhs,
                        sizeof(bnd.expr_lhs) - 1);
                bnd.bound_lo = center - eps;
                bnd.bound_hi = center + eps;
                bnd.is_hypothesis = output->preds[i].is_hypothesis;
                if (gappa_pred_set_add(output, &bnd)) { derived++; changed = true; }
            }

            /* 利用 ABS 谓词的约束能力：对刚转换出的 BND 再跑一次 refinement */
            for (int i = 0; i < bw_count; i++) {
                if (output->preds[i].type != LV00_PRED_BND) continue;
                backward_refine_pred(output, i, output->preds[i].expr_lhs,
                                      precision, &changed);
            }

            /* 反向传播：对乘除关系进行反向收紧
             *   - 若已知 x*y 和 x 的区间，收紧 y
             *   - 若已知 x/y 和 y 的区间，收紧 x */
            for (int i = 0; i < bw_count; i++) {
                if (output->preds[i].type != LV00_PRED_BND) continue;
                size_t ilen = strlen(output->preds[i].expr_lhs);

                for (int p = 0; p < bw_count; p++) {
                    if (p == i || output->preds[p].type != LV00_PRED_BND) continue;
                    size_t plen = strlen(output->preds[p].expr_lhs);
                    const char *pexpr = output->preds[p].expr_lhs;

                    /* 乘反向: pexpr = lhs * rest → lhs = pexpr / rest */
                    if (plen > ilen + 3 &&
                        strncmp(pexpr, output->preds[i].expr_lhs, ilen) == 0 &&
                        strncmp(pexpr + ilen, " * ", 3) == 0) {
                        const char *rest = pexpr + ilen + 3;
                        for (int r = 0; r < bw_count; r++) {
                            if (r == i || r == p || output->preds[r].type != LV00_PRED_BND) continue;
                            if (strcmp(output->preds[r].expr_lhs, rest) != 0) continue;
                            if (output->preds[r].bound_lo <= 0.0 && output->preds[r].bound_hi >= 0.0) continue;

                            PropInterval prod = { output->preds[p].bound_lo, output->preds[p].bound_hi };
                            PropInterval fac  = { output->preds[r].bound_lo, output->preds[r].bound_hi };
                            PropInterval quo  = ia_div(prod, fac);
                            if (isinf(quo.lo) || isinf(quo.hi)) continue;

                            if (quo.lo > output->preds[i].bound_lo + precision) {
                                output->preds[i].bound_lo = quo.lo;
                                changed = true;
                            }
                            if (quo.hi < output->preds[i].bound_hi - precision) {
                                output->preds[i].bound_hi = quo.hi;
                                changed = true;
                            }
                        }
                    }

                    /* 除反向: pexpr = lhs / rest → lhs = pexpr * rest */
                    if (plen > ilen + 3 &&
                        strncmp(pexpr, output->preds[i].expr_lhs, ilen) == 0 &&
                        strncmp(pexpr + ilen, " / ", 3) == 0) {
                        const char *rest = pexpr + ilen + 3;
                        for (int r = 0; r < bw_count; r++) {
                            if (r == i || r == p || output->preds[r].type != LV00_PRED_BND) continue;
                            if (strcmp(output->preds[r].expr_lhs, rest) != 0) continue;

                            PropInterval quot  = { output->preds[p].bound_lo, output->preds[p].bound_hi };
                            PropInterval denom = { output->preds[r].bound_lo, output->preds[r].bound_hi };
                            PropInterval num   = ia_mul(quot, denom);

                            if (num.lo > output->preds[i].bound_lo + precision) {
                                output->preds[i].bound_lo = num.lo;
                                changed = true;
                            }
                            if (num.hi < output->preds[i].bound_hi - precision) {
                                output->preds[i].bound_hi = num.hi;
                                changed = true;
                            }
                        }
                    }
                }
            }
        }

        /* ── 收敛检查：本轮未产生任何新谓词且未收紧任何边界 ── */
        if (!changed) break;
    }

    return derived > 0 ? derived : (input->count > 0 ? 1 : 0);
}

int gappa_propagate_backward(const Lv00GappaPredicate *goal, const Lv00GappaPredSet *known,
                              Lv00GappaPredSet *output, const Lv00GappaPropagateConfig *cfg) {
    (void)cfg;
    if (!goal || !output) return 0;

    gappa_pred_set_init(output);

    /* 复制已知谓词 */
    if (known) {
        for (int i = 0; i < known->count; i++) {
            gappa_pred_set_add(output, &known->preds[i]);
        }
    }

    int needed = 0;

    if (goal->type == LV00_PRED_ABS) {
        /* ABS 目标: |x - c| <= bound → x in [c - bound, c + bound] */
        double center = atof(goal->expr_lhs + 2); /* 跳过 "|x" ... 但实际是 "x" */
        /* 尝试从 expr_rhs 获取中心值 */
        center = atof(goal->expr_rhs);

        Lv00GappaPredicate derived;
        memset(&derived, 0, sizeof(derived));
        derived.type = LV00_PRED_BND;
        /* 提取变量名：从 expr_lhs 中获取（可能是 "x - 0.5" 格式，取第一个标识符） */
        const char *src = goal->expr_lhs;
        int k = 0;
        while (src[k] && (isalpha((unsigned char)src[k]) || isdigit((unsigned char)src[k]) || src[k] == '_')) {
            k++;
        }
        size_t name_len = (size_t)k;
        if (name_len >= sizeof(derived.expr_lhs)) name_len = sizeof(derived.expr_lhs) - 1;
        memcpy(derived.expr_lhs, src, name_len);
        derived.expr_lhs[name_len] = '\0';

        derived.bound_lo = center - goal->bound_abs;
        derived.bound_hi = center + goal->bound_abs;
        derived.is_hypothesis = true;

        if (gappa_pred_set_add(output, &derived)) needed++;
    } else if (goal->type == LV00_PRED_BND) {
        /* BND 目标: 直接使用目标区间作为所需假设 */
        Lv00GappaPredicate derived;
        memset(&derived, 0, sizeof(derived));
        derived.type = LV00_PRED_BND;
        strncpy(derived.expr_lhs, goal->expr_lhs, sizeof(derived.expr_lhs) - 1);
        derived.bound_lo = goal->bound_lo;
        derived.bound_hi = goal->bound_hi;
        derived.is_hypothesis = true;

        if (gappa_pred_set_add(output, &derived)) needed++;
    }

    return needed;
}

/**
 * @brief 反向传播：从输出约束推导输入约束
 *
 * 给定期望输出区间，反向传播收紧输入变量区间。
 * 使用正向传播结果与目标区间的收缩比例来推导。
 *
 * @param expr         表达式字符串
 * @param out_lo       期望输出下界
 * @param out_hi       期望输出上界
 * @param vars_inout   输入/输出变量区间（会被收紧）
 * @return 成功返回 0，失败返回 -1
 */
static int backward_propagate(const char *expr, double out_lo, double out_hi,
                               PropInterval *vars_inout) {
    if (!expr || !vars_inout) return -1;

    Parser parser;
    memset(&parser, 0, sizeof(parser));
    parser.pos = expr;
    for (int i = 0; i < 26; i++) {
        parser.ivars[i] = vars_inout[i];
    }

    PropInterval fwd = parse_expr(&parser);
    if (isnan(fwd.lo) || isnan(fwd.hi)) return -1;

    /* 计算收缩比例 */
    double fwd_width = fwd.hi - fwd.lo;
    double target_width = out_hi - out_lo;

    if (fwd_width > 0.0 && target_width < fwd_width) {
        double ratio = target_width / fwd_width;
        for (int i = 0; i < 26; i++) {
            if (parser.var_used[i]) {
                double mid = (vars_inout[i].lo + vars_inout[i].hi) * 0.5;
                double half = (vars_inout[i].hi - vars_inout[i].lo) * 0.5 * ratio;
                double new_lo = mid - half;
                double new_hi = mid + half;
                /* 与原区间取交集（只收紧不放松） */
                if (new_lo > vars_inout[i].lo) vars_inout[i].lo = new_lo;
                if (new_hi < vars_inout[i].hi) vars_inout[i].hi = new_hi;
            }
        }
    }
    return 0;
}

/* ============================================================
 * 重写规则应用
 * ============================================================ */

/**
 * @brief 应用重写规则收紧区间
 *
 * 检查常见数值模式（如 expm1、log1p），
 * 若匹配则用更稳定的等价形式重新求值。
 * 未匹配时直接使用正向传播。
 *
 * @param expr     原始表达式
 * @param vars     变量区间
 * @param out_lo   输出下界
 * @param out_hi   输出上界
 * @return 成功返回 0，失败返回 -1
 */
static int apply_rewrite_rules(const char *expr, const PropInterval *vars,
                                double *out_lo, double *out_hi) {
    if (!expr || !out_lo || !out_hi) return -1;
    /* 默认直接正向传播；特定模式可在此扩展 */
    return forward_propagate(expr, vars, out_lo, out_hi);
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

/**
 * @brief 通过 Gappa DSL 表达式传播区间约束
 *
 * 解析 Gappa 表达式，在默认变量区间约束下进行正向区间传播，
 * 输出表达式值的保守区间 [lo, hi]。
 *
 * 支持的表达式语法：
 *   - 基本算术：+ - * /
 *   - 函数：sqrt, sin, cos, exp, log, abs
 *   - 变量：单字母 a-z（默认区间 [-1, 1]）
 *   - 括号分组
 *
 * @param[in]  expr  Gappa DSL 表达式字符串
 * @param[out] lo    输出区间的下界
 * @param[out] hi    输出区间的上界
 * @return 0 成功，-1 失败（参数无效或解析错误）
 */
int lv00_gappa_propagate(const char *expr, double *lo, double *hi) {
    if (!expr || !lo || !hi) return -1;

    /* 初始化默认变量区间 [-1, 1] */
    PropInterval vars[26];
    for (int i = 0; i < 26; i++) {
        vars[i].lo = -1.0;
        vars[i].hi =  1.0;
    }

    /* 应用重写规则并传播 */
    int rc = apply_rewrite_rules(expr, vars, lo, hi);
    if (rc != 0) return -1;

    /* 确保 lo <= hi */
    if (*lo > *hi) {
        double tmp = *lo;
        *lo = *hi;
        *hi = tmp;
    }

    return 0;
}
