/**
 * @file mpz_poly.h
 * @brief 多精度整数多项式 —— 初始化、四则运算、结式计算
 *
 * @details 基于 GMP 库实现的多精度整数系数多项式类型 mpz_poly_t，
 *          提供多项式的创建/销毁、加减乘除、比较、序列化以及
 *          代数数运算所需的结式（resultant）计算功能。
 *
 * @author Lv-00 Project
 * @version 3.2.0
 */

#ifndef LV00_MPZ_POLY_H
#define LV00_MPZ_POLY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef MPZ_POLY_T_DEFINED
#define MPZ_POLY_T_DEFINED
typedef struct {
    mpz_t *coeffs;
    int degree;
} mpz_poly_t;
#endif

static inline void mpz_poly_init(mpz_poly_t *p) {
    p->coeffs = NULL;
    p->degree = -1;
}

static inline void mpz_poly_clear(mpz_poly_t *p) {
    if (p->coeffs) {
        for (int i = 0; i <= p->degree; i++) {
            mpz_clear(p->coeffs[i]);
        }
        free(p->coeffs);
    }
    p->coeffs = NULL;
    p->degree = -1;
}

static inline void mpz_poly_set(mpz_poly_t *dst, const mpz_poly_t *src) {
    mpz_poly_clear(dst);
    if (src->degree >= 0) {
        dst->coeffs = malloc((src->degree + 1) * sizeof(mpz_t));
        if (!dst->coeffs) {
            dst->degree = -1;
            return;
        }
        for (int i = 0; i <= src->degree; i++) {
            mpz_init_set(dst->coeffs[i], src->coeffs[i]);
        }
        dst->degree = src->degree;
    }
}

static inline int mpz_poly_equal(const mpz_poly_t *a, const mpz_poly_t *b) {
    if (a->degree != b->degree) return 0;
    if (a->degree < 0) return 1;  /* 两个零多项式相等 */
    for (int i = 0; i <= a->degree; i++) {
        if (mpz_cmp(a->coeffs[i], b->coeffs[i]) != 0) return 0;
    }
    return 1;
}

/* 内部辅助：分配结果多项式空间并初始化
 * 成功返回true，失败返回false */
static inline bool mpz_poly_alloc_result(mpz_poly_t *result, int max_deg) {
    if (max_deg < 0) {
        result->degree = -1;
        result->coeffs = NULL;
        return true;
    }
    result->coeffs = malloc((size_t)(max_deg + 1) * sizeof(mpz_t));
    if (!result->coeffs) {
        result->degree = -1;
        return false;
    }
    for (int i = 0; i <= max_deg; i++) {
        mpz_init(result->coeffs[i]);
    }
    result->degree = max_deg;
    return true;
}

/* 内部辅助：归一化结果（移除尾部零，正确清理GMP内存） */
static inline void mpz_poly_normalize(mpz_poly_t *result) {
    while (result->degree >= 0 && mpz_cmp_si(result->coeffs[result->degree], 0) == 0) {
        mpz_clear(result->coeffs[result->degree]);  /* 防止GMP内存泄漏 */
        result->degree--;
    }
    if (result->degree < 0) {
        free(result->coeffs);
        result->coeffs = NULL;
    }
}

/* 二元多项式操作宏：简化add/sub的重复代码。
 *
 * 警告：此宏内部包含 return 语句，因此只能在返回类型为 void 的函数中使用。
 * 在非 void 函数中展开此宏将导致编译错误或未定义行为。
 */
#define MPZ_POLY_BINARY_OP(result, a, b, op) \
    do { \
        mpz_poly_clear(result); \
        int max_deg = (a)->degree > (b)->degree ? (a)->degree : (b)->degree; \
        if (!mpz_poly_alloc_result(result, max_deg)) return; \
        for (int i = 0; i <= (a)->degree; i++) { \
            mpz_set(result->coeffs[i], (a)->coeffs[i]); \
        } \
        for (int i = 0; i <= (b)->degree; i++) { \
            op(result->coeffs[i], result->coeffs[i], (b)->coeffs[i]); \
        } \
        mpz_poly_normalize(result); \
    } while(0)

static inline void mpz_poly_add(mpz_poly_t *result, const mpz_poly_t *a, const mpz_poly_t *b) {
    MPZ_POLY_BINARY_OP(result, a, b, mpz_add);
}

static inline void mpz_poly_sub(mpz_poly_t *result, const mpz_poly_t *a, const mpz_poly_t *b) {
    MPZ_POLY_BINARY_OP(result, a, b, mpz_sub);
}

static inline void mpz_poly_mul(mpz_poly_t *result, const mpz_poly_t *a, const mpz_poly_t *b) {
    mpz_poly_clear(result);
    if (a->degree < 0 || b->degree < 0) {
        result->degree = -1;
        return;
    }
    /* 防止度数溢出 */
    if (a->degree > INT_MAX - b->degree) {
        result->degree = -1;
        return;
    }
    int new_degree = a->degree + b->degree;
    size_t coeff_count = (size_t)new_degree + 1;
    if (coeff_count > SIZE_MAX / sizeof(mpz_t)) {
        result->degree = -1;
        return;
    }
    result->degree = new_degree;
    result->coeffs = malloc(coeff_count * sizeof(mpz_t));
    if (!result->coeffs) {
        result->degree = -1;
        return;
    }
    for (int i = 0; i <= result->degree; i++) {
        mpz_init(result->coeffs[i]);
    }
    for (int i = 0; i <= a->degree; i++) {
        for (int j = 0; j <= b->degree; j++) {
            mpz_t tmp;
            mpz_init(tmp);
            mpz_mul(tmp, a->coeffs[i], b->coeffs[j]);
            mpz_add(result->coeffs[i + j], result->coeffs[i + j], tmp);
            mpz_clear(tmp);
        }
    }
}

/**
 * @brief 多项式除法（多项式长除法）
 *
 * 计算 dividend / divisor，商存入 quotient，余数留在 dividend 中。
 *
 * @warning 此函数会就地修改 dividend：除法完成后，dividend 的内容变为余数。
 *          如果调用者需要保留原始 dividend，必须在调用前自行拷贝。
 *          这是多项式长除法算法的已知特性（in-place remainder computation）。
 *
 * @param quotient  [out] 商多项式（调用前需已初始化，函数内会先 clear）
 * @param dividend  [in/out] 被除数（输入时为被除数，输出时变为余数）
 * @param divisor   [in] 除数（不会被修改）
 */
static inline void mpz_poly_div(mpz_poly_t *quotient, mpz_poly_t *dividend, const mpz_poly_t *divisor) {
    mpz_poly_clear(quotient);
    /* 检查除数是否有效（度数为负或首项系数为零均视为零多项式） */
    if (divisor->degree < 0 || divisor->degree > dividend->degree ||
        mpz_cmp_si(divisor->coeffs[divisor->degree], 0) == 0) {
        quotient->degree = -1;
        return;
    }
    quotient->degree = dividend->degree - divisor->degree;
    quotient->coeffs = malloc((quotient->degree + 1) * sizeof(mpz_t));
    if (!quotient->coeffs) {
        quotient->degree = -1;
        return;
    }
    for (int i = 0; i <= quotient->degree; i++) {
        mpz_init(quotient->coeffs[i]);
    }
    mpz_t factor;
    mpz_init(factor);
    for (int i = dividend->degree; i >= divisor->degree; i--) {
        int q_deg = i - divisor->degree;
        mpz_divexact(factor, dividend->coeffs[i], divisor->coeffs[divisor->degree]);
        mpz_set(quotient->coeffs[q_deg], factor);
        for (int j = 0; j <= divisor->degree; j++) {
            mpz_t tmp;
            mpz_init(tmp);
            mpz_mul(tmp, factor, divisor->coeffs[j]);
            mpz_sub(dividend->coeffs[i - j], dividend->coeffs[i - j], tmp);
            mpz_clear(tmp);
        }
    }
    mpz_clear(factor);
    if (quotient->degree < 0) {
        free(quotient->coeffs);
        quotient->coeffs = NULL;
    }
}

static inline char* mpz_poly_get_str(const mpz_poly_t *p) {
    if (p->degree < 0) {
        return strdup("0");
    }
    char **coeff_strs = malloc((p->degree + 1) * sizeof(char*));
    size_t total_len = 0;
    for (int i = 0; i <= p->degree; i++) {
        coeff_strs[i] = mpz_get_str(NULL, 10, p->coeffs[i]);
        total_len += strlen(coeff_strs[i]) + 2;
    }
    char *result = malloc(total_len + 1);
    result[0] = '\0';
    for (int i = 0; i <= p->degree; i++) {
        if (i > 0) strcat(result, ",");
        strcat(result, coeff_strs[i]);
    }
    for (int i = 0; i <= p->degree; i++) {
        free(coeff_strs[i]);
    }
    free(coeff_strs);
    return result;
}

/* AlgebraicOp enum for algebraic number operations */

/**
 * @brief 代数数运算类型枚举
 */
typedef enum {
    ALG_OP_SUM,      /**< 加法运算 */
    ALG_OP_PRODUCT   /**< 乘法运算 */
} AlgebraicOp;

/**
 * @brief 计算两个多项式的结式
 *
 * 用于代数数运算：
 * - alpha + beta：Res_y(p(y), q(x - y))
 * - alpha * beta：Res_y(p(y), y^n * q(x/y))，其中 n = deg(q)
 *
 * @param[in] p      alpha 的最小多项式（变量为 y）
 * @param[in] q      beta 的最小多项式（变量为 y）
 * @param[in] op    运算类型：ALG_OP_SUM 或 ALG_OP_PRODUCT
 * @param[out] result 结果多项式（调用者需用 mpz_poly_clear 释放）
 * @return true 成功，false 失败（度数超过 4 或其他错误）
 */
bool mpz_poly_resultant(const mpz_poly_t *p, const mpz_poly_t *q,
                        AlgebraicOp op, mpz_poly_t *result);

#ifdef __cplusplus
}
#endif

#endif /* LV00_MPZ_POLY_H */
