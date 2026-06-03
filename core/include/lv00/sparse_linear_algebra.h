/**
 * @file sparse_linear_algebra.h
 * @brief 稀疏线性代数后端 —— SuiteSparse/GraphBLAS 风格的半环矩阵运算与约束传播
 *
 * 借鉴 SuiteSparse:GraphBLAS (v9.x) 的稀疏矩阵代数框架，
 * 将 Lv-00 约束图的传播求解建模为半环上的稀疏矩阵乘法。
 * 提供 CSR/CSC/COO 稀疏格式、GraphBLAS 半环抽象、
 * 约束传播不动点迭代、以及稀疏线性求解器接口
 * （CHOLMOD/UMFPACK 风格）。
 *
 * 设计借鉴：
 * - SuiteSparse/GraphBLAS (github.com/DrTimothyAldenDavis/GraphBLAS)
 *   — 基于半环的稀疏矩阵代数 C API 规范
 * - SuiteSparse/CHOLMOD (github.com/DrTimothyAldenDavis/SuiteSparse)
 *   — 稀疏 Cholesky/LU 分解，高性能稀疏直接求解
 * - SuiteSparse/UMFPACK — 稀疏非对称 LU 分解
 *
 * 核心思想：
 *   约束传播 = 邻接矩阵在半环 (V, ⊕, ⊗) 上的迭代乘法
 *   其中 V 是约束值域（布尔/实数/区间），⊕ 是值合并，⊗ 是值组合。
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_SPARSE_LINEAR_ALGEBRA_H
#define LV00_SPARSE_LINEAR_ALGEBRA_H

#include "lv00.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明：约束图（定义在 constraint_graph.h） */
typedef struct ConstraintGraph ConstraintGraph;

/* ========================================================================
 * 稀疏矩阵存储格式
 * ======================================================================== */

/**
 * @brief 稀疏矩阵存储格式枚举
 *
 * 支持四种标准稀疏存储格式，与 SuiteSparse 系列库兼容。
 */
typedef enum {
    SPARSE_CSR = 0,  /**< 压缩稀疏行（Compressed Sparse Row）— 适合行遍历 */
    SPARSE_CSC = 1,  /**< 压缩稀疏列（Compressed Sparse Column）— 适合列遍历 */
    SPARSE_COO = 2,  /**< 坐标格式（Coordinate）— 适合增量构建 */
    SPARSE_DENSE = 3 /**< 稠密格式 — 退化情况，小矩阵 fallback */
} SparseFormat;

/* ========================================================================
 * 稀疏矩阵结构体
 * ======================================================================== */

/**
 * @brief 稀疏矩阵（CSR/CSC/COO 统一表示）
 *
 * 对于 CSR 格式：
 *   - row_ptr[i] 到 row_ptr[i+1]-1 是第 i 行的非零元素区间
 *   - col_idx[k] 是该区间内第 k 个非零元素的列索引
 *   - values[k] 是对应的数值
 *
 * 对于 CSC 格式，row_ptr 变为 col_ptr，col_idx 变为 row_idx。
 *
 * owns_data 标志表示该结构体是否拥有底层数组的所有权，
 * 决定 destroy 时是否释放内存。
 */
typedef struct {
    int rows;         /**< 矩阵行数 */
    int cols;         /**< 矩阵列数 */
    int nnz;          /**< 非零元素数量 */
    SparseFormat fmt; /**< 存储格式 */
    int *row_ptr;     /**< 行指针数组（CSR）/ 列指针数组（CSC），长度 rows+1 */
    int *col_idx;     /**< 列索引数组（CSR）/ 行索引数组（CSC），长度 nnz */
    double *values;   /**< 非零数值数组，长度 nnz。为 NULL 时表示结构矩阵 */
    bool owns_data;   /**< 是否拥有底层数组的所有权 */
} SparseMatrix;

/* ========================================================================
 * 稀疏矩阵生命周期
 * ======================================================================== */

/**
 * @brief 创建指定大小的稀疏矩阵（未分配内部数组）
 *
 * 调用者随后应自行填充 row_ptr / col_idx / values 数组。
 * 若传递 nnz = 0，则内部数组保留为 NULL。
 *
 * @param[in] rows 行数
 * @param[in] cols 列数
 * @param[in] fmt  存储格式
 * @return 新分配的稀疏矩阵（调用者需用 sparse_matrix_destroy 释放），失败返回 NULL
 */
SparseMatrix *sparse_matrix_create(int rows, int cols, SparseFormat fmt);

/**
 * @brief 销毁稀疏矩阵并释放所有内部数组
 *
 * 仅当 owns_data 为 true 时才释放 row_ptr / col_idx / values 数组。
 *
 * @param[in,out] mat 要销毁的稀疏矩阵（可为 NULL，此时无操作）
 */
void sparse_matrix_destroy(SparseMatrix *mat);

/**
 * @brief 深拷贝稀疏矩阵
 *
 * 创建完全独立的副本，包括所有内部数组的深拷贝。
 * 副本的 owns_data 始终为 true。
 *
 * @param[in] mat 源矩阵
 * @return 新分配的副本，失败返回 NULL
 */
SparseMatrix *sparse_matrix_clone(const SparseMatrix *mat);

/**
 * @brief 打印稀疏矩阵的结构和数值（调试用）
 *
 * 输出行数、列数、非零元数量、格式，以及 CSR/CSC 格式的
 * 行指针和列索引信息。若矩阵较大（nnz > 100），仅打印前 10 行。
 *
 * @param[in] mat  稀疏矩阵
 * @param[in] name 矩阵名称标签（可为 NULL）
 */
void sparse_matrix_print(const SparseMatrix *mat, const char *name);

/* ========================================================================
 * 半环抽象（GraphBLAS Semiring）
 *
 * GraphBLAS 的核心抽象：半环 (V, ⊕, ⊗)
 *   - V: 值域（布尔/整数/实数/区间）
 *   - ⊕: 加法运算（合并多条路径的值）
 *   - ⊗: 乘法运算（沿路径组合值）
 *
 * 约束传播的矩阵模型：
 *   设 A 为约束图的邻接矩阵，x 为节点约束值向量。
 *   一次传播步 = A ⊗ x（半环矩阵-向量乘法）。
 *   不动点 = 重复传播直到 x 不再变化。
 * ======================================================================== */

/**
 * @brief 半环类型枚举
 *
 * 每种类型对应一组预定义的 add_op / mul_op 函数指针和单位元。
 */
typedef enum {
    SEMIRING_PLUS_TIMES = 0, /**< (R, +, ×) — 经典实数半环，add_id=0, mul_id=1 */
    SEMIRING_MIN_PLUS = 1,   /**< (R∪{∞}, min, +) — 最短路径/热带半环，add_id=∞, mul_id=0 */
    SEMIRING_MAX_TIMES = 2,  /**< (R, max, ×) — 最大可信度传播 */
    SEMIRING_OR_AND = 3,     /**< ({0,1}, ∨, ∧) — 布尔半环，可达性分析 */
    SEMIRING_BOOL = 4,       /**< ({0,1}, ∨, ∧) — 别名，同 SEMIRING_OR_AND */
    SEMIRING_INTERVAL = 5    /**< (I, ∩, ⊕_interval) — 区间约束传播 */
} SemiringType;

/**
 * @brief 半环函数指针类型定义
 *
 * add_op(a, b) 对应半环加法 ⊕，
 * mul_op(a, b) 对应半环乘法 ⊗。
 */
typedef double (*semiring_add_fn)(double a, double b);
typedef double (*semiring_multiply_fn)(double a, double b);

/**
 * @brief 半环结构体
 *
 * 封装一个 GraphBLAS 半环的完整定义：加法运算、乘法运算、
 * 加法单位元（零元）、乘法单位元（壹元）。
 *
 * 用户通常不需要直接构造该结构体，而是通过
 * semiring_create(SemiringType) 获取预定义的半环实例。
 */
typedef struct {
    semiring_add_fn add_op;      /**< 半环加法 ⊕(a, b)，如 min / max / + */
    semiring_multiply_fn mul_op; /**< 半环乘法 ⊗(a, b)，如 + / × / ∧ */
    double add_identity;         /**< 加法单位元（零元），如 0 / ∞ / false */
    double mul_identity;         /**< 乘法单位元（壹元），如 1 / 0 / true */
    SemiringType type;           /**< 半环类型标识 */
    const char *name;            /**< 半环名称（如 "min-plus"），调试用 */
} Semiring;

/**
 * @brief 根据类型创建预定义的半环实例
 *
 * 返回的 Semiring 中 add_op 和 mul_op 已设置为对应类型的函数指针。
 * 调用者无需释放 Semiring 本身（无动态分配），但应注意函数指针
 * 指向的是静态函数，其生命周期与程序相同。
 *
 * @param[in] type 半环类型
 * @return 初始化的半环结构体（值语义，可直接复制）
 */
Semiring semiring_create(SemiringType type);

/* ========================================================================
 * 约束传播操作（半环矩阵乘法建模）
 * ======================================================================== */

/**
 * @brief 使用半环对约束图执行约束传播
 *
 * 将约束图上的约束传播建模为半环矩阵乘法的不动点迭代：
 *   1. 从约束图构建稀疏邻接矩阵 A（CSR 格式）
 *   2. 初始化节点约束值向量 x（根据节点初始约束）
 *   3. 迭代计算 x_{k+1} = A ⊗ x_k（半环矩阵-向量乘法）
 *   4. 直到 x_{k+1} == x_k（达到不动点）或超过最大迭代次数
 *
 * 典型应用：
 *   - SEMIRING_MIN_PLUS：计算约束图中的最短推导路径
 *   - SEMIRING_OR_AND：计算约束的可达性和传递闭包
 *   - SEMIRING_PLUS_TIMES：线性约束系统的迭代求解
 *   - SEMIRING_INTERVAL：区间约束传播（如包围盒传播）
 *
 * @param[in]     g        约束图
 * @param[in]     semiring 半环类型
 * @param[in,out] x        节点值数组（长度为 node_count），输入初值，输出不动点
 * @param[in]     max_iter 最大迭代次数（0 = 自动，默认 1000）
 * @return 实际迭代次数，-1 表示未收敛（超过最大迭代次数）
 *
 * @note 该函数内部构建稀疏矩阵，适合中小规模图。
 *       大规模图建议使用 graph_to_constraint_matrix() 显式构建
 *       并复用矩阵进行多次传播。
 */
int semiring_propagate_constraints(ConstraintGraph *g, SemiringType semiring, double *x, int max_iter);

/* ========================================================================
 * 稀疏线性求解器（CHOLMOD / UMFPACK 风格接口）
 *
 * 这些接口设计为与 SuiteSparse 库兼容的调用签名，
 * 当前实现为占位符（stub），标注 TODO 集成实际库。
 * ======================================================================== */

/**
 * @brief 稀疏 Cholesky 分解求解 Ax = b
 *
 * 要求 A 是对称正定（SPD）矩阵（CSR 格式）。
 * 借鉴 CHOLMOD 的 cholmod_analyze / cholmod_factorize / cholmod_solve 三步法。
 *
 * @param[in]  A 对称正定稀疏矩阵（CSR 格式）
 * @param[in]  b 右端向量（长度 A->rows）
 * @param[out] x 解向量（长度 A->cols，调用者预分配）
 * @return true 成功，false 失败（矩阵非 SPD 或内存不足）
 *
 * @todo 集成 SuiteSparse CHOLMOD：
 *       1. 将 SparseMatrix 转换为 cholmod_sparse
 *       2. cholmod_analyze → cholmod_factorize → cholmod_solve
 *       3. 将 cholmod_dense 结果拷贝回 x
 */
bool sparse_cholesky_solve(const SparseMatrix *A, const double *b, double *x);

/**
 * @brief 稀疏 LU 分解求解 Ax = b（非对称矩阵）
 *
 * 借鉴 UMFPACK 的 umfpack_di_symbolic / umfpack_di_numeric / umfpack_di_solve 三步法。
 *
 * @param[in]  A 稀疏方阵（CSR 格式，不必对称）
 * @param[in]  b 右端向量（长度 A->rows）
 * @param[out] x 解向量（长度 A->cols，调用者预分配）
 * @return true 成功，false 失败
 *
 * @todo 集成 SuiteSparse UMFPACK：
 *       1. 将 SparseMatrix CSR → CSC（UMFPACK 使用 CSC）
 *       2. umfpack_di_symbolic → umfpack_di_numeric → umfpack_di_solve
 */
bool sparse_lu_solve(const SparseMatrix *A, const double *b, double *x);

/**
 * @brief 稀疏 QR 分解求解最小二乘问题 min ||Ax - b||_2
 *
 * 适用于超定系统（行数 > 列数）。
 * 借鉴 SuiteSparse SPQR 的稀疏 QR 分解。
 *
 * @param[in]  A 稀疏矩阵（CSR 格式，允许矩形）
 * @param[in]  b 右端向量（长度 A->rows）
 * @param[out] x 解向量（长度 A->cols，调用者预分配）
 * @return true 成功，false 失败
 *
 * @todo 集成 SuiteSparse SPQR：
 *       1. 将 SparseMatrix 转换为 cholmod_sparse
 *       2. SuiteSparseQR 分解并求解
 */
bool sparse_qr_solve(const SparseMatrix *A, const double *b, double *x);

/* ========================================================================
 * 约束图 → 稀疏矩阵转换
 * ======================================================================== */

/**
 * @brief 从约束图提取约束矩阵的稀疏结构
 *
 * 构建一个稀疏的约束关联矩阵，其中：
 *   - 行 = 约束（constraint_id），共 constraint_count 行
 *   - 列 = 节点（node_id），共 node_count 列
 *   - 矩阵元素 A[i][j] != 0 当且仅当约束 i 涉及节点 j
 *
 * 元素的数值根据约束类型设置：
 *   - INCIDENCE / CONNECTION：±1（线性约束）
 *   - BETWEENNESS：-1, 2, -1（三点共线）
 *   - INTERSECTION：1（交点在两条线上）
 *   - CONTAINMENT：1（包含关系）
 *
 * 输出矩阵为 CSR 格式，owns_data = true。
 *
 * @param[in]  graph 约束图
 * @param[out] mat   输出的稀疏矩阵（调用者需用 sparse_matrix_destroy 释放）
 * @return true 成功，false 失败（参数无效或内存不足）
 */
bool graph_to_constraint_matrix(const ConstraintGraph *graph, SparseMatrix **mat);

/* ========================================================================
 * 稀疏矩阵运算
 * ======================================================================== */

/**
 * @brief 稀疏矩阵乘法 C = A * B（CSR 格式）
 *
 * 经典三重循环 CSR 矩阵乘法：对 A 的每个非零元素，
 * 在 B 的对应行中查找列匹配的非零元素，累加到 C。
 *
 * 要求 A->cols == B->rows。输出矩阵 C 为 CSR 格式，
 * owns_data = true，调用者需用 sparse_matrix_destroy 释放。
 *
 * @param[in]  A 左矩阵（CSR 格式）
 * @param[in]  B 右矩阵（CSR 格式）
 * @param[out] C 乘积矩阵（新分配，调用者释放）
 * @return true 成功，false 失败（维度不匹配或内存不足）
 */
bool sparse_matrix_multiply(const SparseMatrix *A, const SparseMatrix *B, SparseMatrix **C);

/**
 * @brief 稀疏矩阵转置
 *
 * 将 CSR 格式矩阵转置为 CSC 格式（反之亦然），
 * 或保持同格式转换。内部通过计数排序实现 O(nnz) 转置。
 *
 * @param[in]  mat 源矩阵
 * @param[out] out 转置后的矩阵（新分配，调用者释放）
 * @return true 成功，false 失败
 */
bool sparse_matrix_transpose(const SparseMatrix *mat, SparseMatrix **out);

/* ========================================================================
 * 约束图度分析
 * ======================================================================== */

/**
 * @brief 约束图度分析结果
 *
 * 存储约束图节点的度分布统计信息，以稀疏形式表示。
 * degree_counts[i] 表示度为 i 的节点数量，
 * 数组长度为 max_degree + 1。
 */
typedef struct {
    int node_count;     /**< 节点总数 */
    int max_degree;     /**< 最大度数 */
    int min_degree;     /**< 最小度数（非孤立节点） */
    double avg_degree;  /**< 平均度数 */
    int isolated_count; /**< 孤立节点数量（度为 0） */
    int *degree_counts; /**< 度分布直方图：degree_counts[d] = 度为 d 的节点数，长度 max_degree+1 */
    int *node_degrees;  /**< 每个节点的度数，长度 node_count，按 node_id 索引 */
} DegreeAnalysis;

/**
 * @brief 计算约束图度数分布的稀疏表示
 *
 * 遍历约束图的所有约束，统计每个节点参与约束的次数（度数）。
 * 结果以稀疏度分布直方图的形式返回，适合大型稀疏图的分析。
 *
 * @param[in]  graph    约束图
 * @param[out] analysis 度分析结果（调用者需用 degree_analysis_free 释放）
 * @return true 成功，false 失败
 */
bool graph_degree_analysis(const ConstraintGraph *graph, DegreeAnalysis **analysis);

/**
 * @brief 释放度分析结果
 *
 * @param[in,out] analysis 要释放的度分析结果
 */
void degree_analysis_free(DegreeAnalysis *analysis);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SPARSE_LINEAR_ALGEBRA_H */
