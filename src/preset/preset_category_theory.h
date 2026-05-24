/**
 * @file preset_category_theory.h
 * @brief 范畴论预设函数块 - 常量定义
 *
 * 提供理论数学研究中常用的范畴论运算预设函数块。
 * 涵盖范畴基本概念、函子、自然变换、极限与余极限以及泛性质。
 *
 * 包含的预设函数块：
 * - 基本概念 (3个)：恒等态射、态射复合、同构判定
 * - 函子 (1个)：函子作用
 * - 自然变换 (1个)：自然变换
 * - 极限与余极限 (7个)：积、余积、拉回、推出、等化子、余等化子
 * - 泛性质 (4个)：指数对象、初始对象、终止对象、伴随函子
 *
 * @module CategoryTheory
 * @category PRESET_CATEGORY_CATEGORY_THEORY
 * @version 3.2.0
 */

#ifndef LV00_PRESET_CATEGORY_THEORY_H
#define LV00_PRESET_CATEGORY_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 预设名称宏常量
 * ================================================================ */

/* ==================== 基本概念 ==================== */

/**
 * @brief 预设：恒等态射
 * @details 数学定义: $\mathrm{id}_A : A \to A,\ \mathrm{id}_A(x) = x$，满足 $f \circ \mathrm{id}_A = f = \mathrm{id}_B \circ f$
 * @note 输入: PRESET_TYPE_SET | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(1)
 */
#define PRESET_CAT_IDENTITY_MORPHISM "cat_identity_morphism"

/**
 * @brief 预设：态射复合
 * @details 数学定义: $(g \circ f)(x) = g(f(x))$，要求 $\mathrm{cod}(f) = \mathrm{dom}(g)$，满足结合律
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(1)
 */
#define PRESET_CAT_COMPOSITION "cat_composition"

/**
 * @brief 预设：同构判定
 * @details 数学定义: $f$ 是同构 $\Leftrightarrow \exists g,\ f \circ g = \mathrm{id}_B \land g \circ f = \mathrm{id}_A$
 * @note 输入: PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|\mathrm{Hom}|)
 */
#define PRESET_CAT_ISOMORPHISM_TEST "cat_isomorphism_test"

/* ==================== 函子 ==================== */

/**
 * @brief 预设：函子作用
 * @details 数学定义: 函子 $F$ 对态射 $f$ 的作用，保持恒等 $F(\mathrm{id}_A) = \mathrm{id}_{F(A)}$ 和复合 $F(g \circ f) = F(g) \circ F(f)$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(1)
 */
#define PRESET_CAT_FUNCTOR_APPLY "cat_functor_apply"

/* ==================== 自然变换 ==================== */

/**
 * @brief 预设：自然变换
 * @details 数学定义: $\alpha : F \Rightarrow G \Leftrightarrow \forall f : A \to B,\ G(f) \circ \alpha_A = \alpha_B \circ F(f)$（自然性条件）
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|\mathrm{Ob}| \cdot |\mathrm{Mor}|)
 */
#define PRESET_CAT_NATURAL_TRANSFORMATION "cat_natural_transformation"

/* ==================== 极限与余极限 ==================== */

/**
 * @brief 预设：积（Product）
 * @details 数学定义: $A \times B$ 满足 $\forall Q, f, g,\ \exists! \langle f, g \rangle : Q \to A \times B$（泛性质）
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|\mathrm{Hom}|^2)
 */
#define PRESET_CAT_PRODUCT "cat_product"

/**
 * @brief 预设：余积（Coproduct）
 * @details 数学定义: $A \amalg B$ 满足 $\forall Q, f, g,\ \exists! [f, g] : A \amalg B \to Q$（对偶于积的泛性质）
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|\mathrm{Hom}|^2)
 */
#define PRESET_CAT_COPRODUCT "cat_coproduct"

/**
 * @brief 预设：拉回（纤维积, Pullback）
 * @details 数学定义: $A \times_C B = \{(a, b) \mid f(a) = g(b)\}$，交换方框的极限
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_SET | 复杂度: O(|A| \cdot |B|)
 */
#define PRESET_CAT_PULLBACK "cat_pullback"

/**
 * @brief 预设：推出（Pushout）
 * @details 数学定义: $A \amalg_C B$，余交换方框的余极限，拉回的对偶概念
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_SET | 复杂度: O(|A| + |B|)
 */
#define PRESET_CAT_PUSHOUT "cat_pushout"

/**
 * @brief 预设：等化子（Equalizer）
 * @details 数学定义: $\mathrm{eq}(f, g) = \{x \in A : f(x) = g(x)\}$，满足 $f \circ e = g \circ e$ 的泛态射
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_SET | 复杂度: O(|A|)
 */
#define PRESET_CAT_EQUALIZER "cat_equalizer"

/**
 * @brief 预设：余等化子（Coequalizer）
 * @details 数学定义: $\mathrm{coeq}(f, g) = B / {\sim}$，其中 ${\sim}$ 是 $f, g$ 生成的最小等价关系
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_SET | 复杂度: O(|B| + |A|)
 */
#define PRESET_CAT_COEQUALIZER "cat_coequalizer"

/* ==================== 泛性质 ==================== */

/**
 * @brief 预设：指数对象
 * @details 数学定义: $B^A$ 满足 $\mathrm{Hom}(A \times B, C) \cong \mathrm{Hom}(A, C^B)$（自然同构），Descartes 闭范畴的关键结构
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|\mathrm{Hom}|)
 */
#define PRESET_CAT_EXPONENTIAL "cat_exponential"

/**
 * @brief 预设：初始对象
 * @details 数学定义: $0$ 是初始对象 $\Leftrightarrow \forall A,\ |\mathrm{Hom}(0, A)| = 1$，例如 $\mathbf{Set}$ 中的空集
 * @note 输入: PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|\mathrm{Ob}|)
 */
#define PRESET_CAT_INITIAL_OBJECT "cat_initial_object"

/**
 * @brief 预设：终止对象
 * @details 数学定义: $1$ 是终止对象 $\Leftrightarrow \forall A,\ |\mathrm{Hom}(A, 1)| = 1$，例如 $\mathbf{Set}$ 中的单元素集
 * @note 输入: PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|\mathrm{Ob}|)
 */
#define PRESET_CAT_TERMINAL_OBJECT "cat_terminal_object"

/**
 * @brief 预设：伴随函子
 * @details 数学定义: $F \dashv G \Leftrightarrow \mathrm{Hom}_{\mathcal{D}}(F(A), B) \cong \mathrm{Hom}_{\mathcal{C}}(A, G(B))$ 自然同构，$\mathcal{D}(F-,-) \cong \mathcal{C}(-, G-)$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|\mathrm{Ob}| \cdot |\mathrm{Mor}|)
 */
#define PRESET_CAT_ADJOINT "cat_adjoint"

/* ================================================================
 * 模块注册函数
 * ================================================================ */

/**
 * @brief 注册所有范畴论预设函数块
 *
 * 将范畴论模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_category_theory_register(void);

/**
 * @brief 获取范畴论预设函数块数量
 *
 * @return int 范畴论模块预设函数块总数（15）
 */
int preset_category_theory_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_CATEGORY_THEORY_H */
