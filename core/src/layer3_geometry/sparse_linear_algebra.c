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

#include <limits.h>
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

/* ============================================================================
 * 稀疏 LU / Cholesky / QR 求解（真正稀疏实现，替代稠密 dense_*_solve）
 *
 * 设计：以「稀疏行链表」表示消元过程中的矩阵——每行维护有序 (col, value)
 * 动态数组（按列号升序，与 CSR 不变量一致），消元只触碰非零元素，
 * 填充项（fill-in）按需插入，复杂度正比于非零元素而非 n²。
 * ============================================================================ */

/** @brief 稀疏行：有序 (col, value) 对动态数组（升序，容量可增长） */
typedef struct {
    int *cols;
    double *vals;
    int count;
    int capacity;
} SparseRow;

static bool sparse_row_init(SparseRow *r, int cap) {
    if (!r)
        return false;
    r->count = 0;
    r->capacity = cap > 0 ? cap : 4;
    r->cols = (int *) lv_malloc((size_t) r->capacity * sizeof(int));
    r->vals = (double *) lv_malloc((size_t) r->capacity * sizeof(double));
    if (!r->cols || !r->vals) {
        lv_free((void **) &r->cols);
        lv_free((void **) &r->vals);
        r->cols = NULL;
        r->vals = NULL;
        r->capacity = 0;
        return false;
    }
    return true;
}

static void sparse_row_destroy(SparseRow *r) {
    if (!r)
        return;
    lv_free((void **) &r->cols);
    lv_free((void **) &r->vals);
    r->cols = NULL;
    r->vals = NULL;
    r->count = 0;
    r->capacity = 0;
}

/** @brief 稀疏行插入或累加：col 已存在则累加 val，否则有序插入（返回 false 仅 OOM） */
static bool sparse_row_add(SparseRow *r, int col, double val) {
    if (!r || col < 0)
        return false;
    /* 查找插入位置（升序二分；行内元素数通常少，线性扫描亦可） */
    int lo = 0, hi = r->count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (r->cols[mid] < col)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < r->count && r->cols[lo] == col) {
        r->vals[lo] += val;
        return true;
    }
    if (r->count >= r->capacity) {
        /* F27/I3：手写倍增迁 lv_ensure_capacity64 权威（size_t 口径，
         * 补原实现缺失的溢出检查：capacity*2 与分配大小） */
        size_t cap64 = (size_t) r->capacity;
        if (!lv_ensure_capacity64((void **) &r->cols, (size_t) r->count + 1, &cap64, sizeof(int), 1)) {
            lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "sparse row grow: cols overflow");
        }
        double *nv = (double *) lv_realloc(r->vals, cap64 * sizeof(double));
        if (!nv) {
            return false;
        }
        r->vals = nv;
        r->capacity = (int) cap64;
    }
    for (int i = r->count; i > lo; i--) {
        r->cols[i] = r->cols[i - 1];
        r->vals[i] = r->vals[i - 1];
    }
    r->cols[lo] = col;
    r->vals[lo] = val;
    r->count++;
    return true;
}

/** @brief 稀疏行读取：col 存在返回指针，否则 NULL（升序二分） */
static double *sparse_row_get(SparseRow *r, int col) {
    if (!r)
        return NULL;
    int lo = 0, hi = r->count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (r->cols[mid] < col)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < r->count && r->cols[lo] == col)
        return &r->vals[lo];
    return NULL;
}

/** @brief 稀疏行清空所有元素（用于主元行重置后的重新组装） */
static void sparse_row_clear(SparseRow *r) {
    if (r)
        r->count = 0;
}

/** @brief 把 CSR 矩阵加载为稀疏行数组（调用者负责 destroy 每行与数组） */
static SparseRow *sparse_rows_load(const lvSparseMatrix *m, int *out_n) {
    if (!m || !out_n)
        return NULL;
    int n = m->rows;
    SparseRow *rows = (SparseRow *) lv_calloc((size_t) n, sizeof(SparseRow));
    if (!rows)
        return NULL;
    for (int i = 0; i < n; i++) {
        int cnt = m->row_ptr[i + 1] - m->row_ptr[i];
        if (!sparse_row_init(&rows[i], cnt > 0 ? cnt : 4)) {
            for (int k = 0; k <= i; k++)
                sparse_row_destroy(&rows[k]);
            lv_free((void **) &rows);
            return NULL;
        }
        for (int j = m->row_ptr[i]; j < m->row_ptr[i + 1]; j++)
            rows[i].cols[rows[i].count] = m->col_idx[j],
            rows[i].vals[rows[i].count] = m->values[j], rows[i].count++;
    }
    *out_n = n;
    return rows;
}

static void sparse_rows_free(SparseRow *rows, int n) {
    if (!rows)
        return;
    for (int i = 0; i < n; i++)
        sparse_row_destroy(&rows[i]);
    lv_free((void **) &rows);
}

/** @brief 稀疏 LU 分解求解（部分主元）：A x = b，返回 x。
 * 消元过程只维护稀疏行；行交换用主元行覆盖，填充项动态插入。 */
static bool sparse_lu_solve_impl(const lvSparseMatrix *M, const double *b, double *x) {
    int n = M->rows;
    SparseRow *rows = sparse_rows_load(M, &n);
    if (!rows)
        return false;
    if (n == 0) {
        sparse_rows_free(rows, n);
        return false;
    }

    /* 部分主元消元：对每列 k，找该列非零且绝对值最大的行作主元 */
    /* 置换跟踪：行交换时同步交换 b 的对应项（前代 Ly = Pb 直接对
     * 交换后的 b 计算，无需额外置换向量） */
    double *pb = (double *) lv_malloc((size_t) n * sizeof(double));
    if (!pb) {
        sparse_rows_free(rows, n);
        return false;
    }
    memcpy(pb, b, (size_t) n * sizeof(double));

    for (int k = 0; k < n; k++) {
        int pivot = -1;
        double maxv = 0.0;
        for (int i = k; i < n; i++) {
            double *pv = sparse_row_get(&rows[i], k);
            if (pv && fabs(*pv) > maxv) {
                maxv = fabs(*pv);
                pivot = i;
            }
        }
        if (pivot < 0 || maxv < lv_EPSILON_ULTRA) {
            lv_free((void **) &pb);
            sparse_rows_free(rows, n);
            return false; /* 奇异 */
        }
        if (pivot != k) {
            /* 交换行 k 与 pivot（同时交换 b 对应项保持置换一致） */
            SparseRow tmp = rows[k];
            rows[k] = rows[pivot];
            rows[pivot] = tmp;
            double tb = pb[k];
            pb[k] = pb[pivot];
            pb[pivot] = tb;
        }

        /* 消去第 k 列下方的非零元素（稀疏遍历） */
        for (int i = k + 1; i < n; i++) {
            double *aik = sparse_row_get(&rows[i], k);
            if (!aik || fabs(*aik) < lv_EPSILON_ULTRA)
                continue;
            double factor = *aik / *sparse_row_get(&rows[k], k);
            /* 行 i ← 行 i − factor × 行 k（仅遍历行 k 的非零列） */
            for (int t = 0; t < rows[k].count; t++) {
                int ck = rows[k].cols[t];
                double vk = rows[k].vals[t];
                if (ck < k)
                    continue; /* 主元行 k 在 ck<k 处是 L 部分（已消元），跳过 */
                double *av = sparse_row_get(&rows[i], ck);
                if (av)
                    *av -= factor * vk;
                else {
                    if (!sparse_row_add(&rows[i], ck, -factor * vk)) {
                        lv_free((void **) &pb);
                        sparse_rows_free(rows, n);
                        return false;
                    }
                }
            }
            /* 置 L 元素（第 k 列主元下方）= factor（U 部分该位为 0） */
            *aik = factor;
        }
    }

    /* 前代 Ly = Pb（L 为单位下三角，对角线隐含 1；b 已按行交换置换） */
    double *y = (double *) lv_calloc((size_t) n, sizeof(double));
    if (!y) {
        lv_free((void **) &pb);
        sparse_rows_free(rows, n);
        return false;
    }
    for (int i = 0; i < n; i++) {
        double s = pb[i];
        for (int t = 0; t < rows[i].count; t++) {
            int c = rows[i].cols[t];
            if (c < i)
                s -= rows[i].vals[t] * y[c];
        }
        y[i] = s;
    }

    /* 回代 Ux = y（U 对角线为行 k 的 k 列元素） */
    for (int i = n - 1; i >= 0; i--) {
        double diag = 0.0;
        double s = y[i];
        for (int t = 0; t < rows[i].count; t++) {
            int c = rows[i].cols[t];
            double v = rows[i].vals[t];
            if (c == i) {
                diag = v;
            } else if (c > i) {
                s -= v * x[c];
            }
        }
        if (fabs(diag) < lv_EPSILON_ULTRA) {
            lv_free((void **) &y);
            lv_free((void **) &pb);
            sparse_rows_free(rows, n);
            return false;
        }
        x[i] = s / diag;
    }

    lv_free((void **) &y);
    lv_free((void **) &pb);
    sparse_rows_free(rows, n);
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

/* ---- 稠密 Cholesky 分解求解（对称正定；稀疏路径回退时使用） ---- */
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

/* ---- 稠密 QR（修正 Gram-Schmidt）求解最小二乘/方阵；稀疏路径回退时使用 ---- */
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

/** @brief 稀疏 Cholesky 分解求解（对称正定）：A x = b。
 * 只处理下三角非零（i >= j）；L 行按稀疏行存储，填充项动态插入。 */
static bool sparse_cholesky_solve_impl(const lvSparseMatrix *M, const double *b, double *x) {
    int n = M->rows;
    SparseRow *rows = sparse_rows_load(M, &n);
    if (!rows)
        return false;
    if (n == 0) {
        sparse_rows_free(rows, n);
        return false;
    }

    /* 只保留每行下三角部分（col <= row）；上三角对称元素忽略 */
    for (int i = 0; i < n; i++) {
        int w = 0;
        for (int t = 0; t < rows[i].count; t++) {
            if (rows[i].cols[t] <= i) {
                rows[i].cols[w] = rows[i].cols[t];
                rows[i].vals[w] = rows[i].vals[t];
                w++;
            }
        }
        rows[i].count = w;
    }

    /* 稀疏 Cholesky：L[i][j] = (A[i][j] − Σ_{k<j} L[i][k]L[j][k]) / L[j][j] */
    for (int j = 0; j < n; j++) {
        /* 计算 L[j][j]（需列 j 上所有 i>=j 的 A[i][j]，但按行存下三角时
         * A[i][j] 在行 i 中。逐行累加 j 列的贡献：行 i 中 col==j 的项） */
        double ljj = 0.0;
        for (int i = j; i < n; i++) {
            double *aij = sparse_row_get(&rows[i], j);
            if (!aij)
                continue;
            /* Σ_{k<j} L[i][k] L[j][k] */
            double dot = 0.0;
            for (int t = 0; t < rows[i].count; t++) {
                int ck = rows[i].cols[t];
                if (ck >= j)
                    break;
                double *lj = sparse_row_get(&rows[j], ck);
                if (lj)
                    dot += rows[i].vals[t] * (*lj);
            }
            double val = *aij - dot;
            if (i == j) {
                if (val <= 0.0) {
                    sparse_rows_free(rows, n);
                    return false; /* 非正定 */
                }
                ljj = sqrt(val);
                /* 行 j 对角线更新为 L[j][j] */
                *aij = ljj;
            } else {
                if (fabs(ljj) < lv_EPSILON_ULTRA) {
                    sparse_rows_free(rows, n);
                    return false;
                }
                /* 行 i 的 col j 更新为 L[i][j]（覆盖 A[i][j]） */
                *aij = val / ljj;
            }
        }
        if (fabs(ljj) < lv_EPSILON_ULTRA) {
            sparse_rows_free(rows, n);
            return false;
        }
    }

    /* 前代 Ly = b（L 下三角，含对角） */
    double *y = (double *) lv_calloc((size_t) n, sizeof(double));
    if (!y) {
        sparse_rows_free(rows, n);
        return false;
    }
    for (int i = 0; i < n; i++) {
        double s = b[i];
        for (int t = 0; t < rows[i].count; t++) {
            int c = rows[i].cols[t];
            if (c < i)
                s -= rows[i].vals[t] * y[c];
        }
        double *diag = sparse_row_get(&rows[i], i);
        if (!diag || fabs(*diag) < lv_EPSILON_ULTRA) {
            lv_free((void **) &y);
            sparse_rows_free(rows, n);
            return false;
        }
        y[i] = s / *diag;
    }

    /* 回代 L^T x = y：x[i] = (y[i] − Σ_{k>i} L[k][i] x[k]) / L[i][i] */
    for (int i = n - 1; i >= 0; i--) {
        double s = y[i];
        for (int k = i + 1; k < n; k++) {
            double *lki = sparse_row_get(&rows[k], i);
            if (lki)
                s -= (*lki) * x[k];
        }
        double *diag = sparse_row_get(&rows[i], i);
        if (!diag || fabs(*diag) < lv_EPSILON_ULTRA) {
            lv_free((void **) &y);
            sparse_rows_free(rows, n);
            return false;
        }
        x[i] = s / *diag;
    }

    lv_free((void **) &y);
    sparse_rows_free(rows, n);
    return true;
}

/** @brief 稀疏 QR 求解（方阵，Givens 旋转逐列消去）。
 * 逐列对行 i>k 应用 Givens 旋转消去 A[i][k]；旋转保持稀疏性（只触碰
 * 参与旋转的两行的非零列并集）。最终 R 上三角，回代解 Rx = Q^T b
 * （Q^T b 经相同旋转累积）。 */
static bool sparse_qr_solve_impl(const lvSparseMatrix *M, const double *b, double *x) {
    int n = M->rows;
    SparseRow *rows = sparse_rows_load(M, &n);
    if (!rows)
        return false;
    if (n == 0) {
        sparse_rows_free(rows, n);
        return false;
    }

    double *qtb = (double *) lv_malloc((size_t) n * sizeof(double));
    if (!qtb) {
        sparse_rows_free(rows, n);
        return false;
    }
    memcpy(qtb, b, (size_t) n * sizeof(double));

    /* Givens 消去：对每列 k，消去行 k+1..n-1 的 A[i][k] */
    for (int k = 0; k < n; k++) {
        for (int i = k + 1; i < n; i++) {
            double *aik = sparse_row_get(&rows[i], k);
            if (!aik || fabs(*aik) < lv_EPSILON_ULTRA)
                continue;
            double *akk = sparse_row_get(&rows[k], k);
            if (!akk || fabs(*akk) < lv_EPSILON_ULTRA) {
                lv_free((void **) &qtb);
                sparse_rows_free(rows, n);
                return false; /* 秩亏 */
            }
            double a = *akk, c_a = *aik;
            double r = sqrt(a * a + c_a * c_a);
            double cs = a / r, sn = c_a / r;

            /* 旋转行 k 与行 i：仅遍历两行非零列的并集。
             * 先收集两行列号（有序归并），再应用旋转。 */
            /* 应用旋转到两行：用临时累加避免遍历时修改行结构 */
            SparseRow nk, ni;
            if (!sparse_row_init(&nk, rows[k].count + rows[i].count + 4) ||
                !sparse_row_init(&ni, rows[k].count + rows[i].count + 4)) {
                sparse_row_destroy(&nk);
                sparse_row_destroy(&ni);
                lv_free((void **) &qtb);
                sparse_rows_free(rows, n);
                return false;
            }
            int p = 0, q = 0;
            while (p < rows[k].count || q < rows[i].count) {
                int ck = p < rows[k].count ? rows[k].cols[p] : INT_MAX;
                int ci = q < rows[i].count ? rows[i].cols[q] : INT_MAX;
                int c = ck < ci ? ck : ci;
                double vk = (ck == c && p < rows[k].count) ? rows[k].vals[p] : 0.0;
                double vi = (ci == c && q < rows[i].count) ? rows[i].vals[q] : 0.0;
                if (ck == c && p < rows[k].count)
                    p++;
                if (ci == c && q < rows[i].count)
                    q++;
                if (fabs(vk) < lv_EPSILON_ULTRA && fabs(vi) < lv_EPSILON_ULTRA)
                    continue;
                double nv = cs * vk + sn * vi;
                double nw = -sn * vk + cs * vi;
                if (fabs(nv) > lv_EPSILON_ULTRA)
                    sparse_row_add(&nk, c, nv);
                if (fabs(nw) > lv_EPSILON_ULTRA)
                    sparse_row_add(&ni, c, nw);
            }
            sparse_row_destroy(&rows[k]);
            sparse_row_destroy(&rows[i]);
            rows[k] = nk;
            rows[i] = ni;
            /* Q^T b 同步旋转 */
            double bk = qtb[k], bi = qtb[i];
            qtb[k] = cs * bk + sn * bi;
            qtb[i] = -sn * bk + cs * bi;
        }
        /* 列 k 对角线必须非零（数值容差） */
        double *dk = sparse_row_get(&rows[k], k);
        if (!dk || fabs(*dk) < lv_EPSILON_ULTRA) {
            lv_free((void **) &qtb);
            sparse_rows_free(rows, n);
            return false;
        }
    }

    /* 回代 Rx = Q^T b */
    for (int i = n - 1; i >= 0; i--) {
        double diag = 0.0;
        double s = qtb[i];
        for (int t = 0; t < rows[i].count; t++) {
            int c = rows[i].cols[t];
            double v = rows[i].vals[t];
            if (c == i) {
                diag = v;
            } else if (c > i) {
                s -= v * x[c];
            }
        }
        if (fabs(diag) < lv_EPSILON_ULTRA) {
            lv_free((void **) &qtb);
            sparse_rows_free(rows, n);
            return false;
        }
        x[i] = s / diag;
    }

    lv_free((void **) &qtb);
    sparse_rows_free(rows, n);
    return true;
}

bool sparse_lu_solve(void *m, const double *b, double *x) {
    lvSparseMatrix *M = (lvSparseMatrix *) m;
    if (!M || !b || !x || M->rows != M->cols)
        return false;
    /* 真正稀疏 LU（稀疏行消元，仅触碰非零元素） */
    if (sparse_lu_solve_impl(M, b, x))
        return true;
    /* 回退：稀疏路径失败（奇异/内存）时用稠密 LU 二次确认（语义与旧一致） */
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
    if (sparse_cholesky_solve_impl(M, b, x))
        return true;
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
    if (sparse_qr_solve_impl(M, b, x))
        return true;
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
