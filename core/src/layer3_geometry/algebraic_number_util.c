/**
 * @file algebraic_number_util.c
 * @brief 代数数域封装 —— 内部整数工具（gcd/溢出检测/开方/化简）
 *
 * @details 由 algebraic_number.c 按数域类型拆分而来。
 *          所有运算基于 int64_t，不依赖 GMP 等外部库。
 *
 * @version 3.5.0
 * @copyright Copyright (c) 2024-2026 Lv-00 Project
 */

#include "lv/algebraic_number.h"
#include "lv/lv_arith_safe.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "algebraic_number_internal.h"

#include "lv_internal.h"

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 计算 int64_t 的最大公约数（GCD）
 *
 * 使用欧几里得算法。对负数取绝对值后计算。
 * 薄转发到公共设施 lv_gcd_i64（uint64 安全语义，
 * INT64_MIN 绝对值 2^63 正确参与计算，不做 INT64_MAX 近似）。
 *
 * @param a 整数
 * @param b 整数
 * @return |a| 和 |b| 的最大公约数（>= 1）
 */
int64_t alg_gcd(int64_t a, int64_t b) {
    return lv_gcd_i64(a, b);
}

/**
 * @brief 计算 int64_t 的最小公倍数（LCM）
 *
 * @param a 整数（非零）
 * @param b 整数（非零）
 * @return |a| 和 |b| 的最小公倍数
 */
int64_t alg_lcm(int64_t a, int64_t b) {
    return lv_lcm_i64(a, b);
}

/**
 * @brief 检测 int64_t 乘法是否溢出
 *
 * @param a 乘数
 * @param b 乘数
 * @param[out] result 乘积（无溢出时有效）
 * @return true 溢出，false 无溢出
 */
bool alg_mul_overflow(int64_t a, int64_t b, int64_t *result) {
    /* lv_safe_mul_i64 返回 true=成功，语义取反为「true=溢出」 */
    return !lv_safe_mul_i64(a, b, result);
}

/**
 * @brief 检测 int64_t 加法是否溢出
 */
bool alg_add_overflow(int64_t a, int64_t b, int64_t *result) {
    return !lv_safe_add_i64(a, b, result);
}

/**
 * @brief 检测 int64_t 减法是否溢出
 */
bool alg_sub_overflow(int64_t a, int64_t b, int64_t *result) {
    return !lv_safe_sub_i64(a, b, result);
}

/**
 * @brief 约分有理数（内部使用）
 *
 * 将 p/q 约分为最简形式，确保 q > 0。
 * 调用者需确保 q != 0。
 */
void lv_alg_rational_simplify(int64_t *p, int64_t *q) {
    lv_rational_simplify_i64(p, q);
}

/**
 * @brief 判断整数是否为完全平方数
 */
bool alg_is_perfect_square(int64_t n) {
    if (n < 0)
        return false;
    if (n == 0 || n == 1)
        return true;
    int64_t lo = 1, hi = n < 46341 ? n : 46341; /* sqrt(INT64_MAX) ≈ 3037000499，但 46341^2 < INT64_MAX */
    /* 使用更安全的上界 */
    hi = (n < 3037000500LL) ? n : 3037000500LL;
    while (lo <= hi) {
        int64_t mid = lo + (hi - lo) / 2;
        int64_t sq;
        if (alg_mul_overflow(mid, mid, &sq)) {
            hi = mid - 1;
            continue;
        }
        if (sq == n)
            return true;
        if (sq < n)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return false;
}

/**
 * @brief 计算整数平方根（向下取整）
 */
int64_t alg_isqrt(int64_t n) {
    if (n < 0)
        return 0;
    if (n == 0)
        return 0;
    int64_t x = (int64_t) sqrt((double) n);
    /* 修正浮点误差 */
    while (x > 0) {
        int64_t sq;
        if (alg_mul_overflow(x, x, &sq)) {
            x--;
            continue;
        }
        if (sq <= n)
            break;
        x--;
    }
    while (true) {
        int64_t sq;
        if (alg_mul_overflow(x + 1, x + 1, &sq))
            break;
        if (sq > n)
            break;
        x++;
    }
    return x;
}
