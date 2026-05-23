/**
 * @file preset_ring_theory.h
 * @brief 环论预设函数块
 *
 * 提供理论数学研究中常用的环论运算预设函数块，包括：
 * - 环基础运算：环加法、环乘法、加法逆元、零元、单位元、环减法、环幂运算、环特征、子环判定、理想判定
 * - 理想相关：理想和、理想积、理想交、理想商、由集合生成理想、主理想、极大理想判定、素理想判定、幂零根、Jacobson根
 * - 特殊环：多项式环、矩阵环、商环、环同态、同态核、同态像、环同构判定、中国剩余定理
 * - 多项式环运算：多项式次数、多项式最大公因式、多项式最小公倍数、多项式除法、欧几里得算法、不可约性判定、多项式因式分解
 *
 * @module RingTheory
 * @category PRESET_CATEGORY_RING_THEORY
 */

#ifndef LV00_PRESET_RING_THEORY_H
#define LV00_PRESET_RING_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 环基础运算 ==================== */

/**
 * @brief 环加法
 *
 * 数学定义：给定环 $R$ 中两个元素 $a, b$，计算 $a + b$
 *
 * 输入：
 *   - a: 环元素 (PRESET_TYPE_RING)
 *   - b: 环元素 (PRESET_TYPE_RING)
 * 输出：
 *   - result: 环元素 (PRESET_TYPE_RING) - a + b
 *
 * 复杂度：O(1)
 */
#define PRESET_RING_ADD "ring_add"

/**
 * @brief 环乘法
 *
 * 数学定义：给定环 $R$ 中两个元素 $a, b$，计算 $a \cdot b$
 *
 * 输入：
 *   - a: 环元素 (PRESET_TYPE_RING)
 *   - b: 环元素 (PRESET_TYPE_RING)
 * 输出：
 *   - result: 环元素 (PRESET_TYPE_RING) - a * b
 *
 * 复杂度：O(1)
 */
#define PRESET_RING_MULTIPLY "ring_multiply"

/**
 * @brief 加法逆元
 *
 * 数学定义：给定环 $R$ 中元素 $a$，计算其加法逆元 $-a$，满足 $a + (-a) = 0_R$
 *
 * 输入：
 *   - a: 环元素 (PRESET_TYPE_RING)
 * 输出：
 *   - result: 环元素 (PRESET_TYPE_RING) - -a
 *
 * 复杂度：O(1)
 */
#define PRESET_RING_NEGATE "ring_negate"

/**
 * @brief 零元
 *
 * 数学定义：获取环 $R$ 的加法单位元（零元）$0_R$
 *
 * 输入：
 *   - R: 环 (PRESET_TYPE_RING)
 * 输出：
 *   - result: 环元素 (PRESET_TYPE_RING) - 零元 0_R
 *
 * 复杂度：O(1)
 */
#define PRESET_RING_ZERO "ring_zero"

/**
 * @brief 单位元
 *
 * 数学定义：获取环 $R$ 的乘法单位元 $1_R$（若存在）
 *
 * 输入：
 *   - R: 环 (PRESET_TYPE_RING)
 * 输出：
 *   - result: 环元素 (PRESET_TYPE_RING) - 乘法单位元 1_R
 *
 * 复杂度：O(1)
 */
#define PRESET_RING_ONE "ring_one"

/**
 * @brief 环减法
 *
 * 数学定义：计算环 $R$ 中两个元素的差 $a - b = a + (-b)$
 *
 * 输入：
 *   - a: 环元素 (PRESET_TYPE_RING)
 *   - b: 环元素 (PRESET_TYPE_RING)
 * 输出：
 *   - result: 环元素 (PRESET_TYPE_RING) - a - b
 *
 * 复杂度：O(1)
 */
#define PRESET_RING_SUBTRACT "ring_subtract"

/**
 * @brief 环幂运算
 *
 * 数学定义：计算环元素的整数幂 $a^n$，使用快速幂算法
 *
 * 输入：
 *   - a: 环元素 (PRESET_TYPE_RING)
 *   - n: 整数指数 (PRESET_TYPE_INTEGER)
 * 输出：
 *   - result: 环元素 (PRESET_TYPE_RING) - a^n
 *
 * 复杂度：O(log n)
 */
#define PRESET_RING_POWER "ring_power"

/**
 * @brief 环特征
 *
 * 数学定义：计算环 $R$ 的特征 $\text{char}(R)$，
 * 即满足 $n \cdot 1_R = 0_R$ 的最小正整数 $n$
 *
 * 输入：
 *   - R: 环 (PRESET_TYPE_RING)
 * 输出：
 *   - characteristic: 整数 (PRESET_TYPE_INTEGER) - 环特征
 *
 * 复杂度：O(1)
 */
#define PRESET_RING_CHARACTERISTIC "ring_characteristic"

/**
 * @brief 子环判定
 *
 * 数学定义：判定环 $R$ 的子集 $S$ 是否构成子环
 *
 * 输入：
 *   - R: 环 (PRESET_TYPE_RING)
 *   - S: 子集 (PRESET_TYPE_SET)
 * 输出：
 *   - is_subring: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^2)
 */
#define PRESET_RING_SUBRING_CHECK "ring_subring_check"

/**
 * @brief 理想判定
 *
 * 数学定义：判定环 $R$ 的子集 $I$ 是否构成理想
 *
 * 输入：
 *   - R: 环 (PRESET_TYPE_RING)
 *   - I: 子集 (PRESET_TYPE_SET)
 * 输出：
 *   - is_ideal: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^2)
 */
#define PRESET_RING_IDEAL_CHECK "ring_ideal_check"

/* ==================== 理想运算 ==================== */

/**
 * @brief 理想和
 *
 * 数学定义：$I + J = \{a + b : a \in I, b \in J\}$
 *
 * 输入：
 *   - I: 理想 (PRESET_TYPE_IDEAL)
 *   - J: 理想 (PRESET_TYPE_IDEAL)
 * 输出：
 *   - sum: 理想 (PRESET_TYPE_IDEAL) - I + J
 *
 * 复杂度：O(n)
 */
#define PRESET_IDEAL_SUM "ideal_sum"

/**
 * @brief 理想积
 *
 * 数学定义：$I \cdot J = \{\sum_{k=1}^{n} a_k b_k : a_k \in I, b_k \in J\}$
 *
 * 输入：
 *   - I: 理想 (PRESET_TYPE_IDEAL)
 *   - J: 理想 (PRESET_TYPE_IDEAL)
 * 输出：
 *   - product: 理想 (PRESET_TYPE_IDEAL) - I * J
 *
 * 复杂度：O(n^2)
 */
#define PRESET_IDEAL_PRODUCT "ideal_product"

/**
 * @brief 理想交
 *
 * 数学定义：$I \cap J = \{a : a \in I \land a \in J\}$
 *
 * 输入：
 *   - I: 理想 (PRESET_TYPE_IDEAL)
 *   - J: 理想 (PRESET_TYPE_IDEAL)
 * 输出：
 *   - intersection: 理想 (PRESET_TYPE_IDEAL) - I ∩ J
 *
 * 复杂度：O(n)
 */
#define PRESET_IDEAL_INTERSECTION "ideal_intersection"

/**
 * @brief 理想商
 *
 * 数学定义：$(I : J) = \{x \in R : xJ \subseteq I\}$
 *
 * 输入：
 *   - I: 理想 (PRESET_TYPE_IDEAL)
 *   - J: 理想 (PRESET_TYPE_IDEAL)
 * 输出：
 *   - quotient: 理想 (PRESET_TYPE_IDEAL) - (I : J)
 *
 * 复杂度：O(n^2)
 */
#define PRESET_IDEAL_QUOTIENT "ideal_quotient"

/**
 * @brief 由集合生成理想
 *
 * 数学定义：由元素集生成理想 $\langle a_1, \ldots, a_n \rangle$
 *
 * 输入：
 *   - S: 元素集合 (PRESET_TYPE_SET)
 * 输出：
 *   - ideal: 理想 (PRESET_TYPE_IDEAL) - 生成的理想
 *
 * 复杂度：O(n)
 */
#define PRESET_IDEAL_GENERATE "ideal_generate"

/**
 * @brief 主理想
 *
 * 数学定义：由单个元素 $a$ 生成的主理想 $\langle a \rangle = aR$
 *
 * 输入：
 *   - a: 环元素 (PRESET_TYPE_RING)
 * 输出：
 *   - ideal: 理想 (PRESET_TYPE_IDEAL) - 主理想 <a>
 *
 * 复杂度：O(1)
 */
#define PRESET_PRINCIPAL_IDEAL "principal_ideal"

/**
 * @brief 极大理想判定
 *
 * 数学定义：判定理想 $I$ 是否为极大理想，等价条件 $R/I$ 是域
 *
 * 输入：
 *   - I: 理想 (PRESET_TYPE_IDEAL)
 *   - R: 环 (PRESET_TYPE_RING)
 * 输出：
 *   - is_maximal: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^2)
 */
#define PRESET_MAXIMAL_IDEAL_CHECK "maximal_ideal_check"

/**
 * @brief 素理想判定
 *
 * 数学定义：判定理想 $I$ 是否为素理想，等价条件 $R/I$ 是整环
 *
 * 输入：
 *   - I: 理想 (PRESET_TYPE_IDEAL)
 *   - R: 环 (PRESET_TYPE_RING)
 * 输出：
 *   - is_prime: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^2)
 */
#define PRESET_PRIME_IDEAL_CHECK "prime_ideal_check"

/**
 * @brief 幂零根
 *
 * 数学定义：$\mathfrak{N}(R) = \{a \in R : \exists n > 0, a^n = 0\}$
 *
 * 输入：
 *   - R: 环 (PRESET_TYPE_RING)
 * 输出：
 *   - nilradical: 理想 (PRESET_TYPE_IDEAL) - 幂零根
 *
 * 复杂度：O(n^3)
 */
#define PRESET_NILRADICAL "nilradical"

/**
 * @brief Jacobson根
 *
 * 数学定义：$J(R) = \bigcap_{\mathfrak{m} \in \text{MaxSpec}(R)} \mathfrak{m}$
 *
 * 输入：
 *   - R: 环 (PRESET_TYPE_RING)
 * 输出：
 *   - jacobson_radical: 理想 (PRESET_TYPE_IDEAL) - Jacobson根
 *
 * 复杂度：O(n^3)
 */
#define PRESET_JACOBSON_RADICAL "jacobson_radical"

/* ==================== 特殊环 ==================== */

/**
 * @brief 多项式环
 *
 * 数学定义：由环 $R$ 构造多项式环 $R[x]$
 *
 * 输入：
 *   - R: 系数环 (PRESET_TYPE_RING)
 * 输出：
 *   - R[x]: 环 (PRESET_TYPE_RING) - 多项式环
 *
 * 复杂度：O(1)
 */
#define PRESET_POLYNOMIAL_RING "polynomial_ring"

/**
 * @brief 矩阵环
 *
 * 数学定义：由环 $R$ 构造 $n \times n$ 矩阵环 $M_n(R)$
 *
 * 输入：
 *   - R: 系数环 (PRESET_TYPE_RING)
 *   - n: 阶数 (PRESET_TYPE_INTEGER)
 * 输出：
 *   - M_n(R): 环 (PRESET_TYPE_RING) - 矩阵环
 *
 * 复杂度：O(n^3)
 */
#define PRESET_MATRIX_RING "matrix_ring"

/**
 * @brief 商环
 *
 * 数学定义：由环 $R$ 和理想 $I$ 构造商环 $R/I$
 *
 * 输入：
 *   - R: 环 (PRESET_TYPE_RING)
 *   - I: 理想 (PRESET_TYPE_IDEAL)
 * 输出：
 *   - R/I: 环 (PRESET_TYPE_RING) - 商环
 *
 * 复杂度：O(n)
 */
#define PRESET_QUOTIENT_RING "quotient_ring"

/**
 * @brief 环同态
 *
 * 数学定义：构造或验证环同态 $f: R \to S$
 *
 * 输入：
 *   - R: 源环 (PRESET_TYPE_RING)
 *   - S: 目标环 (PRESET_TYPE_RING)
 *   - f: 映射 (PRESET_TYPE_FUNCTION)
 * 输出：
 *   - is_homomorphism: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^2)
 */
#define PRESET_RING_HOMOMORPHISM "ring_homomorphism"

/**
 * @brief 同态核
 *
 * 数学定义：$\ker(f) = \{a \in R : f(a) = 0_S\}$
 *
 * 输入：
 *   - f: 环同态 (PRESET_TYPE_FUNCTION)
 * 输出：
 *   - kernel: 理想 (PRESET_TYPE_IDEAL) - 同态核
 *
 * 复杂度：O(n)
 */
#define PRESET_RING_KERNEL "ring_kernel"

/**
 * @brief 同态像
 *
 * 数学定义：$\text{Im}(f) = \{f(a) : a \in R\}$
 *
 * 输入：
 *   - f: 环同态 (PRESET_TYPE_FUNCTION)
 * 输出：
 *   - image: 环 (PRESET_TYPE_RING) - 同态像
 *
 * 复杂度：O(n)
 */
#define PRESET_RING_IMAGE "ring_image"

/**
 * @brief 环同构判定
 *
 * 数学定义：判定两个环 $R$ 和 $S$ 是否同构
 *
 * 输入：
 *   - R: 环 (PRESET_TYPE_RING)
 *   - S: 环 (PRESET_TYPE_RING)
 * 输出：
 *   - is_isomorphic: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^2)
 */
#define PRESET_RING_ISOMORPHISM_CHECK "ring_isomorphism_check"

/**
 * @brief 中国剩余定理
 *
 * 数学定义：在互素理想下求解同余方程组的解
 *
 * 输入：
 *   - R: 环 (PRESET_TYPE_RING)
 *   - ideals: 理想列表 (PRESET_TYPE_SET)
 * 输出：
 *   - solution: 环元素 (PRESET_TYPE_RING) - 解
 *
 * 复杂度：O(n log n)
 */
#define PRESET_CHINESE_REMAINDER "chinese_remainder"

/* ==================== 多项式环运算 ==================== */

/**
 * @brief 多项式次数
 *
 * 数学定义：计算多项式 $f(x)$ 的次数 $\deg(f)$
 *
 * 输入：
 *   - f: 多项式 (PRESET_TYPE_POLYNOMIAL)
 * 输出：
 *   - degree: 整数 (PRESET_TYPE_INTEGER) - 多项式次数
 *
 * 复杂度：O(n)
 */
#define PRESET_POLYNOMIAL_DEGREE "polynomial_degree"

/**
 * @brief 多项式最大公因式
 *
 * 数学定义：使用欧几里得算法计算 $\gcd(f, g)$
 *
 * 输入：
 *   - f: 多项式 (PRESET_TYPE_POLYNOMIAL)
 *   - g: 多项式 (PRESET_TYPE_POLYNOMIAL)
 * 输出：
 *   - gcd: 多项式 (PRESET_TYPE_POLYNOMIAL) - 最大公因式
 *
 * 复杂度：O(n^2)
 */
#define PRESET_POLYNOMIAL_GCD "polynomial_gcd"

/**
 * @brief 多项式最小公倍数
 *
 * 数学定义：$\text{lcm}(f, g) = \frac{f \cdot g}{\gcd(f, g)}$
 *
 * 输入：
 *   - f: 多项式 (PRESET_TYPE_POLYNOMIAL)
 *   - g: 多项式 (PRESET_TYPE_POLYNOMIAL)
 * 输出：
 *   - lcm: 多项式 (PRESET_TYPE_POLYNOMIAL) - 最小公倍数
 *
 * 复杂度：O(n^2)
 */
#define PRESET_POLYNOMIAL_LCM "polynomial_lcm"

/**
 * @brief 多项式除法
 *
 * 数学定义：带余除法 $f = qg + r$，其中 $\deg(r) < \deg(g)$
 *
 * 输入：
 *   - f: 被除多项式 (PRESET_TYPE_POLYNOMIAL)
 *   - g: 除数多项式 (PRESET_TYPE_POLYNOMIAL)
 * 输出：
 *   - (q, r): 元组 (PRESET_TYPE_TUPLE) - 商和余式
 *
 * 复杂度：O(n^2)
 */
#define PRESET_POLYNOMIAL_DIVISION "polynomial_division"

/**
 * @brief 欧几里得算法
 *
 * 数学定义：扩展欧几里得算法求 $\gcd(f, g)$ 及系数 $u, v$ 使得 $uf + vg = \gcd(f, g)$
 *
 * 输入：
 *   - f: 多项式 (PRESET_TYPE_POLYNOMIAL)
 *   - g: 多项式 (PRESET_TYPE_POLYNOMIAL)
 * 输出：
 *   - (gcd, u, v): 元组 (PRESET_TYPE_TUPLE)
 *
 * 复杂度：O(n^2)
 */
#define PRESET_POLYNOMIAL_EUCLIDEAN "polynomial_euclidean"

/**
 * @brief 不可约性判定
 *
 * 数学定义：判定多项式 $f(x)$ 在给定环/域上是否不可约
 *
 * 输入：
 *   - f: 多项式 (PRESET_TYPE_POLYNOMIAL)
 *   - R: 系数环 (PRESET_TYPE_RING)
 * 输出：
 *   - is_irreducible: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^3)
 */
#define PRESET_POLYNOMIAL_IRREDUCIBLE_CHECK "polynomial_irreducible_check"

/**
 * @brief 多项式因式分解
 *
 * 数学定义：将多项式分解为不可约多项式的乘积
 *
 * 输入：
 *   - f: 多项式 (PRESET_TYPE_POLYNOMIAL)
 *   - R: 系数环 (PRESET_TYPE_RING)
 * 输出：
 *   - factors: 列表 (PRESET_TYPE_LIST) - 因式分解结果
 *
 * 复杂度：O(n^3)
 */
#define PRESET_POLYNOMIAL_FACTORIZATION "polynomial_factorization"

/* ==================== 向后兼容别名（旧宏名称映射） ==================== */
#ifndef PRESET_RING_ADDITION
#define PRESET_RING_ADDITION            PRESET_RING_ADD
#endif
#ifndef PRESET_RING_MULTIPLICATION
#define PRESET_RING_MULTIPLICATION      PRESET_RING_MULTIPLY
#endif
#ifndef PRESET_RING_ADDITIVE_INVERSE
#define PRESET_RING_ADDITIVE_INVERSE    PRESET_RING_NEGATE
#endif
#ifndef PRESET_RING_ZERO_ELEMENT
#define PRESET_RING_ZERO_ELEMENT        PRESET_RING_ZERO
#endif
#ifndef PRESET_RING_IDENTITY_ELEMENT
#define PRESET_RING_IDENTITY_ELEMENT    PRESET_RING_ONE
#endif
#ifndef PRESET_RING_IDEAL_TEST
#define PRESET_RING_IDEAL_TEST          PRESET_RING_IDEAL_CHECK
#endif
#ifndef PRESET_RING_PRINCIPAL_IDEAL
#define PRESET_RING_PRINCIPAL_IDEAL     PRESET_PRINCIPAL_IDEAL
#endif
#ifndef PRESET_RING_IDEAL_SUM
#define PRESET_RING_IDEAL_SUM           PRESET_IDEAL_SUM
#endif
#ifndef PRESET_RING_IDEAL_INTERSECTION
#define PRESET_RING_IDEAL_INTERSECTION   PRESET_IDEAL_INTERSECTION
#endif
#ifndef PRESET_RING_QUOTIENT_RING
#define PRESET_RING_QUOTIENT_RING       PRESET_QUOTIENT_RING
#endif
#ifndef PRESET_RING_MAXIMAL_IDEAL_TEST
#define PRESET_RING_MAXIMAL_IDEAL_TEST  PRESET_MAXIMAL_IDEAL_CHECK
#endif
#ifndef PRESET_RING_PRIME_IDEAL_TEST
#define PRESET_RING_PRIME_IDEAL_TEST    PRESET_PRIME_IDEAL_CHECK
#endif
#ifndef PRESET_RING_HOMOMORPHISM_TEST
#define PRESET_RING_HOMOMORPHISM_TEST   PRESET_RING_HOMOMORPHISM
#endif
#ifndef PRESET_RING_HOMOMORPHISM_KERNEL
#define PRESET_RING_HOMOMORPHISM_KERNEL PRESET_RING_KERNEL
#endif
#ifndef PRESET_RING_HOMOMORPHISM_IMAGE
#define PRESET_RING_HOMOMORPHISM_IMAGE  PRESET_RING_IMAGE
#endif
/* 缺失宏定义（占位符，避免编译错误） */
#ifndef PRESET_RING_MULTIPLICATIVE_INVERSE
#define PRESET_RING_MULTIPLICATIVE_INVERSE "ring_multiplicative_inverse"
#endif

/* ==================== 向后兼容别名（v4.0 .c 文件使用的宏名） ==================== */
#ifndef PRESET_RING_ISOMORPHISM_TEST
#define PRESET_RING_ISOMORPHISM_TEST       "ring_isomorphism_test"
#endif
#ifndef PRESET_RING_INTEGRAL_DOMAIN_TEST
#define PRESET_RING_INTEGRAL_DOMAIN_TEST   "ring_integral_domain_test"
#endif
#ifndef PRESET_RING_FIELD_TEST
#define PRESET_RING_FIELD_TEST             "ring_field_test"
#endif
#ifndef PRESET_RING_EUCLIDEAN_DOMAIN_TEST
#define PRESET_RING_EUCLIDEAN_DOMAIN_TEST  "ring_euclidean_domain_test"
#endif
#ifndef PRESET_RING_PID_TEST
#define PRESET_RING_PID_TEST               "ring_pid_test"
#endif
#ifndef PRESET_RING_UFD_TEST
#define PRESET_RING_UFD_TEST               "ring_ufd_test"
#endif
#ifndef PRESET_RING_POLY_ADDITION
#define PRESET_RING_POLY_ADDITION          "ring_poly_addition"
#endif
#ifndef PRESET_RING_POLY_MULTIPLICATION
#define PRESET_RING_POLY_MULTIPLICATION    "ring_poly_multiplication"
#endif
#ifndef PRESET_RING_POLY_GCD
#define PRESET_RING_POLY_GCD               "ring_poly_gcd"
#endif
#ifndef PRESET_RING_POLY_EVALUATION
#define PRESET_RING_POLY_EVALUATION        "ring_poly_evaluation"
#endif
#ifndef PRESET_RING_POLY_IRREDUCIBLE_TEST
#define PRESET_RING_POLY_IRREDUCIBLE_TEST  "ring_poly_irreducible_test"
#endif
#ifndef PRESET_RING_NILPOTENT_TEST
#define PRESET_RING_NILPOTENT_TEST         "ring_nilpotent_test"
#endif
#ifndef PRESET_RING_IDEMPOTENT_TEST
#define PRESET_RING_IDEMPOTENT_TEST        "ring_idempotent_test"
#endif
#ifndef PRESET_RING_UNIT_GROUP
#define PRESET_RING_UNIT_GROUP             "ring_unit_group"
#endif

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册环论预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用，用户无需直接调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_ring_theory_register(void);

/**
 * @brief 获取环论模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_ring_theory_count(void);

/**
 * @brief 获取环论模块的预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_ring_theory_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取环论预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_ring_theory_category(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_RING_THEORY_H */
