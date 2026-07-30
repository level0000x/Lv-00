/**
 * @file solver.c
 * @brief 符号代数求解器实现
 *
 * @details 实现 Groebner 基算法和增量求解策略。
 *          支持从约束图中提取方程、求解代数系统、计算自由度。
 *          使用 GMP 多精度整数进行精确计算。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - solver.h              : 求解器公共接口定义
 *   - lv_utils.h          : 统一内存分配器（lv_malloc/lv_free）
 *   - lv_internal.h       : 内部数据结构和常量
 *   - constraint_graph.h    : 约束图接口
 *   - mpz_poly.h            : GMP 多精度多项式操作
 *   - stream.h              : 流式事件输出接口
 *
 * @note 内存管理策略（设计决策）：
 *   - GMP 多精度多项式系数数组（poly.coeffs）必须使用标准 malloc/calloc
 *     而非 lv_malloc 分配，因为 mpz_poly_clear() 内部调用标准 lv_free() 释放
 *     系数数组。若使用 lv_malloc 分配（带额外 AllocHeader），lv_free() 将因为
 *     指针不指向标准堆头而导致未定义行为。这是 GMP 库的硬性要求，非可选方案。
 *   - 其他所有动态内存分配统一使用 lv_malloc/lv_free。
 *
 * @note GMP 变量作用域与所有权规则（v3.3.0 补充）：
 *   - 每个 mpz_init/mpq_init 必须在相同或内层作用域内对应一个 mpz_clear/mpq_clear，
 *     且两者的执行路径必须通过所有代码路径可达。不允许有 init 无 clear 的路径。
 *   - 跨代码块复用的 GMP 变量：在变量声明处添加 [owner: block_name] 注释
 *     标明其生命周期边界。此类变量必须在定义所在作用域结束时 clear，不得在
 *     其他作用域提前释放。
 *   - 循环体内 init 的变量：必须在同一次迭代结束前 clear。循环内频繁 init/clear
 *     的变量可考虑提升到循环外并复用（mpz_set 后使用），以节省分配开销。
 *   - 条件分支内的 init：确保 if/else 两个分支都有对应的 clear，不得在
 *     一个分支 init 而在另一个分支泄漏。
 */

#include "lv/solver.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/stream.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h" /* lv_malloc / lv_free —— 统一内存分配器 */
#include "mpz_poly.h"
#include "stream_context_util.h"

/* SOLVER_MAX_VAR_ID 已在 solver.h 中统一定义，此处不再重复 */

/**
 * lv_SOLVER_DYNARRAY_INIT_CAP - 动态数组初始容量
 *
 * 用于 equation_system_push, mv_poly_add_term, dirty_set_add 等
 * 多处动态数组的初始分配容量。值取 16，兼顾小系统零次扩容与大系统
 * 较少扩容次数之间的平衡。
 */
#define lv_SOLVER_DYNARRAY_INIT_CAP 16

/**
 * lv_SOLVER_LINEAR_COEFF_COUNT - 一元一次多项式系数数量
 *
 * degree=1 的多项式拥有 2 个系数：coeffs[0]（常数项）和 coeffs[1]（一次项）。
 * 用于 poly.coeffs 数组的 malloc 分配。
 */
#define lv_SOLVER_LINEAR_COEFF_COUNT 2

/**
 * lv_SOLVER_QUADRATIC_COEFF_COUNT - 一元二次多项式系数数量
 *
 * degree=2 的多项式拥有 3 个系数：coeffs[0]（常数项）、coeffs[1]（一次项）
 * 和 coeffs[2]（二次项）。用于 poly.coeffs 数组的 malloc 分配。
 */
#define lv_SOLVER_QUADRATIC_COEFF_COUNT 3

/**
 * SOLVER_DETAIL_BUF_SIZE - 流式输出 detail JSON 缓冲区大小
 *
 * 用于 stream_emit 中 detail_json 字段的临时栈缓冲区。
 */
/* ------------------------------------------------------------------ */
/*  SolverSnapshot — 求解回滚机制                                       */
/* ------------------------------------------------------------------ */

/* ── 内部结构体（剩余模块引用）─── */
typedef struct SolverSnapshot {
    int *node_ids;
    SymbolicCoord **copies;
    int node_count;
    int coord_count;
} SolverSnapshot;

/* solver_stream_ctx 由 solver_engine.c 集中定义，此处通过 solver_types.h 引用 extern 声明 */
/* PolyEquation、EquationSystem 及其操作函数定义均在 solver_types.h 中 */
#include "lv/solver_types.h"

void solver_set_stream_context(StreamContext *ctx) {
    solver_stream_ctx = ctx;
}

/* ------------------------------------------------------------------ */
/*  前向声明：solver 子模块（solver_eq_system / solver_coord_extract /  */
/*  solver_symbolic / solver_snapshot / solver_linear）中定义的 static  */
/*  函数。这些函数在对应的 .c 文件中实现，solver.c 通过 unity build      */
/*  或链接时可见。                                                      */
/* ------------------------------------------------------------------ */

/* solver_coord_extract.c */
bool coord_to_double(const SymbolicCoord *c, double *out);
bool coord_to_mpz_scaled(const SymbolicCoord *c, mpz_t result, int64_t scale);
void double_to_mpz_scaled(double val, mpz_t result, int64_t scale);
bool point_coord(const GeomNode *pt, int idx, double *out);
void extract_equations_from_constraints(const ConstraintGraph *graph, EquationSystem *sys);

/* 直线方程结构体 ax + by + c = 0 */
typedef struct {
    double a, b, c;
} LineEquation;
bool line_from_two_points(GeomNode *p1, GeomNode *p2, LineEquation *out);

/* solver_linear.c */
bool solve_linear(const mpz_poly_t *poly, double *x_out);

/* solver_symbolic.c */
SymbolicCoord *poly_eval_symbolic(const mpz_poly_t *poly, const SymbolicCoord *value);
void substitute_solved(EquationSystem *sys, int var_node_id, int coord_index, double value);
bool is_out_of_scope(const mpz_poly_t *poly);
bool try_factor_polynomial(const mpz_poly_t *poly, mpz_poly_t *factor1, mpz_poly_t *factor2);
bool check_incompatible_distances(const ConstraintGraph *graph);
bool check_contradiction_after_substitution(EquationSystem *sys);
int constraint_weight(const Constraint *c);
int count_point_variables(const ConstraintGraph *graph, int **out_ids);
void solve_equations_pass(EquationSystem *sys, GroebnerResult *result, int *solved_count, int *multiple_solutions,
                          bool *no_solution, bool do_substitute);
void cleanup_groebner_result(GroebnerResult *result);

/* solver_snapshot.c */
bool solver_snapshot_save(const ConstraintGraph *graph, SolverSnapshot *snapshot);
void solver_snapshot_restore(ConstraintGraph *graph, const SolverSnapshot *snapshot);
void solver_snapshot_free(SolverSnapshot *snapshot);

#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label)               \
    do {                                                               \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) {   \
            lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)"); \
            goto label;                                                \
        }                                                              \
    } while (0)

static int *order_variables_by_dependency(const ConstraintGraph *graph, const int *var_ids, int var_count,
                                          const int *dirty_var_ids, int dirty_count, int *out_count);

/* ================================================================== */
/*  已提取的子模块                                                   */
/* ================================================================== */

/* Extracted to solver/solver_engine.c         - solve_algebraic_system */
/* Extracted to solver/solver_feedback.c        - solver_feedback_destroy et al. */
/* Extracted to solver/solver_eliminate.c       - eliminate_geometry, analyze_out_of_scope */
/* Extracted to solver/solver_stats.c           - count_degrees_of_freedom, constraint_weight */
/* Extracted to solver/solver_conflict.c        - check_conflict_equations */
/* Extracted to solver/solver_groebner.c        - mv_poly_*, s_polynomial, polynomial_reduce, */
/*                                                buchberger_groebner, build_mv_polynomials, */
/*                                                groebner_basis_compute */
/* Extracted to solver/solver_geom_templates.c  - template_* geometry templates */
/* Extracted to solver/solver_dirty_set.c       - dirty_set_* */
/* Extracted to solver/solver_order.c           - order_variables_by_dependency, compute_elimination_order */
/* Extracted to solver/solver_eq_system.c       - equation_system_destroy/count/get_var_id/get_coord_index */
/* Extracted to solver/solver_incremental.c     - propagate_dependency, solver_incremental_solve */
/* Extracted to solver/solver_result.c          - groebner_result_destroy */
/* Extracted to solver/solver_equation_extract.c - solver_extract_equations_full */
/* Extracted to solver/solver_multibranch.c     - solver_handle_multiple_solutions */

/* 注意：solver.c 现在仅保留公共类型定义、宏和前向声明。           */
/* 所有函数实现已拆分到 solver/ 目录下的子模块文件中。             */
