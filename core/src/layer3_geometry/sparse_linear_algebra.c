/**
 * @file sparse_linear_algebra.c
 * @brief 稀疏线性代数 —— CSR 格式稀疏矩阵运算
 *
 * @details 实现 Compressed Sparse Row (CSR) 格式的稀疏矩阵，
 *          支持矩阵元素读写、稀疏矩阵-向量乘法和 Jacobi 迭代求解。
 *          用于几何约束系统中的大型稀疏线性方程组求解。
 *
 * @author Lv-00 Project
 * @version 1.1.0
 */

#include "lv/sparse_linear_algebra.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* ---- CSR 矩阵内部结构 ---- */
struct lvSparseMatrix {
    int rows;
    int cols;
    int nnz;        /* 当前非零元素数 */
    int nnz_cap;    /* 已分配容量 */
    double *values; /* [nnz_cap] */
    int *col_idx;   /* [nnz_cap] */
    int *row_ptr;   /* [rows+1] */
};

#define lv_SPARSE_INIT_CAP 128

/**
 * @brief 创建 CSR 稀疏矩阵
 * @param rows 行数
 * @param cols 列数
 * @return 稀疏矩阵（调用者通过 lv_sparse_destroy 释放），失败返回 NULL
 */
lvSparseMatrix *lv_sparse_create(int rows, int cols) {
    if (rows <= 0 || cols <= 0)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "lv_sparse_create: rows or cols <= 0");

    lvSparseMatrix *m = lv_calloc(1, sizeof(lvSparseMatrix));
    if (!m)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_sparse_create: calloc m failed");

    m->rows = rows;
    m->cols = cols;
    m->nnz = 0;
    m->nnz_cap = lv_SPARSE_INIT_CAP;

    m->values = lv_malloc(sizeof(double) * m->nnz_cap);
    m->col_idx = lv_malloc(sizeof(int) * m->nnz_cap);
    m->row_ptr = lv_malloc(sizeof(int) * (rows + 1));

    if (!m->values || !m->col_idx || !m->row_ptr) {
        lv_free(m->values);
        lv_free(m->col_idx);
        lv_free(m->row_ptr);
        lv_free(m);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "lv_sparse_create: malloc values/col_idx/row_ptr failed");
    }

    for (int i = 0; i <= rows; i++)
        m->row_ptr[i] = 0;
    return m;
}

/**
 * @brief 销毁稀疏矩阵
 * @param m 矩阵指针（可为 NULL）
 */
void lv_sparse_destroy(lvSparseMatrix *m) {
    if (!m)
        return;
    lv_free((void **) &m->values);
    lv_free((void **) &m->col_idx);
    lv_free((void **) &m->row_ptr);
    lv_free((void **) &m);
}

static bool sparse_grow(lvSparseMatrix *m, int needed) {
    if (m->nnz + needed <= m->nnz_cap)
        return true;

    int old_cap = m->nnz_cap;

    /* 先扩容 values（失败时指针与容量均不变，旧指针仍有效） */
    if (!lv_ensure_capacity((void **) &m->values, old_cap,
                            &m->nnz_cap, sizeof(double), needed))
        return false;

    /* 扩容 col_idx。临时回退容量指针使扩容真实执行；
       失败时恢复旧容量，values 保持新块（容量 >= 需求）、
       col_idx 保持旧有效数据，两数组旧指针均有效 */
    m->nnz_cap = old_cap;
    if (!lv_ensure_capacity((void **) &m->col_idx, old_cap,
                            &m->nnz_cap, sizeof(int), needed)) {
        m->nnz_cap = old_cap;
        return false;
    }
    return true;
}

/**
 * @brief 设置稀疏矩阵元素值
 * @param m    矩阵指针
 * @param row  行索引
 * @param col  列索引
 * @param val  值（0.0 时跳过存储）
 * @return 成功返回 0，参数无效返回 -1，扩容失败返回 -1
 */
int lv_sparse_set(lvSparseMatrix *m, int row, int col, double val) {
    if (!m || row < 0 || row >= m->rows || col < 0 || col >= m->cols)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_sparse_set: invalid matrix or out of bounds");

    if (val == 0.0) {
        /* 标准稀疏语义：设为 0 即删除已存储元素，保证 get 返回 0 */
        int start = m->row_ptr[row];
        int end = m->row_ptr[row + 1];
        for (int j = start; j < end; j++) {
            if (m->col_idx[j] == col) {
                for (int k = j; k < m->nnz - 1; k++) {
                    m->values[k] = m->values[k + 1];
                    m->col_idx[k] = m->col_idx[k + 1];
                }
                m->nnz--;
                for (int r = row + 1; r <= m->rows; r++)
                    m->row_ptr[r]--;
                return 0;
            }
        }
        return 0; /* 未存储，无需操作 */
    }

    /* 若该元素已存储，就地更新（避免重复存储导致 get 返回旧值） */
    {
        int start = m->row_ptr[row];
        int end = m->row_ptr[row + 1];
        for (int j = start; j < end; j++) {
            if (m->col_idx[j] == col) {
                m->values[j] = val;
                return 0;
            }
        }
    }

    if (!sparse_grow(m, 1))
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lv_sparse_set: sparse_grow failed");

    int pos = m->nnz;
    m->values[pos] = val;
    m->col_idx[pos] = col;
    m->nnz++;

    /* 更新行指针：row_ptr[i] 指向第 i 行的第一个元素在 values[] 中的位置 */
    for (int r = row + 1; r <= m->rows; r++) {
        m->row_ptr[r] = m->nnz;
    }

    return 0;
}

/**
 * @brief 获取稀疏矩阵元素值
 * @param m    矩阵指针
 * @param row  行索引
 * @param col  列索引
 * @return 元素值；参数无效或未存储时返回 0.0
 */
double lv_sparse_get(const lvSparseMatrix *m, int row, int col) {
    if (!m || row < 0 || row >= m->rows || col < 0 || col >= m->cols)
        return 0.0;

    int start = m->row_ptr[row];
    int end = m->row_ptr[row + 1];

    for (int j = start; j < end; j++) {
        if (m->col_idx[j] == col)
            return m->values[j];
    }
    return 0.0;
}

/**
 * @brief Jacobi 迭代法求解稀疏线性方程组 Ax = b
 * @details 仅支持方阵（rows == cols）。迭代上限 100 次，收敛容差 1e-6。
 * @param A 系数矩阵
 * @param b 右端向量
 * @param x 输出解向量
 * @return 成功时返回迭代次数；参数无效返回 -1；非方阵返回 -2；零对角线返回 -3
 */
int lv_sparse_solve(const lvSparseMatrix *A, const double *b, double *x) {
    if (!A || !b || !x)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_sparse_solve: NULL parameter");
    if (A->rows != A->cols)
        return -2; /* 非方阵错误码（与头文件契约一致） */

    int n = A->rows;

    /* Jacobi 迭代法（简单且适合稀疏矩阵）*/
    const int max_iter = 100;
    const double tol = lv_EPSILON_LOW;

    double *x_next = lv_malloc(sizeof(double) * n);
    if (!x_next)
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lv_sparse_solve: malloc x_next failed");

    /* 初始值 */
    for (int i = 0; i < n; i++)
        x[i] = 0.0;

    int iter = 0;
    /* 计算矩阵最大绝对值的估计值，用于相对容差 */
    double max_val = 0.0;
    for (int i = 0; i < n; i++) {
        double d = fabs(lv_sparse_get(A, i, i));
        if (d > max_val)
            max_val = d;
    }
    double diag_tol = lv_EPSILON_ULTRA * (1.0 + max_val);
    for (iter = 0; iter < max_iter; iter++) {
        double max_diff = 0.0;

        for (int i = 0; i < n; i++) {
            double diag = lv_sparse_get(A, i, i);
            if (fabs(diag) < diag_tol) {
                lv_free(x_next);
                return -3;
            } /* 零/近零对角线 */

            double sum = 0.0;
            int start = A->row_ptr[i];
            int end = A->row_ptr[i + 1];
            for (int j = start; j < end; j++) {
                if (A->col_idx[j] != i) {
                    sum += A->values[j] * x[A->col_idx[j]];
                }
            }

            x_next[i] = (b[i] - sum) / diag;
            double diff = fabs(x_next[i] - x[i]);
            if (diff > max_diff)
                max_diff = diff;
        }

        memcpy(x, x_next, sizeof(double) * n);
        if (max_diff < tol)
            break;
    }

    lv_free((void **) &x_next);
    return iter + 1; /* 返回迭代次数 */
}

/**
 * @brief 清空矩阵所有元素（等效于全部置零，保留已分配容量）
 * @param m 矩阵指针（可为 NULL）
 */
void lv_sparse_zero(lvSparseMatrix *m) {
    if (!m)
        return;
    m->nnz = 0;
    for (int i = 0; i <= m->rows; i++)
        m->row_ptr[i] = 0;
}

/**
 * @brief 矩阵-向量乘法：y = A * x（CSR 行遍历）
 * @param A 矩阵
 * @param x 输入向量（长度 >= A->cols）
 * @param y 输出向量（长度 >= A->rows）
 * @return 成功返回 0；参数无效返回 -1
 */
int lv_sparse_matvec(const lvSparseMatrix *A, const double *x, double *y) {
    if (!A || !x || !y)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_sparse_matvec: NULL parameter");

    for (int i = 0; i < A->rows; i++) {
        double sum = 0.0;
        int start = A->row_ptr[i];
        int end = A->row_ptr[i + 1];
        for (int j = start; j < end; j++) {
            sum += A->values[j] * x[A->col_idx[j]];
        }
        y[i] = sum;
    }
    return 0;
}

/**
 * @brief 深拷贝：dst = src（维度须一致）
 * @param dst 目标矩阵（原有元素被覆盖）
 * @param src 源矩阵
 * @return 成功返回 0；参数无效返回 -1；扩容失败返回 -1
 */
int lv_sparse_copy(lvSparseMatrix *dst, const lvSparseMatrix *src) {
    if (!dst || !src)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_sparse_copy: NULL parameter");
    if (dst->rows != src->rows || dst->cols != src->cols)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_sparse_copy: dimension mismatch");

    lv_sparse_zero(dst);
    if (src->nnz == 0)
        return 0;

    if (!sparse_grow(dst, src->nnz))
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "lv_sparse_copy: sparse_grow failed");

    memcpy(dst->values, src->values, sizeof(double) * src->nnz);
    memcpy(dst->col_idx, src->col_idx, sizeof(int) * src->nnz);
    memcpy(dst->row_ptr, src->row_ptr, sizeof(int) * (dst->rows + 1));
    dst->nnz = src->nnz;
    return 0;
}

/**
 * @brief 矩阵-标量乘法：m = c * m（c == 0.0 时等效于 lv_sparse_zero）
 * @param m 矩阵
 * @param c 缩放因子
 * @return 成功返回 0；参数无效返回 -1
 */
int lv_sparse_scale(lvSparseMatrix *m, double c) {
    if (!m)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "lv_sparse_scale: NULL parameter");

    if (c == 0.0) {
        lv_sparse_zero(m);
        return 0;
    }
    for (int i = 0; i < m->nnz; i++)
        m->values[i] *= c;
    return 0;
}
