/**
 * @file hip_backend.c
 * @brief AMD HIP GPU 数值后端实现
 *
 * 提供 HIP 后端的向量/矩阵/线性求解器操作表注册。
 * 需要 ROCm 平台 (HIP) 编译。
 *
 * 编译时通过宏 LV_HAS_HIP 控制：
 *   - 定义时：链接 HIP 运行时库，提供完整 GPU 加速功能
 *   - 未定义时：提供优雅降级的存根实现
 *
 * @author Lv-00 Project
 * @version v1.0.0
 * @date 2026-07-30
 *
 * @dependencies
 *   - lv/backends/hip_backend.h : HIP 后端公共接口
 *   - lv/lv_utils.h            : 统一内存分配器
 *   - debug.h                  : 日志与断言
 *   - lv_internal.h            : 内部工具宏
 *   - hip/hip_runtime.h (可选) : HIP 运行时 (LV_HAS_HIP)
 */

/* ========================================================================
 * 包含头文件
 * ======================================================================== */

#include "lv/backends/hip_backend.h"
#include "lv/lv_str_utils.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#include "lv/bicgstab_shared.h"
#include "lv/gmres_shared.h"
#include "lv/lv_utils.h"
#include "debug.h"
#include "lv/lv_internal.h"

#ifdef LV_HAS_HIP
#include <hip/hip_runtime.h>
#endif /* LV_HAS_HIP */

/* ========================================================================
 * 后端版本信息
 * ======================================================================== */

/** @brief HIP 后端版本字符串 */
#define HIP_BACKEND_VERSION "1.0.0"

/* ========================================================================
 * 通用（非 HIP）实现 — 当 LV_HAS_HIP 未定义时的存根
 * ======================================================================== */

#ifndef LV_HAS_HIP

int lv_hip_register_backend(void) {
    LOG_WARN("hip_backend", "HIP 后端不可用：未定义 LV_HAS_HIP（需要 ROCm HIP SDK）");
    return -1;
}

int lv_hip_available(void) {
    return 0;
}

int lv_hip_device_count(void) {
    return 0;
}

const char *lv_hip_backend_version(void) {
    return "HIP (unavailable - stub)";
}

#else /* LV_HAS_HIP — 完整实现 */

/* ========================================================================
 * 数据类型定义
 * ======================================================================== */

/**
 * @brief HIP 向量私有数据结构
 *
 * 包含设备端数据指针和分配大小。
 */
typedef struct {
    double *d_data;  /**< 设备端数据指针 */
    int64_t length;  /**< 向量长度 */
} HipVectorData;

/**
 * @brief HIP 矩阵私有数据结构
 *
 * 包含设备端稠密矩阵数据（列主序）和维度信息。
 */
typedef struct {
    double *d_data;  /**< 设备端矩阵数据（列主序） */
    int64_t rows;    /**< 行数 */
    int64_t cols;    /**< 列数 */
} HipMatrixData;

/**
 * @brief HIP 稠密 LU 求解器私有数据
 */
typedef struct {
    lvMatrix *clone; /**< 矩阵副本（含设备端数据） */
    bool factored;   /**< 是否已完成分解 */
} HipDenseLUData;

/**
 * @brief HIP 迭代求解器私有数据
 */
typedef struct {
    lvMatrix *clone;  /**< 矩阵副本 */
    int max_iters;    /**< 最大迭代次数 */
    double tol;       /**< 收敛容差 */
    double *d_r;      /**< 设备端残差向量 */
    double *d_p;      /**< 设备端方向向量 */
    double *d_ap;     /**< 设备端 A*p 向量 */
    double *d_work;   /**< 设备端通用工作向量 */
    double *d_r0;     /**< 设备端影子残差向量（BiCGSTAB） */
    double *d_t;      /**< 设备端 t 向量（BiCGSTAB） */
} HipIterSolverData;

/* ========================================================================
 * HIP 内核函数声明
 * ======================================================================== */

/**
 * @brief 向量标量乘法内核: v[i] = c * v[i]
 */
static __global__ void vector_scale_kernel(double *v, double c, int64_t n) {
    int64_t idx = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    if (idx < n) {
        v[idx] *= c;
    }
}

/**
 * @brief 向量常量设置内核: v[i] = c
 */
static __global__ void vector_const_set_kernel(double *v, double c, int64_t n) {
    int64_t idx = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    if (idx < n) {
        v[idx] = c;
    }
}

/**
 * @brief 向量线性组合内核: z[i] = a * x[i] + b * y[i]
 */
static __global__ void vector_linear_sum_kernel(double a, const double *x, double b,
                                                 const double *y, double *z, int64_t n) {
    int64_t idx = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    if (idx < n) {
        z[idx] = a * x[idx] + b * y[idx];
    }
}

/**
 * @brief 向量点积内核（归约版本）：每线程部分和，需后续在 CPU 端归约
 */
static __global__ void vector_dot_kernel(const double *x, const double *y,
                                          double *partial, int64_t n) {
    extern __shared__ double sdata[];
    int tid = threadIdx.x;
    int64_t gid = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)tid;
    double sum = 0.0;
    while (gid < n) {
        sum += x[gid] * y[gid];
        gid += (int64_t)blockDim.x * (int64_t)gridDim.x;
    }
    sdata[tid] = sum;
    __syncthreads();
    /* 树形归约 */
    for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }
    if (tid == 0) {
        partial[blockIdx.x] = sdata[0];
    }
}

/**
 * @brief 向量 L2 范数内核（归约版本）
 */
static __global__ void vector_norm_kernel(const double *v, double *partial, int64_t n) {
    extern __shared__ double sdata[];
    int tid = threadIdx.x;
    int64_t gid = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)tid;
    double sum = 0.0;
    while (gid < n) {
        sum += v[gid] * v[gid];
        gid += (int64_t)blockDim.x * (int64_t)gridDim.x;
    }
    sdata[tid] = sum;
    __syncthreads();
    for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }
    if (tid == 0) {
        partial[blockIdx.x] = sdata[0];
    }
}

/**
 * @brief 向量最大绝对值内核（归约版本）
 */
static __global__ void vector_max_norm_kernel(const double *v, double *partial, int64_t n) {
    extern __shared__ double sdata[];
    int tid = threadIdx.x;
    int64_t gid = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)tid;
    double local_max = 0.0;
    while (gid < n) {
        double av = fabs(v[gid]);
        if (av > local_max) local_max = av;
        gid += (int64_t)blockDim.x * (int64_t)gridDim.x;
    }
    sdata[tid] = local_max;
    __syncthreads();
    for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            if (sdata[tid + s] > sdata[tid]) sdata[tid] = sdata[tid + s];
        }
        __syncthreads();
    }
    if (tid == 0) {
        partial[blockIdx.x] = sdata[0];
    }
}

/**
 * @brief 向量加权 RMS 范数内核（归约版本）
 */
static __global__ void vector_wrms_norm_kernel(const double *v, const double *weights,
                                                 double *partial, int64_t n) {
    extern __shared__ double sdata[];
    int tid = threadIdx.x;
    int64_t gid = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)tid;
    double sum = 0.0;
    while (gid < n) {
        double wv = v[gid] * weights[gid];
        sum += wv * wv;
        gid += (int64_t)blockDim.x * (int64_t)gridDim.x;
    }
    sdata[tid] = sum;
    __syncthreads();
    for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }
    if (tid == 0) {
        partial[blockIdx.x] = sdata[0] / (double)n;
    }
}

/**
 * @brief 向量逐元素绝对值内核: v[i] = |v[i]|
 */
static __global__ void vector_abs_kernel(double *v, int64_t n) {
    int64_t idx = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    if (idx < n) {
        v[idx] = fabs(v[idx]);
    }
}

/**
 * @brief 向量逐元素除法内核: v[i] = v[i] / d[i]
 */
static __global__ void vector_inv_kernel(double *v, const double *d, int64_t n) {
    int64_t idx = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    if (idx < n) {
        double denom = d[idx];
        v[idx] = (fabs(denom) > 1e-30) ? (v[idx] / denom) : 1e30;
    }
}

/**
 * @brief 向量逐元素最大值内核: v[i] = max(v[i], c)
 */
static __global__ void vector_compare_kernel(double *v, double c, int64_t n) {
    int64_t idx = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    if (idx < n) {
        if (v[idx] < c) v[idx] = c;
    }
}

/**
 * @brief 矩阵-向量乘法内核（列主序稠密）: y[i] = sum_j A[j][i] * x[j]
 */
static __global__ void matvec_kernel(const double *A, const double *x,
                                      double *y, int64_t rows, int64_t cols) {
    int64_t i = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    if (i < rows) {
        double sum = 0.0;
        for (int64_t j = 0; j < cols; ++j) {
            double xj = x[j];
            if (fabs(xj) > 1e-30) {
                sum += A[j * rows + i] * xj;
            }
        }
        y[i] = sum;
    }
}

/**
 * @brief 矩阵标量乘法内核: A[i] = c * A[i]
 */
static __global__ void matrix_scale_kernel(double *A, double c, int64_t n) {
    int64_t idx = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    if (idx < n) {
        A[idx] *= c;
    }
}

/**
 * @brief 向量复制内核: dst[i] = src[i]
 */
static __global__ void vector_copy_kernel(double *dst, const double *src, int64_t n) {
    int64_t idx = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    if (idx < n) {
        dst[idx] = src[idx];
    }
}

/* ========================================================================
 * 内核启动辅助宏
 * ======================================================================== */

/** @brief 默认 HIP 线程块大小 */
#define HIP_BLOCK_SIZE 256

/**
 * @brief 计算 GPU 网格大小（向上取整）
 */
static inline int64_t hip_grid_size(int64_t n) {
    return (n + HIP_BLOCK_SIZE - 1) / HIP_BLOCK_SIZE;
}

/* ========================================================================
 * 辅助函数：设备端归约结果汇总（将部分和从设备拷贝到主机后求和/取最大）
 * ======================================================================== */

/**
 * @brief 在 CPU 上汇总设备端归约内核产生的部分和数组
 */
static double hip_reduce_sum(double *d_partial, int num_blocks) {
    double *h_partial = (double *)lv_calloc((size_t)num_blocks, sizeof(double));
    if (!h_partial) return 0.0;
    hipMemcpy(h_partial, d_partial, (size_t)num_blocks * sizeof(double), hipMemcpyDeviceToHost);
    double sum = 0.0;
    for (int i = 0; i < num_blocks; ++i) sum += h_partial[i];
    lv_free((void **)&h_partial);
    return sum;
}

/**
 * @brief 在 CPU 上汇总设备端归约内核产生的最大值数组
 */
static double hip_reduce_max(double *d_partial, int num_blocks) {
    double *h_partial = (double *)lv_calloc((size_t)num_blocks, sizeof(double));
    if (!h_partial) return 0.0;
    hipMemcpy(h_partial, d_partial, (size_t)num_blocks * sizeof(double), hipMemcpyDeviceToHost);
    double max_val = h_partial[0];
    for (int i = 1; i < num_blocks; ++i) {
        if (h_partial[i] > max_val) max_val = h_partial[i];
    }
    lv_free((void **)&h_partial);
    return max_val;
}

/* ========================================================================
 * 前向声明：HIP 后端操作函数
 * ======================================================================== */

static lvVector *hip_vector_create(int64_t n);
static lvVector *hip_vector_clone(const lvVector *v);
static void hip_vector_destroy(lvVector *v);
static void hip_vector_zero(lvVector *v);
static void hip_vector_const_set(lvVector *v, double c);
static void hip_vector_copy(lvVector *dst, const lvVector *src);
static void hip_vector_scale(lvVector *v, double c);
static void hip_vector_linear_sum(double a, const lvVector *x, double b, const lvVector *y, lvVector *z);
static double hip_vector_dot(const lvVector *x, const lvVector *y);
static double hip_vector_norm(const lvVector *v);
static double hip_vector_max_norm(const lvVector *v);
static double hip_vector_wrms_norm(const lvVector *v, const lvVector *weights);
static void hip_vector_abs(lvVector *v);
static void hip_vector_inv(lvVector *v, const lvVector *d);
static void hip_vector_compare(lvVector *v, double c);
static int64_t hip_vector_length(const lvVector *v);
static double *hip_vector_data_ptr(lvVector *v);

static lvMatrix *hip_matrix_create(int64_t rows, int64_t cols, bool sparse);
static lvMatrix *hip_matrix_clone(const lvMatrix *A);
static void hip_matrix_destroy(lvMatrix *A);
static void hip_matrix_zero(lvMatrix *A);
static void hip_matrix_copy(lvMatrix *dst, const lvMatrix *src);
static int hip_matrix_matvec(const lvMatrix *A, const lvVector *x, lvVector *y);
static void hip_matrix_scale(lvMatrix *A, double c);
static void hip_matrix_set_element(lvMatrix *A, int64_t row, int64_t col, double val);
static double hip_matrix_get_element(const lvMatrix *A, int64_t row, int64_t col);
static int hip_matrix_factor(lvMatrix *A);
static int hip_matrix_solve(const lvMatrix *A, const lvVector *b, lvVector *x);

static int hip_dense_linsol_setup(lvLinearSolver *LS, const lvMatrix *A);
static lvLinearSolver *hip_linsol_create(lvLinearSolverMethod method);
static int hip_dense_linsol_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);
static void hip_dense_linsol_destroy(lvLinearSolver *LS);

static int hip_iter_linsol_setup(lvLinearSolver *LS, const lvMatrix *A);
static int hip_gmres_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);
static int hip_bicgstab_solve(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);

/* HIP 共享迭代内核算子声明（GMRES 复用 BiCGSTAB 算子，见下方实现） */
static double hip_bicgstab_dot(void *ctx, const double *a, const double *b, int64_t n);
static double hip_bicgstab_norm(void *ctx, const double *v, int64_t n);
static void hip_bicgstab_matvec(void *ctx, const lvMatrix *A, const double *x, double *y, int64_t n);
static void hip_iter_linsol_destroy(lvLinearSolver *LS);

/* ========================================================================
 * 静态操作表（HIP 后端）
 * ======================================================================== */

/** @brief HIP 向量操作表 */
static const lvVectorOps hip_vector_ops = {
    hip_vector_create,
    hip_vector_clone,       hip_vector_destroy,      hip_vector_zero,
    hip_vector_const_set,   hip_vector_copy,         hip_vector_scale,
    hip_vector_linear_sum,  hip_vector_dot,          hip_vector_norm,
    hip_vector_max_norm,    hip_vector_wrms_norm,    hip_vector_abs,
    hip_vector_inv,         hip_vector_compare,      hip_vector_length,
    hip_vector_data_ptr,
};

/** @brief HIP 稠密矩阵操作表 */
static const lvMatrixOps hip_dense_matrix_ops = {
    hip_matrix_create,
    hip_matrix_clone,    hip_matrix_destroy,  hip_matrix_zero,
    hip_matrix_copy,     hip_matrix_matvec,   hip_matrix_scale,
    hip_matrix_set_element, hip_matrix_get_element,
    hip_matrix_factor,   hip_matrix_solve,
};

/** @brief HIP 稠密 LU 求解器操作表 */
static const lvLinearSolverOps hip_dense_linsol_ops = {
    hip_linsol_create,
    hip_dense_linsol_setup,
    hip_dense_linsol_solve,
    hip_dense_linsol_destroy,
};

/** @brief HIP GMRES 求解器操作表 */
static const lvLinearSolverOps hip_gmres_linsol_ops = {
    hip_linsol_create,
    hip_iter_linsol_setup,
    hip_gmres_solve,
    hip_iter_linsol_destroy,
};

/** @brief HIP BiCGSTAB 求解器操作表 */
static const lvLinearSolverOps hip_bicgstab_linsol_ops = {
    hip_linsol_create,
    hip_iter_linsol_setup,
    hip_bicgstab_solve,
    hip_iter_linsol_destroy,
};

/* ========================================================================
 * 第一部分：向量操作实现（HIP）
 * ======================================================================== */

/**
 * @brief 深拷贝向量（包含设备端数据）
 */
static lvVector *hip_vector_clone(const lvVector *v) {
    lv_CHECK_NULL(v, NULL);

    HipVectorData *src_vd = (HipVectorData *)v->backend_data;
    lv_CHECK_NULL(src_vd, NULL);

    int64_t n = v->length;

    lvVector *clone = lv_calloc(1, sizeof(lvVector));
    lv_CHECK_ALLOC(clone, NULL);

    clone->length = n;
    clone->backend = lv_BACKEND_HIP;
    clone->ops = &hip_vector_ops;
    clone->data = NULL; /* HIP 后端不在主机端维护数据指针 */
    clone->backend_data = NULL;

    HipVectorData *vd = lv_calloc(1, sizeof(HipVectorData));
    if (!vd) {
        lv_free((void **)&clone);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "HipVectorData 分配失败");
    }
    vd->length = n;

    hipError_t err = hipMalloc(&vd->d_data, (size_t)n * sizeof(double));
    if (err != hipSuccess) {
        lv_free((void **)&vd);
        lv_free((void **)&clone);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "HIP 向量数据分配失败，长度=%lld", (long long)n);
    }

    err = hipMemcpy(vd->d_data, src_vd->d_data, (size_t)n * sizeof(double), hipMemcpyDeviceToDevice);
    if (err != hipSuccess) {
        hipFree(vd->d_data);
        lv_free((void **)&vd);
        lv_free((void **)&clone);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "HIP 向量数据复制失败");
    }

    clone->backend_data = vd;
    return clone;
}

/**
 * @brief 销毁 HIP 向量
 */
static void hip_vector_destroy(lvVector *v) {
    if (!v) return;
    if (v->backend_data) {
        HipVectorData *vd = (HipVectorData *)v->backend_data;
        if (vd->d_data) {
            hipFree(vd->d_data);
            vd->d_data = NULL;
        }
        lv_free((void **)&v->backend_data);
    }
    lv_free((void **)&v);
}

/**
 * @brief 向量置零
 */
static void hip_vector_zero(lvVector *v) {
    if (!v || !v->backend_data) return;
    HipVectorData *vd = (HipVectorData *)v->backend_data;
    if (!vd->d_data) return;
    hipMemset(vd->d_data, 0, (size_t)vd->length * sizeof(double));
}

/**
 * @brief 向量设为常量 c
 */
static void hip_vector_const_set(lvVector *v, double c) {
    if (!v || !v->backend_data) return;
    HipVectorData *vd = (HipVectorData *)v->backend_data;
    if (!vd->d_data || vd->length <= 0) return;
    int64_t grid = hip_grid_size(vd->length);
    hipLaunchKernelGGL(vector_const_set_kernel, dim3((int)grid), dim3(HIP_BLOCK_SIZE), 0, 0,
                       vd->d_data, c, vd->length);
}

/**
 * @brief 深拷贝：dst = src
 */
static void hip_vector_copy(lvVector *dst, const lvVector *src) {
    if (!dst || !src || !dst->backend_data || !src->backend_data) return;
    HipVectorData *dst_vd = (HipVectorData *)dst->backend_data;
    HipVectorData *src_vd = (HipVectorData *)src->backend_data;
    if (!dst_vd->d_data || !src_vd->d_data) return;
    int64_t n = (dst_vd->length < src_vd->length) ? dst_vd->length : src_vd->length;
    hipMemcpy(dst_vd->d_data, src_vd->d_data, (size_t)n * sizeof(double), hipMemcpyDeviceToDevice);
}

/**
 * @brief 标量乘法：v = c * v
 */
static void hip_vector_scale(lvVector *v, double c) {
    if (!v || !v->backend_data) return;
    HipVectorData *vd = (HipVectorData *)v->backend_data;
    if (!vd->d_data || vd->length <= 0) return;
    int64_t grid = hip_grid_size(vd->length);
    hipLaunchKernelGGL(vector_scale_kernel, dim3((int)grid), dim3(HIP_BLOCK_SIZE), 0, 0,
                       vd->d_data, c, vd->length);
}

/**
 * @brief 线性组合：z = a*x + b*y
 */
static void hip_vector_linear_sum(double a, const lvVector *x, double b,
                                   const lvVector *y, lvVector *z) {
    if (!x || !y || !z || !x->backend_data || !y->backend_data || !z->backend_data) return;
    HipVectorData *xd = (HipVectorData *)x->backend_data;
    HipVectorData *yd = (HipVectorData *)y->backend_data;
    HipVectorData *zd = (HipVectorData *)z->backend_data;
    if (!xd->d_data || !yd->d_data || !zd->d_data) return;
    int64_t n = xd->length;
    if (yd->length < n) n = yd->length;
    if (zd->length < n) n = zd->length;
    if (n <= 0) return;
    int64_t grid = hip_grid_size(n);
    hipLaunchKernelGGL(vector_linear_sum_kernel, dim3((int)grid), dim3(HIP_BLOCK_SIZE), 0, 0,
                       a, xd->d_data, b, yd->d_data, zd->d_data, n);
}

/**
 * @brief 点积（内积）
 */
static double hip_vector_dot(const lvVector *x, const lvVector *y) {
    if (!x || !y || !x->backend_data || !y->backend_data) return 0.0;
    HipVectorData *xd = (HipVectorData *)x->backend_data;
    HipVectorData *yd = (HipVectorData *)y->backend_data;
    if (!xd->d_data || !yd->d_data) return 0.0;
    int64_t n = xd->length;
    if (yd->length < n) n = yd->length;
    if (n <= 0) return 0.0;

    int num_blocks = (int)hip_grid_size(n);
    double *d_partial = NULL;
    hipError_t err = hipMalloc(&d_partial, (size_t)num_blocks * sizeof(double));
    if (err != hipSuccess) return 0.0;

    size_t shared_mem = (size_t)HIP_BLOCK_SIZE * sizeof(double);
    hipLaunchKernelGGL(vector_dot_kernel, dim3(num_blocks), dim3(HIP_BLOCK_SIZE), shared_mem, 0,
                       xd->d_data, yd->d_data, d_partial, n);

    double result = hip_reduce_sum(d_partial, num_blocks);
    hipFree(d_partial);
    return result;
}

/**
 * @brief L2 范数
 */
static double hip_vector_norm(const lvVector *v) {
    if (!v || !v->backend_data) return 0.0;
    HipVectorData *vd = (HipVectorData *)v->backend_data;
    if (!vd->d_data || vd->length <= 0) return 0.0;

    int64_t n = vd->length;
    int num_blocks = (int)hip_grid_size(n);
    double *d_partial = NULL;
    hipError_t err = hipMalloc(&d_partial, (size_t)num_blocks * sizeof(double));
    if (err != hipSuccess) return 0.0;

    size_t shared_mem = (size_t)HIP_BLOCK_SIZE * sizeof(double);
    hipLaunchKernelGGL(vector_norm_kernel, dim3(num_blocks), dim3(HIP_BLOCK_SIZE), shared_mem, 0,
                       vd->d_data, d_partial, n);

    double sum_sq = hip_reduce_sum(d_partial, num_blocks);
    hipFree(d_partial);
    return sqrt(sum_sq);
}

/**
 * @brief 无穷范数（最大绝对值）
 */
static double hip_vector_max_norm(const lvVector *v) {
    if (!v || !v->backend_data) return 0.0;
    HipVectorData *vd = (HipVectorData *)v->backend_data;
    if (!vd->d_data || vd->length <= 0) return 0.0;

    int64_t n = vd->length;
    int num_blocks = (int)hip_grid_size(n);
    double *d_partial = NULL;
    hipError_t err = hipMalloc(&d_partial, (size_t)num_blocks * sizeof(double));
    if (err != hipSuccess) return 0.0;

    size_t shared_mem = (size_t)HIP_BLOCK_SIZE * sizeof(double);
    hipLaunchKernelGGL(vector_max_norm_kernel, dim3(num_blocks), dim3(HIP_BLOCK_SIZE), shared_mem, 0,
                       vd->d_data, d_partial, n);

    double result = hip_reduce_max(d_partial, num_blocks);
    hipFree(d_partial);
    return result;
}

/**
 * @brief 加权 RMS 范数
 */
static double hip_vector_wrms_norm(const lvVector *v, const lvVector *weights) {
    if (!v || !weights || !v->backend_data || !weights->backend_data) return 0.0;
    HipVectorData *vd = (HipVectorData *)v->backend_data;
    HipVectorData *wd = (HipVectorData *)weights->backend_data;
    if (!vd->d_data || !wd->d_data) return 0.0;

    int64_t n = vd->length;
    if (wd->length < n) n = wd->length;
    if (n <= 0) return 0.0;

    int num_blocks = (int)hip_grid_size(n);
    double *d_partial = NULL;
    hipError_t err = hipMalloc(&d_partial, (size_t)num_blocks * sizeof(double));
    if (err != hipSuccess) return 0.0;

    size_t shared_mem = (size_t)HIP_BLOCK_SIZE * sizeof(double);
    hipLaunchKernelGGL(vector_wrms_norm_kernel, dim3(num_blocks), dim3(HIP_BLOCK_SIZE), shared_mem, 0,
                       vd->d_data, wd->d_data, d_partial, n);

    double avg = hip_reduce_sum(d_partial, num_blocks);
    hipFree(d_partial);
    return sqrt(avg);
}

/**
 * @brief 逐元素绝对值
 */
static void hip_vector_abs(lvVector *v) {
    if (!v || !v->backend_data) return;
    HipVectorData *vd = (HipVectorData *)v->backend_data;
    if (!vd->d_data || vd->length <= 0) return;
    int64_t grid = hip_grid_size(vd->length);
    hipLaunchKernelGGL(vector_abs_kernel, dim3((int)grid), dim3(HIP_BLOCK_SIZE), 0, 0,
                       vd->d_data, vd->length);
}

/**
 * @brief 逐元素除法：v_i = v_i / d_i
 */
static void hip_vector_inv(lvVector *v, const lvVector *d) {
    if (!v || !d || !v->backend_data || !d->backend_data) return;
    HipVectorData *vd = (HipVectorData *)v->backend_data;
    HipVectorData *dd = (HipVectorData *)d->backend_data;
    if (!vd->d_data || !dd->d_data) return;
    int64_t n = (vd->length < dd->length) ? vd->length : dd->length;
    if (n <= 0) return;
    int64_t grid = hip_grid_size(n);
    hipLaunchKernelGGL(vector_inv_kernel, dim3((int)grid), dim3(HIP_BLOCK_SIZE), 0, 0,
                       vd->d_data, dd->d_data, n);
}

/**
 * @brief 逐元素最大值：v_i = max(v_i, c)
 */
static void hip_vector_compare(lvVector *v, double c) {
    if (!v || !v->backend_data) return;
    HipVectorData *vd = (HipVectorData *)v->backend_data;
    if (!vd->d_data || vd->length <= 0) return;
    int64_t grid = hip_grid_size(vd->length);
    hipLaunchKernelGGL(vector_compare_kernel, dim3((int)grid), dim3(HIP_BLOCK_SIZE), 0, 0,
                       vd->d_data, c, vd->length);
}

/**
 * @brief 获取向量长度
 */
static int64_t hip_vector_length(const lvVector *v) {
    if (!v) return 0;
    return v->length;
}

/**
 * @brief 获取底层原始数据指针
 *
 * HIP 后端的设备数据不可直接从主机访问，返回 NULL。
 */
static double *hip_vector_data_ptr(lvVector *v) {
    (void)v;
    return NULL;
}

/* ========================================================================
 * 第二部分：矩阵操作实现（HIP 稠密）
 * ======================================================================== */

/**
 * @brief 深拷贝矩阵
 */
static lvMatrix *hip_matrix_clone(const lvMatrix *A) {
    lv_CHECK_NULL(A, NULL);

    HipMatrixData *src_md = (HipMatrixData *)A->data;
    lv_CHECK_NULL(src_md, NULL);

    int64_t rows = A->rows;
    int64_t cols = A->cols;

    lvMatrix *clone = lv_calloc(1, sizeof(lvMatrix));
    lv_CHECK_ALLOC(clone, NULL);

    clone->rows = rows;
    clone->cols = cols;
    clone->sparse = A->sparse;
    clone->format = A->format;
    clone->backend = lv_BACKEND_HIP;
    clone->ops = A->ops;
    clone->data = NULL;
    clone->backend_data = NULL;

    HipMatrixData *md = lv_calloc(1, sizeof(HipMatrixData));
    if (!md) {
        lv_free((void **)&clone);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "HipMatrixData 分配失败");
    }
    md->rows = rows;
    md->cols = cols;

    size_t data_size = (size_t)(rows * cols) * sizeof(double);
    hipError_t err = hipMalloc(&md->d_data, data_size);
    if (err != hipSuccess) {
        lv_free((void **)&md);
        lv_free((void **)&clone);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "HIP 矩阵数据分配失败 %lldx%lld",
                     (long long)rows, (long long)cols);
        return NULL;
    }

    err = hipMemcpy(md->d_data, src_md->d_data, data_size, hipMemcpyDeviceToDevice);
    if (err != hipSuccess) {
        hipFree(md->d_data);
        lv_free((void **)&md);
        lv_free((void **)&clone);
        return NULL;
    }

    clone->data = md;
    return clone;
}

/**
 * @brief 销毁 HIP 矩阵
 */
static void hip_matrix_destroy(lvMatrix *A) {
    if (!A) return;
    if (A->data) {
        HipMatrixData *md = (HipMatrixData *)A->data;
        if (md->d_data) {
            hipFree(md->d_data);
            md->d_data = NULL;
        }
        lv_free((void **)&A->data);
    }
    lv_free((void **)&A);
}

/**
 * @brief 矩阵置零
 */
static void hip_matrix_zero(lvMatrix *A) {
    if (!A || !A->data) return;
    HipMatrixData *md = (HipMatrixData *)A->data;
    if (!md->d_data) return;
    hipMemset(md->d_data, 0, (size_t)(md->rows * md->cols) * sizeof(double));
}

/**
 * @brief 深拷贝：dst = src
 */
static void hip_matrix_copy(lvMatrix *dst, const lvMatrix *src) {
    if (!dst || !src || !dst->data || !src->data) return;
    HipMatrixData *dst_md = (HipMatrixData *)dst->data;
    HipMatrixData *src_md = (HipMatrixData *)src->data;
    if (!dst_md->d_data || !src_md->d_data) return;
    int64_t elems = dst_md->rows * dst_md->cols;
    int64_t src_elems = src_md->rows * src_md->cols;
    int64_t n = (elems < src_elems) ? elems : src_elems;
    hipMemcpy(dst_md->d_data, src_md->d_data, (size_t)n * sizeof(double), hipMemcpyDeviceToDevice);
}

/**
 * @brief 矩阵-向量乘法：y = A * x（列主序稠密）
 */
static int hip_matrix_matvec(const lvMatrix *A, const lvVector *x, lvVector *y) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(y, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(x->backend_data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(y->backend_data, lv_BACKEND_NOT_INITIALIZED);

    if (A->cols != x->length || A->rows != y->length) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS,
                     "HIP matvec 维度不匹配: A(%lldx%lld), x(%lld), y(%lld)",
                     (long long)A->rows, (long long)A->cols,
                     (long long)x->length, (long long)y->length);
        return lv_BACKEND_INVALID_ARGS;
    }

    HipMatrixData *md = (HipMatrixData *)A->data;
    HipVectorData *xd = (HipVectorData *)x->backend_data;
    HipVectorData *yd = (HipVectorData *)y->backend_data;

    int64_t grid = hip_grid_size(A->rows);
    hipLaunchKernelGGL(matvec_kernel, dim3((int)grid), dim3(HIP_BLOCK_SIZE), 0, 0,
                       md->d_data, xd->d_data, yd->d_data, A->rows, A->cols);

    return lv_BACKEND_OK;
}

/**
 * @brief 矩阵-标量乘法：A = c * A
 */
static void hip_matrix_scale(lvMatrix *A, double c) {
    if (!A || !A->data) return;
    HipMatrixData *md = (HipMatrixData *)A->data;
    if (!md->d_data) return;
    int64_t n = md->rows * md->cols;
    if (n <= 0) return;
    int64_t grid = hip_grid_size(n);
    hipLaunchKernelGGL(matrix_scale_kernel, dim3((int)grid), dim3(HIP_BLOCK_SIZE), 0, 0,
                       md->d_data, c, n);
}

/**
 * @brief 设置单个元素值（列主序）
 *
 * 由于 HIP 设备内存不能直接由主机写入，此操作通过
 * 将整个矩阵数据从设备拷贝到主机、修改后再拷贝回设备实现。
 */
static void hip_matrix_set_element(lvMatrix *A, int64_t row, int64_t col, double val) {
    if (!A || !A->data || row < 0 || col < 0 || row >= A->rows || col >= A->cols) return;
    HipMatrixData *md = (HipMatrixData *)A->data;
    if (!md->d_data) return;

    int64_t n = md->rows * md->cols;
    double *h_data = (double *)lv_calloc((size_t)n, sizeof(double));
    if (!h_data) return;

    hipMemcpy(h_data, md->d_data, (size_t)n * sizeof(double), hipMemcpyDeviceToHost);
    h_data[col * md->rows + row] = val;
    hipMemcpy(md->d_data, h_data, (size_t)n * sizeof(double), hipMemcpyHostToDevice);
    lv_free((void **)&h_data);
}

/**
 * @brief 获取单个元素值（列主序）
 */
static double hip_matrix_get_element(const lvMatrix *A, int64_t row, int64_t col) {
    if (!A || !A->data || row < 0 || col < 0 || row >= A->rows || col >= A->cols) return 0.0;
    HipMatrixData *md = (HipMatrixData *)A->data;
    if (!md->d_data) return 0.0;

    double val = 0.0;
    hipMemcpy(&val, md->d_data + (col * md->rows + row), sizeof(double), hipMemcpyDeviceToHost);
    return val;
}

/**
 * @brief 就地 LU 分解（在主机端执行）
 *
 * HIP 后端将设备数据拷贝到主机执行 LU 分解，因为该操作
 * 涉及复杂的行交换和条件分支，在 GPU 上实现效率不高。
 */
static int hip_matrix_factor(lvMatrix *A) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->data, lv_BACKEND_NOT_INITIALIZED);

    if (A->rows != A->cols) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "HIP LU分解要求方阵，当前为 %lldx%lld",
                     (long long)A->rows, (long long)A->cols);
        return lv_BACKEND_INVALID_ARGS;
    }

    HipMatrixData *md = (HipMatrixData *)A->data;
    int64_t n = A->rows;
    size_t data_size = (size_t)(n * n) * sizeof(double);

    /* 拷贝设备数据到主机 */
    double *h_data = (double *)lv_calloc((size_t)(n * n), sizeof(double));
    if (!h_data) return lv_BACKEND_MEM_ERROR;
    hipMemcpy(h_data, md->d_data, data_size, hipMemcpyDeviceToHost);

    /* 主机端 LU 分解 */
    for (int64_t k = 0; k < n; ++k) {
        double pivot = fabs(h_data[k * n + k]);
        int64_t pivot_row = k;
        for (int64_t i = k + 1; i < n; ++i) {
            double abs_val = fabs(h_data[k * n + i]);
            if (abs_val > pivot) {
                pivot = abs_val;
                pivot_row = i;
            }
        }
        if (pivot < 1e-12) {
            lv_free((void **)&h_data);
            lv_ERROR_SET(lv_BACKEND_LINSOL_FAILED,
                         "HIP LU分解遇到奇异矩阵，pivot≈0 at col=%lld", (long long)k);
            return lv_BACKEND_LINSOL_FAILED;
        }
        if (pivot_row != k) {
            for (int64_t j = 0; j < n; ++j) {
                double tmp = h_data[j * n + k];
                h_data[j * n + k] = h_data[j * n + pivot_row];
                h_data[j * n + pivot_row] = tmp;
            }
        }
        double inv_pivot = 1.0 / h_data[k * n + k];
        for (int64_t i = k + 1; i < n; ++i) {
            double factor = h_data[k * n + i] * inv_pivot;
            h_data[k * n + i] = factor;
            for (int64_t j = k + 1; j < n; ++j) {
                h_data[j * n + i] -= factor * h_data[j * n + k];
            }
        }
    }

    /* 写回设备 */
    hipMemcpy(md->d_data, h_data, data_size, hipMemcpyHostToDevice);
    lv_free((void **)&h_data);
    return lv_BACKEND_OK;
}

/**
 * @brief 使用已分解矩阵求解 A * x = b（在主机端执行）
 */
static int hip_matrix_solve(const lvMatrix *A, const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A->data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(b->backend_data, lv_BACKEND_NOT_INITIALIZED);
    lv_CHECK_NULL(x->backend_data, lv_BACKEND_NOT_INITIALIZED);

    if (A->rows != A->cols) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "HIP solve要求方阵，当前为 %lldx%lld",
                     (long long)A->rows, (long long)A->cols);
        return lv_BACKEND_INVALID_ARGS;
    }

    HipMatrixData *md = (HipMatrixData *)A->data;
    HipVectorData *bd = (HipVectorData *)b->backend_data;
    HipVectorData *xd = (HipVectorData *)x->backend_data;
    int64_t n = A->rows;

    /* 拷贝到主机 */
    size_t data_size = (size_t)(n * n) * sizeof(double);
    double *h_data = (double *)lv_calloc((size_t)(n * n), sizeof(double));
    double *h_b = (double *)lv_calloc((size_t)n, sizeof(double));
    double *h_x = (double *)lv_calloc((size_t)n, sizeof(double));
    if (!h_data || !h_b || !h_x) {
        if (h_data) lv_free((void **)&h_data);
        if (h_b) lv_free((void **)&h_b);
        if (h_x) lv_free((void **)&h_x);
        return lv_BACKEND_MEM_ERROR;
    }

    hipMemcpy(h_data, md->d_data, data_size, hipMemcpyDeviceToHost);
    hipMemcpy(h_b, bd->d_data, (size_t)n * sizeof(double), hipMemcpyDeviceToHost);

    /* 前代消去（解 L*y = x） */
    memcpy(h_x, h_b, (size_t)n * sizeof(double));
    for (int64_t k = 0; k < n; ++k) {
        for (int64_t i = k + 1; i < n; ++i) {
            h_x[i] -= h_data[k * n + i] * h_x[k];
        }
    }

    /* 回代（解 U*x = y） */
    for (int64_t k = n - 1; k >= 0; --k) {
        double diag = h_data[k * n + k];
        if (fabs(diag) < 1e-12) {
            lv_free((void **)&h_data);
            lv_free((void **)&h_b);
            lv_free((void **)&h_x);
            return lv_BACKEND_LINSOL_FAILED;
        }
        h_x[k] /= diag;
        for (int64_t i = 0; i < k; ++i) {
            h_x[i] -= h_data[k * n + i] * h_x[k];
        }
    }

    /* 写回设备 */
    hipMemcpy(xd->d_data, h_x, (size_t)n * sizeof(double), hipMemcpyHostToDevice);

    lv_free((void **)&h_data);
    lv_free((void **)&h_b);
    lv_free((void **)&h_x);
    return lv_BACKEND_OK;
}

/* ========================================================================
 * 第三部分：线性求解器实现（HIP 稠密 LU）
 * ======================================================================== */

/**
 * @brief 设置稠密 LU 线性求解器
 */
static int hip_dense_linsol_setup(lvLinearSolver *LS, const lvMatrix *A) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);

    HipDenseLUData *lu = lv_calloc(1, sizeof(HipDenseLUData));
    lv_CHECK_ALLOC(lu, lv_BACKEND_MEM_ERROR);

    lu->clone = hip_matrix_clone(A);
    if (!lu->clone) {
        lv_free((void **)&lu);
        return lv_BACKEND_MEM_ERROR;
    }
    lu->factored = false;

    /* 释放旧数据 */
    if (LS->solver_data) {
        HipDenseLUData *old = (HipDenseLUData *)LS->solver_data;
        if (old->clone) {
            old->clone->ops->destroy(old->clone);
        }
        lv_free((void **)&LS->solver_data);
    }
    LS->solver_data = lu;

    return lv_BACKEND_OK;
}

/**
 * @brief 使用稠密 LU 求解 A * x = b
 */
static int hip_dense_linsol_solve(lvLinearSolver *LS, const lvMatrix *A,
                                   const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);

    HipDenseLUData *lu = (HipDenseLUData *)LS->solver_data;
    if (!lu || !lu->clone) {
        lv_ERROR_SET(lv_BACKEND_NOT_INITIALIZED, "HIP 线性求解器未初始化，请先调用 setup");
        return lv_BACKEND_NOT_INITIALIZED;
    }

    if (lu->clone->rows != A->rows || lu->clone->cols != A->cols) {
        int ret = hip_dense_linsol_setup(LS, A);
        if (ret != lv_BACKEND_OK) return ret;
        lu = (HipDenseLUData *)LS->solver_data;
    }

    if (!lu->factored) {
        lu->clone->ops->copy(lu->clone, A);
        int ret = lu->clone->ops->factor(lu->clone);
        if (ret != lv_BACKEND_OK) return ret;
        lu->factored = true;
    }

    return lu->clone->ops->solve(lu->clone, b, x);
}

/**
 * @brief 销毁稠密 LU 线性求解器
 */
static void hip_dense_linsol_destroy(lvLinearSolver *LS) {
    if (!LS) return;
    if (LS->solver_data) {
        HipDenseLUData *lu = (HipDenseLUData *)LS->solver_data;
        if (lu->clone) {
            lu->clone->ops->destroy(lu->clone);
        }
        lv_free((void **)&LS->solver_data);
    }
    lv_free((void **)&LS);
}

/* ========================================================================
 * 第四部分：迭代法求解器实现（HIP）
 * ======================================================================== */

/**
 * @brief 设置迭代求解器
 */
static int hip_iter_linsol_setup(lvLinearSolver *LS, const lvMatrix *A) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);

    HipIterSolverData *is = lv_calloc(1, sizeof(HipIterSolverData));
    lv_CHECK_ALLOC(is, lv_BACKEND_MEM_ERROR);

    lv_linsol_default_params(&is->max_iters, &is->tol);

    is->clone = hip_matrix_clone(A);
    if (!is->clone) {
        lv_free((void **)&is);
        return lv_BACKEND_MEM_ERROR;
    }

    int64_t n = A->rows;
    hipError_t err;

    err = hipMalloc(&is->d_r, (size_t)n * sizeof(double));
    err |= hipMalloc(&is->d_p, (size_t)n * sizeof(double));
    err |= hipMalloc(&is->d_ap, (size_t)n * sizeof(double));
    err |= hipMalloc(&is->d_work, (size_t)n * sizeof(double));
    err |= hipMalloc(&is->d_r0, (size_t)n * sizeof(double));
    err |= hipMalloc(&is->d_t, (size_t)n * sizeof(double));

    if (err != hipSuccess) {
        if (is->d_r) hipFree(is->d_r);
        if (is->d_p) hipFree(is->d_p);
        if (is->d_ap) hipFree(is->d_ap);
        if (is->d_work) hipFree(is->d_work);
        if (is->d_r0) hipFree(is->d_r0);
        if (is->d_t) hipFree(is->d_t);
        is->clone->ops->destroy(is->clone);
        lv_free((void **)&is);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "HIP 迭代求解器工作区分配失败");
        return lv_BACKEND_MEM_ERROR;
    }

    /* 释放旧数据 */
    if (LS->solver_data) {
        HipIterSolverData *old = (HipIterSolverData *)LS->solver_data;
        if (old->clone) old->clone->ops->destroy(old->clone);
        if (old->d_r) hipFree(old->d_r);
        if (old->d_p) hipFree(old->d_p);
        if (old->d_ap) hipFree(old->d_ap);
        if (old->d_work) hipFree(old->d_work);
        if (old->d_r0) hipFree(old->d_r0);
        if (old->d_t) hipFree(old->d_t);
        lv_free((void **)&LS->solver_data);
    }
    LS->solver_data = is;

    return lv_BACKEND_OK;
}

/**
 * @brief 销毁迭代求解器
 */
static void hip_iter_linsol_destroy(lvLinearSolver *LS) {
    if (!LS) return;
    if (LS->solver_data) {
        HipIterSolverData *is = (HipIterSolverData *)LS->solver_data;
        if (is->clone) is->clone->ops->destroy(is->clone);
        if (is->d_r) hipFree(is->d_r);
        if (is->d_p) hipFree(is->d_p);
        if (is->d_ap) hipFree(is->d_ap);
        if (is->d_work) hipFree(is->d_work);
        if (is->d_r0) hipFree(is->d_r0);
        if (is->d_t) hipFree(is->d_t);
        lv_free((void **)&LS->solver_data);
    }
    lv_free((void **)&LS);
}

/**
 * @brief HIP 辅助函数：在设备上执行 matvec 并使用主机端向量
 *
 * 从 HipVectorData 提取 d_data 并调用 GPU 内核。
 */
static int hip_matvec_host_vectors(const lvMatrix *A, const double *h_x,
                                    double *h_y, int64_t n) {
    if (!A || !A->data) return lv_BACKEND_MEM_ERROR;
    HipMatrixData *md = (HipMatrixData *)A->data;
    if (!md->d_data) return lv_BACKEND_NOT_INITIALIZED;

    double *d_x = NULL, *d_y = NULL;
    hipError_t err;

    err = hipMalloc(&d_x, (size_t)n * sizeof(double));
    err |= hipMalloc(&d_y, (size_t)n * sizeof(double));
    if (err != hipSuccess) {
        if (d_x) hipFree(d_x);
        if (d_y) hipFree(d_y);
        return lv_BACKEND_MEM_ERROR;
    }

    hipMemcpy(d_x, h_x, (size_t)n * sizeof(double), hipMemcpyHostToDevice);
    hipMemset(d_y, 0, (size_t)n * sizeof(double));

    int64_t grid = hip_grid_size(A->rows);
    hipLaunchKernelGGL(matvec_kernel, dim3((int)grid), dim3(HIP_BLOCK_SIZE), 0, 0,
                       md->d_data, d_x, d_y, A->rows, A->cols);

    hipMemcpy(h_y, d_y, (size_t)n * sizeof(double), hipMemcpyDeviceToHost);

    hipFree(d_x);
    hipFree(d_y);
    return lv_BACKEND_OK;
}

/**
 * @brief HIP 辅助函数：计算向量点积（使用 HIP 归约内核）
 */
static double hip_dot_host_vectors(const double *h_x, const double *h_y, int64_t n) {
    double *d_x = NULL, *d_y = NULL, *d_partial = NULL;
    hipError_t err;

    err = hipMalloc(&d_x, (size_t)n * sizeof(double));
    err |= hipMalloc(&d_y, (size_t)n * sizeof(double));
    if (err != hipSuccess) {
        if (d_x) hipFree(d_x);
        if (d_y) hipFree(d_y);
        return 0.0;
    }

    hipMemcpy(d_x, h_x, (size_t)n * sizeof(double), hipMemcpyHostToDevice);
    hipMemcpy(d_y, h_y, (size_t)n * sizeof(double), hipMemcpyHostToDevice);

    int num_blocks = (int)hip_grid_size(n);
    err = hipMalloc(&d_partial, (size_t)num_blocks * sizeof(double));
    if (err != hipSuccess) {
        hipFree(d_x);
        hipFree(d_y);
        return 0.0;
    }

    size_t shared_mem = (size_t)HIP_BLOCK_SIZE * sizeof(double);
    hipLaunchKernelGGL(vector_dot_kernel, dim3(num_blocks), dim3(HIP_BLOCK_SIZE), shared_mem, 0,
                       d_x, d_y, d_partial, n);

    double result = hip_reduce_sum(d_partial, num_blocks);

    hipFree(d_x);
    hipFree(d_y);
    hipFree(d_partial);
    return result;
}

/**
 * @brief HIP 辅助函数：计算向量 L2 范数
 */
static double hip_norm_host_vector(const double *h_v, int64_t n) {
    double *d_v = NULL, *d_partial = NULL;
    hipError_t err;

    err = hipMalloc(&d_v, (size_t)n * sizeof(double));
    if (err != hipSuccess) return 0.0;

    hipMemcpy(d_v, h_v, (size_t)n * sizeof(double), hipMemcpyHostToDevice);

    int num_blocks = (int)hip_grid_size(n);
    err = hipMalloc(&d_partial, (size_t)num_blocks * sizeof(double));
    if (err != hipSuccess) {
        hipFree(d_v);
        return 0.0;
    }

    size_t shared_mem = (size_t)HIP_BLOCK_SIZE * sizeof(double);
    hipLaunchKernelGGL(vector_norm_kernel, dim3(num_blocks), dim3(HIP_BLOCK_SIZE), shared_mem, 0,
                       d_v, d_partial, n);

    double sum_sq = hip_reduce_sum(d_partial, num_blocks);

    hipFree(d_v);
    hipFree(d_partial);
    return sqrt(sum_sq);
}

/**
 * @brief HIP GMRES 求解器
 *
 * 利用 HIP 加速 matvec、点积和范数，Arnoldi 过程与回代在主机端执行。
 * 算法主体委托给共享内核 lv_gmres_solve()（gmres_shared.c），
 * 此处仅提供 HIP 算子表（点积/范数/矩阵向量乘，复用 BiCGSTAB 算子）
 * 并在求解前后回读/写回 GPU 数据。
 *
 * @note 有意的缺陷修复：原实现把 breakdown 阈值硬编码为 1e-14，与 SERIAL/CUDA
 *       的 lv_EPSILON_DOUBLE (1e-12) 不一致；共享内核统一为 lv_EPSILON_DOUBLE，
 *       解更新分量跳过阈值统一为 lv_NUM_EPSILON（详见 gmres_shared.c）。
 */
static int hip_gmres_solve(lvLinearSolver *LS, const lvMatrix *A,
                            const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);

    HipIterSolverData *is = (HipIterSolverData *)LS->solver_data;
    if (!is) {
        int ret = hip_iter_linsol_setup(LS, A);
        if (ret != lv_BACKEND_OK) return ret;
        is = (HipIterSolverData *)LS->solver_data;
    }

    int64_t n = A->rows;

    /* 主机端 b/x 缓冲 */
    double *h_b = (double *)lv_calloc((size_t)n, sizeof(double));
    double *h_x = (double *)lv_calloc((size_t)n, sizeof(double));
    if (!h_b || !h_x) {
        if (h_b) lv_free((void **)&h_b);
        if (h_x) lv_free((void **)&h_x);
        return lv_BACKEND_MEM_ERROR;
    }

    /* 拷贝 b 到主机 */
    {
        HipVectorData *bd = (HipVectorData *)b->backend_data;
        hipMemcpy(h_b, bd->d_data, (size_t)n * sizeof(double), hipMemcpyDeviceToHost);
    }

    lvGmresOps ops;
    ops.ctx = NULL;
    ops.vector_dot = hip_bicgstab_dot;
    ops.vector_norm = hip_bicgstab_norm;
    ops.matvec = hip_bicgstab_matvec;

    int ret = lv_gmres_solve(&ops, A, h_b, h_x, n,
                             is->max_iters, is->tol, lv_EPSILON_DOUBLE, 30);

    if (ret == lv_BACKEND_OK) {
        /* 写回结果 */
        HipVectorData *xd = (HipVectorData *)x->backend_data;
        hipMemcpy(xd->d_data, h_x, (size_t)n * sizeof(double), hipMemcpyHostToDevice);
    }

    lv_free((void **)&h_b);
    lv_free((void **)&h_x);
    return ret;
}

/* ========================================================================
 * BiCGSTAB 共享内核的 HIP 算子（GPU matvec/点积/范数 + 主机端递推）
 * ======================================================================== */

/** @brief HIP 点积（GPU 归约） */
static double hip_bicgstab_dot(void *ctx, const double *a, const double *b, int64_t n) {
    (void) ctx;
    return hip_dot_host_vectors(a, b, n);
}

/** @brief HIP L2 范数（GPU 归约） */
static double hip_bicgstab_norm(void *ctx, const double *v, int64_t n) {
    (void) ctx;
    return hip_norm_host_vector(v, n);
}

/** @brief HIP 矩阵向量乘：y = A*x（GPU 内核） */
static void hip_bicgstab_matvec(void *ctx, const lvMatrix *A, const double *x, double *y, int64_t n) {
    (void) ctx;
    hip_matvec_host_vectors(A, x, y, n);
}

/**
 * @brief HIP BiCGSTAB 求解器
 *
 * 利用 HIP 加速 matvec、点积和范数，递推在主机端执行。
 * 算法主体委托给共享内核 lv_bicgstab_solve()（bicgstab_shared.c），
 * 此处仅提供 HIP 算子表并在求解前后回读/写回 GPU 数据。
 */
static int hip_bicgstab_solve(lvLinearSolver *LS, const lvMatrix *A,
                               const lvVector *b, lvVector *x) {
    lv_CHECK_NULL(LS, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(A, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(b, lv_BACKEND_MEM_ERROR);
    lv_CHECK_NULL(x, lv_BACKEND_MEM_ERROR);

    HipIterSolverData *is = (HipIterSolverData *)LS->solver_data;
    if (!is) {
        int ret = hip_iter_linsol_setup(LS, A);
        if (ret != lv_BACKEND_OK) return ret;
        is = (HipIterSolverData *)LS->solver_data;
    }

    int64_t n = A->rows;

    /* 主机端 b/x 缓冲 */
    double *h_b = (double *)lv_calloc((size_t)n, sizeof(double));
    double *h_x = (double *)lv_calloc((size_t)n, sizeof(double));
    if (!h_b || !h_x) {
        if (h_b) lv_free((void **)&h_b);
        if (h_x) lv_free((void **)&h_x);
        return lv_BACKEND_MEM_ERROR;
    }

    /* 拷贝 b 到主机 */
    {
        HipVectorData *bd = (HipVectorData *)b->backend_data;
        hipMemcpy(h_b, bd->d_data, (size_t)n * sizeof(double), hipMemcpyDeviceToHost);
    }

    lvBicgstabOps ops;
    ops.ctx = NULL;
    ops.vector_dot = hip_bicgstab_dot;
    ops.vector_norm = hip_bicgstab_norm;
    ops.matvec = hip_bicgstab_matvec;

    int ret = lv_bicgstab_solve(&ops, A, h_b, h_x, n,
                                is->max_iters, is->tol, 1e-14);

    if (ret == lv_BACKEND_OK) {
        /* 写回结果 */
        HipVectorData *xd = (HipVectorData *)x->backend_data;
        hipMemcpy(xd->d_data, h_x, (size_t)n * sizeof(double), hipMemcpyHostToDevice);
    }

    lv_free((void **)&h_b);
    lv_free((void **)&h_x);
    return ret;
}

/* ========================================================================
 * 第五部分：HIP 后端 create 函数实现
 * ======================================================================== */

/**
 * @brief 创建 HIP 后端向量
 */
static lvVector *hip_vector_create(int64_t n) {
    if (n <= 0) {
        lv_ERROR_SET(lv_BACKEND_INVALID_ARGS, "向量长度必须为正整数，当前 n=%lld", (long long) n);
        return NULL;
    }

    lvVector *v = lv_calloc(1, sizeof(lvVector));
    lv_CHECK_ALLOC(v, NULL);

    v->length = n;
    v->backend = lv_BACKEND_HIP;
    v->ops = &hip_vector_ops;
    v->data = NULL;

    HipVectorData *vd = lv_calloc(1, sizeof(HipVectorData));
    if (!vd) {
        lv_free((void **) &v);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "HipVectorData 分配失败");
        return NULL;
    }
    vd->length = n;

    hipError_t err = hipMalloc(&vd->d_data, (size_t) n * sizeof(double));
    if (err != hipSuccess) {
        lv_free((void **) &vd);
        lv_free((void **) &v);
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "hipMalloc(vector) 失败");
        return NULL;
    }
    hipMemset(vd->d_data, 0, (size_t) n * sizeof(double));

    v->backend_data = vd;
    return v;
}

/**
 * @brief 创建 HIP 后端稠密矩阵
 */
static lvMatrix *hip_matrix_create(int64_t rows, int64_t cols, bool sparse) {
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
    A->backend = lv_BACKEND_HIP;
    A->ops = &hip_dense_matrix_ops;

    HipMatrixData *md = lv_calloc(1, sizeof(HipMatrixData));
    if (!md) {
        lv_free((void **) &A);
        lv_ERROR_SET(lv_ERROR_OUT_OF_MEMORY, "HipMatrixData 分配失败");
        return NULL;
    }
    md->rows = rows;
    md->cols = cols;

    size_t data_size = (size_t) (rows * cols) * sizeof(double);
    hipError_t err = hipMalloc(&md->d_data, data_size);
    if (err != hipSuccess) {
        lv_free((void **) &md);
        lv_free((void **) &A);
        lv_ERROR_SET(lv_BACKEND_MEM_ERROR, "hipMalloc(matrix) 失败");
        return NULL;
    }
    hipMemset(md->d_data, 0, data_size);

    A->data = md;
    A->backend_data = NULL;
    return A;
}

/**
 * @brief 创建 HIP 后端线性求解器
 */
static lvLinearSolver *hip_linsol_create(lvLinearSolverMethod method) {
    lvLinearSolver *LS = lv_calloc(1, sizeof(lvLinearSolver));
    lv_CHECK_ALLOC(LS, NULL);

    LS->method = method;
    LS->backend = lv_BACKEND_HIP;
    LS->solver_data = NULL;
    LS->backend_data = NULL;

    switch (method) {
        case lv_LINSOL_DIRECT_DENSE:
        case lv_LINSOL_DIRECT_BAND:
        case lv_LINSOL_DIRECT_SPARSE:
            LS->ops = &hip_dense_linsol_ops;
            break;
        case lv_LINSOL_ITERATIVE_GMRES:
            LS->ops = &hip_gmres_linsol_ops;
            break;
        case lv_LINSOL_ITERATIVE_BICGSTAB:
            LS->ops = &hip_bicgstab_linsol_ops;
            break;
        default:
            lv_free((void **) &LS);
            lv_ERROR_SET(lv_BACKEND_UNSUPPORTED, "HIP 后端不支持的求解方法: %s",
                         lv_linsol_method_name(method));
            return NULL;
    }

    return LS;
}

/* ========================================================================
 * 第六部分：公共 API 实现
 * ======================================================================== */

/**
 * @brief 注册 HIP 后端操作表到后端注册表
 *
 * 通过 hipGetDeviceCount 检测 HIP 运行时是否可用。
 * 若可用，则注册所有操作表。
 *
 * @return 成功返回 0，失败返回 -1
 */
int lv_hip_register_backend(void) {
    int device_count = 0;
    hipError_t err = hipGetDeviceCount(&device_count);

    if (err != hipSuccess || device_count <= 0) {
        LOG_WARN("hip_backend", "HIP 后端注册失败：未检测到可用的 AMD GPU 设备");
        return -1;
    }

    lv_numerical_backend_register(lv_BACKEND_HIP,
                                   &hip_vector_ops,
                                   &hip_dense_matrix_ops,
                                   &hip_dense_linsol_ops);

    lv_INFO("HIP 后端注册成功（检测到 %d 个 GPU 设备，版本: %s）",
            device_count, lv_hip_backend_version());
    return 0;
}

/**
 * @brief 检查 HIP 是否可用
 *
 * 通过 hipGetDeviceCount 检测是否有可用的 AMD GPU 设备。
 *
 * @return 可用返回 1，不可用返回 0
 */
int lv_hip_available(void) {
    int device_count = 0;
    hipError_t err = hipGetDeviceCount(&device_count);
    if (err != hipSuccess || device_count <= 0) {
        return 0;
    }
    return 1;
}

/**
 * @brief 获取 HIP 设备数
 *
 * @return HIP 设备数量
 */
int lv_hip_device_count(void) {
    int device_count = 0;
    hipError_t err = hipGetDeviceCount(&device_count);
    if (err != hipSuccess) {
        return 0;
    }
    return device_count;
}

/**
 * @brief 获取 HIP 运行时版本字符串
 *
 * @return 指向静态版本字符串的指针
 */
const char *lv_hip_backend_version(void) {
    static char version_str[128] = {0};
    if (lv_str_is_empty(version_str)) {
        int hip_version = 0;
        hipError_t err = hipRuntimeGetVersion(&hip_version);
        if (err == hipSuccess) {
            int major = hip_version / 10000000;
            int minor = (hip_version % 10000000) / 100000;
            int patch = (hip_version % 100000) / 1000;
            snprintf(version_str, sizeof(version_str),
                     "HIP %d.%d.%d (ROCm)", major, minor, patch);
        } else {
            snprintf(version_str, sizeof(version_str),
                     "HIP %s (ROCm)", HIP_BACKEND_VERSION);
        }
    }
    return version_str;
}

#endif /* LV_HAS_HIP */