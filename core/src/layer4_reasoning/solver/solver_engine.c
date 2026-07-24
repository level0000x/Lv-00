/**
 * @file solver_engine.c
 * @brief 代数求解引擎（主入口）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 *          包含 solve_algebraic_system 主求解函数。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/solver.h"
#include "lv/stream.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"
#include "stream_context_util.h"

/* --- 共享宏 --- */
#define lv_SOLVER_DYNARRAY_INIT_CAP 16
#define lv_SOLVER_LINEAR_COEFF_COUNT 2
#define lv_SOLVER_QUADRATIC_COEFF_COUNT 3
#define lv_ZERO_EPSILON 1e-12
#define SOLVER_DETAIL_BUF_SIZE 512
#define EQUATION_PUSH_OR_GOTO(sys, poly, vid, ci, label)               \
    do {                                                               \
        if (equation_system_push((sys), (poly), (vid), (ci)) != 0) {   \
            lv_set_error(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)"); \
            goto label;                                                \
        }                                                              \
    } while (0)

/* ── PolyEquation + EquationSystem ── */
typedef struct {
    mpz_poly_t poly;
    int var_node_id;
    int coord_index;
} PolyEquation;

typedef struct EquationSystem {
    PolyEquation *eqs;
    int count;
    int capacity;
} EquationSystem;

/* ── SolverSnapshot ── */
typedef struct SolverSnapshot {
    int *node_ids;
    SymbolicCoord **copies;
    int node_count;
    int coord_count;
} SolverSnapshot;

/* 前向声明 */
void equation_system_init(EquationSystem *sys);
void equation_system_clear(EquationSystem *sys);
void extract_equations_from_constraints(const ConstraintGraph *graph, EquationSystem *sys);
void solve_equations_pass(EquationSystem *sys, GroebnerResult *result, int *solved_count, int *multiple_solutions,
                          bool *no_solution, bool do_substitute);
void cleanup_groebner_result(GroebnerResult *result);
bool solver_snapshot_save(const ConstraintGraph *graph, SolverSnapshot *snapshot);
void solver_snapshot_restore(ConstraintGraph *graph, const SolverSnapshot *snapshot);
void solver_snapshot_free(SolverSnapshot *snapshot);
bool is_out_of_scope(const mpz_poly_t *poly);
bool check_conflict_equations(const ConstraintGraph *graph);
bool check_contradiction_after_substitution(EquationSystem *sys);
int count_point_variables(const ConstraintGraph *graph, int **out_ids);
SolverStatus analyze_out_of_scope(const ConstraintGraph *graph, int var_id, char **suggestion);
SolverStatus groebner_basis_compute(EquationSystem *system);
static int *order_variables_by_dependency(const ConstraintGraph *graph, const int *var_ids, int var_count,
                                          const int *dirty_var_ids, int dirty_count, int *out_count);

lv_DECLARE_STREAM_CTX(solver);

void solver_set_stream_context(StreamContext *ctx) {
    solver_stream_ctx = ctx;
}

/* ================================================================== */
/*  PUBLIC API: solve_algebraic_system                                 */
/* ================================================================== */

SolverStatus solve_algebraic_system(ConstraintGraph *graph, const int *dirty_variable_ids, int dirty_count,
                                    GroebnerResult **out_result) {
    if (!graph || !out_result) {
        debug_log(LOG_LEVEL_ERROR, "solver", "solve_algebraic_system: 无效参数 graph=%p out_result=%p",
                  (const void *) graph, (const void *) out_result);
        return SOLVER_STATUS_TIMEOUT;
    }

    GroebnerResult *result = lv_calloc(1, sizeof(GroebnerResult));
    if (!result) {
        debug_log(LOG_LEVEL_ERROR, "solver", "solve_algebraic_system: 无法分配 GroebnerResult（大小 %zu 字节）",
                  sizeof(GroebnerResult));
        return SOLVER_STATUS_TIMEOUT;
    }
    result->solutions = NULL;
    result->solution_count = 0;
    result->unique = false;
    result->overdetermined = false;

    SolverSnapshot snapshot;
    bool snapshot_saved = solver_snapshot_save(graph, &snapshot);
    if (!snapshot_saved) {
        debug_log(LOG_LEVEL_WARN, "solver", "solve_algebraic_system: 快照保存失败，继续求解但无法回滚");
        memset(&snapshot, 0, sizeof(snapshot));
    }

    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_START, "代数求解开始", 0);

    /* Step 1: Extract algebraic equations from the constraint graph */
    EquationSystem sys;
    equation_system_init(&sys);
    extract_equations_from_constraints(graph, &sys);

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_EQUATION_EXTRACTED;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = sys.count;
        ev.total_steps = -1;
        ev.description = "方程提取完成";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_eq;
        lv_SAFE_SNPRINTF(_snw_eq, detail, sizeof(detail), "{\"equation_count\":%d,\"phase\":\"extraction\"}",
                         sys.count);
        lv_UNUSED(_snw_eq);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    /* Step 2: Check for overconstrained system */
    int total_eqs = 0;
    int max_degree_global = 0;
    for (int i = 0; i < sys.count; i++) {
        if (sys.eqs[i].poly.degree >= 0)
            total_eqs++;
        if (sys.eqs[i].poly.degree > max_degree_global)
            max_degree_global = sys.eqs[i].poly.degree;
    }

    int *point_ids = NULL;
    int point_count = count_point_variables(graph, &point_ids);
    int scalar_vars = point_count * 2;
    lv_free((void **) &point_ids);

    if (total_eqs > scalar_vars) {
        result->overdetermined = true;
        if (check_conflict_equations(graph)) {
            equation_system_clear(&sys);
            *out_result = result;
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 检测到冲突方程", 0);
            solver_snapshot_restore(graph, &snapshot);
            solver_snapshot_free(&snapshot);
            return SOLVER_STATUS_OVERCONSTRAINED;
        }
    }

    /* Step 3: Check for out-of-scope (degree > 3) */
    bool has_out_of_scope = false;
    for (int i = 0; i < sys.count; i++) {
        if (is_out_of_scope(&sys.eqs[i].poly)) {
            has_out_of_scope = true;
            break;
        }
    }
    if (has_out_of_scope) {
        equation_system_clear(&sys);
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 方程超出代数范围", 0);
        solver_snapshot_restore(graph, &snapshot);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    /* Step 4: Order variables by dependency */
    {
        int *all_var_ids = lv_calloc((size_t) sys.count, sizeof(int));
        if (!all_var_ids) {
            equation_system_clear(&sys);
            *out_result = result;
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解错误: 内存分配失败", 0);
            solver_snapshot_restore(graph, &snapshot);
            solver_snapshot_free(&snapshot);
            return SOLVER_STATUS_TIMEOUT;
        }
        int all_var_count = 0;
        for (int i = 0; i < sys.count; i++) {
            if (sys.eqs[i].poly.degree < 0)
                continue;
            int vid = sys.eqs[i].var_node_id;
            bool found = false;
            for (int j = 0; j < all_var_count; j++) {
                if (all_var_ids[j] == vid) {
                    found = true;
                    break;
                }
            }
            if (!found)
                all_var_ids[all_var_count++] = vid;
        }

        if (all_var_count > 0) {
            if (solver_stream_ctx) {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_PROGRESS;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.progress = 0.15;
                ev.description = "开始变量依赖拓扑排序";
                char detail[SOLVER_DETAIL_BUF_SIZE];
                int _snw_ts;
                lv_SAFE_SNPRINTF(_snw_ts, detail, sizeof(detail), "{\"phase\":\"topology_sort\",\"var_count\":%d}",
                                 all_var_count);
                lv_UNUSED(_snw_ts);
                ev.detail_json = detail;
                stream_emit(solver_stream_ctx, &ev);
            }

            int ordered_count = 0;
            int *ordered_ids = order_variables_by_dependency(graph, all_var_ids, all_var_count, dirty_variable_ids,
                                                             dirty_count, &ordered_count);

            if (ordered_ids && ordered_count > 0) {
                int *priority = lv_calloc((size_t) sys.count, sizeof(int));
                if (!priority) {
                    lv_free((void **) &ordered_ids);
                    lv_free((void **) &all_var_ids);
                    equation_system_clear(&sys);
                    *out_result = result;
                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解错误: 内存分配失败", 0);
                    solver_snapshot_restore(graph, &snapshot);
                    solver_snapshot_free(&snapshot);
                    return SOLVER_STATUS_OUT_OF_MEMORY;
                }
                for (int i = 0; i < sys.count; i++) {
                    priority[i] = INT_MAX;
                    int vid = sys.eqs[i].var_node_id;
                    for (int k = 0; k < ordered_count; k++) {
                        if (ordered_ids[k] == vid) {
                            priority[i] = k;
                            break;
                        }
                    }
                }

                for (int i = 0; i < sys.count - 1; i++) {
                    int best = i;
                    for (int j = i + 1; j < sys.count; j++) {
                        if (priority[j] < priority[best])
                            best = j;
                    }
                    if (best != i) {
                        PolyEquation tmp = sys.eqs[i];
                        sys.eqs[i] = sys.eqs[best];
                        sys.eqs[best] = tmp;
                        int tmp_pri = priority[i];
                        priority[i] = priority[best];
                        priority[best] = tmp_pri;
                    }
                }

                lv_free((void **) &priority);
                lv_free((void **) &ordered_ids);
            }
        }
        lv_free((void **) &all_var_ids);
    }

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_PROGRESS;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.progress = 0.25;
        ev.description = "拓扑排序完成，开始逐方程消元求解";
        stream_emit(solver_stream_ctx, &ev);
    }

    /* Step 5: Geometric reasoning elimination */
    int solved_count = 0;
    int multiple_solutions = 0;
    bool no_solution = false;

    solve_equations_pass(&sys, result, &solved_count, &multiple_solutions, &no_solution, true);

    /* Step 6: Check for contradictions */
    if (no_solution) {
        cleanup_groebner_result(result);
        equation_system_clear(&sys);
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 无解", 0);
        solver_snapshot_restore(graph, &snapshot);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_NO_SOLUTION;
    }

    /* Step 7: Gröbner basis elimination */
    {
        int remaining_before_gb = 0;
        for (int i = 0; i < sys.count; i++) {
            if (sys.eqs[i].poly.degree >= 0)
                remaining_before_gb++;
        }

        if (remaining_before_gb > 0) {
            if (solver_stream_ctx) {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_PROGRESS;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.progress = 0.55;
                ev.step_number = remaining_before_gb;
                ev.description = "逐方程消元完成，仍有剩余方程，进入Gröbner基计算";
                char detail[SOLVER_DETAIL_BUF_SIZE];
                int _snw_gb_prog;
                lv_SAFE_SNPRINTF(_snw_gb_prog, detail, sizeof(detail),
                                 "{\"phase\":\"groebner_entry\",\"remaining\":%d,\"solved\":%d}", remaining_before_gb,
                                 solved_count);
                lv_UNUSED(_snw_gb_prog);
                ev.detail_json = detail;
                stream_emit(solver_stream_ctx, &ev);
            }

            no_solution = false;

            bool has_high_degree = false;
            for (int i = 0; i < sys.count; i++) {
                if (sys.eqs[i].poly.degree >= 0 && sys.eqs[i].poly.degree > 4) {
                    has_high_degree = true;
                    break;
                }
            }

            if (has_high_degree) {
                char *suggestion = NULL;
                analyze_out_of_scope(graph, -1, &suggestion);
                lv_free((void **) &suggestion);
                equation_system_clear(&sys);
                *out_result = result;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 高次方程无法处理", 0);
                solver_snapshot_restore(graph, &snapshot);
                solver_snapshot_free(&snapshot);
                return SOLVER_STATUS_OUT_OF_SCOPE;
            }

            SolverStatus gb_status = groebner_basis_compute(&sys);

            if (gb_status == SOLVER_STATUS_OUT_OF_SCOPE) {
                char *suggestion = NULL;
                analyze_out_of_scope(graph, -1, &suggestion);
                lv_free((void **) &suggestion);
                equation_system_clear(&sys);
                *out_result = result;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: Groebner基计算超出范围", 0);
                solver_snapshot_restore(graph, &snapshot);
                solver_snapshot_free(&snapshot);
                return SOLVER_STATUS_OUT_OF_SCOPE;
            }

            if (gb_status == SOLVER_STATUS_OK) {
                solve_equations_pass(&sys, result, &solved_count, &multiple_solutions, &no_solution, true);

                if (no_solution || check_contradiction_after_substitution(&sys)) {
                    cleanup_groebner_result(result);
                    equation_system_clear(&sys);
                    *out_result = result;
                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: Groebner基求解后无解", 0);
                    solver_snapshot_restore(graph, &snapshot);
                    solver_snapshot_free(&snapshot);
                    return SOLVER_STATUS_NO_SOLUTION;
                }
            }
        }
    }

    /* Step 8: Determine result status */
    if (multiple_solutions > 0) {
        result->unique = false;
    } else if (solved_count > 0) {
        result->unique = true;
    }

    int remaining = 0;
    for (int i = 0; i < sys.count; i++) {
        if (sys.eqs[i].poly.degree >= 0)
            remaining++;
    }

    equation_system_clear(&sys);

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_PROGRESS;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.progress = 1.0;
        ev.step_number = solved_count;
        ev.total_steps = solved_count + remaining;
        ev.description = "代数求解总结";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_sum;
        lv_SAFE_SNPRINTF(_snw_sum, detail, sizeof(detail),
                         "{\"phase\":\"solve_summary\","
                         "\"solved_variables\":%d,"
                         "\"remaining_equations\":%d,"
                         "\"multiple_solutions\":%d,"
                         "\"unique\":%s,"
                         "\"overdetermined\":%s,"
                         "\"max_degree\":%d}",
                         solved_count, remaining, multiple_solutions, result->unique ? "true" : "false",
                         result->overdetermined ? "true" : "false", max_degree_global);
        lv_UNUSED(_snw_sum);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    if (remaining > 0 && solved_count == 0) {
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 未能求解任何变量", 0);
        solver_snapshot_restore(graph, &snapshot);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    if (result->overdetermined) {
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 超定系统", 0);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_OVERCONSTRAINED;
    }

    if (multiple_solutions > 0) {
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 多解", 0);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_MULTIPLE;
    }

    if (solved_count > 0 && remaining == 0) {
        result->unique = true;
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 唯一解", 0);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_UNIQUE;
    }

    *out_result = result;
    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 部分求解/欠定", 0);
    solver_snapshot_free(&snapshot);
    return SOLVER_STATUS_OK;
}
