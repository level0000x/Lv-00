/**
 * @file lv_arith_safe.h
 * @brief Lv-00 公共整数算术工具 —— GCD/约分 与 溢出安全算术
 *
 * @details 收敛代码库中重复的「GCD/分数约分」与「溢出安全算术」实现：
 *          - algebraic_number_util.c 的 alg_gcd/alg_lcm/alg_*_overflow
 *          - exact_arithmetic.c 的 lv_safe_*_impl
 *          - formula_dsl.c / formula_converter_util.c 的内联欧几里得约分
 *
 *          全部为 static inline 纯函数（无需 .c 文件，无外部链接符号）。
 *
 *          GCD 采用 uint64_t 安全语义处理 INT64_MIN 的绝对值：
 *          |INT64_MIN| = 2^63 无法用 int64 表示，在 uint64_t 中安全表示为
 *          (uint64_t)INT64_MAX + 1（见 formula_converter_util.c 的正确处理），
 *          不使用 INT64_MAX 近似（原 alg_gcd 的近似会导致约分不彻底）。
 *
 *          溢出检测函数返回 true = 成功（与 exact_arithmetic.c 的
 *          lv_safe_*_impl 语义一致）；溢出时 *out 不被修改。
 *
 * @version 1.0.0
 * @copyright Copyright (c) 2024-2026 Lv-00 Project
 */

#ifndef lv_ARITH_SAFE_H
#define lv_ARITH_SAFE_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 内部辅助（static inline，不对外承诺）
 * ============================================================ */

/** @brief |a| 的 uint64_t 表示；INT64_MIN 的绝对值 2^63 安全容纳 */
static inline uint64_t lv_abs_u64_i64(int64_t a) {
    return (a == INT64_MIN) ? ((uint64_t) INT64_MAX + 1ULL) : (uint64_t) (a < 0 ? -a : a);
}

/** @brief uint64_t 域内的欧几里得最大公约数（b 可为 0） */
static inline uint64_t lv_gcd_u64(uint64_t a, uint64_t b) {
    while (b != 0) {
        uint64_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/* ============================================================
 * GCD / LCM
 * ============================================================ */

/**
 * @brief 计算 int64_t 的最大公约数：gcd(a, b)
 *
 * @details 对负数取绝对值后计算，绝对值用 uint64_t 表示，
 *          INT64_MIN 安全处理为 (uint64_t)INT64_MAX + 1（真值 2^63），
 *          保证结果的正确性（不做 INT64_MAX 近似）。
 *
 * @param a 整数
 * @param b 整数
 * @return gcd(|a|, |b|)，恒非负。
 *         唯一边界：gcd(INT64_MIN, INT64_MIN) 的真值 2^63 超出 int64
 *         可表示范围，按位模式返回（两补码平台上为 INT64_MIN）；
 *         该情形在约分场景（q > 0）中不会出现。
 */
static inline int64_t lv_gcd_i64(int64_t a, int64_t b) {
    uint64_t ua = lv_abs_u64_i64(a);
    uint64_t ub = lv_abs_u64_i64(b);
    return (int64_t) lv_gcd_u64(ua, ub);
}

/**
 * @brief 计算 int64_t 的最小公倍数：lcm(a, b)
 *
 * @param a 整数
 * @param b 整数
 * @return |a| 与 |b| 的最小公倍数；任一为 0 时返回 0；
 *         真值超出 int64 可表示范围时饱和返回 INT64_MAX（与
 *         原 alg_lcm 的溢出约定一致）。
 */
static inline int64_t lv_lcm_i64(int64_t a, int64_t b) {
    if (a == 0 || b == 0)
        return 0;
    uint64_t ua = lv_abs_u64_i64(a);
    uint64_t ub = lv_abs_u64_i64(b);
    uint64_t g = lv_gcd_u64(ua, ub);
    /* 先除后乘防止中间结果溢出 */
    if (ua / g > (uint64_t) INT64_MAX / ub)
        return INT64_MAX; /* 溢出时返回上限 */
    return (int64_t) ((ua / g) * ub);
}

/**
 * @brief 约分有理数 p/q 为最简形式，并保证 q > 0
 *
 * @details 语义与 formula_converter_util.c 的 uint64 安全约分一致
 *          （gcd 用真值计算，含 INT64_MIN 绝对值 2^63）。
 *
 * @param p 分子指针（原地修改）
 * @param q 分母指针（原地修改；调用者需确保 q != 0）
 */
static inline void lv_rational_simplify_i64(int64_t *p, int64_t *q) {
    if (*q < 0) {
        *p = -*p;
        *q = -*q;
    }
    int64_t g = lv_gcd_i64(*p, *q);
    if (g > 1) {
        *p /= g;
        *q /= g;
    } else if (g == 0 && *q != 0) {
        /* 与历史实现保持一致的兜底分支（gcd(0, q) = |q|，
         * 已被 g > 1 / g == 1 分支覆盖，正常不可达）：
         * 0/q 约分为 0/1 */
        *p /= *q;
        *q = 1;
    }
}

/* ============================================================
 * 溢出安全算术（返回 true = 成功，与 exact_arithmetic.c 语义一致）
 * ============================================================ */

/**
 * @brief 安全乘法：a * b，检测溢出
 *
 * @param a   乘数
 * @param b   乘数
 * @param out 输出指针；溢出或 NULL 时返回 false 且 *out 不被修改
 * @return true 成功（无溢出），false 溢出或 out 为 NULL
 */
static inline bool lv_safe_mul_i64(int64_t a, int64_t b, int64_t *out) {
    if (!out)
        return false;
    if (a == 0 || b == 0) {
        *out = 0;
        return true;
    }
    /* 溢出条件：|a * b| > INT64_MAX */
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

/**
 * @brief 安全加法：a + b，检测溢出
 *
 * @param a   加数
 * @param b   加数
 * @param out 输出指针；溢出或 NULL 时返回 false 且 *out 不被修改
 * @return true 成功（无溢出），false 溢出或 out 为 NULL
 */
static inline bool lv_safe_add_i64(int64_t a, int64_t b, int64_t *out) {
    if (!out)
        return false;
    /* 正溢出: a > 0 && b > 0 && a > INT64_MAX - b
     * 负溢出: a < 0 && b < 0 && a < INT64_MIN - b */
    if (b > 0 && a > INT64_MAX - b)
        return false;
    if (b < 0 && a < INT64_MIN - b)
        return false;
    *out = a + b;
    return true;
}

/**
 * @brief 安全减法：a - b，检测溢出
 *
 * @param a   被减数
 * @param b   减数
 * @param out 输出指针；溢出或 NULL 时返回 false 且 *out 不被修改
 * @return true 成功（无溢出），false 溢出或 out 为 NULL
 */
static inline bool lv_safe_sub_i64(int64_t a, int64_t b, int64_t *out) {
    if (!out)
        return false;
    /* 正溢出: b < 0 && a > INT64_MAX + b  （减负数可能溢出）
     * 负溢出: b > 0 && a < INT64_MIN + b  （减正数可能下溢） */
    if (b < 0 && a > INT64_MAX + b)
        return false;
    if (b > 0 && a < INT64_MIN + b)
        return false;
    *out = a - b;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* lv_ARITH_SAFE_H */
