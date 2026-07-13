/**
 * @file quadratic.c
 * @brief Quadratic 二次根式类型
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv00/symbolic_coord.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv00/constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "mpz_poly.h"

#define SYM_COORD_DYNAMIC_ARRAY_INIT_CAP 16
#define SYM_COORD_SIGFIGS_MIN_SAFE 6
#define SYM_COORD_SIGFIGS_APPROX 4
#define SYM_COORD_EPS 1e-8
#define SYM_COORD_MAX_REFINE 15
#define SYM_COORD_AMB_MIN_SIGFIGS 3
#define COORD_SEVEN_OVER_FIVE_N 32

/* ── 前向声明 ── */
static int remove_square_factors(int n);
static TranscendentalExpr *transcendental_expr_parse(const char *name);
static void transcendental_expr_destroy(TranscendentalExpr *expr);
static void q_transcendental_destroy(Transcendental *t);

/* ── 超越数表达式解析辅助函数 ── */

/**
 * 从名称字符串解析超越数表达式。
 *
 * 委托给 transcendental_create() 进行解析，提取其 expr 字段，
 * 然后释放 Transcendental 外壳保留表达式树。
 *
 * @param name 常量名称（如 "pi/2", "3*pi/4"）
 * @return 新分配的 TranscendentalExpr，调用者需用 transcendental_expr_destroy 释放
 */
static TranscendentalExpr *transcendental_expr_parse(const char *name) {
    if (!name || name[0] == '\0')
        return NULL;
    /* 委托给 transcendental.c 的 transcendental_create 进行完整解析 */
    Transcendental *tmp = transcendental_create(name);
    if (!tmp)
        return NULL;
    TranscendentalExpr *expr = tmp->expr;
    tmp->expr = NULL;          /* 转移所有权 */
    q_transcendental_destroy(tmp); /* 释放外壳，expr 不受影响 */
    return expr;
}

/**
 * 释放超越数表达式树。
 *
 * @param expr 要释放的表达式，可为 NULL
 */
static void transcendental_expr_destroy(TranscendentalExpr *expr) {
    if (!expr)
        return;
    if (expr->rational_operand)
        rational_destroy(expr->rational_operand);
    lv00_free((void **) &expr);
}

/* ── Quadratic type ── */

static bool is_rational_zero(const Rational *r) {
    return mpq_cmp_ui(r->value, 0, 1) == 0;
}

/**
 * 创建二次根式对象。
 *
 * 二次根式表示为 a + b*sqrt(n)，其中 a、b 为有理数，n 为无平方因子的正整数。
 * 创建时自动规范化（移除平方因子）。
 *
 * @param a 二次项的系数有理数（不能为 NULL）
 * @param b 根号项的系数有理数（可为 NULL，视为 0）
 * @param n 根号内的整数（必须为正整数）
 * @return 新创建的二次根式对象，失败时返回 NULL；调用者需负责释放
 */
Quadratic *quadratic_create(Rational *a, Rational *b, unsigned int n) {
    if (!a)
        return NULL;

    Quadratic *q = lv00_malloc(sizeof(Quadratic));
    if (!q)
        return NULL;

    n = remove_square_factors(n);
    q->a = a;
    q->n = n;

    if (b && !is_rational_zero(b)) {
        q->b = b;
    } else {
        if (b)
            rational_destroy(b);
        q->b = rational_create(0, 1);
    }

    return q;
}

/**
 * 销毁二次根式对象并释放内存。
 *
 * @param q 二次根式对象，可为 NULL（空操作）
 */
void quadratic_destroy(Quadratic *q) {
    if (q) {
        rational_destroy(q->a);
        rational_destroy(q->b);
        lv00_free((void**)&q);  /* lv00_malloc分配 */
    }
}

/**
 * 检查两个二次根式是否具有相同的平方根参数 n。
 *
 * 当两个二次根式形式为 a1 + b1*sqrt(n1) 和 a2 + b2*sqrt(n2) 时，
 * 如果 n1 == n2，则它们可以进行精确的加减运算；
 * 否则只能通过数值近似进行比较。
 *
 * @param a 第一个二次根式（不能为 NULL）
 * @param b 第二个二次根式（不能为 NULL）
 * @return 如果 n 相同返回 true，否则返回 false
 */
static bool quadratic_same_n(const Quadratic *a, const Quadratic *b) {
    return a->n == b->n;
}

int quadratic_compare(const Quadratic *a, const Quadratic *b) {
    /* If same sqrt(n), compare directly */
    if (quadratic_same_n(a, b)) {
        int cmp_a = rational_compare(a->a, b->a);
        if (cmp_a != 0)
            return cmp_a;
        return rational_compare(a->b, b->b);
    }

    /* Otherwise, compare numerically */
    double a_val = rational_to_double(a->a) + rational_to_double(a->b) * sqrt((double) a->n);
    double b_val = rational_to_double(b->a) + rational_to_double(b->b) * sqrt((double) b->n);

    if (a_val < b_val - LV00_EPSILON_NUMERIC_COMPARE)
        return -1;
    if (a_val > b_val + LV00_EPSILON_NUMERIC_COMPARE)
        return 1;
    return 0;
}

/**
 * 获取二次根式的数值近似值。
 *
 * 计算公式：a + b * sqrt(n)
 * 其中 a 和 b 为有理数系数，n 为平方根参数。
 *
 * @param q 二次根式对象（不能为 NULL）
 * @return 二次根式的双精度浮点数近似值
 */
static double quadratic_to_double(const Quadratic *q) {
    return rational_to_double(q->a) + rational_to_double(q->b) * sqrt((double) q->n);
}

/**
 * 从整数值创建有理数。
 *
 * 将整数 val 转换为有理数 val/1，是 rational_create 的简化包装函数。
 *
 * @param val 整数值
 * @return 新创建的有理数对象；调用者需负责释放
 */
static Rational *rational_from_int(int64_t val) {
    return rational_create(val, 1);
}

/**
 * 二次根式加法：计算 a + b。
 *
 * 要求两个二次根式的 n 值相同。
 *
 * @param a 被加数（不能为 NULL）
 * @param b 加数（不能为 NULL）
 * @return 新的二次根式对象，n 值不同时返回 NULL；调用者需负责释放
 */
Quadratic *quadratic_add(const Quadratic *a, const Quadratic *b) {
    if (a->n != b->n)
        return NULL;
    Rational *new_a = rational_add(a->a, b->a);
    Rational *new_b = rational_add(a->b, b->b);
    return quadratic_create(new_a, new_b, a->n);
}

/**
 * 二次根式减法：计算 a - b。
 *
 * 要求两个二次根式的 n 值相同。
 *
 * @param a 被减数（不能为 NULL）
 * @param b 减数（不能为 NULL）
 * @return 新的二次根式对象，n 值不同时返回 NULL；调用者需负责释放
 */
Quadratic *quadratic_subtract(const Quadratic *a, const Quadratic *b) {
    if (a->n != b->n)
        return NULL;
    Rational *new_a = rational_subtract(a->a, b->a);
    Rational *new_b = rational_subtract(a->b, b->b);
    return quadratic_create(new_a, new_b, a->n);
}

Quadratic *quadratic_multiply(const Quadratic *a, const Quadratic *b) {
    if (a->n != b->n)
        return NULL;

    /* (a1 + b1*sqrt(n)) * (a2 + b2*sqrt(n)) = (a1*a2 + b1*b2*n) + (a1*b2 + a2*b1)*sqrt(n) */
    Rational *a1 = rational_copy(a->a);
    Rational *a2 = rational_copy(b->a);
    Rational *b1 = rational_copy(a->b);
    Rational *b2 = rational_copy(b->b);

    Rational *term1 = rational_multiply(a1, a2);
    Rational *b1b2 = rational_multiply(b1, b2);
    Rational *n_rat = rational_create(a->n, 1);
    Rational *term2 = rational_multiply(b1b2, n_rat);
    Rational *new_a = rational_add(term1, term2);

    Rational *a1b2 = rational_multiply(a1, b2);
    Rational *a2b1 = rational_multiply(a2, b1);
    Rational *new_b = rational_add(a1b2, a2b1);

    rational_destroy(a1);
    rational_destroy(a2);
    rational_destroy(b1);
    rational_destroy(b2);
    rational_destroy(term1);
    rational_destroy(term2);
    rational_destroy(b1b2);
    rational_destroy(n_rat);
    rational_destroy(a1b2);
    rational_destroy(a2b1);

    return quadratic_create(new_a, new_b, a->n);
}

Quadratic *quadratic_divide(const Quadratic *a, const Quadratic *b) {
    if (a->n != b->n)
        return NULL;

    Rational *zero = rational_create(0, 1);
    bool b_is_zero = (rational_compare(b->a, zero) == 0 && rational_compare(b->b, zero) == 0);
    rational_destroy(zero);

    if (b_is_zero)
        return NULL;

    /* (a1 + b1*sqrt(n)) / (a2 + b2*sqrt(n)) 
     * = (a1 + b1*sqrt(n)) * (a2 - b2*sqrt(n)) / (a2^2 - b2^2*n)
     */
    Rational *c = rational_copy(b->a);
    Rational *d = rational_copy(b->b);
    Rational *n_rat = rational_create(a->n, 1);

    Rational *c2 = rational_multiply(c, c);
    Rational *d2 = rational_multiply(d, d);
    Rational *d2n = rational_multiply(d2, n_rat);
    Rational *denom = rational_subtract(c2, d2n);

    /* 检查分母是否为零 */
    Rational *zero2 = rational_create(0, 1);
    if (rational_compare(denom, zero2) == 0) {
        rational_destroy(zero2);
        rational_destroy(c);
        rational_destroy(d);
        rational_destroy(c2);
        rational_destroy(d2);
        rational_destroy(d2n);
        rational_destroy(denom);
        rational_destroy(n_rat);
        return NULL;
    }
    rational_destroy(zero2);

    Rational *a1 = rational_copy(a->a);
    Rational *b1 = rational_copy(a->b);

    /* Numerator for a: a1*c + b1*d*n */
    Rational *a1c = rational_multiply(a1, c);
    Rational *b1d = rational_multiply(b1, d);
    Rational *b1dn = rational_multiply(b1d, n_rat);
    Rational *num_a = rational_add(a1c, b1dn);

    /* Numerator for b: b1*c - a1*d */
    Rational *b1c = rational_multiply(b1, c);
    Rational *a1d = rational_multiply(a1, d);
    Rational *num_b = rational_subtract(b1c, a1d);

    Rational *new_a = rational_divide(num_a, denom);
    Rational *new_b = rational_divide(num_b, denom);

    rational_destroy(c);
    rational_destroy(d);
    rational_destroy(c2);
    rational_destroy(d2);
    rational_destroy(d2n);
    rational_destroy(denom);
    rational_destroy(n_rat);
    rational_destroy(a1);
    rational_destroy(b1);
    rational_destroy(a1c);
    rational_destroy(b1d);
    rational_destroy(b1dn);
    rational_destroy(num_a);
    rational_destroy(b1c);
    rational_destroy(a1d);
    rational_destroy(num_b);

    if (!new_a || !new_b) {
        if (new_a)
            rational_destroy(new_a);
        if (new_b)
            rational_destroy(new_b);
        return NULL;
    }

    return quadratic_create(new_a, new_b, a->n);
}

char *quadratic_serialize(const Quadratic *q) {
    char *a_str = rational_serialize(q->a);
    char *b_str = rational_serialize(q->b);
    if (!a_str || !b_str) {
        lv00_free((void**)&a_str); /* lv00_malloc分配 */
        lv00_free((void**)&b_str); /* lv00_malloc分配 */
        return NULL;
    }
    size_t len = strlen(a_str) + strlen(b_str) + 32;
    char *result = lv00_malloc(len);
    if (!result) {
        lv00_free((void**)&a_str); /* lv00_malloc分配 */
        lv00_free((void**)&b_str); /* lv00_malloc分配 */
        return NULL;
    }
    snprintf(result, len, "%s + %s*sqrt(%u)", a_str, b_str, q->n);
    lv00_free((void**)&a_str); /* lv00_malloc分配 */
    lv00_free((void**)&b_str); /* lv00_malloc分配 */
    return result;
}

/* ============================================================
 * Transcendental Number Implementation
 * ============================================================ */

/**
 * 销毁超越数对象（quadratic.c 内部版本，用于 transcendental_expr_parse）。
 */
static void q_transcendental_destroy(Transcendental *t) {
    if (!t)
        return;
    if (t->expr)
        transcendental_expr_destroy(t->expr);
    /* t->name 是 char[64] 固定数组，无需单独释放 */
    lv00_free((void **) &t);
}

/**
 * 移除平方因子。
 *
 * @param n 输入整数
 * @return 移除平方因子后的结果
 */
static int remove_square_factors(int n) {
    if (n == 0) return 0;
    int result = 1;
    int temp = (n < 0) ? -n : n;
    for (int i = 2; i * i <= temp; i++) {
        while (temp % (i * i) == 0) {
            temp /= (i * i);
        }
    }
    return (n < 0) ? -temp : temp;
}
