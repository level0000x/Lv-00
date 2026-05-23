/**
 * @file preset_functional_analysis.h
 * @brief 泛函分析预设函数块
 *
 * 提供理论数学研究中常用的泛函分析预设函数块。
 * 涵盖赋范空间、内积空间、线性算子理论、三大基本定理、
 * 谱分析以及弱收敛。
 *
 * 包含的预设函数块：
 * - 赋范空间（2个）：范数判定、Banach空间判定
 * - 内积空间（2个）：内积判定、Hilbert空间判定
 * - 线性算子（3个）：有界性判定、紧算子判定、谱分析
 * - 三大基本定理（3个）：Hahn-Banach定理、开映射定理、闭图像定理
 * - 一致有界原理（1个）
 * - 弱收敛（2个）：弱收敛、弱*收敛
 * - 对偶与伴随（3个）：对偶空间、伴随算子、自伴算子
 * - 投影与正交化（3个）：正交投影、Gram-Schmidt正交化、Riesz表示定理
 * - 不动点理论（2个）：不动点定理、压缩映射原理
 *
 * @module FunctionalAnalysis
 * @category PRESET_CATEGORY_ANALYSIS
 */

#ifndef LV00_PRESET_FUNCTIONAL_ANALYSIS_H
#define LV00_PRESET_FUNCTIONAL_ANALYSIS_H

#include "preset_blocks.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 赋范空间 ==================== */

/**
 * @brief 范数判定
 *
 * 数学定义：验证函数 $\|\cdot\|: X \to [0, +\infty)$ 是否满足范数三公理：
 * (1) $\|x\| = 0 \Leftrightarrow x = 0$ (正定性)
 * (2) $\|\alpha x\| = |\alpha| \cdot \|x\|$ (齐次性)
 * (3) $\|x + y\| \leq \|x\| + \|y\|$ (三角不等式)
 *
 * 输入：
 *   - norm_func: 函数 (PRESET_TYPE_FUNCTION) - 候选范数函数
 *   - space: 空间 (PRESET_TYPE_SPACE) - 向量空间
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否为有效范数
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_NORM_CHECK "fa_norm_check"

/**
 * @brief Banach空间判定
 *
 * 数学定义：判定赋范空间 $(X, \|\cdot\|)$ 是否完备（即是否为Banach空间）
 * $$\forall \{x_n\} \subset X: \lim_{m,n\to\infty} \|x_m - x_n\| = 0 \Rightarrow \exists x \in X: \lim \|x_n - x\| = 0$$
 *
 * 输入：
 *   - space: 空间 (PRESET_TYPE_SPACE) - 赋范线性空间
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否为Banach空间
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_BANACH_SPACE_CHECK "fa_banach_space_check"

/* ==================== 内积空间 ==================== */

/**
 * @brief 内积判定
 *
 * 数学定义：验证二元函数 $\langle\cdot,\cdot\rangle: X \times X \to \mathbb{C}$ 是否为内积：
 * (1) $\langle x, x \rangle \geq 0$，等号仅当 $x = 0$
 * (2) $\langle x, y \rangle = \overline{\langle y, x \rangle}$
 * (3) $\langle \alpha x + \beta y, z \rangle = \alpha\langle x, z \rangle + \beta\langle y, z \rangle$
 *
 * 输入：
 *   - inner_func: 函数 (PRESET_TYPE_FUNCTION) - 候选内积函数
 *   - space: 空间 (PRESET_TYPE_SPACE) - 向量空间
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否为有效内积
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_INNER_PRODUCT_CHECK "fa_inner_product_check"

/**
 * @brief Hilbert空间判定
 *
 * 数学定义：判定内积空间是否关于内积诱导的范数 $\|x\| = \sqrt{\langle x, x \rangle}$ 完备
 *
 * 输入：
 *   - space: 空间 (PRESET_TYPE_SPACE) - 内积空间
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否为Hilbert空间
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_HILBERT_SPACE_CHECK "fa_hilbert_space_check"

/* ==================== 线性算子 ==================== */

/**
 * @brief 有界性判定
 *
 * 数学定义：判定线性算子 $T: X \to Y$ 是否有界，即
 * $$\exists M > 0: \|Tx\|_Y \leq M\|x\|_X,\ \forall x \in X$$
 * 算子范数为 $\|T\| = \sup_{\|x\| \leq 1} \|Tx\|$
 *
 * 输入：
 *   - T: 函数 (PRESET_TYPE_FUNCTION) - 线性算子
 *   - X: 空间 (PRESET_TYPE_SPACE) - 定义域空间
 *   - Y: 空间 (PRESET_TYPE_SPACE) - 值域空间
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否有界
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_BOUNDED_OPERATOR "fa_bounded_operator"

/**
 * @brief 紧算子判定
 *
 * 数学定义：判定线性算子 $T: X \to Y$ 是否为紧算子：
 * 对任意有界集 $B \subset X$，$\overline{T(B)}$ 在 $Y$ 中为紧集
 *
 * 输入：
 *   - T: 函数 (PRESET_TYPE_FUNCTION) - 线性算子
 *   - X: 空间 (PRESET_TYPE_SPACE) - 定义域空间
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否为紧算子
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_COMPACT_OPERATOR "fa_compact_operator"

/**
 * @brief 谱分析
 *
 * 数学定义：对有界线性算子 $T$ 进行谱分析，确定谱集 $\sigma(T)$ 的组成：
 * $$\sigma(T) = \sigma_p(T) \cup \sigma_c(T) \cup \sigma_r(T)$$
 * 其中 $\sigma_p$ 为点谱（特征值），$\sigma_c$ 为连续谱，$\sigma_r$ 为剩余谱
 *
 * 输入：
 *   - T: 函数 (PRESET_TYPE_FUNCTION) - 有界线性算子
 * 输出：
 *   - 列表 (PRESET_TYPE_LIST) - 谱集的分类描述
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_SPECTRAL_ANALYSIS "fa_spectral_analysis"

/* ==================== 三大基本定理 ==================== */

/**
 * @brief Hahn-Banach定理
 *
 * 数学定义：设 $M$ 是赋范空间 $X$ 的子空间，$f$ 是 $M$ 上的有界线性泛函，
 * 则存在 $X$ 上的有界线性泛函 $F$ 使得
 * $$F|_M = f,\quad \|F\| = \|f\|$$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 子空间上的有界线性泛函
 *   - M: 空间 (PRESET_TYPE_SPACE) - 子空间
 *   - X: 空间 (PRESET_TYPE_SPACE) - 全空间
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 保范延拓泛函 $F$
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_HAHN_BANACH "fa_hahn_banach"

/**
 * @brief 开映射定理
 *
 * 数学定义：若 $T: X \to Y$ 是Banach空间之间的满射有界线性算子，
 * 则 $T$ 将开集映射为开集
 *
 * 输入：
 *   - T: 函数 (PRESET_TYPE_FUNCTION) - 满射有界线性算子
 *   - U: 集合 (PRESET_TYPE_SET) - $X$ 中的开集
 * 输出：
 *   - 集合 (PRESET_TYPE_SET) - $T(U)$ 在 $Y$ 中的像
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_OPEN_MAPPING "fa_open_mapping"

/**
 * @brief 闭图像定理
 *
 * 数学定义：若 $T: X \to Y$ 是Banach空间之间的线性算子，
 * 且其图像 $\Gamma(T) = \{(x, Tx) : x \in X\}$ 在 $X \times Y$ 中为闭集，
 * 则 $T$ 有界
 *
 * 输入：
 *   - T: 函数 (PRESET_TYPE_FUNCTION) - 线性算子
 *   - X: 空间 (PRESET_TYPE_SPACE) - Banach空间
 *   - Y: 空间 (PRESET_TYPE_SPACE) - Banach空间
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 图像的闭性与有界性的等价关系
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_CLOSED_GRAPH "fa_closed_graph"

/* ==================== 一致有界原理 ==================== */

/**
 * @brief 一致有界原理（Banach-Steinhaus定理）
 *
 * 数学定义：设 $\{T_\alpha\}_{\alpha \in A}$ 是Banach空间 $X$ 到赋范空间 $Y$
 * 的一族有界线性算子。若对每个 $x \in X$，$\sup_{\alpha} \|T_\alpha x\| < \infty$，
 * 则 $\sup_{\alpha} \|T_\alpha\| < \infty$
 *
 * 输入：
 *   - operators: 列表 (PRESET_TYPE_LIST) - 线性算子族
 *   - X: 空间 (PRESET_TYPE_SPACE) - Banach空间
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否一致有界
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_UNIFORM_BOUNDEDNESS "fa_uniform_boundedness"

/* ==================== 弱收敛 ==================== */

/**
 * @brief 弱收敛判定
 *
 * 数学定义：序列 $\{x_n\} \subset X$ 弱收敛于 $x$，记作 $x_n \rightharpoonup x$，当且仅当
 * $$\forall f \in X^*: \lim_{n\to\infty} f(x_n) = f(x)$$
 *
 * 输入：
 *   - x_n: 序列 (PRESET_TYPE_SEQUENCE) - 序列
 *   - x: 向量 (PRESET_TYPE_VECTOR) - 极限点
 *   - X_star: 空间 (PRESET_TYPE_SPACE) - 对偶空间
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否弱收敛
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_WEAK_CONVERGENCE "fa_weak_convergence"

/**
 * @brief 弱*收敛判定
 *
 * 数学定义：对偶空间 $X^*$ 中的序列 $\{f_n\}$ 弱*收敛于 $f$，记作 $f_n \rightharpoonup^* f$，当且仅当
 * $$\forall x \in X: \lim_{n\to\infty} f_n(x) = f(x)$$
 *
 * 输入：
 *   - f_n: 序列 (PRESET_TYPE_SEQUENCE) - 泛函序列
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 极限泛函
 *   - X: 空间 (PRESET_TYPE_SPACE) - 原空间
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否弱*收敛
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_WEAK_STAR_CONVERGENCE "fa_weak_star_convergence"

/* ==================== 对偶与伴随 ==================== */

/**
 * @brief 对偶空间
 *
 * 数学定义：构造赋范空间 $X$ 的对偶空间 $X^* = \mathcal{L}(X, \mathbb{F})$，
 * 即 $X$ 上所有有界线性泛函构成的赋范空间
 *
 * 输入：
 *   - X: 空间 (PRESET_TYPE_SPACE) - 赋范空间
 * 输出：
 *   - 空间 (PRESET_TYPE_SPACE) - 对偶空间 $X^*$
 *
 * 复杂度：O(1)
 */
#define PRESET_FA_DUAL_SPACE "fa_dual_space"

/**
 * @brief 伴随算子
 *
 * 数学定义：对Hilbert空间上的有界线性算子 $T: H_1 \to H_2$，
 * 其伴随算子 $T^*: H_2 \to H_1$ 满足
 * $$\langle Tx, y \rangle = \langle x, T^*y \rangle,\ \forall x \in H_1, y \in H_2$$
 *
 * 输入：
 *   - T: 函数 (PRESET_TYPE_FUNCTION) - 有界线性算子
 * 输出：
 *   - 函数 (PRESET_TYPE_FUNCTION) - 伴随算子 $T^*$
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_ADJOINT_OPERATOR "fa_adjoint_operator"

/**
 * @brief 自伴算子判定
 *
 * 数学定义：判定算子 $T: H \to H$ 是否自伴，即 $T = T^*$
 *
 * 输入：
 *   - T: 函数 (PRESET_TYPE_FUNCTION) - Hilbert空间上的线性算子
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否自伴
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_SELF_ADJOINT "fa_self_adjoint"

/* ==================== 投影与正交化 ==================== */

/**
 * @brief 正交投影
 *
 * 数学定义：对Hilbert空间 $H$ 的闭子空间 $M$，计算 $x$ 在 $M$ 上的正交投影 $P_M x$
 * 满足 $x - P_M x \perp M$
 *
 * 输入：
 *   - x: 向量 (PRESET_TYPE_VECTOR)
 *   - M: 空间 (PRESET_TYPE_SPACE) - 闭子空间
 * 输出：
 *   - 向量 (PRESET_TYPE_VECTOR) - 正交投影 $P_M x$
 *
 * 复杂度：O(n)
 */
#define PRESET_FA_ORTHOGONAL_PROJECTION "fa_orthogonal_projection"

/**
 * @brief Gram-Schmidt正交化
 *
 * 数学定义：将线性无关向量组 $\{v_1, \ldots, v_n\}$ 正交化为正交向量组 $\{e_1, \ldots, e_n\}$：
 * $$e_k = v_k - \sum_{j=1}^{k-1} \frac{\langle v_k, e_j \rangle}{\langle e_j, e_j \rangle} e_j$$
 *
 * 输入：
 *   - vectors: 列表 (PRESET_TYPE_LIST) - 线性无关向量组
 * 输出：
 *   - 列表 (PRESET_TYPE_LIST) - 正交规范化向量组
 *
 * 复杂度：O(n^2)
 */
#define PRESET_FA_GRAM_SCHMIDT "fa_gram_schmidt"

/**
 * @brief Riesz表示定理
 *
 * 数学定义：对Hilbert空间 $H$ 上的有界线性泛函 $f \in H^*$，
 * 存在唯一的 $y_f \in H$ 使得
 * $$f(x) = \langle x, y_f \rangle,\ \forall x \in H$$
 * 且 $\|f\| = \|y_f\|$
 *
 * 输入：
 *   - f: 函数 (PRESET_TYPE_FUNCTION) - 有界线性泛函
 *   - H: 空间 (PRESET_TYPE_SPACE) - Hilbert空间
 * 输出：
 *   - 向量 (PRESET_TYPE_VECTOR) - Riesz表示元 $y_f$
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_RIESZ_REPRESENTATION "fa_riesz_representation"

/* ==================== 不动点理论 ==================== */

/**
 * @brief Banach不动点定理
 *
 * 数学定义：若 $T: M \to M$ 是完备度量空间 $M$ 上的压缩映射
 * （$\exists k \in (0,1): d(Tx, Ty) \leq k d(x, y)$），
 * 则存在唯一的不动点 $x^* = T(x^*)$
 *
 * 输入：
 *   - T: 函数 (PRESET_TYPE_FUNCTION) - 压缩映射
 *   - M: 空间 (PRESET_TYPE_SPACE) - 完备度量空间
 * 输出：
 *   - 向量 (PRESET_TYPE_VECTOR) - 不动点 $x^*$
 *
 * 复杂度：O(k^n)
 */
#define PRESET_FA_FIXED_POINT "fa_fixed_point"

/**
 * @brief 压缩映射判定
 *
 * 数学定义：判定映射 $T$ 是否为压缩映射：
 * $$\exists k \in (0, 1): d(Tx, Ty) \leq k \cdot d(x, y),\ \forall x, y$$
 *
 * 输入：
 *   - T: 函数 (PRESET_TYPE_FUNCTION) - 候选映射
 *   - M: 空间 (PRESET_TYPE_SPACE) - 度量空间
 * 输出：
 *   - 布尔 (PRESET_TYPE_BOOLEAN) - 是否为压缩映射
 *
 * 复杂度：O(∞)
 */
#define PRESET_FA_CONTRACTION_MAPPING "fa_contraction_mapping"

/* ==================== 模块初始化 ==================== */

/**
 * @brief 注册泛函分析预设函数块
 *
 * 此函数由 preset_blocks_init() 自动调用，用户无需直接调用。
 *
 * @return true 注册成功
 * @return false 注册失败
 */
bool preset_functional_analysis_register(void);

/**
 * @brief 获取泛函分析模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_functional_analysis_count(void);

/**
 * @brief 获取泛函分析模块的预设类别
 *
 * @return 预设类别
 */
PresetCategory preset_functional_analysis_category(void);

/**
 * @brief 获取泛函分析模块的所有预设名称
 *
 * @param out_names 输出名称数组（调用者负责释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_functional_analysis_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_FUNCTIONAL_ANALYSIS_H */
