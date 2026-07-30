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
#include "lv/solver_types.h"
#include "lv/stream.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "mpz_poly.h"
#include "stream_context_util.h"

/* ── SolverSnapshot ── */
typedef struct SolverSnapshot {
    int *node_ids;
    SymbolicCoord **copies;
    int node_count;
    int coord_count;
} SolverSnapshot;

/* 前向声明 */

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
int *order_variables_by_dependency(const ConstraintGraph *graph, const int *var_ids, int var_count,
                                          const int *dirty_var_ids, int dirty_count, int *out_count);

/** @brief solver 全局流式上下文定义（供所有 solver 模块通过 solver_types.h 的 extern 引用） */
lv_THREAD_LOCAL StreamContext *solver_stream_ctx = NULL;

static void solver_set_stream_context_local(StreamContext *ctx) {
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
    PolyEquation *const eqs_arr = (PolyEquation *)sys.eqs.data;
    const int eqs_count = sys.eqs.count;

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_EQUATION_EXTRACTED;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = eqs_count;
        ev.total_steps = -1;
        ev.description = "方程提取完成";
        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
        int _snw_eq;
        lv_SAFE_SNPRINTF(_snw_eq, detail, sizeof(detail), "{\"equation_count\":%d,\"phase\":\"extraction\"}",
                         eqs_count);
        lv_UNUSED(_snw_eq);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    /* Step 2: Check for overconstrained system */
    int total_eqs = 0;
    int max_degree_global = 0;
    for (int i = 0; i < eqs_count; i++) {
        if (eqs_arr[i].poly.degree >= 0)
            total_eqs++;
        if (eqs_arr[i].poly.degree > max_degree_global)
            max_degree_global = eqs_arr[i].poly.degree;
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
    for (int i = 0; i < eqs_count; i++) {
        if (is_out_of_scope(&eqs_arr[i].poly)) {
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
        int *all_var_ids = lv_calloc((size_t) eqs_count, sizeof(int));
        if (!all_var_ids) {
            equation_system_clear(&sys);
            *out_result = result;
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解错误: 内存分配失败", 0);
            solver_snapshot_restore(graph, &snapshot);
            solver_snapshot_free(&snapshot);
            return SOLVER_STATUS_TIMEOUT;
        }
        int all_var_count = 0;
        for (int i = 0; i < eqs_count; i++) {
            if (eqs_arr[i].poly.degree < 0)
                continue;
            int vid = eqs_arr[i].var_node_id;
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
                char detail[lv_SOLVER_DETAIL_BUF_SIZE];
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
                int *priority = lv_calloc((size_t) eqs_count, sizeof(int));
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
                for (int i = 0; i < eqs_count; i++) {
                    priority[i] = INT_MAX;
                    int vid = eqs_arr[i].var_node_id;
                    for (int k = 0; k < ordered_count; k++) {
                        if (ordered_ids[k] == vid) {
                            priority[i] = k;
                            break;
                        }
                    }
                }

                for (int i = 0; i < eqs_count - 1; i++) {
                    int best = i;
                    for (int j = i + 1; j < eqs_count; j++) {
                        if (priority[j] < priority[best])
                            best = j;
                    }
                    if (best != i) {
                        PolyEquation tmp = eqs_arr[i];
                        eqs_arr[i] = eqs_arr[best];
                        eqs_arr[best] = tmp;
                        int tmp_pri = priority[i];
                        priority[i] = priority[best];
                        priority[best] = tmp_pri