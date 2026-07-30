/**
 * @file exact_arithmetic.c
 * @brief 精确算术基础设施实现 —— 时间戳、安全幂运算
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-05-24
 */

#include "lv/lv_platform.h"

#include "exact_arithmetic.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv_utils.h"

/* ========================================================================
 * 时间戳实现
 * ======================================================================== */

lvTimestamp lv_timestamp_now(void) {
    lvTimestamp ts;
    uint64_t ns = lv_get_time_ns();
    ts.seconds = (int64_t) (ns / 1000000000ULL);
    ts.nanoseconds = (int64_t) (ns % 1000000000ULL);
    return ts;
}

/* ========================================================================
 * 安全乘法
 * ======================================================================== */

/**
 * @brief 安全乘法 —— a * b，检测溢出
 */
bool lv_safe_mul_impl(int64_t a, int64_t b, int64_t *out) {
    if (!out)
        return false;
    if (a == 0 || b == 0) {
        *out = 0;
        return true;
    }
    /* 检查是否会溢出: |a * b| > INT64_MAX */
    if (a > 0 && b > 0 && a > INT64_MAX / b)
        return false;
    if (a > 0 && b < 0 && b < INT64_MIN / a)
        return false;
    if (a < 0 && b > 0 && a < INT64_MIN / b)
        return false;
    if (a < 0 && b < 0 && a < INT64_MAX / b)
        return false;
    *out = a * b;
    return true;
}

/* ========================================================================
 * 安全幂运算
 * ======================================================================== */

/**
 * @brief 安全取幂 —— a^b
 *
 * 使用快速幂算法（exponentiation by squaring）。
 * 每次乘法前检查溢出。
 *
 * @param a      底数
 * @param b      指数（必须 >= 0）
 * @param result 输出 a^b
 * @return true 成功，false 溢出或 b < 0
 */
bool lv_safe_pow(int64_t a, int64_t b, int64_t *result) {
    if (!result)
        return false;
    if (b < 0)
        return false; /* 负指数不支持（结果不是整数） */

    if (b == 0) {
        *result = 1;
        return true;
    }

    int64_t base = a;
    int64_t exp = b;
    int64_t res = 1;

    while (exp > 0) {
        if (exp & 1) {
            /* res *= base */
            if (!lv_safe_mul_impl(res, base, &res))
                return false;
        }
        exp >>= 1;
        if (exp > 0) {
            /* base *= base */
            if (!lv_safe_mul_impl(base, base, &base))
                return false;
        }
    }

    *result = res;
    return true;
}

/* ========================================================================
 * 安全加法
 * ======================================================================== */

/**
 * @brief 安全加法 -- a + b，检测溢出
 *
 * @param a  加数 a
 * @param b  加数 b
 * @param out 输出：a + b 的结果
 * @return true 成功（无溢出），false 溢出或 out 为 NULL
 */
bool lv_safe_add_check_impl(int64_t a, int64_t b, int64_t *out) {
    if (!out)
        return false;

    /* 溢出条件检测：
     * 正溢出: a > 0 && b > 0 && a > INT64_MAX - b
     * 负溢出: a < 0 && b < 0 && a < INT64_MIN - b */
    if (b > 0 && a > INT64_MAX - b)
        return false;
    if (b < 0 && a < INT64_MIN - b)
        return false;

    *out = a + b;
    return true;
}

/* ========================================================================
 * 安全减法
 * ======================================================================== */

/**
 * @brief 安全减法 -- a - b，检测溢出
 *
 * @param a  被减数 a
 * @param b  减数 b
 * @param out 输出：a - b 的结果
 * @return true 成功（无溢出），false 溢出或 out 为 NULL
 */
bool lv_safe_sub_impl(int64_t a, int64_t b, int64_t *out) {
    if (!out)
        return false;

    /* 溢出条件检测：
     * 正溢出: b < 0 && a > INT64_MAX + b  (减负数可能溢出)
     * 负溢出: b > 0 && a < INT64_MIN + b  (减正数可能下溢) */
    if (b < 0 && a > INT64_MAX + b)
        return false;
    if (b > 0 && a < INT64_MIN + b)
        return false;

    *out = a - b;
    return true;
}
