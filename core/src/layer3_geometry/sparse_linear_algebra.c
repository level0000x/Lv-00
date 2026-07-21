/**
 * @file sparse_linear_algebra.c
 * @brief 稀疏线性代数 —— CSR 格式稀疏矩阵运算
 *
 * @details 实现 Compressed Sparse Row (CSR) 格式的稀疏矩阵，
 *          支持矩阵元素读写、稀疏矩阵-向量乘法和迭代求解。
 *          用于几何约束系统中的大型稀疏线性方程组。
 *
 * @version 1.1.0
 */

#include "sparse_linear_algebra.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* ---- CSR 矩阵内部结构 ---- */
struct Lv00SparseMatrix {
    int      rows;
    int      cols;
    int      nnz;           /* 当前非零元素数 */
    int      nnz_cap;       /* 已分配容量 */
    double  *values;        /* [nnz_cap] */
    int     *col_idx;       /* [nnz_cap] */
    int     *row_ptr;       /* [rows+1] */
};

#define LV00_SPARSE_INIT_CAP 128

Lv00SparseMatrix *lv00_sparse_create(int rows, int cols) {
    if (rows <= 0 || cols <= 0) return NULL;

    Lv00SparseMatrix *m = lv00_malloc(sizeof(Lv00SparseMatrix));
    if (!m) return NULL;

    m->rows = rows;
    m->cols = cols;
    m->nnz = 0;
    m->nnz_cap = LV00_SPARSE_INIT_CAP;

    m->values   = lv00_malloc(sizeof(double) * m->nnz_cap);
    m->col_idx  = lv00_malloc(sizeof(int)    * m->nnz_cap);
    m->row_ptr  = lv00_malloc(sizeof(int)    * (rows + 1));

    if (!m->values || !m->col_idx || !m->row_ptr) {
        lv00_free(m->values);
        lv00_free(m->col_idx);
        lv00_free(m->row_ptr);
        lv00_free(m);
        return NULL;
    }

    for (int i = 0; i <= rows; i++) m->row_ptr[i] = 0;
    return m;
}

void lv00_sparse_destroy(Lv00SparseMatrix *m) {
    if (!m) return;
    lv00_free(m->values);
    lv00_free(m->col_idx);
    lv00_free(m->row_ptr);
    lv00_free(m);
}

static bool sparse_grow(Lv00SparseMatrix *m, int needed) {
    if (m->nnz + needed <= m->nnz_cap) return true;

    int new_cap = m->nnz_cap * 2;
    while (new_cap < m->nnz + needed) new_cap *= 2;

    double *nv = lv00_realloc(m->values,  sizeof(double) * new_cap);
    int    *nc = lv00_realloc(m->col_idx, sizeof(int)    * new_cap);
    if (!nv || !nc) {
        lv00_free(nv); lv00_free(nc);
        return false;
    }
    m->values  = nv;
    m->col_idx = nc;
    m->nnz_cap = new_cap;
    return true;
}

int lv00_sparse_set(Lv00SparseMatrix *m, int row, int col, double val) {
    if (!m || row < 0 || row >= m->rows || col < 0 || col >= m->cols)
        return -1;

    if (val == 0.0) return 0; /* 不存储零元素 */

    if (!sparse_grow(m, 1)) return -1;

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

double lv00_sparse_get(const Lv00SparseMatrix *m, int row, int col) {
    if (!m || row < 0 || row >= m->rows || col < 0 || col >= m->cols)
        return 0.0;

    int start = m->row_ptr[row];
    int end   = m->row_ptr[row + 1];

    for (int j = start; j < end; j++) {
        if (m->col_idx[j] == col) return m->values[j];
    }
    return 0.0;
}

int lv00_sparse_solve(const Lv00SparseMatrix *A, const double *b, double *x) {
    if (!A || !b || !x) return -1;
    if (A->rows != A->cols) return -2; /* 仅支持方阵 */

    int n = A->rows;

    /* Jacobi 迭代法（简单且适合稀疏矩阵）*/
    const int max_iter = 100;
    const double tol = 1e-6;

    double *x_next = lv00_malloc(sizeof(double) * n);
    if (!x_next) return -1;

    /* 初始值 */
    for (int i = 0; i < n; i++) x[i] = 0.0;

    int iter = 0;
    for (iter = 0; iter < max_iter; iter++) {
        double max_diff = 0.0;

        for (int i = 0; i < n; i++) {
            double diag = lv00_sparse_get(A, i, i);
            if (fabs(diag) < 1e-12) { lv00_free(x_next); return -3; } /* 零对角线 */

            double sum = 0.0;
            int start = A->row_ptr[i];
            int end   = A->row_ptr[i + 1];
            for (int j = start; j < end; j++) {
                if (A->col_idx[j] != i) {
                    sum += A->values[j] * x[A->col_idx[j]];
                }
            }

            x_next[i] = (b[i] - sum) / diag;
            double diff = fabs(x_next[i] - x[i]);
            if (diff > max_diff) max_diff = diff;
        }

        memcpy(x, x_next, sizeof(double) * n);
        if (max_diff < tol) break;
    }

    lv00_free(x_next);
    return iter + 1; /* 返回迭代次数 */
}
