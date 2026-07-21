/**
 * @file preset_algebraic_topology.c
 * @brief 代数拓扑预设函数块 - 实现
 *
 * 实现理论数学研究中常用的代数拓扑运算预设函数块。
 * 涵盖同调论、上同调论、基本群推广和单纯复形四大领域。
 * 共23个预设函数块，均遵循模块化、确定性原则。
 *
 * 采用统一的 preset_blocks_register_simple 注册接口，
 * 使用 REGISTER_AT 宏模式简化注册代码。
 *
 * @module AlgebraicTopology
 * @category PRESET_CATEGORY_TOPOLOGY
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "preset_algebraic_topology.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 代数拓扑模块预设函数块总数（与头文件中 ALGEBRAIC_TOPOLOGY_PRESET_COUNT 一致） */
#define AT_PRESET_COUNT ALGEBRAIC_TOPOLOGY_PRESET_COUNT

/* ==================== REGISTER_AT 宏定义 ==================== */

/**
 * @brief 注册单个代数拓扑预设的便捷宏
 *
 * 封装 preset_blocks_register_simple 调用，简化注册代码。
 * 所有代数拓扑预设使用 PRESET_CATEGORY_TOPOLOGY 类别。
 *
 * @param preset_name   预设名称常量（头文件中定义的宏）
 * @param desc          中文描述
 * @param inputs        输入类型数组（PresetType 复合字面量）
 * @param n_inputs      输入数量
 * @param output        输出类型
 * @param math          数学定义（LaTeX 格式）
 * @param comp          时间复杂度
 * @param constructive  是否构造性
 * @param reversible    是否可逆
 */
#define REGISTER_AT(preset_name, desc, n_inputs, output, math, comp, constructive, reversible, ...) \
    do { \
        PresetType _in[] = { __VA_ARGS__ }; \
        if (register_at_preset(preset_name, desc, _in, n_inputs, output, math, comp, constructive, reversible)) { \
            success_count++; \
        } \
    } while (0)

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个代数拓扑预设
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
static bool register_at_preset(
    const char *name,
    const char *description,
    const PresetType *input_types,
    int input_count,
    PresetType output_type,
    const char *math_def,
    const char *complexity,
    bool is_constructive,
    bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description,
        PRESET_CATEGORY_TOPOLOGY,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

int preset_algebraic_topology_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：同调论（8个）
     * ============================================================ */

    /**
     * @brief 单纯同调群
     *
     * @details 基于单纯复形的单纯链复形计算同调群 H_n(K; G)。
     *          单纯同调是组合拓扑中最基本的同调理论，
     *          通过边界算子 ∂_n: C_n -> C_{n-1} 定义。
     * @param 单纯复形（PRESET_TYPE_STRUCTURE）和系数群（PRESET_TYPE_GROUP）
     * @return 各阶同调群（PRESET_TYPE_GROUP）
     * @math H_n(K; G) = \frac{\ker \partial_n}{\operatorname{im} \partial_{n+1}}
     * @complexity O(2^n)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_SIMPLICIAL_HOMOLOGY,
        "单纯同调群：基于单纯复形的单纯链复形计算同调群 H_n(K; G)",
        2, PRESET_TYPE_GROUP,
        "H_n(K; G) = \\frac{\\ker \\partial_n}{\\operatorname{im} \\partial_{n+1}}, \\quad "
        "\\partial_n: C_n \\to C_{n-1}",
        "O(2^n)", true, false,
        PRESET_TYPE_STRUCTURE, PRESET_TYPE_GROUP);

    /**
     * @brief 奇异同调群
     *
     * @details 计算一般拓扑空间的奇异同调群 H_n(X; G)。
     *          奇异同调是最一般的同调理论，
     *          利用从标准单形到空间的连续映射定义。
     * @param 拓扑空间（PRESET_TYPE_TOPOLOGY）和系数群（PRESET_TYPE_GROUP）
     * @return 各阶奇异同调群（PRESET_TYPE_GROUP）
     * @math H_n(X; G) = \frac{\ker \partial_n}{\operatorname{im} \partial_{n+1}}, \quad \Delta^n \xrightarrow{\sigma} X
     * @complexity O(无穷)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_SINGULAR_HOMOLOGY,
        "奇异同调群：计算一般拓扑空间的奇异同调群 H_n(X; G)（最一般的同调理论）",
        2, PRESET_TYPE_GROUP,
        "H_n(X; G) = \\frac{\\ker \\partial_n}{\\operatorname{im} \\partial_{n+1}}",
        "O(2^n)", false, false,
        PRESET_TYPE_TOPOLOGY, PRESET_TYPE_GROUP);

    /**
     * @brief 相对同调群
     *
     * @details 计算空间对 (X, A) 的相对同调群 H_n(X, A)。
     *          相对同调通过商链复形 C_n(X)/C_n(A) 定义，
     *          是研究空间对的拓扑不变量的基本工具。
     * @param 拓扑空间和子空间（PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY）
     * @return 各阶相对同调群（PRESET_TYPE_GROUP）
     * @math H_n(X, A) = \frac{\ker \partial_n^{rel}}{\operatorname{im} \partial_{n+1}^{rel}}, \quad C_n(X,A) = C_n(X)/C_n(A)
     * @complexity O(2^n)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_RELATIVE_HOMOLOGY,
        "相对同调群：计算空间对 (X, A) 的相对同调群 H_n(X, A)",
        2, PRESET_TYPE_GROUP,
        "H_n(X, A) = \\frac{\\ker \\partial_n^{rel}}{\\operatorname{im} \\partial_{n+1}^{rel}}",
        "O(2^n)", true, false,
        PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY);

    /**
     * @brief Mayer-Vietoris序列
     *
     * @details 利用空间的开覆盖 X = U ∪ V 将同调计算分解为
     *          U、V 和 U ∩ V 的同调，生成长正合序列。
     * @param 两个开子空间及其交（PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY）
     * @return Mayer-Vietoris长正合序列（PRESET_TYPE_STRUCTURE）
     * @math \cdots \to H_n(U\cap V) \to H_n(U)\oplus H_n(V) \to H_n(X) \to H_{n-1}(U\cap V) \to \cdots
     * @complexity O(2^n)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_MAYER_VIETORIS,
        "Mayer-Vietoris序列：利用空间开覆盖 U∪V 分解同调群计算",
        3, PRESET_TYPE_STRUCTURE,
        "\\cdots \\to H_n(U\\cap V) \\to H_n(U)\\oplus H_n(V) \\to H_n(X) \\to H_{n-1}(U\\cap V) \\to \\cdots",
        "O(2^n)", true, false,
        PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY);

    /**
     * @brief 切除定理
     *
     * @details 切除定理断言，如果 Z 的闭包包含在 A 的内部中，
     *          则包含映射诱导的相对同调同构 H_n(X-Z, A-Z) ≅ H_n(X, A)。
     * @param 空间对 (X, A) 和切除子空间 Z（PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY）
     * @return 切除后的相对同调群（PRESET_TYPE_GROUP）
     * @math \bar{Z} \subset \operatorname{int}(A) \Rightarrow H_n(X-Z, A-Z) \cong H_n(X, A)
     * @complexity O(2^n)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_EXCISION_THEOREM,
        "切除定理：验证并应用切除同构 H_n(X-Z, A-Z) ≅ H_n(X, A)（闭包条件）",
        3, PRESET_TYPE_BOOLEAN,
        "\\bar{Z} \\subset \\operatorname{int}(A) \\Rightarrow H_n(X-Z, A-Z) \\cong H_n(X, A)",
        "O(2^n)", false, false,
        PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY);

    /**
     * @brief 胞腔同调
     *
     * @details 基于CW复形的胞腔分解计算同调群。
     *          CW复形的链复形由n维胞腔的自由阿贝尔群生成，
     *          边界算子由附着映射的度数决定。
     * @param CW复形（PRESET_TYPE_STRUCTURE）
     * @return 各阶胞腔同调群（PRESET_TYPE_GROUP）
     * @math H_n^{CW}(X) = H_n(C_n^{cell}, \partial_n^{cell})
     * @complexity O(n_cells)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_CELLULAR_HOMOLOGY,
        "胞腔同调：基于CW复形的胞腔分解计算同调群 H_n^{CW}(X)",
        1, PRESET_TYPE_GROUP,
        "H_n^{CW}(X) = H_n(C_n^{cell}, \\partial_n^{cell})",
        "O(n)", true, false,
        PRESET_TYPE_STRUCTURE);

    /**
     * @brief Betti数
     *
     * @details 计算拓扑空间的各阶Betti数。
     *          Betti数是同调群（自由部分）的秩，
     *          描述了空间中"洞"的数量：beta_0是连通分支数，
     *          beta_1是一维洞数，beta_2是二维空洞数等。
     * @param 同调群数据（PRESET_TYPE_GROUP）
     * @return Betti数列表（PRESET_TYPE_TUPLE）
     * @math \beta_n = \operatorname{rank} H_n(X)
     * @complexity O(n)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_BETTI_NUMBERS,
        "Betti数：计算拓扑空间的Betti数 beta_n = rank H_n(X)，描述'洞'的数量",
        1, PRESET_TYPE_TUPLE,
        "\\beta_n = \\operatorname{rank} H_n(X)",
        "O(n)", true, false,
        PRESET_TYPE_GROUP);

    /**
     * @brief 同调正合序列
     *
     * @details 由空间对 (X, A) 构造长正合同调序列。
     *          该序列通过连接同态 ∂_*: H_n(X,A) -> H_{n-1}(A) 连接，
     *          是代数拓扑中最重要的正合序列之一。
     * @param 空间对（PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY）
     * @return 正合序列结构（PRESET_TYPE_STRUCTURE）
     * @math \cdots \to H_n(A) \to H_n(X) \to H_n(X,A) \xrightarrow{\partial} H_{n-1}(A) \to \cdots
     * @complexity O(2^n)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_HOMOLOGY_EXACT_SEQUENCE,
        "同调正合序列：构造空间对 (X, A) 的长正合同调序列",
        2, PRESET_TYPE_STRUCTURE,
        "\\cdots \\to H_n(A) \\to H_n(X) \\to H_n(X,A) \\xrightarrow{\\partial} H_{n-1}(A) \\to \\cdots",
        "O(2^n)", true, false,
        PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY);

    /* ============================================================
     * 第二部分：上同调论（5个）
     * ============================================================ */

    /**
     * @brief 奇异上同调群
     *
     * @details 计算拓扑空间的奇异上同调群 H^n(X; G)。
     *          上同调通过对偶链复形 C^n = Hom(C_n, G) 定义，
     *          上边缘算子 δ: C^n -> C^{n+1} 是边界算子的对偶。
     * @param 拓扑空间（PRESET_TYPE_TOPOLOGY）和系数群（PRESET_TYPE_GROUP）
     * @return 各阶上同调群（PRESET_TYPE_GROUP）
     * @math H^n(X; G) = \frac{\ker \delta^n}{\operatorname{im} \delta^{n-1}}, \quad \delta^n = (\partial_{n+1})^*
     * @complexity O(2^n)
     * @constructive 否
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_SINGULAR_COHOMOLOGY,
        "奇异上同调群：计算拓扑空间的奇异上同调群 H^n(X; G)（对偶链复形）",
        2, PRESET_TYPE_GROUP,
        "H^n(X; G) = \\frac{\\ker \\delta^n}{\\operatorname{im} \\delta^{n-1}}, \\quad "
        "\\delta^n = (\\partial_{n+1})^*",
        "O(2^n)", false, false,
        PRESET_TYPE_TOPOLOGY, PRESET_TYPE_GROUP);

    /**
     * @brief 上积结构
     *
     * @details 计算上同调类的上积运算。
     *          上积赋予上同调群以分次环结构，
     *          是区分同伦等价但非同伦同胚空间的强大工具。
     * @param 两个上同调类（PRESET_TYPE_GROUP, PRESET_TYPE_GROUP）
     * @return 上积结果（PRESET_TYPE_GROUP）
     * @math \smile: H^p(X) \times H^q(X) \to H^{p+q}(X), \quad (\alpha \smile \beta)(\sigma) = \alpha(\sigma|_{[v_0\ldots v_p]})\beta(\sigma|_{[v_p\ldots v_{p+q}]})
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_CUP_PRODUCT,
        "上积结构：计算两个上同调类的上积 alpha ∪ beta ∈ H^{p+q}(X)",
        2, PRESET_TYPE_GROUP,
        "(\\alpha \\smile \\beta)(\\sigma) = "
        "\\alpha(\\sigma|_{[v_0\\ldots v_p]})\\beta(\\sigma|_{[v_p\\ldots v_{p+q}]})",
        "O(n^2)", true, false,
        PRESET_TYPE_GROUP, PRESET_TYPE_GROUP);

    /**
     * @brief de Rham上同调
     *
     * @details 基于光滑流形上的微分形式计算de Rham上同调群。
     *          de Rham定理断言de Rham上同调与奇异上同调（实系数）同构，
     *          建立了分析和拓扑之间的桥梁。
     * @param 光滑流形（PRESET_TYPE_MANIFOLD）
     * @return 各阶de Rham上同调群（PRESET_TYPE_GROUP）
     * @math H_{dR}^k(M) = \frac{\ker d_k}{\operatorname{im} d_{k-1}}, \quad H_{dR}^k(M) \cong H^k(M; \mathbb{R})
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_DE_RHAM_COHOMOLOGY,
        "de Rham上同调：基于光滑流形上微分形式计算de Rham上同调群",
        1, PRESET_TYPE_GROUP,
        "H_{dR}^k(M) = \\frac{\\ker d_k}{\\operatorname{im} d_{k-1}}, \\quad "
        "H_{dR}^k(M) \\cong H^k(M; \\mathbb{R})",
        "O(n^2)", true, false,
        PRESET_TYPE_MANIFOLD);

    /**
     * @brief 下积
     *
     * @details 计算同调类与上同调类的下积运算。
     *          下积 cap: H_p(X) × H^q(X) -> H_{p-q}(X) 是上积的伴随运算，
     *          在Poincare对偶和相交理论中起核心作用。
     * @param 同调类和上同调类（PRESET_TYPE_GROUP, PRESET_TYPE_GROUP）
     * @return 下积结果（PRESET_TYPE_GROUP）
     * @math \frown: H_p(X) \times H^q(X) \to H_{p-q}(X)
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_CAP_PRODUCT,
        "下积：计算同调类与上同调类的下积 cap: H_p × H^q -> H_{p-q}",
        2, PRESET_TYPE_GROUP,
        "\\frown: H_p(X) \\times H^q(X) \\to H_{p-q}(X)",
        "O(n^2)", true, false,
        PRESET_TYPE_GROUP, PRESET_TYPE_GROUP);

    /**
     * @brief 上同调环
     *
     * @details 构造上同调群在上积运算下的分次环结构 H^*(X; R)。
     *          上同调环是重要的代数不变量，
     *          其结构比单纯的上同调群包含更多拓扑信息。
     * @param 上同调群数据（PRESET_TYPE_GROUP）
     * @return 上同调环结构（PRESET_TYPE_STRUCTURE）
     * @math H^*(X; R) = \bigoplus_{n\ge 0} H^n(X; R), \quad \text{with graded ring structure}
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_COHOMOLOGY_RING,
        "上同调环：构造上同调群的带分次环结构 H^*(X; R)，上积为乘法",
        1, PRESET_TYPE_STRUCTURE,
        "H^*(X; R) = \\bigoplus_{n\\ge 0} H^n(X; R)",
        "O(n^2)", true, false,
        PRESET_TYPE_GROUP);

    /* ============================================================
     * 第三部分：基本群推广 - 高阶同伦（5个）
     * ============================================================ */

    /**
     * @brief 高阶同伦群
     *
     * @details 计算拓扑空间的n阶同伦群 pi_n(X, x0)。
     *          n=1时即为基本群，n>=2时同伦群为阿贝尔群。
     *          同伦群的计算一般是极其困难的。
     * @param 拓扑空间（PRESET_TYPE_TOPOLOGY）和阶数（PRESET_TYPE_INTEGER）
     * @return n阶同伦群（PRESET_TYPE_GROUP）
     * @math \pi_n(X, x_0) = [(S^n, *), (X, x_0)]
     * @complexity O(2^{2^n})
     * @constructive 否
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_HIGHER_HOMOTOPY_GROUPS,
        "高阶同伦群：计算拓扑空间的n阶同伦群 pi_n(X, x0)（n>=2时为阿贝尔群）",
        2, PRESET_TYPE_GROUP,
        "\\pi_n(X, x_0) = [(S^n, *), (X, x_0)], \\quad n \\ge 2 \\Rightarrow \\text{Abel}",
        "O(2^{2^n})", false, false,
        PRESET_TYPE_TOPOLOGY, PRESET_TYPE_INTEGER);

    /**
     * @brief 相对同伦群
     *
     * @details 计算空间对 (X, A) 的相对同伦群 pi_n(X, A, x0)。
     *          相对同伦群的元素是映射 (D^n, S^{n-1}, *) -> (X, A, x0)
     *          的相对同伦类，连接绝对同伦群和同调群。
     * @param 空间对（PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY）和阶数（PRESET_TYPE_INTEGER）
     * @return 相对同伦群（PRESET_TYPE_GROUP）
     * @math \pi_n(X, A, x_0) = [(D^n, S^{n-1}, *), (X, A, x_0)]
     * @complexity O(2^{2^n})
     * @constructive 否
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_RELATIVE_HOMOTOPY,
        "相对同伦群：计算空间对 (X, A) 的相对同伦群 pi_n(X, A, x0)",
        3, PRESET_TYPE_GROUP,
        "\\pi_n(X, A, x_0) = [(D^n, S^{n-1}, *), (X, A, x_0)]",
        "O(2^{2^n})", false, false,
        PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY, PRESET_TYPE_INTEGER);

    /**
     * @brief Hurewicz同态
     *
     * @details 构造从同伦群到同调群的Hurewicz同态。
     *          Hurewicz定理断言：若X为(n-1)-连通（n>=2），
     *          则Hurewicz同态 pi_n(X) -> H_n(X) 为同构。
     *          这是连接同伦和同调的最重要定理。
     * @param 拓扑空间（PRESET_TYPE_TOPOLOGY）和阶数（PRESET_TYPE_INTEGER）
     * @return Hurewicz同态映射（PRESET_TYPE_HOMOMORPHISM）
     * @math h_n: \pi_n(X) \to H_n(X), \quad h_n([f]) = f_*([S^n])
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_HUREWICZ_HOMOMORPHISM,
        "Hurewicz同态：构造从同伦群到同调群的自然同态 h_n: pi_n -> H_n",
        2, PRESET_TYPE_HOMOMORPHISM,
        "h_n: \\pi_n(X) \\to H_n(X), \\quad h_n([f]) = f_*([S^n])",
        "O(n^2)", true, false,
        PRESET_TYPE_TOPOLOGY, PRESET_TYPE_INTEGER);

    /**
     * @brief 同伦正合序列
     *
     * @details 构造空间对 (X, A) 的长正合同伦序列。
     *          该序列是基本群长正合序列向高维的推广，
     *          通过边界映射连接相对同伦群和绝对同伦群。
     * @param 空间对（PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY）
     * @return 同伦正合序列（PRESET_TYPE_STRUCTURE）
     * @math \cdots \to \pi_n(A) \to \pi_n(X) \to \pi_n(X,A) \xrightarrow{\partial} \pi_{n-1}(A) \to \cdots \to \pi_0(A)
     * @complexity O(2^{2^n})
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_HOMOTOPY_EXACT_SEQUENCE,
        "同伦正合序列：构造空间对 (X, A) 的长正合同伦序列",
        2, PRESET_TYPE_STRUCTURE,
        "\\cdots \\to \\pi_n(A) \\to \\pi_n(X) \\to \\pi_n(X,A) \\xrightarrow{\\partial} \\pi_{n-1}(A) \\to \\cdots",
        "O(2^{2^n})", true, false,
        PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY);

    /**
     * @brief Whitehead定理
     *
     * @details 判定CW复形之间的连续映射是否诱导同伦等价。
     *          Whitehead定理断言：若映射 f: X -> Y 诱导所有同伦群的同构，
     *          且X和Y为CW复形，则f为同伦等价。
     * @param 映射和CW复形信息（PRESET_TYPE_FUNCTION, PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE）
     * @return 是否同伦等价（PRESET_TYPE_BOOLEAN）
     * @math f_*: \pi_n(X) \xrightarrow{\cong} \pi_n(Y), \; \forall n \Rightarrow f \text{ 是同伦等价}
     * @complexity O(2^{2^n})
     * @constructive 否
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_WHITEHEAD_THEOREM,
        "Whitehead定理：判定CW复形间映射是否同伦等价（诱导所有同伦群的同构）",
        3, PRESET_TYPE_BOOLEAN,
        "f_*: \\pi_n(X) \\xrightarrow{\\cong} \\pi_n(Y), \\; \\forall n \\Rightarrow f \\text{ 同伦等价}",
        "O(2^{2^n})", false, false,
        PRESET_TYPE_FUNCTION, PRESET_TYPE_STRUCTURE, PRESET_TYPE_STRUCTURE);

    /* ============================================================
     * 第四部分：单纯复形（5个）
     * ============================================================ */

    /**
     * @brief 单纯复形构造
     *
     * @details 由顶点集合和单形列表构造单纯复形K。
     *          单纯复形是代数拓扑中最基本的组合对象，
     *          每个单形由其顶点子集唯一确定。
     * @param 顶点集（PRESET_TYPE_SET）和单形列表（PRESET_TYPE_LIST）
     * @return 单纯复形结构（PRESET_TYPE_STRUCTURE）
     * @math K = \{ \sigma \subset V : \tau \subset \sigma \in K \Rightarrow \tau \in K \}
     * @complexity O(n log n)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_SIMPLICIAL_COMPLEX,
        "单纯复形构造：由顶点集合和单形列表构造单纯复形K（满足子单形封闭性）",
        2, PRESET_TYPE_STRUCTURE,
        "K = \\{ \\sigma \\subset V : \\tau \\subset \\sigma \\in K \\Rightarrow \\tau \\in K \\}",
        "O(n \\log n)", true, false,
        PRESET_TYPE_SET, PRESET_TYPE_LIST);

    /**
     * @brief 三角剖分
     *
     * @details 对拓扑空间或多面体进行三角剖分，
     *          将其表示为单纯复形的几何实现。
     *          三角剖分是将连续对象离散化的基础工具。
     * @param 拓扑空间或多面体（PRESET_TYPE_TOPOLOGY）
     * @return 单纯复形结构（PRESET_TYPE_STRUCTURE）
     * @math |K| \cong X, \quad \text{K是X的三角剖分}
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_TRIANGULATION,
        "三角剖分：对拓扑空间或多面体进行三角剖分，表示为单纯复形",
        1, PRESET_TYPE_STRUCTURE,
        "|K| \\cong X \\text{（同胚）}",
        "O(n^2)", true, false,
        PRESET_TYPE_TOPOLOGY);

    /**
     * @brief Euler示性数
     *
     * @details 计算单纯复形的Euler示性数。
     *          Euler示性数是拓扑不变量，由Poincare证明，
     *          等于各维单形数的交错和。
     * @param 单纯复形（PRESET_TYPE_STRUCTURE）
     * @return Euler示性数（PRESET_TYPE_INTEGER）
     * @math \chi(K) = \sum_{i=0}^n (-1)^i f_i, \quad f_i \text{ 是i维单形数}
     * @complexity O(n)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_EULER_CHARACTERISTIC,
        "Euler示性数：计算单纯复形的Euler示性数 chi = sum (-1)^i * (i维单形数)",
        1, PRESET_TYPE_INTEGER,
        "\\chi(K) = \\sum_{i=0}^n (-1)^i f_i",
        "O(n)", true, false,
        PRESET_TYPE_STRUCTURE);

    /**
     * @brief 重心重分
     *
     * @details 计算单纯复形的重心重分。
     *          重心重分是对每个单形取其重心并重新三角剖分，
     *          保持几何实现不变但使网格细化。
     * @param 单纯复形（PRESET_TYPE_STRUCTURE）
     * @return 重心重分后的单纯复形（PRESET_TYPE_STRUCTURE）
     * @math \operatorname{Sd}(K) \text{ 满足 } |\operatorname{Sd}(K)| = |K|
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_BARYCENTRIC_SUBDIVISION,
        "重心重分：计算单纯复形的重心重分 Sd(K)，细化网格但保持几何实现",
        1, PRESET_TYPE_STRUCTURE,
        "\\operatorname{Sd}(K), \\quad |\\operatorname{Sd}(K)| = |K|",
        "O(n^2)", true, false,
        PRESET_TYPE_STRUCTURE);

    /**
     * @brief 单纯逼近
     *
     * @details 构造连续映射的单纯逼近。
     *          单纯逼近定理断言，通过对值域进行足够多次的重心重分后，
     *          任何连续映射都存在一个单纯逼近。
     * @param 连续映射（PRESET_TYPE_FUNCTION）
     * @return 单纯逼近映射（PRESET_TYPE_FUNCTION）
     * @math g: K \to L \text{ 是f的单纯逼近} \Leftrightarrow |g| \simeq f
     * @complexity O(n^2)
     * @constructive 是
     * @reversible 否
     */
    REGISTER_AT(PRESET_AT_SIMPLICIAL_APPROX,
        "单纯逼近：构造连续映射的单纯逼近 g: K -> L 满足 |g| ≈ f",
        1, PRESET_TYPE_FUNCTION,
        "g: K \\to L \\text{ 是 } f: |K| \\to |L| \\text{ 的单纯逼近}",
        "O(n^2)", true, false,
        PRESET_TYPE_FUNCTION);

    /* 返回是否所有预设都注册成功 */
    if (success_count == AT_PRESET_COUNT) {
        /* lv00_log_info("代数拓扑模块注册成功：%d/%d 个预设", success_count, AT_PRESET_COUNT) */
        return true;
    }

    /* lv00_log_info("代数拓扑模块注册部分失败：%d/%d 个预设", success_count, AT_PRESET_COUNT) */
    return false;
}

/**
 * @brief 获取代数拓扑预设函数块数量
 *
 * @return int 代数拓扑模块预设函数块总数（23）
 */
int preset_algebraic_topology_count(void)
{
    return AT_PRESET_COUNT;
}

/**
 * @brief 获取代数拓扑预设的类别
 *
 * @return PresetCategory 预设类别（PRESET_CATEGORY_TOPOLOGY）
 */
PresetCategory preset_algebraic_topology_category(void)
{
    return PRESET_CATEGORY_TOPOLOGY;
}

/**
 * @brief 获取代数拓扑预设名称列表
 *
 * @param out_names 输出名称数组（调用者需释放每个元素和数组本身）
 * @param out_count 输出数量
 * @return true 成功
 * @return false 失败
 */
bool preset_algebraic_topology_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    char **names = (char**)lv00_malloc(AT_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    const char *preset_names[] = {
        /* 同调论 */
        PRESET_AT_SIMPLICIAL_HOMOLOGY,
        PRESET_AT_SINGULAR_HOMOLOGY,
        PRESET_AT_RELATIVE_HOMOLOGY,
        PRESET_AT_MAYER_VIETORIS,
        PRESET_AT_EXCISION_THEOREM,
        PRESET_AT_CELLULAR_HOMOLOGY,
        PRESET_AT_BETTI_NUMBERS,
        PRESET_AT_HOMOLOGY_EXACT_SEQUENCE,
        /* 上同调论 */
        PRESET_AT_SINGULAR_COHOMOLOGY,
        PRESET_AT_CUP_PRODUCT,
        PRESET_AT_DE_RHAM_COHOMOLOGY,
        PRESET_AT_CAP_PRODUCT,
        PRESET_AT_COHOMOLOGY_RING,
        /* 基本群推广（高阶同伦） */
        PRESET_AT_HIGHER_HOMOTOPY_GROUPS,
        PRESET_AT_RELATIVE_HOMOTOPY,
        PRESET_AT_HUREWICZ_HOMOMORPHISM,
        PRESET_AT_HOMOTOPY_EXACT_SEQUENCE,
        PRESET_AT_WHITEHEAD_THEOREM,
        /* 单纯复形 */
        PRESET_AT_SIMPLICIAL_COMPLEX,
        PRESET_AT_TRIANGULATION,
        PRESET_AT_EULER_CHARACTERISTIC,
        PRESET_AT_BARYCENTRIC_SUBDIVISION,
        PRESET_AT_SIMPLICIAL_APPROX,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) { void *tmp = names[j]; lv00_free(&tmp); }
            { void *tmp = names; lv00_free(&tmp); }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}