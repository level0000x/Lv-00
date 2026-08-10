/**
 * @file numerical_backend.c
 * @brief 多后端数值抽象层 —— SERIAL 后端完整实现
 *
 * @details 实现 lvVector / lvMatrix / lvLinearSolver 在 SERIAL 后端的
 *          全部操作函数和工厂函数。借鉴 SUNDIALS N_Vector/SUNMatrix/SUNLinearSolver
 *          的三层抽象设计，每个结构体绑定自己的操作表。
 *
 *          当前实现完整覆盖 SERIAL 后端与 OpenMP 后端。
 *          CUDA / HIP / Singular 后端通过单独编译单元实现，
 *          由 CMake SDK 检测（lv_ENABLE_CUDA / lv_ENABLE_HIP / lv_ENABLE_SINGULAR）
 *          条件编译，SDK 未安装时优雅降级返回 lv_BACKEND_UNSUPPORTED。
 *
 * @author Lv-00 Project
 * @version v3.3.0
 * @date 2026-05-24
 *
 * @dependencies
 *   - numerical_backend.h     : 后端公共接口与类型定义
 *   - lv_utils.h            : 统一内存分配器
 *   - lv_internal.h         : 内部常量与工具宏
 *   - error_codes.h           : 统一错误码系统
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "numerical_backend.h"

#include "lv/bicgstab_shared.h"
#include "lv/gmres_shared.h"
#include "lv/default_host_ops.h"
#include "lv/host_linalg.h"
#include "lv/sparse_linear_algebra.h" /* CSR 稀疏矩阵 + Jacobi 求解 */

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_numeric.h" /* lv_index_in_range */

#include "error_codes.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_thread.h"  /* lv_MUTEX_* 兼容宏依赖 lv_mutex_* 实现 */

/* 多后端支持（条件编译，由 CMake SDK 检测控制） */
#ifdef LV_HAS_CUDA
#include "lv/backends/cuda_backend.h"
#endif
#ifdef LV_HAS_HIP
#include "lv/backends/hip_backend.h"
#endif
#ifdef LV_HAS_SINGULAR
#include "lv/backends/singular_backend.h"
#endif

#ifndef lv_NUM_EPSILON
#define lv_NUM_EPSILON lv_EPSILON_MEDIUM
#endif

/* ========================================================================
 * 后端注册表（集中式后端分发机制）
 *
 * 取代之前的 #ifdef LV_HAS_CUDA / LV_HAS_HIP / LV_HAS_SINGULAR 编译时
 * 三重分发模式。各后端在初始化时通过 lv_numerical_backend_register()
 * 注册其操作表，工厂函数通过查表找到对应操作集。
 * ======================================================================== */

/** @brief 注册表最大容量 */
#define lv_MAX_BACKENDS 8

/**
 * @brief 后端注册表条目
 */
typedef struct {
    lvBackendType type;                    /**< 后端类型 */
    const lvVectorOps *vector_ops;         /**< 向量操作表 */
    const lvMatrixOps *matrix_ops;         /**< 矩阵操作表 */
    const lvLinearSolverOps *linsol_ops;   /**< 线性求解器操作表 */
} BackendEntry;

/** @brief 数值后端注册表单例状态 */
typedef struct {
    BackendEntry entries[lv_MAX_BACKENDS]; /**< 后端注册表 */
    int count;                             /**< 已注册后端数量 */
    lv_lazy_lock lock;                     /**< 注册表互斥锁（惰性初始化，首次使用自动初始化） */
    bool initialized;                      /**< 注册表是否已初始化 */
} NumericBackendState;

/** @brief 数值后端注册表全局单例 */
static NumericBackendState s_numeric_state = {0};

/** @brief 注册表互斥锁的一次性初始化回调（由 lv_lazy_lock 触发，线程安全） */
static void numeric_state_lock_init_once(void) {
    lv_mutex_init(&s_numeric_state.lock.mutex);
}

/* 前向声明（用于下方 SERIAL 操作表）
 * 主机端数据搬移 ops 由 default_host_ops.c 提供（default_*），
 * 此处仅声明 SERIAL 特有的 create 与计算型 ops。 */
static lvVector *serial_vector_create(int64_t n);
static void serial_vector_scale(lvVector *v, double c);
static void serial_vector_linear_sum(double a, const lvVector *x, double b, const lvVector *y, lvVector *z);
static double serial_vector_dot(const lvVector *x, const lvVector *y);
static double serial_vector_norm(const lvVector *v);
static double serial_vector_max_norm(const lvVector *v);
static double serial_vector_wrms_norm(const lvVector *v, const lvVector *weights);

static lvMatrix *serial_matrix_create(int64_t rows, int64_t cols, bool sparse);
static int serial_matrix_matvec(const lvMatrix *A, const lvVector *x, lvVector *y);
static int serial_matrix_factor(lvMatrix *A);
static int serial_matrix_solve(const lvMatrix *A, const lvVector *b, lvVector *x);

/* CSR 稀疏矩阵分支操作表函数（serial_sparse_matrix_ops） */
static lvMatrix *sparse_matrix_clone(const lvMatrix *A);
static void sparse_matrix_destroy(lvMatrix *A);
static void sparse_matrix_zero(lvMatrix *A);
static void sparse_matrix_copy(lvMatrix *dst, const lvMatrix *src);
static int sparse_matrix_matvec(const lvMatrix *A, const lvVector *x, lvVector *y);
static void sparse_matrix_scale(lvMatrix *A, double c);
static void sparse_matrix_set_element(lvMatrix *A, int64_t row, int64_t col, double val);
static double sparse_matrix_get_element(const lvMatrix *A, int64_t row, int64_t col);
static int sparse_matrix_factor(lvMatrix *A);
static int sparse_matrix_solve(const lvMatrix *A, const lvVector *b, lvVector *x);

static lvLinearSolver *serial_linsol_create(lvLinearSolverMethod method);
static int serial_linsol_setup(lvLinearSolver *LS, const lvMatrix *A);
static int serial_linsol_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);
static void serial_linsol_destroy(lvLinearSolver *LS);
static void serial_iter_linsol_destroy(lvLinearSolver *LS);

static int iterative_gmres_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);
static int iterative_bicgstab_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);
static int iterative_cg_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);

/**
 * 静态操作表（SERIAL 后端）
 */
static const lvVectorOps serial_vector_ops = {
    serial_vector_create,
    default_vector_clone, default_vector_destroy,  default_vector_zero,       default_vector_const_set,
    default_vector_copy,  serial_vector_scale,    serial_vector_linear_sum, serial_vector_dot,
    serial_vector_norm,  serial_vector_max_norm, serial_vector_wrms_norm,  default_vector_abs,
    default_vector_inv,   default_vector_compare,  default_vector_length,     default_vector_data_ptr,
};

static const lvMatrixOps serial_dense_matrix_ops = {
    serial_matrix_create,
    default_matrix_clone,  default_matrix_destroy, default_matrix_zero,        default_matrix_copy,
    serial_matrix_matvec, default_matrix_scale,   default_matrix_set_element, default_matrix_get_element,
    serial_matrix_factor, serial_matrix_solve,
};

/* CSR 稀疏矩阵操作表：lv_matrix_create(..., sparse=true) 时绑定。
 * data 字段持有 lvSparseMatrix*（见 lvMatrix 注释："CSR 为自定义结构"）。 */
static const lvMatrixOps serial_sparse_matrix_ops = {
    serial_matrix_create,
    sparse_matrix_clone,   sparse_matrix_destroy, sparse_matrix_zero,        sparse_matrix_copy,
    sparse_matrix_matvec, sparse_matrix_scale,   sparse_matrix_set_element, sparse_matrix_get_element,
    sparse_matrix_factor, sparse_matrix_solve,
};

static const lvLinearSolverOps serial_dense_linsol_ops = {
    serial_linsol_create,
    serial_linsol_setup,
    serial_linsol_solve,
    serial_linsol_destroy,
};

/** @brief 注册后端操作集（调用方须持有 s_numeric_state.lock） */
static void numerical_backend_register_locked(lvBackendType backend_type,
                                              const lvVectorOps *vector_ops,
                                              const lvMatrixOps *matrix_ops,
                                              const lvLinearSolverOps *linsol_ops) {
    if (s_numeric_state.count < lv_MAX_BACKENDS) {
        s_numeric_state.entries[s_numeric_state.count].type       = backend_type;
        s_numeric_state.entries[s_numeric_state.count].vector_ops = vector_ops;
        s_numeric_state.entries[s_numeric_state.count].matrix_ops = matrix_ops;
        s_numeric_state.entries[s_numeric_state.count].linsol_ops = linsol_ops;
        s_numeric_state.count++;
    }
}

/**
 * @brief 初始化注册表并注册内置后端
 *
 * 持锁重查 initialized，消除并发首次调用时的双重初始化竞态；
 * 内置注册走无锁内部函数，避免嵌套加锁。
 */
static void numerical_backend_init_registry(void) {
    lv_lazy_lock_lock(&s_numeric_state.lock, numeric_state_lock_init_once);
    if (!s_numeric_state.initialized) {
        numerical_backend_register_locked(lv_BACKEND_SERIAL, &serial_vector_ops,
                                          &serial_dense_matrix_ops, &serial_dense_linsol_ops);
        numerical_backend_register_locked(lv_BACKEND_OPENMP, &serial_vector_ops,
                                          &serial_dense_matrix_ops, &serial_dense_linsol_ops);
        s_numeric_state.initialized = true;
    }
    lv_lazy_lock_unlock(&s_numeric_state.lock);
}

/**
 * @brief 注册数值后端操作集
 *
 * 惰性锁首次使用时自动完成互斥锁初始化（此前 mutex 仅在
 * numerical_backend_init_registry 中初始化，外部后端（如 Singular）
 * 直接调用本函数会锁定未初始化的互斥锁）。
 */
void lv_numerical_backend_register(lvBackendType backend_type,
                                   const lvVectorOps *vector_ops,
                                   const lvMatrixOps *matrix_ops,
                                   const lvLinearSolverOps *linsol_ops) {
    lv_lazy_lock_lock(&s_numeric_state.lock, numeric_state_lock_init_once);
    numerical_backend_register_locked(backend_type, vector_ops, matrix_ops, linsol_ops);
    lv_lazy_lock_unlock(&s_numeric_state.lock);
}

/**
 * @brief 查找指定后端的向量操作表
 */
static const lvVectorOps *find_vector_ops(lvBackendType type) {
    for (int i = 0; i < s_numeric_state.count; i++) {
        if (s_numeric_state.entries[i].type == type) return s_numeric_state.entries[i].vector_ops;
    }
    return NULL;
}

/**
 * @brief 查找指定后端的矩阵操作表
 */
static const lvMatrixOps *find_matrix_ops(lvBackendType type) {
    for (int i = 0; i < s_numeric_state.count; i++) {
        if (s_numeric_state.entries[i].type == type) return s_numeric_state.entries[i].matrix_ops;
    }
    return NULL;
}

/**
 * @brief 查找指定后端的线性求解器操作表
 */
static const lvLinearSolverOps *find_linsol_ops(lvBackendType type) {
    for (int i = 0; i < s_numeric_state.count; i++) {
        if (s_numeric_state.entries[i].type == type) return s_numeric_state.entries[i].linsol_ops;
    }
    return NULL;
}

/* ========================================================================
 * 模块级常量定义
 * ======================================================================== */

/** @brief 小型行/列块大小（用于 LU 分解的临时工作区） */
#define NBMAT_WORK_BLOCK_SIZE 64

/* ========================================================================
 * 静态辅助函数的前向声明
 * ======================================================================== */

static void serial_vector_scale(lvVector *v, double c);
static void serial_vector_linear_sum(double a, const lvVector *x, double b, const lvVector *y, lvVector *z);
static double serial_vector_dot(const lvVector *x, const lvVector *y);
static double serial_vector_norm(const lvVector *v);
static double serial_vector_max_norm(const lvVector *v);
static double serial_vector_wrms_norm(const lvVector *v, const lvVector *weights);

static int serial_matrix_matvec(const lvMatrix *A, const lvVector *x, lvVector *y);
static int serial_matrix_factor(lvMatrix *A);
static int serial_matrix_solve(const lvMatrix *A, const lvVector *b, lvVector *x);

static int serial_linsol_setup(lvLinearSolver *LS, const lvMatrix *A);
static int serial_linsol_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);
static void serial_linsol_destroy(lvLinearSolver *LS);

/* 迭代法接口声明（完整实现见下方：GMRES/BiCGSTAB 委托共享内核，CG 内联实现） */
static int iterative_gmres_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);
static int iterative_bicgstab_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);
static int iterative_cg_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);

/* SERIAL 共享迭代内核算子声明（GMRES 复用 BiCGSTAB 算子，见下方实现） */
static double serial_bicgstab_dot(void *ctx, const double *a, const double *b, int64_t n);
static double serial_bicgstab_norm(void *ctx, const double *v, int64_t n);
static void serial_bicgstab_matvec(void *ctx, const lvMatrix *A, const double *x, double *y, int64_t n);

/* SERIAL 后端 create 函数声明 */
static lvVector *serial_vector_create(int64_t n);
static lvMatrix *serial_matrix_create(int64_t rows, int64_t cols, bool sparse);
static lvLinearSolver *serial_linsol_create(lvLinearSolverMethod method);

/** @brief 迭代法 GMRES 求解器操作表 */
static const lvLinearSolverOps serial_gmres_linsol_ops = {
    serial_linsol_create,
    serial_linsol_setup,
    iterative_gmres_solve,
    serial_iter_linsol_destroy,
};

/** @brief 迭代法 BiCGSTAB 求解器操作表 */
static const lvLinearSolverOps serial_bicgstab_linsol_ops = {
    serial_linsol_create,
    serial_linsol_setup,
    iterative_bicgstab_solve,
    serial_iter_linsol_destroy,
};

/** @brief 迭代法 CG 求解器操作表 */
static const lvLinearSolverOps serial_cg_linsol_ops = {
    serial_linsol_create,
    serial_linsol_setup,
    iterative_cg_solve,
    serial_iter_linsol_destroy,
};

/* ========================================================================
 * 第一部分：向量操作实现（SERIAL）
 * ======================================================================== */

/**
 * @brief 标量乘法：v = c * v
 */
static void serial_vector_scale(lvVector *v, double c) {
    if (!v || !v->data) {
        return;
    }
#ifdef _OPENMP
    #pragma omp parallel for
#endif
    for (int64_t i = 0; i < v->length; ++i) {
        v->data[i] *= c;
    }
}

/**
 * @brief 线性组合：z = a*x + b*y
 */
static void serial_vector_linear_sum(double a, const lvVector *x, double b, const lvVector *y, lvVector *z) {
    if (!x || !y || !z || !x->data || !y->data || !z->data) {
        return;
    }
    int64_t n = x->length;
    /* 使用最短的向量长度避免越界 */
    if (y->length < n)
        n = y->length;
    if (z->length < n)
        n = z->length;
#ifdef _OPENMP
    #pragma omp parallel for
#endif
    for (int64_t i = 0; i < n; ++i) {
        z->data[i] = a * x->data[i] + b * y->data[i];
    }
}

/**
 * @brief 点积（内积）
 */
static double serial_vector_dot(const lvVector *x, const lvVector *y) {
    if (!x || !y || !x->data || !y->data) {
        return 0.0;
    }
    double sum = 0.0;
    int64_t n = x->length;
    if (y->length < n)
        n = y->length;
#ifdef _OPENMP
    #pragma omp parallel for reduction(+:sum)
#endif
    for (int64_t i = 0; i < n; ++i) {
        sum += x->data[i] * y->data[i];
    }
    return sum;
}

/**
 * @brief L2 范数
 */
static double serial_vector_norm(const lvVector *v) {
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
static double serial_vector_max_norm(const lvVector *v) {
    if (!v || !v->data || v->length == 0) {
        return 0.0;
    }
    return lv_max_abs(v->data, v->length);
}

/**
 * @brief 加权 RMS 范数 —— 借鉴 SUNDIALS 误差控制
 *
 * WRMS-norm = sqrt( (1/n) * sum_i (v_i * w_i)^2 )
 */
static double serial_vector_wrms_norm(const lvVector *v, const lvVector *weights) {
    if (!v || !weights || !v->data || !weights->data || v->length == 0) {
        return 0.0;
    }
    double sum_sq = 0.0;
    int64_t n = v->length;
    if (weights->length < n)
        n = weights->length;
    for (int64_t i = 0; i < n; ++i) {
        double wv = v->data[i] * weights->data[i];
        sum_sq += wv * wv;
    }
    return sqrt(sum_sq / (double) n);
}

/* ========================================================================
 * 第二部分：矩阵操作实现（SERIAL 稠密）
 * ======================================================================== */

/**
 * @brief 矩阵-向量乘法：y = A * x（列主序稠密）
 *
 * 对于行 i，y[i] = sum_j A[j * rows + i] * x[j]
 */
static int serial_matrix_matvec(const lvMatrix *A, const lvVector *x, lvVector *y) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(y, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(x->data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(y->data, lv_BACKEND_NOT_INITIALIZED);

    if (A->cols != x->length || A->rows != y->length) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "matvec 维度不匹配: A(%lldx%lld), x(%lld), y(%lld)", (long long) A->rows,
                     (long long) A->cols, (long long) x->length, (long long) y->length);
        return lv_BACKEND_INVALID_ARGS;
    }

    double *data = (double *) A->data;
    int64_t rows = A->rows;
    int64_t cols = A->cols;

#ifdef _OPENMP
    #pragma omp parallel for
#endif
    for (int64_t i = 0; i < rows; ++i) {
        double sum = 0.0;
        for (int64_t j = 0; j < cols; ++j) {
            double xj = x->data[j];
            if (fabs(xj) < lv_NUM_EPSILON)
                continue;
            sum += data[j * rows + i] * xj;
        }
        y->data[i] = sum;
    }

    return lv_BACKEND_OK;
}

/**
 * @brief 就地 LU 分解（列主序稠密，部分选主元）
 *
 * 将矩阵 A 覆盖为包含 L 和 U 的紧凑形式。
 * 对角线元素属于 U（L 的对角线为 1，隐式不存储）。
 *
 * @return 成功返回 lv_BACKEND_OK，奇异矩阵返回 lv_BACKEND_SINGULAR
 */
static int serial_matrix_factor(lvMatrix *A) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->data, lv_BACKEND_NOT_INITIALIZED);

    if (A->rows != A->cols) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "LU分解要求方阵，当前为 %lldx%lld", (long long) A->rows,
                     (long long) A->cols);
        return lv_BACKEND_INVALID_ARGS;
    }

    return host_lu_factor((double *) A->data, A->rows, lv_EPSILON_DOUBLE);
}

/**
 * @brief 使用已分解矩阵求解 A * x = b
 *
 * 前代消去（利用 L）后再回代（利用 U）。
 */
static int serial_matrix_solve(const lvMatrix *A, const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(b->data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(x->data, lv_BACKEND_NOT_INITIALIZED);

    if (A->rows != A->cols) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "solve要求方阵，当前为 %lldx%lld", (long long) A->rows,
                     (long long) A->cols);
        return lv_BACKEND_INVALID_ARGS;
    }

    return host_lu_solve((const double *) A->data, A->rows, (const double *) b->data, (double *) x->data);
}

/* ========================================================================
 * 第三部分：线性求解器实现（SERIAL 稠密 LU）
 * ======================================================================== */

/**
 * @brief 稠密 LU 求解器私有数据结构
 */
typedef struct DenseLUData {
    lvMatrix *clone; /**< 矩阵副本（用于保留分解结果） */
    bool factored;   /**< 是否已完成分解 */
} DenseLUData;

/**
 * @brief 释放求解器缓存的矩阵副本（LU clone/destroy 配对样板收敛，判据 A）
 *
 * 语义契约：m 非 NULL 时经操作表调用 m->ops->destroy(m)。
 * 前置条件：m 必须由矩阵操作表 clone 创建。
 * 失败/截断语义：无失败路径。
 * 边界行为：m == NULL → 无操作。
 * exempt: 与 hip_backend.c 的同名 static 函数逐字同构（判据 A 候选）；
 *         统一为公共头 static inline 设施需修改白名单外头文件（本批次禁止），
 *         保留双 static 副本，行为等价。
 * 扩展点：无。
 */
static void linsol_clone_destroy(lvMatrix *m) {
    if (m) {
        m->ops->destroy(m);
    }
}

/**
 * @brief 设置线性求解器
 *
 * 为稠密直接法分配一个与模板矩阵 A 同形的副本。
 */
static int serial_linsol_setup(lvLinearSolver *LS, const lvMatrix *A) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);

    DenseLUData *lu = lv_calloc(1, sizeof(DenseLUData));
    lv_CHECK_ALLOC(lu, lv_BACKEND_MEM_ERROR);

    lu->clone = A->ops->clone(A); /* 经操作表克隆，sparse 矩阵走 CSR 深拷贝 */
    if (!lu->clone) {
        lv_free((void **) &lu);
        return lv_BACKEND_MEM_ERROR;
    }
    lu->factored = false;

    /* 释放旧数据 */
    if (LS->solver_data) {
        DenseLUData *old = (DenseLUData *) LS->solver_data;
        linsol_clone_destroy(old->clone);
        lv_free((void **) &LS->solver_data);
    }
    LS->solver_data = lu;

    return lv_BACKEND_OK;
}

/**
 * @brief 使用稠密 LU 求解 A * x = b
 *
 * 首次调用时自动执行 LU 分解。
 */
static int serial_linsol_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);

    DenseLUData *lu = (DenseLUData *) LS->solver_data;
    if (!lu || !lu->clone) {
        lv_ERROR_SET(lv_BACKEND_NOT_INITIALIZED, "线性求解器未初始化，请先调用 setup");
        return lv_BACKEND_NOT_INITIALIZED;
    }

    /* 若 A 与已分解矩阵为不同维度，则需重新 setup */
    if (lu->clone->rows != A->rows || lu->clone->cols != A->cols) {
        /* 重新 setup */
        int ret = serial_linsol_setup(LS, A);
        if (ret != lv_BACKEND_OK) {
            return ret;
        }
        lu = (DenseLUData *) LS->solver_data;
    }

    /* 需重新分解时复制矩阵并分解 */
    if (!lu->factored) {
        lu->clone->ops->copy(lu->clone, A);
        int ret = lu->clone->ops->factor(lu->clone);
        if (ret != lv_BACKEND_OK) {
            return ret;
        }
        lu->factored = true;
    }

    return lu->clone->ops->solve(lu->clone, b, x);
}

/**
 * @brief 销毁线性求解器
 */
static void serial_linsol_destroy(lvLinearSolver *LS) {
    if (!LS) {
        return;
    }
    if (LS->solver_data) {
        DenseLUData *lu = (DenseLUData *) LS->solver_data;
        linsol_clone_destroy(lu->clone);
        lv_free((void **) &LS->solver_data);
    }
    lv_free((void **) &LS);
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
    lvMatrix *clone; /**< 矩阵副本 */
    int max_iters;   /**< 最大迭代次数 */
    double tol;      /**< 收敛容差 */
    double *r;       /**< 残差向量 */
    double *p;       /**< 方向向量 */
    double *ap;      /**< A*p 向量 */
    double *work;    /**< 通用工作向量 */
} IterSolverData;

static void serial_iter_linsol_destroy(lvLinearSolver *LS) {
    if (!LS) {
        return;
    }
    if (LS->solver_data) {
        IterSolverData *is = (IterSolverData *) LS->solver_data;
        linsol_clone_destroy(is->clone);
        if (is->r) lv_free((void **) &is->r);
        if (is->p) lv_free((void **) &is->p);
        if (is->ap) lv_free((void **) &is->ap);
        if (is->work) lv_free((void **) &is->work);
        lv_free((void **) &LS->solver_data);
    }
    lv_free((void **) &LS);
}

/**
 * @brief 设置迭代求解器
 */
static int serial_iter_setup(lvLinearSolver *LS, const lvMatrix *A) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);

    IterSolverData *is = lv_calloc(1, sizeof(IterSolverData));
    lv_CHECK_ALLOC(is, lv_BACKEND_MEM_ERROR);
    lv_linsol_default_params(&is->max_iters, &is->tol);

    is->clone = default_matrix_clone(A);
    if (!is->clone) {
        lv_free((void **) &is);
        return lv_BACKEND_MEM_ERROR;
    }

    int64_t n = A->rows;
    is->r = lv_calloc((size_t) n, sizeof(double));
    is->p = lv_calloc((size_t) n, sizeof(double));
    is->ap = lv_calloc((size_t) n, sizeof(double));
    is->work = lv_calloc((size_t) n, sizeof(double));

    if (!is->r || !is->p || !is->ap || !is->work) {
        /* 清理已分配资源 */
        if (is->r)
            lv_free((void **) &is->r);
        if (is->p)
            lv_free((void **) &is->p);
        if (is->ap)
            lv_free((void **) &is->ap);
        if (is->work)
            lv_free((void **) &is->work);
        linsol_clone_destroy(is->clone);
        lv_free((void **) &is);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "迭代求解器工作区分配失败");
        return lv_BACKEND_MEM_ERROR;
    }

    /* 释放旧数据 */
    if (LS->solver_data) {
        IterSolverData *old = (IterSolverData *) LS->solver_data;
        linsol_clone_destroy(old->clone);
        if (old->r)
            lv_free((void **) &old->r);
        if (old->p)
            lv_free((void **) &old->p);
        if (old->ap)
            lv_free((void **) &old->ap);
        if (old->work)
            lv_free((void **) &old->work);
        lv_free((void **) &LS->solver_data);
    }
    LS->solver_data = is;

    return lv_BACKEND_OK;
}

/**
 * @brief GMRES(m) 求解器 —— 带重启的广义最小残差法
 *
 * @details 使用 Modified Gram-Schmidt 正交化的 Arnoldi 过程构建上 Hessenberg 矩阵，
 *          通过 Givens 旋转求解最小二乘问题，每 m=30 步重启一次。
 *          算法主体委托给共享内核 lv_gmres_solve()（gmres_shared.c），
 *          此处仅提供 SERIAL 算子表（点积/范数/矩阵向量乘，复用下方
 *          BiCGSTAB 共享内核的 SERIAL 算子）。
 */
static int iterative_gmres_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);

    IterSolverData *is = (IterSolverData *) LS->solver_data;
    if (!is) {
        int ret = serial_iter_setup(LS, A);
        if (ret != lv_BACKEND_OK)
            return ret;
        is = (IterSolverData *) LS->solver_data;
    }

    lvGmresOps ops;
    ops.ctx = is;
    ops.vector_dot = serial_bicgstab_dot;
    ops.vector_norm = serial_bicgstab_norm;
    ops.matvec = serial_bicgstab_matvec;

    return lv_gmres_solve(&ops, A, b->data, x->data, A->rows,
                          is->max_iters, is->tol, lv_EPSILON_DOUBLE, 30);
}

/* ========================================================================
 * BiCGSTAB 共享内核的 SERIAL 算子
 * ======================================================================== */

/** @brief SERIAL 点积：<a, b>（与原实现内联求和顺序一致） */
static double serial_bicgstab_dot(void *ctx, const double *a, const double *b, int64_t n) {
    (void) ctx;
    double sum = 0.0;
    for (int64_t i = 0; i < n; ++i)
        sum += a[i] * b[i];
    return sum;
}

/** @brief SERIAL L2 范数：||v||_2 */
static double serial_bicgstab_norm(void *ctx, const double *v, int64_t n) {
    (void) ctx;
    double sum = 0.0;
    for (int64_t i = 0; i < n; ++i)
        sum += v[i] * v[i];
    return sqrt(sum);
}

/** @brief SERIAL 矩阵向量乘：y = A*x（列主序，跳过微小分量，与原实现一致） */
static void serial_bicgstab_matvec(void *ctx, const lvMatrix *A, const double *x, double *y, int64_t n) {
    lvMatrix *clone = ((IterSolverData *) ctx)->clone;
    if (clone->sparse) {
        /* CSR 稀疏矩阵：直接行遍历（y = A*x） */
        lv_sparse_matvec((const lvSparseMatrix *) clone->data, x, y);
        return;
    }
    double *a_data = (double *) clone->data;
    memset(y, 0, (size_t) n * sizeof(double));
    for (int64_t j = 0; j < A->cols; ++j) {
        double xj = x[j];
        if (fabs(xj) < lv_NUM_EPSILON)
            continue;
        double *col_j = a_data + j * n;
        for (int64_t i = 0; i < n; ++i)
            y[i] += col_j[i] * xj;
    }
}

/**
 * @brief BiCGSTAB 求解器 —— 稳定双共轭梯度法 (van der Vorst 1992)
 *
 * @details 标准 BiCGSTAB 算法，使用两组递推关系计算影子残差，
 *          相比 BiCG 具有更平滑的收敛行为。
 *          算法主体委托给共享内核 lv_bicgstab_solve()（bicgstab_shared.c），
 *          此处仅提供 SERIAL 算子表（点积/范数/矩阵向量乘）。
 */
static int iterative_bicgstab_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);

    IterSolverData *is = (IterSolverData *) LS->solver_data;
    if (!is) {
        int ret = serial_iter_setup(LS, A);
        if (ret != lv_BACKEND_OK)
            return ret;
        is = (IterSolverData *) LS->solver_data;
    }

    lvBicgstabOps ops;
    ops.ctx = is;
    ops.vector_dot = serial_bicgstab_dot;
    ops.vector_norm = serial_bicgstab_norm;
    ops.matvec = serial_bicgstab_matvec;

    return lv_bicgstab_solve(&ops, A, b->data, x->data, A->rows,
                             is->max_iters, is->tol, lv_EPSILON_DOUBLE);
}

/**
 * @brief 共轭梯度 (CG) 求解器 —— Hestenes-Stiefel 算法
 *
 * @details 标准共轭梯度法，适用于对称正定 (SPD) 线性系统。
 *          使用短递推关系，每步仅需一次矩阵-向量乘法和两次内积。
 *          工作向量：r=is->r, p=is->p, Ap=is->ap
 */
static int iterative_cg_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);

    IterSolverData *is = (IterSolverData *) LS->solver_data;
    if (!is) {
        int ret = serial_iter_setup(LS, A);
        if (ret != lv_BACKEND_OK)
            return ret;
        is = (IterSolverData *) LS->solver_data;
    }

    int64_t n = A->rows;
    int max_iter = is->max_iters;
    double tol = is->tol;

    /* 工作向量别名 */
    double *r = is->r;   /* 残差 */
    double *p = is->p;   /* 搜索方向 */
    double *ap = is->ap; /* A*p */

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

    double threshold = (b_norm > lv_EPSILON_DOUBLE) ? tol * b_norm : tol;

    for (int iter = 0; iter < max_iter; ++iter) {
        /* ap = A * p */
        if (is->clone->sparse) {
            lv_sparse_matvec((const lvSparseMatrix *) is->clone->data, p, ap);
        } else {
            double *a_data = (double *) is->clone->data;
            memset(ap, 0, (size_t) n * sizeof(double));
            for (int64_t j = 0; j < A->cols; ++j) {
                double pj = p[j];
                if (fabs(pj) < lv_NUM_EPSILON)
                    continue;
                double *col_j = a_data + j * n;
                for (int64_t i = 0; i < n; ++i)
                    ap[i] += col_j[i] * pj;
            }
        }

        /* alpha = (r, r) / (p, A*p) */
        double pap = 0.0;
        for (int64_t i = 0; i < n; ++i)
            pap += p[i] * ap[i];
        if (fabs(pap) < lv_EPSILON_DOUBLE)
            break;
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
        if (r_norm < threshold)
            break;

        /* beta = (r_new, r_new) / (r_old, r_old) */
        if (fabs(rs_old) < lv_EPSILON_DOUBLE)
            rs_old = 1.0;
        double beta = rs_new / rs_old;

        /* p = r + beta * p */
        for (int64_t i = 0; i < n; ++i)
            p[i] = r[i] + beta * p[i];

        rs_old = rs_new;
    }

    return lv_BACKEND_OK;
}

/* ========================================================================
 * 第五部分：SERIAL 后端 create 函数实现
 * ======================================================================== */

/**
 * @brief 创建 SERIAL 后端向量
 */
static lvVector *serial_vector_create(int64_t n) {
    if (n <= 0) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "向量长度必须为正整数，当前 n=%lld", (long long) n);
        return NULL;
    }

    lvVector *v = lv_calloc(1, sizeof(lvVector));
    lv_CHECK_ALLOC(v, NULL);

    v->length = n;
    v->backend = lv_BACKEND_SERIAL;
    v->ops = &serial_vector_ops;
    v->backend_data = NULL;

    v->data = lv_calloc((size_t) n, sizeof(double));
    if (!v->data) {
        lv_free((void **) &v);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "向量数据分配失败，长度=%lld", (long long) n);
        return NULL;
    }
    memset(v->data, 0, (size_t) n * sizeof(double));

    return v;
}

/**
 * @brief 创建 SERIAL 后端矩阵
 */
static lvMatrix *serial_matrix_create(int64_t rows, int64_t cols, bool sparse) {
    if (rows <= 0 || cols <= 0) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "矩阵维度必须为正整数，当前 %lldx%lld", (long long) rows,
                     (long long) cols);
        return NULL;
    }

    lvMatrix *A = lv_calloc(1, sizeof(lvMatrix));
    lv_CHECK_ALLOC(A, NULL);

    A->rows = rows;
    A->cols = cols;
    A->sparse = sparse;
    A->format = sparse ? lv_MATRIX_SPARSE_CSR : lv_MATRIX_DENSE;
    A->backend = lv_BACKEND_SERIAL;
    A->backend_data = NULL;

    /* 稀疏分支：data 挂 CSR 矩阵（lvSparseMatrix*），ops 绑定稀疏操作表。
     * 仅在 nnz 处存储非零元素，消除"稀疏标志 + 稠密存储"的假稀疏。 */
    if (sparse) {
        if (rows > INT_MAX || cols > INT_MAX) {
            lv_free((void **) &A);
            lv_ERROR_SET(lv_BACKEND_INVALID_ARGS,
                         "稀疏矩阵维度超出 CSR 支持范围(int32)，当前 %lldx%lld", (long long) rows,
                         (long long) cols);
            return NULL;
        }
        A->ops = &serial_sparse_matrix_ops;
        A->data = lv_sparse_create((int) rows, (int) cols);
        if (!A->data) {
            lv_free((void **) &A);
            lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "稀疏矩阵 CSR 数据分配失败 %lldx%lld", (long long) rows,
                         (long long) cols);
            return NULL;
        }
        return A;
    }

    A->ops = &serial_dense_matrix_ops;

    size_t data_size = (size_t) (rows * cols) * sizeof(double);
    A->data = lv_malloc(data_size);
    if (!A->data) {
        lv_free((void **) &A);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "矩阵数据分配失败 %lldx%lld", (long long) rows, (long long) cols);
        return NULL;
    }
    memset(A->data, 0, data_size);

    return A;
}

/* ========================================================================
 * 第五部分（续）：CSR 稀疏矩阵分支操作表实现
 *
 * data 字段持有 lvSparseMatrix*（由 lv_sparse_create 创建），所有操作
 * 委托给 sparse_linear_algebra 模块。与稠密分支的生命周期契约一致：
 * create 分配、destroy 释放、clone 深拷贝。
 * ======================================================================== */

/**
 * @brief 深拷贝稀疏矩阵（继承源矩阵后端与操作表）
 */
static lvMatrix *sparse_matrix_clone(const lvMatrix *A) {
    lv_CHECK_NULL(A, NULL);
    lv_CHECK_NULL(A->data, NULL);

    lvMatrix *clone = lv_calloc(1, sizeof(lvMatrix));
    lv_CHECK_ALLOC(clone, NULL);

    clone->rows = A->rows;
    clone->cols = A->cols;
    clone->sparse = true;
    clone->format = lv_MATRIX_SPARSE_CSR;
    clone->backend = A->backend;
    clone->ops = A->ops;
    clone->backend_data = NULL;

    clone->data = lv_sparse_create((int) A->rows, (int) A->cols);
    if (!clone->data) {
        lv_free((void **) &clone);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "sparse_matrix_clone: lv_sparse_create failed");
        return NULL;
    }
    if (lv_sparse_copy((lvSparseMatrix *) clone->data, (const lvSparseMatrix *) A->data) != 0) {
        lv_sparse_destroy((lvSparseMatrix *) clone->data);
        lv_free((void **) &clone);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "sparse_matrix_clone: lv_sparse_copy failed");
        return NULL;
    }
    return clone;
}

/**
 * @brief 销毁稀疏矩阵（释放 CSR 内部数组与矩阵本体）
 */
static void sparse_matrix_destroy(lvMatrix *A) {
    if (!A)
        return;
    lv_sparse_destroy((lvSparseMatrix *) A->data);
    A->data = NULL;
    lv_free((void **) &A);
}

/**
 * @brief 置零（清空所有非零元素，保留 CSR 容量）
 */
static void sparse_matrix_zero(lvMatrix *A) {
    if (!A || !A->data)
        return;
    lv_sparse_zero((lvSparseMatrix *) A->data);
}

/**
 * @brief 深拷贝：dst = src（均为稀疏矩阵）
 */
static void sparse_matrix_copy(lvMatrix *dst, const lvMatrix *src) {
    if (!dst || !src || !dst->data || !src->data)
        return;
    lv_sparse_copy((lvSparseMatrix *) dst->data, (const lvSparseMatrix *) src->data);
}

/**
 * @brief 矩阵-向量乘法：y = A * x（CSR 行遍历）
 */
static int sparse_matrix_matvec(const lvMatrix *A, const lvVector *x, lvVector *y) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(y, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(x->data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(y->data, lv_BACKEND_NOT_INITIALIZED);

    if (A->cols != x->length || A->rows != y->length) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "sparse matvec 维度不匹配: A(%lldx%lld), x(%lld), y(%lld)",
                     (long long) A->rows, (long long) A->cols, (long long) x->length, (long long) y->length);
        return lv_BACKEND_INVALID_ARGS;
    }

    if (lv_sparse_matvec((const lvSparseMatrix *) A->data, x->data, y->data) != 0) {
        lv_ERROR_SET(lv_BACKEND_MATVEC_FAILED, "sparse matvec 执行失败");
        return lv_BACKEND_MATVEC_FAILED;
    }
    return lv_BACKEND_OK;
}

/**
 * @brief 矩阵-标量乘法：A = c * A
 */
static void sparse_matrix_scale(lvMatrix *A, double c) {
    if (!A || !A->data)
        return;
    lv_sparse_scale((lvSparseMatrix *) A->data, c);
}

/**
 * @brief 设置单个元素值（越界时静默忽略，与稠密 default_matrix_set_element 一致）
 */
static void sparse_matrix_set_element(lvMatrix *A, int64_t row, int64_t col, double val) {
    if (!A || !A->data || !lv_index_in_range((int) row, (int) A->rows) || !lv_index_in_range((int) col, (int) A->cols))
        return;
    lv_sparse_set((lvSparseMatrix *) A->data, (int) row, (int) col, val);
}

/**
 * @brief 获取单个元素值（越界返回 0.0，与稠密 default_matrix_get_element 一致）
 */
static double sparse_matrix_get_element(const lvMatrix *A, int64_t row, int64_t col) {
    if (!A || !A->data || !lv_index_in_range((int) row, (int) A->rows) || !lv_index_in_range((int) col, (int) A->cols))
        return 0.0;
    return lv_sparse_get((const lvSparseMatrix *) A->data, (int) row, (int) col);
}

/**
 * @brief 稀疏矩阵"分解"（no-op：Jacobi 迭代无需预分解）
 */
static int sparse_matrix_factor(lvMatrix *A) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->data, lv_BACKEND_NOT_INITIALIZED);
    return lv_BACKEND_OK;
}

/**
 * @brief 求解 A * x = b（委托 lv_sparse_solve，Jacobi 迭代）
 *
 * 映射 lv_sparse_solve 返回码到后端错误码：
 *   iter > 0  → lv_BACKEND_OK；-2 非方阵 → lv_BACKEND_INVALID_ARGS；
 *   -3 零/近零对角线 → lv_BACKEND_LINSOL_FAILED；-1 → lv_BACKEND_MEM_ERROR
 */
static int sparse_matrix_solve(const lvMatrix *A, const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(b->data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(x->data, lv_BACKEND_NOT_INITIALIZED);

    if (A->rows != A->cols) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "sparse solve 要求方阵，当前为 %lldx%lld", (long long) A->rows,
                     (long long) A->cols);
        return lv_BACKEND_INVALID_ARGS;
    }
    if (A->cols != b->length || A->rows != x->length) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "sparse solve 维度不匹配: A(%lldx%lld), b(%lld), x(%lld)",
                     (long long) A->rows, (long long) A->cols, (long long) b->length, (long long) x->length);
        return lv_BACKEND_INVALID_ARGS;
    }

    int iter = lv_sparse_solve((const lvSparseMatrix *) A->data, b->data, x->data);
    if (iter > 0)
        return lv_BACKEND_OK;
    if (iter == -2) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "sparse solve: 非方阵");
        return lv_BACKEND_INVALID_ARGS;
    }
    if (iter == -3) {
        lv_ERROR_SET(lv_BACKEND_LINSOL_FAILED, "sparse solve: 零/近零对角线，Jacobi 无法收敛");
        return lv_BACKEND_LINSOL_FAILED;
    }
    lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "sparse solve: 参数无效或内存分配失败");
    return lv_BACKEND_MEM_ERROR;
}

/**
 * @brief 创建 SERIAL 后端线性求解器
 */
static lvLinearSolver *serial_linsol_create(lvLinearSolverMethod method) {
    lvLinearSolver *LS = lv_calloc(1, sizeof(lvLinearSolver));
    lv_CHECK_ALLOC(LS, NULL);

    LS->method = method;
    LS->backend = lv_BACKEND_SERIAL;
    LS->solver_data = NULL;
    LS->backend_data = NULL;

    static const lvLinearSolverOps *s_linsol_ops[] = {
        [lv_LINSOL_DIRECT_DENSE] = &serial_dense_linsol_ops,
        [lv_LINSOL_DIRECT_BAND] = &serial_dense_linsol_ops,
        [lv_LINSOL_DIRECT_SPARSE] = &serial_dense_linsol_ops,
        [lv_LINSOL_ITERATIVE_GMRES] = &serial_gmres_linsol_ops,
        [lv_LINSOL_ITERATIVE_BICGSTAB] = &serial_bicgstab_linsol_ops,
        [lv_LINSOL_ITERATIVE_CG] = &serial_cg_linsol_ops,
    };
    if (lv_index_in_range((int) method, (int) lv_ARRAY_SIZE(s_linsol_ops)) && s_linsol_ops[(int) method]) {
        LS->ops = s_linsol_ops[(int) method];
    } else {
        lv_free((void **) &LS);
        lv_ERROR_SET(lv_BACKEND_UNSUPPORTED, "不支持的线性求解方法: %s", lv_linsol_method_name(method));
        return NULL;
    }

    return LS;
}

/* ========================================================================
 * 第六部分：工厂函数（通过注册表分发）
 * ======================================================================== */

/**
 * @brief 创建向量（指定后端和长度）
 *
 * 通过后端注册表查找对应后端的操作表，调用其 create 函数。
 * 注册表由 lv_numerical_backend_register() 填充。
 */
lvVector *lv_vector_create(lvBackendType backend, int64_t n) {
    if (n <= 0) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "向量长度必须为正整数，当前 n=%lld", (long long) n);
        return NULL;
    }

    /* 确保注册表已初始化（内置 SERIAL/OPENMP 后端已注册） */
    numerical_backend_init_registry();

    const lvVectorOps *ops = find_vector_ops(backend);
    if (!ops || !ops->create) {
        lv_ERROR_SET(lv_BACKEND_UNSUPPORTED, "后端 %s 尚未注册或不支持向量操作",
                     lv_backend_name(backend));
        return NULL;
    }

    return ops->create(n);
}

/**
 * @brief 创建矩阵（指定后端、维度和格式）
 *
 * 通过后端注册表查找对应后端的操作表，调用其 create 函数。
 */
lvMatrix *lv_matrix_create(lvBackendType backend, int64_t rows, int64_t cols, bool sparse) {
    if (rows <= 0 || cols <= 0) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "矩阵维度必须为正整数，当前 %lldx%lld", (long long) rows,
                     (long long) cols);
        return NULL;
    }

    /* 确保注册表已初始化（内置 SERIAL/OPENMP 后端已注册） */
    numerical_backend_init_registry();

    const lvMatrixOps *ops = find_matrix_ops(backend);
    if (!ops || !ops->create) {
        lv_ERROR_SET(lv_BACKEND_UNSUPPORTED, "后端 %s 尚未注册或不支持矩阵操作",
                     lv_backend_name(backend));
        return NULL;
    }

    return ops->create(rows, cols, sparse);
}

/**
 * @brief 创建线性求解器（指定后端和求解方法）
 *
 * 通过后端注册表查找对应后端的操作表，调用其 create 函数。
 */
lvLinearSolver *lv_linsol_create(lvBackendType backend, lvLinearSolverMethod method) {
    /* 确保注册表已初始化（内置 SERIAL/OPENMP 后端已注册） */
    numerical_backend_init_registry();

    const lvLinearSolverOps *ops = find_linsol_ops(backend);
    if (!ops || !ops->create) {
        lv_ERROR_SET(lv_BACKEND_UNSUPPORTED, "后端 %s 尚未注册或不支持线性求解器",
                     lv_backend_name(backend));
        return NULL;
    }

    return ops->create(method);
}
