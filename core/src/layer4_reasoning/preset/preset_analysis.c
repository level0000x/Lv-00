/**
 * @file preset_analysis.c
 * @brief 分析学预设函数块 - 实现
 *
 * 实现理论数学研究中常用的分析学运算预设函数块。
 * 涵盖极限、微分、积分、级数、函数空间及度量空间。
 *
 * @module Analysis
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 */

/*
 * ============================================================
 * 头文件包含说明
 * ============================================================
 * preset_analysis.h -> preset_blocks.h -> func_block_registry.h
 *   -> 提供 PresetType 枚举、preset_blocks_register_simple() 声明
 *   -> 提供 PresetCategory 枚举（PRESET_CATEGORY_ANALYSIS 等）
 * preset_common.h
 *   -> 提供 PRESET_REGISTER 等宏、preset_register_common() 内联函数
 *   -> 提供 PRESET_SAFE_MALLOC 等安全内存操作宏
 * lv00_internal.h / lv00_utils.h
 *   -> 提供 lv00_malloc、lv00_free、lv00_strdup、lv00_log_* 等
 * ============================================================
 */
#include "preset_analysis.h"
#include "preset_blocks.h"
#include "preset_common.h"     /* 预设公共宏与辅助函数（PRESET_ERROR_LOG 等） */
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 分析学模块预设函数块总数 */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个分析学预设
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_analysis_preset(
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
        PRESET_CATEGORY_ANALYSIS,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_analysis_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：极限运算
     * ============================================================ */

    /* -------------------- 数列极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_analysis_preset(
                PRESET_SEQUENCE_LIMIT,
                "计算数列的极限 lim(n→∞) a_n",
                inputs, 1, PRESET_TYPE_LIMIT,
                "\\lim_{n \\to \\infty} a_n = L \\Leftrightarrow "
                "\\forall \\epsilon > 0, \\exists N: n > N \\Rightarrow |a_n - L| < \\epsilon",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 函数极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_FUNCTION_LIMIT,
                "计算函数在某点的极限 lim(x→a) f(x)",
                inputs, 2, PRESET_TYPE_LIMIT,
                "\\lim_{x \\to a} f(x) = L \\Leftrightarrow "
                "\\forall \\epsilon > 0, \\exists \\delta: 0 < |x-a| < \\delta \\Rightarrow |f(x)-L| < \\epsilon",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 左极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_LEFT_LIMIT,
                "计算函数在某点的左极限 lim(x→a⁻) f(x)",
                inputs, 2, PRESET_TYPE_LIMIT,
                "\\lim_{x \\to a^-} f(x) = L",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 右极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_RIGHT_LIMIT,
                "计算函数在某点的右极限 lim(x→a⁺) f(x)",
                inputs, 2, PRESET_TYPE_LIMIT,
                "\\lim_{x \\to a^+} f(x) = L",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 上极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_analysis_preset(
                PRESET_LIMIT_SUPERIOR,
                "计算数列的上极限 lim sup a_n",
                inputs, 1, PRESET_TYPE_LIMIT,
                "\\limsup_{n \\to \\infty} a_n = \\lim_{n \\to \\infty} \\sup_{k \\ge n} a_k",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 下极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_analysis_preset(
                PRESET_LIMIT_INFERIOR,
                "计算数列的下极限 lim inf a_n",
                inputs, 1, PRESET_TYPE_LIMIT,
                "\\liminf_{n \\to \\infty} a_n = \\lim_{n \\to \\infty} \\inf_{k \\ge n} a_k",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 极限存在性判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_analysis_preset(
                PRESET_LIMIT_EXISTS_TEST,
                "判定数列极限是否存在",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\lim a_n \\text{ 存在} \\Leftrightarrow \\limsup a_n = \\liminf a_n",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：连续性
     * ============================================================ */

    /* -------------------- 连续性判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_CONTINUITY_TEST,
                "判定函数在某点是否连续",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "f \\text{ 在 } a \\text{ 连续} \\Leftrightarrow \\lim_{x \\to a} f(x) = f(a)",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 一致连续判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_REGION};
        if (register_analysis_preset(
                PRESET_UNIFORM_CONTINUITY_TEST,
                "判定函数在区间上是否一致连续",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "f \\text{ 一致连续} \\Leftrightarrow \\forall \\epsilon, \\exists \\delta: "
                "|x-y| < \\delta \\Rightarrow |f(x)-f(y)| < \\epsilon",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 间断点分类 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_DISCONTINUITY_CLASSIFY,
                "对函数的间断点进行分类（可去、跳跃、无穷、振荡）",
                inputs, 2, PRESET_TYPE_INTEGER,
                "\\text{第一类: 可去/跳跃; 第二类: 无穷/振荡}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Lipschitz 连续判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_REGION};
        if (register_analysis_preset(
                PRESET_LIPSCHITZ_TEST,
                "判定函数是否 Lipschitz 连续",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "\\exists L: |f(x) - f(y)| \\le L|x-y|, \\forall x, y",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：微分运算
     * ============================================================ */

    /* -------------------- 导数计算 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_DERIVATIVE,
                "计算函数在某点的导数 f'(a)",
                inputs, 2, PRESET_TYPE_DERIVATIVE,
                "f'(a) = \\lim_{h \\to 0} \\frac{f(a+h) - f(a)}{h}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 高阶导数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_INTEGER};
        if (register_analysis_preset(
                PRESET_HIGHER_DERIVATIVE,
                "计算函数的 n 阶导数 f^(n)(x)",
                inputs, 2, PRESET_TYPE_DERIVATIVE,
                "f^{(n)}(x) = \\frac{d^n f}{dx^n}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 偏导数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_INTEGER, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_PARTIAL_DERIVATIVE,
                "计算多元函数的偏导数 ∂f/∂x_i",
                inputs, 3, PRESET_TYPE_DERIVATIVE,
                "\\frac{\\partial f}{\\partial x_i} = \\lim_{h \\to 0} \\frac{f(x+he_i) - f(x)}{h}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 梯度 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_analysis_preset(
                PRESET_GRADIENT,
                "计算标量场的梯度 ∇f",
                inputs, 1, PRESET_TYPE_VECTOR,
                "\\nabla f = \\left(\\frac{\\partial f}{\\partial x_1}, \\ldots, \\frac{\\partial f}{\\partial x_n}\\right)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 散度 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_analysis_preset(
                PRESET_DIVERGENCE,
                "计算向量场的散度 ∇·F",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\nabla \\cdot \\mathbf{F} = \\sum_{i=1}^{n} \\frac{\\partial F_i}{\\partial x_i}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 旋度 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_analysis_preset(
                PRESET_CURL,
                "计算三维向量场的旋度 ∇×F",
                inputs, 1, PRESET_TYPE_VECTOR,
                "\\nabla \\times \\mathbf{F} = "
                "\\begin{vmatrix} \\mathbf{i} & \\mathbf{j} & \\mathbf{k} \\\\ "
                "\\frac{\\partial}{\\partial x} & \\frac{\\partial}{\\partial y} & \\frac{\\partial}{\\partial z} \\\\ "
                "F_x & F_y & F_z \\end{vmatrix}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 拉普拉斯算子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_analysis_preset(
                PRESET_LAPLACIAN,
                "计算标量场的拉普拉斯算子 Δf",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\Delta f = \\nabla^2 f = \\sum_{i=1}^{n} \\frac{\\partial^2 f}{\\partial x_i^2}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 可微性判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_DIFFERENTIABILITY_TEST,
                "判定函数在某点是否可微",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "f \\text{ 可微} \\Leftrightarrow \\lim_{h \\to 0} \\frac{f(a+h) - f(a) - f'(a)h}{h} = 0",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 泰勒展开 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER};
        if (register_analysis_preset(
                PRESET_TAYLOR_EXPANSION,
                "计算函数在 a 点的 n 阶泰勒展开",
                inputs, 3, PRESET_TYPE_POLYNOMIAL,
                "f(x) = \\sum_{k=0}^{n} \\frac{f^{(k)}(a)}{k!}(x-a)^k + R_n(x)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：积分运算
     * ============================================================ */

    /* -------------------- 不定积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_analysis_preset(
                PRESET_INDEFINITE_INTEGRAL,
                "计算函数的不定积分 ∫f(x)dx",
                inputs, 1, PRESET_TYPE_FUNCTION,
                "\\int f(x) \\, dx = F(x) + C, \\text{其中 } F'(x) = f(x)",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 定积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_DEFINITE_INTEGRAL,
                "计算定积分 ∫ₐᵇ f(x)dx",
                inputs, 3, PRESET_TYPE_SCALAR,
                "\\int_a^b f(x) \\, dx = F(b) - F(a)",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 广义积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_IMPROPER_INTEGRAL,
                "计算广义积分（无穷积分或瑕积分）",
                inputs, 2, PRESET_TYPE_SCALAR,
                "\\int_a^{\\infty} f(x) \\, dx = \\lim_{b \\to \\infty} \\int_a^b f(x) \\, dx",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 重积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_REGION, PRESET_TYPE_INTEGER};
        if (register_analysis_preset(
                PRESET_MULTIPLE_INTEGRAL,
                "计算重积分 ∫∫...∫_D f dV",
                inputs, 3, PRESET_TYPE_SCALAR,
                "\\int \\cdots \\int_D f(x_1, \\ldots, x_n) \\, dV",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 曲线积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_PATH};
        if (register_analysis_preset(
                PRESET_LINE_INTEGRAL,
                "计算曲线积分 ∫_C f ds 或 ∫_C F·dr",
                inputs, 2, PRESET_TYPE_SCALAR,
                "\\int_C \\mathbf{F} \\cdot d\\mathbf{r} = \\int_a^b \\mathbf{F}(\\mathbf{r}(t)) \\cdot \\mathbf{r}'(t) \\, dt",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 曲面积分 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SURFACE};
        if (register_analysis_preset(
                PRESET_SURFACE_INTEGRAL,
                "计算曲面积分 ∬_S f dS 或 ∬_S F·dS",
                inputs, 2, PRESET_TYPE_SCALAR,
                "\\iint_S \\mathbf{F} \\cdot d\\mathbf{S} = \\iint_D \\mathbf{F} \\cdot \\mathbf{n} \\, dS",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 可积性判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_REGION};
        if (register_analysis_preset(
                PRESET_INTEGRABILITY_TEST,
                "判定函数在区间上是否黎曼可积",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "f \\text{ 可积} \\Leftrightarrow \\text{间断点集为零测集}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：级数运算
     * ============================================================ */

    /* -------------------- 级数收敛判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_analysis_preset(
                PRESET_SERIES_CONVERGENCE_TEST,
                "判定数项级数是否收敛（比较、比值、根值判别法）",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\sum a_n \\text{ 收敛} \\Leftrightarrow \\{S_n\\} \\text{ 收敛}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 绝对收敛判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_analysis_preset(
                PRESET_ABSOLUTE_CONVERGENCE,
                "判定级数是否绝对收敛",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\sum a_n \\text{ 绝对收敛} \\Leftrightarrow \\sum |a_n| \\text{ 收敛}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 条件收敛判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_analysis_preset(
                PRESET_CONDITIONAL_CONVERGENCE,
                "判定级数是否条件收敛",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\sum a_n \\text{ 条件收敛} \\Leftrightarrow \\sum a_n \\text{ 收敛} \\land \\sum |a_n| \\text{ 发散}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 幂级数收敛半径 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_analysis_preset(
                PRESET_POWER_SERIES_RADIUS,
                "计算幂级数的收敛半径 R",
                inputs, 1, PRESET_TYPE_SCALAR,
                "R = \\frac{1}{\\limsup_{n \\to \\infty} \\sqrt[n]{|a_n|}}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 级数求和 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_analysis_preset(
                PRESET_SERIES_SUM,
                "计算收敛级数的和",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\sum_{n=1}^{\\infty} a_n = \\lim_{N \\to \\infty} S_N",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 傅里叶级数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_FOURIER_SERIES,
                "计算函数的傅里叶级数展开",
                inputs, 2, PRESET_TYPE_SEQUENCE,
                "f(x) \\sim \\frac{a_0}{2} + \\sum_{n=1}^{\\infty} (a_n \\cos nx + b_n \\sin nx)",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：函数空间
     * ============================================================ */

    /* -------------------- L^p 范数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_REGION};
        if (register_analysis_preset(
                PRESET_LP_NORM,
                "计算函数的 L^p 范数",
                inputs, 3, PRESET_TYPE_SCALAR,
                "\\|f\\|_p = \\left(\\int_D |f|^p \\, dx\\right)^{1/p}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 一致范数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_REGION};
        if (register_analysis_preset(
                PRESET_SUP_NORM,
                "计算函数的一致范数（上确界范数）",
                inputs, 2, PRESET_TYPE_SCALAR,
                "\\|f\\|_{\\infty} = \\sup_{x \\in D} |f(x)|",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 完备化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE};
        if (register_analysis_preset(
                PRESET_COMPLETION,
                "构造度量空间的完备化",
                inputs, 1, PRESET_TYPE_SPACE,
                "\\hat{X} = \\{\\text{柯西序列等价类}\\}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 紧致性判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE};
        if (register_analysis_preset(
                PRESET_COMPACTNESS_TEST,
                "判定函数空间是否紧致（Arzelà-Ascoli 定理）",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\mathcal{F} \\text{ 紧致} \\Leftrightarrow \\text{一致有界且等度连续}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第七部分：度量空间
     * ============================================================ */

    /* -------------------- 度量空间判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_FUNCTION};
        if (register_analysis_preset(
                PRESET_METRIC_SPACE_TEST,
                "判定二元函数是否构成度量",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "d \\text{ 是度量} \\Leftrightarrow d(x,y) \\ge 0, d(x,y)=0 \\Leftrightarrow x=y, "
                "d(x,y)=d(y,x), d(x,z) \\le d(x,y)+d(y,z)",
                "O(|X|³)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 柯西序列判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FUNCTION};
        if (register_analysis_preset(
                PRESET_CAUCHY_SEQUENCE_TEST,
                "判定序列是否是柯西序列",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "\\{x_n\\} \\text{ 柯西} \\Leftrightarrow \\forall \\epsilon, \\exists N: "
                "m,n > N \\Rightarrow d(x_m, x_n) < \\epsilon",
                "O(n²)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 完备度量空间判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE};
        if (register_analysis_preset(
                PRESET_COMPLETE_SPACE_TEST,
                "判定度量空间是否完备",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "X \\text{ 完备} \\Leftrightarrow \\text{每个柯西序列收敛}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 压缩映射 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_CONTRACTION_MAPPING,
                "判定映射是否是压缩映射",
                inputs, 2, PRESET_TYPE_BOOLEAN,
                "T \\text{ 是压缩映射} \\Leftrightarrow d(Tx, Ty) \\le k \\cdot d(x,y), k < 1",
                "O(|X|²)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 不动点定理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_analysis_preset(
                PRESET_FIXED_POINT_THEOREM,
                "应用 Banach 不动点定理求不动点",
                inputs, 1, PRESET_TYPE_SET,
                "T \\text{ 压缩} \\Rightarrow \\exists! x^*: Tx^* = x^*",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第八部分：特殊函数
     * ============================================================ */

    /* -------------------- Γ 函数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_GAMMA_FUNCTION,
                "计算 Γ 函数值",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\Gamma(z) = \\int_0^{\\infty} t^{z-1} e^{-t} \\, dt",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- B 函数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_BETA_FUNCTION,
                "计算 B 函数值",
                inputs, 2, PRESET_TYPE_SCALAR,
                "B(a, b) = \\int_0^1 t^{a-1} (1-t)^{b-1} \\, dt = \\frac{\\Gamma(a)\\Gamma(b)}{\\Gamma(a+b)}",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- ζ 函数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_ZETA_FUNCTION,
                "计算黎曼 ζ 函数值",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\zeta(s) = \\sum_{n=1}^{\\infty} \\frac{1}{n^s}, \\quad \\text{Re}(s) > 1",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 误差函数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_ERROR_FUNCTION,
                "计算误差函数 erf(x)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\text{erf}(x) = \\frac{2}{\\sqrt{\\pi}} \\int_0^x e^{-t^2} \\, dt",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 方向导数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_VECTOR, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_DIRECTIONAL_DERIVATIVE,
                "计算函数在某点沿某方向的方向导数",
                inputs, 3, PRESET_TYPE_DERIVATIVE,
                "D_{\\mathbf{v}} f(a) = \\lim_{h \\to 0} \\frac{f(a + h\\mathbf{v}) - f(a)}{h} = \\nabla f(a) \\cdot \\mathbf{v}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 积分中值定理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_analysis_preset(
                PRESET_MEAN_VALUE_THEOREM,
                "应用积分中值定理求中值点",
                inputs, 3, PRESET_TYPE_SCALAR,
                "\\exists c \\in [a,b]: \\int_a^b f(x) \\, dx = f(c)(b-a)",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 无穷极限 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_analysis_preset(
                PRESET_INFINITE_LIMIT,
                "计算函数在无穷远处的极限",
                inputs, 1, PRESET_TYPE_LIMIT,
                "\\lim_{x \\to \\infty} f(x) = L",
                "O(∞)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == ANALYSIS_PRESET_COUNT;
}

/**
 * @brief 获取分析学预设函数块数量
 *
 * @return int 分析学模块预设函数块总数
 */
int preset_analysis_count(void)
{
    return ANALYSIS_PRESET_COUNT;
}

PresetCategory preset_analysis_category(void)
{
    return PRESET_CATEGORY_ANALYSIS;
}

bool preset_analysis_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    /* 分配名称数组 */
    char **names = (char**)lv00_malloc(ANALYSIS_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 极限运算 */
        PRESET_SEQUENCE_LIMIT,
        PRESET_FUNCTION_LIMIT,
        PRESET_LEFT_LIMIT,
        PRESET_RIGHT_LIMIT,
        PRESET_INFINITE_LIMIT,
        PRESET_LIMIT_SUPERIOR,
        PRESET_LIMIT_INFERIOR,
        PRESET_LIMIT_EXISTS_TEST,
        /* 连续性 */
        PRESET_CONTINUITY_TEST,
        PRESET_UNIFORM_CONTINUITY_TEST,
        PRESET_DISCONTINUITY_CLASSIFY,
        PRESET_LIPSCHITZ_TEST,
        /* 微分运算 */
        PRESET_DERIVATIVE,
        PRESET_HIGHER_DERIVATIVE,
        PRESET_PARTIAL_DERIVATIVE,
        PRESET_DIRECTIONAL_DERIVATIVE,
        PRESET_GRADIENT,
        PRESET_DIVERGENCE,
        PRESET_CURL,
        PRESET_LAPLACIAN,
        PRESET_DIFFERENTIABILITY_TEST,
        PRESET_TAYLOR_EXPANSION,
        /* 积分运算 */
        PRESET_INDEFINITE_INTEGRAL,
        PRESET_DEFINITE_INTEGRAL,
        PRESET_IMPROPER_INTEGRAL,
        PRESET_MULTIPLE_INTEGRAL,
        PRESET_LINE_INTEGRAL,
        PRESET_SURFACE_INTEGRAL,
        PRESET_INTEGRABILITY_TEST,
        PRESET_MEAN_VALUE_THEOREM,
        /* 级数运算 */
        PRESET_SERIES_CONVERGENCE_TEST,
        PRESET_ABSOLUTE_CONVERGENCE,
        PRESET_CONDITIONAL_CONVERGENCE,
        PRESET_POWER_SERIES_RADIUS,
        PRESET_SERIES_SUM,
        PRESET_FOURIER_SERIES,
        /* 函数空间 */
        PRESET_LP_NORM,
        PRESET_SUP_NORM,
        PRESET_COMPLETION,
        PRESET_COMPACTNESS_TEST,
        /* 度量空间 */
        PRESET_METRIC_SPACE_TEST,
        PRESET_CAUCHY_SEQUENCE_TEST,
        PRESET_COMPLETE_SPACE_TEST,
        PRESET_CONTRACTION_MAPPING,
        PRESET_FIXED_POINT_THEOREM,
        /* 特殊函数 */
        PRESET_GAMMA_FUNCTION,
        PRESET_BETA_FUNCTION,
        PRESET_ZETA_FUNCTION,
        PRESET_ERROR_FUNCTION,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) { void *tmp = names[j]; lv00_free(&tmp); }
            { void *tmp = names; lv00_free(&tmp); }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
