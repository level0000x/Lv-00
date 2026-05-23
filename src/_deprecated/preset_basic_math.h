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

/**
 * @brief 加法运算 a + b
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1) | 可逆: 是
 */
#define PRESET_ARITHMETIC_ADD            "arithmetic_add"

/**
 * @brief 减法运算 a - b
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1) | 可逆: 是
 */
#define PRESET_ARITHMETIC_SUBTRACT       "arithmetic_subtract"

/**
 * @brief 乘法运算 a * b
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1)
 */
#define PRESET_ARITHMETIC_MULTIPLY       "arithmetic_multiply"

/**
 * @brief 除法运算 a / b（b != 0）
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1)
 */
#define PRESET_ARITHMETIC_DIVIDE         "arithmetic_divide"

/**
 * @brief 幂运算 a^n
 * @details 数学定义：整数次幂，当 n >= 0 时使用快速幂算法。
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(log n)
 */
#define PRESET_ARITHMETIC_POWER          "arithmetic_power"

/**
 * @brief 开方运算 n√a（a 的第 n 次实根）
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(log(1/epsilon))
 */
#define PRESET_ARITHMETIC_ROOT           "arithmetic_root"

/**
 * @brief 对数运算 log_b(a)（以 b 为底 a 的对数，a > 0, b > 0, b != 1）
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1)
 */
#define PRESET_ARITHMETIC_LOGARITHM      "arithmetic_logarithm"

/**
 * @brief 模运算 a mod n（a 对 n 取模，n > 0）
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(1)
 */
#define PRESET_ARITHMETIC_MODULAR        "arithmetic_modular"

/* -------------------- 数论基础 -------------------- */

/**
 * @brief 最大公约数 gcd(a, b)
 * @details 数学定义：a 和 b 的最大正整数因子，使用欧几里得算法计算。
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(log(min(a,b)))
 */
#define PRESET_NUMBER_GCD                "number_gcd"

/**
 * @brief 最小公倍数 lcm(a, b) = |a*b| / gcd(a, b)
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(log(min(a,b)))
 */
#define PRESET_NUMBER_LCM                "number_lcm"

/**
 * @brief 素性检测：判定整数 n 是否为素数
 * @details 数学定义：n > 1 且仅能被 1 和自身整除，使用 Miller-Rabin 概率算法。
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(k log^3 n)，k 为测试轮数
 */
#define PRESET_NUMBER_PRIME_CHECK        "number_prime_check"

/**
 * @brief 质因数分解：将整数 n 分解为质因数的乘积
 * @details 数学定义：n = product_i p_i^{e_i}，p_i 为质数。
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_LIST | 复杂度: O(sqrt(n))
 */
#define PRESET_NUMBER_PRIME_FACTORIZATION "number_prime_factorization"

/**
 * @brief 欧拉函数 phi(n) = |{k in [1,n] : gcd(k,n) = 1}|
 * @details 数学定义：小于 n 且与 n 互素的正整数个数。
 *          phi(p^k) = p^k - p^{k-1}（p 为素数），phi(mn) = phi(m)*phi(n)（互素时）。
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(sqrt(n))
 */
#define PRESET_NUMBER_EULER_TOTIENT      "number_euler_totient"

/**
 * @brief 模逆元：a^{-1} mod n，满足 (a * a^{-1}) ≡ 1 (mod n)
 * @details 数学定义：需 gcd(a, n) = 1 才存在，使用扩展欧几里得算法求解。
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(log n)
 */
#define PRESET_NUMBER_MODULAR_INVERSE    "number_modular_inverse"

/* -------------------- 组合计数 -------------------- */

/**
 * @brief 排列数 P(n, k) = n! / (n-k)!
 * @details 数学定义：从 n 个不同元素中取 k 个的有序排列数，n >= k >= 0。
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(k)
 */
#define PRESET_COMBIN_PERMUTATION        "combin_permutation"

/**
 * @brief 组合数 C(n, k) = n! / (k! * (n-k)!)
 * @details 数学定义：从 n 个不同元素中取 k 个的无序组合数，满足对称性 C(n,k) = C(n,n-k)。
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(min(k, n-k))
 */
#define PRESET_COMBIN_COMBINATION        "combin_combination"

/**
 * @brief 第一类 Stirling 数 s(n, k)（无符号）
 * @details 数学定义：n 个元素的排列中恰好包含 k 个轮换的排列数。
 *          递推：s(n,k) = s(n-1,k-1) + (n-1)*s(n-1,k)。
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(nk)
 */
#define PRESET_COMBIN_STIRLING_FIRST     "combin_stirling_first"

/**
 * @brief 第二类 Stirling 数 S(n, k) = {n atop k}
 * @details 数学定义：将 n 个不同元素划分到 k 个非空子集的方案数。
 *          递推：S(n,k) = S(n-1,k-1) + k*S(n-1,k)。
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(nk)
 */
#define PRESET_COMBIN_STIRLING_SECOND    "combin_stirling_second"

/**
 * @brief 整数分拆数 p(n)：将 n 写成正整数之和的不同方式数
 * @details 数学定义：p(4) = 5（4, 3+1, 2+2, 2+1+1, 1+1+1+1），
 *          使用 Euler 五边形数定理加速计算。
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(n sqrt(n))
 */
#define PRESET_COMBIN_PARTITION          "combin_partition"

/**
 * @brief Catalan 数 C_n = C(2n, n) / (n+1)
 * @details 数学定义：第 n 个 Catalan 数，计数有效括号匹配、二叉搜索树数量等。
 *          C_0=1, C_1=1, C_2=2, C_3=5, C_4=14。
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(n)
 */
#define PRESET_COMBIN_CATALAN            "combin_catalan"

/* -------------------- 数列与级数 -------------------- */

/**
 * @brief 等差数列求和 S_n = n*(a_1 + a_n)/2
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER
 *       输出: PRESET_TYPE_SCALAR | 复杂度: O(1)
 */
#define PRESET_SEQUENCE_ARITHMETIC_SUM   "sequence_arithmetic_sum"

/**
 * @brief 等比数列求和 S_n = a1*(1 - r^n)/(1 - r), r != 1
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER
 *       输出: PRESET_TYPE_SCALAR | 复杂度: O(log n)
 */
#define PRESET_SEQUENCE_GEOMETRIC_SUM    "sequence_geometric_sum"

/**
 * @brief Fibonacci 数列第 n 项 F_n，F_0=0, F_1=1, F_{n+2}=F_{n+1}+F_n
 * @details 数学定义：使用矩阵快速幂或 Binet 公式 O(log n) 计算。
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(log n)
 */
#define PRESET_SEQUENCE_FIBONACCI        "sequence_fibonacci"

/**
 * @brief 二项式系数 C(n, k)
 * @details 数学定义：同组合数，(1+x)^n 展开式中 x^k 的系数。
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(k)
 */
#define PRESET_SEQUENCE_BINOMIAL_COEFFICIENT "sequence_binomial_coefficient"

/**
 * @brief 调和数 H_n = sum_{k=1}^n 1/k
 * @details 数学定义：前 n 个倒数的部分和，渐近于 ln(n) + gamma（Euler 常数）。
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
#define PRESET_SEQUENCE_HARMONIC_NUMBER  "sequence_harmonic_number"

/* -------------------- 特殊函数 -------------------- */

/**
 * @brief 阶乘 n! = product_{k=1}^n k
 * @details 数学定义：0! = 1，n! = Gamma(n+1)。
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_INTEGER | 复杂度: O(n)
 */
#define PRESET_SPECIAL_FACTORIAL         "special_factorial"

/**
 * @brief Gamma 函数 Gamma(z) = integral_0^infinity t^{z-1} * e^{-t} dt
 * @details 数学定义：阶乘的连续推广，Gamma(n) = (n-1)!（n 为正整数），
 *          Gamma(1/2) = sqrt(pi)。
 * @note 输入: PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1)（使用数值近似）
 */
#define PRESET_SPECIAL_GAMMA             "special_gamma"

/**
 * @brief Beta 函数 B(x, y) = Gamma(x) * Gamma(y) / Gamma(x+y)
 * @details 数学定义：B(x,y) = integral_0^1 t^{x-1} * (1-t)^{y-1} dt，
 *          x, y > 0。
 * @note 输入: PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(1)
 */
#define PRESET_SPECIAL_BETA              "special_beta"

/**
 * @brief Bernoulli 数 B_n
 * @details 数学定义：由生成函数 t/(e^t - 1) = sum_{n=0}^infinity B_n * t^n/n! 定义，
 *          B_0=1, B_1=-1/2, B_2=1/6, B_4=-1/30, B_{2k+1}=0 (k>=1)。
 * @note 输入: PRESET_TYPE_INTEGER | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n^2)
 */
#define PRESET_SPECIAL_BERNOULLI         "special_bernoulli"

/**
 * @brief Legendre 多项式 P_n(x) = (1/(2^n*n!)) * d^n/dx^n [(x^2-1)^n]
 * @details 数学定义：Rodrigues 公式定义的正交多项式，
 *          在 [-1,1] 上关于权重 w(x)=1 正交。
 * @note 输入: PRESET_TYPE_INTEGER, PRESET_TYPE_SCALAR | 输出: PRESET_TYPE_SCALAR | 复杂度: O(n)
 */
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
