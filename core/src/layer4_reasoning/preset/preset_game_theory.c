/**
 * @file preset_game_theory.c
 * @brief 博弈论预设函数块 - 实现
 *
 * 实现理论数学研究中常用的博弈论预设函数块。
 * 涵盖策略型博弈、合作博弈、展开型博弈及特殊博弈模型。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module GameTheory
 * @category PRESET_CATEGORY_OPTIMIZATION
 * @version 5.0.0
 */

#include "preset_game_theory.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 博弈论模块预设函数块总数 */
#define GAME_THEORY_PRESET_COUNT 20

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个博弈论预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有博弈论预设使用 PRESET_CATEGORY_OPTIMIZATION 类别。
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
static bool register_game_theory_preset(const char *name, const char *description, const PresetType *input_types,
                                        int input_count, PresetType output_type, const char *math_def,
                                        const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_OPTIMIZATION, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_game_theory_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：策略型博弈
     * ============================================================ */

    /* -------------------- 策略型博弈构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        if (register_game_theory_preset(
                PRESET_GT_NORMAL_FORM_CONSTRUCT,
                "策略型博弈构造：由玩家集合N、策略空间和支付矩阵构造正规型（策略型）博弈 Gamma = (N, {S_i}, {u_i})",
                inputs, 3, PRESET_TYPE_STRUCTURE, "\\Gamma = (N, \\{S_i\\}_{i \\in N}, \\{u_i\\}_{i \\in N})",
                "O(|N| \\cdot |S|)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 占优策略判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        if (register_game_theory_preset(
                PRESET_GT_DOMINANT_STRATEGY,
                "占优策略判定：判定博弈中是否存在（严格）占优策略 s_i* > s_i，即对所有对手策略组合均严格更优", inputs,
                1, PRESET_TYPE_BOOLEAN,
                "s_i^* \\succ s_i \\iff u_i(s_i^*, s_{-i}) > u_i(s_i, s_{-i}), \\quad \\forall s_{-i}",
                "O(|N| \\cdot |S|^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 劣策略剔除 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        if (register_game_theory_preset(
                PRESET_GT_DOMINATED_ELIMINATION,
                "劣策略剔除：迭代剔除所有严格劣策略，每次剔除对所有对手策略组合均非最优的策略，直至无劣策略可剔除",
                inputs, 1, PRESET_TYPE_STRUCTURE,
                "\\text{Iterative elimination: } \\forall s_i', \\exists s_i: u_i(s_i, s_{-i}) > u_i(s_i', s_{-i}), "
                "\\quad \\forall s_{-i}",
                "O(|N| \\cdot |S|^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Nash均衡计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        if (register_game_theory_preset(
                PRESET_GT_NASH_EQUILIBRIUM,
                "Nash均衡计算：计算策略型博弈的所有纯策略Nash均衡，即无人有单方面偏离动机的策略组合", inputs, 1,
                PRESET_TYPE_LIST,
                "u_i(s_i^*, s_{-i}^*) \\ge u_i(s_i, s_{-i}^*), \\quad \\forall s_i \\in S_i, \\quad \\forall i \\in N",
                "O(|S|^{|N|})", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 混合策略Nash均衡 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        if (register_game_theory_preset(
                PRESET_GT_NASH_MIXED,
                "混合策略Nash均衡：计算策略型博弈的混合策略Nash均衡，通过枚举支撑集求解线性方程组", inputs, 1,
                PRESET_TYPE_LIST,
                "\\sigma_i^* \\in \\Delta(S_i): U_i(\\sigma_i^*, \\sigma_{-i}^*) \\ge U_i(\\sigma_i, \\sigma_{-i}^*), "
                "\\quad \\forall \\sigma_i \\in \\Delta(S_i)",
                "O(|S|^{|N|}) 枚举支撑集", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 最优响应 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_TUPLE};
        if (register_game_theory_preset(
                PRESET_GT_BEST_RESPONSE,
                "最优响应：计算给定对手策略组合 s_{-i} 时，玩家 i 的所有最优响应策略 BR_i(s_{-i})", inputs, 2,
                PRESET_TYPE_LIST, "BR_i(s_{-i}) = \\arg\\max_{s_i \\in S_i} u_i(s_i, s_{-i})", "O(|N| \\cdot |S|)",
                true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：合作博弈
     * ============================================================ */

    /* -------------------- 合作博弈构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        if (register_game_theory_preset(
                PRESET_GT_COALITIONAL_GAME_CONSTRUCT,
                "合作博弈构造：由玩家集合N和联盟特征函数 v(S) 构造合作博弈 (N, v)，其中 v(空集) = 0", inputs, 2,
                PRESET_TYPE_STRUCTURE, "(N, v), \\quad v: 2^N \\to \\mathbb{R}, \\quad v(\\emptyset) = 0", "O(2^{|N|})",
                true, false)) {
            success_count++;
        }
    }

    /* -------------------- Shapley值计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        if (register_game_theory_preset(
                PRESET_GT_SHAPLEY_VALUE,
                "Shapley值计算：计算每个玩家 i 的Shapley值 phi_i(v)，基于边际贡献在所有排列上的期望值", inputs, 1,
                PRESET_TYPE_TUPLE,
                "\\phi_i(v) = \\sum_{S \\subseteq N \\setminus \\{i\\}} \\frac{|S|!(|N|-|S|-1)!}{|N|!}[v(S \\cup "
                "\\{i\\}) - v(S)]",
                "O(|N| \\cdot 2^{|N|})", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 核心判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_TUPLE};
        if (register_game_theory_preset(PRESET_GT_CORE_CHECK,
                                        "核心判定：判定分配方案 x 是否在核心 Core(v) "
                                        "中，即满足效率性（总量分配）和联盟稳定性（无联盟有偏离动机）",
                                        inputs, 2, PRESET_TYPE_BOOLEAN,
                                        "\\text{Core}(v) = \\{x \\in \\mathbb{R}^N : \\sum_{i \\in N} x_i = v(N), "
                                        "\\sum_{i \\in S} x_i \\ge v(S), \\forall S \\subseteq N\\}",
                                        "O(2^{|N|})", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Banzhaf权力指数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        if (register_game_theory_preset(PRESET_GT_BANZHAF_POWER,
                                        "Banzhaf权力指数：计算投票博弈中每个玩家的Banzhaf权力指数，衡量其在所有联盟中的"
                                        "边际贡献（关键玩家次数）",
                                        inputs, 1, PRESET_TYPE_TUPLE,
                                        "\\beta_i = \\sum_{S \\subseteq N \\setminus \\{i\\}} [v(S \\cup \\{i\\}) - "
                                        "v(S)], \\quad \\beta_i' = \\beta_i / \\sum_j \\beta_j",
                                        "O(|N| \\cdot 2^{|N|})", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 分配集计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        if (register_game_theory_preset(
                PRESET_GT_IMPUTATION_SET,
                "分配集计算：计算合作博弈的分配集 I(v)，即满足个体理性 x_i >= v({i}) 和效率性 sum(x_i) = v(N) "
                "的所有分配",
                inputs, 1, PRESET_TYPE_SET,
                "I(v) = \\{x \\in \\mathbb{R}^N : x_i \\ge v(\\{i\\}), \\sum_{i \\in N} x_i = v(N)\\}", "O(|N|)", true,
                false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：展开型博弈
     * ============================================================ */

    /* -------------------- 展开型博弈构造 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_TREE, PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        if (register_game_theory_preset(
                PRESET_GT_EXTENSIVE_FORM_CONSTRUCT,
                "展开型博弈构造：由博弈树、玩家行动映射和信息集划分构造展开型博弈 Gamma_E", inputs, 3,
                PRESET_TYPE_STRUCTURE,
                "\\Gamma_E = (N, \\mathcal{X}, \\mathcal{A}, \\alpha, \\mathcal{H}, \\rho, \\sigma, u)",
                "O(|\\mathcal{X}|)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 逆向归纳法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        if (register_game_theory_preset(
                PRESET_GT_BACKWARD_INDUCTION,
                "逆向归纳法：从博弈树终端节点倒推，在每个决策节点选择使当前玩家支付最大化的行动，求解子博弈精炼均衡",
                inputs, 1, PRESET_TYPE_LIST,
                "\\text{从终端节点倒推: } V(x) = \\max_{a \\in A(x)} u_i(a, V(child(x, a)))",
                "O(|\\mathcal{X}| \\cdot |\\mathcal{A}|)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 子博弈精炼均衡判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_LIST};
        if (register_game_theory_preset(
                PRESET_GT_SUBGAME_PERFECT,
                "子博弈精炼均衡判定：判定策略组合 s* 是否为子博弈精炼Nash均衡，即在每个子博弈中都诱导Nash均衡", inputs,
                2, PRESET_TYPE_BOOLEAN,
                "s^* \\text{ 是 SPNE} \\iff s^*|_{\\Gamma'} \\text{ 是每个子博弈 } \\Gamma' \\text{ 的 NE}",
                "O(|\\mathcal{X}|^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 信息集分析 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE};
        if (register_game_theory_preset(
                PRESET_GT_INFORMATION_SET,
                "信息集分析：分析博弈中的信息集划分结构，判定完美信息/不完美信息、完美回忆、单节点信息集等性质", inputs,
                1, PRESET_TYPE_TUPLE,
                "\\mathcal{H}_i = \\{H_{i1}, H_{i2}, \\ldots\\}, \\quad H_{ij} \\subseteq \\mathcal{X}_i, \\quad "
                "\\text{完美信息: } |H| = 1, \\forall H \\in \\mathcal{H}",
                "O(|\\mathcal{X}| \\cdot |\\mathcal{H}|)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：特殊博弈模型
     * ============================================================ */

    /* -------------------- 囚徒困境分析 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        if (register_game_theory_preset(
                PRESET_GT_PRISONERS_DILEMMA,
                "囚徒困境分析：分析囚徒困境博弈的结构，验证 T > R > P > S 且 2R > T + S，识别Nash均衡与社会最优的偏离",
                inputs, 2, PRESET_TYPE_TUPLE,
                "T > R > P > S, \\quad 2R > T + S, \\quad \\text{NE} = (D, D), \\quad \\text{社会最优} = (C, C)",
                "O(|S|^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 极小化极大策略 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        if (register_game_theory_preset(PRESET_GT_MINIMAX,
                                        "极小化极大策略：计算二人零和博弈的极小化极大策略和博弈值，基于von Neumann "
                                        "minimax定理转化为线性规划求解",
                                        inputs, 2, PRESET_TYPE_TUPLE,
                                        "v = \\max_{\\sigma_1} \\min_{\\sigma_2} U_1(\\sigma_1, \\sigma_2) = "
                                        "\\min_{\\sigma_2} \\max_{\\sigma_1} U_1(\\sigma_1, \\sigma_2)",
                                        "O(|S_1|^3 + |S_2|^3) 线性规划", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Pareto最优判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_STRUCTURE, PRESET_TYPE_TUPLE};
        if (register_game_theory_preset(
                PRESET_GT_PARETO_OPTIMAL,
                "Pareto最优判定：判定博弈结果 x 是否为Pareto最优，即不存在其他结果使所有玩家不劣且至少一个玩家严格更优",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "x \\text{ Pareto最优} \\iff \\not\\exists y: u_i(y) \\ge u_i(x), \\forall i \\text{ 且 } \\exists j: "
                "u_j(y) > u_j(x)",
                "O(|N| \\cdot |S|^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 进化稳定策略 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_game_theory_preset(
                PRESET_GT_EVOLUTIONARY_STABLE,
                "进化稳定策略（ESS）判定：判定策略 s* 是否为进化稳定策略，满足Nash均衡条件和进化稳定性条件", inputs, 2,
                PRESET_TYPE_BOOLEAN,
                "s^* \\text{ 是 ESS} \\iff (1)\\ u(s^*, s^*) \\ge u(s, s^*), \\forall s; \\quad (2)\\ u(s, s^*) = "
                "u(s^*, s^*) \\Rightarrow u(s^*, s) > u(s, s)",
                "O(|S|^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Nash讨价还价解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_TUPLE};
        if (register_game_theory_preset(
                PRESET_GT_BARGAINING_NASH,
                "Nash讨价还价解：在可行集F和争议点d下，最大化Nash乘积 (x_1 - d_1)(x_2 - d_2) 求解最优分配", inputs, 2,
                PRESET_TYPE_TUPLE,
                "(x_1^*, x_2^*) = \\arg\\max_{(x_1, x_2) \\in F,\\ x_i \\ge d_i} (x_1 - d_1)(x_2 - d_2)", "O(n) 凸优化",
                true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    /* lv00_log_info("博弈论预设注册完成，共 %d 个预设", success_count) */
    return success_count == GAME_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取博弈论预设函数块数量
 *
 * @return int 博弈论模块预设函数块总数（20）
 */
int preset_game_theory_count(void) {
    return GAME_THEORY_PRESET_COUNT;
}

/**
 * @brief 获取博弈论预设函数块的名称列表
 * @param out_names 输出：名称数组（调用者需先释放每个元素再释放数组本身）
 * @param out_count 输出：名称数量
 * @return true 成功，false 内存分配失败
 */
bool preset_game_theory_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv00_malloc(GAME_THEORY_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* 策略型博弈 */
        PRESET_GT_NORMAL_FORM_CONSTRUCT,
        PRESET_GT_DOMINANT_STRATEGY,
        PRESET_GT_DOMINATED_ELIMINATION,
        PRESET_GT_NASH_EQUILIBRIUM,
        PRESET_GT_NASH_MIXED,
        PRESET_GT_BEST_RESPONSE,
        /* 合作博弈 */
        PRESET_GT_COALITIONAL_GAME_CONSTRUCT,
        PRESET_GT_SHAPLEY_VALUE,
        PRESET_GT_CORE_CHECK,
        PRESET_GT_BANZHAF_POWER,
        PRESET_GT_IMPUTATION_SET,
        /* 展开型博弈 */
        PRESET_GT_EXTENSIVE_FORM_CONSTRUCT,
        PRESET_GT_BACKWARD_INDUCTION,
        PRESET_GT_SUBGAME_PERFECT,
        PRESET_GT_INFORMATION_SET,
        /* 特殊博弈模型 */
        PRESET_GT_PRISONERS_DILEMMA,
        PRESET_GT_MINIMAX,
        PRESET_GT_PARETO_OPTIMAL,
        PRESET_GT_EVOLUTIONARY_STABLE,
        PRESET_GT_BARGAINING_NASH,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
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
    *out_count = count;
    return true;
}
