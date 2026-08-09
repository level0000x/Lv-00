/**
 * @file geo_constraint_solver_linear.c
 * @brief 几何约束求解器 —— 高斯消元法与向量范数
 */

#include "geo_constraint_solver_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 第八部分：高斯消元法
 * ======================================================================== */

/**
 * @brief 高斯消元法求解 n x n 线性方程组 A * x = b
 *
 * 使用部分主元选取（列主元）提高数值稳定性。
 * 矩阵 A 在求解过程中会被修改（行变换）。
 *
 * @param A  n x n 系数矩阵（行优先存储），求解后被修改
 * @param b  右端向量，求解后存储解 x
 * @param n  矩阵维度（n <= 20）
 * @return 0 成功，-1 奇异矩阵
 */
int gauss_eliminate(double *A, double *b, int n) {
    if (n <= 0 || n > 20)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "gauss_eliminate: invalid dimension n=%d", n);

    /* 前向消元（列主元选取） */
    for (int col = 0; col < n; col++) {
        /* 寻找主元 */
        int pivot = col;
        double max_val = fabs(A[col * n + col]);
        for (int row = col + 1; row < n; row++) {
            double val = fabs(A[row * n + col]);
            if (val > max_val) {
                max_val = val;
                pivot = row;
            }
        }

        /* 奇异检测：使用相对容差，依据矩阵列范数自适应缩放 */
        double col_norm = 0.0;
        for (int r = 0; r < n; r++) {
            double av = fabs(A[r * n + col]);
            if (av > col_norm)
                col_norm = av;
        }
        double singular_tol = lv_EPSILON_NEWTON * fmax(1.0, col_norm);
        if (max_val < singular_tol)
            return -1;

        /* 交换行 */
        if (pivot != col) {
            for (int j = 0; j < n; j++) {
                double tmp = A[col * n + j];
                A[col * n + j] = A[pivot * n + j];
                A[pivot * n + j] = tmp;
            }
            double tmp = b[col];
            b[col] = b[pivot];
            b[pivot] = tmp;
        }

        /* 消元 */
        for (int row = col + 1; row < n; row++) {
            double factor = A[row * n + col] / A[col * n + col];
            for (int j = col; j < n; j++) {
                A[row * n + j] -= factor * A[col * n + j];
            }
            b[row] -= factor * b[col];
        }
    }

    /* 回代 */
    for (int row = n - 1; row >= 0; row--) {
        double sum = b[row];
        for (int j = row + 1; j < n; j++) {
            sum -= A[row * n + j] * b[j];
        }
        b[row] = sum / A[row * n + row];
    }

    return 0;
}

/* ========================================================================
 * 第九部分：向量范数
 * ======================================================================== */

/**
 * @brief 计算向量的 L2 范数
 */
double vec_norm(const double *v, int n) {
    if (n <= 0 || !v)
        return 0.0;
    /* 找到最大绝对值，用于缩放以避免平方和溢出到 Inf */
    double max_abs = lv_max_abs(v, (int64_t) n);
    if (max_abs == 0.0)
        return 0.0;
    /* 缩放后计算平方和：sqrt(sum((v[i]/scale)^2)) * scale */
    double scale = max_abs;
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        double scaled = v[i] / scale;
        sum += scaled * scaled;
    }
    return sqrt(sum) * scale;
}

