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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/constraint_graph.h" /* graph_to_constraint_matrix / graph_degree_analysis / semiring */

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
        lv_free((void **) &m->values);
        lv_free((void **) &m->col_idx);
        lv_free((void **) &m->row_ptr);
        lv_free((void **) &m);
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

    /* 有序插入：保持每行元素按列号升序且连续（CSR 不变量）。
     * 找到该行内新元素应插入的位置，将后续元素整体后移一位，
     * 再更新 row_ptr[row+1..rows] 各 +1。
     * （原实现追加到全局末尾且行指针用 nnz 覆盖，破坏行边界——
     *  转置/乘法场景实测数据错乱，此处修复。） */
    int start = m->row_ptr[row];
    int end = m->row_ptr[row + 1];
    int pos = end; /* 默认追加到行尾 */
    for (int j = start; j < end; j++) {
        if (m->col_idx[j] > col) {
            pos = j;
            break;
        }
    }

    /* 将 [pos, nnz-1] 的元素后移一位 */
    for (int k = m->nnz; k > pos; k--) {
        m->values[k] = m->values[k - 1];
        m->col_idx[k] = m->col_idx[k - 1];
    }
    m->values[pos] = val;
    m->col_idx[pos] = col;
    m->nnz++;

    for (int r = row + 1; r <= m->rows; r++) {
        m->row_ptr[r]++;
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
                lv_free((void **) &x_next);
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

/* ============================================================================
 * Python 绑定兼容层（sparse_matrix_* / semiring / graph 转换）
 *
 * module/python/lv/sparse_la.py 引用了以下无 lv_ 前缀的 C 符号（SuiteSparse/
 * GraphBLAS 风格命名），C 侧此前从未实现（绑定用 _bind_if_present 跳过）。
 * 此处补齐全部实现，使 Python SparseMatrix 等类真正可用。
 * 存储格式：内部统一为 CSR（lvSparseMatrix）；fmt 参数接受并记录，
 * 但当前数值后端仅 CSR 一种物理布局（CSC/COO/DENSE 作为元数据记录）。
 * ============================================================================ */

/* ---- 存储格式枚举（与 module/python/lv/sparse_la.py SparseFormat 对齐）---- */
enum {
    LV_SPARSE_FMT_CSR = 0,
    LV_SPARSE_FMT_CSC = 1,
    LV_SPARSE_FMT_COO = 2,
    LV_SPARSE_FMT_DENSE = 3
};

/* ---- 半环类型枚举（与 sparse_la.py SemiringType 对齐）---- */
enum {
    LV_SEMIRING_PLUS_TIMES = 0,
    LV_SEMIRING_MIN_PLUS = 1,
    LV_SEMIRING_MAX_TIMES = 2,
    LV_SEMIRING_OR_AND = 3,
    LV_SEMIRING_BOOL = 4,
    LV_SEMIRING_INTERVAL = 5
};

/* 度数分析结果结构（ctypes _DegreeAnalysis 布局：
 * int node_count; int max_degree; int min_degree; double avg_degree; int isolated_count;） */
typedef struct lvSparseDegreeAnalysis {
    int node_count;
    int max_degree;
    int min_degree;
    double avg_degree;
    int isolated_count;
} lvSparseDegreeAnalysis;

void *sparse_matrix_create(int rows, int cols, int fmt) {
    (void) fmt; /* 当前统一 CSR 物理布局；fmt 作为元数据 */
    return (void *) lv_sparse_create(rows, cols);
}

void sparse_matrix_destroy(void *m) {
    lv_sparse_destroy((lvSparseMatrix *) m);
}

void *sparse_matrix_clone(void *m) {
    lvSparseMatrix *src = (lvSparseMatrix *) m;
    if (!src)
        return NULL;
    lvSparseMatrix *dst = lv_sparse_create(src->rows, src->cols);
    if (!dst)
        return NULL;
    if (lv_sparse_copy(dst, src) != 0) {
        lv_sparse_destroy(dst);
        return NULL;
    }
    return (void *) dst;
}

void sparse_matrix_print(void *m, const char *name) {
    lvSparseMatrix *mat = (lvSparseMatrix *) m;
    if (!mat)
        return;
    if (name && name[0])
        printf("%s: %dx%d, nnz=%d\n", name, mat->rows, mat->cols, mat->nnz);
    else
        printf("%dx%d, nnz=%d\n", mat->rows, mat->cols, mat->nnz);
    for (int i = 0; i < mat->rows; i++) {
        for (int j = mat->row_ptr[i]; j < mat->row_ptr[i + 1]; j++) {
            printf("  (%d,%d) = %.6g\n", i, mat->col_idx[j], mat->values[j]);
        }
    }
}

int sparse_matrix_get_dims(void *m, int *rows, int *cols) {
    lvSparseMatrix *mat = (lvSparseMatrix *) m;
    if (!mat || !rows || !cols)
        return -1;
    *rows = mat->rows;
    *cols = mat->cols;
    return 0;
}

/* ---- CSR 转稠密辅助（供乘法/求解复用）---- */
static double *sparse_to_dense(const lvSparseMatrix *m) {
    double *d = (double *) lv_calloc((size_t) m->rows * (size_t) m->cols, sizeof(double));
    if (!d)
        return NULL;
    for (int i = 0; i < m->rows; i++) {
        for (int j = m->row_ptr[i]; j < m->row_ptr[i + 1]; j++) {
            d[(size_t) i * (size_t) m->cols + (size_t) m->col_idx[j]] = m->values[j];
        }
    }
    return d;
}

bool sparse_matrix_multiply(void *a, void *b, void **out) {
    if (!out)
        return false;
    *out = NULL;
    lvSparseMatrix *A = (lvSparseMatrix *) a;
    lvSparseMatrix *B = (lvSparseMatrix *) b;
    if (!A || !B || A->cols != B->rows)
        return false;

    double *dA = sparse_to_dense(A);
    double *dB = sparse_to_dense(B);
    if (!dA || !dB) {
        lv_free((void **) &dA);
        lv_free((void **) &dB);
        return false;
    }

    lvSparseMatrix *C = lv_sparse_create(A->rows, B->cols);
    if (!C) {
        lv_free((void **) &dA);
        lv_free((void **) &dB);
        return false;
    }

    /* 稠密累加 C[i][j] += A[i][k] * B[k][j]，避免 lv_sparse_set 覆盖语义 */
    double *dC = (double *) lv_calloc((size_t) A->rows * (size_t) B->cols, sizeof(double));
    if (!dC) {
        lv_free((void **) &dA);
        lv_free((void **) &dB);
        lv_sparse_destroy(C);
        return false;
    }
    for (int i = 0; i < A->rows; i++) {
        for (int k = 0; k < A->cols; k++) {
            double aik = dA[(size_t) i * (size_t) A->cols + (size_t) k];
            if (aik == 0.0)
                continue;
            for (int j = 0; j < B->cols; j++) {
                double bkj = dB[(size_t) k * (size_t) B->cols + (size_t) j];
                if (bkj != 0.0)
                    dC[(size_t) i * (size_t) B->cols + (size_t) j] += aik * bkj;
            }
        }
    }
    for (int i = 0; i < A->rows; i++)
        for (int j = 0; j < B->cols; j++)
            if (dC[(size_t) i * (size_t) B->cols + (size_t) j] != 0.0)
                lv_sparse_set(C, i, j, dC[(size_t) i * (size_t) B->cols + (size_t) j]);
    lv_free((void **) &dC);
    lv_free((void **) &dA);
    lv_free((void **) &dB);
    *out = (void *) C;
    return true;
}

bool sparse_matrix_transpose(void *m, void **out) {
    if (!out)
        return false;
    *out = NULL;
    lvSparseMatrix *M = (lvSparseMatrix *) m;
    if (!M)
        return false;

    lvSparseMatrix *T = lv_sparse_create(M->cols, M->rows);
    if (!T)
        return false;

    for (int i = 0; i < M->rows; i++) {
        for (int j = M->row_ptr[i]; j < M->row_ptr[i + 1]; j++) {
            lv_sparse_set(T, M->col_idx[j], i, M->values[j]);
        }
    }
    *out = (void *) T;
    return true;
}

/* ---- 稠密 LU 分解求解（自包含，避免 L3→L4 依赖 host_linalg）---- */
static bool dense_lu_solve(const double *A, int n, const double *b, double *x) {
    double *lu = (double *) lv_malloc((size_t) n * (size_t) n * sizeof(double));
    if (!lu)
        return false;
    memcpy(lu, A, (size_t) n * (size_t) n * sizeof(double));

    /* 部分主元高斯消元（就地 LU） */
    for (int k = 0; k < n; k++) {
        int pivot = k;
        double maxv = fabs(lu[(size_t) k * n + k]);
        for (int i = k + 1; i < n; i++) {
            double v = fabs(lu[(size_t) i * n + k]);
            if (v > maxv) {
                maxv = v;
                pivot = i;
            }
        }
        if (maxv < lv_EPSILON_ULTRA) {
            lv_free((void **) &lu);
            return false;
        }
        if (pivot != k) {
            for (int j = 0; j < n; j++) {
                double tmp = lu[(size_t) k * n + j];
                lu[(size_t) k * n + j] = lu[(size_t) pivot * n + j];
                lu[(size_t) pivot * n + j] = tmp;
            }
        }
        for (int i = k + 1; i < n; i++) {
            double f = lu[(size_t) i * n + k] / lu[(size_t) k * n + k];
            lu[(size_t) i * n + k] = f;
            for (int j = k + 1; j < n; j++)
                lu[(size_t) i * n + j] -= f * lu[(size_t) k * n + j];
        }
    }

    /* 前代 Ly = b */
    double *y = (double *) lv_malloc((size_t) n * sizeof(double));
    if (!y) {
        lv_free((void **) &lu);
        return false;
    }
    memcpy(y, b, (size_t) n * sizeof(double));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < i; j++)
            y[i] -= lu[(size_t) i * n + j] * y[j];

    /* 回代 Ux = y */
    for (int i = n - 1; i >= 0; i--) {
        double s = y[i];
        for (int j = i + 1; j < n; j++)
            s -= lu[(size_t) i * n + j] * x[j];
        x[i] = s / lu[(size_t) i * n + i];
    }
    lv_free((void **) &lu);
    lv_free((void **) &y);
    return true;
}

/* ---- 稠密 Cholesky 分解求解（对称正定）---- */
static bool dense_cholesky_solve(const double *A, int n, const double *b, double *x) {
    double *L = (double *) lv_calloc((size_t) n * (size_t) n, sizeof(double));
    if (!L)
        return false;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            double s = A[(size_t) i * n + j];
            for (int k = 0; k < j; k++)
                s -= L[(size_t) i * n + k] * L[(size_t) j * n + k];
            if (i == j) {
                if (s <= 0.0) {
                    lv_free((void **) &L);
                    return false;
                }
                L[(size_t) i * n + j] = sqrt(s);
            } else {
                L[(size_t) i * n + j] = s / L[(size_t) j * n + j];
            }
        }
    }
    /* Ly = b, L^T x = y */
    double *y = (double *) lv_malloc((size_t) n * sizeof(double));
    if (!y) {
        lv_free((void **) &L);
        return false;
    }
    for (int i = 0; i < n; i++) {
        double s = b[i];
        for (int j = 0; j < i; j++)
            s -= L[(size_t) i * n + j] * y[j];
        y[i] = s / L[(size_t) i * n + i];
    }
    for (int i = n - 1; i >= 0; i--) {
        double s = y[i];
        for (int j = i + 1; j < n; j++)
            s -= L[(size_t) j * n + i] * x[j];
        x[i] = s / L[(size_t) i * n + i];
    }
    lv_free((void **) &L);
    lv_free((void **) &y);
    return true;
}

/* ---- 稠密 QR（修正 Gram-Schmidt）求解最小二乘/方阵 ---- */
static bool dense_qr_solve(const double *A, int n, const double *b, double *x) {
    double *Q = (double *) lv_calloc((size_t) n * (size_t) n, sizeof(double));
    double *R = (double *) lv_calloc((size_t) n * (size_t) n, sizeof(double));
    if (!Q || !R) {
        lv_free((void **) &Q);
        lv_free((void **) &R);
        return false;
    }
    /* 修正 Gram-Schmidt */
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++)
            Q[(size_t) i * n + j] = A[(size_t) i * n + j];
        for (int k = 0; k < j; k++) {
            double dot = 0.0;
            for (int i = 0; i < n; i++)
                dot += Q[(size_t) i * n + k] * A[(size_t) i * n + j];
            R[(size_t) k * n + j] = dot;
            for (int i = 0; i < n; i++)
                Q[(size_t) i * n + j] -= dot * Q[(size_t) i * n + k];
        }
        double norm = 0.0;
        for (int i = 0; i < n; i++)
            norm += Q[(size_t) i * n + j] * Q[(size_t) i * n + j];
        norm = sqrt(norm);
        if (norm < lv_EPSILON_ULTRA) {
            lv_free((void **) &Q);
            lv_free((void **) &R);
            return false;
        }
        R[(size_t) j * n + j] = norm;
        for (int i = 0; i < n; i++)
            Q[(size_t) i * n + j] /= norm;
    }
    /* Q^T b */
    double *qtb = (double *) lv_malloc((size_t) n * sizeof(double));
    if (!qtb) {
        lv_free((void **) &Q);
        lv_free((void **) &R);
        return false;
    }
    for (int i = 0; i < n; i++) {
        qtb[i] = 0.0;
        for (int j = 0; j < n; j++)
            qtb[i] += Q[(size_t) j * n + i] * b[j];
    }
    /* 回代 Rx = Q^T b */
    for (int i = n - 1; i >= 0; i--) {
        double s = qtb[i];
        for (int j = i + 1; j < n; j++)
            s -= R[(size_t) i * n + j] * x[j];
        x[i] = s / R[(size_t) i * n + i];
    }
    lv_free((void **) &Q);
    lv_free((void **) &R);
    lv_free((void **) &qtb);
    return true;
}

bool sparse_lu_solve(void *m, const double *b, double *x) {
    lvSparseMatrix *M = (lvSparseMatrix *) m;
    if (!M || !b || !x || M->rows != M->cols)
        return false;
    double *d = sparse_to_dense(M);
    if (!d)
        return false;
    bool ok = dense_lu_solve(d, M->rows, b, x);
    lv_free((void **) &d);
    return ok;
}

bool sparse_cholesky_solve(void *m, const double *b, double *x) {
    lvSparseMatrix *M = (lvSparseMatrix *) m;
    if (!M || !b || !x || M->rows != M->cols)
        return false;
    double *d = sparse_to_dense(M);
    if (!d)
        return false;
    bool ok = dense_cholesky_solve(d, M->rows, b, x);
    lv_free((void **) &d);
    return ok;
}

bool sparse_qr_solve(void *m, const double *b, double *x) {
    lvSparseMatrix *M = (lvSparseMatrix *) m;
    if (!M || !b || !x || M->rows != M->cols)
        return false;
    double *d = sparse_to_dense(M);
    if (!d)
        return false;
    bool ok = dense_qr_solve(d, M->rows, b, x);
    lv_free((void **) &d);
    return ok;
}

/* ============================================================================
 * 图 → 稀疏矩阵 / 度数分析 / 半环传播
 * ============================================================================ */

/* graph_to_constraint_matrix：node×node 邻接矩阵（两节点共享活跃约束 → 边）
 * 文档承诺 CSR 格式约束矩阵。 */
bool graph_to_constraint_matrix(const ConstraintGraph *graph, void **out) {
    if (!out)
        return false;
    *out = NULL;
    if (!graph || graph->node_count <= 0)
        return false;

    lvSparseMatrix *m = lv_sparse_create(graph->node_count, graph->node_count);
    if (!m)
        return false;

    for (int c = 0; c < graph->constraint_count; c++) {
        const Constraint *cn = graph->constraints[c];
        if (!cn || !cn->is_active || cn->participant_count < 2)
            continue;
        for (int a = 0; a < cn->participant_count; a++) {
            for (int bb = 0; bb < cn->participant_count; bb++) {
                if (a == bb)
                    continue;
                int pa = cn->participants[a];
                int pb = cn->participants[bb];
                if (pa < 0 || pa >= graph->node_count || pb < 0 || pb >= graph->node_count)
                    continue;
                lv_sparse_set(m, pa, pb, 1.0);
            }
        }
    }
    *out = (void *) m;
    return true;
}

bool graph_degree_analysis(const ConstraintGraph *graph, void **out) {
    if (!out)
        return false;
    *out = NULL;
    if (!graph || graph->node_count <= 0)
        return false;

    lvSparseDegreeAnalysis *a = (lvSparseDegreeAnalysis *) lv_calloc(1, sizeof(lvSparseDegreeAnalysis));
    if (!a)
        return false;

    a->node_count = graph->node_count;
    a->max_degree = 0;
    a->min_degree = 0;
    a->isolated_count = 0;

    /* 度数 = 参与活跃约束的次数（去重：每节点每约束计 1） */
    int *degree = (int *) lv_calloc((size_t) graph->node_count, sizeof(int));
    if (!degree) {
        lv_free((void **) &a);
        return false;
    }
    for (int c = 0; c < graph->constraint_count; c++) {
        const Constraint *cn = graph->constraints[c];
        if (!cn || !cn->is_active)
            continue;
        for (int p = 0; p < cn->participant_count; p++) {
            int nid = cn->participants[p];
            if (nid >= 0 && nid < graph->node_count)
                degree[nid]++;
        }
    }
    double sum = 0.0;
    for (int i = 0; i < graph->node_count; i++) {
        sum += degree[i];
        if (degree[i] > a->max_degree)
            a->max_degree = degree[i];
        if (i == 0 || degree[i] < a->min_degree)
            a->min_degree = degree[i];
        if (degree[i] == 0)
            a->isolated_count++;
    }
    a->avg_degree = graph->node_count > 0 ? sum / graph->node_count : 0.0;
    lv_free((void **) &degree);
    *out = (void *) a;
    return true;
}

void degree_analysis_free(void *p) {
    lv_free((void **) &p);
}

/* semiring_propagate_constraints：邻接矩阵半环乘法不动点迭代。
 * x = A ⊗ x，⊗ 由 semiring 决定；返回迭代次数，-1 未收敛。 */
int semiring_propagate_constraints(const ConstraintGraph *graph, int semiring, double *x, int max_iter) {
    if (!graph || !x || graph->node_count <= 0)
        return -1;
    int n = graph->node_count;
    if (max_iter <= 0)
        max_iter = 1000;

    /* 构建 node×node 邻接（bool 邻接，值 1） */
    bool *adj = (bool *) lv_calloc((size_t) n * (size_t) n, sizeof(bool));
    if (!adj)
        return -1;
    for (int c = 0; c < graph->constraint_count; c++) {
        const Constraint *cn = graph->constraints[c];
        if (!cn || !cn->is_active || cn->participant_count < 2)
            continue;
        for (int a = 0; a < cn->participant_count; a++) {
            for (int bb = 0; bb < cn->participant_count; bb++) {
                if (a == bb)
                    continue;
                int pa = cn->participants[a];
                int pb = cn->participants[bb];
                if (pa >= 0 && pa < n && pb >= 0 && pb < n)
                    adj[(size_t) pa * n + pb] = true;
            }
        }
    }

    double *next = (double *) lv_malloc((size_t) n * sizeof(double));
    if (!next) {
        lv_free((void **) &adj);
        return -1;
    }

    int iter = 0;
    for (; iter < max_iter; iter++) {
        bool changed = false;
        for (int i = 0; i < n; i++) {
            double acc;
            if (semiring == LV_SEMIRING_MIN_PLUS) {
                acc = INFINITY;
                for (int j = 0; j < n; j++)
                    if (adj[(size_t) i * n + j] && x[j] + 1.0 < acc)
                        acc = x[j] + 1.0;
                if (acc == INFINITY)
                    acc = x[i];
            } else if (semiring == LV_SEMIRING_MAX_TIMES) {
                acc = 0.0;
                for (int j = 0; j < n; j++)
                    if (adj[(size_t) i * n + j] && x[j] > acc)
                        acc = x[j];
            } else if (semiring == LV_SEMIRING_OR_AND || semiring == LV_SEMIRING_BOOL) {
                acc = x[i];
                for (int j = 0; j < n; j++)
                    if (adj[(size_t) i * n + j] && x[j] != 0.0)
                        acc = 1.0;
            } else {
                /* PLUS_TIMES / INTERVAL（退化为 PLUS_TIMES） */
                acc = x[i];
                for (int j = 0; j < n; j++)
                    if (adj[(size_t) i * n + j])
                        acc += x[j];
            }
            if (fabs(acc - x[i]) > lv_EPSILON_MEDIUM)
                changed = true;
            next[i] = acc;
        }
        memcpy(x, next, (size_t) n * sizeof(double));
        if (!changed)
            break;
    }
    lv_free((void **) &adj);
    lv_free((void **) &next);
    return iter < max_iter ? iter + 1 : -1;
}
