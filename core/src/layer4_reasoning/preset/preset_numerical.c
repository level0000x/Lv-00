/**
 * @file preset_numerical.c
 * @brief 数值分析预设函数块 - 实现
 *
 * H1 评估结论（2026-08-07）：注册样板已由 LV_PRESET_REGISTER 宏抽象（每条目 1 行），无需代码生成；运行时注册由 module/presets/numerical.lvz 数据驱动（convert_presets.py 生成）。
 *
 * 实现理论数学研究中常用的数值分析预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module Numerical
 * @category PRESET_CATEGORY_NUMERICAL
 * @version 4.0.0
 */

#include "preset_numerical.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 数值分析模块预设函数块总数：24（与头文件中 NUMERICAL_PRESET_COUNT 一致） */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个数值分析预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有数值分析预设都属于 PRESET_CATEGORY_NUMERICAL 类别。
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
LV_DECLARE_PRESET_REGISTER(PRESET_CATEGORY_NUMERICAL)

/* ==================== 模块注册实现 ==================== */

bool preset_numerical_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：方程求根
     * ============================================================ */

    /* -------------------- 二分法 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_BISECTION, "二分法求方程 f(x)=0 的根，要求 f 在区间 [a,b] 上连续且变号", 3,
            PRESET_TYPE_SCALAR, "x_{n+1} = \\frac{a_n + b_n}{2}, \\quad f(a_n) \\cdot f(b_n) < 0",
            "O(n \\log \\frac{b-a}{\\varepsilon})", true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR);

    /* -------------------- 牛顿法 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NUMERICAL_NEWTON, "牛顿法（切线法）求方程 f(x)=0 的根，需要函数及其导数",
            3, PRESET_TYPE_SCALAR, "x_{n+1} = x_n - \\frac{f(x_n)}{f'(x_n)}",
            "O(n \\log \\frac{1}{\\varepsilon})", true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR);

    /* -------------------- 割线法 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NUMERICAL_SECANT, "割线法求方程 f(x)=0 的根，用差商代替导数", 3,
            PRESET_TYPE_SCALAR,
            "x_{n+1} = x_n - f(x_n) \\cdot \\frac{x_n - x_{n-1}}{f(x_n) - f(x_{n-1})}",
            "O(n \\log \\frac{1}{\\varepsilon})", true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR);

    /* -------------------- 不动点迭代 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NUMERICAL_FIXED_POINT,
            "不动点迭代法：将 f(x)=0 转化为 x=g(x) 进行迭代求解", 2,
            PRESET_TYPE_SCALAR, "x_{n+1} = g(x_n), \\quad x^* = g(x^*)",
            "O(n \\log \\frac{1}{\\varepsilon})", true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR);

    /* ============================================================
     * 第二部分：数值积分
     * ============================================================ */

    /* -------------------- 梯形公式 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_TRAPEZOID, "复化梯形公式求定积分 ∫[a,b] f(x)dx", 4, PRESET_TYPE_SCALAR,
            "T_n = \\frac{h}{2}\\left[f(a) + 2\\sum_{i=1}^{n-1} f(x_i) + f(b)\\right], \\quad h = \\frac{b-a}{n}",
            "O(n)", true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER);

    /* -------------------- Simpson公式 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NUMERICAL_SIMPSON, "复化Simpson公式求定积分 ∫[a,b] f(x)dx", 4,
            PRESET_TYPE_SCALAR,
            "S_n = \\frac{h}{3}\\left[f(a) + 4\\sum_{\\text{odd}} f(x_i) + "
            "2\\sum_{\\text{even}} f(x_i) + f(b)\\right]",
            "O(n)", true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER);

    /* -------------------- 高斯求积 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NUMERICAL_GAUSS_QUADRATURE,
            "高斯求积公式：利用正交多项式的零点作为求积节点", 4, PRESET_TYPE_SCALAR,
            "\\int_a^b f(x)\\,dx \\approx \\sum_{i=1}^{n} A_i \\, f(x_i), \\quad x_i \\text{ "
            "为 Legendre 多项式零点}",
            "O(n)", true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER);

    /* -------------------- Romberg积分 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_ROMBERG, "Romberg积分：基于Richardson外推的逐次加速积分方法", 4,
            PRESET_TYPE_SCALAR, "R_{k,m} = \\frac{4^m R_{k,m-1} - R_{k-1,m-1}}{4^m - 1}", "O(n^2)", true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER);

    /* ============================================================
     * 第三部分：数值微分
     * ============================================================ */

    /* -------------------- 前向差分 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NUMERICAL_FORWARD_DIFF, "前向差分公式近似计算函数在 x 处的一阶导数",
            3, PRESET_TYPE_SCALAR,
            "f'(x) \\approx \\frac{f(x+h) - f(x)}{h}, \\quad \\text{误差 } O(h)", "O(1)",
            true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR);

    /* -------------------- 中心差分 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NUMERICAL_CENTRAL_DIFF, "中心差分公式近似计算函数在 x 处的一阶导数",
            3, PRESET_TYPE_SCALAR,
            "f'(x) \\approx \\frac{f(x+h) - f(x-h)}{2h}, \\quad \\text{误差 } O(h^2)", "O(1)",
            true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR);

    /* -------------------- Richardson外推 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_RICHARDSON, "Richardson外推法：通过逐步减半步长并外推消除低阶误差项", 3,
            PRESET_TYPE_SCALAR, "D_{k,m} = \\frac{4^m D_{k,m-1} - D_{k-1,m-1}}{4^m - 1}", "O(n^2)", true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER);

    /* ============================================================
     * 第四部分：插值
     * ============================================================ */

    /* -------------------- Lagrange插值 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_LAGRANGE_INTERP, "Lagrange插值：给定 n+1 个数据点构造 Lagrange 插值多项式并求值",
            3, PRESET_TYPE_SCALAR,
            "L_n(x) = \\sum_{i=0}^{n} y_i \\prod_{\\substack{j=0 \\\\ j \\neq i}}^{n} \\frac{x - x_j}{x_i - x_j}",
            "O(n^2)", true, false,
            PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR);

    /* -------------------- Newton插值 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_NEWTON_INTERP, "Newton差商插值：利用差商表构造 Newton 插值多项式并求值", 3,
            PRESET_TYPE_SCALAR,
            "N_n(x) = f[x_0] + \\sum_{i=1}^{n} f[x_0, \\ldots, x_i] \\prod_{j=0}^{i-1} (x - x_j)", "O(n^2)", true,
            false,
            PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR);

    /* -------------------- 样条插值 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_SPLINE_INTERP, "三次样条插值：构造分段三次多项式，保证节点处二阶连续", 3,
            PRESET_TYPE_SCALAR,
            "S_i(x) = a_i + b_i(x-x_i) + c_i(x-x_i)^2 + d_i(x-x_i)^3, \\quad x \\in [x_i, x_{i+1}]", "O(n)", true,
            false,
            PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR);

    /* ============================================================
     * 第五部分：拟合
     * ============================================================ */

    /* -------------------- 最小二乘法 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_LEAST_SQUARES, "最小二乘拟合：给定数据点和基函数族，求最小二乘逼近", 3,
            PRESET_TYPE_LIST,
            "\\min_{\\{c_i\\}} \\sum_{j=1}^{m} \\left| y_j - \\sum_{i=1}^{n} c_i \\, \\varphi_i(x_j) \\right|^2",
            "O(mn^2)", true, false,
            PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_FUNCTION);

    /* -------------------- 多项式拟合 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_POLYNOMIAL_FIT, "多项式拟合：用最小二乘法拟合 n 次多项式", 3, PRESET_TYPE_LIST,
            "\\min_{\\{a_k\\}} \\sum_{j=1}^{m} \\left| y_j - \\sum_{k=0}^{n} a_k x_j^k \\right|^2", "O(mn^2)", true,
            false,
            PRESET_TYPE_LIST, PRESET_TYPE_LIST, PRESET_TYPE_INTEGER);

    /* ============================================================
     * 第六部分：常微分方程
     * ============================================================ */

    /* -------------------- Euler方法 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_EULER, "显式Euler方法求解初值问题 y'=f(x,y), y(x0)=y0", 5, PRESET_TYPE_LIST,
            "y_{n+1} = y_n + h \\, f(x_n, y_n), \\quad h = \\frac{x_{end} - x_0}{N}", "O(N)", true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR,
            PRESET_TYPE_INTEGER);

    /* -------------------- 四阶Runge-Kutta方法 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NUMERICAL_RK4, "经典四阶Runge-Kutta方法求解初值问题 y'=f(x,y), y(x0)=y0",
            5, PRESET_TYPE_LIST,
            "y_{n+1} = y_n + \\frac{h}{6}(k_1 + 2k_2 + 2k_3 + k_4)", "O(N)", true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR,
            PRESET_TYPE_INTEGER);

    /* -------------------- Adams多步方法 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NUMERICAL_ADAMS, "Adams-Bashforth四步显式方法求解初值问题 y'=f(x,y)",
            5, PRESET_TYPE_LIST,
            "y_{n+1} = y_n + \\frac{h}{24}(55f_n - 59f_{n-1} + 37f_{n-2} - 9f_{n-3})", "O(N)",
            true, false,
            PRESET_TYPE_FUNCTION, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR,
            PRESET_TYPE_INTEGER);

    /* ============================================================
     * 第七部分：线性方程组
     * ============================================================ */

    /* -------------------- 高斯消元法 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NUMERICAL_GAUSS_ELIMINATION, "高斯消元法（列主元）求解线性方程组 Ax=b",
            2, PRESET_TYPE_LIST,
            "Ax = b \\Rightarrow Ux = c \\Rightarrow x = U^{-1}c", "O(n^3)", true, false,
            PRESET_TYPE_MATRIX, PRESET_TYPE_LIST);

    /* -------------------- LU分解 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_LU_DECOMPOSITION, "LU分解：将矩阵 A 分解为下三角矩阵 L 和上三角矩阵 U 的乘积", 1,
            PRESET_TYPE_TUPLE, "A = LU, \\quad L \\text{ 为单位下三角矩阵}, \\quad U \\text{ 为上三角矩阵}",
            "O(n^3)", true, false,
            PRESET_TYPE_MATRIX);

    /* -------------------- 迭代求解法 -------------------- */
    LV_PRESET_REGISTER(success_count, PRESET_NUMERICAL_ITERATIVE_SOLVE,
            "迭代法（Jacobi/Gauss-Seidel）求解线性方程组 Ax=b", 4, PRESET_TYPE_LIST,
            "x^{(k+1)} = D^{-1}(b - (L+U)x^{(k)})", /* Jacobi迭代 */
            "O(n^2 \\cdot k)", true, false,
            PRESET_TYPE_MATRIX, PRESET_TYPE_LIST, PRESET_TYPE_SCALAR, PRESET_TYPE_INTEGER);

    /* ============================================================
     * 第八部分：矩阵特征值
     * ============================================================ */

    /* -------------------- 特征值计算 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_EIGENVALUES, "QR算法计算矩阵的全部特征值", 1, PRESET_TYPE_LIST,
            "Ax = \\lambda x, \\quad A = QR \\Rightarrow A' = RQ, \\quad \\text{迭代收敛至上三角矩阵}",
            "O(n^3 \\cdot k)", true, false,
            PRESET_TYPE_MATRIX);

    /* -------------------- 奇异值分解 -------------------- */
    LV_PRESET_REGISTER(success_count,
            PRESET_NUMERICAL_SVD, "奇异值分解：将矩阵分解为 A = UΣV^T", 1, PRESET_TYPE_TUPLE,
            "A = U \\Sigma V^T, \\quad \\sigma_i = \\sqrt{\\lambda_i(A^T A)}", "O(mn^2)", true, false,
            PRESET_TYPE_MATRIX);

    /* 返回是否所有预设都注册成功 */
    /* lv_log_info("数值分析预设注册完成，共 %d 个预设", success_count) */
    return success_count == NUMERICAL_PRESET_COUNT;
}

/**
 * @brief 获取数值分析预设函数块的类别
 * @return 预设类别枚举值
 */
PresetCategory preset_numerical_category(void) {
    return PRESET_CATEGORY_CUSTOM;
}

/**
 * @brief 获取数值分析预设函数块的名称列表
 * @param out_names 输出：名称数组（调用者需先释放每个元素再释放数组本身）
 * @param out_count 输出：名称数量
 * @return true 成功，false 内存分配失败
 */
bool preset_numerical_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;
    *out_count = NUMERICAL_PRESET_COUNT;
    char **names = (char **) lv_malloc(NUMERICAL_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* 方程求根 */
        PRESET_NUMERICAL_BISECTION,
        PRESET_NUMERICAL_NEWTON,
        PRESET_NUMERICAL_SECANT,
        PRESET_NUMERICAL_FIXED_POINT,
        /* 数值积分 */
        PRESET_NUMERICAL_TRAPEZOID,
        PRESET_NUMERICAL_SIMPSON,
        PRESET_NUMERICAL_GAUSS_QUADRATURE,
        PRESET_NUMERICAL_ROMBERG,
        /* 数值微分 */
        PRESET_NUMERICAL_FORWARD_DIFF,
        PRESET_NUMERICAL_CENTRAL_DIFF,
        PRESET_NUMERICAL_RICHARDSON,
        /* 插值 */
        PRESET_NUMERICAL_LAGRANGE_INTERP,
        PRESET_NUMERICAL_NEWTON_INTERP,
        PRESET_NUMERICAL_SPLINE_INTERP,
        /* 拟合 */
        PRESET_NUMERICAL_LEAST_SQUARES,
        PRESET_NUMERICAL_POLYNOMIAL_FIT,
        /* 常微分方程 */
        PRESET_NUMERICAL_EULER,
        PRESET_NUMERICAL_RK4,
        PRESET_NUMERICAL_ADAMS,
        /* 线性方程组 */
        PRESET_NUMERICAL_GAUSS_ELIMINATION,
        PRESET_NUMERICAL_LU_DECOMPOSITION,
        PRESET_NUMERICAL_ITERATIVE_SOLVE,
        /* 矩阵特征值 */
        PRESET_NUMERICAL_EIGENVALUES,
        PRESET_NUMERICAL_SVD,
    };

    for (int i = 0; i < NUMERICAL_PRESET_COUNT; i++) {
        size_t len = strlen(preset_names[i]) + 1;
        names[i] = (char *) lv_malloc(len);
        if (!names[i]) {
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv_free(&tmp);
            }
            {
                void *tmp = names;
                lv_free(&tmp);
            }
            return false;
        }
        memcpy(names[i], preset_names[i], len);
    }
    *out_names = names;
    return true;
}