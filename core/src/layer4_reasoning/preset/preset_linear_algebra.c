/**
 * @file preset_linear_algebra.c
 * @brief 线性代数预设函数块 - 实现
 *
 * 实现理论数学研究中常用的线性代数运算预设函数块。
 * 涵盖矩阵基础运算、行列式与逆矩阵、矩阵分解、向量空间、
 * 线性映射及内积空间。
 *
 * @module LinearAlgebra
 * @category PRESET_CATEGORY_LINEAR_ALGEBRA
 * @version 5.0.0
 * @author Lv-00 开发团队
 */

#include "preset_linear_algebra.h"
#include "preset_blocks.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <string.h>

/* ==================== 预设函数块数量 ==================== */

/** 线性代数模块预设函数块总数 */

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个线性代数预设
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
static bool register_linalg_preset(
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
        PRESET_CATEGORY_LINEAR_ALGEBRA,
        input_types, input_count, output_type,
        math_def, complexity,
        is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_linear_algebra_register(void)
{
    int success_count = 0;

    /* ============================================================
     * 第一部分：矩阵基础运算（8个）
     * ============================================================ */

    /* -------------------- 矩阵创建 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        if (register_linalg_preset(
                PRESET_LINALG_MATRIX_CREATE,
                "矩阵创建：根据行数和列数创建 m x n 零矩阵",
                inputs, 2, PRESET_TYPE_MATRIX,
                "O_{m \\times n} = (o_{ij})_{1 \\le i \\le m, 1 \\le j \\le n}",
                "O(mn)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵加法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_MATRIX_ADD,
                "矩阵加法：计算两个同阶矩阵的和 C = A + B，对应元素相加",
                inputs, 2, PRESET_TYPE_MATRIX,
                "C_{ij} = A_{ij} + B_{ij}",
                "O(n^2)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵减法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_MATRIX_SUBTRACT,
                "矩阵减法：计算两个同阶矩阵的差 C = A - B，对应元素相减",
                inputs, 2, PRESET_TYPE_MATRIX,
                "C_{ij} = A_{ij} - B_{ij}",
                "O(n^2)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_MATRIX_MULTIPLY,
                "矩阵乘法：计算两个矩阵的乘积 C = A * B，行与列的点积",
                inputs, 2, PRESET_TYPE_MATRIX,
                "C_{ij} = \\sum_{k=1}^{n} A_{ik} \\cdot B_{kj}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵标量乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_MATRIX_SCALE,
                "矩阵标量乘法：计算标量与矩阵的乘积 B = k * A",
                inputs, 2, PRESET_TYPE_MATRIX,
                "B_{ij} = k \\cdot A_{ij}",
                "O(n^2)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵转置 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_MATRIX_TRANSPOSE,
                "矩阵转置：将矩阵的行与列互换，得到转置矩阵 A^T",
                inputs, 1, PRESET_TYPE_MATRIX,
                "(A^T)_{ij} = A_{ji}",
                "O(n^2)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵迹 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_MATRIX_TRACE,
                "矩阵迹：计算方阵主对角线元素之和 tr(A) = sum(a_ii)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\text{tr}(A) = \\sum_{i=1}^{n} a_{ii}",
                "O(n)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 矩阵取负 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_MATRIX_NEGATE,
                "矩阵取负：计算矩阵的负矩阵 B = -A，每个元素取反",
                inputs, 1, PRESET_TYPE_MATRIX,
                "B_{ij} = -A_{ij}",
                "O(n^2)", true, true)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：行列式与逆矩阵（6个）
     * ============================================================ */

    /* -------------------- 2x2行列式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_DETERMINANT_2X2,
                "2x2行列式：det(A) = ad - bc",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\det\\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix} = ad - bc",
                "O(1)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 3x3行列式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_DETERMINANT_3X3,
                "3x3行列式：按第一行展开计算 det(A)",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\det(A) = a(ei-fh) - b(di-fg) + c(dh-eg)",
                "O(1)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- n阶行列式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_DETERMINANT_N,
                "n阶行列式：使用LU分解或余子式展开计算方阵的行列式",
                inputs, 1, PRESET_TYPE_SCALAR,
                "\\det(A) = \\sum_{\\sigma \\in S_n} \\text{sgn}(\\sigma) "
                "\\prod_{i=1}^{n} a_{i,\\sigma(i)}",
                "O(n^3)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 2x2逆矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_INVERSE_2X2,
                "2x2逆矩阵：A^{-1} = (1/det(A)) * adj(A)",
                inputs, 1, PRESET_TYPE_MATRIX,
                "A^{-1} = \\frac{1}{ad-bc} \\begin{pmatrix} d & -b \\\\ -c & a \\end{pmatrix}",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 3x3逆矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_INVERSE_3X3,
                "3x3逆矩阵：使用伴随矩阵法计算 A^{-1} = adj(A)/det(A)",
                inputs, 1, PRESET_TYPE_MATRIX,
                "A^{-1} = \\frac{\\text{adj}(A)}{\\det(A)}",
                "O(1)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- 伴随矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_ADJUGATE,
                "伴随矩阵：计算方阵的伴随矩阵 adj(A)，元素为代数余子式的转置",
                inputs, 1, PRESET_TYPE_MATRIX,
                "\\text{adj}(A)_{ij} = C_{ji} = (-1)^{i+j} M_{ji}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：矩阵分解（5个）
     * ============================================================ */

    /* -------------------- LU分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_LU_DECOMPOSITION,
                "LU分解：将方阵分解为下三角矩阵 L 和上三角矩阵 U 的乘积 A = LU",
                inputs, 1, PRESET_TYPE_TUPLE,
                "A = LU, \\quad L \\text{ 为下三角矩阵}, U \\text{ 为上三角矩阵}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- QR分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_QR_DECOMPOSITION,
                "QR分解：将矩阵分解为正交矩阵 Q 和上三角矩阵 R 的乘积 A = QR",
                inputs, 1, PRESET_TYPE_TUPLE,
                "A = QR, \\quad Q^T Q = I, \\quad R \\text{ 为上三角矩阵}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- Cholesky分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_CHOLESKY,
                "Cholesky分解：将正定对称矩阵分解为 A = L * L^T",
                inputs, 1, PRESET_TYPE_MATRIX,
                "A = LL^T, \\quad A \\text{ 正定对称}, L \\text{ 下三角矩阵}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 2x2特征值 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_EIGENVALUES_2X2,
                "2x2矩阵特征值：求解特征方程 det(A - lambda*I) = 0",
                inputs, 1, PRESET_TYPE_LIST,
                "\\lambda = \\frac{\\text{tr}(A) \\pm \\sqrt{\\text{tr}(A)^2 - 4\\det(A)}}{2}",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 2x2特征向量 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_SCALAR};
        if (register_linalg_preset(
                PRESET_LINALG_EIGENVECTORS_2X2,
                "2x2矩阵特征向量：计算对应于给定特征值的特征向量",
                inputs, 2, PRESET_TYPE_LIST,
                "Av = \\lambda v, \\quad v \\neq 0",
                "O(1)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：向量空间（5个）
     * ============================================================ */

    /* -------------------- 向量空间判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_linalg_preset(
                PRESET_LINALG_VECTOR_SPACE_TEST,
                "向量空间判定：验证集合在给定加法和标量乘法下是否满足向量空间公理",
                inputs, 3, PRESET_TYPE_BOOLEAN,
                "V \\text{ 是向量空间} \\Leftrightarrow "
                "\\text{封闭性 + 交换律 + 结合律 + 存在零元/逆元 + 分配律}",
                "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 线性无关判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_linalg_preset(
                PRESET_LINALG_LINEAR_INDEPENDENCE,
                "线性无关判定：验证向量组 {v1,...,vk} 是否线性无关",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\sum_{i=1}^{k} c_i v_i = 0 \\Rightarrow c_1 = \\cdots = c_k = 0",
                "O(n^3)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 生成空间 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_linalg_preset(
                PRESET_LINALG_SPAN,
                "生成空间：计算向量组 {v1,...,vk} 的线性生成空间 span{v1,...,vk}",
                inputs, 1, PRESET_TYPE_SET,
                "\\text{span}\\{v_1, \\ldots, v_k\\} = "
                "\\left\\{\\sum_{i=1}^{k} c_i v_i : c_i \\in \\mathbb{F}\\right\\}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 基 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_linalg_preset(
                PRESET_LINALG_BASIS,
                "基：计算向量空间的一组基（极大线性无关组）",
                inputs, 1, PRESET_TYPE_LIST,
                "\\mathcal{B} = \\{e_1, \\ldots, e_n\\}, \\quad "
                "\\text{线性无关且 } \\text{span}(\\mathcal{B}) = V",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 维数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_linalg_preset(
                PRESET_LINALG_DIMENSION,
                "维数：计算向量空间的维数 dim(V)（基中元素个数）",
                inputs, 1, PRESET_TYPE_INTEGER,
                "\\dim(V) = |\\mathcal{B}|, \\quad "
                "\\text{所有基的元素个数相同}",
                "O(n^3)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：线性映射（4个）
     * ============================================================ */

    /* -------------------- 线性映射判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_linalg_preset(
                PRESET_LINALG_LINEAR_MAP_TEST,
                "线性映射判定：验证映射 T 是否满足 T(u+v) = T(u)+T(v) 和 T(cv) = cT(v)",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "T(\\alpha u + \\beta v) = \\alpha T(u) + \\beta T(v)",
                "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- 核空间 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_KERNEL,
                "核空间：计算线性映射的核 ker(T) = {v : T(v) = 0} 的基",
                inputs, 1, PRESET_TYPE_LIST,
                "\\ker(T) = \\{v \\in V : T(v) = 0\\}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 像 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_IMAGE,
                "像：计算线性映射的像空间 Im(T) = {T(v) : v in V} 的基",
                inputs, 1, PRESET_TYPE_LIST,
                "\\text{Im}(T) = \\{T(v) : v \\in V\\} = "
                "\\text{span}\\{T(e_1), \\ldots, T(e_n)\\}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 秩-零化度定理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        if (register_linalg_preset(
                PRESET_LINALG_RANK_NULLITY,
                "秩-零化度定理：验证 rank(T) + nullity(T) = dim(V)",
                inputs, 1, PRESET_TYPE_BOOLEAN,
                "\\text{rank}(T) + \\text{nullity}(T) = \\dim(V)",
                "O(n^3)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：内积空间（4个）
     * ============================================================ */

    /* -------------------- 内积 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_VECTOR, PRESET_TYPE_VECTOR};
        if (register_linalg_preset(
                PRESET_LINALG_INNER_PRODUCT,
                "内积：计算内积空间中两个向量的内积 <u, v>",
                inputs, 2, PRESET_TYPE_SCALAR,
                "\\langle u, v \\rangle = \\sum_{i=1}^{n} u_i \\overline{v_i}",
                "O(n)", true, true)) {
            success_count++;
        }
    }

    /* -------------------- Gram-Schmidt正交化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_linalg_preset(
                PRESET_LINALG_GRAM_SCHMIDT,
                "Gram-Schmidt正交化：将线性无关向量组转化为正交向量组",
                inputs, 1, PRESET_TYPE_LIST,
                "u_k = v_k - \\sum_{i=1}^{k-1} "
                "\\frac{\\langle v_k, u_i \\rangle}{\\langle u_i, u_i \\rangle} u_i",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 正交补 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        if (register_linalg_preset(
                PRESET_LINALG_ORTHOGONAL_COMPLEMENT,
                "正交补：计算子空间 W 的正交补 W^perp",
                inputs, 1, PRESET_TYPE_SET,
                "W^\\perp = \\{v : \\langle v, w \\rangle = 0, \\forall w \\in W\\}",
                "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- 子空间投影 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_VECTOR, PRESET_TYPE_SET};
        if (register_linalg_preset(
                PRESET_LINALG_PROJECTION,
                "子空间投影：计算向量 v 在子空间 W 上的正交投影 proj_W(v)",
                inputs, 2, PRESET_TYPE_VECTOR,
                "\\text{proj}_W(v) = \\sum_{i=1}^{k} "
                "\\frac{\\langle v, e_i \\rangle}{\\langle e_i, e_i \\rangle} e_i",
                "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == LINEAR_ALGEBRA_PRESET_COUNT;
}

/**
 * @brief 获取线性代数预设函数块数量
 *
 * @return int 线性代数模块预设函数块总数
 */
int preset_linear_algebra_count(void)
{
    return LINEAR_ALGEBRA_PRESET_COUNT;
}

/**
 * @brief 获取线性代数预设的类别
 *
 * @return PresetCategory 线性代数模块所属类别
 */
PresetCategory preset_linear_algebra_category(void)
{
    return PRESET_CATEGORY_LINEAR_ALGEBRA;
}

bool preset_linear_algebra_get_names(char ***out_names, int *out_count)
{
    if (!out_names || !out_count) return false;

    /* 分配名称数组 */
    char **names = (char**)lv00_malloc(LINEAR_ALGEBRA_PRESET_COUNT * sizeof(char*));
    if (!names) return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 矩阵基础运算 */
        PRESET_LINALG_MATRIX_CREATE,
        PRESET_LINALG_MATRIX_ADD,
        PRESET_LINALG_MATRIX_SUBTRACT,
        PRESET_LINALG_MATRIX_MULTIPLY,
        PRESET_LINALG_MATRIX_SCALE,
        PRESET_LINALG_MATRIX_TRANSPOSE,
        PRESET_LINALG_MATRIX_TRACE,
        PRESET_LINALG_MATRIX_NEGATE,
        /* 行列式与逆矩阵 */
        PRESET_LINALG_DETERMINANT_2X2,
        PRESET_LINALG_DETERMINANT_3X3,
        PRESET_LINALG_DETERMINANT_N,
        PRESET_LINALG_INVERSE_2X2,
        PRESET_LINALG_INVERSE_3X3,
        PRESET_LINALG_ADJUGATE,
        /* 矩阵分解 */
        PRESET_LINALG_LU_DECOMPOSITION,
        PRESET_LINALG_QR_DECOMPOSITION,
        PRESET_LINALG_CHOLESKY,
        PRESET_LINALG_EIGENVALUES_2X2,
        PRESET_LINALG_EIGENVECTORS_2X2,
        /* 向量空间 */
        PRESET_LINALG_VECTOR_SPACE_TEST,
        PRESET_LINALG_LINEAR_INDEPENDENCE,
        PRESET_LINALG_SPAN,
        PRESET_LINALG_BASIS,
        PRESET_LINALG_DIMENSION,
        /* 线性映射 */
        PRESET_LINALG_LINEAR_MAP_TEST,
        PRESET_LINALG_KERNEL,
        PRESET_LINALG_IMAGE,
        PRESET_LINALG_RANK_NULLITY,
        /* 内积空间 */
        PRESET_LINALG_INNER_PRODUCT,
        PRESET_LINALG_GRAM_SCHMIDT,
        PRESET_LINALG_ORTHOGONAL_COMPLEMENT,
        PRESET_LINALG_PROJECTION,
    };

    int count = (int)(sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
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
