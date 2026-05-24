/**
 * @file solver.h
 * @brief 代数求解器 —— Groebner 基、方程提取与冲突检测
 * @details 提供基于 Groebner 基方法的代数约束求解、全类型约束方程提取、
 * 增量求解（仅重解脏变量子图）、冲突方程检测、自由度计算以及
 * 代数数运算（结式法求和/积的最小多项式）。
 */

#ifndef LV00_SOLVER_H
#define LV00_SOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "mpz_poly.h"
#include "stream.h"
#include "symbolic_coord.h"

/* 稀疏矩阵求解器（sparse_linear_algebra.h）在 solver_sparse_solve 中按需引用 */

/**
 * @brief 求解器中变量 ID 的最大值
 * @details 防止稀疏 ID 导致 OOM。此值与 solver.c 中保持严格一致，
 *          任何修改必须同步更新两处，否则可能导致未定义行为。
 * @note 当前值 100000，对应 solver.c 中的同名常量
 */
#define SOLVER_MAX_VAR_ID 100000

/**
 * @brief 设置求解器的流式输出上下文
 * @param ctx  流式上下文（可为 NULL 以禁用流式输出）
 */
void solver_set_stream_context(StreamContext *ctx);

/* Forward declaration: opaque equation system type */
typedef struct EquationSystem EquationSystem;

/**
 * @brief Groebner 基求解结果
 */
typedef struct GroebnerResult {
    SymbolicCoord **solutions; /**< 解数组 */
    int solution_count;        /**< 解的数量 */
    bool unique;               /**< 是否唯一解 */
    bool overdetermined;       /**< 是否过度约束 */
} GroebnerResult;

/**
 * @brief 求解器状态
 */
typedef enum {
    SOLVER_OK,              /**< 成功 */
    SOLVER_UNIQUE,          /**< 唯一解 */
    SOLVER_MULTIPLE,        /**< 多解 */
    SOLVER_NO_SOLUTION,     /**< 无解 */
    SOLVER_OVERCONSTRAINED, /**< 过度约束 */
    SOLVER_OUT_OF_SCOPE,    /**< 超出范围 */
    SOLVER_TIMEOUT          /**< 超时 */
} SolverStatus;

/* AlgebraicOp is now defined in mpz_poly.h (included above) */

/**
 * @brief 使用 Groebner 基方法求解代数约束系统
 *
 * @param[in]  graph              约束图
 * @param[in]  dirty_variable_ids 脏变量节点 ID 数组（可为 NULL）
 * @param[in]  dirty_count       脏变量数量
 * @param[out] out_result        成功时接收新分配的 GroebnerResult。
 *                               调用者需用 groebner_result_free() 释放。
 *                               失败时设为 NULL。
 *
 * @return 求解器状态：SOLVER_OK / SOLVER_UNIQUE / SOLVER_MULTIPLE /
 *         SOLVER_NO_SOLUTION / SOLVER_OVERCONSTRAINED /
 *         SOLVER_OUT_OF_SCOPE / SOLVER_TIMEOUT
 */
SolverStatus solve_algebraic_system(ConstraintGraph *graph, const int *dirty_variable_ids, int dirty_count,
                                    GroebnerResult **out_result);

/**
 * @brief 消除几何约束
 * @param[in,out] graph        约束图
 * @param[in] target_var_id   目标变量 ID
 * @param[in] eliminate_ids   要消除的变量 ID 数组
 * @param[in] elim_count      要消除的变量数量
 * @return 求解器状态
 */
SolverStatus eliminate_geometry(ConstraintGraph *graph, int target_var_id, const int *eliminate_ids, int elim_count);

/**
 * @brief 分析超出范围的变量
 * @param[in] graph    约束图
 * @param[in] var_id   变量 ID
 * @param[out] suggestion 建议信息输出
 * @return 求解器状态
 */
SolverStatus analyze_out_of_scope(const ConstraintGraph *graph, int var_id, char **suggestion);

/**
 * 计算约束图的自由度（degrees of freedom）。
 *
 * @param[in]  graph             约束图（不得为 NULL）
 * @param[out] out_free_var_ids  成功时输出新分配的"自由变量节点ID"数组。
 *                               调用者负责释放该数组（使用 lv00_free）。
 *                               若图完全确定（0 自由度），*out_free_var_ids 被设为 NULL。
 *                               若发生错误，*out_free_var_ids 保持不变。
 *
 * @return 自由变量数量（自由度），出错时返回 -1。
 *         注意：返回 -1 明确区分了"0 自由度（完全确定）"与"函数出错"两种情况。
 */
int count_degrees_of_freedom(const ConstraintGraph *graph, int **out_free_var_ids);

bool check_conflict_equations(const ConstraintGraph *graph);

/**
 * @brief 使用结式法计算代数数的和或积的最小多项式
 *
 * 对于 alpha + beta：Res_y(p(y), q(x - y))
 * 对于 alpha * beta：Res_y(p(y), y^n * q(x/y))，其中 n = deg(q)
 *
 * @param[in] p      alpha 的最小多项式（变量为 y）
 * @param[in] q      beta 的最小多项式（变量为 y）
 * @param[in] op     操作类型：ALG_OP_SUM 或 ALG_OP_PRODUCT
 * @param[out] result 结果最小多项式
 * @return true 成功，false 失败（度数超过 4 或其他错误）
 */
bool compute_algebraic_resultant(const mpz_poly_t *p, const mpz_poly_t *q, AlgebraicOp op, mpz_poly_t *result);

/* ------------------------------------------------------------------ */
/*  增量求解：仅重解脏变量子图                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief 增量求解约束系统
 *
 * 仅重解与脏变量相关的最小依赖子图。
 *
 * @param[in] graph         约束图
 * @param[in] dirty_var_ids 发生变化的变量节点 ID 数组
 * @param[in] n_dirty_vars 脏变量数量
 * @return 脏变量的 GroebnerResult。
 *         调用者需用 groebner_result_free() 释放。
 *         出错返回 NULL。
 */
GroebnerResult *solver_incremental_solve(ConstraintGraph *graph, const int *dirty_var_ids, int n_dirty_vars);

/**
 * @brief 释放 Groebner 结果
 * @param[in,out] result 要释放的结果
 */
void groebner_result_free(GroebnerResult *result);

/* ------------------------------------------------------------------ */
/*  增强方程提取：支持所有约束类型                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief 从所有约束类型提取代数方程
 *
 * 支持的约束类型：
 *   INCIDENCE：点在线上 => 叉积 = 0（线性）
 *   INTERSECTION：两线段相交 => 参数化线性系统
 *   CONTAINMENT：点在区域内 => 卷绕数（标记为超出范围）
 *   BETWEENNESS：无独立方程（用于解选择）
 *
 * @param[in]  graph     约束图
 * @param[out] out_system 输出方程系统（调用者需初始化/清空）
 * @return 提取的方程数量，出错返回 -1
 */
int solver_extract_equations_full(const ConstraintGraph *graph, EquationSystem *out_system);

/* ------------------------------------------------------------------ */
/*  Groebner 基计算（度数 <= 2）                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief 使用简化的 Buchberger 算法计算 Groebner 基
 *
 * 仅处理总度数 <= 2 的多项式系统。
 *
 * @param[in,out] system 方程系统（原地修改：方程被 Groebner 基替换）
 * @return SOLVER_OK 成功，
 *         SOLVER_OUT_OF_SCOPE 多项式度数 > 2，
 *         SOLVER_TIMEOUT 超过步数限制。
 */
SolverStatus groebner_basis_compute(EquationSystem *system);

/* ------------------------------------------------------------------ */
/*  方程系统生命周期                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief 创建新的空方程系统
 * @return 新创建的方程系统，失败返回 NULL
 */
EquationSystem *equation_system_create(void);

/**
 * @brief 销毁方程系统并释放所有资源
 * @param[in,out] sys 要销毁的方程系统
 */
void equation_system_destroy(EquationSystem *sys);

/**
 * @brief 获取方程系统中的方程数量
 * @param[in] sys 方程系统
 * @return 方程数量
 */
int equation_system_count(const EquationSystem *sys);

/**
 * @brief 获取指定索引的多项式
 * @param[in] sys   方程系统
 * @param[in] index 索引
 * @return 多项式指针，超出范围返回 NULL
 */
const mpz_poly_t *equation_system_get_poly(const EquationSystem *sys, int index);

/**
 * @brief 获取指定索引的变量节点 ID
 * @param[in] sys   方程系统
 * @param[in] index 索引
 * @return 变量节点 ID，超出范围返回 -1
 */
int equation_system_get_var_id(const EquationSystem *sys, int index);

/**
 * @brief 获取指定索引的坐标索引
 * @param[in] sys   方程系统
 * @param[in] index 索引
 * @return 坐标索引（0=x, 1=y），超出范围返回 -1
 */
int equation_system_get_coord_index(const EquationSystem *sys, int index);

/* ------------------------------------------------------------------ */
/*  多解分支处理                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief 处理代数求解中的多解分支
 *
 * 当求解器发现二次（degree=2）或更高次的方程存在多个代数解时，
 * 生成所有可能的解分支。每个分支是一个符号坐标数组，
 * 包含每个变量的可能取值。
 *
 * 算法：
 *  1. 遍历方程系统，识别所有二次方程
 *  2. 对每个二次方程，计算其两个根
 *  3. 生成所有解组合（笛卡尔积），即 2^k 个分支（k = 二次方程数）
 *  4. 过滤无效的分支（如导致其他方程矛盾的组合）
 *
 * 使用流式输出：在计算每个分支和过滤无效分支时发射事件。
 *
 * @param[in]  result           已有求解结果（含已求解的变量和方程信息）
 * @param[in]  system           方程系统（用于二次方程识别和解的计算）
 * @param[out] out_branches     输出的解分支数组（每个分支是一个符号坐标数组），
 *                              调用者需逐分支释放
 * @param[out] out_branch_count 输出的分支数量
 *
 * @return SOLVER_OK 成功生成分支
 *         SOLVER_UNIQUE 实际只有唯一解（无二次方程或只有一个有效根）
 *         SOLVER_NO_SOLUTION 所有分支均无效
 *         SOLVER_OUT_OF_SCOPE 有高次（degree>2）方程无法处理
 *         SOLVER_TIMEOUT 分支组合过多或内存不足
 */
SolverStatus solver_handle_multiple_solutions(const GroebnerResult *result, const EquationSystem *system,
                                              SymbolicCoord ***out_branches, int *out_branch_count);

/* ============== 交互式求解反馈（Solvespace风格） ============== */

/**
 * @brief 求解器交互事件类型（借鉴 Solvespace 的拖拽约束实时反馈设计）
 *
 * Solvespace 的核心交互理念：
 * - 用户拖拽几何元素 → 约束图变化 → 求解器增量重解 → 实时视觉反馈
 * - 明确告诉用户：哪些变量已确定（绿色）、哪些可拖动（蓝色）、哪些过约束（红色）
 * - 自由度可视化：每个可拖动元素高亮，过约束元素警告
 */
typedef enum {
    SOLVER_FEEDBACK_CONSTRAINT_ADDED, /**< 约束已添加，增量求解开始 */
    SOLVER_FEEDBACK_VARIABLE_SOLVED,  /**< 变量被唯一确定（变绿） */
    SOLVER_FEEDBACK_VARIABLE_FREE,    /**< 变量仍有自由度（保持蓝） */
    SOLVER_FEEDBACK_OVERCONSTRAINED,  /**< 检测到过约束（变红） */
    SOLVER_FEEDBACK_DOF_CHANGED,      /**< 自由度数量变化 */
    SOLVER_FEEDBACK_CONFLICT_DETECTED /**< 约束冲突：上次解不满足新约束 */
} SolverFeedbackType;

/**
 * @brief 求解器交互反馈信息（Solvespace风格）
 *
 * 每次用户交互后，求解器返回结构化的反馈信息，
 * 供 Web GUI 画布层即时更新视觉状态。
 */
typedef struct SolverFeedback {
    SolverFeedbackType type;   /**< 反馈类型 */
    int affected_var_id;       /**< 受影响的变量节点ID */
    char *message;             /**< 人类可读消息（如"点A的位置已确定"） */
    int degrees_of_freedom;    /**< 当前剩余自由度 */
    int *free_var_ids;         /**< 仍有自由度的变量ID数组 */
    int free_var_count;        /**< 自由变量数量 */
    int *overconstrained_ids;  /**< 过约束的变量ID数组 */
    int overconstrained_count; /**< 过约束变量数量 */
} SolverFeedback;

/**
 * @brief 创建求解器反馈（调用者需用 solver_feedback_destroy 释放）
 */
SolverFeedback *solver_feedback_create(SolverFeedbackType type, const char *message);

/**
 * @brief 销毁求解器反馈
 */
void solver_feedback_destroy(SolverFeedback *feedback);

/**
 * @brief 增量求解并返回交互反馈
 *
 * 借鉴 Solvespace 的拖拽-实时反馈循环：
 * 1. 用户添加/修改约束
 * 2. 求解器仅重解脏变量子图
 * 3. 返回结构化的反馈信息，标注每个变量的状态变化
 *
 * @param graph          约束图
 * @param dirty_vars     发生变化的变量节点ID
 * @param dirty_count    脏变量数量
 * @return 新分配的求解器反馈（调用者需释放），失败返回NULL
 */
SolverFeedback *solver_feedback_solve(ConstraintGraph *graph, const int *dirty_vars, int dirty_count);

/* ============== 稀疏矩阵求解后端（SuiteSparse/GraphBLAS） ============== */

/**
 * @brief 使用稀疏矩阵方法求解约束系统（替代密集 GMP 求解器）
 * 借鉴 SuiteSparse CHOLMOD/UMFPACK 的稀疏 Cholesky/LU 分解
 * 用于加速线性约束和二次约束的数值求解路径
 * @param graph 约束图
 * @param out_result 接收 GroebnerResult（调用者释放）
 * @return 求解器状态
 */
SolverStatus solver_sparse_solve(ConstraintGraph *graph, GroebnerResult **out_result);

#ifdef __cplusplus
}
#endif

#endif /* LV00_SOLVER_H */