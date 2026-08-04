/**
 * @file groebner_mono.c
 * @brief Groebner 引擎单项式操作实现（从 groebner_engine.c 拆分）
 *
 * @details 单项式序比较（lex/grlex/grevlex/elim/weight）、总次数、LCM、
 *          整除判断、除法、互质判断与指数向量复制。
 *          仅依赖 lvPolynomialRing 结构，与多项式池、注册表完全解耦。
 */

#include "groebner_engine_internal.h"

#include "lv/lv.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  单项式序比较函数 — VTable 实现
 * ================================================================ */

/* ---------- 函数指针类型 ---------- */
typedef int (*MonomialCompareHandler)(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b);

/* ---------- handler 函数 ---------- */

/**
 * @brief 纯字典序（lex）
 */
static int compare_lex(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b) {
    int vc = ring->var_count;
    for (int i = 0; i < vc; i++) {
        if (powers_a[i] != powers_b[i]) {
            return powers_a[i] - powers_b[i];
        }
    }
    return 0;
}

/**
 * @brief 分次字典序（grlex）
 */
static int compare_grlex(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b) {
    int vc = ring->var_count;
    int deg_a = 0, deg_b = 0;
    for (int i = 0; i < vc; i++) {
        deg_a += powers_a[i];
        deg_b += powers_b[i];
    }
    if (deg_a != deg_b) {
        return deg_a - deg_b;
    }
    for (int i = 0; i < vc; i++) {
        if (powers_a[i] != powers_b[i]) {
            return powers_a[i] - powers_b[i];
        }
    }
    return 0;
}

/**
 * @brief 分次反字典序（grevlex）
 */
static int compare_grevlex(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b) {
    int vc = ring->var_count;
    int deg_a = 0, deg_b = 0;
    for (int i = 0; i < vc; i++) {
        deg_a += powers_a[i];
        deg_b += powers_b[i];
    }
    if (deg_a != deg_b) {
        return deg_a - deg_b;
    }
    for (int i = vc - 1; i >= 0; i--) {
        if (powers_a[i] != powers_b[i]) {
            return powers_b[i] - powers_a[i];
        }
    }
    return 0;
}

/**
 * @brief 消去序（elim）
 */
static int compare_elim(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b) {
    int vc = ring->var_count;
    int elim_count = ring->elim_var_count;
    if (elim_count > 0 && ring->elim_vars) {
        int deg_elim_a = 0, deg_elim_b = 0;
        for (int i = 0; i < vc; i++) {
            bool is_elim = false;
            for (int j = 0; j < elim_count; j++) {
                if (ring->elim_vars[j] == i) {
                    is_elim = true;
                    break;
                }
            }
            if (is_elim) {
                deg_elim_a += powers_a[i];
                deg_elim_b += powers_b[i];
            }
        }
        if (deg_elim_a != deg_elim_b) {
            return deg_elim_a - deg_elim_b;
        }
    }
    return compare_grevlex(ring, powers_a, powers_b);
}

/**
 * @brief 权重序（weight）
 */
static int compare_weight(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b) {
    int vc = ring->var_count;
    if (ring->weights) {
        double w_a = 0.0, w_b = 0.0;
        for (int i = 0; i < vc; i++) {
            w_a += ring->weights[i] * powers_a[i];
            w_b += ring->weights[i] * powers_b[i];
        }
        if (fabs(w_a - w_b) > lv_config_get_double(LV_CFG_GROEBNER_ZERO_THRESHOLD, GROEBNER_ZERO_THRESHOLD)) {
            return (w_a > w_b) ? 1 : -1;
        }
    }
    return compare_grevlex(ring, powers_a, powers_b);
}

/* ---------- 静态查找表 ---------- */
static const MonomialCompareHandler kMonomialCompareHandlers[] = {
    [MONOMIAL_LEX]     = compare_lex,
    [MONOMIAL_GRLEX]   = compare_grlex,
    [MONOMIAL_GREVLEX] = compare_grevlex,
    [MONOMIAL_ELIM]    = compare_elim,
    [MONOMIAL_WEIGHT]  = compare_weight,
};

/**
 * @brief 比较两个单项式的序关系
 *
 * 根据环中定义的单项式序类型，比较两个指数向量的大小。
 *
 * @param ring      多项式环（指定单项式序类型）
 * @param powers_a  第一个单项式的指数向量
 * @param powers_b  第二个单项式的指数向量
 * @return 正数若 A > B，0 若相等，负数若 A < B
 */
int mono_compare(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b) {
    if (!ring || !powers_a || !powers_b) {
        return 0;
    }

    if (ring->order >= 0
        && ring->order < (int)(sizeof(kMonomialCompareHandlers) / sizeof(kMonomialCompareHandlers[0]))
        && kMonomialCompareHandlers[ring->order]) {
        return kMonomialCompareHandlers[ring->order](ring, powers_a, powers_b);
    }
    return compare_grevlex(ring, powers_a, powers_b);
}

/**
 * @brief 计算单项式的总次数
 *
 * @param powers   指数向量
 * @param var_count 变量数量
 * @return 指数之和
 */
int mono_total_degree(const int *powers, int var_count) {
    int deg = 0;
    for (int i = 0; i < var_count; i++) {
        deg += powers[i];
    }
    return deg;
}

/**
 * @brief 计算两个单项式的 LCM（最小公倍式）
 *
 * LCM 的每个变量指数取两者最大值。
 *
 * @param ring       多项式环
 * @param powers_a   第一个指数向量
 * @param powers_b   第二个指数向量
 * @param lcm_out    输出 LCM 指数向量（调用者需确保空间 >= var_count）
 */
void mono_lcm(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b, int *lcm_out) {
    int vc = ring->var_count;
    for (int i = 0; i < vc; i++) {
        lcm_out[i] = (powers_a[i] > powers_b[i]) ? powers_a[i] : powers_b[i];
    }
}

/**
 * @brief 判断单项式 m1 是否被 m2 整除
 *
 * m1 被 m2 整除当且仅当 m1 每个变量的指数 >= m2 对应指数。
 *
 * @param ring      多项式环
 * @param powers_d  被除单项式指数
 * @param powers_e  除单项式指数
 * @return 可整除返回 true
 */
bool mono_divides(const lvPolynomialRing *ring, const int *powers_d, const int *powers_e) {
    int vc = ring->var_count;
    for (int i = 0; i < vc; i++) {
        if (powers_d[i] < powers_e[i]) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 计算两个单项式的商（指数向量逐项相减）
 *
 * @param ring            多项式环
 * @param powers_dividend 被除指数
 * @param powers_divisor  除指数
 * @param quotient_out    商指数输出
 *
 * @note 调用者必须确保 divisor 整除 dividend
 */
void mono_divide(const lvPolynomialRing *ring, const int *powers_dividend, const int *powers_divisor,
                        int *quotient_out) {
    int vc = ring->var_count;
    for (int i = 0; i < vc; i++) {
        quotient_out[i] = powers_dividend[i] - powers_divisor[i];
    }
}

/**
 * @brief 判断两个单项式是否互质（每个变量的指数最小值都为 0）
 *
 * @param ring      多项式环
 * @param powers_a  第一个指数
 * @param powers_b  第二个指数
 * @return 互质返回 true
 */
bool mono_is_coprime(const lvPolynomialRing *ring, const int *powers_a, const int *powers_b) {
    int vc = ring->var_count;
    for (int i = 0; i < vc; i++) {
        if (powers_a[i] > 0 && powers_b[i] > 0) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 复制指数向量
 *
 * @param dest      目标数组
 * @param src       源数组
 * @param var_count 变量数量
 */
void mono_copy(int *dest, const int *src, int var_count) {
    memcpy(dest, src, (size_t) var_count * sizeof(int));
}
