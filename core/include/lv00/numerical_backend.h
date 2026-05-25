/**
 * @file numerical_backend.h
 * @brief 多后端数值抽象层 —— 借鉴 SUNDIALS N_Vector/SUNMatrix/SUNLinearSolver 架构
 *
 * @details 设计借鉴来源：
 *          - SUNDIALS (github.com/LLNL/sundials) — 大规模非线性/微分代数方程求解器
 *            · N_Vector 三层抽象（内容向量 + 运算表 + 具体实现）
 *            · SUNMatrix 稀疏/稠密统一接口
 *            · SUNLinearSolver 线性求解器抽象
 *            · 多后端支持（SERIAL/OpenMP/CUDA/HIP）
 *
 *          设计目标：
 *          - 纯 C 头文件定义（结构体和枚举完整内联）
 *          - 函数指针操作表模式（允许编译时/运行时切换后端）
 *          - 工厂函数接口声明（具体实现在各后端 .c 文件中）
 *          - 支持 SERIAL、OpenMP、CUDA、HIP 和自定义后端
 *
 * @author Lv-00 Project
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_NUMERICAL_BACKEND_H
#define LV00_NUMERICAL_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "exact_arithmetic.h" /* LV00_TOLERATED_FLOAT for approximate backend */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/** 后端名称最大长度 */
#define LV00_BACKEND_NAME_MAX 64

/** 向量/矩阵数据对齐（字节，便于 SIMD） */
#define LV00_BACKEND_ALIGNMENT 32

/* ==================== 后端类型枚举 ==================== */

/**
 * @brief 计算后端类型 —— 借鉴 SUNDIALS 多后端设计
 *
 * SUNDIALS 通过编译时宏选择 SERIAL/OpenMP/CUDA/HIP 后端。
 * Lv-00 在运行时通过枚举选择，允许同一可执行文件混用多个后端。
 */
typedef enum {
    LV00_BACKEND_SERIAL = 0, /**< 串行 CPU（默认实现） */
    LV00_BACKEND_OPENMP = 1, /**< OpenMP 多核 CPU 并行 */
    LV00_BACKEND_CUDA = 2,   /**< NVIDIA CUDA GPU */
    LV00_BACKEND_HIP = 3,    /**< AMD HIP GPU（ROCm 平台） */
    LV00_BACKEND_CUSTOM = 99 /**< 用户自定义后端（使用自定义操作表） */
} Lv00BackendType;

/* ==================== 误差码 ==================== */

/**
 * @brief 数值后端操作返回码
 */
typedef enum {
    LV00_BACKEND_OK = 0,              /**< 成功 */
    LV00_BACKEND_MEM_ERROR = -1,      /**< 内存分配失败 */
    LV00_BACKEND_UNSUPPORTED = -2,    /**< 不支持的操作 */
    LV00_BACKEND_INVALID_ARGS = -3,   /**< 无效参数 */
    LV00_BACKEND_LINSOL_FAILED = -4,  /**< 线性求解失败 */
    LV00_BACKEND_MATVEC_FAILED = -5,  /**< 矩阵-向量乘法失败 */
    LV00_BACKEND_NOT_INITIALIZED = -6 /**< 未初始化 */
} Lv00BackendError;

/* ==================== 第一部分：Lv00Vector + 操作表 ==================== */

/** @cond 前向声明 */
typedef struct Lv00Vector Lv00Vector;
typedef struct Lv00VectorOps Lv00VectorOps;
/** @endcond */

/**
 * @brief Lv00Vector 操作表 —— 借鉴 SUNDIALS N_Vector_Ops
 *
 * SUNDIALS 中每种 N_Vector 实现（serial, openmp, cuda 等）都提供
 * 自己的操作表，包含 clone/destroy/线性代数原语等函数指针。
 * Lv-00 采用同样的设计。
 */
struct Lv00VectorOps {
    /**
     * @brief 深拷贝构造一个新的同类型向量
     * @param[in] v  源向量
     * @return 新分配的克隆向量，失败返回 NULL
     */
    Lv00Vector *(*clone)(const Lv00Vector *v);

    /**
     * @brief 销毁向量并释放所有关联资源
     * @param[in,out] v  要销毁的向量
     */
    void (*destroy)(Lv00Vector *v);

    /**
     * @brief 将所有元素设为 0
     * @param[in,out] v  向量
     */
    void (*zero)(Lv00Vector *v);

    /**
     * @brief 将所有元素设为常量 c
     * @param[in,out] v  向量
     * @param[in]     c  常量值
     */
    void (*const_set)(Lv00Vector *v, double c);

    /**
     * @brief 深拷贝：dst = src
     * @param[out] dst  目标向量
     * @param[in]  src  源向量
     */
    void (*copy)(Lv00Vector *dst, const Lv00Vector *src);

    /**
     * @brief 标量乘法：v = c * v
     * @param[in,out] v  向量
     * @param[in]     c  标量因子
     */
    void (*scale)(Lv00Vector *v, double c);

    /**
     * @brief 线性组合：z = a*x + b*y
     * @param[in]  a  x 的系数
     * @param[in]  x  向量 x
     * @param[in]  b  y 的系数
     * @param[in]  y  向量 y
     * @param[out] z  结果向量
     */
    void (*linear_sum)(double a, const Lv00Vector *x, double b, const Lv00Vector *y, Lv00Vector *z);

    /**
     * @brief 点积（内积）：返回 dot(x, y)
     * @param[in] x  向量 x
     * @param[in] y  向量 y
     * @return x 和 y 的点积
     */
    double (*dot)(const Lv00Vector *x, const Lv00Vector *y);

    /**
     * @brief L2 范数
     * @param[in] v  向量
     * @return ||v||_2
     */
    double (*norm)(const Lv00Vector *v);

    /**
     * @brief 逐元素最大值范数（L-infinity）
     * @param[in] v  向量
     * @return ||v||_inf
     */
    double (*max_norm)(const Lv00Vector *v);

    /**
     * @brief 逐元素加权均方根范数，常用于 SUNDIALS/CVODE 误差控制
     * @param[in] v       向量
     * @param[in] weights 权重向量
     * @return 加权 RMS 范数
     */
    double (*wrms_norm)(const Lv00Vector *v, const Lv00Vector *weights);

    /**
     * @brief 逐元素绝对值：v_i = |v_i|
     * @param[in,out] v  向量
     */
    void (*abs)(Lv00Vector *v);

    /**
     * @brief 逐元素除法：v_i = v_i / d_i
     * @param[in,out] v  被除数向量
     * @param[in]     d  除数向量
     */
    void (*inv)(Lv00Vector *v, const Lv00Vector *d);

    /**
     * @brief 逐元素最大值：v_i = max(v_i, c)
     * @param[in,out] v  向量
     * @param[in]     c  比较值
     */
    void (*compare)(Lv00Vector *v, double c);

    /**
     * @brief 获取向量长度（元素个数）
     * @param[in] v  向量
     * @return 元素个数
     */
    int64_t (*length)(const Lv00Vector *v);

    /**
     * @brief 获取底层原始数据指针（可能为 NULL，取决于后端）
     * @param[in] v  向量
     * @return 数据指针，无直接访问时返回 NULL
     */
    double *(*data_ptr)(Lv00Vector *v);
};

/**
 * @brief Lv00Vector 结构体 —— 借鉴 SUNDIALS N_Vector 内容结构
 *
 * 包含长度、后端标识、数据指针（序列后端直接存储）和操作表。
 * 具体后端的向量实现可以通过扩展此结构来附加额外字段。
 */
struct Lv00Vector {
    int64_t length;           /**< 向量长度（元素个数） */
    Lv00BackendType backend;  /**< 所属后端类型 */
    double LV00_TOLERATED_FLOAT(*data);  /**< 数据数组（序列后端直接使用）
                                          * @note LV00_TOLERATED_FLOAT:
                                          * 数值后端为近似求解路径，double 可容忍 */
    void *backend_data;       /**< 后端私有不透明数据（GPU 指针等） */
    const Lv00VectorOps *ops; /**< 操作表 */
};

/* ==================== 第二部分：Lv00Matrix + 操作表 ==================== */

/** @cond 前向声明 */
typedef struct Lv00Matrix Lv00Matrix;
typedef struct Lv00MatrixOps Lv00MatrixOps;
/** @endcond */

/**
 * @brief 矩阵存储格式 —— 借鉴 SUNDIALS SUNMatrix 存储类型
 */
typedef enum {
    LV00_MATRIX_DENSE = 0,      /**< 稠密矩阵（列主序） */
    LV00_MATRIX_SPARSE_CSR = 1, /**< CSR 格式稀疏矩阵 */
    LV00_MATRIX_SPARSE_CSC = 2, /**< CSC 格式稀疏矩阵 */
    LV00_MATRIX_BANDED = 3,     /**< 带状矩阵 */
    LV00_MATRIX_CUSTOM = 4      /**< 自定义格式 */
} Lv00MatrixFormat;

/**
 * @brief Lv00Matrix 操作表 —— 借鉴 SUNDIALS SUNMatrix_Ops
 */
struct Lv00MatrixOps {
    /**
     * @brief 深拷贝构造一个新的同类型矩阵
     * @param[in] A  源矩阵
     * @return 新分配的克隆矩阵
     */
    Lv00Matrix *(*clone)(const Lv00Matrix *A);

    /**
     * @brief 销毁矩阵并释放所有关联资源
     * @param[in,out] A  要销毁的矩阵
     */
    void (*destroy)(Lv00Matrix *A);

    /**
     * @brief 将所有元素设为 0
     * @param[in,out] A  矩阵
     */
    void (*zero)(Lv00Matrix *A);

    /**
     * @brief 深拷贝：dst = src
     * @param[out] dst  目标矩阵
     * @param[in]  src  源矩阵
     */
    void (*copy)(Lv00Matrix *dst, const Lv00Matrix *src);

    /**
     * @brief 矩阵-向量乘法：y = A * x
     * @param[in]  A  矩阵
     * @param[in]  x  向量
     * @param[out] y  结果向量
     * @return 成功返回 LV00_BACKEND_OK
     */
    int (*matvec)(const Lv00Matrix *A, const Lv00Vector *x, Lv00Vector *y);

    /**
     * @brief 矩阵-标量乘法：A = c * A
     * @param[in,out] A  矩阵
     * @param[in]     c  标量
     */
    void (*scale)(Lv00Matrix *A, double c);

    /**
     * @brief 设置单个元素值
     * @param[in,out] A    矩阵
     * @param[in]     row  行索引
     * @param[in]     col  列索引
     * @param[in]     val  新值
     */
    void (*set_element)(Lv00Matrix *A, int64_t row, int64_t col, double val);

    /**
     * @brief 获取单个元素值
     * @param[in] A    矩阵
     * @param[in] row  行索引
     * @param[in] col  列索引
     * @return 元素值
     */
    double (*get_element)(const Lv00Matrix *A, int64_t row, int64_t col);

    /**
     * @brief LU 分解（若适用）
     * @param[in,out] A  矩阵（输入时未分解，输出时已分解）
     * @return 成功返回 LV00_BACKEND_OK
     */
    int (*factor)(Lv00Matrix *A);

    /**
     * @brief 使用已分解矩阵求解 A * x = b
     * @param[in]  A  已分解矩阵（通过 factor() 预处理）
     * @param[in]  b  右端向量
     * @param[out] x  解向量
     * @return 成功返回 LV00_BACKEND_OK
     */
    int (*solve)(const Lv00Matrix *A, const Lv00Vector *b, Lv00Vector *x);
};

/**
 * @brief Lv00Matrix 结构体 —— 借鉴 SUNDIALS SUNMatrix 内容结构
 */
struct Lv00Matrix {
    int64_t rows;             /**< 行数 */
    int64_t cols;             /**< 列数 */
    bool sparse;              /**< 是否稀疏矩阵 */
    Lv00MatrixFormat format;  /**< 存储格式 */
    Lv00BackendType backend;  /**< 所属后端 */
    void *data;               /**< 矩阵数据（稠密时为 double*，CSR 为自定义结构） */
    void *backend_data;       /**< 后端私有不透明数据 */
    const Lv00MatrixOps *ops; /**< 操作表 */
};

/* ==================== 第三部分：Lv00LinearSolver + 操作表 ==================== */

/** @cond 前向声明 */
typedef struct Lv00LinearSolver Lv00LinearSolver;
typedef struct Lv00LinearSolverOps Lv00LinearSolverOps;
/** @endcond */

/**
 * @brief 线性求解方法 —— 借鉴 SUNDIALS SUNLinearSolver 类型
 */
typedef enum {
    LV00_LINSOL_DIRECT_DENSE = 0,       /**< 直接法：稠密 LU */
    LV00_LINSOL_DIRECT_BAND = 1,        /**< 直接法：带状 LU */
    LV00_LINSOL_DIRECT_SPARSE = 2,      /**< 直接法：稀疏 LU */
    LV00_LINSOL_ITERATIVE_GMRES = 3,    /**< 迭代法：GMRES */
    LV00_LINSOL_ITERATIVE_BICGSTAB = 4, /**< 迭代法：BiCGSTAB */
    LV00_LINSOL_ITERATIVE_CG = 5,       /**< 迭代法：共轭梯度 */
    LV00_LINSOL_CUSTOM = 99             /**< 自定义求解器 */
} Lv00LinearSolverMethod;

/**
 * @brief Lv00LinearSolver 操作表 —— 借鉴 SUNDIALS SUNLinearSolver_Ops
 */
struct Lv00LinearSolverOps {
    /**
     * @brief 设置线性求解器（初始化/重初始化）
     * @param[in,out] LS  线性求解器
     * @param[in]     A   矩阵（用作求解模板）
     * @return 成功返回 LV00_BACKEND_OK
     */
    int (*setup)(Lv00LinearSolver *LS, const Lv00Matrix *A);

    /**
     * @brief 求解线性系统 A * x = b
     * @param[in]  LS  线性求解器（已通过 setup() 初始化）
     * @param[in]  A   系数矩阵
     * @param[in]  b   右端向量
     * @param[out] x   解向量
     * @return 成功返回 LV00_BACKEND_OK
     */
    int (*solve)(Lv00LinearSolver *LS, const Lv00Matrix *A, const Lv00Vector *b, Lv00Vector *x);

    /**
     * @brief 销毁线性求解器并释放所有关联资源
     * @param[in,out] LS  要销毁的求解器
     */
    void (*destroy)(Lv00LinearSolver *LS);
};

/**
 * @brief Lv00LinearSolver 结构体 —— 借鉴 SUNDIALS SUNLinearSolver 内容结构
 */
struct Lv00LinearSolver {
    Lv00LinearSolverMethod method;  /**< 求解方法 */
    Lv00BackendType backend;        /**< 所属后端 */
    void *solver_data;              /**< 求解器私有数据 */
    void *backend_data;             /**< 后端私有不透明数据 */
    const Lv00LinearSolverOps *ops; /**< 操作表 */
};

/* ==================== 第四部分：工厂函数 ==================== */

/**
 * @brief 创建向量（指定后端和长度）
 *
 * @param[in] backend  后端类型
 * @param[in] n        向量长度（元素个数）
 * @return 新分配的向量，失败返回 NULL
 *
 * @note 具体实现在各后端的 .c 文件中，此处仅为接口声明。
 */
Lv00Vector *lv00_vector_create(Lv00BackendType backend, int64_t n);

/**
 * @brief 创建矩阵（指定后端、维度和格式）
 *
 * @param[in] backend  后端类型
 * @param[in] rows     行数
 * @param[in] cols     列数
 * @param[in] sparse   是否稀疏矩阵（true 时使用 CSR 格式）
 * @return 新分配的矩阵，失败返回 NULL
 *
 * @note 具体实现在各后端的 .c 文件中，此处仅为接口声明。
 */
Lv00Matrix *lv00_matrix_create(Lv00BackendType backend, int64_t rows, int64_t cols, bool sparse);

/**
 * @brief 创建线性求解器（指定后端和求解方法）
 *
 * @param[in] backend  后端类型
 * @param[in] method   求解方法
 * @return 新分配的线性求解器，失败返回 NULL
 *
 * @note 具体实现在各后端的 .c 文件中，此处仅为接口声明。
 */
Lv00LinearSolver *lv00_linsol_create(Lv00BackendType backend, Lv00LinearSolverMethod method);

/**
 * @brief 获取后端名称字符串
 *
 * @param[in] backend  后端类型
 * @return 后端名称（如 "SERIAL"、"CUDA" 等）
 */
static inline const char *lv00_backend_name(Lv00BackendType backend) {
    switch (backend) {
        case LV00_BACKEND_SERIAL:
            return "SERIAL";
        case LV00_BACKEND_OPENMP:
            return "OpenMP";
        case LV00_BACKEND_CUDA:
            return "CUDA";
        case LV00_BACKEND_HIP:
            return "HIP";
        case LV00_BACKEND_CUSTOM:
            return "CUSTOM";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 获取线性求解方法名称字符串
 *
 * @param[in] method  求解方法
 * @return 方法名称
 */
static inline const char *lv00_linsol_method_name(Lv00LinearSolverMethod method) {
    switch (method) {
        case LV00_LINSOL_DIRECT_DENSE:
            return "Direct-Dense-LU";
        case LV00_LINSOL_DIRECT_BAND:
            return "Direct-Band-LU";
        case LV00_LINSOL_DIRECT_SPARSE:
            return "Direct-Sparse-LU";
        case LV00_LINSOL_ITERATIVE_GMRES:
            return "Iterative-GMRES";
        case LV00_LINSOL_ITERATIVE_BICGSTAB:
            return "Iterative-BiCGSTAB";
        case LV00_LINSOL_ITERATIVE_CG:
            return "Iterative-CG";
        case LV00_LINSOL_CUSTOM:
            return "Custom";
        default:
            return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════
 * 使用示例（参考）
 * ═══════════════════════════════════════════════════════════════
 *
 * @code
 * // 创建一个串行 100 维向量
 * Lv00Vector *x = lv00_vector_create(LV00_BACKEND_SERIAL, 100);
 * x->ops->const_set(x, 1.0);
 *
 * // 创建一个串行 100x100 稠密矩阵
 * Lv00Matrix *A = lv00_matrix_create(LV00_BACKEND_SERIAL, 100, 100, false);
 *
 * // 矩阵-向量乘法
 * Lv00Vector *y = lv00_vector_create(LV00_BACKEND_SERIAL, 100);
 * A->ops->matvec(A, x, y);
 *
 * // 创建并求解线性系统 A * sol = y
 * Lv00LinearSolver *LS = lv00_linsol_create(LV00_BACKEND_SERIAL,
 *                                            LV00_LINSOL_DIRECT_DENSE);
 * LS->ops->setup(LS, A);
 * Lv00Vector *sol = lv00_vector_create(LV00_BACKEND_SERIAL, 100);
 * int ret = LS->ops->solve(LS, A, y, sol);
 * if (ret == LV00_BACKEND_OK) {
 *     printf("Solver: %s  |  Backend: %s\n",
 *            lv00_linsol_method_name(LS->method),
 *            lv00_backend_name(x->backend));
 * }
 *
 * // 清理
 * LS->ops->destroy(LS);
 * A->ops->destroy(A);
 * x->ops->destroy(x);
 * y->ops->destroy(y);
 * sol->ops->destroy(sol);
 * @endcode
 */

#ifdef __cplusplus
}
#endif

#endif /* LV00_NUMERICAL_BACKEND_H */
