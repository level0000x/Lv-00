/**
 * @file preset_complex_analysis.c
 * @brief 复分析预设函数块模块 - 实现
 *
 * 实现理论数学研究中常用的复分析运算预设函数块。
 * 涵盖复变函数基础、复积分、级数展开、共形映射和特殊函数。
 *
 * 采用v2统一宏模式，使用 PRESET_CATEGORY_ANALYSIS 类别。
 *
 * @module ComplexAnalysis
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "preset_complex_analysis.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 复分析模块预设函数块总数 */
#define COMPLEX_ANALYSIS_PRESET_COUNT 35

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个复分析预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有复分析预设都属于 PRESET_CATEGORY_ANALYSIS 类别。
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
static bool register_complex_analysis_preset(
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

/**
 * @brief 简化预设注册的宏
 *
 * 减少重复代码，提高可维护性。
 * 注册成功时递增 success_count，失败时输出错误日志。
 */
#define REGISTER_CA(name, desc, inputs, in_count, output, math, comp, cons, rev) \
    do { \
        if (register_complex_analysis_preset( \
                (name), (desc), (inputs), (in_count), (output), \
                (math), (comp), (cons), (rev))) { \
            success_count++; \
        } else { \
            /* PRESET_ERROR_LOG("注册预设失败: %s", (name)); */ \
        } \
    } while (0)

/* ==================== 模块注册实现 ==================== */

bool preset_complex_analysis_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：复变函数基础 (10个)
     * ============================================================ */

    /* -------------------- 1. 复变函数求值 -------------------- */
    {
        /**
         * @brief 复变函数求值 f(z)
         *
         * 给定复变函数 f 和复数 z，计算 f(z) 的值。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z: 标量 (PRESET_TYPE_SCALAR) - 自变量
         * 输出：
         *   - f(z): 标量 (PRESET_TYPE_SCALAR) - 函数值
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "complex_function_eval",
            "复变函数求值：给定复变函数 f 和复数 z，计算 f(z)",
            inputs, 2, PRESET_TYPE_SCALAR,
            "w = f(z), \\quad z \\in \\mathbb{C}",
            "O(1)", true, false);
    }

    /* -------------------- 2. 复极限 -------------------- */
    {
        /**
         * @brief 复极限 lim(z->z0) f(z)
         *
         * 给定复变函数 f 和点 z0，计算复极限。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 极限点
         * 输出：
         *   - L: 标量 (PRESET_TYPE_SCALAR) - 极限值
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "complex_limit",
            "复极限：计算 lim(z->z0) f(z)，z 沿任意路径趋近 z0",
            inputs, 2, PRESET_TYPE_SCALAR,
            "L = \\lim_{z \\to z_0} f(z), \\quad z \\in \\mathbb{C}",
            "O(1)", true, false);
    }

    /* -------------------- 3. 复连续性判定 -------------------- */
    {
        /**
         * @brief 复连续性判定
         *
         * 判定复变函数 f 在点 z0 处是否连续。
         * 等价条件：lim(z->z0) f(z) = f(z0)。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 判定点
         * 输出：
         *   - is_continuous: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否连续
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "complex_continuity_check",
            "复连续性判定：f 在 z0 处连续当且仅当 lim(z->z0) f(z) = f(z0)",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "f \\text{ 在 } z_0 \\text{ 连续} \\Leftrightarrow "
            "\\lim_{z \\to z_0} f(z) = f(z_0)",
            "O(1)", true, false);
    }

    /* -------------------- 4. 复可微性判定 -------------------- */
    {
        /**
         * @brief 复可微性判定（Cauchy-Riemann条件）
         *
         * 判定复变函数 f = u + iv 在点 z0 处是否复可微。
         * 通过验证 Cauchy-Riemann 方程和偏导数连续性来判定。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 判定点
         * 输出：
         *   - is_differentiable: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否复可微
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "complex_differentiable_check",
            "复可微性判定：验证 Cauchy-Riemann 条件及偏导数连续性",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "f'(z_0) = \\lim_{\\Delta z \\to 0} "
            "\\frac{f(z_0 + \\Delta z) - f(z_0)}{\\Delta z} \\text{ 存在}",
            "O(1)", true, false);
    }

    /* -------------------- 5. 复导数 -------------------- */
    {
        /**
         * @brief 复导数 f'(z)
         *
         * 给定复变函数 f 和点 z0，计算复导数 f'(z0)。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 求导点
         * 输出：
         *   - f'(z0): 标量 (PRESET_TYPE_SCALAR) - 导数值
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "complex_derivative",
            "复导数：f'(z0) = lim(Dz->0) [f(z0+Dz) - f(z0)] / Dz",
            inputs, 2, PRESET_TYPE_SCALAR,
            "f'(z_0) = \\lim_{\\Delta z \\to 0} "
            "\\frac{f(z_0 + \\Delta z) - f(z_0)}{\\Delta z}",
            "O(1)", true, false);
    }

    /* -------------------- 6. Cauchy-Riemann方程验证 -------------------- */
    {
        /**
         * @brief Cauchy-Riemann方程验证
         *
         * 给定 f(z) = u(x,y) + iv(x,y)，验证在点 z0 处是否满足
         * Cauchy-Riemann 方程：ux = vy, uy = -vx。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 验证点
         * 输出：
         *   - satisfies: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否满足CR方程
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "cauchy_riemann_check",
            "Cauchy-Riemann方程验证：ux=vy, uy=-vx",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "\\frac{\\partial u}{\\partial x} = \\frac{\\partial v}{\\partial y}, "
            "\\quad \\frac{\\partial u}{\\partial y} = -\\frac{\\partial v}{\\partial x}",
            "O(1)", true, false);
    }

    /* -------------------- 7. 调和函数判定 -------------------- */
    {
        /**
         * @brief 调和函数判定
         *
         * 给定实值函数 u(x,y)，判定是否满足拉普拉斯方程
         * Delta(u) = u_xx + u_yy = 0。
         *
         * 输入：
         *   - u: 函数 (PRESET_TYPE_FUNCTION) - 实值函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 判定点
         * 输出：
         *   - is_harmonic: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否调和
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "harmonic_function_check",
            "调和函数判定：Delta(u) = u_xx + u_yy = 0",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "\\Delta u = \\frac{\\partial^2 u}{\\partial x^2} + "
            "\\frac{\\partial^2 u}{\\partial y^2} = 0",
            "O(1)", true, false);
    }

    /* -------------------- 8. 调和共轭 -------------------- */
    {
        /**
         * @brief 调和共轭
         *
         * 给定调和函数 u(x,y)，求其调和共轭 v(x,y)，
         * 使得 f(z) = u + iv 为解析函数。
         *
         * 输入：
         *   - u: 函数 (PRESET_TYPE_FUNCTION) - 调和函数
         * 输出：
         *   - v: 函数 (PRESET_TYPE_FUNCTION) - 调和共轭
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        REGISTER_CA(
            "harmonic_conjugate",
            "调和共轭：给定调和函数 u，求 v 使得 f = u + iv 解析",
            inputs, 1, PRESET_TYPE_FUNCTION,
            "f(z) = u(x,y) + iv(x,y) \\text{ 解析}",
            "O(1)", true, false);
    }

    /* -------------------- 9. 解析函数判定 -------------------- */
    {
        /**
         * @brief 解析函数判定
         *
         * 判定复变函数 f 在区域 Omega 内是否解析（全纯）。
         * f 在开集上每一点都复可微则称为解析函数。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 判定点
         * 输出：
         *   - is_analytic: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否解析
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "analytic_function_check",
            "解析函数判定：f 在 z0 的某邻域内处处复可微",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "f \\text{ 在 } z_0 \\text{ 解析} \\Leftrightarrow "
            "f \\text{ 在 } z_0 \\text{ 的某邻域内复可微}",
            "O(1)", true, false);
    }

    /* -------------------- 10. 整函数判定 -------------------- */
    {
        /**
         * @brief 整函数判定
         *
         * 判定函数 f(z) 是否在复平面 C 上处处解析。
         * 整函数的例子：多项式、指数函数、正弦函数等。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         * 输出：
         *   - is_entire: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为整函数
         *
         * 复杂度：O(inf)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        REGISTER_CA(
            "entire_function_check",
            "整函数判定：f(z) 在复平面 C 上处处解析",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "f \\text{ 是整函数} \\Leftrightarrow "
            "f \\text{ 在 } \\mathbb{C} \\text{ 上处处解析}",
            "O(\\infty)", true, false);
    }

    /* ============================================================
     * 第二部分：复积分 (8个)
     * ============================================================ */

    /* -------------------- 11. 复线积分 -------------------- */
    {
        /**
         * @brief 复线积分 int_gamma f(z) dz
         *
         * 给定复变函数 f 和有向曲线 gamma，计算复线积分。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 被积函数
         *   - gamma: 路径 (PRESET_TYPE_PATH) - 积分路径
         * 输出：
         *   - integral: 标量 (PRESET_TYPE_SCALAR) - 积分值
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_PATH};
        REGISTER_CA(
            "complex_line_integral",
            "复线积分：int_gamma f(z) dz，沿有向曲线 gamma 的复积分",
            inputs, 2, PRESET_TYPE_SCALAR,
            "\\int_{\\gamma} f(z) \\, dz = \\int_a^b f(\\gamma(t)) \\gamma'(t) \\, dt",
            "O(n)", true, false);
    }

    /* -------------------- 12. Cauchy积分公式 -------------------- */
    {
        /**
         * @brief Cauchy积分公式
         *
         * 若 f 在简单闭曲线 gamma 及其内部解析，z0 在 gamma 内部，则
         * f(z0) = (1/2*pi*i) * oint_gamma f(z)/(z-z0) dz。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 解析函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 内部点
         *   - gamma: 路径 (PRESET_TYPE_PATH) - 简单闭曲线
         * 输出：
         *   - f(z0): 标量 (PRESET_TYPE_SCALAR) - 函数值
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR,
                               PRESET_TYPE_PATH};
        REGISTER_CA(
            "cauchy_integral_formula",
            "Cauchy积分公式：f(z0) = (1/2*pi*i) oint f(z)/(z-z0) dz",
            inputs, 3, PRESET_TYPE_SCALAR,
            "f(z_0) = \\frac{1}{2\\pi i} \\oint_{\\gamma} "
            "\\frac{f(z)}{z - z_0} \\, dz",
            "O(n)", true, false);
    }

    /* -------------------- 13. Cauchy积分公式求导 -------------------- */
    {
        /**
         * @brief Cauchy积分公式求导
         *
         * Cauchy积分公式的推广形式，用于计算解析函数的各阶导数：
         * f^(n)(z0) = n!/(2*pi*i) * oint_gamma f(z)/(z-z0)^(n+1) dz。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 解析函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 内部点
         *   - gamma: 路径 (PRESET_TYPE_PATH) - 简单闭曲线
         *   - n: 整数 (PRESET_TYPE_INTEGER) - 导数阶数
         * 输出：
         *   - f^(n)(z0): 标量 (PRESET_TYPE_SCALAR) - n阶导数值
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR,
                               PRESET_TYPE_PATH, PRESET_TYPE_INTEGER};
        REGISTER_CA(
            "cauchy_integral_derivative",
            "Cauchy积分公式求导：f^(n)(z0) = n!/(2*pi*i) oint f(z)/(z-z0)^(n+1) dz",
            inputs, 4, PRESET_TYPE_SCALAR,
            "f^{(n)}(z_0) = \\frac{n!}{2\\pi i} \\oint_{\\gamma} "
            "\\frac{f(z)}{(z - z_0)^{n+1}} \\, dz",
            "O(n)", true, false);
    }

    /* -------------------- 14. Cauchy定理验证 -------------------- */
    {
        /**
         * @brief Cauchy定理验证
         *
         * 验证若 f 在简单闭曲线 gamma 及其内部解析，
         * 则 oint_gamma f(z) dz = 0。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - gamma: 路径 (PRESET_TYPE_PATH) - 简单闭曲线
         * 输出：
         *   - is_zero: 布尔值 (PRESET_TYPE_BOOLEAN) - 积分是否为零
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_PATH};
        REGISTER_CA(
            "cauchy_theorem",
            "Cauchy定理验证：f 在 gamma 及其内部解析 => oint_gamma f(z) dz = 0",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "\\oint_{\\gamma} f(z) \\, dz = 0",
            "O(n)", true, false);
    }

    /* -------------------- 15. Morera定理验证 -------------------- */
    {
        /**
         * @brief Morera定理验证
         *
         * Morera定理是Cauchy定理的逆定理：若 f 在区域 Omega 内连续，
         * 且对 Omega 内任意三角形的边界 gamma 都有 oint_gamma f(z) dz = 0，
         * 则 f 在 Omega 内解析。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - region: 区域 (PRESET_TYPE_REGION) - 判定区域
         * 输出：
         *   - is_analytic: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否解析
         *
         * 复杂度：O(inf)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_REGION};
        REGISTER_CA(
            "morera_theorem",
            "Morera定理验证：f 连续且沿任意三角形积分为零 => f 解析",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "\\oint_{\\Delta} f(z) \\, dz = 0, \\forall \\Delta \\subset \\Omega "
            "\\Rightarrow f \\text{ 在 } \\Omega \\text{ 内解析}",
            "O(\\infty)", true, false);
    }

    /* -------------------- 16. 绕数 -------------------- */
    {
        /**
         * @brief 绕数（环绕数）
         *
         * 给定闭曲线 gamma 和点 z0（z0 不在 gamma 上），
         * 计算绕数 n(gamma, z0) = (1/2*pi*i) oint_gamma dz/(z-z0)。
         *
         * 输入：
         *   - gamma: 路径 (PRESET_TYPE_PATH) - 闭曲线
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 参考点
         * 输出：
         *   - n: 整数 (PRESET_TYPE_INTEGER) - 绕数
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_PATH, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "winding_number",
            "绕数：n(gamma, z0) = (1/2*pi*i) oint_gamma dz/(z-z0)",
            inputs, 2, PRESET_TYPE_INTEGER,
            "n(\\gamma, z_0) = \\frac{1}{2\\pi i} "
            "\\oint_{\\gamma} \\frac{dz}{z - z_0}",
            "O(n)", true, false);
    }

    /* -------------------- 17. 留数积分 -------------------- */
    {
        /**
         * @brief 留数积分
         *
         * 利用留数定理计算围道积分：
         * oint_gamma f(z) dz = 2*pi*i * sum Res(f, zk)，
         * 其中 zk 为 gamma 内部的所有孤立奇点。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 亚纯函数
         *   - singularities: 元组 (PRESET_TYPE_TUPLE) - 奇点列表
         *   - gamma: 路径 (PRESET_TYPE_PATH) - 积分围道
         * 输出：
         *   - integral: 标量 (PRESET_TYPE_SCALAR) - 积分值
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_TUPLE,
                               PRESET_TYPE_PATH};
        REGISTER_CA(
            "residue_integral",
            "留数积分：oint_gamma f(z) dz = 2*pi*i * sum Res(f, zk)",
            inputs, 3, PRESET_TYPE_SCALAR,
            "\\oint_{\\gamma} f(z) \\, dz = "
            "2\\pi i \\sum_{k} \\text{Res}(f, z_k)",
            "O(n)", true, false);
    }

    /* -------------------- 18. 围道积分参数化 -------------------- */
    {
        /**
         * @brief 围道积分参数化
         *
         * 将复线积分转化为关于参数 t 的实积分：
         * int_gamma f(z) dz = int_a^b f(gamma(t)) * gamma'(t) dt。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 被积函数
         *   - gamma: 路径 (PRESET_TYPE_PATH) - 参数化路径 gamma(t)
         * 输出：
         *   - param_integral: 表达式 (PRESET_TYPE_EXPRESSION) - 参数化后的实积分
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_PATH};
        REGISTER_CA(
            "contour_integral_param",
            "围道积分参数化：int_gamma f(z) dz = int_a^b f(gamma(t)) gamma'(t) dt",
            inputs, 2, PRESET_TYPE_EXPRESSION,
            "\\int_{\\gamma} f(z) \\, dz = "
            "\\int_a^b f(\\gamma(t)) \\, \\gamma'(t) \\, dt",
            "O(n)", true, false);
    }

    /* ============================================================
     * 第三部分：级数展开 (8个)
     * ============================================================ */

    /* -------------------- 19. 复Taylor级数 -------------------- */
    {
        /**
         * @brief 复Taylor级数
         *
         * 给定在 z0 处解析的函数 f(z)，展开为Taylor级数：
         * f(z) = sum_{n=0}^{inf} f^(n)(z0)/n! * (z-z0)^n。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 解析函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 展开中心
         *   - n: 整数 (PRESET_TYPE_INTEGER) - 展开阶数
         * 输出：
         *   - coefficients: 元组 (PRESET_TYPE_TUPLE) - Taylor级数系数
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR,
                               PRESET_TYPE_INTEGER};
        REGISTER_CA(
            "taylor_series_complex",
            "复Taylor级数：f(z) = sum f^(n)(z0)/n! * (z-z0)^n",
            inputs, 3, PRESET_TYPE_TUPLE,
            "f(z) = \\sum_{n=0}^{\\infty} "
            "\\frac{f^{(n)}(z_0)}{n!} (z - z_0)^n",
            "O(n)", true, false);
    }

    /* -------------------- 20. Laurent级数 -------------------- */
    {
        /**
         * @brief Laurent级数
         *
         * 给定函数 f(z) 和孤立奇点 z0，在环形区域 0 < |z-z0| < R 内展开为
         * f(z) = sum_{n=-inf}^{inf} a_n * (z-z0)^n。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 展开中心
         * 输出：
         *   - coefficients: 元组 (PRESET_TYPE_TUPLE) - Laurent级数系数
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "laurent_series",
            "Laurent级数：f(z) = sum a_n (z-z0)^n，n 从 -inf 到 +inf",
            inputs, 2, PRESET_TYPE_TUPLE,
            "f(z) = \\sum_{n=-\\infty}^{\\infty} a_n (z - z_0)^n, "
            "\\quad 0 < |z - z_0| < R",
            "O(n)", true, false);
    }

    /* -------------------- 21. 幂级数收敛半径 -------------------- */
    {
        /**
         * @brief 幂级数收敛半径
         *
         * 给定幂级数 sum a_n (z-z0)^n，计算收敛半径 R。
         * 使用 Hadamard 公式：1/R = limsup |a_n|^(1/n)。
         *
         * 输入：
         *   - coefficients: 元组 (PRESET_TYPE_TUPLE) - 系数序列
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 展开中心
         * 输出：
         *   - R: 标量 (PRESET_TYPE_SCALAR) - 收敛半径
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_TUPLE, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "power_series_radius",
            "幂级数收敛半径：1/R = limsup |a_n|^(1/n)（Hadamard公式）",
            inputs, 2, PRESET_TYPE_SCALAR,
            "\\frac{1}{R} = \\limsup_{n \\to \\infty} |a_n|^{1/n}",
            "O(n)", true, false);
    }

    /* -------------------- 22. Laurent级数主部 -------------------- */
    {
        /**
         * @brief Laurent级数主部
         *
         * 提取Laurent级数的负幂次部分（主部）：
         * PP(f, z0) = sum_{n=1}^{inf} a_{-n} (z-z0)^{-n}。
         * 主部决定奇点的类型。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 展开中心
         * 输出：
         *   - principal_part: 表达式 (PRESET_TYPE_EXPRESSION) - 主部表达式
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "laurent_series_principal_part",
            "Laurent级数主部：提取负幂次部分 sum a_{-n} (z-z0)^{-n}",
            inputs, 2, PRESET_TYPE_EXPRESSION,
            "\\text{PP}(f, z_0) = \\sum_{n=1}^{\\infty} "
            "a_{-n} (z - z_0)^{-n}",
            "O(n)", true, false);
    }

    /* -------------------- 23. 留数计算 -------------------- */
    {
        /**
         * @brief 留数计算
         *
         * 给定函数 f(z) 和孤立奇点 z0，计算留数 Res(f, z0)。
         * 留数等于Laurent级数中 (z-z0)^{-1} 项的系数 a_{-1}。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 奇点
         * 输出：
         *   - Res(f, z0): 标量 (PRESET_TYPE_SCALAR) - 留数值
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "residue_compute",
            "留数计算：Res(f, z0) = a_{-1}（Laurent级数 (z-z0)^{-1} 项系数）",
            inputs, 2, PRESET_TYPE_SCALAR,
            "\\text{Res}(f, z_0) = a_{-1} = "
            "\\frac{1}{2\\pi i} \\oint_{\\gamma} f(z) \\, dz",
            "O(1)", true, false);
    }

    /* -------------------- 24. 极点阶数判定 -------------------- */
    {
        /**
         * @brief 极点阶数判定
         *
         * 判定函数 f 在孤立奇点 z0 处的极点阶数 m。
         * m阶极点：Laurent级数主部最高负幂次为 m。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 奇点
         * 输出：
         *   - order: 整数 (PRESET_TYPE_INTEGER) - 极点阶数（0表示非极点）
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "pole_order",
            "极点阶数判定：确定孤立奇点 z0 处极点的阶数 m",
            inputs, 2, PRESET_TYPE_INTEGER,
            "\\lim_{z \\to z_0} (z - z_0)^m f(z) \\neq 0, "
            "\\lim_{z \\to z_0} (z - z_0)^{m+1} f(z) = 0",
            "O(1)", true, false);
    }

    /* -------------------- 25. 本性奇点判定 -------------------- */
    {
        /**
         * @brief 本性奇点判定
         *
         * 判定函数 f 在孤立奇点 z0 处是否为本性奇点。
         * 本性奇点：Laurent级数主部有无穷多项。
         * 等价判定（Casorati-Weierstrass）：f 在 z0 附近的像在 C 中稠密。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 奇点
         * 输出：
         *   - is_essential: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为本性奇点
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "essential_singular_check",
            "本性奇点判定：Laurent级数主部有无穷多项（非极点、非可去）",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "\\text{本性奇点} \\Leftrightarrow "
            "\\text{Laurent级数主部有无穷多项}",
            "O(1)", true, false);
    }

    /* -------------------- 26. 可去奇点判定 -------------------- */
    {
        /**
         * @brief 可去奇点判定
         *
         * 判定函数 f 在孤立奇点 z0 处是否为可去奇点。
         * 可去奇点：lim(z->z0) f(z) 存在且有限。
         * 等价条件：Laurent级数主部为零。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 奇点
         * 输出：
         *   - is_removable: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为可去奇点
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "removable_singular_check",
            "可去奇点判定：lim(z->z0) f(z) 存在且有限",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "\\lim_{z \\to z_0} f(z) \\text{ 存在且有限} "
            "\\Leftrightarrow \\text{Laurent级数主部为零}",
            "O(1)", true, false);
    }

    /* ============================================================
     * 第四部分：共形映射 (5个)
     * ============================================================ */

    /* -------------------- 27. 共形映射判定 -------------------- */
    {
        /**
         * @brief 共形映射判定
         *
         * 判定解析函数 f 在点 z0 处是否为共形映射。
         * 条件：f'(z0) != 0（保持角度和定向）。
         *
         * 输入：
         *   - f: 函数 (PRESET_TYPE_FUNCTION) - 复变函数
         *   - z0: 标量 (PRESET_TYPE_SCALAR) - 判定点
         * 输出：
         *   - is_conformal: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否为共形映射
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "conformal_map_check",
            "共形映射判定：f'(z0) != 0 时 f 在 z0 处保持角度和定向",
            inputs, 2, PRESET_TYPE_BOOLEAN,
            "f \\text{ 在 } z_0 \\text{ 共形} \\Leftrightarrow "
            "f'(z_0) \\neq 0",
            "O(1)", true, false);
    }

    /* -------------------- 28. Mobius变换 -------------------- */
    {
        /**
         * @brief Mobius变换
         *
         * 给定参数 a, b, c, d（ad - bc != 0），计算 Mobius 变换
         * T(z) = (az + b) / (cz + d)。
         *
         * 输入：
         *   - z: 标量 (PRESET_TYPE_SCALAR) - 自变量
         *   - a, b, c, d: 标量 (PRESET_TYPE_SCALAR) - 变换参数
         * 输出：
         *   - T(z): 标量 (PRESET_TYPE_SCALAR) - 变换结果
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR,
                               PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR,
                               PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "mobius_transform",
            "Mobius变换：T(z) = (az + b) / (cz + d)，ad - bc != 0",
            inputs, 5, PRESET_TYPE_SCALAR,
            "T(z) = \\frac{az + b}{cz + d}, \\quad ad - bc \\neq 0",
            "O(1)", true, true);
    }

    /* -------------------- 29. Mobius变换复合 -------------------- */
    {
        /**
         * @brief Mobius变换复合
         *
         * 给定两个Mobius变换 T1(z) = (a1*z+b1)/(c1*z+d1) 和
         * T2(z) = (a2*z+b2)/(c2*z+d2)，计算复合变换 T1 o T2。
         *
         * 输入：
         *   - T1: 元组 (PRESET_TYPE_TUPLE) - 第一个Mobius变换参数 (a1,b1,c1,d1)
         *   - T2: 元组 (PRESET_TYPE_TUPLE) - 第二个Mobius变换参数 (a2,b2,c2,d2)
         * 输出：
         *   - T1_o_T2: 元组 (PRESET_TYPE_TUPLE) - 复合变换参数
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_TUPLE, PRESET_TYPE_TUPLE};
        REGISTER_CA(
            "mobius_compose",
            "Mobius变换复合：计算 T1 o T2 的参数",
            inputs, 2, PRESET_TYPE_TUPLE,
            "T_1 \\circ T_2(z) = T_1(T_2(z))",
            "O(1)", true, false);
    }

    /* -------------------- 30. Mobius变换逆 -------------------- */
    {
        /**
         * @brief Mobius变换逆
         *
         * 给定Mobius变换 T(z) = (az+b)/(cz+d)，计算其逆变换。
         * T^{-1}(w) = (dw - b) / (-cw + a)。
         *
         * 输入：
         *   - T: 元组 (PRESET_TYPE_TUPLE) - Mobius变换参数 (a,b,c,d)
         * 输出：
         *   - T_inv: 元组 (PRESET_TYPE_TUPLE) - 逆变换参数
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_TUPLE};
        REGISTER_CA(
            "mobius_inverse",
            "Mobius变换逆：T^{-1}(w) = (dw - b) / (-cw + a)",
            inputs, 1, PRESET_TYPE_TUPLE,
            "T^{-1}(w) = \\frac{dw - b}{-cw + a}",
            "O(1)", true, true);
    }

    /* -------------------- 31. Riemann映射 -------------------- */
    {
        /**
         * @brief Riemann映射定理
         *
         * Riemann映射定理断言：对复平面上任意单连通开区域 Omega（非空且非全平面），
         * 存在共形映射 f: Omega -> 单位圆盘 D。
         * 此预设验证给定区域是否满足Riemann映射定理的条件。
         *
         * 输入：
         *   - region: 区域 (PRESET_TYPE_REGION) - 待映射区域
         * 输出：
         *   - exists: 布尔值 (PRESET_TYPE_BOOLEAN) - 是否存在Riemann映射
         *
         * 复杂度：O(inf)
         */
        PresetType inputs[] = {PRESET_TYPE_REGION};
        REGISTER_CA(
            "riemann_mapping",
            "Riemann映射定理：验证单连通区域到单位圆盘的共形映射存在性",
            inputs, 1, PRESET_TYPE_BOOLEAN,
            "\\exists f: \\Omega \\to \\mathbb{D} \\text{ 共形双射}, "
            "\\Omega \\text{ 单连通开集}",
            "O(\\infty)", false, false);
    }

    /* ============================================================
     * 第五部分：特殊函数 (4个)
     * ============================================================ */

    /* -------------------- 32. 复指数函数 -------------------- */
    {
        /**
         * @brief 复指数函数 e^z
         *
         * 给定复数 z = x + iy，计算 e^z = e^x * (cos y + i*sin y)。
         * Euler公式：e^(iy) = cos y + i*sin y。
         *
         * 输入：
         *   - z: 标量 (PRESET_TYPE_SCALAR) - 复数
         * 输出：
         *   - e^z: 标量 (PRESET_TYPE_SCALAR) - 指数值
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "complex_exp",
            "复指数函数：e^z = e^x (cos y + i sin y)，z = x + iy",
            inputs, 1, PRESET_TYPE_SCALAR,
            "e^z = e^x (\\cos y + i\\sin y), \\quad z = x + iy",
            "O(1)", true, false);
    }

    /* -------------------- 33. 复对数函数 -------------------- */
    {
        /**
         * @brief 复对数函数 Log(z)
         *
         * 给定非零复数 z = r*e^(i*theta)，计算主值分支
         * Log(z) = ln(r) + i*Arg(z)，Arg(z) in (-pi, pi]。
         *
         * 输入：
         *   - z: 标量 (PRESET_TYPE_SCALAR) - 非零复数
         * 输出：
         *   - Log(z): 标量 (PRESET_TYPE_SCALAR) - 对数主值
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "complex_log",
            "复对数函数：Log(z) = ln|z| + i Arg(z)，Arg(z) in (-pi, pi]",
            inputs, 1, PRESET_TYPE_SCALAR,
            "\\text{Log}(z) = \\ln r + i\\Theta, \\quad "
            "\\Theta \\in (-\\pi, \\pi]",
            "O(1)", true, false);
    }

    /* -------------------- 34. 复三角函数 -------------------- */
    {
        /**
         * @brief 复三角函数
         *
         * 给定复数 z = x + iy，计算复三角函数：
         * sin(z), cos(z), tan(z) 等。
         * sin(z) = (e^(iz) - e^(-iz)) / (2i)
         * cos(z) = (e^(iz) + e^(-iz)) / 2
         *
         * 输入：
         *   - z: 标量 (PRESET_TYPE_SCALAR) - 复数
         *   - func: 标量 (PRESET_TYPE_SCALAR) - 函数类型标识（sin/cos/tan等）
         * 输出：
         *   - result: 标量 (PRESET_TYPE_SCALAR) - 三角函数值
         *
         * 复杂度：O(1)
         */
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "complex_trig",
            "复三角函数：sin(z), cos(z), tan(z) 等，z in C",
            inputs, 2, PRESET_TYPE_SCALAR,
            "\\sin z = \\frac{e^{iz} - e^{-iz}}{2i}, \\quad "
            "\\cos z = \\frac{e^{iz} + e^{-iz}}{2}",
            "O(1)", true, false);
    }

    /* -------------------- 35. Gamma函数 -------------------- */
    {
        /**
         * @brief Gamma函数 Gamma(z)
         *
         * 给定复数 z（Re(z) > 0），计算Gamma函数。
         * Gamma(z) = int_0^{inf} t^{z-1} e^{-t} dt。
         * 满足递推关系：Gamma(z+1) = z * Gamma(z)，Gamma(1) = 1。
         *
         * 输入：
         *   - z: 标量 (PRESET_TYPE_SCALAR) - 复数（Re(z) > 0）
         * 输出：
         *   - Gamma(z): 标量 (PRESET_TYPE_SCALAR) - Gamma函数值
         *
         * 复杂度：O(n)
         */
        PresetType inputs[] = {PRESET_TYPE_SCALAR};
        REGISTER_CA(
            "gamma_function",
            "Gamma函数：Gamma(z) = int_0^inf t^{z-1} e^{-t} dt，Re(z) > 0",
            inputs, 1, PRESET_TYPE_SCALAR,
            "\\Gamma(z) = \\int_0^{\\infty} t^{z-1} e^{-t} \\, dt, "
            "\\quad \\text{Re}(z) > 0",
            "O(n)", true, false);
    }

    /* 返回是否所有预设都注册成功 */
    /* lv00_log_info("复分析预设注册完成，共 %d 个预设", success_count) */
    return success_count == COMPLEX_ANALYSIS_PRESET_COUNT;
}

/**
 * @brief 获取复分析预设函数块数量
 */
int preset_complex_analysis_count(void)
{
    return COMPLEX_ANALYSIS_PRESET_COUNT;
}

/**
 * @brief 获取复分析预设名称列表
 */
bool preset_complex_analysis_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    char **names = (char **)lv00_malloc(COMPLEX_ANALYSIS_PRESET_COUNT * sizeof(char *));
    if (!names) return false;

    const char *preset_names[] = {
        "complex_function_eval",
        "complex_limit",
        "complex_continuity_check",
        "complex_differentiable_check",
        "complex_derivative",
        "cauchy_riemann_check",
        "harmonic_function_check",
        "harmonic_conjugate",
        "analytic_function_check",
        "entire_function_check",
        "complex_line_integral",
        "cauchy_integral_formula",
        "cauchy_integral_derivative",
        "cauchy_theorem",
        "morera_theorem",
        "winding_number",
        "residue_integral",
        "contour_integral_param",
        "taylor_series_complex",
        "laurent_series",
        "power_series_radius",
        "laurent_series_principal_part",
        "residue_compute",
        "pole_order",
        "essential_singular_check",
        "removable_singular_check",
        "conformal_map_check",
        "mobius_transform",
        "mobius_compose",
        "mobius_inverse",
        "riemann_mapping",
        "complex_exp",
        "complex_log",
        "complex_trig",
        "gamma_function",
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

PresetCategory preset_complex_analysis_category(void)
{
    return PRESET_CATEGORY_ANALYSIS;
}
