/**

 * @file sparse_linear_algebra.c

 * @brief 绋€鐤忕嚎鎬т唬鏁板悗绔殑妗╁疄鐜?鈥斺€?SuiteSparse/GraphBLAS 鍙傝€? */



#include "lv00/sparse_linear_algebra.h"


#include <float.h>

#include <math.h>

#include <stdio.h>

#include <stdlib.h>

#include <string.h>


#include "lv00/constraint_graph.h"



/* ========================================================================

 * 鍐呴儴杈呭姪瀹? * ======================================================================== */



#define LV00_MAX(a, b) ((a) > (b) ? (a) : (b))

#define LV00_MIN(a, b) ((a) < (b) ? (a) : (b))



/* ========================================================================

 * 绋€鐤忕煩闃电敓鍛藉懆鏈? * ======================================================================== */



SparseMatrix *sparse_matrix_create(int rows, int cols, SparseFormat fmt) {

    if (rows < 0 || cols < 0) {

        return NULL;

    }



    SparseMatrix *mat = (SparseMatrix *) lv00_calloc(1, sizeof(SparseMatrix));

    if (!mat) {

        return NULL;

    }



    mat->rows = rows;

    mat->cols = cols;

    mat->nnz = 0;

    mat->fmt = fmt;

    mat->row_ptr = NULL;

    mat->col_idx = NULL;

    mat->values = NULL;

    mat->owns_data = false;



    return mat;

}



void sparse_matrix_destroy(SparseMatrix *mat) {

    if (!mat) {

        return;

    }



    if (mat->owns_data) {

        lv00_free((void **) &mat->row_ptr);

        lv00_free((void **) &mat->col_idx);

        lv00_free((void **) &mat->values);

    }



    lv00_free((void **) &mat);

}



SparseMatrix *sparse_matrix_clone(const SparseMatrix *mat) {

    if (!mat) {

        return NULL;

    }



    SparseMatrix *clone = sparse_matrix_create(mat->rows, mat->cols, mat->fmt);

    if (!clone) {

        return NULL;

    }



    clone->nnz = mat->nnz;



    if (mat->nnz > 0) {

        /* 鍒嗛厤鍐呴儴鏁扮粍 */

        if (mat->fmt == SPARSE_CSR || mat->fmt == SPARSE_CSC) {

            clone->row_ptr = (int *) lv00_calloc((size_t) mat->rows + 1, sizeof(int));

            if (!clone->row_ptr) {

                sparse_matrix_destroy(clone);

                return NULL;

            }

            memcpy(clone->row_ptr, mat->row_ptr, ((size_t) mat->rows + 1) * sizeof(int));

        }



        clone->col_idx = (int *) lv00_calloc((size_t) mat->nnz, sizeof(int));

        if (!clone->col_idx) {

            sparse_matrix_destroy(clone);

            return NULL;

        }

        memcpy(clone->col_idx, mat->col_idx, (size_t) mat->nnz * sizeof(int));



        if (mat->values) {

            clone->values = (double *) lv00_calloc((size_t) mat->nnz, sizeof(double));

            if (!clone->values) {

                sparse_matrix_destroy(clone);

                return NULL;

            }

            memcpy(clone->values, mat->values, (size_t) mat->nnz * sizeof(double));

        }

    }



    clone->owns_data = true;

    return clone;

}



void sparse_matrix_print(const SparseMatrix *mat, const char *name) {

    if (!mat) {

        fprintf(stderr, "[sparse_matrix_print] mat is NULL\n");

        return;

    }



    const char *fmt_names[] = {"CSR", "CSC", "COO", "DENSE"};

    const char *fmt_str = (mat->fmt >= 0 && mat->fmt <= 3) ? fmt_names[mat->fmt] : "UNKNOWN";



    fprintf(stderr, "--- SparseMatrix %s ---\n", name ? name : "(unnamed)");

    fprintf(stderr, "  rows=%d, cols=%d, nnz=%d, fmt=%s\n", mat->rows, mat->cols, mat->nnz, fmt_str);



    if (mat->nnz == 0) {

        fprintf(stderr, "  (empty matrix)\n");

        return;

    }



    if (mat->fmt == SPARSE_CSR && mat->row_ptr && mat->col_idx) {

        int max_rows = (mat->rows > 10) ? 10 : mat->rows;

        for (int i = 0; i < max_rows; i++) {

            fprintf(stderr, "  row[%d]: ", i);

            for (int k = mat->row_ptr[i]; k < mat->row_ptr[i + 1]; k++) {

                if (mat->values) {

                    fprintf(stderr, "(%d,%.4g) ", mat->col_idx[k], mat->values[k]);

                } else {

                    fprintf(stderr, "(%d) ", mat->col_idx[k]);

                }

            }

            fprintf(stderr, "\n");

        }

        if (mat->rows > 10) {

            fprintf(stderr, "  ... (%d more rows)\n", mat->rows - 10);

        }

    }

}



/* ========================================================================

 * 鍗婄幆瀹炵幇

 * ======================================================================== */



/* --- 鍐呴儴闈欐€佽繍绠楀嚱鏁?--- */



static double add_plus(double a, double b) {

    return a + b;

}

static double mul_times(double a, double b) {

    return a * b;

}

static double add_min(double a, double b) {

    return fmin(a, b);

}

static double add_max(double a, double b) {

    return fmax(a, b);

}

static double mul_plus(double a, double b) {

    return a + b;

}

static double add_or(double a, double b) {

    return (a != 0.0 || b != 0.0) ? 1.0 : 0.0;

}

static double mul_and(double a, double b) {

    return (a != 0.0 && b != 0.0) ? 1.0 : 0.0;

}

static double add_interval(double a, double b) {

    return fmax(a, b);

}

static double mul_interval(double a, double b) {

    return (a + b);

}



Semiring semiring_create(SemiringType type) {

    Semiring sr;

    memset(&sr, 0, sizeof(sr));

    sr.type = type;



    switch (type) {

        case SEMIRING_PLUS_TIMES:

            sr.add_op = add_plus;

            sr.mul_op = mul_times;

            sr.add_identity = 0.0;

            sr.mul_identity = 1.0;

            sr.name = "plus-times";

            break;

        case SEMIRING_MIN_PLUS:

            sr.add_op = add_min;

            sr.mul_op = mul_plus;

            sr.add_identity = DBL_MAX;

            sr.mul_identity = 0.0;

            sr.name = "min-plus";

            break;

        case SEMIRING_MAX_TIMES:

            sr.add_op = add_max;

            sr.mul_op = mul_times;

            sr.add_identity = -DBL_MAX;

            sr.mul_identity = 1.0;

            sr.name = "max-times";

            break;

        case SEMIRING_OR_AND:

        case SEMIRING_BOOL:

            sr.add_op = add_or;

            sr.mul_op = mul_and;

            sr.add_identity = 0.0;

            sr.mul_identity = 1.0;

            sr.name = "or-and";

            break;

        case SEMIRING_INTERVAL:

            sr.add_op = add_interval;

            sr.mul_op = mul_interval;

            sr.add_identity = -DBL_MAX;

            sr.mul_identity = 0.0;

            sr.name = "interval";

            break;

        default:

            /* fallback to plus-times */

            sr.add_op = add_plus;

            sr.mul_op = mul_times;

            sr.add_identity = 0.0;

            sr.mul_identity = 1.0;

            sr.name = "plus-times (default)";

            break;

    }



    return sr;

}



/* ========================================================================

 * 绾︽潫浼犳挱锛氬崐鐜笂鐨勪笉鍔ㄧ偣杩唬

 * ======================================================================== */



static bool semiring_vector_equal(const double *a, const double *b, int n) {

    for (int i = 0; i < n; i++) {

        if (fabs(a[i] - b[i]) > 1e-12) {

            return false;

        }

    }

    return true;

}



static void semiring_matvec(const SparseMatrix *A, const Semiring *sr, const double *x, double *y) {

    /* y = A 鈯?x锛屽崐鐜煩闃?鍚戦噺涔樻硶 */

    /* 瀵规瘡涓妭鐐癸紙琛岋級锛岄亶鍘嗚琛岀殑鎵€鏈夐潪闆跺厓绱?*/

    int n = graph_get_node_count((const ConstraintGraph *) NULL); /* unused param, avoid warning */

    (void) n;



    for (int i = 0; i < A->rows; i++) {

        double acc = sr->add_identity;

        int row_start = A->row_ptr[i];

        int row_end = A->row_ptr[i + 1];



        for (int k = row_start; k < row_end; k++) {

            int j = A->col_idx[k];

            double aval = (A->values) ? A->values[k] : 1.0;

            double prod = sr->mul_op(aval, x[j]);

            acc = sr->add_op(acc, prod);

        }

        y[i] = acc;

    }

}



int semiring_propagate_constraints(ConstraintGraph *g, SemiringType semiring, double *x, int max_iter) {

    if (!g || !x) {

        return -1;

    }



    /* 鏋勫缓绾︽潫鐭╅樀 */

    SparseMatrix *A = NULL;

    if (!graph_to_constraint_matrix(g, &A)) {

        return -1;

    }



    Semiring sr = semiring_create(semiring);



    int node_count = graph_get_node_count(g);

    double *x_new = (double *) lv00_calloc((size_t) node_count, sizeof(double));

    if (!x_new) {

        sparse_matrix_destroy(A);

        return -1;

    }



    if (max_iter <= 0) {

        max_iter = 1000;

    }



    int iter;

    for (iter = 0; iter < max_iter; iter++) {

        /* x_new = A 鈯?x */

        semiring_matvec(A, &sr, x, x_new);



        /* 妫€鏌ヤ笉鍔ㄧ偣 */

        if (semiring_vector_equal(x, x_new, node_count)) {

            /* 宸茶揪涓嶅姩鐐癸紝灏?x_new 鎷疯礉鍥?x */

            memcpy(x, x_new, (size_t) node_count * sizeof(double));

            lv00_free((void **) &x_new);

            sparse_matrix_destroy(A);

            return iter + 1;

        }



        /* 鏇存柊 x = x_new */

        memcpy(x, x_new, (size_t) node_count * sizeof(double));

    }



    lv00_free((void **) &x_new);

    sparse_matrix_destroy(A);

    return -1; /* 鏈敹鏁?*/

}



/* ========================================================================

 * 绾︽潫鍥?鈫?绋€鐤忕煩闃佃浆鎹? * ======================================================================== */



bool graph_to_constraint_matrix(const ConstraintGraph *graph, SparseMatrix **mat) {

    if (!graph || !mat) {

        return false;

    }



    int n_rows = graph->constraint_count;

    int n_cols = graph->node_count;



    if (n_rows == 0 || n_cols == 0) {

        *mat = sparse_matrix_create(n_rows, n_cols, SPARSE_CSR);

        return (*mat != NULL);

    }



    /* 绗竴閬嶏細缁熻姣忚鐨勯潪闆跺厓绱犳暟閲?*/

    int *row_nnz = (int *) lv00_calloc((size_t) n_rows, sizeof(int));

    if (!row_nnz) {

        return false;

    }



    for (int i = 0; i < n_rows; i++) {

        const Constraint *c = graph->constraints[i];

        if (!c)

            continue;

        row_nnz[i] = c->participant_count;

    }



    /* 鍒嗛厤 CSR 缁撴瀯 */

    SparseMatrix *csr = sparse_matrix_create(n_rows, n_cols, SPARSE_CSR);

    if (!csr) {

        lv00_free((void **) &row_nnz);

        return false;

    }



    int total_nnz = 0;

    for (int i = 0; i < n_rows; i++) {

        total_nnz += row_nnz[i];

    }



    csr->nnz = total_nnz;

    csr->row_ptr = (int *) lv00_calloc((size_t) n_rows + 1, sizeof(int));

    csr->col_idx = (int *) lv00_calloc((size_t) total_nnz, sizeof(int));

    csr->values = (double *) lv00_calloc((size_t) total_nnz, sizeof(double));

    csr->owns_data = true;



    if (!csr->row_ptr || !csr->col_idx || !csr->values) {

        lv00_free((void **) &row_nnz);

        sparse_matrix_destroy(csr);

        return false;

    }



    /* 鏋勫缓琛屾寚閽?*/

    csr->row_ptr[0] = 0;

    for (int i = 0; i < n_rows; i++) {

        csr->row_ptr[i + 1] = csr->row_ptr[i] + row_nnz[i];

    }



    /* 绗簩閬嶏細濉厖鍒楃储寮曞拰鏁板€?*/

    for (int i = 0; i < n_rows; i++) {

        const Constraint *c = graph->constraints[i];

        if (!c)

            continue;



        int offset = csr->row_ptr[i];

        for (int p = 0; p < c->participant_count; p++) {

            csr->col_idx[offset + p] = c->participants[p];



            /* 鏍规嵁绾︽潫绫诲瀷璁剧疆鏁板€?*/

            switch (c->type) {

                case INCIDENCE:

                case CONNECTION:

                    csr->values[offset + p] = 1.0;

                    break;

                case BETWEENNESS:

                    /* -1, 2, -1 妯″紡 */

                    if (p == 1) {

                        csr->values[offset + p] = 2.0;

                    } else {

                        csr->values[offset + p] = -1.0;

                    }

                    break;

                case INTERSECTION:

                case CONTAINMENT:

                    csr->values[offset + p] = 1.0;

                    break;

                default:

                    csr->values[offset + p] = 1.0;

                    break;

            }

        }

    }



    lv00_free((void **) &row_nnz);

    *mat = csr;

    return true;

}



/* ========================================================================

 * 绋€鐤忕煩闃典箻娉曪紙CSR 涓夐噸寰幆锛? * ======================================================================== */



bool sparse_matrix_multiply(const SparseMatrix *A, const SparseMatrix *B, SparseMatrix **C) {

    if (!A || !B || !C) {

        return false;

    }

    if (A->cols != B->rows) {

        return false;

    }

    if (A->fmt != SPARSE_CSR || B->fmt != SPARSE_CSR) {

        return false;

    }



    int m = A->rows;

    int n = B->cols;



    /* 宸ヤ綔鏁扮粍锛氱敤浜庢瘡琛岀疮鍔犳椂鐨勫垪鏍囪 */

    double *work = (double *) lv00_calloc((size_t) n, sizeof(double));

    if (!work) {

        return false;

    }



    /* 绗竴閬嶏細缁熻 C 姣忚鐨勯潪闆跺厓绱犳暟 */

    int *c_row_nnz = (int *) lv00_calloc((size_t) m, sizeof(int));

    if (!c_row_nnz) {

        lv00_free((void **) &work);

        return false;

    }



    for (int i = 0; i < m; i++) {

        for (int k = A->row_ptr[i]; k < A->row_ptr[i + 1]; k++) {

            int j = A->col_idx[k];

            double aval = (A->values) ? A->values[k] : 1.0;



            for (int l = B->row_ptr[j]; l < B->row_ptr[j + 1]; l++) {

                int col = B->col_idx[l];

                double old = work[col];

                double bval = (B->values) ? B->values[l] : 1.0;

                work[col] = old + aval * bval;



                if (fabs(old) < 1e-15 && fabs(work[col]) >= 1e-15) {

                    c_row_nnz[i]++;

                }

            }

        }

        /* 閲嶇疆宸ヤ綔鏁扮粍 */

        for (int k = A->row_ptr[i]; k < A->row_ptr[i + 1]; k++) {

            int j = A->col_idx[k];

            for (int l = B->row_ptr[j]; l < B->row_ptr[j + 1]; l++) {

                work[B->col_idx[l]] = 0.0;

            }

        }

    }



    /* 鏋勫缓 C */

    SparseMatrix *csr = sparse_matrix_create(m, n, SPARSE_CSR);

    if (!csr) {

        lv00_free((void **) &work);

        lv00_free((void **) &c_row_nnz);

        return false;

    }



    csr->row_ptr = (int *) lv00_calloc((size_t) m + 1, sizeof(int));

    csr->row_ptr[0] = 0;

    int total_nnz = 0;

    for (int i = 0; i < m; i++) {

        total_nnz += c_row_nnz[i];

        csr->row_ptr[i + 1] = total_nnz;

    }



    csr->nnz = total_nnz;

    csr->col_idx = (int *) lv00_calloc((size_t) total_nnz, sizeof(int));

    csr->values = (double *) lv00_calloc((size_t) total_nnz, sizeof(double));

    csr->owns_data = true;



    if ((total_nnz > 0) && (!csr->col_idx || !csr->values)) {

        lv00_free((void **) &work);

        lv00_free((void **) &c_row_nnz);

        sparse_matrix_destroy(csr);

        return false;

    }



    /* 绗簩閬嶏細濉厖 C 鐨勬暟鍊?*/

    for (int i = 0; i < m; i++) {

        int offset = csr->row_ptr[i];

        int cur = 0;



        for (int k = A->row_ptr[i]; k < A->row_ptr[i + 1]; k++) {

            int j = A->col_idx[k];

            double aval = (A->values) ? A->values[k] : 1.0;



            for (int l = B->row_ptr[j]; l < B->row_ptr[j + 1]; l++) {

                int col = B->col_idx[l];

                double bval = (B->values) ? B->values[l] : 1.0;

                work[col] += aval * bval;

            }

        }



        /* 灏嗘湰琛岄潪闆跺厓绱犳敹闆嗗埌 C */

        for (int k = A->row_ptr[i]; k < A->row_ptr[i + 1]; k++) {

            int j = A->col_idx[k];

            for (int l = B->row_ptr[j]; l < B->row_ptr[j + 1]; l++) {

                int col = B->col_idx[l];

                if (fabs(work[col]) >= 1e-15) {

                    csr->col_idx[offset + cur] = col;

                    csr->values[offset + cur] = work[col];

                    cur++;

                    work[col] = 0.0; /* 鏍囪宸插鐞?*/

                }

            }

        }

    }



    lv00_free((void **) &work);

    lv00_free((void **) &c_row_nnz);

    *C = csr;

    return true;

}



/* ========================================================================

 * 绋€鐤忕煩闃佃浆缃? * ======================================================================== */



bool sparse_matrix_transpose(const SparseMatrix *mat, SparseMatrix **out) {

    if (!mat || !out) {

        return false;

    }

    if (mat->fmt != SPARSE_CSR && mat->fmt != SPARSE_CSC) {

        return false;

    }



    int m = mat->rows;

    int n = mat->cols;



    SparseMatrix *t = sparse_matrix_create(n, m, mat->fmt);

    if (!t) {

        return false;

    }



    t->nnz = mat->nnz;

    t->row_ptr = (int *) lv00_calloc((size_t) n + 1, sizeof(int));

    t->col_idx = (int *) lv00_calloc((size_t) mat->nnz, sizeof(int));

    t->values = NULL;

    t->owns_data = true;



    if (!t->row_ptr || !t->col_idx) {

        sparse_matrix_destroy(t);

        return false;

    }



    if (mat->values) {

        t->values = (double *) lv00_calloc((size_t) mat->nnz, sizeof(double));

        if (!t->values) {

            sparse_matrix_destroy(t);

            return false;

        }

    }



    /* 璁℃暟鎺掑簭锛氱粺璁℃瘡鍒楋紙杞疆鍚庣殑琛岋級鐨勯潪闆跺厓鏁伴噺 */

    int *col_counts = (int *) lv00_calloc((size_t) n, sizeof(int));

    if (!col_counts) {

        sparse_matrix_destroy(t);

        return false;

    }



    for (int k = 0; k < mat->nnz; k++) {

        col_counts[mat->col_idx[k]]++;

    }



    /* 鏋勫缓杞疆琛屾寚閽?*/

    t->row_ptr[0] = 0;

    for (int i = 0; i < n; i++) {

        t->row_ptr[i + 1] = t->row_ptr[i] + col_counts[i];

    }



    /* 濉厖杞疆鍚庣殑鍒楃储寮曞拰鍊?*/

    int *offset = (int *) lv00_calloc((size_t) n, sizeof(int));

    if (!offset) {

        lv00_free((void **) &col_counts);

        sparse_matrix_destroy(t);

        return false;

    }



    for (int i = 0; i < m; i++) {

        for (int k = mat->row_ptr[i]; k < mat->row_ptr[i + 1]; k++) {

            int col = mat->col_idx[k];

            int dest = t->row_ptr[col] + offset[col];

            t->col_idx[dest] = i;

            if (t->values) {

                t->values[dest] = mat->values[k];

            }

            offset[col]++;

        }

    }



    lv00_free((void **) &col_counts);

    lv00_free((void **) &offset);

    *out = t;

    return true;

}



/* ========================================================================

 * 绋€鐤忕嚎鎬ф眰瑙ｅ櫒锛堝崰浣嶇瀹炵幇锛? * ======================================================================== */



bool sparse_cholesky_solve(const SparseMatrix *A, const double *b, double *x) {

    /* Native dense Cholesky fallback implementation.
     * Converts the sparse matrix to dense form and performs a standard
     * column-major Cholesky decomposition (L * L^T), followed by
     * forward and back substitution.
     *
     * Performance: O(n^3) time and O(n^2) memory due to dense conversion.
     * For large sparse matrices, consider integrating an external library
     * (e.g., SuiteSparse CHOLMOD) to exploit sparsity and achieve
     * near-O(nnz) factorization cost. */
    if (!A || !b || !x) {

        return false;

    }

    /* 检查方阵前提（Cholesky 分解要求矩阵为对称正定矩阵）*/
    if (A->rows != A->cols) {
        return false;
    }
    /* 稠密回退求解器：将稀疏矩阵拷贝为稠密矩阵后进行 Cholesky 分解，适用于任意大小矩阵 */
    int n = A->rows;

    double *dense = (double *) lv00_calloc((size_t) n * n, sizeof(double));

    if (!dense)

        return false;



    for (int i = 0; i < n; i++) {

        for (int k = A->row_ptr[i]; k < A->row_ptr[i + 1]; k++) {

            dense[i * n + A->col_idx[k]] = A->values[k];

        }

    }



    /* 绠€鍗?Cholesky 鍒嗚В锛堜粎 SPD 鐭╅樀锛?*/

    for (int j = 0; j < n; j++) {

        double sum = 0.0;

        for (int k = 0; k < j; k++) {

            sum += dense[j * n + k] * dense[j * n + k];

        }

        double diag = dense[j * n + j] - sum;

        if (diag <= 0.0) {

            lv00_free((void **) &dense);

            return false; /* 闈炴瀹?*/

        }

        dense[j * n + j] = sqrt(diag);



        for (int i = j + 1; i < n; i++) {

            sum = 0.0;

            for (int k = 0; k < j; k++) {

                sum += dense[i * n + k] * dense[j * n + k];

            }

            dense[i * n + j] = (dense[i * n + j] - sum) / dense[j * n + j];

        }

    }



    /* 鍓嶄唬 */

    memcpy(x, b, (size_t) n * sizeof(double));

    for (int j = 0; j < n; j++) {

        x[j] /= dense[j * n + j];

        for (int i = j + 1; i < n; i++) {

            x[i] -= dense[i * n + j] * x[j];

        }

    }



    /* 鍥炰唬 */

    for (int j = n - 1; j >= 0; j--) {

        for (int i = 0; i < j; i++) {

            x[i] -= dense[j * n + i] * x[j];

        }

        x[j] /= dense[j * n + j];

    }



    lv00_free((void **) &dense);

    return true;

}



bool sparse_lu_solve(const SparseMatrix *A, const double *b, double *x) {

    /* Native dense LU fallback implementation (Doolittle algorithm).
     * Converts the sparse matrix to dense form and performs in-place
     * Doolittle LU decomposition (unit lower triangular L, upper triangular U),
     * followed by forward and back substitution.
     *
     * Performance: O(n^3) time and O(n^2) memory due to dense conversion.
     * For large sparse matrices, consider integrating an external library
     * (e.g., SuiteSparse UMFPACK) to exploit sparsity and achieve
     * near-O(nnz) factorization cost with partial pivoting support. */
    if (!A || !b || !x) {

        return false;

    }



    /* 检查方阵前提（LU 分解要求矩阵为非奇异方阵）*/
    if (A->rows != A->cols) {
        return false;
    }

    /* 稠密回退求解器：将稀疏矩阵拷贝为稠密矩阵后进行 Doolittle LU 分解，适用于任意大小矩阵 */
    int n = A->rows;

    double *dense = (double *) lv00_calloc((size_t) n * n, sizeof(double));

    if (!dense)

        return false;



    for (int i = 0; i < n; i++) {

        for (int k = A->row_ptr[i]; k < A->row_ptr[i + 1]; k++) {

            dense[i * n + A->col_idx[k]] = A->values[k];

        }

    }



    /* 绠€鍗?LU 鍒嗚В锛圖oolittle 绠楁硶锛?*/

    for (int k = 0; k < n; k++) {

        for (int i = k + 1; i < n; i++) {

            if (fabs(dense[k * n + k]) < 1e-15) {

                lv00_free((void **) &dense);

                return false;

            }

            dense[i * n + k] /= dense[k * n + k];

            for (int j = k + 1; j < n; j++) {

                dense[i * n + j] -= dense[i * n + k] * dense[k * n + j];

            }

        }

    }



    /* 鍓嶄唬 Ly = b */

    memcpy(x, b, (size_t) n * sizeof(double));

    for (int i = 1; i < n; i++) {

        for (int j = 0; j < i; j++) {

            x[i] -= dense[i * n + j] * x[j];

        }

    }



    /* 鍥炰唬 Ux = y */

    for (int i = n - 1; i >= 0; i--) {

        for (int j = i + 1; j < n; j++) {

            x[i] -= dense[i * n + j] * x[j];

        }

        if (fabs(dense[i * n + i]) < 1e-15) {

            lv00_free((void **) &dense);

            return false;

        }

        x[i] /= dense[i * n + i];

    }



    lv00_free((void **) &dense);

    return true;

}



bool sparse_qr_solve(const SparseMatrix *A, const double *b, double *x) {

    /* Native dense QR fallback implementation (normal equation approach).
     * Solves the least-squares problem min ||Ax - b|| by forming the
     * normal equations A^T A x = A^T b and solving via Cholesky
     * decomposition (sparse_cholesky_solve).
     *
     * Performance: O(n^2 * m + n^3) time due to forming A^T A (sparse)
     * and the dense Cholesky solve. For large sparse matrices, consider
     * integrating an external library (e.g., SuiteSparse SPQR) for a
     * direct QR factorization that avoids forming A^T A, which can
     * improve numerical stability on ill-conditioned problems. */
    if (!A || !b || !x) {

        return false;

    }



    /* 稠密回退求解器：通过正规方程法（A^T A x = A^T b）将最小二乘问题转化为
     * Cholesky 分解求解，适用于任意大小矩阵 */
    SparseMatrix *AT = NULL;

    if (!sparse_matrix_transpose(A, &AT)) {

        return false;

    }



    SparseMatrix *ATA = NULL;

    if (!sparse_matrix_multiply(AT, A, &ATA)) {

        sparse_matrix_destroy(AT);

        return false;

    }



    int n = A->cols;

    double *ATb = (double *) lv00_calloc((size_t) n, sizeof(double));

    if (!ATb) {

        sparse_matrix_destroy(AT);

        sparse_matrix_destroy(ATA);

        return false;

    }



    /* 璁＄畻 A^T b */

    for (int i = 0; i < AT->rows; i++) {

        double sum = 0.0;

        for (int k = AT->row_ptr[i]; k < AT->row_ptr[i + 1]; k++) {

            sum += AT->values[k] * b[AT->col_idx[k]];

        }

        ATb[i] = sum;

    }



    bool ok = sparse_cholesky_solve(ATA, ATb, x);



    lv00_free((void **) &ATb);

    sparse_matrix_destroy(AT);

    sparse_matrix_destroy(ATA);

    return ok;

}



/* ========================================================================

 * 绾︽潫鍥惧害鍒嗘瀽

 * ======================================================================== */



bool graph_degree_analysis(const ConstraintGraph *graph, DegreeAnalysis **analysis) {

    if (!graph || !analysis) {

        return false;

    }



    DegreeAnalysis *da = (DegreeAnalysis *) lv00_calloc(1, sizeof(DegreeAnalysis));

    if (!da) {

        return false;

    }



    int n_nodes = graph->node_count;

    da->node_count = n_nodes;



    da->node_degrees = (int *) lv00_calloc((size_t) n_nodes, sizeof(int));

    if (!da->node_degrees) {

        lv00_free((void **) &da);

        return false;

    }



    /* 閬嶅巻鎵€鏈夌害鏉燂紝缁熻姣忎釜鑺傜偣鍙備笌绾︽潫鐨勬鏁?*/

    for (int ci = 0; ci < graph->constraint_count; ci++) {

        const Constraint *c = graph->constraints[ci];

        if (!c)

            continue;



        for (int p = 0; p < c->participant_count; p++) {

            int node_id = c->participants[p];

            if (node_id >= 0 && node_id < n_nodes) {

                da->node_degrees[node_id]++;

            }

        }

    }



    /* 缁熻搴﹀垎甯?*/

    da->max_degree = 0;

    da->min_degree = INT_MAX;

    da->isolated_count = 0;

    double total_degree = 0.0;



    for (int i = 0; i < n_nodes; i++) {

        int deg = da->node_degrees[i];

        total_degree += deg;

        if (deg > da->max_degree)

            da->max_degree = deg;

        if (deg < da->min_degree && deg > 0)

            da->min_degree = deg;

        if (deg == 0)

            da->isolated_count++;

    }



    if (da->isolated_count == n_nodes) {

        da->min_degree = 0;

    }



    da->avg_degree = (n_nodes > 0) ? (total_degree / n_nodes) : 0.0;



    /* 鏋勫缓搴﹀垎甯冪洿鏂瑰浘 */

    int hist_len = da->max_degree + 1;

    da->degree_counts = (int *) lv00_calloc((size_t) hist_len, sizeof(int));

    if (!da->degree_counts) {

        degree_analysis_free(da);

        return false;

    }



    for (int i = 0; i < n_nodes; i++) {

        int deg = da->node_degrees[i];

        da->degree_counts[deg]++;

    }



    *analysis = da;

    return true;

}



void degree_analysis_free(DegreeAnalysis *analysis) {

    if (!analysis) {

        return;

    }

    lv00_free((void **) &analysis->node_degrees);

    lv00_free((void **) &analysis->degree_counts);

    lv00_free((void **) &analysis);

}

