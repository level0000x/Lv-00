/**
 * @file preset_matrix.c
 * @brief 矩阵运算预设函数块 - 实现
 *
 * 实现理论数学研究中常用的矩阵运算预设函数块。
 * 所有预设函数块都遵循模块化、确定性原则。
 *
 * @module Matrix
 * @category PRESET_CATEGORY_ALGEBRAIC
 * @version 5.0.0
 */

#include "preset_matrix.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 矩阵运算模块预设函数块总数 */
#define MATRIX_PRESET_COUNT 28

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个矩阵运算预设
 *
 * 辅助函数，简化预设注册过程。
 * 所有矩阵运算预设都属于 PRESET_CATEGORY_ALGEBRAIC 类别。
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
static bool register_matrix_preset(
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
        PRESET_CATEGORY_ALGEBRAIC,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_matrix_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：基础矩阵运算
     * ============================================================ */

    /* -------------------- 矩阵加法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_ADD,
                "矩阵加法：计算同阶矩阵 A 与 B 的和 C = A + B",
                inputs, 2, PRESET_TYPE_MATRIX,
                "C_{ij} = A_{ij} + B_{ij}",
                "O(mn)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵减法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_SUBTRACT,
                "矩阵减法：计算同阶矩阵 A 与 B 的差 C = A - B",
                inputs, 2, PRESET_TYPE_MATRIX,
                "C_{ij} = A_{ij} - B_{ij}",
                "O(mn)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 标量乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_SCALAR_MULTIPLY,
                "标量乘法：计算标量 k 与矩阵 A 的数乘 kA",
                inputs, 2, PRESET_TYPE_MATRIX,
                "(kA)_{ij} = k \\cdot A_{ij}",
                "O(mn)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_MULTIPLY,
                "矩阵乘法：计算矩阵 A 与 B 的乘积 C = AB（A的列数等于B的行数）",
                inputs, 2, PRESET_TYPE_MATRIX,
                "C_{ij} = \\sum_{k=1}^{n} A_{ik} B_{kj}",
                "O(mnp)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵转置 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_TRANSPOSE,
                "矩阵转置：计算矩阵 A 的转置 A^T",
                inputs, 1, PRESET_TYPE_MATRIX,
                "(A^T)_{ij} = A_{ji}",
                "O(mn)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵迹 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_TRACE,
                "矩阵迹：计算方阵 A 的迹 tr(A)，即主对角线元素之和",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\text{tr}(A) = \\sum_{i=1}^{n} A_{ii}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵行列式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_DETERMINANT,
                "矩阵行列式：计算方阵 A 的行列式 det(A)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\det(A) = \\sum_{\\sigma \\in S_n} \\text{sgn}(\\sigma) \\prod_{i=1}^{n} A_{i,\\sigma(i)}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵逆 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_INVERSE,
                "矩阵逆：计算可逆方阵 A 的逆矩阵 A^{-1}（det(A) != 0）",
                inputs, 1, PRESET_TYPE_MATRIX,
                "A A^{-1} = A^{-1} A = I_n, \\quad \\det(A) \\neq 0",
                "O(n^3)", true, true)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：线性代数
     * ============================================================ */

    /* -------------------- 矩阵秩 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_RANK,
                "矩阵秩：计算矩阵 A 的秩 rank(A)，即行空间（或列空间）的维数",
                inputs, 1, PRESET_TYPE_INTEGER,
                "\\text{rank}(A) = \\dim(\\text{Im}(A))",
                "O(mn \\cdot \\min(m,n))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 零化度 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_NULLITY,
                "零化度：计算矩阵 A 的零化度 nullity(A) = dim(ker(A))",
                inputs, 1, PRESET_TYPE_INTEGER,
                "\\text{nullity}(A) = \\dim(\\ker(A)) = n - \\text{rank}(A)",
                "O(mn \\cdot \\min(m,n))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 特征值 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_EIGENVALUES,
                "特征值：计算方阵 A 的所有特征值 lambda_i，满足 det(A - lambda I) = 0",
                inputs, 1, PRESET_TYPE_LIST,
                "\\det(A - \\lambda I) = 0",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 特征向量 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_EIGENVECTORS,
                "特征向量：计算方阵 A 的特征向量，满足 Av = lambda v",
                inputs, 1, PRESET_TYPE_LIST,
                "A \\mathbf{v} = \\lambda \\mathbf{v}, \\quad \\mathbf{v} \\neq \\mathbf{0}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 特征多项式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_CHARACTERISTIC_POLY,
                "特征多项式：计算方阵 A 的特征多项式 p(lambda) = det(lambda I - A)",
                inputs, 1, PRESET_TYPE_POLYNOMIAL,
                "p(\\lambda) = \\det(\\lambda I - A) = \\lambda^n + c_{n-1}\\lambda^{n-1} + \\cdots + c_0",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 最小多项式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_MINIMAL_POLY,
                "最小多项式：计算方阵 A 的最小多项式 m(x)，即满足 m(A) = 0 的最低次首一多项式",
                inputs, 1, PRESET_TYPE_POLYNOMIAL,
                "m(A) = 0, \\quad m(x) | p(x)",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 核空间 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_KERNEL,
                "核空间：计算矩阵 A 的核空间（零空间）ker(A) = {x : Ax = 0} 的基",
                inputs, 1, PRESET_TYPE_LIST,
                "\\ker(A) = \\{\\mathbf{x} \\in \\mathbb{R}^n : A\\mathbf{x} = \\mathbf{0}\\}",
                "O(mn \\cdot \\min(m,n))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 像空间 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_IMAGE,
                "像空间：计算矩阵 A 的像空间（列空间）Im(A) = {Ax : x in R^n} 的基",
                inputs, 1, PRESET_TYPE_LIST,
                "\\text{Im}(A) = \\{A\\mathbf{x} : \\mathbf{x} \\in \\mathbb{R}^n\\}",
                "O(mn \\cdot \\min(m,n))", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：矩阵分解
     * ============================================================ */

    /* -------------------- LU分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_LU_DECOMPOSITION,
                "LU分解：将方阵 A 分解为下三角矩阵 L 和上三角矩阵 U 的乘积 A = LU",
                inputs, 1, PRESET_TYPE_TUPLE,
                "A = LU, \\quad L \\text{为下三角}, U \\text{为上三角}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- QR分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_QR_DECOMPOSITION,
                "QR分解：将矩阵 A 分解为正交矩阵 Q 和上三角矩阵 R 的乘积 A = QR",
                inputs, 1, PRESET_TYPE_TUPLE,
                "A = QR, \\quad Q^T Q = I, R \\text{为上三角}",
                "O(mn^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 奇异值分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_SVD,
                "奇异值分解：将矩阵 A 分解为 A = U Sigma V^T，其中 U、V 为正交矩阵，Sigma 为对角矩阵",
                inputs, 1, PRESET_TYPE_TUPLE,
                "A = U \\Sigma V^T, \\quad \\sigma_1 \\geq \\sigma_2 \\geq \\cdots \\geq 0",
                "O(mn \\cdot \\min(m,n))", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Cholesky分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_CHOLESKY,
                "Cholesky分解：将对称正定矩阵 A 分解为 A = LL^T，其中 L 为下三角矩阵",
                inputs, 1, PRESET_TYPE_MATRIX,
                "A = LL^T, \\quad L \\text{为下三角}, A \\text{对称正定}",
                "O(n^3 / 3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Jordan标准形 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_JORDAN_FORM,
                "Jordan标准形：计算方阵 A 的Jordan标准形 J = P^{-1}AP",
                inputs, 1, PRESET_TYPE_TUPLE,
                "J = P^{-1}AP = \\text{diag}(J_1, J_2, \\ldots, J_k)",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 谱分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_matrix_preset(
                PRESET_MATRIX_SPECTRAL,
                "谱分解：将可对角化的方阵 A 分解为 A = PDP^{-1}，其中 D 为特征值对角矩阵",
                inputs, 1, PRESET_TYPE_TUPLE,
                "A = PDP^{-1}, \\quad D = \\text{diag}(\\lambda_1, \\ldots, \\lambda_n)",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：特殊矩阵
     * ============================================================ */

    /* -------------------- 单位矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_matrix_preset(
                PRESET_MATRIX_IDENTITY,
                "单位矩阵：生成 n 阶单位矩阵 I_n，主对角线元素为1，其余为0",
                inputs, 1, PRESET_TYPE_MATRIX,
                "I_n = (\\delta_{ij})_{n \\times n}, \\quad \\delta_{ij} = \\begin{cases} 1 & i = j \\\\ 0 & i \\neq j \\end{cases}",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 零矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_matrix_preset(
                PRESET_MATRIX_ZERO,
                "零矩阵：生成 m x n 阶零矩阵 O_{m x n}，所有元素为0",
                inputs, 2, PRESET_TYPE_MATRIX,
                "O_{m \\times n} = (0)_{m \\times n}",
                "O(mn)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 对角矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_VECTOR};
        if (register_matrix_preset(
                PRESET_MATRIX_DIAGONAL,
                "对角矩阵：以向量 v 的元素作为主对角线生成对角矩阵 diag(v)",
                inputs, 1, PRESET_TYPE_MATRIX,
                "\\text{diag}(v) = \\begin{pmatrix} v_1 & & \\\\ & \\ddots & \\\\ & & v_n \\end{pmatrix}",
                "O(n)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 初等行变换矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER, PRESET_TYPE_SCALAR};
        if (register_matrix_preset(
                PRESET_MATRIX_ELEMENTARY_ROW,
                "初等行变换矩阵：生成 n 阶初等行变换矩阵（行交换、行倍乘、行倍加）",
                inputs, 3, PRESET_TYPE_MATRIX,
                "E_{ij}(c) = I + c \\cdot e_i e_j^T",
                "O(n^2)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- Vandermonde矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_VECTOR};
        if (register_matrix_preset(
                PRESET_MATRIX_VANDERMONDE,
                "Vandermonde矩阵：以向量 x 的元素生成 Vandermonde 矩阵",
                inputs, 1, PRESET_TYPE_MATRIX,
                "V(x_1, \\ldots, x_n) = \\begin{pmatrix} 1 & x_1 & x_1^2 & \\cdots & x_1^{n-1} \\\\ \\vdots & \\vdots & \\vdots & \\ddots & \\vdots \\\\ 1 & x_n & x_n^2 & \\cdots & x_n^{n-1} \\end{pmatrix}",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Hilbert矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER};
        if (register_matrix_preset(
                PRESET_MATRIX_HILBERT,
                "Hilbert矩阵：生成 n 阶 Hilbert 矩阵 H_{ij} = 1/(i+j-1)，经典的病态矩阵",
                inputs, 1, PRESET_TYPE_MATRIX,
                "H_{ij} = \\frac{1}{i + j - 1}, \\quad 1 \\leq i, j \\leq n",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* 输出注册结果日志 */
    ; /* 注册完成 */

    /* 返回是否所有预设都注册成功 */
    return success_count == MATRIX_PRESET_COUNT;
}

/**
 * @brief 获取矩阵运算预设函数块数量
 *
 * @return int 矩阵运算模块预设函数块总数
 */
int preset_matrix_count(void)
{
    return MATRIX_PRESET_COUNT;
}

/**
 * @brief 获取矩阵运算模块的预设类别
 *
 * @return PresetCategory 矩阵运算模块所属类别
 */
PresetCategory preset_matrix_category(void)
{
    return PRESET_CATEGORY_ALGEBRAIC;
}

/**
 * @brief 获取矩阵运算预设函数块名称列表
 *
 * @param out_names 输出名称数组（需调用者释放）
 * @param out_count 输出名称数量
 * @return true 获取成功
 * @return false 参数无效或内存不足
 */
bool preset_matrix_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    /* 分配名称数组 */
    char **names = (char**)lv00_malloc(MATRIX_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 基础矩阵运算 */
        PRESET_MATRIX_ADD,
        PRESET_MATRIX_SUBTRACT,
        PRESET_MATRIX_SCALAR_MULTIPLY,
        PRESET_MATRIX_MULTIPLY,
        PRESET_MATRIX_TRANSPOSE,
        PRESET_MATRIX_TRACE,
        PRESET_MATRIX_DETERMINANT,
        PRESET_MATRIX_INVERSE,
        /* 线性代数 */
        PRESET_MATRIX_RANK,
        PRESET_MATRIX_NULLITY,
        PRESET_MATRIX_EIGENVALUES,
        PRESET_MATRIX_EIGENVECTORS,
        PRESET_MATRIX_CHARACTERISTIC_POLY,
        PRESET_MATRIX_MINIMAL_POLY,
        PRESET_MATRIX_KERNEL,
        PRESET_MATRIX_IMAGE,
        /* 矩阵分解 */
        PRESET_MATRIX_LU_DECOMPOSITION,
        PRESET_MATRIX_QR_DECOMPOSITION,
        PRESET_MATRIX_SVD,
        PRESET_MATRIX_CHOLESKY,
        PRESET_MATRIX_JORDAN_FORM,
        PRESET_MATRIX_SPECTRAL,
        /* 特殊矩阵 */
        PRESET_MATRIX_IDENTITY,
        PRESET_MATRIX_ZERO,
        PRESET_MATRIX_DIAGONAL,
        PRESET_MATRIX_ELEMENTARY_ROW,
        PRESET_MATRIX_VANDERMONDE,
        PRESET_MATRIX_HILBERT,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                { void *tmp = names[j]; lv00_free(&tmp); }
            }
            { void *tmp = names; lv00_free(&tmp); }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
