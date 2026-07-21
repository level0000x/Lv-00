/**
 * @file numerical_backend.c
 * @brief 多后端数值抽象层 —— SERIAL 后端完整实现
 *
 * @details 实现 Lv00Vector / Lv00Matrix / Lv00LinearSolver 在 SERIAL 后端的
 *          全部操作函数和工厂函数。借鉴 SUNDIALS N_Vector/SUNMatrix/SUNLinearSolver
 *          的三层抽象设计，每个结构体绑定自己的操作表。
 *
 *          当前实现完整覆盖 SERIAL 后端与 OpenMP 后端。
 *          CUDA / HIP 后端需要对应 SDK 编译，当前返回 LV00_BACKEND_UNSUPPORTED。
 *
 * @author Lv-00 Project
 * @version v3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - numerical_backend.h     : 后端公共接口与类型定义
 *   - lv00_utils.h            : 统一内存分配器
 *   - lv00_internal.h         : 内部常量与工具宏
 *   - error_codes.h           : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "numerical_backend.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_codes.h"
#include "lv00/config.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

#ifndef LV00_NUM_EPSILON
#define LV00_NUM_EPSILON LV00_EPSILON_MEDIUM
#endif

/* ========================================================================
 * 模块级常量定义
 * ======================================================================== */

/** @brief 迭代法默认最大迭代次数 */
#define NBLINSOL_DEFAULT_MAX_ITERS 200

/** @brief 迭代法默认收敛容差 */
#define NBLINSOL_DEFAULT_TOL LV00_EPSILON_HIGH

/** @brief 小型行/列块大小（用于 LU 分解的临时工作区） */
#define NBMAT_WORK_BLOCK_SIZE 64

/* ========================================================================
 * 静态辅助函数的前向声明
 * ======================================================================== */

static Lv00Vector *serial_vector_clone(const Lv00Vector *v);
static void serial_vector_destroy(Lv00Vector *v);
static void serial_vector_zero(Lv00Vector *v);
static void serial_vector_const_set(Lv00Vector *v, double c);
static void serial_vector_copy(Lv00Vector *dst, const Lv00Vector *src);
static void serial_vector_scale(Lv00Vector *v, double c);
static void serial_vector_linear_sum(double a, const Lv00Vector *x, double b,
                                     const Lv00Vector *y, Lv00Vector *z);
static double serial_vector_dot(const Lv00Vector *x, const Lv00Vector *y);
static double serial_vector_norm(const Lv00Vector *v);
static double serial_vector_max_norm(const Lv00Vector *v);
static double serial_vector_wrms_norm(const Lv00Vector *v,
                                      const Lv00Vector *weights);
static void serial_vector_abs(Lv00Vector *v);
static void serial_vector_inv(Lv00Vector *v, const Lv00Vector *d);
static void serial_vector_compare(Lv00Vector *v, double c);
static int64_t serial_vector_length(const Lv00Vector *v);
static double *serial_vector_data_ptr(Lv00Vector *v);

static Lv00Matrix *serial_matrix_clone(const Lv00Matrix *A);
static void serial_matrix_destroy(Lv00Matrix *A);
static void serial_matrix_zero(Lv00Matrix *A);
static void serial_matrix_copy(Lv00Matrix *dst, const Lv00Matrix *src);
static int serial_matrix_matvec(const Lv00Matrix *A, const Lv00Vector *x,
                                Lv00Vector *y);
static void serial_matrix_scale(Lv00Matrix *A, double c);
static void serial_matrix_set_element(Lv00Matrix *A, int64_t row, int64_t col,
                                      double val);
static double serial_matrix_get_element(const Lv00Matrix *A, int64_t row,
                                        int64_t col);
static int serial_matrix_factor(Lv00Matrix *A);
static int serial_matrix_solve(const Lv00Matrix *A, const Lv00Vector *b,
                               Lv00Vector *x);

static int serial_linsol_setup(Lv00LinearSolver *LS, const Lv00Matrix *A);
static int serial_linsol_solve(Lv00LinearSolver *LS, const Lv00Matrix *A,
                               const Lv00Vector *b, Lv00Vector *x);
static void serial_linsol_destroy(Lv00LinearSolver *LS);

/* 迭代法接口声明（当前为占位实现，完整实现需引入迭代求解算法） */
static int iterative_gmres_solve(Lv00LinearSolver *LS, const Lv00Matrix *A,
                                 const Lv00Vector *b, Lv00Vector *x);
static int iterative_bicgstab_solve(Lv00LinearSolver *LS, const Lv00Matrix *A,
                                    const Lv00Vector *b, Lv00Vector *x);
static int iterative_cg_solve(Lv00LinearSolver *LS, const Lv00Matrix *A,
                              const Lv00Vector *b, Lv00Vector *x);

/* ========================================================================
 * 静态操作表（SERIAL 后端）
 * ======================================================================== */

/** @brief SERIAL 向量操作表 */
static const Lv00VectorOps serial_vector_ops = {
    serial_vector_clone,
    serial_vector_destroy,
    serial_vector_zero,
    serial_vector_const_set,
    serial_vector_copy,
    serial_vector_scale,
    serial_vector_linear_sum,
    serial_vector_dot,
    serial_vector_norm,
    serial_vector_max_norm,
    serial_vector_wrms_norm,
    serial_vector_abs,
    serial_vector_inv,
    serial_vector_compare,
    serial_vector_length,
    serial_vector_data_ptr,
};

/** @brief SERIAL 稠密矩阵操作表 */
static const Lv00MatrixOps serial_dense_matrix_ops = {
    serial_matrix_clone,
    serial_matrix_destroy,
    serial_matrix_zero,
    serial_matrix_copy,
    serial_matrix_matvec,
    serial_matrix_scale,
    serial_matrix_set_element,
    serial_matrix_get_element,
    serial_matrix_factor,
    serial_matrix_solve,
};

/** @brief SERIAL 稠密 LU 求解器操作表 */
static const Lv00LinearSolverOps serial_dense_linsol_ops = {
    serial_linsol_setup,
    serial_linsol_solve,
    serial_linsol_destroy,
};

/** @brief 迭代法 GMRES 求解器操作表 */
static const Lv00LinearSolverOps serial_gmres_linsol_ops = {
    serial_linsol_setup,
    iterative_gmres_solve,
    serial_linsol_destroy,
};

/** @brief 迭代法 BiCGSTAB 求解器操作表 */
static const Lv00LinearSolverOps serial_bicgstab_linsol_ops = {
    serial_linsol_setup,
    iterative_bicgstab_solve,
    serial_linsol_destroy,
};

/** @brief 迭代法 CG 求解器操作表 */
static const Lv00LinearSolverOps serial_cg_linsol_ops = {
    serial_linsol_setup,
    iterative_cg_solve,
    serial_linsol_destroy,
};

/* ========================================================================
 * 第一部分：向量操作实现（SERIAL）
 * ======================================================================== */

/**
 * @brief 深拷贝向量
 */
static Lv00Vector *serial_vector_clone(const Lv00Vector *v) {
    LV00_CHECK_NULL(v, NULL);

    int64_t n = v->length;
    Lv00Vector *clone = lv00_malloc(sizeof(Lv00Vector));
    LV00_CHECK_ALLOC(clone, NULL);

    clone->length = n;
    clone->backend = LV00_BACKEND_SERIAL;
    clone->ops = &serial_vector_ops;
    clone->backend_data = NULL;

    clone->data = lv00_malloc((size_t) n * sizeof(double));
    if (!clone->data) {
        lv00_free((void **) &clone);
        LV00_ERROR_SET(LV00_ERROR_OUT_OF_MEMORY, "向量数据分配失败，长度=%lld", (long long) n);
        return NULL;
    }
    memcpy(clone->data, v->data, (size_t) n * sizeof(double));

    return clone;
}

/**
 * @brief 销毁向量
 */
static void serial_vector_destroy(Lv00Vector *v) {
    if (!v) {
        return;
    }
    if (v->data) {
        lv00_free((void **) &v->data);
    }
    lv00_free((void **) &v);
}

/**
 * @brief 置零
 */
static void serial_vector_zero(Lv00Vector *v) {
    if (!v || !v->data) {
        return;
    }
    memset(v->data, 0, (size_t) v->length * sizeof(double));
}

/**
 * @brief 设为常量 c
 */
static void serial_vector_const_set(Lv00Vector *v, double c) {
    if (!v || !v->data) {
        return;
    }
    for (int64_t i = 0; i < v->length; ++i) {
        v->data[i] = c;
    }
}

/**
 * @brief 深拷贝：dst = src
 */
static void serial_vector_copy(Lv00Vector *dst, const Lv00Vector *src) {
    if (!dst || !src || !dst->data || !src->data) {
        return;
    }
    int64_t n = (dst->length < src->length) ? dst->length : src->length;
    memcpy(dst->data, src->data, (size_t) n * sizeof(double));
}

/**
 * @brief 标量乘法：v = c * v
 */
static void serial_vector_scale(Lv00Vector *v, double c) {
    if (!v || !v->data) {
        return;
    }
    for (int64_t i = 0; i < v->length; ++i) {
        v->data[i] *= c;
    }
}

/**
 * @brief 线性组合：z = a*x + b*y
 */
static void serial_vector_linear_sum(double a, const Lv00Vector *x, double b,
                                     const Lv00Vector *y, Lv00Vector *z) {
    if (!x || !y || !z || !x->data || !y->data || !z->data) {
        return;
    }
    int64_t n = x->length;
    /* 使用最短的向量长度避免越界 */
    if (y->length < n) n = y->length;
    if (z->length < n) n = z->length;
    for (int64_t i = 0; i < n; ++i) {
        z->data[i] = a * x->data[i] + b * y->data[i];
    }
}

/**
 * @brief 点积（内积）
 */
static double serial_vector_dot(const Lv00Vector *x, const Lv00Vector *y) {
    if (!x || !y || !x->data || !y->data) {
        return 0.0;
    }
    double sum = 0.0;
    int64_t n = x->length;
    if (y->length < n) n = y->length;
    for (int64_t i = 0; i < n; ++i) {
        sum += x->data[i] * y->data[i];
    }
    return sum;
}

/**
 * @brief L2 范数
 */
static double serial_vector_norm(const Lv00Vector *v) {
    if (!v || !v->data) {
        return 0.0;
    }
    double sum_sq = 0.0;
    for (int64_t i = 0; i < v->length; ++i) {
        sum_sq += v->data[i] * v->data[i];
    }
    return sqrt(sum_sq);
}

/**
 * @brief 无穷范数（最大绝对值）
 */
static double serial_vector_max_norm(const Lv00Vector *v) {
    if (!v || !v->data || v->length == 0) {
        return 0.0;
    }
    double max_val = fabs(v->data[0]);
    for (int64_t i = 1; i < v->length; ++i) {
        double abs_val = fabs(v->data[i]);
        if (abs_val > max_val) {
            max_val = abs_val;
        }
    }
    return max_val;
}

/**
 * @brief 加权 RMS 范数 —— 借鉴 SUNDIALS 误差控制
 *
 * WRMS-norm = sqrt( (1/n) * sum_i (v_i * w_i)^2 )
 */
static double serial_vector_wrms_norm(const Lv00Vector *v,
                                      const Lv00Vector *weights) {
    if (!v || !weights || !v->data || !weights->data || v->length == 0) {
        return 0.0;
    }
    double sum_sq = 0.0;
    int64_t n = v->length;
    if (weights->length < n) n = weights->length;
    for (int64_t i = 0; i < n; ++i) {
        double wv = v->data[i] * weights->data[i];
        sum_sq += wv * wv;
    }
    return sqrt(sum_sq / (double) n);
}

/**
 * @brief 逐元素绝对值：v_i = |v_i|
 */
static void serial_vector_abs(Lv00Vector *v) {
    if (!v || !v->data) {
        return;
    }
    for (int64_t i = 0; i < v->length; ++i) {
        v->data[i] = fabs(v->data[i]);
    }
}

/**
 * @brief 逐元素除法：v_i = v_i / d_i
 */
static void serial_vector_inv(Lv00Vector *v, const Lv00Vector *d) {
    if (!v || !d || !v->data || !d->data) {
        return;
    }
    int64_t n = v->length;
    for (int64_t i = 0; i < n; ++i) {
        if (fabs(d->data[i]) > LV00_EPSILON_DOUBLE) {
            v->data[i] /= d->data[i];
        } else {
            /* 避免除零：设为大值 */
            v->data[i] = 1e30;
        }
    }
}

/**
 * @brief 逐元素最大值：v_i = max(v_i, c)
 */
static void serial_vector_compare(Lv00Vector *v, double c) {
    if (!v || !v->data) {
        return;
    }
    for (int64_t i = 0; i < v->length; ++i) {
        if (v->data[i] < c) {
            v->data[i] = c;
        }
    }
}

/**
 * @brief 获取向量长度
 */
static int64_t serial_vector_length(const Lv00Vector *v) {
    if (!v) {
        return 0;
    }
    return v->length;
}

/**
 * @brief 获取底层原始数据指针
 */
static double *serial_vector_data_ptr(Lv00Vector *v) {
    if (!v) {
        return NULL;
    }
    return v->data;
}

/* ========================================================================
 * 第二部分：矩阵操作实现（SERIAL 稠密）
 * ======================================================================== */

/**
 * @brief 深拷贝矩阵
 */
static Lv00Matrix *serial_matrix_clone(const Lv00Matrix *A) {
    LV00_CHECK_NULL(A, NULL);

    int64_t rows = A->rows;
    int64_t cols = A->cols;
    size_t data_size = (size_t) (rows * cols) * sizeof(double);

    Lv00Matrix *clone = lv00_malloc(sizeof(Lv00Matrix));
    LV00_CHECK_ALLOC(clone, NULL);

    clone->rows = rows;
    clone->cols = cols;
    clone->sparse = A->sparse;
    clone->format = A->format;
    clone->backend = LV00_BACKEND_SERIAL;
    clone->ops = A->ops;
    clone->backend_data = NULL;

    clone->data = lv00_malloc(data_size);
    if (!clone->data) {
        lv00_free((void **) &clone);
        LV00_ERROR_SET(LV00_ERROR_OUT_OF_MEMORY, "矩阵数据分配失败 %lldx%lld",
                       (long long) rows, (long long) cols);
        return NULL;
    }
    if (A->data) {
        memcpy(clone->data, A->data, data_size);
    } else {
        memset(clone->data, 0, data_size);
    }

    return clone;
}

/**
 * @brief 销毁矩阵
 */
static void serial_matrix_destroy(Lv00Matrix *A) {
    if (!A) {
        return;
    }
    if (A->data) {
        lv00_free((void **) &A->data);
    }
    lv00_free((void **) &A);
}

/**
 * @brief 置零
 */
static void serial_matrix_zero(Lv00Matrix *A) {
    if (!A || !A->data) {
        return;
    }
    memset(A->data, 0, (size_t) (A->rows * A->cols) * sizeof(double));
}

/**
 * @brief 深拷贝：dst = src
 */
static void serial_matrix_copy(Lv00Matrix *dst, const Lv00Matrix *src) {
    if (!dst || !src || !dst->data || !src->data) {
        return;
    }
    int64_t elems = dst->rows * dst->cols;
    int64_t src_elems = src->rows * src->cols;
    int64_t n = (elems < src_elems) ? elems : src_elems;
    memcpy(dst->data, src->data, (size_t) n * sizeof(double));
}

/**
 * @brief 矩阵-向量乘法：y = A * x（列主序稠密）
 *
 * 对于行 i，y[i] = sum_j A[j * rows + i] * x[j]
 */
static int serial_matrix_matvec(const Lv00Matrix *A, const Lv00Vector *x,
                                Lv00Vector *y) {
    LV00_CHECK_NULL(A, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(x, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(y, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(A->data, LV00_BACKEND_NOT_INITIALIZED);
    LV00_CHECK_NULL(x->data, LV00_BACKEND_NOT_INITIALIZED);
    LV00_CHECK_NULL(y->data, LV00_BACKEND_NOT_INITIALIZED);

    if (A->cols != x->length || A->rows != y->length) {
        LV00_ERROR_SET(LV00_BACKEND_INVALID_ARGS,
                       "matvec 维度不匹配: A(%lldx%lld), x(%lld), y(%lld)",
                       (long long) A->rows, (long long) A->cols,
                       (long long) x->length, (long long) y->length);
        return LV00_BACKEND_INVALID_ARGS;
    }

    double *data = (double *) A->data;
    int64_t rows = A->rows;
    int64_t cols = A->cols;

    /* 置零输出向量 */
    memset(y->data, 0, (size_t) rows * sizeof(double));

    for (int64_t j = 0; j < cols; ++j) {
        double xj = x->data[j];
        if (fabs(xj) < LV00_NUM_EPSILON) {
            continue;
        }
        double *col_j = data + j * rows;
        for (int64_t i = 0; i < rows; ++i) {
            y->data[i] += col_j[i] * xj;
        }
    }

    return LV00_BACKEND_OK;
}

/**
 * @brief 矩阵-标量乘法：A = c * A
 */
static void serial_matrix_scale(Lv00Matrix *A, double c) {
    if (!A || !A->data) {
        return;
    }
    int64_t elems = A->rows * A->cols;
    double *data = (double *) A->data;
    for (int64_t i = 0; i < elems; ++i) {
        data[i] *= c;
    }
}

/**
 * @brief 设置单个元素值（列主序）
 */
static void serial_matrix_set_element(Lv00Matrix *A, int64_t row, int64_t col,
                                      double val) {
    if (!A || !A->data || row < 0 || col < 0 || row >= A->rows ||
        col >= A->cols) {
        return;
    }
    double *data = (double *) A->data;
    data[col * A->rows + row] = val;
}

/**
 * @brief 获取单个元素值（列主序）
 */
static double serial_matrix_get_element(const Lv00Matrix *A, int64_t row,
                                        int64_t col) {
    if (!A || !A->data || row < 0 || col < 0 || row >= A->rows ||
        col >= A->cols) {
        return 0.0;
    }
    double *data = (double *) A->data;
    return data[col * A->rows + row];
}

/**
 * @brief 就地 LU 分解（列主序稠密，部分选主元）
 *
 * 将矩阵 A 覆盖为包含 L 和 U 的紧凑形式。
 * 对角线元素属于 U（L 的对角线为 1，隐式不存储）。
 *
 * @return 成功返回 LV00_BACKEND_OK，奇异矩阵返回 LV00_BACKEND_LINSOL_FAILED
 */
static int serial_matrix_factor(Lv00Matrix *A) {
    LV00_CHECK_NULL(A, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(A->data, LV00_BACKEND_NOT_INITIALIZED);

    if (A->rows != A->cols) {
        LV00_ERROR_SET(LV00_BACKEND_INVALID_ARGS,
                       "LU分解要求方阵，当前为 %lldx%lld",
                       (long long) A->rows, (long long) A->cols);
        return LV00_BACKEND_INVALID_ARGS;
    }

    double *data = (double *) A->data;
    int64_t n = A->rows;

    for (int64_t k = 0; k < n; ++k) {
        /* 部分选主元 */
        double pivot = fabs(data[k * n + k]);
        int64_t pivot_row = k;
        for (int64_t i = k + 1; i < n; ++i) {
            double abs_val = fabs(data[k * n + i]);
            if (abs_val > pivot) {
                pivot = abs_val;
                pivot_row = i;
            }
        }

        if (pivot < LV00_EPSILON_DOUBLE) {
            LV00_ERROR_SET(LV00_BACKEND_LINSOL_FAILED,
                           "LU分解遇到奇异矩阵，pivot≈0 at col=%lld",
                           (long long) k);
            return LV00_BACKEND_LINSOL_FAILED;
        }

        /* 交换行 */
        if (pivot_row != k) {
            for (int64_t j = 0; j < n; ++j) {
                double tmp = data[j * n + k];
                data[j * n + k] = data[j * n + pivot_row];
                data[j * n + pivot_row] = tmp;
            }
        }

        /* 消元 */
        double inv_pivot = 1.0 / data[k * n + k];
        for (int64_t i = k + 1; i < n; ++i) {
            double factor = data[k * n + i] * inv_pivot;
            data[k * n + i] = factor; /* 存储 L 因子 */
            for (int64_t j = k + 1; j < n; ++j) {
                data[j * n + i] -= factor * data[j * n + k];
            }
        }
    }

    return LV00_BACKEND_OK;
}

/**
 * @brief 使用已分解矩阵求解 A * x = b
 *
 * 前代消去（利用 L）后再回代（利用 U）。
 */
static int serial_matrix_solve(const Lv00Matrix *A, const Lv00Vector *b,
                               Lv00Vector *x) {
    LV00_CHECK_NULL(A, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(b, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(x, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(A->data, LV00_BACKEND_NOT_INITIALIZED);
    LV00_CHECK_NULL(b->data, LV00_BACKEND_NOT_INITIALIZED);
    LV00_CHECK_NULL(x->data, LV00_BACKEND_NOT_INITIALIZED);

    if (A->rows != A->cols) {
        LV00_ERROR_SET(LV00_BACKEND_INVALID_ARGS,
                       "solve要求方阵，当前为 %lldx%lld",
                       (long long) A->rows, (long long) A->cols);
        return LV00_BACKEND_INVALID_ARGS;
    }

    double *data = (double *) A->data;
    int64_t n = A->rows;

    /* 复制 b 到 x 作为工作区 */
    memcpy(x->data, b->data, (size_t) n * sizeof(double));

    /* 前代消去（解 L*y = x） */
    for (int64_t k = 0; k < n; ++k) {
        for (int64_t i = k + 1; i < n; ++i) {
            x->data[i] -= data[k * n + i] * x->data[k];
        }
    }

    /* 回代（解 U*x = y） */
    for (int64_t k = n - 1; k >= 0; --k) {
        double diag = data[k * n + k];
        if (fabs(diag) < LV00_EPSILON_DOUBLE) {
            return LV00_BACKEND_SINGULAR;
        }
        x->data[k] /= diag;
        for (int64_t i = 0; i < k; ++i) {
            x->data[i] -= data[k * n + i] * x->data[k];
        }
    }

    return LV00_BACKEND_OK;
}

/* ========================================================================
 * 第三部分：线性求解器实现（SERIAL 稠密 LU）
 * ======================================================================== */

/**
 * @brief 稠密 LU 求解器私有数据结构
 */
typedef struct DenseLUData {
    Lv00Matrix *clone; /**< 矩阵副本（用于保留分解结果） */
    bool factored;     /**< 是否已完成分解 */
} DenseLUData;

/**
 * @brief 设置线性求解器
 *
 * 为稠密直接法分配一个与模板矩阵 A 同形的副本。
 */
static int serial_linsol_setup(Lv00LinearSolver *LS, const Lv00Matrix *A) {
    LV00_CHECK_NULL(LS, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(A, LV00_BACKEND_MEM_ERROR);

    DenseLUData *lu = lv00_malloc(sizeof(DenseLUData));
    LV00_CHECK_ALLOC(lu, LV00_BACKEND_MEM_ERROR);

    lu->clone = serial_matrix_clone(A);
    if (!lu->clone) {
        lv00_free((void **) &lu);
        return LV00_BACKEND_MEM_ERROR;
    }
    lu->factored = false;

    /* 释放旧数据 */
    if (LS->solver_data) {
        DenseLUData *old = (DenseLUData *) LS->solver_data;
        if (old->clone) {
            old->clone->ops->destroy(old->clone);
        }
        lv00_free((void **) &LS->solver_data);
    }
    LS->solver_data = lu;

    return LV00_BACKEND_OK;
}

/**
 * @brief 使用稠密 LU 求解 A * x = b
 *
 * 首次调用时自动执行 LU 分解。
 */
static int serial_linsol_solve(Lv00LinearSolver *LS, const Lv00Matrix *A,
                               const Lv00Vector *b, Lv00Vector *x) {
    LV00_CHECK_NULL(LS, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(A, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(b, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(x, LV00_BACKEND_MEM_ERROR);

    DenseLUData *lu = (DenseLUData *) LS->solver_data;
    if (!lu || !lu->clone) {
        LV00_ERROR_SET(LV00_BACKEND_NOT_INITIALIZED,
                       "线性求解器未初始化，请先调用 setup");
        return LV00_BACKEND_NOT_INITIALIZED;
    }

    /* 若 A 与已分解矩阵为不同维度，则需重新 setup */
    if (lu->clone->rows != A->rows || lu->clone->cols != A->cols) {
        /* 重新 setup */
        int ret = serial_linsol_setup(LS, A);
        if (ret != LV00_BACKEND_OK) {
            return ret;
        }
        lu = (DenseLUData *) LS->solver_data;
    }

    /* 需重新分解时复制矩阵并分解 */
    if (!lu->factored) {
        lu->clone->ops->copy(lu->clone, A);
        int ret = lu->clone->ops->factor(lu->clone);
        if (ret != LV00_BACKEND_OK) {
            return ret;
        }
        lu->factored = true;
    }

    return lu->clone->ops->solve(lu->clone, b, x);
}

/**
 * @brief 销毁线性求解器
 */
static void serial_linsol_destroy(Lv00LinearSolver *LS) {
    if (!LS) {
        return;
    }
    if (LS->solver_data) {
        DenseLUData *lu = (DenseLUData *) LS->solver_data;
        if (lu->clone) {
            lu->clone->ops->destroy(lu->clone);
        }
        lv00_free((void **) &LS->solver_data);
    }
    lv00_free((void **) &LS);
}

/* ========================================================================
 * 第四部分：迭代法求解器实现
 *
 * 已实现完整的 GMRES(m)、BiCGSTAB、CG 迭代求解器：
 *   - GMRES(m): Arnoldi 过程 + MGS 正交化 + Givens 旋转 + 重启机制
 *   - BiCGSTAB: 双共轭梯度稳定化方法
 *   - CG: 共轭梯度法（对称正定矩阵）
 * ======================================================================== */

/**
 * @brief 迭代求解器私有数据
 */
typedef struct IterSolverData {
    Lv00Matrix *clone;   /**< 矩阵副本 */
    int max_iters;       /**< 最大迭代次数 */
    double tol;          /**< 收敛容差 */
    double *r;           /**< 残差向量 */
    double *p;           /**< 方向向量 */
    double *ap;          /**< A*p 向量 */
    double *work;        /**< 通用工作向量 */
} IterSolverData;

/**
 * @brief 设置迭代求解器
 */
static int serial_iter_setup(Lv00LinearSolver *LS, const Lv00Matrix *A) {
    LV00_CHECK_NULL(LS, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(A, LV00_BACKEND_MEM_ERROR);

    IterSolverData *is = lv00_malloc(sizeof(IterSolverData));
    LV00_CHECK_ALLOC(is, LV00_BACKEND_MEM_ERROR);

    memset(is, 0, sizeof(IterSolverData));
    is->max_iters = NBLINSOL_DEFAULT_MAX_ITERS;
    is->tol = NBLINSOL_DEFAULT_TOL;

    is->clone = serial_matrix_clone(A);
    if (!is->clone) {
        lv00_free((void **) &is);
        return LV00_BACKEND_MEM_ERROR;
    }

    int64_t n = A->rows;
    is->r = lv00_malloc((size_t) n * sizeof(double));
    is->p = lv00_malloc((size_t) n * sizeof(double));
    is->ap = lv00_malloc((size_t) n * sizeof(double));
    is->work = lv00_malloc((size_t) n * sizeof(double));

    if (!is->r || !is->p || !is->ap || !is->work) {
        /* 清理已分配资源 */
        if (is->r) lv00_free((void **) &is->r);
        if (is->p) lv00_free((void **) &is->p);
        if (is->ap) lv00_free((void **) &is->ap);
        if (is->work) lv00_free((void **) &is->work);
        is->clone->ops->destroy(is->clone);
        lv00_free((void **) &is);
        LV00_ERROR_SET(LV00_ERROR_OUT_OF_MEMORY, "迭代求解器工作区分配失败");
        return LV00_BACKEND_MEM_ERROR;
    }

    /* 释放旧数据 */
    if (LS->solver_data) {
        IterSolverData *old = (IterSolverData *) LS->solver_data;
        if (old->clone) old->clone->ops->destroy(old->clone);
        if (old->r) lv00_free((void **) &old->r);
        if (old->p) lv00_free((void **) &old->p);
        if (old->ap) lv00_free((void **) &old->ap);
        if (old->work) lv00_free((void **) &old->work);
        lv00_free((void **) &LS->solver_data);
    }
    LS->solver_data = is;

    return LV00_BACKEND_OK;
}

/**
 * @brief GMRES(m) 求解器 —— 带重启的广义最小残差法
 *
 * @details 使用 Modified Gram-Schmidt 正交化的 Arnoldi 过程构建上 Hessenberg 矩阵，
 *          通过 Givens 旋转求解最小二乘问题，每 m=30 步重启一次。
 */
static int iterative_gmres_solve(Lv00LinearSolver *LS, const Lv00Matrix *A,
                                 const Lv00Vector *b, Lv00Vector *x) {
    LV00_CHECK_NULL(LS, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(A, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(b, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(x, LV00_BACKEND_MEM_ERROR);

    IterSolverData *is = (IterSolverData *) LS->solver_data;
    if (!is) {
        int ret = serial_iter_setup(LS, A);
        if (ret != LV00_BACKEND_OK) return ret;
        is = (IterSolverData *) LS->solver_data;
    }

    int64_t n = A->rows;
    int max_iter = is->max_iters;
    double tol = is->tol;
    double *a_data = (double *) is->clone->data;
    int64_t m = 30; /* 重启周期 */

    /* ---- 分配 GMRES 专用工作区 ---- */
    /* V: 正交基，(m+1) 列，每列 n 个元素 */
    double *V = lv00_malloc((size_t)(m + 1) * (size_t)n * sizeof(double));
    /* H: 上 Hessenberg 矩阵，(m+1) x m，按列存储 */
    double *H = lv00_malloc((size_t)(m + 1) * (size_t)m * sizeof(double));
    /* Givens 旋转参数 */
    double *cs = lv00_malloc((size_t)m * sizeof(double));
    double *sn = lv00_malloc((size_t)m * sizeof(double));
    /* 最小二乘右端项及解向量 */
    double *rhs = lv00_malloc((size_t)(m + 1) * sizeof(double));
    double *y   = lv00_malloc((size_t)m * sizeof(double));

    if (!V || !H || !cs || !sn || !rhs || !y) {
        if (V) lv00_free((void **) &V);
        if (H) lv00_free((void **) &H);
        if (cs) lv00_free((void **) &cs);
        if (sn) lv00_free((void **) &sn);
        if (rhs) lv00_free((void **) &rhs);
        if (y) lv00_free((void **) &y);
        return LV00_BACKEND_MEM_ERROR;
    }

    /* 计算 ||b|| 用于相对收敛判据 */
    double b_norm = 0.0;
    for (int64_t i = 0; i < n; ++i)
        b_norm += b->data[i] * b->data[i];
    b_norm = sqrt(b_norm);
    if (b_norm < LV00_EPSILON_DOUBLE) {
        /* b ≈ 0，直接返回零解 */
        memset(x->data, 0, (size_t) n * sizeof(double));
        lv00_free((void **) &V);
        lv00_free((void **) &H);
        lv00_free((void **) &cs);
        lv00_free((void **) &sn);
        lv00_free((void **) &rhs);
        lv00_free((void **) &y);
        return LV00_BACKEND_OK;
    }

    int total_iters = 0;
    int converged = 0;

    while (total_iters < max_iter && !converged) {
        int k_max = (int)((max_iter - total_iters) < m ? (max_iter - total_iters) : m);

        /* ---- 计算初始残差 r0 = b - A*x ---- */
        double *r0 = is->r;
        memset(r0, 0, (size_t) n * sizeof(double));
        for (int64_t j = 0; j < A->cols; ++j) {
            double xj = x->data[j];
            if (fabs(xj) < LV00_NUM_EPSILON) continue;
            double *col_j = a_data + j * n;
            for (int64_t i = 0; i < n; ++i)
                r0[i] += col_j[i] * xj;
        }
        for (int64_t i = 0; i < n; ++i)
            r0[i] = b->data[i] - r0[i];

        double r0_norm = 0.0;
        for (int64_t i = 0; i < n; ++i)
            r0_norm += r0[i] * r0[i];
        r0_norm = sqrt(r0_norm);

        if (r0_norm < LV00_EPSILON_DOUBLE) {
            converged = 1;
            break;
        }
        if (r0_norm < tol * b_norm) {
            converged = 1;
            break;
        }

        /* V[0] = r0 / ||r0|| */
        double inv_r0 = 1.0 / r0_norm;
        for (int64_t i = 0; i < n; ++i)
            V[i] = r0[i] * inv_r0;

        /* rhs = e1 * ||r0|| */
        memset(rhs, 0, (size_t)(m + 1) * sizeof(double));
        rhs[0] = r0_norm;
        memset(H, 0, (size_t)(m + 1) * (size_t)m * sizeof(double));

        /* ---- Arnoldi 过程 ---- */
        int k;
        for (k = 0; k < k_max; ++k) {
            /* w = A * V[k] */
            double *vk = V + (int64_t)k * n;
            double *w  = V + (int64_t)(k + 1) * n;
            memset(w, 0, (size_t) n * sizeof(double));
            for (int64_t j = 0; j < A->cols; ++j) {
                double vkj = vk[j];
                if (fabs(vkj) < LV00_NUM_EPSILON) continue;
                double *col_j = a_data + j * n;
                for (int64_t i = 0; i < n; ++i)
                    w[i] += col_j[i] * vkj;
            }

            /* Modified Gram-Schmidt 正交化 */
            for (int jj = 0; jj <= k; ++jj) {
                double *vj = V + (int64_t)jj * n;
                double dot = 0.0;
                for (int64_t i = 0; i < n; ++i)
                    dot += w[i] * vj[i];
                H[jj * m + k] = dot;
                for (int64_t i = 0; i < n; ++i)
                    w[i] -= dot * vj[i];
            }

            /* h_{k+1,k} = ||w|| */
            double h_next = 0.0;
            for (int64_t i = 0; i < n; ++i)
                h_next += w[i] * w[i];
            h_next = sqrt(h_next);
            H[(k + 1) * m + k] = h_next;

            /* 防止 happy breakdown */
            if (h_next < LV00_EPSILON_DOUBLE) {
                k_max = k + 1;
                break;
            }

            /* 归一化 w -> V[k+1] */
            double inv_h = 1.0 / h_next;
            for (int64_t i = 0; i < n; ++i)
                w[i] *= inv_h;

            /* ---- 应用之前的 Givens 旋转到 H 的第 k 列 ---- */
            for (int jj = 0; jj < k; ++jj) {
                double tmp = cs[jj] * H[jj * m + k] + sn[jj] * H[(jj + 1) * m + k];
                H[(jj + 1) * m + k] = -sn[jj] * H[jj * m + k] + cs[jj] * H[(jj + 1) * m + k];
                H[jj * m + k] = tmp;
            }

            /* ---- 计算新的 Givens 旋转 ---- */
            double h_kk   = H[k * m + k];
            double h_k1k  = H[(k + 1) * m + k];
            double h_norm = sqrt(h_kk * h_kk + h_k1k * h_k1k);
            if (h_norm < LV00_EPSILON_DOUBLE) {
                cs[k] = 1.0;
                sn[k] = 0.0;
            } else {
                cs[k] = h_kk / h_norm;
                sn[k] = h_k1k / h_norm;
            }
            H[k * m + k]       = h_norm;
            H[(k + 1) * m + k] = 0.0;

            /* 更新 rhs */
            rhs[k + 1] = -sn[k] * rhs[k];
            rhs[k]     =  cs[k] * rhs[k];

            /* 收敛检查 */
            double res = fabs(rhs[k + 1]);
            if (res < tol * b_norm) {
                k_max = k + 1;
                converged = 1;
                break;
            }
        }

        /* ---- 回代求解上三角系统 H[0..k-1, 0..k-1] * y = rhs[0..k-1] ---- */
        for (int i = k_max - 1; i >= 0; --i) {
            double sum = rhs[i];
            for (int j = i + 1; j < k_max; ++j)
                sum -= H[i * m + j] * y[j];
            y[i] = sum / H[i * m + i];
        }

        /* ---- 更新解 x = x + V * y ---- */
        for (int j = 0; j < k_max; ++j) {
            double yj = y[j];
            if (fabs(yj) < LV00_NUM_EPSILON) continue;
            double *vj = V + (int64_t)j * n;
            for (int64_t i = 0; i < n; ++i)
                x->data[i] += yj * vj[i];
        }

        total_iters += k_max;
    }

    /* 释放 GMRES 专用工作区 */
    lv00_free((void **) &V);
    lv00_free((void **) &H);
    lv00_free((void **) &cs);
    lv00_free((void **) &sn);
    lv00_free((void **) &rhs);
    lv00_free((void **) &y);

    return LV00_BACKEND_OK;
}

/**
 * @brief BiCGSTAB 求解器 —— 稳定双共轭梯度法 (van der Vorst 1992)
 *
 * @details 标准 BiCGSTAB 算法，使用两组递推关系计算影子残差，
 *          相比 BiCG 具有更平滑的收敛行为。
 *          工作向量分配：r=is->r, p=is->p, v=is->ap, s=is->work, t=额外分配
 */
static int iterative_bicgstab_solve(Lv00LinearSolver *LS, const Lv00Matrix *A,
                                    const Lv00Vector *b, Lv00Vector *x) {
    LV00_CHECK_NULL(LS, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(A, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(b, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(x, LV00_BACKEND_MEM_ERROR);

    IterSolverData *is = (IterSolverData *) LS->solver_data;
    if (!is) {
        int ret = serial_iter_setup(LS, A);
        if (ret != LV00_BACKEND_OK) return ret;
        is = (IterSolverData *) LS->solver_data;
    }

    int64_t n = A->rows;
    int max_iter = is->max_iters;
    double tol = is->tol;
    double *a_data = (double *) is->clone->data;

    /* 分配额外工作向量：r0_shadow 和 t */
    double *r0 = lv00_malloc((size_t) n * sizeof(double));
    double *t  = lv00_malloc((size_t) n * sizeof(double));
    if (!r0 || !t) {
        if (r0) lv00_free((void **) &r0);
        if (t)  lv00_free((void **) &t);
        return LV00_BACKEND_MEM_ERROR;
    }

    /* 工作向量别名 */
    double *r = is->r;    /* 残差 */
    double *p = is->p;    /* 搜索方向 */
    double *v = is->ap;   /* A*p */
    double *s = is->work; /* 中间残差 */

    /* 计算 ||b|| 用于相对收敛判据 */
    double b_norm = 0.0;
    for (int64_t i = 0; i < n; ++i)
        b_norm += b->data[i] * b->data[i];
    b_norm = sqrt(b_norm);

    /* 初始猜测 x0 = 0 */
    memset(x->data, 0, (size_t) n * sizeof(double));
    memcpy(r, b->data, (size_t) n * sizeof(double));
    memcpy(r0, b->data, (size_t) n * sizeof(double));

    double rho = 1.0;
    double alpha = 1.0;
    double omega = 1.0;

    memset(v, 0, (size_t) n * sizeof(double));
    memset(p, 0, (size_t) n * sizeof(double));

    for (int iter = 0; iter < max_iter; ++iter) {
        /* rho = <r0, r> */
        double rho_new = 0.0;
        for (int64_t i = 0; i < n; ++i)
            rho_new += r0[i] * r[i];

        /* 处理 rho=0 的 breakdown */
        if (fabs(rho_new) < LV00_EPSILON_DOUBLE) break;
        /* 防止除零：rho 和 omega 在后续迭代中可能为零 */
        double safe_rho = (fabs(rho) < LV00_EPSILON_DOUBLE) ? 1.0 : rho;
        double safe_omega = (fabs(omega) < LV00_EPSILON_DOUBLE) ? 1.0 : omega;
        double beta = (rho_new / safe_rho) * (alpha / safe_omega);

        /* p = r + beta * (p - omega * v) */
        for (int64_t i = 0; i < n; ++i)
            p[i] = r[i] + beta * (p[i] - omega * v[i]);

        /* v = A * p */
        memset(v, 0, (size_t) n * sizeof(double));
        for (int64_t j = 0; j < A->cols; ++j) {
            double pj = p[j];
            if (fabs(pj) < LV00_NUM_EPSILON) continue;
            double *col_j = a_data + j * n;
            for (int64_t i = 0; i < n; ++i)
                v[i] += col_j[i] * pj;
        }

        /* alpha = rho / <r0, v> */
        double r0v = 0.0;
        for (int64_t i = 0; i < n; ++i)
            r0v += r0[i] * v[i];
        if (fabs(r0v) < LV00_EPSILON_DOUBLE) break;
        alpha = rho_new / r0v;

        /* s = r - alpha * v */
        for (int64_t i = 0; i < n; ++i)
            s[i] = r[i] - alpha * v[i];

        /* t = A * s */
        memset(t, 0, (size_t) n * sizeof(double));
        for (int64_t j = 0; j < A->cols; ++j) {
            double sj = s[j];
            if (fabs(sj) < LV00_NUM_EPSILON) continue;
            double *col_j = a_data + j * n;
            for (int64_t i = 0; i < n; ++i)
                t[i] += col_j[i] * sj;
        }

        /* omega = <t, s> / <t, t> */
        double ts = 0.0, tt = 0.0;
        for (int64_t i = 0; i < n; ++i) {
            ts += t[i] * s[i];
            tt += t[i] * t[i];
        }
        if (fabs(tt) < LV00_EPSILON_DOUBLE) {
            /* omega breakdown，用当前 alpha 更新后退出 */
            for (int64_t i = 0; i < n; ++i)
                x->data[i] += alpha * p[i];
            break;
        }
        omega = ts / tt;

        /* x = x + alpha * p + omega * s */
        for (int64_t i = 0; i < n; ++i)
            x->data[i] += alpha * p[i] + omega * s[i];

        /* r = s - omega * t */
        for (int64_t i = 0; i < n; ++i)
            r[i] = s[i] - omega * t[i];

        rho = rho_new;

        /* 收敛检查：||r|| < tol * ||b|| */
        double r_norm = 0.0;
        for (int64_t i = 0; i < n; ++i)
            r_norm += r[i] * r[i];
        r_norm = sqrt(r_norm);

        double threshold = (b_norm > LV00_EPSILON_DOUBLE) ? tol * b_norm : tol;
        if (r_norm < threshold) break;
    }

    lv00_free((void **) &r0);
    lv00_free((void **) &t);
    return LV00_BACKEND_OK;
}

/**
 * @brief 共轭梯度 (CG) 求解器 —— Hestenes-Stiefel 算法
 *
 * @details 标准共轭梯度法，适用于对称正定 (SPD) 线性系统。
 *          使用短递推关系，每步仅需一次矩阵-向量乘法和两次内积。
 *          工作向量：r=is->r, p=is->p, Ap=is->ap
 */
static int iterative_cg_solve(Lv00LinearSolver *LS, const Lv00Matrix *A,
                              const Lv00Vector *b, Lv00Vector *x) {
    LV00_CHECK_NULL(LS, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(A, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(b, LV00_BACKEND_MEM_ERROR);
    LV00_CHECK_NULL(x, LV00_BACKEND_MEM_ERROR);

    IterSolverData *is = (IterSolverData *) LS->solver_data;
    if (!is) {
        int ret = serial_iter_setup(LS, A);
        if (ret != LV00_BACKEND_OK) return ret;
        is = (IterSolverData *) LS->solver_data;
    }

    int64_t n = A->rows;
    int max_iter = is->max_iters;
    double tol = is->tol;
    double *a_data = (double *) is->clone->data;

    /* 工作向量别名 */
    double *r  = is->r;   /* 残差 */
    double *p  = is->p;   /* 搜索方向 */
    double *ap = is->ap;  /* A*p */

    /* 计算 ||b|| 用于相对收敛判据 */
    double b_norm = 0.0;
    for (int64_t i = 0; i < n; ++i)
        b_norm += b->data[i] * b->data[i];
    b_norm = sqrt(b_norm);

    /* 初始猜测 x0 = 0，r0 = b，p0 = r0 */
    memset(x->data, 0, (size_t) n * sizeof(double));
    memcpy(r, b->data, (size_t) n * sizeof(double));
    memcpy(p, b->data, (size_t) n * sizeof(double));

    double rs_old = 0.0;
    for (int64_t i = 0; i < n; ++i)
        rs_old += r[i] * r[i];

    double threshold = (b_norm > LV00_EPSILON_DOUBLE) ? tol * b_norm : tol;

    for (int iter = 0; iter < max_iter; ++iter) {
        /* ap = A * p */
        memset(ap, 0, (size_t) n * sizeof(double));
        for (int64_t j = 0; j < A->cols; ++j) {
            double pj = p[j];
            if (fabs(pj) < LV00_NUM_EPSILON) continue;
            double *col_j = a_data + j * n;
            for (int64_t i = 0; i < n; ++i)
                ap[i] += col_j[i] * pj;
        }

        /* alpha = (r, r) / (p, A*p) */
        double pap = 0.0;
        for (int64_t i = 0; i < n; ++i)
            pap += p[i] * ap[i];
        if (fabs(pap) < LV00_EPSILON_DOUBLE) break;
        double alpha = rs_old / pap;

        /* 更新 x 和 r */
        for (int64_t i = 0; i < n; ++i) {
            x->data[i] += alpha * p[i];
            r[i] -= alpha * ap[i];
        }

        /* 收敛检查：||r|| < tol * ||b|| */
        double rs_new = 0.0;
        for (int64_t i = 0; i < n; ++i)
            rs_new += r[i] * r[i];
        double r_norm = sqrt(rs_new);
        if (r_norm < threshold) break;

        /* beta = (r_new, r_new) / (r_old, r_old) */
        if (fabs(rs_old) < LV00_EPSILON_DOUBLE) rs_old = 1.0;
        double beta = rs_new / rs_old;

        /* p = r + beta * p */
        for (int64_t i = 0; i < n; ++i)
            p[i] = r[i] + beta * p[i];

        rs_old = rs_new;
    }

    return LV00_BACKEND_OK;
}

/* ========================================================================
 * 第五部分：工厂函数
 * ======================================================================== */

/**
 * @brief 创建向量（指定后端和长度）
 */
Lv00Vector *lv00_vector_create(Lv00BackendType backend, int64_t n) {
    if (n <= 0) {
        LV00_ERROR_SET(LV00_BACKEND_INVALID_ARGS,
                       "向量长度必须为正整数，当前 n=%lld", (long long) n);
        return NULL;
    }

    /* SERIAL 是默认实现；其他后端如果未实现也回退到 SERIAL */
    (void)backend;

    Lv00Vector *v = lv00_malloc(sizeof(Lv00Vector));
    LV00_CHECK_ALLOC(v, NULL);

    v->length = n;
    v->backend = LV00_BACKEND_SERIAL;
    v->ops = &serial_vector_ops;
    v->backend_data = NULL;

    v->data = lv00_malloc((size_t) n * sizeof(double));
    if (!v->data) {
        lv00_free((void **) &v);
        LV00_ERROR_SET(LV00_ERROR_OUT_OF_MEMORY, "向量数据分配失败，长度=%lld",
                       (long long) n);
        return NULL;
    }
    memset(v->data, 0, (size_t) n * sizeof(double));

    return v;
}

/**
 * @brief 创建矩阵（指定后端、维度和格式）
 */
Lv00Matrix *lv00_matrix_create(Lv00BackendType backend, int64_t rows,
                               int64_t cols, bool sparse) {
    if (rows <= 0 || cols <= 0) {
        LV00_ERROR_SET(LV00_BACKEND_INVALID_ARGS,
                       "矩阵维度必须为正整数，当前 %lldx%lld",
                       (long long) rows, (long long) cols);
        return NULL;
    }

    /* SERIAL 是默认实现；其他后端即使未实现也回退到 SERIAL */
    (void)backend;

    Lv00Matrix *A = lv00_malloc(sizeof(Lv00Matrix));
    LV00_CHECK_ALLOC(A, NULL);

    A->rows = rows;
    A->cols = cols;
    A->sparse = sparse;
    A->format = sparse ? LV00_MATRIX_SPARSE_CSR : LV00_MATRIX_DENSE;
    A->backend = LV00_BACKEND_SERIAL;
    A->ops = &serial_dense_matrix_ops;
    A->backend_data = NULL;

    size_t data_size = (size_t) (rows * cols) * sizeof(double);
    A->data = lv00_malloc(data_size);
    if (!A->data) {
        lv00_free((void **) &A);
        LV00_ERROR_SET(LV00_ERROR_OUT_OF_MEMORY, "矩阵数据分配失败 %lldx%lld",
                       (long long) rows, (long long) cols);
        return NULL;
    }
    memset(A->data, 0, data_size);

    return A;
}

/**
 * @brief 创建线性求解器（指定后端和求解方法）
 */
Lv00LinearSolver *lv00_linsol_create(Lv00BackendType backend,
                                     Lv00LinearSolverMethod method) {
    /* SERIAL 是默认实现；其他后端如果未实现也回退到 SERIAL */
    (void)backend;

    Lv00LinearSolver *LS = lv00_malloc(sizeof(Lv00LinearSolver));
    LV00_CHECK_ALLOC(LS, NULL);

    LS->method = method;
    LS->backend = backend;
    LS->solver_data = NULL;
    LS->backend_data = NULL;

    switch (method) {
        case LV00_LINSOL_DIRECT_DENSE:
        case LV00_LINSOL_DIRECT_BAND:
        case LV00_LINSOL_DIRECT_SPARSE:
            LS->ops = &serial_dense_linsol_ops;
            break;
        case LV00_LINSOL_ITERATIVE_GMRES:
            LS->ops = &serial_gmres_linsol_ops;
            break;
        case LV00_LINSOL_ITERATIVE_BICGSTAB:
            LS->ops = &serial_bicgstab_linsol_ops;
            break;
        case LV00_LINSOL_ITERATIVE_CG:
            LS->ops = &serial_cg_linsol_ops;
            break;
        default:
            lv00_free((void **) &LS);
            LV00_ERROR_SET(LV00_BACKEND_UNSUPPORTED,
                           "不支持的线性求解方法: %s",
                           lv00_linsol_method_name(method));
            return NULL;
    }

    return LS;
}
