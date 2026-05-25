/**
 * @file preset_set_theory.h
 * @brief 集合论预设函数块 - 常量定义
 *
 * 提供理论数学研究中常用的集合论运算预设函数块。
 * 覆盖集合基本运算、关系与函数、映射理论、序理论及公理集合论。
 *
 * 包含的预设函数块：
 * - 集合基本运算 (10个)：并集、交集、差集、补集、对称差、笛卡尔积、幂集、子集判定、相等判定、空集判定
 * - 关系与函数 (8个)：关系复合、逆关系、自反性/对称性/传递性判定、等价关系、等价类、商集
 * - 映射理论 (8个)：函数复合、逆函数判定、单射/满射/双射判定、像、原像、不动点
 * - 序理论 (5个)：偏序关系判定、最小元、最大元、上确界、下确界
 * - 公理集合论 (4个)：ZFC配对公理、并集公理、幂集公理、替换公理
 *
 * @module SetTheory
 * @category PRESET_CATEGORY_LOGIC
 * @version 2.0.0
 */

#ifndef LV00_PRESET_SET_THEORY_H
#define LV00_PRESET_SET_THEORY_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 预设名称宏常量
 * ================================================================ */

/* ==================== 集合基本运算 ==================== */

/**
 * @brief 预设：并集运算
 * @details 数学定义: $A \cup B = \{x : x \in A \lor x \in B\}$，并集满足交换律、结合律，以空集为单位元
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|A|+|B|)
 */
#define PRESET_SET_UNION "set_union"

/**
 * @brief 预设：交集运算
 * @details 数学定义: $A \cap B = \{x : x \in A \land x \in B\}$，交集满足交换律、结合律
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(min(|A|,|B|))
 */
#define PRESET_SET_INTERSECTION "set_intersection"

/**
 * @brief 预设：差集运算
 * @details 数学定义: $A \setminus B = \{x : x \in A \land x \notin B\}$，差集不满足交换律
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|A|)
 */
#define PRESET_SET_DIFFERENCE "set_difference"

/**
 * @brief 预设：补集运算
 * @details 数学定义: $A^c = U \setminus A = \{x \in U : x \notin A\}$，相对于全集 $U$，满足 $(A^c)^c = A$
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|U|) | 可逆: 是
 */
#define PRESET_SET_COMPLEMENT "set_complement"

/**
 * @brief 预设：对称差运算
 * @details 数学定义: $A \triangle B = (A \setminus B) \cup (B \setminus A)$，以空集为单位元的阿贝尔群结构
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|A|+|B|) | 可逆: 是
 */
#define PRESET_SET_SYMMETRIC_DIFFERENCE "set_symmetric_difference"

/**
 * @brief 预设：笛卡尔积运算
 * @details 数学定义: $A \times B = \{(a, b) : a \in A, b \in B\}$，不满足交换律
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|A| \cdot |B|)
 */
#define PRESET_SET_CARTESIAN_PRODUCT "set_cartesian_product"

/**
 * @brief 预设：幂集运算
 * @details 数学定义: $\mathcal{P}(A) = \{S : S \subseteq A\}$，$|\mathcal{P}(A)| = 2^{|A|}$，包含关系构成布尔代数
 * @note 输入: PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(2^{|A|})
 */
#define PRESET_SET_POWER_SET "set_power_set"

/**
 * @brief 预设：子集判定
 * @details 数学定义: $A \subseteq B \Leftrightarrow \forall x \in A, x \in B$，子集关系是偏序关系
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|A|)
 */
#define PRESET_SET_SUBSET_CHECK "set_subset_check"

/**
 * @brief 预设：集合相等判定
 * @details 数学定义: $A = B \Leftrightarrow A \subseteq B \land B \subseteq A$（外延公理）
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|A|+|B|) | 可逆: 是
 */
#define PRESET_SET_EQUALITY_CHECK "set_equality_check"

/**
 * @brief 预设：空集判定
 * @details 数学定义: $A = \emptyset \Leftrightarrow \lnot \exists x: x \in A$，空集是并集单位元、交集零元
 * @note 输入: PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(1)
 */
#define PRESET_SET_EMPTY_CHECK "set_empty_check"

/* ==================== 关系与函数 ==================== */

/**
 * @brief 预设：关系复合
 * @details 数学定义: $R \circ S = \{(a, c) : \exists b, (a,b) \in S \land (b,c) \in R\}$，满足结合律
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|S| \cdot |R|)
 */
#define PRESET_RELATION_COMPOSE "relation_compose"

/**
 * @brief 预设：逆关系
 * @details 数学定义: $R^{-1} = \{(b, a) : (a, b) \in R\}$，满足 $(R^{-1})^{-1} = R$ 和 $(R \circ S)^{-1} = S^{-1} \circ R^{-1}$
 * @note 输入: PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|R|) | 可逆: 是
 */
#define PRESET_RELATION_INVERSE "relation_inverse"

/**
 * @brief 预设：自反性判定
 * @details 数学定义: $R$ 自反 $\Leftrightarrow \forall a \in A, (a, a) \in R$
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|A|)
 */
#define PRESET_RELATION_REFLEXIVE_CHECK "relation_reflexive_check"

/**
 * @brief 预设：对称性判定
 * @details 数学定义: $R$ 对称 $\Leftrightarrow \forall a, b \in A, (a,b) \in R \Rightarrow (b,a) \in R$
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|R|)
 */
#define PRESET_RELATION_SYMMETRIC_CHECK "relation_symmetric_check"

/**
 * @brief 预设：传递性判定
 * @details 数学定义: $R$ 传递 $\Leftrightarrow \forall a,b,c, (a,b) \in R \land (b,c) \in R \Rightarrow (a,c) \in R$
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|R|^2)
 */
#define PRESET_RELATION_TRANSITIVE_CHECK "relation_transitive_check"

/**
 * @brief 预设：等价关系判定
 * @details 数学定义: $R$ 是等价关系 $\Leftrightarrow$ 自反$(R) \land$ 对称$(R) \land$ 传递$(R)$，将集合划分为不相交等价类
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|A|^2 \cdot |R|)
 */
#define PRESET_RELATION_EQUIVALENCE_CHECK "relation_equivalence_check"

/**
 * @brief 预设：等价类
 * @details 数学定义: $[a]_R = \{x \in A : (a, x) \in R\}$，若 $b \in [a]_R$ 则 $[a]_R = [b]_R$
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_ANY | 输出: PRESET_TYPE_SET | 复杂度: O(|A|)
 */
#define PRESET_EQUIVALENCE_CLASS "equivalence_class"

/**
 * @brief 预设：商集
 * @details 数学定义: $A/R = \{[a]_R : a \in A\}$，自然映射 $\pi: A \to A/R,\ a \mapsto [a]_R$ 是满射
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|A|)
 */
#define PRESET_QUOTIENT_SET "quotient_set"

/* ==================== 映射理论 ==================== */

/**
 * @brief 预设：函数复合
 * @details 数学定义: $(g \circ f)(x) = g(f(x))$，要求 $\text{cod}(f) = \text{dom}(g)$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION | 输出: PRESET_TYPE_FUNCTION | 复杂度: O(n)
 */
#define PRESET_FUNCTION_COMPOSE "function_compose"

/**
 * @brief 预设：逆函数判定
 * @details 数学定义: $f^{-1}$ 存在 $\Leftrightarrow f$ 是双射，满足 $f \circ f^{-1} = \text{id}_B$ 且 $f^{-1} \circ f = \text{id}_A$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|A|+|B|)
 */
#define PRESET_FUNCTION_INVERSE_CHECK "function_inverse_check"

/**
 * @brief 预设：单射判定
 * @details 数学定义: $f$ 单射 $\Leftrightarrow \forall a_1, a_2 \in A, f(a_1)=f(a_2) \Rightarrow a_1 = a_2$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|A| \log |A|)
 */
#define PRESET_FUNCTION_INJECTIVE_CHECK "function_injective_check"

/**
 * @brief 预设：满射判定
 * @details 数学定义: $f$ 满射 $\Leftrightarrow \forall b \in B, \exists a \in A, f(a) = b \Leftrightarrow f(A) = B$
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|A|+|B|)
 */
#define PRESET_FUNCTION_SURJECTIVE_CHECK "function_surjective_check"

/**
 * @brief 预设：双射判定
 * @details 数学定义: $f$ 双射 $\Leftrightarrow f$ 单射 $\land f$ 满射，双射是定义集合基数相等的基础
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|A| \log |A| + |B|)
 */
#define PRESET_FUNCTION_BIJECTIVE_CHECK "function_bijective_check"

/**
 * @brief 预设：函数的像
 * @details 数学定义: $f(S) = \{f(x) : x \in S \subseteq A\}$，满足单调性和对并集保持
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|S|)
 */
#define PRESET_FUNCTION_IMAGE "function_image"

/**
 * @brief 预设：函数的原像（逆像）
 * @details 数学定义: $f^{-1}(T) = \{x \in A : f(x) \in T \subseteq B\}$，保持所有集合运算
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|A|)
 */
#define PRESET_FUNCTION_PREIMAGE "function_preimage"

/**
 * @brief 预设：不动点
 * @details 数学定义: $\text{Fix}(f) = \{x \in A : f(x) = x\}$，Banach 不动点定理：完备度量空间上压缩映射有唯一不动点
 * @note 输入: PRESET_TYPE_FUNCTION, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|A|)
 */
#define PRESET_FUNCTION_FIXPOINT "function_fixpoint"

/* ==================== 序理论 ==================== */

/**
 * @brief 预设：偏序关系判定
 * @details 数学定义: $\leq$ 是偏序 $\Leftrightarrow$ 自反 $\land$ 反对称 $\land$ 传递，其中反对称性: $a \leq b \land b \leq a \Rightarrow a = b$
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_BOOLEAN | 复杂度: O(|A|^2 \cdot |R|)
 */
#define PRESET_ORDER_CHECK "order_check"

/**
 * @brief 预设：最小元
 * @details 数学定义: $\min S = m \Leftrightarrow m \in S \land \forall x \in S, m \leq x$
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_ANY | 复杂度: O(|S|^2)
 */
#define PRESET_ORDER_MIN "order_min"

/**
 * @brief 预设：最大元
 * @details 数学定义: $\max S = M \Leftrightarrow M \in S \land \forall x \in S, x \leq M$
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_ANY | 复杂度: O(|S|^2)
 */
#define PRESET_ORDER_MAX "order_max"

/**
 * @brief 预设：上确界（最小上界）
 * @details 数学定义: $\sup S = \min\{u \in A : \forall x \in S, x \leq u\}$，在格中记为 $a \lor b$（join）
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_ANY | 复杂度: O(|S|^2)
 */
#define PRESET_ORDER_SUPREMUM "order_supremum"

/**
 * @brief 预设：下确界（最大下界）
 * @details 数学定义: $\inf S = \max\{l \in A : \forall x \in S, l \leq x\}$，在格中记为 $a \land b$（meet）
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_ANY | 复杂度: O(|S|^2)
 */
#define PRESET_ORDER_INFIMUM "order_infimum"

/* ==================== 公理集合论 ==================== */

/**
 * @brief 预设：ZFC配对公理
 * @details 数学定义: $\forall a \forall b \exists c \forall x: x \in c \Leftrightarrow x = a \lor x = b$，保证无序对存在
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(1)
 */
#define PRESET_ZFC_PAIRING "zfc_pairing"

/**
 * @brief 预设：ZFC并集公理
 * @details 数学定义: $\forall A \exists B \forall x: x \in B \Leftrightarrow \exists Y \in A, x \in Y$，对集合族取并
 * @note 输入: PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(|A| \cdot \max|Y|)
 */
#define PRESET_ZFC_UNION "zfc_union"

/**
 * @brief 预设：ZFC幂集公理
 * @details 数学定义: $\forall A \exists B \forall S: S \in B \Leftrightarrow S \subseteq A$，Cantor 定理: $|\mathcal{P}(A)| > |A|$
 * @note 输入: PRESET_TYPE_SET | 输出: PRESET_TYPE_SET | 复杂度: O(2^{|A|})
 */
#define PRESET_ZFC_POWER_SET "zfc_power_set"

/**
 * @brief 预设：ZFC替换公理模式
 * @details 数学定义: $(\forall x \exists! y\ \varphi(x,y)) \Rightarrow \forall A \exists B \forall y (y \in B \Leftrightarrow \exists x \in A\ \varphi(x,y))$，ZFC最强公理之一
 * @note 输入: PRESET_TYPE_SET, PRESET_TYPE_EXPRESSION | 输出: PRESET_TYPE_SET | 复杂度: O(|A|) | 构造性: 否
 */
#define PRESET_ZFC_REPLACEMENT "zfc_replacement"

/* ================================================================
 * 模块注册函数
 * ================================================================ */

/**
 * @brief 注册所有集合论预设函数块
 *
 * 将集合论模块的所有预设函数块注册到全局预设库中。
 * 此函数由 preset_blocks_init() 自动调用。
 *
 * @return true 全部注册成功
 * @return false 部分注册失败
 */
bool preset_set_theory_register(void);

/**
 * @brief 获取集合论预设函数块数量
 *
 * @return int 集合论模块预设函数块总数（35）
 */
int preset_set_theory_count(void);

/**
 * @brief 获取集合论预设的类别
 *
 * @return 预设类别
 */
PresetCategory preset_set_theory_category(void);

/**
 * @brief 获取集合论模块的预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_set_theory_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_SET_THEORY_H */
