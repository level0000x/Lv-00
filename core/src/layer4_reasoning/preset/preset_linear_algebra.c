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

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_register_helper.h" /* 统一注册辅助宏 */

/* ==================== 预设函数块数量 ==================== */

/** 线性代数模块预设函数块总数 */
#define LINEAR_ALGEBRA_PRESET_COUNT 32

/* ==================== 模块注册实现 ==================== */

bool preset_linear_algebra_register(void) {
    int success_count = 0;
    int total_count = 0;

    /* ============================================================
     * 第一部分：矩阵基础运算（8个）
     * ============================================================ */

    /* -------------------- 矩阵创建 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_INTEGER, PRESET_TYPE_INTEGER};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_MATRIX_CREATE,
                                PRESET_TYPE_MATRIX, inputs, 2, "矩阵创建：根据行数和列数创建 m x n 零矩阵",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(mn)", false);
    }

    /* -------------------- 矩阵加法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_MATRIX_ADD,
                                PRESET_TYPE_MATRIX, inputs, 2, "矩阵加法：计算两个同阶矩阵的和 C = A + B，对应元素相加",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^2)", true);
    }

    /* -------------------- 矩阵减法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_MATRIX_SUBTRACT,
                                PRESET_TYPE_MATRIX, inputs, 2, "矩阵减法：计算两个同阶矩阵的差 C = A - B，对应元素相减",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^2)", true);
    }

    /* -------------------- 矩阵乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_MATRIX_MULTIPLY,
                                PRESET_TYPE_MATRIX, inputs, 2, "矩阵乘法：计算两个矩阵的乘积 C = A * B，行与列的点积",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- 矩阵标量乘法 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SCALAR, PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_MATRIX_SCALE,
                                PRESET_TYPE_MATRIX, inputs, 2, "矩阵标量乘法：计算标量与矩阵的乘积 B = k * A",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^2)", true);
    }

    /* -------------------- 矩阵转置 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_MATRIX_TRANSPOSE,
                                PRESET_TYPE_MATRIX, inputs, 1, "矩阵转置：将矩阵的行与列互换，得到转置矩阵 A^T",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^2)", true);
    }

    /* -------------------- 矩阵迹 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_MATRIX_TRACE,
                                PRESET_TYPE_SCALAR, inputs, 1, "矩阵迹：计算方阵主对角线元素之和 tr(A) = sum(a_ii)",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n)", false);
    }

    /* -------------------- 矩阵取负 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_MATRIX_NEGATE,
                                PRESET_TYPE_MATRIX, inputs, 1, "矩阵取负：计算矩阵的负矩阵 B = -A，每个元素取反",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^2)", true);
    }

    /* ============================================================
     * 第二部分：行列式与逆矩阵（6个）
     * ============================================================ */

    /* -------------------- 2x2行列式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_DETERMINANT_2X2,
                                PRESET_TYPE_SCALAR, inputs, 1, "2x2行列式：det(A) = ad - bc",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(1)", false);
    }

    /* -------------------- 3x3行列式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_DETERMINANT_3X3,
                                PRESET_TYPE_SCALAR, inputs, 1, "3x3行列式：按第一行展开计算 det(A)",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(1)", false);
    }

    /* -------------------- n阶行列式 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_DETERMINANT_N,
                                PRESET_TYPE_SCALAR, inputs, 1, "n阶行列式：使用LU分解或余子式展开计算方阵的行列式",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- 2x2逆矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_INVERSE_2X2,
                                PRESET_TYPE_MATRIX, inputs, 1, "2x2逆矩阵：A^{-1} = (1/det(A)) * adj(A)",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(1)", true);
    }

    /* -------------------- 3x3逆矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_INVERSE_3X3,
                                PRESET_TYPE_MATRIX, inputs, 1, "3x3逆矩阵：使用伴随矩阵法计算 A^{-1} = adj(A)/det(A)",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(1)", true);
    }

    /* -------------------- 伴随矩阵 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_ADJUGATE,
                                PRESET_TYPE_MATRIX, inputs, 1, "伴随矩阵：计算方阵的伴随矩阵 adj(A)，元素为代数余子式的转置",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* ============================================================
     * 第三部分：矩阵分解（5个）
     * ============================================================ */

    /* -------------------- LU分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_LU_DECOMPOSITION,
                                PRESET_TYPE_TUPLE, inputs, 1, "LU分解：将方阵分解为下三角矩阵 L 和上三角矩阵 U 的乘积 A = LU",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- QR分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_QR_DECOMPOSITION,
                                PRESET_TYPE_TUPLE, inputs, 1, "QR分解：将矩阵分解为正交矩阵 Q 和上三角矩阵 R 的乘积 A = QR",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- Cholesky分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_CHOLESKY,
                                PRESET_TYPE_MATRIX, inputs, 1, "Cholesky分解：将正定对称矩阵分解为 A = L * L^T",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- 2x2特征值 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_EIGENVALUES_2X2,
                                PRESET_TYPE_LIST, inputs, 1, "2x2矩阵特征值：求解特征方程 det(A - lambda*I) = 0",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(1)", false);
    }

    /* -------------------- 2x2特征向量 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX, PRESET_TYPE_SCALAR};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_EIGENVECTORS_2X2,
                                PRESET_TYPE_LIST, inputs, 2, "2x2矩阵特征向量：计算对应于给定特征值的特征向量",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(1)", false);
    }

    /* ============================================================
     * 第四部分：向量空间（5个）
     * ============================================================ */

    /* -------------------- 向量空间判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET, PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_VECTOR_SPACE_TEST,
                                PRESET_TYPE_BOOLEAN, inputs, 3, "向量空间判定：验证集合在给定加法和标量乘法下是否满足向量空间公理",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^2)", false);
    }

    /* -------------------- 线性无关判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_LINEAR_INDEPENDENCE,
                                PRESET_TYPE_BOOLEAN, inputs, 1, "线性无关判定：验证向量组 {v1,...,vk} 是否线性无关",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- 生成空间 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_SPAN,
                                PRESET_TYPE_SET, inputs, 1, "生成空间：计算向量组 {v1,...,vk} 的线性生成空间 span{v1,...,vk}",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- 基 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_BASIS,
                                PRESET_TYPE_LIST, inputs, 1, "基：计算向量空间的一组基（极大线性无关组）",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- 维数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_DIMENSION,
                                PRESET_TYPE_INTEGER, inputs, 1, "维数：计算向量空间的维数 dim(V)（基中元素个数）",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* ============================================================
     * 第五部分：线性映射（4个）
     * ============================================================ */

    /* -------------------- 线性映射判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_LINEAR_MAP_TEST,
                                PRESET_TYPE_BOOLEAN, inputs, 1, "线性映射判定：验证映射 T 是否满足 T(u+v) = T(u)+T(v) 和 T(cv) = cT(v)",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^2)", false);
    }

    /* -------------------- 核空间 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_KERNEL,
                                PRESET_TYPE_LIST, inputs, 1, "核空间：计算线性映射的核 ker(T) = {v : T(v) = 0} 的基",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- 像 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_IMAGE,
                                PRESET_TYPE_LIST, inputs, 1, "像：计算线性映射的像空间 Im(T) = {T(v) : v in V} 的基",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- 秩-零化度定理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MATRIX};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_RANK_NULLITY,
                                PRESET_TYPE_BOOLEAN, inputs, 1, "秩-零化度定理：验证 rank(T) + nullity(T) = dim(V)",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* ============================================================
     * 第六部分：内积空间（4个）
     * ============================================================ */

    /* -------------------- 内积 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_VECTOR, PRESET_TYPE_VECTOR};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_INNER_PRODUCT,
                                PRESET_TYPE_SCALAR, inputs, 2, "内积：计算内积空间中两个向量的内积 <u, v>",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n)", true);
    }

    /* -------------------- Gram-Schmidt正交化 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_GRAM_SCHMIDT,
                                PRESET_TYPE_LIST, inputs, 1, "Gram-Schmidt正交化：将线性无关向量组转化为正交向量组",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- 正交补 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SET};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_ORTHOGONAL_COMPLEMENT,
                                PRESET_TYPE_SET, inputs, 1, "正交补：计算子空间 W 的正交补 W^perp",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^3)", false);
    }

    /* -------------------- 子空间投影 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_VECTOR, PRESET_TYPE_SET};
        LV00_PRESET_REGISTER_EX(success_count, total_count, NULL, PRESET_LINALG_PROJECTION,
                                PRESET_TYPE_VECTOR, inputs, 2, "子空间投影：计算向量 v 在子空间 W 上的正交投影 proj_W(v)",
                                PRESET_CATEGORY_LINEAR_ALGEBRA, "O(n^2)", false);
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == LINEAR_ALGEBRA_PRESET_COUNT;
}

/**
 * @brief 获取线性代数预设函数块数量
 *
 * @return int 线性代数模块预设函数块总数
 */
int preset_linear_algebra_count(void) {
    return LINEAR_ALGEBRA_PRESET_COUNT;
}

/**
 * @brief 获取线性代数预设的类别
 *
 * @return PresetCategory 线性代数模块所属类别
 */
PresetCategory preset_linear_algebra_category(void) {
    return PRESET_CATEGORY_LINEAR_ALGEBRA;
}

bool preset_linear_algebra_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    /* 分配名称数组 */
    char **names = (char **) lv00_malloc(LINEAR_ALGEBRA_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

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

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                {
                    void *tmp = names[j];
                    lv00_free(&tmp);
                }
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
