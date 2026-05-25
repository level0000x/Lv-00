/**
 * @file preset_field_theory.h
 * @brief 域论预设函数块
 *
 * 提供理论数学研究中常用的域论运算预设函数块，包括：
 * - 域基础运算：域加法、域乘法、乘法逆元、域特征、子域判定
 * - 域扩张：域扩张构造、扩张次数、代数元/超越元判定、极小多项式、域塔
 * - 伽罗瓦理论：伽罗瓦群、正规扩张、可分扩张、伽罗瓦扩张、不动域、伽罗瓦对应
 * - 有限域：有限域构造、有限域乘法、有限域逆元、有限域离散对数
 *
 * @module FieldTheory
 * @category PRESET_CATEGORY_FIELD_THEORY
 */

#ifndef LV00_PRESET_FIELD_THEORY_H
#define LV00_PRESET_FIELD_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 域基础运算 ==================== */

/**
 * @brief 域加法
 *
 * 数学定义：给定域 $F$ 中两个元素 $a, b$，计算 $a + b$
 *
 * 输入：
 *   - F: 域 (PRESET_TYPE_FIELD)
 *   - a: 域元素 (PRESET_TYPE_SCALAR)
 *   - b: 域元素 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - result: 域元素 (PRESET_TYPE_SCALAR) - a + b
 *
 * 复杂度：O(1)
 */
#define PRESET_FIELD_ADD "field_add"

/**
 * @brief 域乘法
 *
 * 数学定义：给定域 $F$ 中两个元素 $a, b$，计算 $a \cdot b$
 *
 * 输入：
 *   - F: 域 (PRESET_TYPE_FIELD)
 *   - a: 域元素 (PRESET_TYPE_SCALAR)
 *   - b: 域元素 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - result: 域元素 (PRESET_TYPE_SCALAR) - a * b
 *
 * 复杂度：O(1)
 */
#define PRESET_FIELD_MULTIPLY "field_multiply"

/**
 * @brief 乘法逆元
 *
 * 数学定义：给定域 $F$ 中非零元素 $a$，计算其乘法逆元 $a^{-1}$，满足 $a \cdot a^{-1} = 1$
 *
 * 输入：
 *   - F: 域 (PRESET_TYPE_FIELD)
 *   - a: 域元素 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - result: 域元素 (PRESET_TYPE_SCALAR) - a^(-1)
 *
 * 复杂度：O(1)
 */
#define PRESET_FIELD_INVERSE "field_inverse"

/**
 * @brief 域特征
 *
 * 数学定义：域 $F$ 的特征 $\text{char}(F)$ 是使得 $n \cdot 1 = 0$ 的最小正整数 $n$，
 * 若不存在这样的 $n$，则 $\text{char}(F) = 0$
 *
 * 输入：
 *   - F: 域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - characteristic: 整数 (PRESET_TYPE_INTEGER) - 域特征（0 或素数 p）
 *
 * 复杂度：O(1)
 */
#define PRESET_FIELD_CHARACTERISTIC "field_characteristic"

/**
 * @brief 子域判定
 *
 * 数学定义：判定域 $F$ 的子集 $K$ 是否构成子域，
 * 即 $K$ 对加法、乘法封闭，包含 $0, 1$，且每个非零元素有逆元
 *
 * 输入：
 *   - F: 域 (PRESET_TYPE_FIELD)
 *   - K: 子集 (PRESET_TYPE_SET)
 * 输出：
 *   - is_subfield: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(|K|^2)
 */
#define PRESET_FIELD_SUBFIELD_TEST "field_subfield_test"

/* ==================== 域扩张 ==================== */

/**
 * @brief 域扩张构造
 *
 * 数学定义：给定域 $F$ 和元素 $\alpha$，构造域扩张 $F(\alpha)$，
 * 即包含 $F$ 和 $\alpha$ 的最小域
 *
 * 输入：
 *   - F: 基域 (PRESET_TYPE_FIELD)
 *   - alpha: 代数或超越元 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - E: 域 (PRESET_TYPE_FIELD) - 扩张域 F(alpha)
 *
 * 复杂度：O(n^2)，n = 极小多项式次数
 */
#define PRESET_FIELD_EXTENSION "field_extension"

/**
 * @brief 扩张次数
 *
 * 数学定义：给定域扩张 $E/F$，计算扩张次数 $[E : F] = \dim_F E$
 *
 * 输入：
 *   - E: 扩张域 (PRESET_TYPE_FIELD)
 *   - F: 基域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - degree: 整数 (PRESET_TYPE_INTEGER) - 扩张次数 [E:F]
 *
 * 复杂度：O(n^3)，n = 扩张次数
 */
#define PRESET_EXTENSION_DEGREE "extension_degree"

/**
 * @brief 代数元判定
 *
 * 数学定义：给定域扩张 $E/F$ 中元素 $\alpha$，判定 $\alpha$ 是否为 $F$ 上的代数元，
 * 即是否存在非零多项式 $f \in F[x]$ 使得 $f(\alpha) = 0$
 *
 * 输入：
 *   - alpha: 元素 (PRESET_TYPE_SCALAR)
 *   - F: 基域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - is_algebraic: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^2)
 */
#define PRESET_ALGEBRAIC_ELEMENT_TEST "algebraic_element_test"

/**
 * @brief 超越元判定
 *
 * 数学定义：给定域扩张 $E/F$ 中元素 $\alpha$，判定 $\alpha$ 是否为 $F$ 上的超越元，
 * 即不存在非零多项式 $f \in F[x]$ 使得 $f(\alpha) = 0$
 *
 * 输入：
 *   - alpha: 元素 (PRESET_TYPE_SCALAR)
 *   - F: 基域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - is_transcendental: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^2)
 */
#define PRESET_TRANSCENDENTAL_ELEMENT_TEST "transcendental_element_test"

/**
 * @brief 极小多项式
 *
 * 数学定义：给定域扩张 $E/F$ 中代数元 $\alpha$，计算其极小多项式
 * $m_\alpha(x) \in F[x]$，即满足 $m_\alpha(\alpha) = 0$ 的最低次首一多项式
 *
 * 输入：
 *   - alpha: 代数元 (PRESET_TYPE_SCALAR)
 *   - F: 基域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - min_poly: 多项式 (PRESET_TYPE_POLYNOMIAL) - 极小多项式
 *
 * 复杂度：O(n^3)，n = 扩张次数
 */
#define PRESET_MINIMAL_POLYNOMIAL "minimal_polynomial"

/**
 * @brief 域塔（复合扩张）
 *
 * 数学定义：给定域塔 $F \subseteq K \subseteq E$，计算复合扩张的次数
 * $[E : F] = [E : K] \cdot [K : F]$
 *
 * 输入：
 *   - E: 顶层域 (PRESET_TYPE_FIELD)
 *   - K: 中间域 (PRESET_TYPE_FIELD)
 *   - F: 底层域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - degree: 整数 (PRESET_TYPE_INTEGER) - 复合扩张次数 [E:F]
 *
 * 复杂度：O(n^3)
 */
#define PRESET_FIELD_TOWER "field_tower"

/* ==================== 伽罗瓦理论 ==================== */

/**
 * @brief 伽罗瓦群计算
 *
 * 数学定义：给定伽罗瓦扩张 $E/F$，计算伽罗瓦群
 * $\text{Gal}(E/F) = \{\sigma \in \text{Aut}(E) : \sigma|_F = \text{id}_F\}$
 *
 * 输入：
 *   - E: 扩张域 (PRESET_TYPE_FIELD)
 *   - F: 基域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - gal_group: 群 (PRESET_TYPE_GROUP) - 伽罗瓦群 Gal(E/F)
 *
 * 复杂度：O(n!)，n = 扩张次数
 */
#define PRESET_GALOIS_GROUP "galois_group"

/**
 * @brief 伽罗瓦群阶
 *
 * 数学定义：给定伽罗瓦扩张 $E/F$，计算伽罗瓦群的阶
 * $|\text{Gal}(E/F)| = [E : F]$
 *
 * 输入：
 *   - E: 扩张域 (PRESET_TYPE_FIELD)
 *   - F: 基域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - order: 整数 (PRESET_TYPE_INTEGER) - 伽罗瓦群的阶
 *
 * 复杂度：O(n^3)
 */
#define PRESET_GALOIS_GROUP_ORDER "galois_group_order"

/**
 * @brief 正规扩张判定
 *
 * 数学定义：判定域扩张 $E/F$ 是否为正规扩张，
 * 即 $E$ 中每个不可约多项式的根若有一个在 $E$ 中，则所有根都在 $E$ 中
 *
 * 输入：
 *   - E: 扩张域 (PRESET_TYPE_FIELD)
 *   - F: 基域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - is_normal: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^3)
 */
#define PRESET_NORMAL_EXTENSION_TEST "normal_extension_test"

/**
 * @brief 可分扩张判定
 *
 * 数学定义：判定域扩张 $E/F$ 是否为可分扩张，
 * 即 $E$ 中每个元素在 $F$ 上的极小多项式在分裂域中无重根
 *
 * 输入：
 *   - E: 扩张域 (PRESET_TYPE_FIELD)
 *   - F: 基域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - is_separable: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^3)
 */
#define PRESET_SEPARABLE_EXTENSION_TEST "separable_extension_test"

/**
 * @brief 伽罗瓦扩张判定
 *
 * 数学定义：判定域扩张 $E/F$ 是否为伽罗瓦扩张，
 * 即 $E/F$ 既是正规扩张又是可分扩张，等价于 $|\text{Gal}(E/F)| = [E : F]$
 *
 * 输入：
 *   - E: 扩张域 (PRESET_TYPE_FIELD)
 *   - F: 基域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - is_galois: 布尔值 (PRESET_TYPE_BOOLEAN)
 *
 * 复杂度：O(n^3)
 */
#define PRESET_GALOIS_EXTENSION_TEST "galois_extension_test"

/**
 * @brief 不动域计算
 *
 * 数学定义：给定伽罗瓦群 $G \leq \text{Gal}(E/F)$，计算不动域
 * $E^G = \{a \in E : \sigma(a) = a, \forall \sigma \in G\}$
 *
 * 输入：
 *   - E: 扩张域 (PRESET_TYPE_FIELD)
 *   - G: 子群 (PRESET_TYPE_GROUP)
 * 输出：
 *   - fixed_field: 域 (PRESET_TYPE_FIELD) - 不动域 E^G
 *
 * 复杂度：O(|G| * |E|)
 */
#define PRESET_FIXED_FIELD "fixed_field"

/**
 * @brief 伽罗瓦对应
 *
 * 数学定义：给定伽罗瓦扩张 $E/F$，建立伽罗瓦对应（基本定理），
 * 即中间域 $K$（$F \subseteq K \subseteq E$）与 $\text{Gal}(E/F)$ 的子群之间的反序双射
 *
 * 输入：
 *   - E: 扩张域 (PRESET_TYPE_FIELD)
 *   - F: 基域 (PRESET_TYPE_FIELD)
 * 输出：
 *   - correspondence: 元组列表 (PRESET_TYPE_TUPLE) - 中间域与子群的对应关系
 *
 * 复杂度：O(n!)
 */
#define PRESET_GALOIS_CORRESPONDENCE "galois_correspondence"

/* ==================== 有限域 ==================== */

/**
 * @brief 有限域构造
 *
 * 数学定义：给定素数 $p$ 和正整数 $n$，构造有限域 $\mathbb{F}_{p^n} = \text{GF}(p^n)$，
 * 其元素个数为 $p^n$
 *
 * 输入：
 *   - p: 素数 (PRESET_TYPE_PRIME) - 特征
 *   - n: 整数 (PRESET_TYPE_INTEGER) - 扩张次数
 * 输出：
 *   - GF: 域 (PRESET_TYPE_FIELD) - 有限域 GF(p^n)
 *
 * 复杂度：O(p^n * n^2)
 */
#define PRESET_FINITE_FIELD_CONSTRUCT "finite_field_construct"

/**
 * @brief 有限域乘法
 *
 * 数学定义：给定有限域 $\mathbb{F}_{p^n}$ 中两个元素 $a, b$，计算 $a \cdot b \mod f(x)$，
 * 其中 $f(x)$ 是构造域时使用的不可约多项式
 *
 * 输入：
 *   - GF: 有限域 (PRESET_TYPE_FIELD)
 *   - a: 域元素 (PRESET_TYPE_SCALAR)
 *   - b: 域元素 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - result: 域元素 (PRESET_TYPE_SCALAR) - a * b
 *
 * 复杂度：O(n^2)
 */
#define PRESET_FINITE_FIELD_MULTIPLY "finite_field_multiply"

/**
 * @brief 有限域逆元
 *
 * 数学定义：给定有限域 $\mathbb{F}_{p^n}$ 中非零元素 $a$，
 * 使用扩展欧几里得算法计算 $a^{-1}$
 *
 * 输入：
 *   - GF: 有限域 (PRESET_TYPE_FIELD)
 *   - a: 域元素 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - result: 域元素 (PRESET_TYPE_SCALAR) - a^(-1)
 *
 * 复杂度：O(n^2)
 */
#define PRESET_FINITE_FIELD_INVERSE "finite_field_inverse"

/**
 * @brief 有限域离散对数
 *
 * 数学定义：给定有限域 $\mathbb{F}_{p^n}$ 中生成元 $g$ 和元素 $h$，
 * 求解离散对数 $x$ 使得 $g^x = h$（Pollard's rho 算法）
 *
 * 输入：
 *   - GF: 有限域 (PRESET_TYPE_FIELD)
 *   - g: 生成元 (PRESET_TYPE_SCALAR)
 *   - h: 目标元素 (PRESET_TYPE_SCALAR)
 * 输出：
 *   - x: 整数 (PRESET_TYPE_INTEGER) - 离散对数
 *
 * 复杂度：O(sqrt(p^n))
 */
#define PRESET_FINITE_FIELD_DISCRETE_LOG "finite_field_discrete_log"

/* ==================== 扩展预设名称常量 ==================== */

/** 域除法 */
#define PRESET_FIELD_DIVIDE "field_divide"

/** 子域检查 */
#define PRESET_FIELD_SUBFIELD_CHECK "field_subfield_check"

/** 域扩张检查 */
#define PRESET_FIELD_EXTENSION_CHECK "field_extension_check"

/** 素子域 */
#define PRESET_FIELD_PRIME_SUBFIELD "field_prime_subfield"

/** 单扩张 */
#define PRESET_SIMPLE_EXTENSION "simple_extension"

/** 代数扩张 */
#define PRESET_ALGEBRAIC_EXTENSION "algebraic_extension"

/** 超越扩张 */
#define PRESET_TRANSCENDENTAL_EXTENSION "transcendental_extension"

/** 有限扩张 */
#define PRESET_FINITE_EXTENSION "finite_extension"

/** 代数元检查 */
#define PRESET_ALGEBRAIC_ELEMENT_CHECK "algebraic_element_check"

/** 本原元 */
#define PRESET_PRIMITIVE_ELEMENT "primitive_element"

/** 正规扩张 */
#define PRESET_NORMAL_EXTENSION "normal_extension"

/** 伽罗瓦检查 */
#define PRESET_GALOIS_CHECK "galois_check"

/** 可分扩张 */
#define PRESET_SEPARABLE_EXTENSION "separable_extension"

/** 分裂域 */
#define PRESET_SPLITTING_FIELD "splitting_field"

/** 分圆域 */
#define PRESET_CYCLOTOMIC_FIELD "cyclotomic_field"

/** Frobenius自同构 */
#define PRESET_FROBENIUS_AUTOMORPHISM "frobenius_automorphism"

/** 域嵌入 */
#define PRESET_FIELD_EMBEDDING "field_embedding"

/** 代数闭包 */
#define PRESET_ALGEBRAIC_CLOSURE "algebraic_closure"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册域论预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用，用户无需直接调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_field_theory_register(void);

/**
 * @brief 获取域论模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_field_theory_count(void);

/**
 * @brief 获取域论预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_field_theory_get_names(char ***out_names, int *out_count);

/**
 * @brief 获取域论预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_field_theory_category(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_FIELD_THEORY_H */
