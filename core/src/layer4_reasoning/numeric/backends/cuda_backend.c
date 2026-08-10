/**
 * @file cuda_backend.c
 * @brief CUDA GPU 数值后端实现
 *
 * 提供 CUDA 后端的向量/矩阵/线性求解器操作。
 * 编译时通过宏 LV_HAS_CUDA 控制：
 *   - 定义时：链接 CUDA Toolkit (NVCC)，提供完整 GPU 加速实现
 *   - 未定义时：提供优雅降级的存根实现
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-07-30
 *
 * @dependencies
 *   - lv/backends/cuda_backend.h : CUDA 后端公共接口
 *   - lv/lv_utils.h              : 统一内存分配器
 *   - debug.h                    : 日志与断言
 *   - <cuda_runtime.h> / <cublas_v2.h> (可选): CUDA SDK (LV_HAS_CUDA)
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "lv/backends/cuda_backend.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#include "lv/bicgstab_shared.h"
#include "lv/gmres_shared.h"
#include "lv/lv_utils.h"
#include "lv/host_linalg.h"
#include "lv/lv_numeric.h"
#include "debug.h"
#include "lv/lv_internal.h"

#ifdef LV_HAS_CUDA
/* CUDA SDK 头文件 — 需要 CUDA Toolkit */
#include <cuda_runtime.h>
#include <cublas_v2.h>
#endif /* LV_HAS_CUDA */

/* ========================================================================
 * 后端版本信息
 * ======================================================================== */

#define CUDA_BACKEND_VERSION "1.0.0"

/* ========================================================================
 * 模块级常量定义
 * ======================================================================== */

#ifndef lv_NUM_EPSILON
#define lv_NUM_EPSILON lv_EPSILON_MEDIUM
#endif

#define CUDA_MAT_WORK_BLOCK_SIZE 64

/* ========================================================================
 * 通用（非 CUDA）实现 — 当 LV_HAS_CUDA 未定义时的存根
 * ======================================================================== */

#ifndef LV_HAS_CUDA

int lv_cuda_register_backend(void) {
    lv_RETURN_ERROR(lv_ERROR_UNSUPPORTED, "CUDA 后端不可用：未定义 LV_HAS_CUDA（需要 CUDA Toolkit SDK）");
}

int lv_cuda_available(void) {
    return 0;
}

int lv_cuda_device_count(void) {
    return 0;
}

const char *lv_cuda_backend_version(void) {
    return "CUDA (unavailable - stub)";
}

#else /* LV_HAS_CUDA — 完整实现 */

/* ========================================================================
 * CUDA 内核函数声明
 * ======================================================================== */

/**
 * @brief 向量标量乘法内核：v[i] = c * v[i]
 */
static __global__ void vector_scale_kernel(double *v, double c, int64_t n) {
    int64_t i = (int64_t) blockIdx.x * (int64_t) blockDim.x + (int64_t) threadIdx.x;
    if (i < n) {
        v[i] *= c;
    }
}

/**
 * @brief 向量常量设置内核：v[i] = c
 */
static __global__ void vector_const_set_kernel(double *v, double c, int64_t n) {
    int64_t i = (int64_t) blockIdx.x * (int64_t) blockDim.x + (int64_t) threadIdx.x;
    if (i < n) {
        v[i] = c;
    }
}

/**
 * @brief 向量线性组合内核：z[i] = a * x[i] + b * y[i]
 */
static __global__ void vector_linear_sum_kernel(double a, const double *x, double b,
                                                 const double *y, double *z, int64_t n) {
    int64_t i = (int64_t) blockIdx.x * (int64_t) blockDim.x + (int64_t) threadIdx.x;
    if (i < n) {
        z[i] = a * x[i] + b * y[i];
    }
}

/**
 * @brief 矩阵-向量乘法内核：y[i] = sum_j A[j * rows + i] * x[j]（列主序）
 *
 * 每个线程块处理一行（或连续元素行），实现按行并行。
 */
static __global__ void matvec_kernel(const double *data, int64_t rows, int64_t cols,
                                      const double *x, double *y) {
    int64_t i = (int64_t) blockIdx.x * (int64_t) blockDim.x + (int64_t) threadIdx.x;
    if (i < rows) {
        double sum = 0.0;
        for (int64_t j = 0; j < cols; ++j) {
            sum += data[j * rows + i] * x[j];
        }
        y[i] = sum;
    }
}

/**
 * @brief 逐元素绝对值内核：v[i] = fabs(v[i])
 */
static __global__ void vector_abs_kernel(double *v, int64_t n) {
    int64_t i = (int64_t) blockIdx.x * (int64_t) blockDim.x + (int64_t) threadIdx.x;
    if (i < n) {
        v[i] = fabs(v[i]);
    }
}

/**
 * @brief 逐元素除法内核：v[i] = v[i] / d[i]
 */
static __global__ void vector_inv_kernel(double *v, const double *d, int64_t n) {
    int64_t i = (int64_t) blockIdx.x * (int64_t) blockDim.x + (int64_t) threadIdx.x;
    if (i < n) {
        double denom = fabs(d[i]);
        if (denom > lv_EPSILON_DOUBLE) {
            v[i] /= d[i];
        } else {
            v[i] = 1e30;
        }
    }
}

/**
 * @brief 逐元素比较内核：v[i] = max(v[i], c)
 */
static __global__ void vector_compare_kernel(double *v, double c, int64_t n) {
    int64_t i = (int64_t) blockIdx.x * (int64_t) blockDim.x + (int64_t) threadIdx.x;
    if (i < n) {
        if (v[i] < c) {
            v[i] = c;
        }
    }
}

/* ========================================================================
 * CUDA 辅助函数
 * ======================================================================== */

/** @brief 计算 CUDA 线程块配置 */
static inline void cuda_grid_config(int64_t n, int *blocks, int *threads) {
    const int TPB = 256;
    *threads = TPB;
    *blocks = (int) ((n + TPB - 1) / TPB);
    if (*blocks < 1) *blocks = 1;
}

/** @brief 安全的 cudaMemcpy 包装，返回操作状态 */
static int cuda_safe_memcpy(void *dst, const void *src, size_t count,
                             enum cudaMemcpyKind kind, const char *what) {
    cudaError_t err = cudaMemcpy(dst, src, count, kind);
    if (err != cudaSuccess) {
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMemcpy(%s) 失败: %s", what, cudaGetErrorString(err));
        return lv_BACKEND_MEM_ERROR;
    }
    return lv_BACKEND_OK;
}

/** @brief 规约求和核 — 辅助计算点积和范数 */
static __global__ void reduce_sum_kernel(const double *data, double *partial, int64_t n) {
    extern __shared__ double sdata[];
    int tid = threadIdx.x;
    int64_t i = (int64_t) blockIdx.x * (int64_t) blockDim.x + (int64_t) tid;
    sdata[tid] = (i < n) ? data[i] : 0.0;
    __syncthreads();
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }
    if (tid == 0) {
        partial[blockIdx.x] = sdata[0];
    }
}

/** @brief 使用 CUDA 规约计算 sum(data[i]) */
static double cuda_reduce_sum(const double *d_data, int64_t n) {
    int blocks, threads;
    cuda_grid_config(n, &blocks, &threads);

    double *d_partial = NULL;
    cudaError_t err = cudaMalloc((void **) &d_partial, (size_t) blocks * sizeof(double));
    if (err != cudaSuccess) {
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMalloc(partial) 失败: %s", cudaGetErrorString(err));
        return 0.0;
    }

    reduce_sum_kernel<<<blocks, threads, (size_t) threads * sizeof(double)>>>(d_data, d_partial, n);
    cudaDeviceSynchronize();

    double *h_partial = (double *) lv_malloc((size_t) blocks * sizeof(double));
    if (!h_partial) {
        cudaFree(d_partial);
        return 0.0;
    }
    cuda_safe_memcpy(h_partial, d_partial, (size_t) blocks * sizeof(double), cudaMemcpyDeviceToHost, "reduce partial");

    double total = 0.0;
    for (int i = 0; i < blocks; ++i) {
        total += h_partial[i];
    }

    lv_free((void **) &h_partial);
    cudaFree(d_partial);
    return total;
}

/* ========================================================================
 * CUDA 后端私有数据结构
 * ======================================================================== */

/** @brief CUDA 向量私有数据 */
typedef struct CudaVectorData {
    double *d_data;   /**< GPU 端数据指针 */
    int64_t length;   /**< 向量长度 */
} CudaVectorData;

/** @brief CUDA 矩阵私有数据 */
typedef struct CudaMatrixData {
    double *d_data;   /**< GPU 端数据指针（列主序） */
    int64_t rows;     /**< 行数 */
    int64_t cols;     /**< 列数 */
} CudaMatrixData;

/** @brief 稠密 LU 求解器私有数据 */
typedef struct CudaDenseLUData {
    CudaMatrixData *clone; /**< GPU 端矩阵副本 */
    bool factored;          /**< 是否已完成分解 */
    /* CUDA LU 分解工作区 */
    double *d_work;         /**< GPU 端工作区 */
    int *d_pivot;           /**< GPU 端主元索引 */
    int *h_pivot;           /**< CPU 端主元索引 */
    int work_size;          /**< 工作区大小 */
} CudaDenseLUData;

/** @brief 迭代求解器私有数据 */
typedef struct CudaIterSolverData {
    CudaMatrixData *clone; /**< GPU 端矩阵副本 */
    int max_iters;          /**< 最大迭代次数 */
    double tol;             /**< 收敛容差 */
    double *d_r;            /**< GPU 端残差向量 */
    double *d_p;            /**< GPU 端方向向量 */
    double *d_ap;           /**< GPU 端 A*p 向量 */
    double *d_work;         /**< GPU 端通用工作向量 */
    double *h_work;         /**< CPU 端工作向量（规约结果回读） */
    int64_t n;              /**< 问题规模 */
} CudaIterSolverData;

/* ========================================================================
 * 前向声明
 * ======================================================================== */

static lvVector *cuda_vector_create(int64_t n);
static lvVector *cuda_vector_clone(const lvVector *v);
static void cuda_vector_destroy(lvVector *v);
static void cuda_vector_zero(lvVector *v);
static void cuda_vector_const_set(lvVector *v, double c);
static void cuda_vector_copy(lvVector *dst, const lvVector *src);
static void cuda_vector_scale(lvVector *v, double c);
static void cuda_vector_linear_sum(double a, const lvVector *x, double b, const lvVector *y, lvVector *z);
static double cuda_vector_dot(const lvVector *x, const lvVector *y);
static double cuda_vector_norm(const lvVector *v);
static double cuda_vector_max_norm(const lvVector *v);
static double cuda_vector_wrms_norm(const lvVector *v, const lvVector *weights);
static void cuda_vector_abs(lvVector *v);
static void cuda_vector_inv(lvVector *v, const lvVector *d);
static void cuda_vector_compare(lvVector *v, double c);
static int64_t cuda_vector_length(const lvVector *v);
static double *cuda_vector_data_ptr(lvVector *v);

static lvMatrix *cuda_matrix_create(int64_t rows, int64_t cols, bool sparse);
static lvMatrix *cuda_matrix_clone(const lvMatrix *A);
static void cuda_matrix_destroy(lvMatrix *A);
static void cuda_matrix_zero(lvMatrix *A);
static void cuda_matrix_copy(lvMatrix *dst, const lvMatrix *src);
static int cuda_matrix_matvec(const lvMatrix *A, const lvVector *x, lvVector *y);
static void cuda_matrix_scale(lvMatrix *A, double c);
static void cuda_matrix_set_element(lvMatrix *A, int64_t row, int64_t col, double val);
static double cuda_matrix_get_element(const lvMatrix *A, int64_t row, int64_t col);
static int cuda_matrix_factor(lvMatrix *A);
static int cuda_matrix_solve(const lvMatrix *A, const lvVector *b, lvVector *x);

static int cuda_linsol_setup(lvLinearSolver *LS, const lvMatrix *A);
static lvLinearSolver *cuda_linsol_create(lvLinearSolverMethod method);
static int cuda_linsol_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);
static void cuda_linsol_destroy(lvLinearSolver *LS);

static int cuda_gmres_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);
static int cuda_bicgstab_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);

/* CUDA 共享迭代内核算子声明（GMRES 复用 BiCGSTAB 算子，见下方实现） */
static double cuda_bicgstab_dot(void *ctx, const double *a, const double *b, int64_t n);
static double cuda_bicgstab_norm(void *ctx, const double *v, int64_t n);
static void cuda_bicgstab_matvec(void *ctx, const lvMatrix *A, const double *x, double *y, int64_t n);
static int cuda_iter_setup(lvLinearSolver *LS, const lvMatrix *A);

/* ========================================================================
 * 静态操作表（CUDA 后端）
 * ======================================================================== */

/** @brief CUDA 向量操作表 */
static const lvVectorOps cuda_vector_ops = {
    cuda_vector_create,
    cuda_vector_clone, cuda_vector_destroy,  cuda_vector_zero,       cuda_vector_const_set,
    cuda_vector_copy,  cuda_vector_scale,    cuda_vector_linear_sum, cuda_vector_dot,
    cuda_vector_norm,  cuda_vector_max_norm, cuda_vector_wrms_norm,  cuda_vector_abs,
    cuda_vector_inv,   cuda_vector_compare,  cuda_vector_length,     cuda_vector_data_ptr,
};

/** @brief CUDA 稠密矩阵操作表 */
static const lvMatrixOps cuda_dense_matrix_ops = {
    cuda_matrix_create,
    cuda_matrix_clone,  cuda_matrix_destroy, cuda_matrix_zero,        cuda_matrix_copy,
    cuda_matrix_matvec, cuda_matrix_scale,   cuda_matrix_set_element, cuda_matrix_get_element,
    cuda_matrix_factor, cuda_matrix_solve,
};

/** @brief CUDA 稠密 LU 求解器操作表 */
static const lvLinearSolverOps cuda_dense_linsol_ops = {
    cuda_linsol_create,
    cuda_linsol_setup,
    cuda_linsol_solve,
    cuda_linsol_destroy,
};

/** @brief CUDA GMRES 求解器操作表 */
static const lvLinearSolverOps cuda_gmres_linsol_ops = {
    cuda_linsol_create,
    cuda_linsol_setup,
    cuda_gmres_solve,
    cuda_iter_linsol_destroy,
};

/** @brief CUDA BiCGSTAB 求解器操作表 */
static const lvLinearSolverOps cuda_bicgstab_linsol_ops = {
    cuda_linsol_create,
    cuda_linsol_setup,
    cuda_bicgstab_solve,
    cuda_iter_linsol_destroy,
};

/* ========================================================================
 * 第一部分：向量操作实现（CUDA）
 * ======================================================================== */

/**
 * @brief 深拷贝向量
 */
static lvVector *cuda_vector_clone(const lvVector *v) {
    lv_CHECK_NULL(v, NULL);

    CudaVectorData *vdata = (CudaVectorData *) v->backend_data;
    lv_CHECK_NULL(vdata, NULL);

    int64_t n = v->length;

    lvVector *clone = lv_calloc(1, sizeof(lvVector));
    lv_CHECK_ALLOC(clone, NULL);

    clone->length = n;
    clone->backend = lv_BACKEND_CUDA;
    clone->ops = &cuda_vector_ops;
    clone->data = NULL;

    CudaVectorData *cdata = lv_calloc(1, sizeof(CudaVectorData));
    if (!cdata) {
        lv_free((void **) &clone);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "CudaVectorData 分配失败");
        return NULL;
    }
    cdata->length = n;

    cudaError_t err = cudaMalloc((void **) &cdata->d_data, (size_t) n * sizeof(double));
    if (err != cudaSuccess) {
        lv_free((void **) &cdata);
        lv_free((void **) &clone);
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMalloc(clone) 失败: %s", cudaGetErrorString(err));
        return NULL;
    }

    err = cudaMemcpy(cdata->d_data, vdata->d_data, (size_t) n * sizeof(double), cudaMemcpyDeviceToDevice);
    if (err != cudaSuccess) {
        cudaFree(cdata->d_data);
        lv_free((void **) &cdata);
        lv_free((void **) &clone);
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMemcpy(D2D) clone 失败: %s", cudaGetErrorString(err));
        return NULL;
    }

    clone->backend_data = cdata;
    return clone;
}

/**
 * @brief 销毁向量
 */
static void cuda_vector_destroy(lvVector *v) {
    if (!v) {
        return;
    }
    if (v->backend_data) {
        CudaVectorData *cvd = (CudaVectorData *) v->backend_data;
        if (cvd->d_data) {
            cudaFree(cvd->d_data);
        }
        lv_free((void **) &v->backend_data);
    }
    if (v->data) {
        lv_free((void **) &v->data);
    }
    lv_free((void **) &v);
}

/**
 * @brief 置零
 */
static void cuda_vector_zero(lvVector *v) {
    if (!v || !v->backend_data) {
        return;
    }
    CudaVectorData *cvd = (CudaVectorData *) v->backend_data;
    if (!cvd->d_data) {
        return;
    }
    cudaError_t err = cudaMemset(cvd->d_data, 0, (size_t) v->length * sizeof(double));
    if (err != cudaSuccess) {
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMemset zero 失败: %s", cudaGetErrorString(err));
    }
}

/**
 * @brief 设为常量 c
 */
static void cuda_vector_const_set(lvVector *v, double c) {
    if (!v || !v->backend_data) {
        return;
    }
    CudaVectorData *cvd = (CudaVectorData *) v->backend_data;
    if (!cvd->d_data) {
        return;
    }
    int blocks, threads;
    cuda_grid_config(v->length, &blocks, &threads);
    vector_const_set_kernel<<<blocks, threads>>>(cvd->d_data, c, v->length);
    cudaDeviceSynchronize();
}

/**
 * @brief 深拷贝：dst = src
 */
static void cuda_vector_copy(lvVector *dst, const lvVector *src) {
    if (!dst || !src || !dst->backend_data || !src->backend_data) {
        return;
    }
    CudaVectorData *dstd = (CudaVectorData *) dst->backend_data;
    CudaVectorData *srcd = (CudaVectorData *) src->backend_data;
    if (!dstd->d_data || !srcd->d_data) {
        return;
    }
    int64_t n = (dst->length < src->length) ? dst->length : src->length;
    cudaError_t err = cudaMemcpy(dstd->d_data, srcd->d_data, (size_t) n * sizeof(double),
                                  cudaMemcpyDeviceToDevice);
    if (err != cudaSuccess) {
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMemcpy(D2D) copy 失败: %s", cudaGetErrorString(err));
    }
}

/**
 * @brief 标量乘法：v = c * v
 */
static void cuda_vector_scale(lvVector *v, double c) {
    if (!v || !v->backend_data) {
        return;
    }
    CudaVectorData *cvd = (CudaVectorData *) v->backend_data;
    if (!cvd->d_data) {
        return;
    }
    int blocks, threads;
    cuda_grid_config(v->length, &blocks, &threads);
    vector_scale_kernel<<<blocks, threads>>>(cvd->d_data, c, v->length);
    cudaDeviceSynchronize();
}

/**
 * @brief 线性组合：z = a*x + b*y
 */
static void cuda_vector_linear_sum(double a, const lvVector *x, double b,
                                    const lvVector *y, lvVector *z) {
    if (!x || !y || !z || !x->backend_data || !y->backend_data || !z->backend_data) {
        return;
    }
    CudaVectorData *xd = (CudaVectorData *) x->backend_data;
    CudaVectorData *yd = (CudaVectorData *) y->backend_data;
    CudaVectorData *zd = (CudaVectorData *) z->backend_data;
    if (!xd->d_data || !yd->d_data || !zd->d_data) {
        return;
    }
    int64_t n = x->length;
    if (y->length < n) n = y->length;
    if (z->length < n) n = z->length;

    int blocks, threads;
    cuda_grid_config(n, &blocks, &threads);
    vector_linear_sum_kernel<<<blocks, threads>>>(a, xd->d_data, b, yd->d_data, zd->d_data, n);
    cudaDeviceSynchronize();
}

/**
 * @brief 点积（内积）
 */
static double cuda_vector_dot(const lvVector *x, const lvVector *y) {
    if (!x || !y || !x->backend_data || !y->backend_data) {
        return 0.0;
    }
    CudaVectorData *xd = (CudaVectorData *) x->backend_data;
    CudaVectorData *yd = (CudaVectorData *) y->backend_data;
    if (!xd->d_data || !yd->d_data) {
        return 0.0;
    }
    int64_t n = (x->length < y->length) ? x->length : y->length;

    /* 临时在 GPU 上计算逐元素乘积 */
    double *d_prod = NULL;
    cudaError_t err = cudaMalloc((void **) &d_prod, (size_t) n * sizeof(double));
    if (err != cudaSuccess) {
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMalloc(dot) 失败: %s", cudaGetErrorString(err));
        return 0.0;
    }

    int blocks, threads;
    cuda_grid_config(n, &blocks, &threads);
    vector_linear_sum_kernel<<<blocks, threads>>>(1.0, xd->d_data, 0.0, yd->d_data, d_prod, n);
    cudaDeviceSynchronize();

    /* 对元素乘积做点积：实际为 sum(x[i] * y[i]) */
    /* 用 element-wise multiply 加规约实现 */
    {
        int b2, t2;
        cuda_grid_config(n, &b2, &t2);
        double *d_partial = NULL;
        err = cudaMalloc((void **) &d_partial, (size_t) b2 * sizeof(double));
        if (err != cudaSuccess) {
            cudaFree(d_prod);
            lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMalloc(dot partial) 失败: %s", cudaGetErrorString(err));
            return 0.0;
        }
        reduce_sum_kernel<<<b2, t2, (size_t) t2 * sizeof(double)>>>(d_prod, d_partial, n);
        cudaDeviceSynchronize();

        double *h_partial = (double *) lv_malloc((size_t) b2 * sizeof(double));
        if (!h_partial) {
            cudaFree(d_partial);
            cudaFree(d_prod);
            return 0.0;
        }
        cuda_safe_memcpy(h_partial, d_partial, (size_t) b2 * sizeof(double),
                          cudaMemcpyDeviceToHost, "dot partial");

        double sum = 0.0;
        for (int i = 0; i < b2; ++i) {
            sum += h_partial[i];
        }
        lv_free((void **) &h_partial);
        cudaFree(d_partial);
        cudaFree(d_prod);
        return sum;
    }
}

/**
 * @brief L2 范数
 */
static double cuda_vector_norm(const lvVector *v) {
    if (!v || !v->backend_data) {
        return 0.0;
    }
    CudaVectorData *cvd = (CudaVectorData *) v->backend_data;
    if (!cvd->d_data || v->length == 0) {
        return 0.0;
    }
    /* 计算平方和 */
    double *d_sq = NULL;
    cudaError_t err = cudaMalloc((void **) &d_sq, (size_t) v->length * sizeof(double));
    if (err != cudaSuccess) {
        return 0.0;
    }
    int blocks, threads;
    cuda_grid_config(v->length, &blocks, &threads);
    /* 使用 linear_sum 变体计算平方 */
    /* v[i] * v[i] = v[i] * v[i] + 0 * v[i] — 不能直接用 linear_sum 内核 */
    /* 改用自定义内核计算平方 */
    {
        double *d_partial = NULL;
        err = cudaMalloc((void **) &d_partial, (size_t) blocks * sizeof(double));
        if (err != cudaSuccess) {
            cudaFree(d_sq);
            return 0.0;
        }
        reduce_sum_kernel<<<blocks, threads, (size_t) threads * sizeof(double)>>>(cvd->d_data, d_partial, v->length);
        cudaDeviceSynchronize();

        double *h_partial = (double *) lv_malloc((size_t) blocks * sizeof(double));
        if (!h_partial) {
            cudaFree(d_partial);
            cudaFree(d_sq);
            return 0.0;
        }
        cuda_safe_memcpy(h_partial, d_partial, (size_t) blocks * sizeof(double),
                          cudaMemcpyDeviceToHost, "norm partial");

        /* 注意 reduce_sum_kernel 直接对 data 做和，需要先平方 */
        /* 重新实现：先做平方再规约 */
        double sum = 0.0;
        double *h_data = (double *) lv_malloc((size_t) v->length * sizeof(double));
        if (h_data) {
            cuda_safe_memcpy(h_data, cvd->d_data, (size_t) v->length * sizeof(double),
                              cudaMemcpyDeviceToHost, "norm data");
            for (int64_t i = 0; i < v->length; ++i) {
                sum += h_data[i] * h_data[i];
            }
            lv_free((void **) &h_data);
        }
        lv_free((void **) &h_partial);
        cudaFree(d_partial);
        cudaFree(d_sq);
        return sqrt(sum);
    }
}

/**
 * @brief 无穷范数（最大绝对值）
 */
static double cuda_vector_max_norm(const lvVector *v) {
    if (!v || !v->backend_data || v->length == 0) {
        return 0.0;
    }
    CudaVectorData *cvd = (CudaVectorData *) v->backend_data;
    if (!cvd->d_data) {
        return 0.0;
    }
    /* 将数据回读 CPU 后再求最大范数（更简单可靠） */
    double *h_data = (double *) lv_malloc((size_t) v->length * sizeof(double));
    if (!h_data) {
        return 0.0;
    }
    cuda_safe_memcpy(h_data, cvd->d_data, (size_t) v->length * sizeof(double),
                      cudaMemcpyDeviceToHost, "max_norm");
    double max_val = lv_max_abs(h_data, v->length);
    lv_free((void **) &h_data);
    return max_val;
}

/**
 * @brief 加权 RMS 范数
 */
static double cuda_vector_wrms_norm(const lvVector *v, const lvVector *weights) {
    if (!v || !weights || !v->backend_data || !weights->backend_data || v->length == 0) {
        return 0.0;
    }
    CudaVectorData *vd = (CudaVectorData *) v->backend_data;
    CudaVectorData *wd = (CudaVectorData *) weights->backend_data;
    if (!vd->d_data || !wd->d_data) {
        return 0.0;
    }
    int64_t n = v->length;
    if (weights->length < n) n = weights->length;

    /* 回读 CPU 计算 */
    double *h_v = (double *) lv_malloc((size_t) n * sizeof(double));
    double *h_w = (double *) lv_malloc((size_t) n * sizeof(double));
    if (!h_v || !h_w) {
        if (h_v) lv_free((void **) &h_v);
        if (h_w) lv_free((void **) &h_w);
        return 0.0;
    }
    cuda_safe_memcpy(h_v, vd->d_data, (size_t) n * sizeof(double), cudaMemcpyDeviceToHost, "wrms v");
    cuda_safe_memcpy(h_w, wd->d_data, (size_t) n * sizeof(double), cudaMemcpyDeviceToHost, "wrms w");

    double sum_sq = 0.0;
    for (int64_t i = 0; i < n; ++i) {
        double wv = h_v[i] * h_w[i];
        sum_sq += wv * wv;
    }
    lv_free((void **) &h_v);
    lv_free((void **) &h_w);
    return sqrt(sum_sq / (double) n);
}

/**
 * @brief 逐元素绝对值：v_i = |v_i|
 */
static void cuda_vector_abs(lvVector *v) {
    if (!v || !v->backend_data) {
        return;
    }
    CudaVectorData *cvd = (CudaVectorData *) v->backend_data;
    if (!cvd->d_data) {
        return;
    }
    int blocks, threads;
    cuda_grid_config(v->length, &blocks, &threads);
    vector_abs_kernel<<<blocks, threads>>>(cvd->d_data, v->length);
    cudaDeviceSynchronize();
}

/**
 * @brief 逐元素除法：v_i = v_i / d_i
 */
static void cuda_vector_inv(lvVector *v, const lvVector *d) {
    if (!v || !d || !v->backend_data || !d->backend_data) {
        return;
    }
    CudaVectorData *vd = (CudaVectorData *) v->backend_data;
    CudaVectorData *dd = (CudaVectorData *) d->backend_data;
    if (!vd->d_data || !dd->d_data) {
        return;
    }
    int64_t n = (v->length < d->length) ? v->length : d->length;
    int blocks, threads;
    cuda_grid_config(n, &blocks, &threads);
    vector_inv_kernel<<<blocks, threads>>>(vd->d_data, dd->d_data, n);
    cudaDeviceSynchronize();
}

/**
 * @brief 逐元素最大值：v_i = max(v_i, c)
 */
static void cuda_vector_compare(lvVector *v, double c) {
    if (!v || !v->backend_data) {
        return;
    }
    CudaVectorData *cvd = (CudaVectorData *) v->backend_data;
    if (!cvd->d_data) {
        return;
    }
    int blocks, threads;
    cuda_grid_config(v->length, &blocks, &threads);
    vector_compare_kernel<<<blocks, threads>>>(cvd->d_data, c, v->length);
    cudaDeviceSynchronize();
}

/**
 * @brief 获取向量长度
 */
static int64_t cuda_vector_length(const lvVector *v) {
    if (!v) {
        return 0;
    }
    return v->length;
}

/**
 * @brief 获取底层原始数据指针
 */
static double *cuda_vector_data_ptr(lvVector *v) {
    if (!v) {
        return NULL;
    }
    return v->data;
}

/* ========================================================================
 * 第二部分：矩阵操作实现（CUDA 稠密）
 * ======================================================================== */

/**
 * @brief 深拷贝矩阵
 */
static lvMatrix *cuda_matrix_clone(const lvMatrix *A) {
    lv_CHECK_NULL(A, NULL);

    CudaMatrixData *adata = (CudaMatrixData *) A->backend_data;
    lv_CHECK_NULL(adata, NULL);

    int64_t rows = A->rows;
    int64_t cols = A->cols;
    size_t data_size = (size_t) (rows * cols) * sizeof(double);

    lvMatrix *clone = lv_calloc(1, sizeof(lvMatrix));
    lv_CHECK_ALLOC(clone, NULL);

    clone->rows = rows;
    clone->cols = cols;
    clone->sparse = A->sparse;
    clone->format = A->format;
    clone->backend = lv_BACKEND_CUDA;
    clone->ops = A->ops;
    clone->data = NULL;

    CudaMatrixData *cdata = lv_calloc(1, sizeof(CudaMatrixData));
    if (!cdata) {
        lv_free((void **) &clone);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "CudaMatrixData 分配失败");
    }
    cdata->rows = rows;
    cdata->cols = cols;

    cudaError_t err = cudaMalloc((void **) &cdata->d_data, data_size);
    if (err != cudaSuccess) {
        lv_free((void **) &cdata);
        lv_free((void **) &clone);
        lv_RETURN_ERROR_NULL(lv_BACKEND_MEM_ERROR, "cudaMalloc(matrix clone) 失败: %s", cudaGetErrorString(err));
    }

    if (adata->d_data) {
        err = cudaMemcpy(cdata->d_data, adata->d_data, data_size, cudaMemcpyDeviceToDevice);
        if (err != cudaSuccess) {
            cudaFree(cdata->d_data);
            lv_free((void **) &cdata);
            lv_free((void **) &clone);
            lv_RETURN_ERROR_NULL(lv_BACKEND_MEM_ERROR, "cudaMemcpy(D2D) matrix clone 失败: %s", cudaGetErrorString(err));
        }
    } else {
        err = cudaMemset(cdata->d_data, 0, data_size);
        if (err != cudaSuccess) {
            cudaFree(cdata->d_data);
            lv_free((void **) &cdata);
            lv_free((void **) &clone);
            lv_RETURN_ERROR_NULL(lv_BACKEND_MEM_ERROR, "cudaMemset matrix clone 失败: %s", cudaGetErrorString(err));
        }
    }

    clone->backend_data = cdata;
    return clone;
}

/**
 * @brief 销毁矩阵
 */
static void cuda_matrix_destroy(lvMatrix *A) {
    if (!A) {
        return;
    }
    if (A->backend_data) {
        CudaMatrixData *cmd = (CudaMatrixData *) A->backend_data;
        if (cmd->d_data) {
            cudaFree(cmd->d_data);
        }
        lv_free((void **) &A->backend_data);
    }
    if (A->data) {
        lv_free((void **) &A->data);
    }
    lv_free((void **) &A);
}

/**
 * @brief 置零
 */
static void cuda_matrix_zero(lvMatrix *A) {
    if (!A || !A->backend_data) {
        return;
    }
    CudaMatrixData *cmd = (CudaMatrixData *) A->backend_data;
    if (!cmd->d_data) {
        return;
    }
    cudaError_t err = cudaMemset(cmd->d_data, 0, (size_t) (A->rows * A->cols) * sizeof(double));
    if (err != cudaSuccess) {
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMemset matrix zero 失败: %s", cudaGetErrorString(err));
    }
}

/**
 * @brief 深拷贝：dst = src
 */
static void cuda_matrix_copy(lvMatrix *dst, const lvMatrix *src) {
    if (!dst || !src || !dst->backend_data || !src->backend_data) {
        return;
    }
    CudaMatrixData *dstd = (CudaMatrixData *) dst->backend_data;
    CudaMatrixData *srcd = (CudaMatrixData *) src->backend_data;
    if (!dstd->d_data || !srcd->d_data) {
        return;
    }
    int64_t elems = dst->rows * dst->cols;
    int64_t src_elems = src->rows * src->cols;
    int64_t n = (elems < src_elems) ? elems : src_elems;
    cudaError_t err = cudaMemcpy(dstd->d_data, srcd->d_data, (size_t) n * sizeof(double),
                                  cudaMemcpyDeviceToDevice);
    if (err != cudaSuccess) {
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMemcpy(D2D) matrix copy 失败: %s", cudaGetErrorString(err));
    }
}

/**
 * @brief 矩阵-向量乘法：y = A * x（列主序稠密）
 */
static int cuda_matrix_matvec(const lvMatrix *A, const lvVector *x, lvVector *y) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(y, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->backend_data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(x->backend_data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(y->backend_data, lv_BACKEND_NOT_INITIALIZED);

    if (A->cols != x->length || A->rows != y->length) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "matvec 维度不匹配: A(%lldx%lld), x(%lld), y(%lld)",
                     (long long) A->rows, (long long) A->cols,
                     (long long) x->length, (long long) y->length);
        return lv_BACKEND_INVALID_ARGS;
    }

    CudaMatrixData *amd = (CudaMatrixData *) A->backend_data;
    CudaVectorData *xvd = (CudaVectorData *) x->backend_data;
    CudaVectorData *yvd = (CudaVectorData *) y->backend_data;

    int blocks, threads;
    cuda_grid_config(A->rows, &blocks, &threads);
    matvec_kernel<<<blocks, threads>>>(amd->d_data, A->rows, A->cols, xvd->d_data, yvd->d_data);
    cudaDeviceSynchronize();

    return lv_BACKEND_OK;
}

/**
 * @brief 矩阵-标量乘法：A = c * A
 */
static void cuda_matrix_scale(lvMatrix *A, double c) {
    if (!A || !A->backend_data) {
        return;
    }
    CudaMatrixData *cmd = (CudaMatrixData *) A->backend_data;
    if (!cmd->d_data) {
        return;
    }
    int64_t elems = A->rows * A->cols;
    int blocks, threads;
    cuda_grid_config(elems, &blocks, &threads);
    vector_scale_kernel<<<blocks, threads>>>(cmd->d_data, c, elems);
    cudaDeviceSynchronize();
}

/**
 * @brief 设置单个元素值（列主序）
 *
 * CUDA 后端不能直接修改单个 GPU 元素，回传主机修改后再传回设备。
 */
static void cuda_matrix_set_element(lvMatrix *A, int64_t row, int64_t col, double val) {
    if (!A || !A->backend_data || !lv_index_in_range((int) row, (int) A->rows) || !lv_index_in_range((int) col, (int) A->cols)) {
        return;
    }
    CudaMatrixData *cmd = (CudaMatrixData *) A->backend_data;
    if (!cmd->d_data) {
        return;
    }
    int64_t idx = col * A->rows + row;
    cudaError_t err = cudaMemcpy(cmd->d_data + idx, &val, sizeof(double), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMemcpy(H2D) set_element 失败: %s", cudaGetErrorString(err));
    }
}

/**
 * @brief 获取单个元素值（列主序）
 */
static double cuda_matrix_get_element(const lvMatrix *A, int64_t row, int64_t col) {
    if (!A || !A->backend_data || !lv_index_in_range((int) row, (int) A->rows) || !lv_index_in_range((int) col, (int) A->cols)) {
        return 0.0;
    }
    CudaMatrixData *cmd = (CudaMatrixData *) A->backend_data;
    if (!cmd->d_data) {
        return 0.0;
    }
    int64_t idx = col * A->rows + row;
    double val = 0.0;
    cudaError_t err = cudaMemcpy(&val, cmd->d_data + idx, sizeof(double), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMemcpy(D2H) get_element 失败: %s", cudaGetErrorString(err));
        return 0.0;
    }
    return val;
}

/**
 * @brief 就地 LU 分解（列主序稠密，部分选主元）
 *
 * 将 GPU 上的矩阵数据回读 CPU，在 CPU 上做 LU 分解，再传回 GPU。
 * 对于大型矩阵，建议使用 cuBLAS/cuSOLVER 的原生 GPU 分解。
 */
static int cuda_matrix_factor(lvMatrix *A) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->backend_data, lv_BACKEND_NOT_INITIALIZED);

    if (A->rows != A->cols) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "LU分解要求方阵，当前为 %lldx%lld",
                     (long long) A->rows, (long long) A->cols);
        return lv_BACKEND_INVALID_ARGS;
    }

    CudaMatrixData *cmd = (CudaMatrixData *) A->backend_data;
    int64_t n = A->rows;

    /* 将 GPU 数据回读 CPU */
    double *h_data = lv_malloc((size_t) (n * n) * sizeof(double));
    lv_CHECK_ALLOC(h_data, lv_BACKEND_MEM_ERROR);

    cudaError_t err = cudaMemcpy(h_data, cmd->d_data, (size_t) (n * n) * sizeof(double),
                                  cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        lv_free((void **) &h_data);
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMemcpy(D2H) factor 失败: %s", cudaGetErrorString(err));
        return lv_BACKEND_MEM_ERROR;
    }

    /* CPU 端 LU 分解 */
    int ret = host_lu_factor(h_data, n, lv_EPSILON_DOUBLE);
    if (ret != lv_BACKEND_OK) {
        lv_free((void **) &h_data);
        return ret;
    }

    /* 将分解后的数据传回 GPU */
    err = cudaMemcpy(cmd->d_data, h_data, (size_t) (n * n) * sizeof(double), cudaMemcpyHostToDevice);
    lv_free((void **) &h_data);
    if (err != cudaSuccess) {
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMemcpy(H2D) factor 回写失败: %s", cudaGetErrorString(err));
        return lv_BACKEND_MEM_ERROR;
    }

    return lv_BACKEND_OK;
}

/**
 * @brief 使用已分解矩阵求解 A * x = b
 *
 * 回读 CPU 端求解，结果传回 GPU。
 */
static int cuda_matrix_solve(const lvMatrix *A, const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->backend_data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(b->backend_data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(x->backend_data, lv_BACKEND_NOT_INITIALIZED);

    if (A->rows != A->cols) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "solve要求方阵，当前为 %lldx%lld",
                     (long long) A->rows, (long long) A->cols);
        return lv_BACKEND_INVALID_ARGS;
    }

    CudaMatrixData *amd = (CudaMatrixData *) A->backend_data;
    CudaVectorData *bvd = (CudaVectorData *) b->backend_data;
    CudaVectorData *xvd = (CudaVectorData *) x->backend_data;

    int64_t n = A->rows;

    /* 回读 CPU */
    double *h_data = lv_malloc((size_t) (n * n) * sizeof(double));
    double *h_b = lv_malloc((size_t) n * sizeof(double));
    double *h_x = lv_malloc((size_t) n * sizeof(double));
    lv_CHECK_ALLOC(h_data, lv_BACKEND_MEM_ERROR);
    lv_CHECK_ALLOC(h_b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_ALLOC(h_x, lv_BACKEND_MEM_ERROR);

    cuda_safe_memcpy(h_data, amd->d_data, (size_t) (n * n) * sizeof(double),
                      cudaMemcpyDeviceToHost, "solve data");
    cuda_safe_memcpy(h_b, bvd->d_data, (size_t) n * sizeof(double),
                      cudaMemcpyDeviceToHost, "solve b");

    /* CPU 端前代 + 回代 */
    int ret = host_lu_solve(h_data, n, h_b, h_x);
    if (ret != lv_BACKEND_OK) {
        lv_free((void **) &h_data);
        lv_free((void **) &h_b);
        lv_free((void **) &h_x);
        return ret;
    }

    /* 结果传回 GPU */
    cudaError_t err = cudaMemcpy(xvd->d_data, h_x, (size_t) n * sizeof(double),
                                  cudaMemcpyHostToDevice);
    lv_free((void **) &h_data);
    lv_free((void **) &h_b);
    lv_free((void **) &h_x);

    if (err != cudaSuccess) {
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMemcpy(H2D) solve 结果回写失败: %s", cudaGetErrorString(err));
        return lv_BACKEND_MEM_ERROR;
    }

    return lv_BACKEND_OK;
}

/* ========================================================================
 * 第三部分：线性求解器实现（CUDA 稠密 LU）
 * ======================================================================== */

/**
 * @brief 稠密 LU 求解器私有数据
 *
 * 继承自 CudaDenseLUData，在 GPU 上维护矩阵副本。
 */
static int cuda_linsol_setup(lvLinearSolver *LS, const lvMatrix *A) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);

    CudaDenseLUData *lu = lv_calloc(1, sizeof(CudaDenseLUData));
    lv_CHECK_ALLOC(lu, lv_BACKEND_MEM_ERROR);

    /* 在 GPU 上创建矩阵副本 */
    lvMatrix *clone = cuda_matrix_clone(A);
    if (!clone) {
        lv_free((void **) &lu);
        return lv_BACKEND_MEM_ERROR;
    }
    lu->clone = (CudaMatrixData *) clone->backend_data;
    /* 释放 lvMatrix 外壳，仅保留 backend_data */
    lv_free((void **) &clone);
    lu->factored = false;
    lu->d_work = NULL;
    lu->d_pivot = NULL;
    lu->h_pivot = NULL;
    lu->work_size = 0;

    /* 释放旧数据 */
    if (LS->solver_data) {
        CudaDenseLUData *old = (CudaDenseLUData *) LS->solver_data;
        if (old->clone) {
            if (old->clone->d_data) cudaFree(old->clone->d_data);
            lv_free((void **) &old->clone);
        }
        if (old->d_work) cudaFree(old->d_work);
        if (old->d_pivot) cudaFree(old->d_pivot);
        if (old->h_pivot) lv_free((void **) &old->h_pivot);
        lv_free((void **) &LS->solver_data);
    }
    LS->solver_data = lu;

    return lv_BACKEND_OK;
}

/**
 * @brief 使用稠密 LU 求解 A * x = b
 */
static int cuda_linsol_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);

    CudaDenseLUData *lu = (CudaDenseLUData *) LS->solver_data;
    if (!lu || !lu->clone) {
        lv_ERROR_SET(lv_BACKEND_NOT_INITIALIZED, "线性求解器未初始化，请先调用 setup");
        return lv_BACKEND_NOT_INITIALIZED;
    }

    if (lu->clone->rows != A->rows || lu->clone->cols != A->cols) {
        int ret = cuda_linsol_setup(LS, A);
        if (ret != lv_BACKEND_OK) return ret;
        lu = (CudaDenseLUData *) LS->solver_data;
    }

    if (!lu->factored) {
        /* 构造临时 lvMatrix 用于 factor */
        lvMatrix tmp_mat;
        tmp_mat.rows = lu->clone->rows;
        tmp_mat.cols = lu->clone->cols;
        tmp_mat.backend_data = lu->clone;
        tmp_mat.sparse = false;
        tmp_mat.format = lv_MATRIX_DENSE;
        tmp_mat.backend = lv_BACKEND_CUDA;
        tmp_mat.data = NULL;
        tmp_mat.ops = &cuda_dense_matrix_ops;

        int ret = cuda_matrix_factor(&tmp_mat);
        if (ret != lv_BACKEND_OK) return ret;
        lu->factored = true;
    }

    /* 构造临时对象用于 solve */
    lvMatrix tmp_mat;
    tmp_mat.rows = lu->clone->rows;
    tmp_mat.cols = lu->clone->cols;
    tmp_mat.backend_data = lu->clone;
    tmp_mat.sparse = false;
    tmp_mat.format = lv_MATRIX_DENSE;
    tmp_mat.backend = lv_BACKEND_CUDA;
    tmp_mat.data = NULL;
    tmp_mat.ops = &cuda_dense_matrix_ops;

    return cuda_matrix_solve(&tmp_mat, b, x);
}

/**
 * @brief 销毁线性求解器
 */
static void cuda_linsol_destroy(lvLinearSolver *LS) {
    if (!LS) {
        return;
    }
    if (LS->solver_data) {
        CudaDenseLUData *lu = (CudaDenseLUData *) LS->solver_data;
        if (lu->clone) {
            if (lu->clone->d_data) cudaFree(lu->clone->d_data);
            lv_free((void **) &lu->clone);
        }
        if (lu->d_work) cudaFree(lu->d_work);
        if (lu->d_pivot) cudaFree(lu->d_pivot);
        if (lu->h_pivot) lv_free((void **) &lu->h_pivot);
        lv_free((void **) &LS->solver_data);
    }
    lv_free((void **) &LS);
}

static void cuda_iter_linsol_destroy(lvLinearSolver *LS) {
    if (!LS) {
        return;
    }
    if (LS->solver_data) {
        CudaIterSolverData *is = (CudaIterSolverData *) LS->solver_data;
        if (is->clone) {
            if (is->clone->d_data) cudaFree(is->clone->d_data);
            lv_free((void **) &is->clone);
        }
        if (is->d_r) cudaFree(is->d_r);
        if (is->d_p) cudaFree(is->d_p);
        if (is->d_ap) cudaFree(is->d_ap);
        if (is->d_work) cudaFree(is->d_work);
        if (is->h_work) lv_free((void **) &is->h_work);
        lv_free((void **) &LS->solver_data);
    }
    lv_free((void **) &LS);
}

/* ========================================================================
 * 第四部分：迭代法求解器实现（CUDA GMRES / BiCGSTAB）
 * ======================================================================== */

/**
 * @brief 设置迭代求解器
 */
static int cuda_iter_setup(lvLinearSolver *LS, const lvMatrix *A) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);

    CudaIterSolverData *is = lv_calloc(1, sizeof(CudaIterSolverData));
    lv_CHECK_ALLOC(is, lv_BACKEND_MEM_ERROR);
    lv_linsol_default_params(&is->max_iters, &is->tol);
    is->n = A->rows;

    /* 在 GPU 上创建矩阵副本 */
    lvMatrix *clone = cuda_matrix_clone(A);
    if (!clone) {
        lv_free((void **) &is);
        return lv_BACKEND_MEM_ERROR;
    }
    is->clone = (CudaMatrixData *) clone->backend_data;
    lv_free((void **) &clone);

    int64_t n = A->rows;
    size_t nbytes = (size_t) n * sizeof(double);

    cudaError_t err;
    err = cudaMalloc((void **) &is->d_r, nbytes);
    if (err != cudaSuccess) goto alloc_fail;
    err = cudaMalloc((void **) &is->d_p, nbytes);
    if (err != cudaSuccess) goto alloc_fail;
    err = cudaMalloc((void **) &is->d_ap, nbytes);
    if (err != cudaSuccess) goto alloc_fail;
    err = cudaMalloc((void **) &is->d_work, nbytes);
    if (err != cudaSuccess) goto alloc_fail;

    is->h_work = lv_malloc(nbytes);
    if (!is->h_work) goto alloc_fail;

    /* 释放旧数据 */
    if (LS->solver_data) {
        CudaIterSolverData *old = (CudaIterSolverData *) LS->solver_data;
        if (old->clone) {
            if (old->clone->d_data) cudaFree(old->clone->d_data);
            lv_free((void **) &old->clone);
        }
        if (old->d_r) cudaFree(old->d_r);
        if (old->d_p) cudaFree(old->d_p);
        if (old->d_ap) cudaFree(old->d_ap);
        if (old->d_work) cudaFree(old->d_work);
        if (old->h_work) lv_free((void **) &old->h_work);
        lv_free((void **) &LS->solver_data);
    }
    LS->solver_data = is;
    return lv_BACKEND_OK;

alloc_fail:
    if (is->d_r) cudaFree(is->d_r);
    if (is->d_p) cudaFree(is->d_p);
    if (is->d_ap) cudaFree(is->d_ap);
    if (is->d_work) cudaFree(is->d_work);
    if (is->h_work) lv_free((void **) &is->h_work);
    if (is->clone) {
        if (is->clone->d_data) cudaFree(is->clone->d_data);
        lv_free((void **) &is->clone);
    }
    lv_free((void **) &is);
    lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "CUDA 迭代求解器工作区分配失败");
    return lv_BACKEND_MEM_ERROR;
}

/** @brief CUDA GMRES 求解器 —— 在 GPU 上执行矩阵-向量乘法，CPU 端执行 Arnoldi 过程 */
static int cuda_gmres_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);

    CudaIterSolverData *is = (CudaIterSolverData *) LS->solver_data;
    if (!is) {
        int ret = cuda_iter_setup(LS, A);
        if (ret != lv_BACKEND_OK) return ret;
        is = (CudaIterSolverData *) LS->solver_data;
    }

    int64_t n = A->rows;
    CudaVectorData *bvd = (CudaVectorData *) b->backend_data;
    CudaVectorData *xvd = (CudaVectorData *) x->backend_data;

    /* 主机端 b/x 缓冲 */
    double *h_b = lv_calloc((size_t) n, sizeof(double));
    double *h_x = lv_calloc((size_t) n, sizeof(double));
    if (!h_b || !h_x) {
        if (h_b) lv_free((void **) &h_b);
        if (h_x) lv_free((void **) &h_x);
        return lv_BACKEND_MEM_ERROR;
    }

    /* b 回读 */
    cuda_safe_memcpy(h_b, bvd->d_data, (size_t) n * sizeof(double),
                      cudaMemcpyDeviceToHost, "gmres b");

    lvGmresOps ops;
    ops.ctx = is;
    ops.vector_dot = cuda_bicgstab_dot;
    ops.vector_norm = cuda_bicgstab_norm;
    ops.matvec = cuda_bicgstab_matvec;

    int ret = lv_gmres_solve(&ops, A, h_b, h_x, n,
                             is->max_iters, is->tol, lv_EPSILON_DOUBLE, 30);

    if (ret == lv_BACKEND_OK) {
        /* 结果传回 GPU */
        cuda_safe_memcpy(xvd->d_data, h_x, (size_t) n * sizeof(double),
                          cudaMemcpyHostToDevice, "gmres final x");
    }

    lv_free((void **) &h_b);
    lv_free((void **) &h_x);
    return ret;
}

/* ========================================================================
 * BiCGSTAB 共享内核的 CUDA 算子（GPU matvec + 主机端点积/范数）
 * ======================================================================== */

/** @brief CUDA 主机端点积：<a, b>（与原实现内联求和顺序一致） */
static double cuda_bicgstab_dot(void *ctx, const double *a, const double *b, int64_t n) {
    (void) ctx;
    double sum = 0.0;
    for (int64_t i = 0; i < n; ++i) sum += a[i] * b[i];
    return sum;
}

/** @brief CUDA 主机端 L2 范数：||v||_2 */
static double cuda_bicgstab_norm(void *ctx, const double *v, int64_t n) {
    (void) ctx;
    double sum = 0.0;
    for (int64_t i = 0; i < n; ++i) sum += v[i] * v[i];
    return sqrt(sum);
}

/** @brief CUDA 矩阵向量乘：y = A*x（GPU 内核，经 is->d_work / is->d_ap 中转） */
static void cuda_bicgstab_matvec(void *ctx, const lvMatrix *A, const double *x, double *y, int64_t n) {
    CudaIterSolverData *is = (CudaIterSolverData *) ctx;
    CudaMatrixData *amd = (CudaMatrixData *) A->backend_data;
    cuda_safe_memcpy(is->d_work, x, (size_t) n * sizeof(double),
                      cudaMemcpyHostToDevice, "bicgstab p");
    cudaMemset(is->d_ap, 0, (size_t) n * sizeof(double));
    {
        int blocks, threads;
        cuda_grid_config(A->rows, &blocks, &threads);
        matvec_kernel<<<blocks, threads>>>(amd->d_data, A->rows, A->cols,
                                            is->d_work, is->d_ap);
        cudaDeviceSynchronize();
    }
    cuda_safe_memcpy(y, is->d_ap, (size_t) n * sizeof(double),
                      cudaMemcpyDeviceToHost, "bicgstab v");
}

/**
 * @brief CUDA BiCGSTAB 求解器
 *
 * GPU 加速矩阵-向量乘法，CPU 端执行向量操作与收敛判断。
 * 算法主体委托给共享内核 lv_bicgstab_solve()（bicgstab_shared.c），
 * 此处仅提供 CUDA 算子表并在求解前后回读/写回 GPU 数据。
 */
static int cuda_bicgstab_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);

    CudaIterSolverData *is = (CudaIterSolverData *) LS->solver_data;
    if (!is) {
        int ret = cuda_iter_setup(LS, A);
        if (ret != lv_BACKEND_OK) return ret;
        is = (CudaIterSolverData *) LS->solver_data;
    }

    int64_t n = A->rows;
    CudaVectorData *bvd = (CudaVectorData *) b->backend_data;
    CudaVectorData *xvd = (CudaVectorData *) x->backend_data;

    /* 主机端 b/x 缓冲 */
    double *h_b = lv_calloc((size_t) n, sizeof(double));
    double *h_x = lv_calloc((size_t) n, sizeof(double));
    if (!h_b || !h_x) {
        if (h_b) lv_free((void **) &h_b);
        if (h_x) lv_free((void **) &h_x);
        return lv_BACKEND_MEM_ERROR;
    }

    /* b 回读 */
    cuda_safe_memcpy(h_b, bvd->d_data, (size_t) n * sizeof(double),
                      cudaMemcpyDeviceToHost, "bicgstab b");

    lvBicgstabOps ops;
    ops.ctx = is;
    ops.vector_dot = cuda_bicgstab_dot;
    ops.vector_norm = cuda_bicgstab_norm;
    ops.matvec = cuda_bicgstab_matvec;

    int ret = lv_bicgstab_solve(&ops, A, h_b, h_x, n,
                                is->max_iters, is->tol, lv_EPSILON_DOUBLE);

    if (ret == lv_BACKEND_OK) {
        /* 结果传回 GPU */
        cuda_safe_memcpy(xvd->d_data, h_x, (size_t) n * sizeof(double),
                          cudaMemcpyHostToDevice, "bicgstab final x");
    }

    lv_free((void **) &h_b);
    lv_free((void **) &h_x);
    return ret;
}

/* ========================================================================
 * 第五部分：CUDA 后端 create 函数实现
 * ======================================================================== */

/**
 * @brief 创建 CUDA 后端向量
 */
static lvVector *cuda_vector_create(int64_t n) {
    if (n <= 0) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "向量长度必须为正整数，当前 n=%lld", (long long) n);
        return NULL;
    }

    lvVector *v = lv_calloc(1, sizeof(lvVector));
    lv_CHECK_ALLOC(v, NULL);

    v->length = n;
    v->backend = lv_BACKEND_CUDA;
    v->ops = &cuda_vector_ops;
    v->data = NULL;

    CudaVectorData *vd = lv_calloc(1, sizeof(CudaVectorData));
    if (!vd) {
        lv_free((void **) &v);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "CudaVectorData 分配失败");
        return NULL;
    }
    vd->length = n;

    cudaError_t err = cudaMalloc((void **) &vd->d_data, (size_t) n * sizeof(double));
    if (err != cudaSuccess) {
        lv_free((void **) &vd);
        lv_free((void **) &v);
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMalloc(vector) 失败: %s", cudaGetErrorString(err));
        return NULL;
    }
    cudaMemset(vd->d_data, 0, (size_t) n * sizeof(double));

    v->backend_data = vd;
    return v;
}

/**
 * @brief 创建 CUDA 后端稠密矩阵
 */
static lvMatrix *cuda_matrix_create(int64_t rows, int64_t cols, bool sparse) {
    if (rows <= 0 || cols <= 0) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "矩阵维度必须为正整数，当前 %lldx%lld",
                     (long long) rows, (long long) cols);
        return NULL;
    }

    lvMatrix *A = lv_calloc(1, sizeof(lvMatrix));
    lv_CHECK_ALLOC(A, NULL);

    A->rows = rows;
    A->cols = cols;
    A->sparse = sparse;
    A->format = sparse ? lv_MATRIX_SPARSE_CSR : lv_MATRIX_DENSE;
    A->backend = lv_BACKEND_CUDA;
    A->ops = &cuda_dense_matrix_ops;
    A->data = NULL;

    CudaMatrixData *md = lv_calloc(1, sizeof(CudaMatrixData));
    if (!md) {
        lv_free((void **) &A);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "CudaMatrixData 分配失败");
        return NULL;
    }
    md->rows = rows;
    md->cols = cols;

    size_t data_size = (size_t) (rows * cols) * sizeof(double);
    cudaError_t err = cudaMalloc((void **) &md->d_data, data_size);
    if (err != cudaSuccess) {
        lv_free((void **) &md);
        lv_free((void **) &A);
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "cudaMalloc(matrix) 失败: %s", cudaGetErrorString(err));
        return NULL;
    }
    cudaMemset(md->d_data, 0, data_size);

    A->backend_data = md;
    return A;
}

/* 线性求解方法 → ops 操作表映射（方法枚举 0..4 为下标；CG/CUSTOM 等不支持方法走错误分支） */
static const lvLinearSolverOps *kCudaLinsolOpsByMethod[] = {
    [lv_LINSOL_DIRECT_DENSE]   = &cuda_dense_linsol_ops,
    [lv_LINSOL_DIRECT_BAND]    = &cuda_dense_linsol_ops,
    [lv_LINSOL_DIRECT_SPARSE]  = &cuda_dense_linsol_ops,
    [lv_LINSOL_ITERATIVE_GMRES]    = &cuda_gmres_linsol_ops,
    [lv_LINSOL_ITERATIVE_BICGSTAB] = &cuda_bicgstab_linsol_ops,
};

/**
 * @brief 创建 CUDA 后端线性求解器
 */
static lvLinearSolver *cuda_linsol_create(lvLinearSolverMethod method) {
    lvLinearSolver *LS = lv_calloc(1, sizeof(lvLinearSolver));
    lv_CHECK_ALLOC(LS, NULL);

    LS->method = method;
    LS->backend = lv_BACKEND_CUDA;
    LS->solver_data = NULL;
    LS->backend_data = NULL;

    if ((unsigned) method < sizeof(kCudaLinsolOpsByMethod) / sizeof(kCudaLinsolOpsByMethod[0]) &&
        kCudaLinsolOpsByMethod[method]) {
        LS->ops = kCudaLinsolOpsByMethod[method];
    } else {
        lv_free((void **) &LS);
        lv_ERROR_SET(lv_BACKEND_UNSUPPORTED, "CUDA 后端不支持的求解方法: %s",
                     lv_linsol_method_name(method));
        return NULL;
    }

    return LS;
}

/* ========================================================================
 * 第六部分：公共 API 实现
 * ======================================================================== */

/**
 * @brief 注册 CUDA 后端操作表
 *
 * 将 CUDA 向量/矩阵/求解器操作表注册到全局后端注册表中。
 * 返回 0 表示成功。
 */
int lv_cuda_register_backend(void) {
    lv_numerical_backend_register(lv_BACKEND_CUDA,
                                   &cuda_vector_ops,
                                   &cuda_dense_matrix_ops,
                                   &cuda_dense_linsol_ops);
    return 0;
}

/**
 * @brief 检查 CUDA 是否可用
 *
 * 通过 cudaGetDeviceCount 检测 GPU 设备是否可达。
 */
int lv_cuda_available(void) {
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err != cudaSuccess) {
        return 0;
    }
    return (device_count > 0) ? 1 : 0;
}

/**
 * @brief 获取 CUDA 设备数
 */
int lv_cuda_device_count(void) {
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err != cudaSuccess) {
        return 0;
    }
    return device_count;
}

/**
 * @brief 获取 CUDA 工具箱版本字符串
 */
const char *lv_cuda_backend_version(void) {
    static lv_THREAD_LOCAL char version_buf[128];
    int driver_version = 0, runtime_version = 0;
    cudaError_t err_drv = cudaDriverGetVersion(&driver_version);
    cudaError_t err_rt  = cudaRuntimeGetVersion(&runtime_version);
    if (err_drv != cudaSuccess || err_rt != cudaSuccess) {
        lv_SAFE_SNPRINTF(int ignore, version_buf, sizeof(version_buf),
                          "CUDA %s (version query failed)", CUDA_BACKEND_VERSION);
        (void) ignore;
        return version_buf;
    }
    lv_SAFE_SNPRINTF(int ignore, version_buf, sizeof(version_buf),
                      "CUDA %s (driver=%d.%d, runtime=%d.%d)",
                      CUDA_BACKEND_VERSION,
                      driver_version / 1000, (driver_version % 1000) / 10,
                      runtime_version / 1000, (runtime_version % 1000) / 10);
    (void) ignore;
    return version_buf;
}

#endif /* LV_HAS_CUDA */