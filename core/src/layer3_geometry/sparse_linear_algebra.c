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
#include "lv_internal.h"
#include "lv_utils.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* ---- CSR 矩阵内部结构 ---- */
struct lvSparseMatrix {
    int      rows;
    int      cols;
    int      nnz;           /* 当前非零元素数 */
    int      nnz_cap;       /* 已分配容量 */
    double  *values;        /* [nnz_cap] */
    int     *col_idx;       /* [nnz_cap] */
    int     *row_ptr;       /* [rows+1] */
};

#define lv_SPARSE_INIT_CAP 128

lvSparseMatrix *lv_sparse_create(int rows, int cols) {
    if (rows <= 0 || cols <= 0) return NULL;

    lvSparseMatrix *m = lv_malloc(sizeof(lvSparseMatrix));
    if (!m) return NULL;

    m->rows = rows;
    m->cols = cols;
    m->nnz = 0;
    m->nnz_cap = lv_SPARSE_INIT_CAP;

    m->values   = lv_malloc(sizeof(double) * m->nnz_cap);
    m->col_idx  = lv_malloc(sizeof(int)    * m->nnz_cap);
    m->row_ptr  = lv_malloc(sizeof(int)    * (rows + 1));

    if (!m->values || !m->col_idx || !m->row_ptr) {
        lv_free(m->values);
        lv_free(m->col_idx);
        lv_free(m->row_ptr);
        lv_free(m);
        return NULL;
    }

    for (int i = 0; i <= rows; i++) m->row_ptr[i] = 0;
    return m;
}

void lv_sparse_destroy(lvSparseMatrix *m) {
    if (!m) return;
    lv_free(m->values);
    lv_free(m->col_idx);
    lv_free(m->row_ptr);
    lv_free(m);
}

static bool sparse_grow(lvSparseMatrix *m, int needed) {
    if (m->nnz + needed <= m->nnz_cap) return true;

    int new_cap = m->nnz_cap * 2;
    while (new_cap < m->nnz + needed) new_cap *= 2;

    double *nv = lv_realloc(m->values,  sizeof(double) * new_cap);
    int    *nc = lv_realloc(m->col_idx, sizeof(int)    * new_cap);
    if (!nv || !nc) {
        lv_free(nv); lv_free(nc);
        return false;
    }
    m->values  = nv;
    m->col_idx = nc;
    m->nnz_cap = new_cap;
    return true;
}

int lv_sparse_set(lvSparseMatrix *m, int row, int col, double val) {
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

double lv_sparse_get(const lvSparseMatrix *m, int row, int col) {
    if (!m || row < 0 || row >= m->rows || col < 0 || col >= m->cols)
        return 0.0;

    int start = m->row_ptr[row];
    int end   = m->row_ptr[row + 1];

    for (int j = start; j < end; j++) {
        if (m->col_idx[j] == col) return m->values[j];
    }
    return 0.0;
}

int lv_sparse_solve(const lvSparseMatrix *A, const double *b, double *x) {
    if (!A || !b || !x) return -1;
    if (A->rows != A->cols) return -2; /* 仅支持方阵 */

    int n = A->rows;

    /* Jacobi 迭代法（简单且适合稀疏矩阵）*/
    const int max_iter = 100;
    const double tol = 1e-6;

    double *x_next = lv_malloc(sizeof(double) * n);
    if (!x_next) return -1;

    /* 初始值 */
    for (int i = 0; i < n; i++) x[i] = 0.0;

    int iter = 0;
    for (iter = 0; iter < max_iter; iter++) {
        double max_diff = 0.0;

        for (int i = 0; i < n; i++) {
            double diag = lv_sparse_get(A, i, i);
            if (fabs(diag) < 1e-12) { lv_free(x_next); return -3; } /* 零对角线 */

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

    lv_free(x_next);
    return iter + 1; /* 返回迭代次数 */
}
