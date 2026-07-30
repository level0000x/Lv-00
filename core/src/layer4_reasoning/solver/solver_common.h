#ifndef SOLVER_COMMON_H
#define SOLVER_COMMON_H

/**
 * @file solver_common.h
 * @brief 求解器子模块公共包含头文件
 *
 * 消除 14+ 个 solver_*.c 文件中重复的 include 块和前向声明。
 * 每个 solver_*.c 文件只需 #include "solver_common.h" 即可。@n
 *
 * 包含：
 *   - 标准库头文件（float.h, math.h, stdint.h, stdio.h, stdlib.h, string.h）
 *   - Lv-00 公共 API 头文件
 *   - 模块内部工具头文件
 *   - 跨模块前向声明
 */

/* ===== 标准库头文件 ===== */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Lv-00 公共 API ===== */

#include "lv/constraint_graph.h"
#include "lv/solver.h"
#include "lv/solver_types.h"
#include "lv/stream.h"

/* ===== 模块内部工具头文件 ===== */

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"
#include "stream_context_util.h"

/* ===== 跨模块前向声明 ===== */

/* --- solver_symbolic.c (精确求解/回代/消元) --- */
int coord_to_double(const SymbolicCoord *c, double *out);
void double_to_mpz_scaled(double val, mpz_t result, int64_t scale);
SymbolicCoord *poly_eval_symbolic(const mpz_poly_t *poly, const SymbolicCoord *value);
void symbolic_coord_destroy(SymbolicCoord *coord);
SymbolicCoord *symbolic_coord_create_rational(int64_t num, uint64_t den);
char *symbolic_coord_serialize(const SymbolicCoord *coord);
bool is_out_of_scope(const mpz_poly_t *poly);
bool check_contradiction_after_substitution(EquationSystem *sys);
int constraint_weight(const Constraint *c);
int count_point_variables(const ConstraintGraph *graph, int **out_ids);
bool try_factor_polynomial(const mpz_poly_t *poly, mpz_poly_t *factor1, mpz_poly_t *factor2);
void substitute_solved(EquationSystem *sys, int var_node_id, int coord_index, double value);
void solve_equations_pass(EquationSystem *sys, GroebnerResult *result,
                          int *solved_count, int *multiple_solutions,
                          bool *no_solution, bool do_substitute);
void cleanup_groebner_result(GroebnerResult *result);
int solve_quadratic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions);
int solve_cubic_exact(const mpz_poly_t *poly, SymbolicCoord **solutions, int max_solutions);

/* --- solver_coord_extract.c (坐标/方程提取) --- */
void extract_equations_from_constraints(const ConstraintGraph *graph, EquationSystem *sys);
bool point_coord(const GeomNode *pt, int idx, double *out);
bool coord_to_mpz_scaled(const SymbolicCoord *c, mpz_t result, int64_t scale);

/* --- solver_linear.c (数值求解) --- */
bool solve_linear(const mpz_poly_t *poly, double *x_out);

/* --- solver_conflict.c (矛盾检测) --- */
bool check_incompatible_distances(const ConstraintGraph *graph);
bool check_conflict_equations(const ConstraintGraph *graph);

/* --- solver_order.c (消元顺序) --- */
int *order_variables_by_dependency(const ConstraintGraph *graph,
                                   const int *var_ids, int var_count,
                                   const int *dirty_var_ids, int dirty_count,
                                   int *out_count);

/* --- solver_stats.c (自由度统计) --- */
int count_degrees_of_freedom(const ConstraintGraph *graph, int **out_free_var_ids);

/* --- solver_result.c (结果生命周期) --- */
void groebner_result_destroy(GroebnerResult *result);

/* --- solver_incremental.c (增量求解) --- */
GroebnerResult *solver_incremental_solve(ConstraintGraph *graph,
                                         const int *dirty_var_ids, int n_dirty_vars);

/* --- solver_geom_templates.c (几何消元模板) --- */
int template_similar_triangles(ConstraintGraph *graph, EquationSystem *sys);
int template_pythagorean(ConstraintGraph *graph, EquationSystem *sys);
int template_parallel_cut(const ConstraintGraph *graph, EquationSystem *sys);

/* --- solver_eliminate.c (消元与分析) --- */
SolverStatus analyze_out_of_scope(const ConstraintGraph *graph, int var_id, char **suggestion);

/* --- solver_groebner.c (Groebner 基计算) --- */
SolverStatus groebner_basis_compute(EquationSystem *system);

/* --- solver_equation_extract.c (增强方程提取) --- */
int solver_extract_equations_full(const ConstraintGraph *graph, EquationSystem *out_system);

#endif /* SOLVER_COMMON_H */
