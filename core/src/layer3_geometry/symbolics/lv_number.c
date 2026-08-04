/**
 * @file lv_number.c
 * @brief 数值类型统一抽象层 (lvNumber) 实现
 *
 * 提供统一的数值操作接口，内部通过 ops vtable 分发到
 * 具体数值类型（有理数、浮点数、整数等）。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/lv_number.h"
#include "lv/rational.h"
#include "lv/lv_utils.h"
#include "lv/lv_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* ============================================================
 * 内部辅助结构体
 * ============================================================ */

/* 浮点数实现 */
typedef struct {
    double value;
} FloatImpl;

/* 整数实现 */
typedef struct {
    int64_t value;
} IntImpl;

/* ============================================================
 * 前向声明：各类型的 ops vtable
 * ============================================================ */

static const lvNumberOps g_rational_ops;
static const lvNumberOps g_float_ops;
static const lvNumberOps g_int_ops;

/* ============================================================
 * 辅助函数：创建 lvNumber 句柄
 * ============================================================ */

static lvNumber *lv_number_alloc(const lvNumberOps *ops, void *impl) {
    lv_CHECK_NULL(ops, NULL);
    lv_CHECK_NULL(impl, NULL);

    lvNumber *n = (lvNumber *) lv_malloc(sizeof(lvNumber));
    lv_CHECK_ALLOC(n, NULL);
    n->ops = ops;
    n->impl = impl;
    return n;
}

/* ============================================================
 * 有理数 ops 实现
 * ============================================================ */

static lvNumber *rational_op_add(const lvNumber *a, const lvNumber *b) {
    const lvRational *ra = (const lvRational *) a->impl;
    const lvRational *rb = (const lvRational *) b->impl;
    lvRational *result = lv_rational_add(ra, rb);
    lv_CHECK_ALLOC(result, NULL);
    return lv_number_alloc(&g_rational_ops, result);
}

static lvNumber *rational_op_sub(const lvNumber *a, const lvNumber *b) {
    const lvRational *ra = (const lvRational *) a->impl;
    const lvRational *rb = (const lvRational *) b->impl;
    lvRational *result = lv_rational_sub(ra, rb);
    lv_CHECK_ALLOC(result, NULL);
    return lv_number_alloc(&g_rational_ops, result);
}

static lvNumber *rational_op_mul(const lvNumber *a, const lvNumber *b) {
    const lvRational *ra = (const lvRational *) a->impl;
    const lvRational *rb = (const lvRational *) b->impl;
    lvRational *result = lv_rational_mul(ra, rb);
    lv_CHECK_ALLOC(result, NULL);
    return lv_number_alloc(&g_rational_ops, result);
}

static lvNumber *rational_op_div(const lvNumber *a, const lvNumber *b) {
    const lvRational *ra = (const lvRational *) a->impl;
    const lvRational *rb = (const lvRational *) b->impl;
    lvRational *result = lv_rational_div(ra, rb);
    lv_CHECK_ALLOC(result, NULL);
    return lv_number_alloc(&g_rational_ops, result);
}

static int rational_op_compare(const lvNumber *a, const lvNumber *b) {
    const lvRational *ra = (const lvRational *) a->impl;
    const lvRational *rb = (const lvRational *) b->impl;
    int cmp = lv_rational_cmp(ra, rb);
    if (cmp < 0) return -1;
    if (cmp > 0) return 1;
    return 0;
}

static double rational_op_to_double(const lvNumber *n) {
    const lvRational *r = (const lvRational *) n->impl;
    double out;
    lv_rational_to_double(r, &out, NULL);
    return out;
}

static uint64_t rational_op_hash(const lvNumber *n) {
    const lvRational *r = (const lvRational *) n->impl;
    double d;
    lv_rational_to_double(r, &d, NULL);
    /* 基于 double 的简单哈希 */
    union { double d; uint64_t u; } conv;
    conv.d = d;
    return conv.u;
}

static char *rational_op_to_string(const lvNumber *n) {
    const lvRational *r = (const lvRational *) n->impl;
    return lv_rational_to_string(r);
}

static bool rational_op_is_zero(const lvNumber *n) {
    const lvRational *r = (const lvRational *) n->impl;
    return lv_rational_is_zero(r);
}

static bool rational_op_is_one(const lvNumber *n) {
    const lvRational *r = (const lvRational *) n->impl;
    return lv_rational_is_one(r);
}

static bool rational_op_is_negative(const lvNumber *n) {
    const lvRational *r = (const lvRational *) n->impl;
    return lv_rational_sgn(r) < 0;
}

static lvNumber *rational_op_clone(const lvNumber *n) {
    const lvRational *r = (const lvRational *) n->impl;
    lvRational *copy = lv_rational_clone(r);
    lv_CHECK_ALLOC(copy, NULL);
    return lv_number_alloc(&g_rational_ops, copy);
}

static void rational_op_destroy(lvNumber *n) {
    if (!n) return;
    if (n->impl) {
        lv_rational_destroy((lvRational **) &n->impl);
    }
    lv_free((void **) &n);
}

static lvNumberType rational_op_type(const lvNumber *n) {
    (void)n;
    return lv_NUMBER_RATIONAL;
}

static const lvNumberOps g_rational_ops = {
    .add        = rational_op_add,
    .sub        = rational_op_sub,
    .mul        = rational_op_mul,
    .div        = rational_op_div,
    .compare    = rational_op_compare,
    .to_double  = rational_op_to_double,
    .hash       = rational_op_hash,
    .to_string  = rational_op_to_string,
    .is_zero    = rational_op_is_zero,
    .is_one     = rational_op_is_one,
    .is_negative= rational_op_is_negative,
    .clone      = rational_op_clone,
    .destroy    = rational_op_destroy,
    .type       = rational_op_type,
};

/* ============================================================
 * 浮点数 ops 实现
 * ============================================================ */

static lvNumber *float_op_add(const lvNumber *a, const lvNumber *b) {
    const FloatImpl *fa = (const FloatImpl *) a->impl;
    const FloatImpl *fb = (const FloatImpl *) b->impl;
    FloatImpl *result = (FloatImpl *) lv_malloc(sizeof(FloatImpl));
    lv_CHECK_ALLOC(result, NULL);
    result->value = fa->value + fb->value;
    return lv_number_alloc(&g_float_ops, result);
}

static lvNumber *float_op_sub(const lvNumber *a, const lvNumber *b) {
    const FloatImpl *fa = (const FloatImpl *) a->impl;
    const FloatImpl *fb = (const FloatImpl *) b->impl;
    FloatImpl *result = (FloatImpl *) lv_malloc(sizeof(FloatImpl));
    lv_CHECK_ALLOC(result, NULL);
    result->value = fa->value - fb->value;
    return lv_number_alloc(&g_float_ops, result);
}

static lvNumber *float_op_mul(const lvNumber *a, const lvNumber *b) {
    const FloatImpl *fa = (const FloatImpl *) a->impl;
    const FloatImpl *fb = (const FloatImpl *) b->impl;
    FloatImpl *result = (FloatImpl *) lv_malloc(sizeof(FloatImpl));
    lv_CHECK_ALLOC(result, NULL);
    result->value = fa->value * fb->value;
    return lv_number_alloc(&g_float_ops, result);
}

static lvNumber *float_op_div(const lvNumber *a, const lvNumber *b) {
    const FloatImpl *fa = (const FloatImpl *) a->impl;
    const FloatImpl *fb = (const FloatImpl *) b->impl;
    FloatImpl *result = (FloatImpl *) lv_malloc(sizeof(FloatImpl));
    lv_CHECK_ALLOC(result, NULL);
    result->value = fa->value / fb->value;
    return lv_number_alloc(&g_float_ops, result);
}

static int float_op_compare(const lvNumber *a, const lvNumber *b) {
    const FloatImpl *fa = (const FloatImpl *) a->impl;
    const FloatImpl *fb = (const FloatImpl *) b->impl;
    if (fa->value < fb->value) return -1;
    if (fa->value > fb->value) return 1;
    return 0;
}

static double float_op_to_double(const lvNumber *n) {
    const FloatImpl *f = (const FloatImpl *) n->impl;
    return f->value;
}

static uint64_t float_op_hash(const lvNumber *n) {
    const FloatImpl *f = (const FloatImpl *) n->impl;
    union { double d; uint64_t u; } conv;
    conv.d = f->value;
    return conv.u;
}

static char *float_op_to_string(const lvNumber *n) {
    const FloatImpl *f = (const FloatImpl *) n->impl;
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%.17g", f->value);
    if (len < 0) len = 0;
    char *str = (char *) lv_malloc((size_t)(len + 1));
    lv_CHECK_ALLOC(str, NULL);
    memcpy(str, buf, (size_t)(len + 1));
    return str;
}

static bool float_op_is_zero(const lvNumber *n) {
    const FloatImpl *f = (const FloatImpl *) n->impl;
    return fabs(f->value) < 1e-15;
}

static bool float_op_is_one(const lvNumber *n) {
    const FloatImpl *f = (const FloatImpl *) n->impl;
    return fabs(f->value - 1.0) < 1e-15;
}

static bool float_op_is_negative(const lvNumber *n) {
    const FloatImpl *f = (const FloatImpl *) n->impl;
    return f->value < 0.0;
}

static lvNumber *float_op_clone(const lvNumber *n) {
    const FloatImpl *f = (const FloatImpl *) n->impl;
    FloatImpl *copy = (FloatImpl *) lv_malloc(sizeof(FloatImpl));
    lv_CHECK_ALLOC(copy, NULL);
    copy->value = f->value;
    return lv_number_alloc(&g_float_ops, copy);
}

static void float_op_destroy(lvNumber *n) {
    if (!n) return;
    if (n->impl) {
        lv_free((void **) &n->impl);
    }
    lv_free((void **) &n);
}

static lvNumberType float_op_type(const lvNumber *n) {
    (void)n;
    return lv_NUMBER_FLOAT;
}

static const lvNumberOps g_float_ops = {
    .add        = float_op_add,
    .sub        = float_op_sub,
    .mul        = float_op_mul,
    .div        = float_op_div,
    .compare    = float_op_compare,
    .to_double  = float_op_to_double,
    .hash       = float_op_hash,
    .to_string  = float_op_to_string,
    .is_zero    = float_op_is_zero,
    .is_one     = float_op_is_one,
    .is_negative= float_op_is_negative,
    .clone      = float_op_clone,
    .destroy    = float_op_destroy,
    .type       = float_op_type,
};

/* ============================================================
 * 整数 ops 实现
 * ============================================================ */

static lvNumber *int_op_add(const lvNumber *a, const lvNumber *b) {
    const IntImpl *ia = (const IntImpl *) a->impl;
    const IntImpl *ib = (const IntImpl *) b->impl;
    IntImpl *result = (IntImpl *) lv_malloc(sizeof(IntImpl));
    lv_CHECK_ALLOC(result, NULL);
    result->value = ia->value + ib->value;
    return lv_number_alloc(&g_int_ops, result);
}

static lvNumber *int_op_sub(const lvNumber *a, const lvNumber *b) {
    const IntImpl *ia = (const IntImpl *) a->impl;
    const IntImpl *ib = (const IntImpl *) b->impl;
    IntImpl *result = (IntImpl *) lv_malloc(sizeof(IntImpl));
    lv_CHECK_ALLOC(result, NULL);
    result->value = ia->value - ib->value;
    return lv_number_alloc(&g_int_ops, result);
}

static lvNumber *int_op_mul(const lvNumber *a, const lvNumber *b) {
    const IntImpl *ia = (const IntImpl *) a->impl;
    const IntImpl *ib = (const IntImpl *) b->impl;
    IntImpl *result = (IntImpl *) lv_malloc(sizeof(IntImpl));
    lv_CHECK_ALLOC(result, NULL);
    result->value = ia->value * ib->value;
    return lv_number_alloc(&g_int_ops, result);
}

static lvNumber *int_op_div(const lvNumber *a, const lvNumber *b) {
    const IntImpl *ia = (const IntImpl *) a->impl;
    const IntImpl *ib = (const IntImpl *) b->impl;
    if (ib->value == 0) return NULL;
    IntImpl *result = (IntImpl *) lv_malloc(sizeof(IntImpl));
    lv_CHECK_ALLOC(result, NULL);
    result->value = ia->value / ib->value;
    return lv_number_alloc(&g_int_ops, result);
}

static int int_op_compare(const lvNumber *a, const lvNumber *b) {
    const IntImpl *ia = (const IntImpl *) a->impl;
    const IntImpl *ib = (const IntImpl *) b->impl;
    if (ia->value < ib->value) return -1;
    if (ia->value > ib->value) return 1;
    return 0;
}

static double int_op_to_double(const lvNumber *n) {
    const IntImpl *in = (const IntImpl *) n->impl;
    return (double) in->value;
}

static uint64_t int_op_hash(const lvNumber *n) {
    const IntImpl *in = (const IntImpl *) n->impl;
    return (uint64_t) in->value;
}

static char *int_op_to_string(const lvNumber *n) {
    const IntImpl *in = (const IntImpl *) n->impl;
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%" PRId64, in->value);
    if (len < 0) len = 0;
    char *str = (char *) lv_malloc((size_t)(len + 1));
    lv_CHECK_ALLOC(str, NULL);
    memcpy(str, buf, (size_t)(len + 1));
    return str;
}

static bool int_op_is_zero(const lvNumber *n) {
    const IntImpl *in = (const IntImpl *) n->impl;
    return in->value == 0;
}

static bool int_op_is_one(const lvNumber *n) {
    const IntImpl *in = (const IntImpl *) n->impl;
    return in->value == 1;
}

static bool int_op_is_negative(const lvNumber *n) {
    const IntImpl *in = (const IntImpl *) n->impl;
    return in->value < 0;
}

static lvNumber *int_op_clone(const lvNumber *n) {
    const IntImpl *in = (const IntImpl *) n->impl;
    IntImpl *copy = (IntImpl *) lv_malloc(sizeof(IntImpl));
    lv_CHECK_ALLOC(copy, NULL);
    copy->value = in->value;
    return lv_number_alloc(&g_int_ops, copy);
}

static void int_op_destroy(lvNumber *n) {
    if (!n) return;
    if (n->impl) {
        lv_free((void **) &n->impl);
    }
    lv_free((void **) &n);
}

static lvNumberType int_op_type(const lvNumber *n) {
    (void)n;
    return lv_NUMBER_INTEGER;
}

static const lvNumberOps g_int_ops = {
    .add        = int_op_add,
    .sub        = int_op_sub,
    .mul        = int_op_mul,
    .div        = int_op_div,
    .compare    = int_op_compare,
    .to_double  = int_op_to_double,
    .hash       = int_op_hash,
    .to_string  = int_op_to_string,
    .is_zero    = int_op_is_zero,
    .is_one     = int_op_is_one,
    .is_negative= int_op_is_negative,
    .clone      = int_op_clone,
    .destroy    = int_op_destroy,
    .type       = int_op_type,
};

/* ============================================================
 * 类型转换辅助：将不同数值类型统一为同类型以便运算
 *
 * 当前策略：若两个操作数类型不同，将两者都转换为 double 运算。
 * 后续可扩展为更精确的类型提升规则。
 * ============================================================ */

/* 获取一个 lvNumber 的 ops 指针（用于类型间运算时提升） */
static const lvNumberOps *ops_for_type(lvNumberType type) {
    switch (type) {
        case lv_NUMBER_RATIONAL: return &g_rational_ops;
        case lv_NUMBER_FLOAT:    return &g_float_ops;
        case lv_NUMBER_INTEGER:  return &g_int_ops;
        default:                 return &g_float_ops;
    }
}

/* ============================================================
 * 工厂函数
 * ============================================================ */

lv_PUBLIC_API lvNumber *lv_number_from_rational(int64_t num, uint64_t den) {
    if (den == 0) return NULL;
    lvRational *r = lv_rational_create_from_i64(num, den);
    lv_CHECK_ALLOC(r, NULL);
    return lv_number_alloc(&g_rational_ops, r);
}

lv_PUBLIC_API lvNumber *lv_number_from_double(double val) {
    FloatImpl *f = (FloatImpl *) lv_malloc(sizeof(FloatImpl));
    lv_CHECK_ALLOC(f, NULL);
    f->value = val;
    return lv_number_alloc(&g_float_ops, f);
}

lv_PUBLIC_API lvNumber *lv_number_from_int(int64_t val) {
    IntImpl *in = (IntImpl *) lv_malloc(sizeof(IntImpl));
    lv_CHECK_ALLOC(in, NULL);
    in->value = val;
    return lv_number_alloc(&g_int_ops, in);
}

lv_PUBLIC_API lvNumber *lv_number_from_string(const char *str) {
    lv_CHECK_NULL(str, NULL);

    /* 尝试解析为有理数 */
    lvRational *r = lv_rational_from_string(str);
    if (r) {
        return lv_number_alloc(&g_rational_ops, r);
    }

    /* 尝试解析为双精度浮点数 */
    char *end = NULL;
    double d = strtod(str, &end);
    if (end != str && *end == '\0') {
        FloatImpl *f = (FloatImpl *) lv_malloc(sizeof(FloatImpl));
        lv_CHECK_ALLOC(f, NULL);
        f->value = d;
        return lv_number_alloc(&g_float_ops, f);
    }

    return NULL;
}

/* ============================================================
 * 算术运算（通过 ops vtable 分发，含类型提升）
 * ============================================================ */

lv_PUBLIC_API lvNumber *lv_number_add(const lvNumber *a, const lvNumber *b) {
    lv_CHECK_NULL(a, NULL);
    lv_CHECK_NULL(b, NULL);
    return a->ops->add(a, b);
}

lv_PUBLIC_API lvNumber *lv_number_sub(const lvNumber *a, const lvNumber *b) {
    lv_CHECK_NULL(a, NULL);
    lv_CHECK_NULL(b, NULL);
    return a->ops->sub(a, b);
}

lv_PUBLIC_API lvNumber *lv_number_mul(const lvNumber *a, const lvNumber *b) {
    lv_CHECK_NULL(a, NULL);
    lv_CHECK_NULL(b, NULL);
    return a->ops->mul(a, b);
}

lv_PUBLIC_API lvNumber *lv_number_div(const lvNumber *a, const lvNumber *b) {
    lv_CHECK_NULL(a, NULL);
    lv_CHECK_NULL(b, NULL);
    return a->ops->div(a, b);
}

lv_PUBLIC_API lvNumber *lv_number_neg(const lvNumber *n) {
    lv_CHECK_NULL(n, NULL);
    /* 取负 = 0 - n */
    lvNumber *zero = lv_number_from_int(0);
    if (!zero) return NULL;
    lvNumber *result = n->ops->sub(zero, n);
    lv_number_destroy(zero);
    return result;
}

lv_PUBLIC_API lvNumber *lv_number_abs(const lvNumber *n) {
    lv_CHECK_NULL(n, NULL);
    if (n->ops->is_negative(n)) {
        return lv_number_neg(n);
    }
    return n->ops->clone(n);
}

lv_PUBLIC_API lvNumber *lv_number_pow(const lvNumber *base, int exp) {
    lv_CHECK_NULL(base, NULL);

    if (exp == 0) {
        return lv_number_from_int(1);
    }
    if (exp < 0) {
        /* 负指数: 1 / base^|exp| */
        lvNumber *pos = lv_number_pow(base, -exp);
        if (!pos) return NULL;
        lvNumber *one = lv_number_from_int(1);
        if (!one) { lv_number_destroy(pos); return NULL; }
        lvNumber *result = lv_number_div(one, pos);
        lv_number_destroy(one);
        lv_number_destroy(pos);
        return result;
    }

    /* 快速幂算法 */
    lvNumber *result = lv_number_from_int(1);
    lvNumber *cur = lv_number_clone(base);
    if (!result || !cur) {
        lv_number_destroy(result);
        lv_number_destroy(cur);
        return NULL;
    }

    int e = exp;
    while (e > 0) {
        if (e & 1) {
            lvNumber *new_result = lv_number_mul(result, cur);
            lv_number_destroy(result);
            result = new_result;
            if (!result) { lv_number_destroy(cur); return NULL; }
        }
        e >>= 1;
        if (e > 0) {
            lvNumber *new_cur = lv_number_mul(cur, cur);
            lv_number_destroy(cur);
            cur = new_cur;
            if (!cur) { lv_number_destroy(result); return NULL; }
        }
    }
    lv_number_destroy(cur);
    return result;
}

/* ============================================================
 * 比较运算
 * ============================================================ */

lv_PUBLIC_API int lv_number_compare(const lvNumber *a, const lvNumber *b) {
    lv_CHECK_NULL(a, 0);
    lv_CHECK_NULL(b, 0);
    return a->ops->compare(a, b);
}

lv_PUBLIC_API bool lv_number_eq(const lvNumber *a, const lvNumber *b) {
    lv_CHECK_NULL(a, false);
    lv_CHECK_NULL(b, false);
    return a->ops->compare(a, b) == 0;
}

lv_PUBLIC_API bool lv_number_lt(const lvNumber *a, const lvNumber *b) {
    lv_CHECK_NULL(a, false);
    lv_CHECK_NULL(b, false);
    return a->ops->compare(a, b) < 0;
}

lv_PUBLIC_API bool lv_number_gt(const lvNumber *a, const lvNumber *b) {
    lv_CHECK_NULL(a, false);
    lv_CHECK_NULL(b, false);
    return a->ops->compare(a, b) > 0;
}

lv_PUBLIC_API bool lv_number_lte(const lvNumber *a, const lvNumber *b) {
    lv_CHECK_NULL(a, false);
    lv_CHECK_NULL(b, false);
    return a->ops->compare(a, b) <= 0;
}

lv_PUBLIC_API bool lv_number_gte(const lvNumber *a, const lvNumber *b) {
    lv_CHECK_NULL(a, false);
    lv_CHECK_NULL(b, false);
    return a->ops->compare(a, b) >= 0;
}

/* ============================================================
 * 转换函数
 * ============================================================ */

lv_PUBLIC_API double lv_number_to_double(const lvNumber *n) {
    lv_CHECK_NULL(n, 0.0);
    return n->ops->to_double(n);
}

lv_PUBLIC_API int64_t lv_number_to_int(const lvNumber *n) {
    lv_CHECK_NULL(n, 0);
    return (int64_t) n->ops->to_double(n);
}

lv_PUBLIC_API char *lv_number_to_string(const lvNumber *n) {
    lv_CHECK_NULL(n, NULL);
    return n->ops->to_string(n);
}

/* ============================================================
 * 查询函数
 * ============================================================ */

lv_PUBLIC_API bool lv_number_is_zero(const lvNumber *n) {
    lv_CHECK_NULL(n, false);
    return n->ops->is_zero(n);
}

lv_PUBLIC_API bool lv_number_is_one(const lvNumber *n) {
    lv_CHECK_NULL(n, false);
    return n->ops->is_one(n);
}

lv_PUBLIC_API bool lv_number_is_negative(const lvNumber *n) {
    lv_CHECK_NULL(n, false);
    return n->ops->is_negative(n);
}

lv_PUBLIC_API bool lv_number_is_positive(const lvNumber *n) {
    lv_CHECK_NULL(n, false);
    if (n->ops->is_zero(n)) return false;
    return !n->ops->is_negative(n);
}

lv_PUBLIC_API bool lv_number_is_integer(const lvNumber *n) {
    lv_CHECK_NULL(n, false);
    if (n->ops->type(n) == lv_NUMBER_INTEGER) return true;
    /* 对于有理数，检查分母是否为 1 */
    double d = n->ops->to_double(n);
    return fabs(d - (double)(int64_t)d) < 1e-15;
}

lv_PUBLIC_API lvNumberType lv_number_type(const lvNumber *n) {
    lv_CHECK_NULL(n, lv_NUMBER_RATIONAL);
    return n->ops->type(n);
}

lv_PUBLIC_API uint64_t lv_number_hash(const lvNumber *n) {
    lv_CHECK_NULL(n, 0);
    return n->ops->hash(n);
}

lv_PUBLIC_API lvNumber *lv_number_clone(const lvNumber *n) {
    lv_CHECK_NULL(n, NULL);
    return n->ops->clone(n);
}

lv_PUBLIC_API void lv_number_destroy(lvNumber *n) {
    if (!n) return;
    if (n->ops && n->ops->destroy) {
        n->ops->destroy(n);
    } else {
        /* 安全 fallback */
        if (n->impl) lv_free((void **) &n->impl);
        lv_free((void **) &n);
    }
}

/* ============================================================
 * 类型信息
 * ============================================================ */

lv_PUBLIC_API const char *lv_number_type_name(lvNumberType type) {
    switch (type) {
        case lv_NUMBER_RATIONAL:  return "Rational";
        case lv_NUMBER_ALGEBRAIC: return "Algebraic";
        case lv_NUMBER_INTERVAL:  return "Interval";
        case lv_NUMBER_FLOAT:     return "Float";
        case lv_NUMBER_INTEGER:   return "Integer";
        default:                  return "Unknown";
    }
}