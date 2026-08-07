/**
 * @file preset_algebraic_topology_adv.c
 * @brief 代数拓扑进阶预设函数块模块 - 实现（v2统一宏模式）
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/algebraic_topology_adv.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的代数拓扑进阶运算预设函数块。
 * 涵盖同伦论、同调理论、正合序列与拓扑不变量。
 * 共8个预设函数块，均遵循模块化、确定性原则。
 *
 * @module AlgebraicTopologyAdv
 * @category PRESET_CATEGORY_TOPOLOGY
 * @version 1.0.0
 */

#include "preset_algebraic_topology_adv.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 代数拓扑进阶模块预设函数块总数：8（与头文件中 ALGEBRAIC_TOPOLOGY_ADV_PRESET_COUNT 一致） */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个代数拓扑进阶预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有代数拓扑进阶预设都属于 PRESET_CATEGORY_TOPOLOGY 类别。
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
LV_DECLARE_PRESET_REGISTER(PRESET_CATEGORY_TOPOLOGY)

/* ==================== 模块注册实现 ==================== */

/* ==================== 模块注册实现 ==================== */

bool preset_algebraic_topology_adv_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：同伦论（3个）
     * ============================================================ */

    /**
     * @brief fundamental_group - 基本群 π₁(X, x₀)
     *
     * 计算拓扑空间X在基点x₀处的基本群 π₁(X, x₀)。
     * 基本群由基点上的所有闭路径的同伦类构成，
     * 群运算是路径的拼接。基本群是代数拓扑中最基本的
     * 同伦不变量，反映了空间的"一维洞"结构。
     *
     * @param X 拓扑空间（PRESET_TYPE_TOPOLOGY）
     * @param x₀ 基点（PRESET_TYPE_POINT）
     * @return 基本群 π₁(X, x₀)（PRESET_TYPE_GROUP）
     * @math \\pi_1(X, x_0) = \\{ [\\gamma] : \\gamma: [0,1] \\to X, \\gamma(0) = \\gamma(1) = x_0 \\} / \\sim
     * @complexity O(∞)（一般不可构造）
     * @constructive false
     * @reversible false
     */
    LV_PRESET_REGISTER(success_count, PRESET_AT_FUNDAMENTAL_GROUP,
            "基本群 π₁(X, x₀)：计算拓扑空间X在基点x₀处的基本群，由闭路径同伦类构成，反映空间的一维洞结构", 2,
            PRESET_TYPE_GROUP,
            "\\pi_1(X, x_0) = \\{ [\\gamma] : \\gamma: [0,1] \\to X, \\gamma(0) = \\gamma(1) = x_0 \\} / \\sim", "O(∞)",
            false, false, PRESET_TYPE_TOPOLOGY, PRESET_TYPE_POINT);

    /**
     * @brief covering_space - 覆盖空间
     *
     * 构造或验证覆盖映射 p: Ỹ → Y。
     * 覆盖映射是局部同胚的满射，使得Y中每个点都有一个
     * 开邻域U，其原像p^{-1}(U)是Ỹ中不相交开集的不交并，
     * 且每个开集通过p同胚于U。覆盖空间理论是研究基本群的重要工具。
     *
     * @param Ỹ 候选覆盖空间（PRESET_TYPE_TOPOLOGY）
     * @param Y 底空间（PRESET_TYPE_TOPOLOGY）
     * @return 是否为覆盖映射（PRESET_TYPE_BOOLEAN）
     * @math p: \\tilde{Y} \\to Y, \\quad \\forall y \\in Y, \\exists U \\ni y: p^{-1}(U) = \\bigsqcup_{\\alpha} V_\\alpha, V_\\alpha \\cong U
     * @complexity O(∞)（验证覆盖性质一般不可判定）
     * @constructive false
     * @reversible false
     */
    LV_PRESET_REGISTER(success_count, PRESET_AT_COVERING_SPACE, "覆盖空间：验证覆盖映射 p: Ỹ → Y，局部同胚且满足离散纤维条件",
                        2, PRESET_TYPE_BOOLEAN,
                        "p: \\tilde{Y} \\to Y, \\quad \\forall y \\in Y, \\exists U \\ni y: p^{-1}(U) = "
                        "\\bigsqcup_{\\alpha} V_\\alpha, V_\\alpha \\cong U",
                        "O(∞)", false, false, PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY);

    /**
     * @brief universal_covering - 万有覆盖空间
     *
     * 构造拓扑空间X的万有覆盖空间 ̃X。
     * 万有覆盖是单连通的覆盖空间，即其基本群平凡。
     * 对道路连通且局部道路连通、半局部单连通的空间，
     * 万有覆盖存在且在等价意义下唯一。万有覆盖的基本群
     * 同构于X的基本群，覆盖变换群也与之同构。
     *
     * @param X 拓扑空间（PRESET_TYPE_TOPOLOGY）
     * @return 万有覆盖空间 ̃X（PRESET_TYPE_TOPOLOGY）
     * @math \\tilde{X} \\to X, \\quad \\pi_1(\\tilde{X}) = 0, \\quad \\text{Aut}(\\tilde{X}/X) \\cong \\pi_1(X)
     * @complexity O(∞)
     * @constructive true
     * @reversible false
     */
    LV_PRESET_REGISTER(success_count,
            PRESET_AT_UNIVERSAL_COVERING, "万有覆盖空间：构造拓扑空间X的万有覆盖空间，单连通且覆盖变换群同构于基本群",
            1, PRESET_TYPE_TOPOLOGY,
            "\\tilde{X} \\to X, \\quad \\pi_1(\\tilde{X}) = 0, \\quad \\text{Aut}(\\tilde{X}/X) \\cong \\pi_1(X)",
            "O(∞)", true, false, PRESET_TYPE_TOPOLOGY);

    /* ============================================================
     * 第二部分：同调理论（3个）
     * ============================================================ */

    /**
     * @brief homology_group - 奇异同调群 Hₙ(X)
     *
     * 计算拓扑空间X的第n维奇异同调群 Hₙ(X)。
     * 同调群通过奇异单形构造链复形C_*(X)，取Zₙ/Bₙ得到。
     * Hₙ(X)是一个Abel群，其秩（Betti数）反映了空间中
     * n维"洞"的数量。同调群是同伦不变量。
     *
     * @param X 拓扑空间（PRESET_TYPE_TOPOLOGY）
     * @param n 维度（PRESET_TYPE_INTEGER）
     * @return n维同调群 Hₙ(X)（PRESET_TYPE_GROUP）
     * @math H_n(X) = \\ker(\\partial_n: C_n(X) \\to C_{n-1}(X)) / \\operatorname{im}(\\partial_{n+1}: C_{n+1}(X) \\to C_n(X))
     * @complexity O(∞)
     * @constructive true
     * @reversible false
     */
    LV_PRESET_REGISTER(success_count, PRESET_AT_HOMOLOGY_GROUP, "同调群 Hₙ(X)：计算拓扑空间X的第n维奇异同调群，反映n维洞结构",
                        2, PRESET_TYPE_GROUP, "H_n(X) = \\ker(\\partial_n) / \\operatorname{im}(\\partial_{n+1})",
                        "O(∞)", true, false, PRESET_TYPE_TOPOLOGY, PRESET_TYPE_INTEGER);

    /**
     * @brief cohomology_group - 上同调群 Hⁿ(X)
     *
     * 计算拓扑空间X的上同调群 Hⁿ(X)。
     * 上同调群是对偶于同调群的结构，由Hom(C_n(X), G)
     * 和δⁿ的上核与核的商得到。上同调具有杯积结构，
     * 构成分次交换环，比同调群携带更多的代数信息。
     *
     * @param X 拓扑空间（PRESET_TYPE_TOPOLOGY）
     * @param n 维度（PRESET_TYPE_INTEGER）
     * @return n维上同调群 Hⁿ(X)（PRESET_TYPE_GROUP）
     * @math H^n(X) = \\ker(\\delta^n) / \\operatorname{im}(\\delta^{n-1}), \\quad \\delta^n: C^n(X) \\to C^{n+1}(X)
     * @complexity O(∞)
     * @constructive true
     * @reversible false
     */
    LV_PRESET_REGISTER(success_count,
            PRESET_AT_COHOMOLOGY_GROUP, "上同调群 Hⁿ(X)：计算拓扑空间X的上同调群，对偶于同调群，具有杯积环结构",
            2, PRESET_TYPE_GROUP,
            "H^n(X) = \\ker(\\delta^n) / \\operatorname{im}(\\delta^{n-1}), \\quad \\delta^n: C^n(X) \\to C^{n+1}(X)",
            "O(∞)", true, false, PRESET_TYPE_TOPOLOGY, PRESET_TYPE_INTEGER);

    /**
     * @brief mayer_vietoris_sequence - Mayer-Vietoris序列
     *
     * 利用空间分解 X = A ∪ B 计算同调群的长正合序列。
     * 若A和B是X的开子集（或满足某些条件），则存在长正合序列：
     * ... → Hₙ(A∩B) → Hₙ(A)⊕Hₙ(B) → Hₙ(X) → Hₙ₋₁(A∩B) → ...
     * 这是计算同调群最强大的工具之一。
     *
     * @param A 第一个子空间（PRESET_TYPE_TOPOLOGY）
     * @param B 第二个子空间（PRESET_TYPE_TOPOLOGY）
     * @param X 整体空间（PRESET_TYPE_TOPOLOGY）
     * @return Mayer-Vietoris长正合序列（PRESET_TYPE_SEQUENCE）
     * @math \\cdots \\to H_n(A \\cap B) \\to H_n(A) \\oplus H_n(B) \\to H_n(X) \\to H_{n-1}(A \\cap B) \\to \\cdots
     * @complexity O(∞)
     * @constructive true
     * @reversible false
     */
    LV_PRESET_REGISTER(success_count,
            PRESET_AT_MAYER_VIETORIS_SEQUENCE, "Mayer-Vietoris序列：利用空间分解 X = A ∪ B 导出同调群的长正合序列",
            3, PRESET_TYPE_SEQUENCE,
            "\\cdots \\to H_n(A \\cap B) \\to H_n(A) \\oplus H_n(B) \\to H_n(X) \\to H_{n-1}(A \\cap B) \\to \\cdots",
            "O(∞)", true, false, PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY, PRESET_TYPE_TOPOLOGY);

    /* ============================================================
     * 第三部分：序列与结构（2个）
     * ============================================================ */

    /**
     * @brief exact_sequence - 正合序列
     *
     * 验证或构造群同态的正合序列。
     * 序列 ... → A_{i-1} → A_i → A_{i+1} → ... 称为正合的，
     * 如果在每处都有 im(前一个同态) = ker(后一个同态)。
     * 正合序列是研究同调代数和代数拓扑的基本工具，
     * 短正合序列 0 → A → B → C → 0 尤为重要。
     *
     * @param seq 群同态序列（PRESET_TYPE_SEQUENCE）
     * @return 是否为正合序列（PRESET_TYPE_BOOLEAN）
     * @math \\operatorname{im}(f_i) = \\ker(f_{i+1}), \\quad \\forall i, \\quad \\cdots \\to A_i \\xrightarrow{f_i} A_{i+1} \\xrightarrow{f_{i+1}} A_{i+2} \\to \\cdots
     * @complexity O(∞)（一般不可判定，有限群情形可计算）
     * @constructive false
     * @reversible false
     */
    LV_PRESET_REGISTER(success_count, PRESET_AT_EXACT_SEQUENCE, "正合序列：验证群同态序列是否满足 im(f_i) = ker(f_{i+1}) 的正合条件",
                        1, PRESET_TYPE_BOOLEAN, "\\operatorname{im}(f_i) = \\ker(f_{i+1}), \\quad \\forall i",
                        "O(∞)", false, false, PRESET_TYPE_SEQUENCE);

    /**
     * @brief euler_characteristic - Euler特征数 χ(X)
     *
     * 计算拓扑空间X的Euler特征数 χ(X)。
     * 对有限CW复形，χ(X) = Σ_{n≥0} (-1)ⁿ rk(Hₙ(X))，
     * 即各维Betti数的交错和。Euler特征数是拓扑不变量，
     * 也是同调论中最重要的数值不变量之一，在组合拓扑和物理中
     * 有着广泛应用（如多面体公式 V - E + F = 2）。
     *
     * @param X 拓扑空间（PRESET_TYPE_TOPOLOGY）
     * @return Euler特征数 χ(X)（PRESET_TYPE_INTEGER）
     * @math \\chi(X) = \\sum_{n=0}^{\\infty} (-1)^n \\operatorname{rk}(H_n(X))
     * @complexity O(∞)（一般拓扑空间）；对有限CW复形为O(m)，m为胞腔数
     * @constructive true
     * @reversible false
     */
    LV_PRESET_REGISTER(success_count, PRESET_AT_EULER_CHARACTERISTIC,
                        "Euler特征数 χ(X)：计算拓扑空间X的Euler特征数，各维Betti数的交错和", 1,
                        PRESET_TYPE_INTEGER, "\\chi(X) = \\sum_{n=0}^{\\infty} (-1)^n \\operatorname{rk}(H_n(X))",
                        "O(∞)", true, false, PRESET_TYPE_TOPOLOGY);

    /* 返回是否所有预设都注册成功 */
    return success_count == ALGEBRAIC_TOPOLOGY_ADV_PRESET_COUNT;
}

/**
 * @brief 获取代数拓扑进阶预设函数块数量
 *
 * @return int 代数拓扑进阶模块预设函数块总数（8）
 */
int preset_algebraic_topology_adv_count(void) {
    return ALGEBRAIC_TOPOLOGY_ADV_PRESET_COUNT;
}
