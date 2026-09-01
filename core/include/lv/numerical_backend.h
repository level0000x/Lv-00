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
 * @version 1.1.0
 * @date 2026-05-24
 */
#ifndef lv_NUMERICAL_BACKEND_H
#define lv_NUMERICAL_BACKEND_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "exact_arithmetic.h" /* lv_TOLERATED_FLOAT for approximate backend */
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#ifdef __cplusplus
extern "C" {
#endif
/* ==================== 常量定义 ==================== */
/** 后端名称最大长度 */
#ifndef lv_BACKEND_NAME_MAX
#define lv_BACKEND_NAME_MAX 64
#endif
/** 向量/矩阵数据对齐（字节，便于 SIMD） */
#define lv_BACKEND_ALIGNMENT 32
/* ==================== 后端类型枚举 ==================== */
/**
 * @brief 计算后端类型 —— 借鉴 SUNDIALS 多后端设计
 *
 * SUNDIALS 通过编译时宏选择 SERIAL/OpenMP/CUDA/HIP 后端。
 * Lv-00 在运行时通过枚举选择，允许同一可执行文件混用多个后端。
 */
typedef enum {
    lv_BACKEND_SERIAL = 0,   /**< 串行 CPU（默认实现） */
    lv_BACKEND_OPENMP = 1,   /**< OpenMP 多核 CPU 并行 */
    lv_BACKEND_CUDA = 2,     /**< NVIDIA CUDA GPU */
    lv_BACKEND_HIP = 3,      /**< AMD HIP GPU（ROCm 平台） */
    lv_BACKEND_SINGULAR = 4, /**< Singular 计算机代数后端 */
    lv_BACKEND_CUSTOM = 99   /**< 用户自定义后端（使用自定义操作表） */
} lvBackendType;
/* ==================== 误差码 ==================== */
/**
 * @brief 数值后端操作返回码
 */
typedef enum {
    lv_BACKEND_OK = 0,              /**< 成功 */
    lv_BACKEND_MEM_ERROR = -1,      /**< 内存分配失败 */
    lv_BACKEND_UNSUPPORTED = -2,    /**< 不支持的操作 */
    lv_BACKEND_INVALID_ARGS = -3,   /**< 无效参数 */
    lv_BACKEND_LINSOL_FAILED = -4,  /**< 线性求解失败 */
    lv_BACKEND_MATVEC_FAILED = -5,  /**< 矩阵-向量乘法失败 */
    lv_BACKEND_NOT_INITIALIZED = -6 /**< 未初始化 */
} lvBackendError;
/* ==================== 第一部分：lvVector + 操作表 ==================== */
/** @cond 前向声明 */
typedef struct lvVector lvVector;
typedef struct lvVectorOps lvVectorOps;
/** @endcond */
/**
 * @brief lvVector 操作表 —— 借鉴 SUNDIALS N_Vector_Ops
 *
 * SUNDIALS 中每种 N_Vector 实现（serial, openmp, cuda 等）都提供
 * 自己的操作表，包含 clone/destroy/线性代数原语等函数指针。
 * Lv-00 采用同样的设计。
 */
struct lvVectorOps {
    /**
     * @brief 创建新向量（分配内存并初始化）
     * @param[in] n  向量长度
     * @return 新分配的向量，失败返回 NULL
     */
    lvVector *(*create)(int64_t n);
    /**
     * @brief 深拷贝构造一个新的同类型向量
     * @param[in] v  源向量
     * @return 新分配的克隆向量，失败返回 NULL
     */
    lvVector *(*clone)(const lvVector *v);
    /**
     * @brief 销毁向量并释放所有关联资源
     * @param[in,out] v  要销毁的向量
     */
lv_PUBLIC_API     void (*destroy)(lvVector *v);
    /**
     * @brief 将所有元素设为 0
     * @param[in,out] v  向量
     */
lv_PUBLIC_API     void (*zero)(lvVector *v);
    /**
     * @brief 将所有元素设为常量 c
     * @param[in,out] v  向量
     * @param[in]     c  常量值
     */
lv_PUBLIC_API     void (*const_set)(lvVector *v, double c);
    /**
     * @brief 深拷贝：dst = src
     * @param[out] dst  目标向量
     * @param[in]  src  源向量
     */
lv_PUBLIC_API     void (*copy)(lvVector *dst, const lvVector *src);
    /**
     * @brief 标量乘法：v = c * v
     * @param[in,out] v  向量
     * @param[in]     c  标量因子
     */
lv_PUBLIC_API     void (*scale)(lvVector *v, double c);
    /**
     * @brief 线性组合：z = a*x + b*y
     * @param[in]  a  x 的系数
     * @param[in]  x  向量 x
     * @param[in]  b  y 的系数
     * @param[in]  y  向量 y
     * @param[out] z  结果向量
     */
lv_PUBLIC_API     void (*linear_sum)(double a, const lvVector *x, double b, const lvVector *y, lvVector *z);
    /**
     * @brief 点积（内积）：返回 dot(x, y)
     * @param[in] x  向量 x
     * @param[in] y  向量 y
     * @return x 和 y 的点积
     */
lv_PUBLIC_API     double (*dot)(const lvVector *x, const lvVector *y);
    /**
     * @brief L2 范数
     * @param[in] v  向量
     * @return ||v||_2
     */
lv_PUBLIC_API     double (*norm)(const lvVector *v);
    /**
     * @brief 逐元素最大值范数（L-infinity）
     * @param[in] v  向量
     * @return ||v||_inf
     */
lv_PUBLIC_API     double (*max_norm)(const lvVector *v);
    /**
     * @brief 逐元素加权均方根范数，常用于 SUNDIALS/CVODE 误差控制
     * @param[in] v       向量
     * @param[in] weights 权重向量
     * @return 加权 RMS 范数
     */
lv_PUBLIC_API     double (*wrms_norm)(const lvVector *v, const lvVector *weights);
    /**
     * @brief 逐元素绝对值：v_i = |v_i|
     * @param[in,out] v  向量
     */
lv_PUBLIC_API     void (*abs)(lvVector *v);
    /**
     * @brief 逐元素除法：v_i = v_i / d_i
     * @param[in,out] v  被除数向量
     * @param[in]     d  除数向量
     */
lv_PUBLIC_API     void (*inv)(lvVector *v, const lvVector *d);
    /**
     * @brief 逐元素最大值：v_i = max(v_i, c)
     * @param[in,out] v  向量
     * @param[in]     c  比较值
     */
lv_PUBLIC_API     void (*compare)(lvVector *v, double c);
    /**
     * @brief 获取向量长度（元素个数）
     * @param[in] v  向量
     * @return 元素个数
     */
lv_PUBLIC_API     int64_t (*length)(const lvVector *v);
    /**
     * @brief 获取底层原始数据指针（可能为 NULL，取决于后端）
     * @param[in] v  向量
     * @return 数据指针，无直接访问时返回 NULL
     */
lv_PUBLIC_API     double *(*data_ptr)(lvVector *v);
};
/**
 * @brief lvVector 结构体 —— 借鉴 SUNDIALS N_Vector 内容结构
 *
 * 包含长度、后端标识、数据指针（序列后端直接存储）和操作表。
 * 具体后端的向量实现可以通过扩展此结构来附加额外字段。
 */
struct lvVector {
    int64_t length;                   /**< 向量长度（元素个数） */
    lvBackendType backend;            /**< 所属后端类型 */
    double lv_TOLERATED_FLOAT(*data); /**< 数据数组（序列后端直接使用）
                                          * @note lv_TOLERATED_FLOAT:
                                          * 数值后端为近似求解路径，double 可容忍 */
    void *backend_data;               /**< 后端私有不透明数据（GPU 指针等） */
    const lvVectorOps *ops;           /**< 操作表 */
};
/* ==================== 第二部分：lvMatrix + 操作表 ==================== */
/** @cond 前向声明 */
typedef struct lvMatrix lvMatrix;
typedef struct lvMatrixOps lvMatrixOps;
/** @endcond */
/**
 * @brief 矩阵存储格式 —— 借鉴 SUNDIALS SUNMatrix 存储类型
 */
typedef enum {
    lv_MATRIX_DENSE = 0,      /**< 稠密矩阵（列主序） */
    lv_MATRIX_SPARSE_CSR = 1, /**< CSR 格式稀疏矩阵 */
    lv_MATRIX_SPARSE_CSC = 2, /**< CSC 格式稀疏矩阵 */
    lv_MATRIX_BANDED = 3,     /**< 带状矩阵 */
    lv_MATRIX_CUSTOM = 4      /**< 自定义格式 */
} lvMatrixFormat;
/**
 * @brief lvMatrix 操作表 —— 借鉴 SUNDIALS SUNMatrix_Ops
 */
struct lvMatrixOps {
    /**
     * @brief 创建新矩阵（分配内存并初始化）
     * @param[in] rows   行数
     * @param[in] cols   列数
     * @param[in] sparse 是否稀疏矩阵
     * @return 新分配的矩阵，失败返回 NULL
     */
    lvMatrix *(*create)(int64_t rows, int64_t cols, bool sparse);
    /**
     * @brief 深拷贝构造一个新的同类型矩阵
     * @param[in] A  源矩阵
     * @return 新分配的克隆矩阵
     */
    lvMatrix *(*clone)(const lvMatrix *A);
    /**
     * @brief 销毁矩阵并释放所有关联资源
     * @param[in,out] A  要销毁的矩阵
     */
lv_PUBLIC_API     void (*destroy)(lvMatrix *A);
    /**
     * @brief 将所有元素设为 0
     * @param[in,out] A  矩阵
     */
lv_PUBLIC_API     void (*zero)(lvMatrix *A);
    /**
     * @brief 深拷贝：dst = src
     * @param[out] dst  目标矩阵
     * @param[in]  src  源矩阵
     */
lv_PUBLIC_API     void (*copy)(lvMatrix *dst, const lvMatrix *src);
    /**
     * @brief 矩阵-向量乘法：y = A * x
     * @param[in]  A  矩阵
     * @param[in]  x  向量
     * @param[out] y  结果向量
     * @return 成功返回 lv_BACKEND_OK
     */
lv_PUBLIC_API     int (*matvec)(const lvMatrix *A, const lvVector *x, lvVector *y);
    /**
     * @brief 矩阵-标量乘法：A = c * A
     * @param[in,out] A  矩阵
     * @param[in]     c  标量
     */
lv_PUBLIC_API     void (*scale)(lvMatrix *A, double c);
    /**
     * @brief 设置单个元素值
     * @param[in,out] A    矩阵
     * @param[in]     row  行索引
     * @param[in]     col  列索引
     * @param[in]     val  新值
     */
lv_PUBLIC_API     void (*set_element)(lvMatrix *A, int64_t row, int64_t col, double val);
    /**
     * @brief 获取单个元素值
     * @param[in] A    矩阵
     * @param[in] row  行索引
     * @param[in] col  列索引
     * @return 元素值
     */
lv_PUBLIC_API     double (*get_element)(const lvMatrix *A, int64_t row, int64_t col);
    /**
     * @brief LU 分解（若适用）
     * @param[in,out] A  矩阵（输入时未分解，输出时已分解）
     * @return 成功返回 lv_BACKEND_OK
     */
lv_PUBLIC_API     int (*factor)(lvMatrix *A);
    /**
     * @brief 使用已分解矩阵求解 A * x = b
     * @param[in]  A  已分解矩阵（通过 factor() 预处理）
     * @param[in]  b  右端向量
     * @param[out] x  解向量
     * @return 成功返回 lv_BACKEND_OK
     */
lv_PUBLIC_API     int (*solve)(const lvMatrix *A, const lvVector *b, lvVector *x);
};
/**
 * @brief lvMatrix 结构体 —— 借鉴 SUNDIALS SUNMatrix 内容结构
 */
struct lvMatrix {
    int64_t rows;           /**< 行数 */
    int64_t cols;           /**< 列数 */
    bool sparse;            /**< 是否稀疏矩阵 */
    lvMatrixFormat format;  /**< 存储格式 */
    lvBackendType backend;  /**< 所属后端 */
    void *data;             /**< 矩阵数据（稠密时为 double*，CSR 为自定义结构） */
    void *backend_data;     /**< 后端私有不透明数据 */
    const lvMatrixOps *ops; /**< 操作表 */
};
/* ==================== 第三部分：lvLinearSolver + 操作表 ==================== */
/** @cond 前向声明 */
typedef struct lvLinearSolver lvLinearSolver;
typedef struct lvLinearSolverOps lvLinearSolverOps;
/** @endcond */
/**
 * @brief 线性求解方法 —— 借鉴 SUNDIALS SUNLinearSolver 类型
 */
typedef enum {
    lv_LINSOL_DIRECT_DENSE = 0,       /**< 直接法：稠密 LU */
    lv_LINSOL_DIRECT_BAND = 1,        /**< 直接法：带状 LU */
    lv_LINSOL_DIRECT_SPARSE = 2,      /**< 直接法：稀疏 LU */
    lv_LINSOL_ITERATIVE_GMRES = 3,    /**< 迭代法：GMRES */
    lv_LINSOL_ITERATIVE_BICGSTAB = 4, /**< 迭代法：BiCGSTAB */
    lv_LINSOL_ITERATIVE_CG = 5,       /**< 迭代法：共轭梯度 */
    lv_LINSOL_CUSTOM = 99             /**< 自定义求解器 */
} lvLinearSolverMethod;
/**
 * @brief lvLinearSolver 操作表 —— 借鉴 SUNDIALS SUNLinearSolver_Ops
 */
struct lvLinearSolverOps {
    /**
     * @brief 创建新线性求解器
     * @param[in] method 求解方法
     * @return 新分配的求解器，失败返回 NULL
     */
    lvLinearSolver *(*create)(lvLinearSolverMethod method);
    /**
     * @brief 设置线性求解器（初始化/重初始化）
     * @param[in,out] LS  线性求解器
     * @param[in]     A   矩阵（用作求解模板）
     * @return 成功返回 lv_BACKEND_OK
     */
lv_PUBLIC_API     int (*setup)(lvLinearSolver *LS, const lvMatrix *A);
    /**
     * @brief 求解线性系统 A * x = b
     * @param[in]  LS  线性求解器（已通过 setup() 初始化）
     * @param[in]  A   系数矩阵
     * @param[in]  b   右端向量
     * @param[out] x   解向量
     * @return 成功返回 lv_BACKEND_OK
     */
lv_PUBLIC_API     int (*solve)(lvLinearSolver *LS, const lvMatrix *A, const lvVector *b, lvVector *x);
    /**
     * @brief 销毁线性求解器并释放所有关联资源
     * @param[in,out] LS  要销毁的求解器
     */
lv_PUBLIC_API     void (*destroy)(lvLinearSolver *LS);
};
/**
 * @brief lvLinearSolver 结构体 —— 借鉴 SUNDIALS SUNLinearSolver 内容结构
 */
struct lvLinearSolver {
    lvLinearSolverMethod method;  /**< 求解方法 */
    lvBackendType backend;        /**< 所属后端 */
    void *solver_data;            /**< 求解器私有数据 */
    void *backend_data;           /**< 后端私有不透明数据 */
    const lvLinearSolverOps *ops; /**< 操作表 */
};
/* ==================== 第四部分：工厂函数和注册机制 ==================== */
/**
 * @brief 注册数值后端（向量/矩阵/求解器操作集）
 *
 * 在初始化时调用，将后端名称与其操作集关联。
 * 之后 lv_vector_create/lv_matrix_create/lv_linsol_create
 * 将通过查表找到对应的操作集，无需 #ifdef 编译时判断。
 *
 * @param backend_type 后端类型枚举值
 * @param vector_ops   向量操作集指针
 * @param matrix_ops   矩阵操作集指针
 * @param linsol_ops   线性求解器操作集指针
 */
void lv_numerical_backend_register(lvBackendType backend_type,
                                   const lvVectorOps *vector_ops,
                                   const lvMatrixOps *matrix_ops,
                                   const lvLinearSolverOps *linsol_ops);
/**
 * @brief 创建向量（指定后端和长度）
 *
 * @param[in] backend  后端类型
 * @param[in] n        向量长度（元素个数）
 * @return 新分配的向量，失败返回 NULL
 *
 * @note 具体实现在各后端的 .c 文件中，此处仅为接口声明。
 */
lvVector *lv_vector_create(lvBackendType backend, int64_t n);
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
lvMatrix *lv_matrix_create(lvBackendType backend, int64_t rows, int64_t cols, bool sparse);
/**
 * @brief 创建线性求解器（指定后端和求解方法）
 *
 * @param[in] backend  后端类型
 * @param[in] method   求解方法
 * @return 新分配的线性求解器，失败返回 NULL
 *
 * @note 具体实现在各后端的 .c 文件中，此处仅为接口声明。
 */
lvLinearSolver *lv_linsol_create(lvBackendType backend, lvLinearSolverMethod method);
/**
 * @brief 获取后端名称字符串
 *
 * @param[in] backend  后端类型
 * @return 后端名称（如 "SERIAL"、"CUDA" 等）
 */
static inline const char *lv_backend_name(lvBackendType backend) {
    switch (backend) {
        case lv_BACKEND_SERIAL:
            return "SERIAL";
        case lv_BACKEND_OPENMP:
            return "OpenMP";
        case lv_BACKEND_CUDA:
            return "CUDA";
        case lv_BACKEND_HIP:
            return "HIP";
        case lv_BACKEND_SINGULAR:
            return "Singular";
        case lv_BACKEND_CUSTOM:
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
static inline const char *lv_linsol_method_name(lvLinearSolverMethod method) {
    switch (method) {
        case lv_LINSOL_DIRECT_DENSE:
            return "Direct-Dense-LU";
        case lv_LINSOL_DIRECT_BAND:
            return "Direct-Band-LU";
        case lv_LINSOL_DIRECT_SPARSE:
            return "Direct-Sparse-LU";
        case lv_LINSOL_ITERATIVE_GMRES:
            return "Iterative-GMRES";
        case lv_LINSOL_ITERATIVE_BICGSTAB:
            return "Iterative-BiCGSTAB";
        case lv_LINSOL_ITERATIVE_CG:
            return "Iterative-CG";
        case lv_LINSOL_CUSTOM:
            return "Custom";
        default:
            return "Unknown";
    }
}
#ifdef __cplusplus
}
#endif
#endif /* lv_NUMERICAL_BACKEND_H */
