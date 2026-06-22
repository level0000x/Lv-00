/**
 * @file preset_functional_analysis_adv.c
 * @brief 泛函分析进阶预设函数块模块 - 实现（v2统一宏模式）
 *
 * 实现理论数学研究中常用的泛函分析进阶运算预设函数块。
 * 涵盖空间构造、算子理论与三大基本定理、弱拓扑。
 * 共8个预设函数块，均遵循模块化、确定性原则。
 *
 * @module FunctionalAnalysisAdv
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_functional_analysis_adv.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 泛函分析进阶模块预设函数块总数 */
#define FUNCTIONAL_ANALYSIS_ADV_PRESET_COUNT 8

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个泛函分析进阶预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有泛函分析进阶预设都属于 PRESET_CATEGORY_ANALYSIS 类别。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义（LaTeX格式）
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_fa_adv_preset(
    const char *name, const char *description,
    const PresetType *input_types, int input_count, PresetType output_type,
    const char *math_def, const char *complexity,
    bool is_constructive, bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description, PRESET_CATEGORY_ANALYSIS,
        input_types, input_count, output_type,
        math_def, complexity, is_constructive, is_reversible);
}

/* ==================== v2统一注册宏 ==================== */

/**
 * @brief 泛函分析进阶预设统一注册宏
 *
 * 使用do-while(0)包装，确保宏展开后在语法上等价于单条语句。
 * 注册成功时递增success_count，失败时输出错误日志。
 *
 * @param name       预设名称
 * @param desc       中文描述
 * @param inputs     输入类型数组
 * @param in_count   输入数量
 * @param output     输出类型
 * @param math       数学定义（LaTeX格式字符串）
 * @param comp       时间复杂度
 * @param cons       是否构造性
 * @param rev        是否可逆
 */
#define REGISTER_FA_ADV(name, desc, inputs, in_count, output, math, comp, cons, rev) \
    do { \
        if (register_fa_adv_preset( \
                (name), (desc), (inputs), (in_count), (output), \
                (math), (comp), (cons), (rev))) { \
            success_count++; \
        } else { \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */ \
        } \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_functional_analysis_adv_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：空间构造（2个）
     * ============================================================ */

    /**
     * @brief banach_space - Banach空间验证
     *
     * 验证赋范线性空间 (X, ||·||) 是否为Banach空间。
     * Banach空间是完备的赋范线性空间，即其中每个Cauchy序列
     * 都收敛到空间中的一点。Banach空间是泛函分析的
     * 中心对象之一，其完备性保证了许多重要定理的成立，
     * 如Hahn-Banach定理、一致有界原理和开映射定理。
     *
     * @param X 赋范线性空间（PRESET_TYPE_SPACE）
     * @return 是否为Banach空间（PRESET_TYPE_BOOLEAN）
     * @math (X, \\|\\cdot\\|) \\text{ 是Banach空间} \\Leftrightarrow \\forall \\{x_n\\} \\text{ Cauchy}: \\exists x \\in X: \\lim_{n\\to\\infty} \\|x_n - x\\| = 0
     * @complexity O(∞)（完备性验证一般不可判定）
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE};
        REGISTER_FA_ADV(PRESET_FA_BANACH_SPACE,
            "Banach空间验证：验证赋范线性空间是否完备，即每个Cauchy列都收敛于空间中一点",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "(X, \\|\\cdot\\|) \\text{ 是Banach空间} \\Leftrightarrow \\forall \\{x_n\\} \\text{ Cauchy}: \\exists x \\in X: \\lim_{n\\to\\infty} \\|x_n - x\\| = 0",
            "O(∞)", false, false);
    }

    /**
     * @brief hilbert_space - Hilbert空间验证
     *
     * 验证内积空间 (H, <·,·>) 是否为Hilbert空间。
     * Hilbert空间是完备的内积空间，其范数由内积诱导：
     * ||x|| = √<x,x>。Hilbert空间具有正交性和投影定理
     * 等强大性质，是泛函分析和量子力学的数学基础。
     * 完备性通过Cauchy序列的收敛性来刻画。
     *
     * @param H 内积空间（PRESET_TYPE_SPACE）
     * @return 是否为Hilbert空间（PRESET_TYPE_BOOLEAN）
     * @math (H, \\langle\\cdot,\\cdot\\rangle) \\text{ 是Hilbert空间} \\Leftrightarrow \\forall \\{x_n\\} \\text{ Cauchy}: \\exists x \\in H: \\lim_{n\\to\\infty} \\|x_n - x\\| = 0, \\|x\\| = \\sqrt{\\langle x,x\\rangle}
     * @complexity O(∞)（完备性验证一般不可判定）
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE};
        REGISTER_FA_ADV(PRESET_FA_HILBERT_SPACE,
            "Hilbert空间验证：验证内积空间是否完备，即由内积诱导的范数下每个Cauchy列收敛",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "(H, \\langle\\cdot,\\cdot\\rangle) \\text{ 是Hilbert空间} \\Leftrightarrow \\forall \\{x_n\\} \\text{ Cauchy}: \\exists x \\in H: \\lim \\|x_n - x\\| = 0, \\|x\\| = \\sqrt{\\langle x,x\\rangle}",
            "O(∞)", false, false);
    }

    /* ============================================================
     * 第二部分：算子理论（2个）
     * ============================================================ */

    /**
     * @brief bounded_operator - 有界线性算子
     *
     * 构造或验证有界线性算子 T: X → Y 并计算其算子范数 ||T||。
     * 有界线性算子满足 ||Tx||_Y ≤ M||x||_X，且线性。
     * 算子范数定义为 ||T|| = sup{||Tx||_Y : ||x||_X = 1}。
     * 有界线性算子在Banach空间之间等价于连续线性算子。
     * 算子范数是衡量算子"大小"的基本量。
     *
     * @param T 线性算子（PRESET_TYPE_FUNCTION）
     * @param X 定义域赋范空间（PRESET_TYPE_SPACE）
     * @param Y 值域赋范空间（PRESET_TYPE_SPACE）
     * @return 算子范数 ||T||（PRESET_TYPE_SCALAR）
     * @math \\|T\\| = \\sup\\{\\|Tx\\|_Y : \\|x\\|_X = 1\\} = \\inf\\{M \\geq 0 : \\|Tx\\| \\leq M\\|x\\|, \\forall x\\}
     * @complexity O(∞)（一般不可计算，有限维情形为O(n³)）
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SPACE, PRESET_TYPE_SPACE};
        REGISTER_FA_ADV(PRESET_FA_BOUNDED_OPERATOR,
            "有界线性算子：验证线性算子T: X → Y的有界性并计算算子范数 ||T|| = sup_{||x||=1} ||Tx||",
            inputs, 3, PRESET_TYPE_SCALAR,
            "\\|T\\| = \\sup\\{\\|Tx\\|_Y : \\|x\\|_X = 1\\} = \\inf\\{M \\geq 0 : \\|Tx\\| \\leq M\\|x\\|, \\forall x\\}",
            "O(∞)", true, false);
    }

    /**
     * @brief spectral_theorem - 自伴紧算子的谱定理
     *
     * 自伴紧算子T: H → H 在Hilbert空间H上的谱分解。
     * 根据谱定理，存在H的标准正交基 {eₙ} 和实特征值 λₙ → 0，
     * 使得 Tx = Σₙ λₙ <x, eₙ> eₙ。
     * 谱定理是算子理论的核心结果，将自伴算子"对角化"，
     * 推广了有限维线性代数中的对称矩阵对角化。
     *
     * @param T 自伴紧线性算子（PRESET_TYPE_FUNCTION）
     * @return 特征值序列 {λₙ}（PRESET_TYPE_SEQUENCE）
     * @math Tx = \\sum_{n=1}^{\\infty} \\lambda_n \\langle x, e_n \\rangle e_n, \\quad T \\text{ 自伴紧算子}, \\lambda_n \\in \\mathbb{R}, \\lambda_n \\to 0
     * @complexity O(∞)（一般不可计算）
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        REGISTER_FA_ADV(PRESET_FA_SPECTRAL_THEOREM,
            "谱定理：对自伴紧算子T进行谱分解，得到特征值序列 {λₙ} 满足 Tx = Σₙ λₙ <x, eₙ> eₙ",
            inputs, 1, PRESET_TYPE_SEQUENCE,
            "Tx = \\sum_{n=1}^{\\infty} \\lambda_n \\langle x, e_n \\rangle e_n, \\quad T \\text{ 自伴紧算子}, \\lambda_n \\in \\mathbb{R}, \\lambda_n \\to 0",
            "O(∞)", true, false);
    }

    /* ============================================================
     * 第三部分：三大基本定理（3个）
     * ============================================================ */

    /**
     * @brief hahn_banach_theorem - Hahn-Banach延拓定理
     *
     * 将有界线性泛函 f: M → R（定义在子空间M上）延拓到
     * 整个赋范空间X上的有界线性泛函 F，且保持范数不变：
     * ||F|| = ||f||。这是泛函分析三大基本定理之一，
     * 依赖于Zorn引理（选择公理），在一般的实赋范线性空间中成立。
     * 其推论包括分离超平面定理等。
     *
     * @param f 子空间上的有界线性泛函（PRESET_TYPE_FUNCTION）
     * @param M 子空间（PRESET_TYPE_SET）
     * @param X 赋范线性空间（PRESET_TYPE_SPACE）
     * @return 延拓后的线性泛函 F（PRESET_TYPE_FUNCTION）
     * @math F: X \\to \\mathbb{R}, \\quad F|_M = f, \\quad \\|F\\| = \\|f\\|
     * @complexity O(∞)（非构造性，依赖选择公理）
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET, PRESET_TYPE_SPACE};
        REGISTER_FA_ADV(PRESET_FA_HAHN_BANACH_THEOREM,
            "Hahn-Banach定理：将子空间M上的有界线性泛函f保范延拓到整个赋范空间X",
            inputs, 3, PRESET_TYPE_FUNCTION,
            "F: X \\to \\mathbb{R}, \\quad F|_M = f, \\quad \\|F\\| = \\|f\\|",
            "O(∞)", false, false);
    }

    /**
     * @brief open_mapping_theorem - 开映射定理
     *
     * 在Banach空间之间，满射有界线性算子T: X → Y将开集映射为开集。
     * 即对任意开集U ⊆ X，T(U)在Y中为开集。
     * 开映射定理是泛函分析三大基本定理之二，
     * 也是Baire纲定理的重要推论。它蕴含了
     * 连续线性双射的逆算子也是连续的（逆算子定理）。
     *
     * @param T 满射有界线性算子（PRESET_TYPE_FUNCTION）
     * @return 是否满足开映射性质（PRESET_TYPE_BOOLEAN）
     * @math T: X \\to Y \\text{ 满射}, X,Y \\text{ Banach空间} \\Rightarrow U \\subseteq X \\text{ 开} \\implies T(U) \\subseteq Y \\text{ 开}
     * @complexity O(∞)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        REGISTER_FA_ADV(PRESET_FA_OPEN_MAPPING_THEOREM,
            "开映射定理：Banach空间之间的满射有界线性算子将开集映射为开集",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "T: X \\to Y \\text{ 满射}, X,Y \\text{ Banach空间} \\Rightarrow U \\subseteq X \\text{ 开} \\implies T(U) \\subseteq Y \\text{ 开}",
            "O(∞)", false, false);
    }

    /**
     * @brief closed_graph_theorem - 闭图像定理
     *
     * Banach空间X到Banach空间Y的线性算子T有界当且仅当
     * 其图像 G(T) = {(x, Tx) : x ∈ X} 在X×Y中是闭集。
     * 闭图像定理是泛函分析三大基本定理之三。
     * 若算子T已经定义在整个X上且图像闭，
     * 则T自动连续（有界），这为验证算子的连续性
     * 提供了一种极为有效的方法。
     *
     * @param T 线性算子（PRESET_TYPE_FUNCTION）
     * @return T是否有界（即图像闭蕴含连续性）（PRESET_TYPE_BOOLEAN）
     * @math G(T) = \\{(x, Tx) : x \\in X\\} \\subseteq X \\times Y \\text{ 闭} \\Leftrightarrow T \\text{ 有界}, X,Y \\text{ Banach空间}
     * @complexity O(∞)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        REGISTER_FA_ADV(PRESET_FA_CLOSED_GRAPH_THEOREM,
            "闭图像定理：线性算子T有界当且仅当其图像 G(T) = {(x, Tx)} 在X×Y中为闭集",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "G(T) = \\{(x, Tx) : x \\in X\\} \\subseteq X \\times Y \\text{ 闭} \\Leftrightarrow T \\text{ 有界}, X,Y \\text{ Banach空间}",
            "O(∞)", false, false);
    }

    /* ============================================================
     * 第四部分：弱拓扑（1个）
     * ============================================================ */

    /**
     * @brief weak_convergence - 弱收敛判定
     *
     * 判定赋范空间X中的函数序列 {fₙ} 在弱拓扑下是否收敛。
     * 弱收敛 fₙ ⇀ f 定义为：对所有有界线性泛函 φ ∈ X*，
     * 有 φ(fₙ) → φ(f)。弱收敛弱于范数收敛（强收敛），
     * 强收敛必蕴含弱收敛，但反之不成立。
     * 弱收敛在研究紧性和自反性时至关重要。
     * 例如，在Hilbert空间中，正交序列弱收敛于零。
     *
     * @param f_n 函数序列（PRESET_TYPE_SEQUENCE）
     * @param X 赋范线性空间（PRESET_TYPE_SPACE）
     * @return 是否弱收敛（PRESET_TYPE_BOOLEAN）
     * @math f_n \\rightharpoonup f \\Leftrightarrow \\forall \\varphi \\in X^*: \\lim_{n\\to\\infty} \\varphi(f_n) = \\varphi(f)
     * @complexity O(∞)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_SPACE};
        REGISTER_FA_ADV(PRESET_FA_WEAK_CONVERGENCE,
            "弱收敛判定：判定赋范空间中函数序列是否弱收敛，即对一切有界线性泛函取值收敛",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "f_n \\rightharpoonup f \\Leftrightarrow \\forall \\varphi \\in X^*: \\lim_{n\\to\\infty} \\varphi(f_n) = \\varphi(f)",
            "O(∞)", false, false);
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == FUNCTIONAL_ANALYSIS_ADV_PRESET_COUNT;
}

/**
 * @brief 获取泛函分析进阶预设函数块数量
 *
 * @return int 泛函分析进阶模块预设函数块总数（8）
 */
int preset_functional_analysis_adv_count(void)
{
    return FUNCTIONAL_ANALYSIS_ADV_PRESET_COUNT;
}
