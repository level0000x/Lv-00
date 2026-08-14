/**
 * @file algebraic_number_io.c
 * @brief 代数数域封装 —— 跨层数域转换与误差字符串
 *
 * @details 由 algebraic_number.c 按数域类型拆分而来。
 *          所有运算基于 int64_t，不依赖 GMP 等外部库。
 *
 * @version 3.5.0
 * @copyright Copyright (c) 2024-2026 Lv-00 Project
 */

#include "lv/algebraic_number.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "algebraic_number_internal.h"
#include "lv/lv_internal.h"

/* ============================================================
 * 跨层数域转换工具 —— 实现
 * ============================================================ */

AlgInterval lv_alg_quadratic_to_interval(const AlgQuadratic *x, AlgIntervalError *err) {
    return lv_alg_interval_from_quadratic(x, err);
}

AlgInterval lv_alg_rational_to_interval(const AlgRational *r) {
    return lv_alg_interval_point(r);
}

bool lv_alg_has_real_roots(int64_t a, int64_t b, int64_t c) {
    if (a == 0) {
        /* 退化为一次方程 bx + c = 0，总有实根（若 b != 0） */
        return b != 0;
    }
    /* 判别式 b^2 - 4ac >= 0 */
    int64_t b_sq, four_ac, disc;
    if (alg_mul_overflow(b, b, &b_sq) || alg_mul_overflow(4, a, &four_ac) || alg_mul_overflow(four_ac, c, &four_ac) ||
        alg_sub_overflow(b_sq, four_ac, &disc)) {
        /* int64 溢出时使用 __int128 精确计算判别式符号，
         * 避免 double 近似可能导致的符号误判。 */
        int sign = 0;
        __int128 b_128 = b;
        __int128 a_128 = a;
        __int128 c_128 = c;
        __int128 disc_128 = b_128 * b_128 - (__int128) 4 * a_128 * c_128;
        if (disc_128 > 0)
            sign = 1;
        else if (disc_128 < 0)
            sign = -1;
        return sign >= 0;
    }
    return disc >= 0;
}
