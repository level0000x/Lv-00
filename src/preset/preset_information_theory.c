/**
 * @file preset_information_theory.c
 * @brief 信息论预设函数块模块 - 实现（v2统一宏模式）
 *
 * 实现理论数学研究中常用的信息论运算预设函数块。
 * 涵盖信息度量、信道理论、率失真理论、信息论应用。
 * 共20个预设函数块，均遵循模块化、确定性原则。
 *
 * @module InformationTheory
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 */

#include "preset_information_theory.h"

#include <stdlib.h>
#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 信息论模块预设函数块总数 */
#define INFORMATION_THEORY_PRESET_COUNT 20

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个信息论预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有信息论预设都属于 PRESET_CATEGORY_ANALYSIS 类别。
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
static bool register_information_theory_preset(const char *name, const char *description, const PresetType *input_types,
                                               int input_count, PresetType output_type, const char *math_def,
                                               const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ANALYSIS, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== v2统一注册宏 ==================== */

/**
 * @brief 信息论预设统一注册宏
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
#define REGISTER_IT(name, desc, inputs, in_count, output, math, comp, cons, rev)                                       \
    do {                                                                                                               \
        if (register_information_theory_preset((name), (desc), (inputs), (in_count), (output), (math), (comp), (cons), \
                                               (rev))) {                                                               \
            success_count++;                                                                                           \
        } else {                                                                                                       \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */                                                        \
        }                                                                                                              \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_information_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：信息度量（6个）
     * ============================================================ */

    /**
     * @brief it_entropy - Shannon熵
     *
     * 计算离散随机变量X的Shannon熵 H(X) = -Σ p(x) log₂ p(x)。
     * Shannon熵度量随机变量的不确定性，是信息论的基础概念。
     * 熵越大表示不确定性越高，熵为零表示确定性事件。
     * 对于均匀分布，熵达到最大值 log₂|X|。
     *
     * @param P 概率分布（PRESET_TYPE_DISTRIBUTION）
     * @return Shannon熵 H(X)（PRESET_TYPE_SCALAR）
     * @math H(X) = -\\sum_{x \\in \\mathcal{X}} p(x) \\log_2 p(x)
     * @complexity O(n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION};
        REGISTER_IT("it_entropy", "Shannon熵：计算离散随机变量X的熵 H(X) = -Σ p(x) log₂ p(x)", inputs, 1,
                    PRESET_TYPE_SCALAR, "H(X) = -\\sum_{x \\in \\mathcal{X}} p(x) \\log_2 p(x)", "O(n)", true, false);
    }

    /**
     * @brief it_joint_entropy - 联合熵
     *
     * 计算两个离散随机变量X和Y的联合熵 H(X,Y) = -Σ p(x,y) log₂ p(x,y)。
     * 联合熵度量两个随机变量联合分布的不确定性。
     * 满足 H(X,Y) >= max(H(X), H(Y))，等号成立当且仅当一个变量是另一个的函数。
     *
     * @param P_XY 联合概率分布（PRESET_TYPE_DISTRIBUTION）
     * @return 联合熵 H(X,Y)（PRESET_TYPE_SCALAR）
     * @math H(X,Y) = -\\sum_{x,y} p(x,y) \\log_2 p(x,y)
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION};
        REGISTER_IT("it_joint_entropy", "联合熵：计算两个离散随机变量X和Y的联合熵 H(X,Y) = -Σ p(x,y) log₂ p(x,y)",
                    inputs, 1, PRESET_TYPE_SCALAR, "H(X,Y) = -\\sum_{x,y} p(x,y) \\log_2 p(x,y)", "O(n^2)", true,
                    false);
    }

    /**
     * @brief it_conditional_entropy - 条件熵
     *
     * 计算条件熵 H(Y|X) = H(X,Y) - H(X)。
     * 条件熵度量在已知X的条件下Y的平均不确定性。
     * 等价于 H(Y|X) = Σ p(x) H(Y|X=x)。
     * 满足 H(Y|X) <= H(Y)，等号成立当且仅当X和Y独立。
     *
     * @param P_XY 联合概率分布（PRESET_TYPE_DISTRIBUTION）
     * @param P_X 边缘概率分布（PRESET_TYPE_DISTRIBUTION）
     * @return 条件熵 H(Y|X)（PRESET_TYPE_SCALAR）
     * @math H(Y|X) = H(X,Y) - H(X) = \\sum_x p(x) H(Y|X=x)
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_DISTRIBUTION};
        REGISTER_IT("it_conditional_entropy", "条件熵：计算条件熵 H(Y|X) = H(X,Y) - H(X)，已知X时Y的平均不确定性",
                    inputs, 2, PRESET_TYPE_SCALAR, "H(Y|X) = H(X,Y) - H(X) = \\sum_x p(x) H(Y|X=x)", "O(n^2)", true,
                    false);
    }

    /**
     * @brief it_mutual_information - 互信息
     *
     * 计算互信息 I(X;Y) = H(X) - H(X|Y) = H(Y) - H(Y|X)。
     * 互信息度量两个随机变量之间的统计依赖程度。
     * 等价于 I(X;Y) = Σ p(x,y) log₂(p(x,y)/(p(x)p(y)))。
     * I(X;Y) >= 0，等号成立当且仅当X和Y独立。
     *
     * @param P_XY 联合概率分布（PRESET_TYPE_DISTRIBUTION）
     * @param P_X 边缘概率分布X（PRESET_TYPE_DISTRIBUTION）
     * @param P_Y 边缘概率分布Y（PRESET_TYPE_DISTRIBUTION）
     * @return 互信息 I(X;Y)（PRESET_TYPE_SCALAR）
     * @math I(X;Y) = \\sum_{x,y} p(x,y) \\log_2 \\frac{p(x,y)}{p(x)p(y)}
     * @complexity O(n^2)
     * @constructive true
     * @reversible true（互信息满足对称性 I(X;Y) = I(Y;X)）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_DISTRIBUTION};
        REGISTER_IT("it_mutual_information", "互信息：计算互信息 I(X;Y) = H(X) - H(X|Y)，度量X和Y的统计依赖程度",
                    inputs, 3, PRESET_TYPE_SCALAR, "I(X;Y) = \\sum_{x,y} p(x,y) \\log_2 \\frac{p(x,y)}{p(x)p(y)}",
                    "O(n^2)", true, true);
    }

    /**
     * @brief it_relative_entropy - 相对熵（KL散度）
     *
     * 计算Kullback-Leibler散度 D(P||Q) = Σ p(x) log₂(p(x)/q(x))。
     * KL散度度量两个概率分布之间的"距离"或差异程度。
     * KL散度非负（Gibbs不等式），但不对称，不满足三角不等式。
     * D(P||Q) = 0 当且仅当 P = Q 几乎处处。
     *
     * @param P 概率分布P（PRESET_TYPE_DISTRIBUTION）
     * @param Q 概率分布Q（PRESET_TYPE_DISTRIBUTION）
     * @return KL散度 D(P||Q)（PRESET_TYPE_SCALAR）
     * @math D(P \\| Q) = \\sum_{x} p(x) \\log_2 \\frac{p(x)}{q(x)}
     * @complexity O(n)
     * @constructive true
     * @reversible false（KL散度不对称：D(P||Q) ≠ D(Q||P)）
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_DISTRIBUTION};
        REGISTER_IT("it_relative_entropy", "相对熵（KL散度）：计算 D(P||Q) = Σ p(x) log₂(p(x)/q(x))，度量分布差异",
                    inputs, 2, PRESET_TYPE_SCALAR, "D(P \\| Q) = \\sum_{x} p(x) \\log_2 \\frac{p(x)}{q(x)}", "O(n)",
                    true, false);
    }

    /**
     * @brief it_cross_entropy - 交叉熵
     *
     * 计算交叉熵 H(P,Q) = -Σ p(x) log₂ q(x)。
     * 交叉熵度量使用分布Q对分布P进行编码所需的平均比特数。
     * 满足 H(P,Q) = H(P) + D(P||Q)，即交叉熵等于熵加KL散度。
     * 在机器学习中广泛用作分类问题的损失函数。
     *
     * @param P 真实分布P（PRESET_TYPE_DISTRIBUTION）
     * @param Q 近似分布Q（PRESET_TYPE_DISTRIBUTION）
     * @return 交叉熵 H(P,Q)（PRESET_TYPE_SCALAR）
     * @math H(P,Q) = -\\sum_{x} p(x) \\log_2 q(x) = H(P) + D(P \\| Q)
     * @complexity O(n)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_DISTRIBUTION};
        REGISTER_IT("it_cross_entropy", "交叉熵：计算 H(P,Q) = -Σ p(x) log₂ q(x)，使用Q编码P所需的平均比特数", inputs,
                    2, PRESET_TYPE_SCALAR, "H(P,Q) = -\\sum_{x} p(x) \\log_2 q(x) = H(P) + D(P \\| Q)", "O(n)", true,
                    false);
    }

    /* ============================================================
     * 第二部分：信道理论（5个）
     * ============================================================ */

    /**
     * @brief it_channel_capacity - 信道容量
     *
     * 计算离散无记忆信道的容量 C = max_{p(x)} I(X;Y)。
     * 信道容量是信道能可靠传输信息的最大速率。
     * Shannon信道编码定理保证：速率低于容量的码存在且可实现任意小的错误概率。
     * 容量计算通常需要凸优化方法（如Blahut-Arimoto算法）。
     *
     * @param channel 信道转移矩阵（PRESET_TYPE_MATRIX）
     * @return 信道容量 C（PRESET_TYPE_SCALAR）
     * @math C = \\max_{p(x)} I(X;Y) = \\max_{p(x)} \\sum_{x,y} p(x) p(y|x) \\log_2 \\frac{p(y|x)}{p(y)}
     * @complexity O(n^3)（Blahut-Arimoto迭代）
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        REGISTER_IT("it_channel_capacity", "信道容量：计算离散无记忆信道的容量 C = max I(X;Y)，可靠传输的最大速率",
                    inputs, 1, PRESET_TYPE_SCALAR,
                    "C = \\max_{p(x)} I(X;Y) = \\max_{p(x)} \\sum_{x,y} p(x) p(y|x) \\log_2 \\frac{p(y|x)}{p(y)}",
                    "O(n^3)", true, false);
    }

    /**
     * @brief it_binary_symmetric_channel - 二元对称信道
     *
     * 分析二元对称信道 BSC(p) 的容量和错误概率。
     * BSC(p) 以概率 p 翻转输入比特，以概率 1-p 正确传输。
     * 容量 C = 1 - H(p) = 1 - (-p log₂ p - (1-p) log₂(1-p))。
     * 当 p = 0 或 p = 1 时容量最大（C = 1），当 p = 1/2 时容量为零。
     *
     * @param p 错误概率（PRESET_TYPE_SCALAR）
     * @return 信道容量 C（PRESET_TYPE_SCALAR）
     * @math C_{BSC}(p) = 1 - H(p) = 1 + p \\log_2 p + (1-p) \\log_2(1-p)
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        REGISTER_IT("it_binary_symmetric_channel", "二元对称信道：分析BSC(p)的容量 C = 1 - H(p) 和错误概率", inputs, 1,
                    PRESET_TYPE_SCALAR, "C_{BSC}(p) = 1 - H(p) = 1 + p \\log_2 p + (1-p) \\log_2(1-p)", "O(1)", true,
                    false);
    }

    /**
     * @brief it_binary_erasure_channel - 二元删除信道
     *
     * 分析二元删除信道 BEC(ε) 的容量。
     * BEC(ε) 以概率 ε 删除输入比特（输出为删除符号 e），以概率 1-ε 正确传输。
     * 容量 C = 1 - ε。
     * BEC的容量与删除概率呈线性关系，这是BEC的重要特性。
     *
     * @param epsilon 删除概率（PRESET_TYPE_SCALAR）
     * @return 信道容量 C（PRESET_TYPE_SCALAR）
     * @math C_{BEC}(\\varepsilon) = 1 - \\varepsilon
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        REGISTER_IT("it_binary_erasure_channel", "二元删除信道：分析BEC(ε)的容量 C = 1 - ε", inputs, 1,
                    PRESET_TYPE_SCALAR, "C_{BEC}(\\varepsilon) = 1 - \\varepsilon", "O(1)", true, false);
    }

    /**
     * @brief it_channel_coding_theorem - 信道编码定理
     *
     * Shannon信道编码定理：判定给定速率R是否可达。
     * 正定理：若 R < C，则存在编码方案使错误概率任意小。
     * 逆定理：若 R > C，则不可能使错误概率趋于零。
     * 信道容量是可靠通信的临界速率。
     *
     * @param R 传输速率（PRESET_TYPE_SCALAR）
     * @param C 信道容量（PRESET_TYPE_SCALAR）
     * @return 是否可达（PRESET_TYPE_BOOLEAN）
     * @math R < C \\Rightarrow \\exists \\text{ 码使 } P_e \\to 0; \\quad R > C \\Rightarrow P_e \\text{ 有下界}
     * @complexity O(1)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_IT(
            "it_channel_coding_theorem", "信道编码定理：判定给定速率R是否可达（R < C则存在可靠码）", inputs, 2,
            PRESET_TYPE_BOOLEAN,
            "R < C \\Rightarrow \\exists \\text{ 码使 } P_e \\to 0; \\quad R > C \\Rightarrow P_e \\text{ 有下界}",
            "O(1)", false, false);
    }

    /**
     * @brief it_source_coding_theorem - 信源编码定理
     *
     * Shannon信源编码定理（第一定理）：无损压缩的极限。
     * 定理：任何无损压缩方案的平均码长 L 满足 H(X) <= L < H(X) + 1。
     * Huffman编码可以达到这个下界（在整数码长约束下最优）。
     * 算术编码可以趋近 H(X)。
     *
     * @param P 信源分布（PRESET_TYPE_DISTRIBUTION）
     * @param L 编码平均码长（PRESET_TYPE_SCALAR）
     * @return 是否满足压缩极限（PRESET_TYPE_BOOLEAN）
     * @math H(X) \\leq L < H(X) + 1, \\quad \\text{Huffman编码最优}
     * @complexity O(n)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_SCALAR};
        REGISTER_IT("it_source_coding_theorem", "信源编码定理：判定无损压缩是否满足极限 H(X) <= L < H(X) + 1", inputs,
                    2, PRESET_TYPE_BOOLEAN, "H(X) \\leq L < H(X) + 1, \\quad \\text{Huffman编码最优}", "O(n)", false,
                    false);
    }

    /* ============================================================
     * 第三部分：率失真理论（4个）
     * ============================================================ */

    /**
     * @brief it_rate_distortion_function - 率失真函数
     *
     * 计算率失真函数 R(D) = min_{p(x̂|x): E[d(X,X̂)] <= D} I(X;X̂)。
     * 率失真函数给出在失真不超过D时，编码所需的最低速率。
     * 是信源编码（有损压缩）的理论基础。
     * 对于Bernoulli(p)信源和Hamming失真，R(D) = H(p) - H(D)（D <= p）。
     *
     * @param P 信源分布（PRESET_TYPE_DISTRIBUTION）
     * @param D 目标失真（PRESET_TYPE_SCALAR）
     * @param distortion_matrix 失真矩阵（PRESET_TYPE_MATRIX）
     * @return 率失真函数值 R(D)（PRESET_TYPE_SCALAR）
     * @math R(D) = \\min_{p(\\hat{x}|x): E[d(X,\\hat{X})] \\leq D} I(X;\\hat{X})
     * @complexity O(n^3)（Blahut-Arimoto型算法）
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_SCALAR, PRESET_TYPE_MATRIX};
        REGISTER_IT("it_rate_distortion_function", "率失真函数：计算 R(D) = min I(X;X̂)，失真不超过D时的最低编码速率",
                    inputs, 3, PRESET_TYPE_SCALAR,
                    "R(D) = \\min_{p(\\hat{x}|x): E[d(X,\\hat{X})] \\leq D} I(X;\\hat{X})", "O(n^3)", true, false);
    }

    /**
     * @brief it_distortion_rate_function - 失真率函数
     *
     * 计算失真率函数 D(R) = min_{p(x̂|x): I(X;X̂) <= R} E[d(X,X̂)]。
     * 失真率函数是率失真函数的反函数，给定速率R时的最小可达失真。
     * D(R) 是 R(D) 的反函数，两者互为对偶。
     *
     * @param P 信源分布（PRESET_TYPE_DISTRIBUTION）
     * @param R 目标速率（PRESET_TYPE_SCALAR）
     * @param distortion_matrix 失真矩阵（PRESET_TYPE_MATRIX）
     * @return 失真率函数值 D(R)（PRESET_TYPE_SCALAR）
     * @math D(R) = \\min_{p(\\hat{x}|x): I(X;\\hat{X}) \\leq R} E[d(X,\\hat{X})]
     * @complexity O(n^3)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_SCALAR, PRESET_TYPE_MATRIX};
        REGISTER_IT("it_distortion_rate_function", "失真率函数：计算 D(R) = min E[d(X,X̂)]，给定速率R时的最小可达失真",
                    inputs, 3, PRESET_TYPE_SCALAR,
                    "D(R) = \\min_{p(\\hat{x}|x): I(X;\\hat{X}) \\leq R} E[d(X,\\hat{X})]", "O(n^3)", true, false);
    }

    /**
     * @brief it_quantization - 量化误差分析
     *
     * 分析标量量化和矢量量化的率失真性能。
     * 标量量化：对每个样本独立量化，高分辨率下失真 D ≈ (πe/6)σ² 2^{-2R}。
     * 矢量量化：联合量化一组样本，性能优于标量量化。
     * 矢量量化的失真随维数增加而趋近率失真函数。
     *
     * @param P 信源分布（PRESET_TYPE_DISTRIBUTION）
     * @param R 每样本速率（PRESET_TYPE_SCALAR）
     * @param quant_type 量化类型（PRESET_TYPE_INTEGER，0=标量，1=矢量）
     * @return 量化失真（PRESET_TYPE_SCALAR）
     * @math D_{SQ} \\approx \\frac{\\pi e}{6} \\sigma^2 2^{-2R}, \\quad D_{VQ} \\to R^{-1}(R) \\text{（高维）}
     * @complexity O(n^2)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        REGISTER_IT(
            "it_quantization", "量化误差分析：分析标量/矢量量化的率失真性能和量化失真", inputs, 3, PRESET_TYPE_SCALAR,
            "D_{SQ} \\approx \\frac{\\pi e}{6} \\sigma^2 2^{-2R}, \\quad D_{VQ} \\to R^{-1}(R) \\text{（高维）}",
            "O(n^2)", true, false);
    }

    /**
     * @brief it_data_compression - 数据压缩
     *
     * 分析Huffman编码和算术编码的压缩性能。
     * Huffman编码：前缀码，平均码长 L 满足 H(X) <= L < H(X) + 1。
     * 算术编码：非前缀码，平均码长可趋近 H(X)，适合小字母表。
     * 两者都是无损压缩的经典方法。
     *
     * @param P 信源分布（PRESET_TYPE_DISTRIBUTION）
     * @param method 压缩方法（PRESET_TYPE_INTEGER，0=Huffman，1=算术编码）
     * @return 压缩率和平均码长（PRESET_TYPE_TUPLE）
     * @math L_{Huffman} \\in [H(X), H(X)+1), \\quad L_{Arithmetic} \\to H(X)
     * @complexity O(n log n)（Huffman），O(n)（算术编码）
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_DISTRIBUTION, PRESET_TYPE_INTEGER};
        REGISTER_IT("it_data_compression", "数据压缩：分析Huffman编码、算术编码的压缩率和平均码长", inputs, 2,
                    PRESET_TYPE_TUPLE, "L_{Huffman} \\in [H(X), H(X)+1), \\quad L_{Arithmetic} \\to H(X)", "O(n log n)",
                    true, false);
    }

    /* ============================================================
     * 第四部分：信息论应用（5个）
     * ============================================================ */

    /**
     * @brief it_kolmogorov_complexity - Kolmogorov复杂度
     *
     * 计算字符串s的Kolmogorov复杂度 K(s) = min{|p| : U(p) = s}。
     * K(s) 是在通用图灵机U上输出s的最短程序p的长度。
     * Kolmogorov复杂度是不可计算的（图灵不可判定）。
     * 它提供了信息内容的绝对度量，与概率分布无关。
     *
     * @param s 输入字符串（PRESET_TYPE_STRING）
     * @return Kolmogorov复杂度上界估计（PRESET_TYPE_INTEGER）
     * @math K(s) = \\min\\{|p| : U(p) = s\\}
     * @complexity 不可计算（提供上界估计）
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_STRING};
        REGISTER_IT("it_kolmogorov_complexity",
                    "Kolmogorov复杂度：计算字符串s的最短描述长度 K(s) = min{|p| : U(p) = s}（上界估计）", inputs, 1,
                    PRESET_TYPE_INTEGER, "K(s) = \\min\\{|p| : U(p) = s\\}", "不可计算", false, false);
    }

    /**
     * @brief it_algorithmic_entropy - 算法熵
     *
     * 计算与Kolmogorov复杂度相关的算法熵。
     * 算法熵 H_K(X) = E[K(X)] 是随机变量X的Kolmogorov复杂度的期望。
     * 对于i.i.d.信源，H_K(X) ≈ nH(X) + O(log n)。
     * 算法熵将Shannon熵推广到个体序列。
     *
     * @param X 随机变量/数据序列（PRESET_TYPE_SEQUENCE）
     * @return 算法熵估计（PRESET_TYPE_SCALAR）
     * @math H_K(X) = E[K(X)] \\approx nH(X) + O(\\log n)
     * @complexity 不可计算（提供近似估计）
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        REGISTER_IT("it_algorithmic_entropy", "算法熵：计算随机变量的Kolmogorov复杂度期望 H_K(X) = E[K(X)]（近似估计）",
                    inputs, 1, PRESET_TYPE_SCALAR, "H_K(X) = E[K(X)] \\approx nH(X) + O(\\log n)", "不可计算", false,
                    false);
    }

    /**
     * @brief it_fano_inequality - Fano不等式
     *
     * Fano不等式：H(P_e) + P_e log₂(|X|-1) >= H(X|Y)。
     * 其中 P_e 是估计错误概率，H(P_e) = -P_e log₂ P_e - (1-P_e) log₂(1-P_e)。
     * Fano不等式给出了条件熵的下界，用于证明逆定理。
     * 在信道编码逆定理和信息论下界证明中有重要应用。
     *
     * @param H_X_given_Y 条件熵 H(X|Y)（PRESET_TYPE_SCALAR）
     * @param alphabet_size 字母表大小 |X|（PRESET_TYPE_INTEGER）
     * @return 最小错误概率下界（PRESET_TYPE_SCALAR）
     * @math H(P_e) + P_e \\log_2(|\\mathcal{X}|-1) \\geq H(X|Y)
     * @complexity O(1)
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        REGISTER_IT("it_fano_inequality", "Fano不等式：由条件熵推导最小错误概率下界 H(Pe) + Pe log(|X|-1) >= H(X|Y)",
                    inputs, 2, PRESET_TYPE_SCALAR, "H(P_e) + P_e \\log_2(|\\mathcal{X}|-1) \\geq H(X|Y)", "O(1)", true,
                    false);
    }

    /**
     * @brief it_data_processing_inequality - 数据处理不等式
     *
     * 数据处理不等式：若 X → Y → Z 构成马尔可夫链，则 I(X;Z) <= I(X;Y)。
     * 即通过更多处理步骤不会增加互信息。
     * 等号成立当且仅当 I(X;Y|Z) = 0，即Z包含关于X的所有信息。
     * 这是信息论中的基本不等式，广泛应用于证明各种信息论界。
     *
     * @param I_X_Y 互信息 I(X;Y)（PRESET_TYPE_SCALAR）
     * @param I_X_Z 互信息 I(X;Z)（PRESET_TYPE_SCALAR）
     * @return 是否满足数据处理不等式（PRESET_TYPE_BOOLEAN）
     * @math X \\to Y \\to Z \\Rightarrow I(X;Z) \\leq I(X;Y) \\leq \\min\\{H(X), H(Y)\\}
     * @complexity O(1)
     * @constructive false
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_IT("it_data_processing_inequality", "数据处理不等式：验证马尔可夫链 X→Y→Z 是否满足 I(X;Z) <= I(X;Y)",
                    inputs, 2, PRESET_TYPE_BOOLEAN,
                    "X \\to Y \\to Z \\Rightarrow I(X;Z) \\leq I(X;Y) \\leq \\min\\{H(X), H(Y)\\}", "O(1)", false,
                    false);
    }

    /**
     * @brief it_entropy_maximization - 最大熵原理
     *
 * 在给定约束条件下最大化熵，求满足约束的最无偏概率分布。
     * 常见约束：均值约束（指数分布）、均值和方差约束（高斯分布）、
     * 矩约束（最大熵分布族）。
     * 使用拉格朗日乘数法求解，结果为指数族分布。
     *
     * @param constraints 约束条件列表（PRESET_TYPE_LIST）
     * @param support 支撑集（PRESET_TYPE_SET）
     * @return 最大熵分布（PRESET_TYPE_DISTRIBUTION）
     * @math \\max_{p} H(p) \\text{ s.t. } \\sum_x p(x) f_i(x) = c_i, \\; \\sum_x p(x) = 1
     * @complexity O(n^2)（拉格朗日乘数法）
     * @constructive true
     * @reversible false
     */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_SET};
        REGISTER_IT("it_entropy_maximization", "最大熵原理：在约束条件下最大化熵，求最无偏的概率分布（指数族）", inputs,
                    2, PRESET_TYPE_DISTRIBUTION,
                    "\\max_{p} H(p) \\text{ s.t. } \\sum_x p(x) f_i(x) = c_i, \\; \\sum_x p(x) = 1", "O(n^2)", true,
                    false);
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == INFORMATION_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取信息论预设函数块数量
 *
 * @return int 信息论模块预设函数块总数
 */
int preset_information_theory_count(void) {
    return INFORMATION_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取信息论预设函数块名称列表
 *
 * @param out_names 输出名称数组指针（调用者负责释放）
 * @param out_count 输出预设数量
 * @return true 获取成功
 * @return false 获取失败
 */
bool preset_information_theory_get_names(char ***out_names, int *out_count) {
    if (out_names == NULL || out_count == NULL) {
        return false;
    }

    /* 分配名称数组（使用项目统一的内存管理函数） */
    char **names = (char **) lv00_malloc(INFORMATION_THEORY_PRESET_COUNT * sizeof(char *));
    if (names == NULL) {
        return false;
    }

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 信息度量 */
        "it_entropy", "it_joint_entropy", "it_conditional_entropy", "it_mutual_information", "it_relative_entropy",
        "it_cross_entropy",
        /* 信道理论 */
        "it_channel_capacity", "it_binary_symmetric_channel", "it_binary_erasure_channel", "it_channel_coding_theorem",
        "it_source_coding_theorem",
        /* 率失真理论 */
        "it_rate_distortion_function", "it_distortion_rate_function", "it_quantization", "it_data_compression",
        /* 信息论应用 */
        "it_kolmogorov_complexity", "it_algorithmic_entropy", "it_fano_inequality", "it_data_processing_inequality",
        "it_entropy_maximization"};

    for (int i = 0; i < INFORMATION_THEORY_PRESET_COUNT; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 分配失败时释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv00_free(&tmp);
            }
            {
                void *tmp = names;
                lv00_free(&tmp);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = INFORMATION_THEORY_PRESET_COUNT;
    return true;
}

/**
 * @brief 获取信息论模块类别
 *
 * @return 预设类别枚举值
 */
PresetCategory preset_information_theory_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}
