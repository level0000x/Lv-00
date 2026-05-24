/**
 * @file preset_optimization.c
 * @brief 优化理论预设函数块 - 实现
 *
 * 实现理论数学研究中常用的优化理论预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module Optimization
 * @category PRESET_CATEGORY_OPTIMIZATION
 * @version 3.2.0
 */

#include "preset_optimization.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 优化理论模块预设函数块总数 */
#define OPTIMIZATION_PRESET_COUNT 22

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个优化理论预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有优化理论预设使用 PRESET_CATEGORY_OPTIMIZATION 类别。
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
static bool register_optimization_preset(
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
        PRESET_CATEGORY_OPTIMIZATION,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_optimization_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：无约束优化
     * ============================================================ */

    /* -------------------- 梯度下降法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE};
        if (register_optimization_preset(
                PRESET_OPT_GRADIENT_DESCENT,
                "梯度下降法：沿目标函数的负梯度方向迭代搜索极小值点",
                inputs, 2, PRESET_TYPE_TUPLE,
                "x_{k+1} = x_k - \\alpha_k \\nabla f(x_k)",
                "O(n \\cdot k)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 牛顿法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE};
        if (register_optimization_preset(
                PRESET_OPT_NEWTON_METHOD,
                "牛顿法：利用目标函数的二阶导数（Hessian矩阵）构造二次近似模型进行迭代",
                inputs, 2, PRESET_TYPE_TUPLE,
                "x_{k+1} = x_k - [\\nabla^2 f(x_k)]^{-1} \\nabla f(x_k)",
                "O(n^3 \\cdot k)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 共轭梯度法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE};
        if (register_optimization_preset(
                PRESET_OPT_CONJUGATE_GRADIENT,
                "共轭梯度法：在共轭方向上依次进行一维搜索，适用于大规模无约束优化问题",
                inputs, 2, PRESET_TYPE_TUPLE,
                "d_{k+1} = -\\nabla f(x_{k+1}) + \\beta_k d_k, \\quad \\beta_k = \\frac{\\|\\nabla f(x_{k+1})\\|^2}{\\|\\nabla f(x_k)\\|^2}",
                "O(n \\cdot k)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 拟牛顿法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE};
        if (register_optimization_preset(
                PRESET_OPT_QUASI_NEWTON,
                "拟牛顿法（BFGS/DFP）：通过梯度差分近似Hessian矩阵的逆，避免直接计算二阶导数",
                inputs, 2, PRESET_TYPE_TUPLE,
                "B_{k+1} = B_k + \\frac{y_k y_k^T}{y_k^T s_k} - \\frac{B_k s_k s_k^T B_k}{s_k^T B_k s_k}",
                "O(n^2 \\cdot k)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：约束优化
     * ============================================================ */

    /* -------------------- 拉格朗日乘子法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST};
        if (register_optimization_preset(
                PRESET_OPT_LAGRANGE_MULTIPLIER,
                "拉格朗日乘子法：引入拉格朗日乘子将等式约束优化问题转化为无约束问题",
                inputs, 2, PRESET_TYPE_TUPLE,
                "\\mathcal{L}(x, \\lambda) = f(x) + \\sum_{i=1}^{m} \\lambda_i h_i(x)",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- KKT条件 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        if (register_optimization_preset(
                PRESET_OPT_KKT_CONDITIONS,
                "KKT条件：约束优化问题局部最优解的必要条件（Karush-Kuhn-Tucker条件）",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "\\nabla f(x^*) + \\sum_i \\lambda_i \\nabla g_i(x^*) + \\sum_j \\mu_j \\nabla h_j(x^*) = 0, \\quad \\lambda_i \\ge 0, \\quad \\lambda_i g_i(x^*) = 0",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 罚函数法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR};
        if (register_optimization_preset(
                PRESET_OPT_PENALTY_METHOD,
                "罚函数法：通过在目标函数中添加约束违反的惩罚项，将约束优化转化为无约束优化",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "P(x, \\rho) = f(x) + \\frac{\\rho}{2} \\sum_{i=1}^{m} [\\max(0, g_i(x))]^2 + \\frac{\\rho}{2} \\sum_{j=1}^{l} h_j(x)^2",
                "O(n \\cdot k)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 障碍函数法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR};
        if (register_optimization_preset(
                PRESET_OPT_BARRIER_METHOD,
                "障碍函数法：在可行域内部添加障碍项，使迭代点始终保持在可行域内并逼近最优解",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "B(x, \\mu) = f(x) - \\mu \\sum_{i=1}^{m} \\ln(-g_i(x))",
                "O(n \\cdot k)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：线性规划
     * ============================================================ */

    /* -------------------- 单纯形法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_MATRIX, PRESET_TYPE_TUPLE};
        if (register_optimization_preset(
                PRESET_OPT_SIMPLEX,
                "单纯形法：在可行域的顶点之间移动，逐步改进目标函数值，求解线性规划问题",
                inputs, 3, PRESET_TYPE_TUPLE,
                "\\min c^T x \\quad \\text{s.t.} \\quad Ax = b, \\quad x \\ge 0",
                "O(2^n) 最坏情况", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 内点法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_MATRIX, PRESET_TYPE_TUPLE};
        if (register_optimization_preset(
                PRESET_OPT_INTERIOR_POINT,
                "内点法：通过障碍函数在可行域内部逼近最优解，具有多项式时间复杂度",
                inputs, 3, PRESET_TYPE_TUPLE,
                "\\min c^T x - \\mu \\sum_{i} \\ln x_i \\quad \\text{s.t.} \\quad Ax = b, \\quad x > 0",
                "O(n^{3.5} L)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 对偶单纯形法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_MATRIX, PRESET_TYPE_TUPLE};
        if (register_optimization_preset(
                PRESET_OPT_DUAL_SIMPLEX,
                "对偶单纯形法：基于对偶理论，在保持对偶可行性的前提下恢复原始可行性",
                inputs, 3, PRESET_TYPE_TUPLE,
                "\\max b^T y \\quad \\text{s.t.} \\quad A^T y \\le c",
                "O(2^n) 最坏情况", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：凸优化
     * ============================================================ */

    /* -------------------- 凸性检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_optimization_preset(
                PRESET_OPT_CONVEXITY_TEST,
                "凸性检验：判断目标函数是否为凸函数（Hessian矩阵半正定）",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "f(\\theta x + (1-\\theta)y) \\le \\theta f(x) + (1-\\theta)f(y), \\quad \\forall \\theta \\in [0,1]",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 凸梯度法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE, PRESET_TYPE_SCALAR};
        if (register_optimization_preset(
                PRESET_OPT_CVX_GRADIENT,
                "凸梯度法：针对凸函数的梯度下降优化，保证收敛到全局最优解",
                inputs, 3, PRESET_TYPE_TUPLE,
                "x_{k+1} = x_k - \\alpha \\nabla f(x_k), \\quad f(x^*) \\le f(x_k) - \\frac{\\|\\nabla f(x_k)\\|^2}{2L}",
                "O(n \\cdot k)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 近端梯度法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE};
        if (register_optimization_preset(
                PRESET_OPT_PROXIMAL_GRADIENT,
                "近端梯度法：处理复合目标函数 min f(x)+g(x)，其中f光滑、g非光滑但可计算近端算子",
                inputs, 3, PRESET_TYPE_TUPLE,
                "x_{k+1} = \\text{prox}_{\\alpha g}(x_k - \\alpha \\nabla f(x_k)), \\quad \\text{prox}_{g}(v) = \\arg\\min_x \\frac{1}{2}\\|x - v\\|^2 + g(x)",
                "O(n \\cdot k)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ADMM -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_MATRIX};
        if (register_optimization_preset(
                PRESET_OPT_ADMM,
                "交替方向乘子法（ADMM）：将全局优化问题分解为局部子问题，适用于分布式凸优化",
                inputs, 3, PRESET_TYPE_TUPLE,
                "\\min f(x) + g(z) \\quad \\text{s.t.} \\quad Ax + Bz = c",
                "O(n \\cdot k)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：变分法
     * ============================================================ */

    /* -------------------- 欧拉-拉格朗日方程 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_optimization_preset(
                PRESET_OPT_EULER_LAGRANGE,
                "欧拉-拉格朗日方程：泛函极值问题的必要条件，将变分问题转化为微分方程",
                inputs, 1, PRESET_TYPE_EQUATION,
                "\\frac{\\partial L}{\\partial y} - \\frac{d}{dx}\\frac{\\partial L}{\\partial y'} = 0",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 变分法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_optimization_preset(
                PRESET_OPT_CALCULUS_OF_VARIATIONS,
                "变分法：求解泛函极值问题的数学框架，寻找使泛函取极值的函数",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "J[y] = \\int_a^b L(x, y(x), y'(x)) \\, dx \\to \\min",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：对偶理论
     * ============================================================ */

    /* -------------------- 对偶问题 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        if (register_optimization_preset(
                PRESET_OPT_DUAL_PROBLEM,
                "对偶问题：构造原优化问题的Lagrange对偶问题",
                inputs, 3, PRESET_TYPE_FUNCTION,
                "g(\\lambda, \\mu) = \\inf_x \\mathcal{L}(x, \\lambda, \\mu) = \\inf_x \\left[ f(x) + \\sum_i \\lambda_i g_i(x) + \\sum_j \\mu_j h_j(x) \\right]",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 强对偶性检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        if (register_optimization_preset(
                PRESET_OPT_STRONG_DUALITY_TEST,
                "强对偶性检验：验证原问题最优值与对偶问题最优值是否相等（对偶间隙为零）",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "p^* = d^* \\quad \\text{其中} \\quad p^* = \\inf_x f_0(x), \\quad d^* = \\sup_{\\lambda, \\mu} g(\\lambda, \\mu)",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 弱对偶性检验 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_LIST};
        if (register_optimization_preset(
                PRESET_OPT_WEAK_DUALITY_TEST,
                "弱对偶性检验：验证对偶间隙是否非负（d* <= p* 恒成立）",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "d^* \\le p^* \\quad \\text{（对偶间隙} \\quad p^* - d^* \\ge 0 \\text{）}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第七部分：全局优化
     * ============================================================ */

    /* -------------------- 模拟退火 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE, PRESET_TYPE_SCALAR};
        if (register_optimization_preset(
                PRESET_OPT_SIMULATED_ANNEALING,
                "模拟退火：受物理退火过程启发的概率性全局优化算法，通过接受劣解来跳出局部最优",
                inputs, 3, PRESET_TYPE_TUPLE,
                "P(\\text{接受}) = \\begin{cases} 1 & \\Delta E < 0 \\\\ \\exp(-\\Delta E / T_k) & \\Delta E \\ge 0 \\end{cases}",
                "O(n \\cdot k)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 遗传算法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR};
        if (register_optimization_preset(
                PRESET_OPT_GENETIC_ALGORITHM,
                "遗传算法：基于自然选择和遗传机制的全局优化元启发式算法，通过选择、交叉、变异操作进化种群",
                inputs, 3, PRESET_TYPE_TUPLE,
                "x_{\\text{child}} = \\text{Crossover}(x_{p1}, x_{p2}), \\quad x' = \\text{Mutate}(x, p_m)",
                "O(N \\cdot G \\cdot n)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    /* lv00_log_info("优化理论预设注册完成，共 %d 个预设", success_count) */
    return success_count == OPTIMIZATION_PRESET_COUNT;
}

/**
 * @brief 获取优化理论预设函数块的类别
 * @return 预设类别枚举值
 */
PresetCategory preset_optimization_category(void)
{
    return PRESET_CATEGORY_CUSTOM;
}

/**
 * @brief 获取优化理论预设函数块的名称列表
 * @param out_names 输出：名称数组（调用者需先释放每个元素再释放数组本身）
 * @param out_count 输出：名称数量
 * @return true 成功，false 内存分配失败
 */
bool preset_optimization_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;
    *out_count = OPTIMIZATION_PRESET_COUNT;
    char **names = (char **)lv00_malloc(OPTIMIZATION_PRESET_COUNT * sizeof(char *));
    if (!names) return false;

    const char *preset_names[] = {
        /* 无约束优化 */
        PRESET_OPT_GRADIENT_DESCENT,
        PRESET_OPT_NEWTON_METHOD,
        PRESET_OPT_CONJUGATE_GRADIENT,
        PRESET_OPT_QUASI_NEWTON,
        /* 约束优化 */
        PRESET_OPT_LAGRANGE_MULTIPLIER,
        PRESET_OPT_KKT_CONDITIONS,
        PRESET_OPT_PENALTY_METHOD,
        PRESET_OPT_BARRIER_METHOD,
        /* 线性规划 */
        PRESET_OPT_SIMPLEX,
        PRESET_OPT_INTERIOR_POINT,
        PRESET_OPT_DUAL_SIMPLEX,
        /* 凸优化 */
        PRESET_OPT_CONVEXITY_TEST,
        PRESET_OPT_CVX_GRADIENT,
        PRESET_OPT_PROXIMAL_GRADIENT,
        PRESET_OPT_ADMM,
        /* 变分法 */
        PRESET_OPT_EULER_LAGRANGE,
        PRESET_OPT_CALCULUS_OF_VARIATIONS,
        /* 对偶理论 */
        PRESET_OPT_DUAL_PROBLEM,
        PRESET_OPT_STRONG_DUALITY_TEST,
        PRESET_OPT_WEAK_DUALITY_TEST,
        /* 全局优化 */
        PRESET_OPT_SIMULATED_ANNEALING,
        PRESET_OPT_GENETIC_ALGORITHM,
    };

    for (int i = 0; i < OPTIMIZATION_PRESET_COUNT; i++) {
        size_t len = strlen(preset_names[i]) + 1;
        names[i] = (char *)lv00_malloc(len);
        if (!names[i]) {
            for (int j = 0; j < i; j++) { void *tmp = names[j]; lv00_free(&tmp); }
            { void *tmp = names; lv00_free(&tmp); }
            return false;
        }
        memcpy(names[i], preset_names[i], len);
    }
    *out_names = names;
    return true;
}
