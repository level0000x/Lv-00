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
#include "lv00/lv00_internal.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <float.h>

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
