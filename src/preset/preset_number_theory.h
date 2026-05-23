/**
 * @file preset_number_theory.h
 * @brief 数论预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的数论运算预设函数块，包括：
 *   - 整数运算：最大公约数、最小公倍数、扩展欧几里得
 *   - 素数相关：素性检测、素因子分解、素数生成
 *   - 同余运算：模运算、中国剩余定理、离散对数
 *   - 数论函数：欧拉函数、莫比乌斯函数、约数函数
 *   - 二次剩余：勒让德符号、雅可比符号、二次剩余判定
 *
 * @module NumberTheory
 * @category PRESET_CATEGORY_NUMBER_THEORY
 * @version 4.0.0
 * @author Lv-00 开发团队
 */

#ifndef LV00_PRESET_NUMBER_THEORY_H
#define LV00_PRESET_NUMBER_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 基础整数运算 -------------------- */

/** 最大公约数：gcd(a, b) */
#define PRESET_GCD                     "gcd"

/** 最小公倍数：lcm(a, b) */
#define PRESET_LCM                     "lcm"

/** 扩展欧几里得：求解 ax + by = gcd(a,b) */
#define PRESET_EXTENDED_GCD            "extended_gcd"

/** 模逆元：求解 a^(-1) mod m */
#define PRESET_MODULAR_INVERSE         "modular_inverse"
/** 模加法：(a + b) mod m */
#define PRESET_MODULAR_ADD              "modular_add"
/** 模乘法：(a * b) mod m */
#define PRESET_MODULAR_MULTIPLY         "modular_multiply"
/** Wilson素数判定：(n-1)! ≡ -1 (mod n) */
#define PRESET_WILSON_TEST              "wilson_test"

/** 模幂运算：a^b mod m */
#define PRESET_MODULAR_EXPONENTIATION  "modular_exp"

/* -------------------- 素数相关 -------------------- */

/** 素性检测（Miller-Rabin） */
#define PRESET_PRIMALITY_TEST          "primality_test"

/** 素因子分解 */
#define PRESET_PRIME_FACTORIZATION     "prime_factorization"

/** 生成下一个素数 */
#define PRESET_NEXT_PRIME              "next_prime"

/** 欧拉筛法生成素数列表 */
#define PRESET_SIEVE_OF_ERATOSTHENES   "sieve_eratosthenes"

/* -------------------- 同余运算 -------------------- */

/** 中国剩余定理求解 */
#define PRESET_CHINESE_REMAINDER       "chinese_remainder"

/** 离散对数（Shanks算法） */
#define PRESET_DISCRETE_LOGARITHM      "discrete_log"

/** 阶计算：a模m的阶 */
#define PRESET_MULTIPLICATIVE_ORDER    "multiplicative_order"

/** 原根检测与生成 */
#define PRESET_PRIMITIVE_ROOT          "primitive_root"

/* -------------------- 数论函数 -------------------- */

/** 欧拉函数：φ(n) */
#define PRESET_EULER_TOTIENT           "euler_totient"

/** 莫比乌斯函数：μ(n) */
#define PRESET_MOBIUS_FUNCTION         "mobius_function"

/** 约数个数：d(n) */
#define PRESET_DIVISOR_COUNT           "divisor_count"

/** 约数和：σ(n) */
#define PRESET_DIVISOR_SUM             "divisor_sum"

/** 最大约数（不含自身） */
#define PRESET_LARGEST_PROPER_DIVISOR  "largest_proper_divisor"

/* -------------------- 二次剩余 -------------------- */

/** 勒让德符号：(a/p) */
#define PRESET_LEGENDRE_SYMBOL         "legendre_symbol"

/** 雅可比符号：(a/n) */
#define PRESET_JACOBI_SYMBOL           "jacobi_symbol"

/** 二次剩余判定 */
#define PRESET_QUADRATIC_RESIDUE_TEST  "quadratic_residue_test"

/** 平方根模素数（Tonelli-Shanks） */
#define PRESET_TONELLI_SHANKS          "tonelli_shanks"

/* -------------------- 特殊数列 -------------------- */

/** 斐波那契数 */
#define PRESET_FIBONACCI               "fibonacci"

/** 卢卡斯数 */
#define PRESET_LUCAS                   "lucas"

/** 佩尔方程求解 */
#define PRESET_PELL_EQUATION           "pell_equation"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有数论预设函数块
 *
 * 将数论模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_number_theory_register(void);

/**
 * @brief 获取数论预设函数块数量
 *
 * @return int 数论模块预设函数块总数
 */
int preset_number_theory_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_NUMBER_THEORY_H */
