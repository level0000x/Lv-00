/**
 * @file preset_basic_math.h
 * @brief 基础数学计算预设函数块 - 头文件
 *
 * 提供理论数学研究中常用的基础数学计算预设函数块，包括：
 *   - 算术运算：加法、减法、乘法、除法、幂运算、开方、对数、模运算
 *   - 数论基础：最大公约数、最小公倍数、素性检测、质因数分解、欧拉函数、模逆元
 *   - 组合计数：排列数、组合数、Stirling数、整数分拆数、Catalan数
 *   - 数列与级数：等差数列求和、等比数列求和、Fibonacci数列、二项式系数、调和数
 *   - 特殊函数：阶乘、Gamma函数、Beta函数、Bernoulli数、Legendre多项式
 *
 * @module BasicMath
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#ifndef PRESET_BASIC_MATH_H
#define PRESET_BASIC_MATH_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义
 * ============================================================ */

/* -------------------- 算术运算 -------------------- */

/** 加法运算：a + b */
#define PRESET_ARITHMETIC_ADD            "arithmetic_add"

/** 减法运算：a - b */
#define PRESET_ARITHMETIC_SUBTRACT       "arithmetic_subtract"

/** 乘法运算：a * b */
#define PRESET_ARITHMETIC_MULTIPLY       "arithmetic_multiply"

/** 除法运算：a / b */
#define PRESET_ARITHMETIC_DIVIDE         "arithmetic_divide"

/** 幂运算：a^n */
#define PRESET_ARITHMETIC_POWER          "arithmetic_power"

/** 开方运算：n√a */
#define PRESET_ARITHMETIC_ROOT           "arithmetic_root"

/** 对数运算：log_b(a) */
#define PRESET_ARITHMETIC_LOGARITHM      "arithmetic_logarithm"

/** 模运算：a mod n */
#define PRESET_ARITHMETIC_MODULAR        "arithmetic_modular"

/* -------------------- 数论基础 -------------------- */

/** 最大公约数：gcd(a, b) */
#define PRESET_NUMBER_GCD                "number_gcd"

/** 最小公倍数：lcm(a, b) */
#define PRESET_NUMBER_LCM                "number_lcm"

/** 素性检测 */
#define PRESET_NUMBER_PRIME_CHECK        "number_prime_check"

/** 质因数分解 */
#define PRESET_NUMBER_PRIME_FACTORIZATION "number_prime_factorization"

/** 欧拉函数：phi(n) */
#define PRESET_NUMBER_EULER_TOTIENT      "number_euler_totient"

/** 模逆元 */
#define PRESET_NUMBER_MODULAR_INVERSE    "number_modular_inverse"

/* -------------------- 组合计数 -------------------- */

/** 排列数：P(n, k) */
#define PRESET_COMBIN_PERMUTATION        "combin_permutation"

/** 组合数：C(n, k) */
#define PRESET_COMBIN_COMBINATION        "combin_combination"

/** 第一类Stirling数 */
#define PRESET_COMBIN_STIRLING_FIRST     "combin_stirling_first"

/** 第二类Stirling数 */
#define PRESET_COMBIN_STIRLING_SECOND    "combin_stirling_second"

/** 整数分拆数 */
#define PRESET_COMBIN_PARTITION          "combin_partition"

/** Catalan数 */
#define PRESET_COMBIN_CATALAN            "combin_catalan"

/* -------------------- 数列与级数 -------------------- */

/** 等差数列求和 */
#define PRESET_SEQUENCE_ARITHMETIC_SUM   "sequence_arithmetic_sum"

/** 等比数列求和 */
#define PRESET_SEQUENCE_GEOMETRIC_SUM    "sequence_geometric_sum"

/** Fibonacci数列第n项 */
#define PRESET_SEQUENCE_FIBONACCI        "sequence_fibonacci"

/** 二项式系数 */
#define PRESET_SEQUENCE_BINOMIAL_COEFFICIENT "sequence_binomial_coefficient"

/** 调和数H_n */
#define PRESET_SEQUENCE_HARMONIC_NUMBER  "sequence_harmonic_number"

/* -------------------- 特殊函数 -------------------- */

/** 阶乘：n! */
#define PRESET_SPECIAL_FACTORIAL         "special_factorial"

/** Gamma函数：Gamma(z) */
#define PRESET_SPECIAL_GAMMA             "special_gamma"

/** Beta函数：B(x, y) */
#define PRESET_SPECIAL_BETA              "special_beta"

/** Bernoulli数 */
#define PRESET_SPECIAL_BERNOULLI         "special_bernoulli"

/** Legendre多项式 */
#define PRESET_SPECIAL_LEGENDRE          "special_legendre"

/* ============================================================
 * 模块注册函数
 * ============================================================ */

/**
 * @brief 注册所有基础数学预设函数块
 *
 * 将基础数学模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_basic_math_register(void);

/**
 * @brief 获取基础数学预设函数块数量
 *
 * @return int 基础数学模块预设函数块总数
 */
int preset_basic_math_count(void);

/**
 * @brief 获取基础数学预设函数块名称列表
 *
 * 返回所有已注册的基础数学预设名称数组。
 * 调用者负责释放返回的名称数组和每个名称字符串。
 *
 * @param out_names 输出名称数组（需调用者释放）
 * @param out_count 输出名称数量
 * @return true 获取成功
 * @return false 参数无效或内存不足
 */
bool preset_basic_math_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取基础数学模块的预设类别
 *
 * @return PresetCategory 基础数学模块所属类别
 */
PresetCategory preset_basic_math_category(void);

#ifdef __cplusplus
}
#endif

#endif /* PRESET_BASIC_MATH_H */
