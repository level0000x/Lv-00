/**
 * @file solver_incremental.c
 * @brief 增量求解（仅重新求解与脏变量相关的依赖子图）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
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

/* 共享宏 */
#define lv_SOLVER_DYNARRAY_INIT_CAP 16
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

/* 脏变量集合结构体 */
typedef struct {
    int *dirty_ids;
    int dirty_count;
    int capacity;
} DirtyVariableSet;

/* 前向声明 */
void equation_system_init(EquationSystem *sys);
int equation_system_push(EquationSystem *sys, mpz_poly_t poly, int var_node_id, int coord_index);
void equation_system_clear(EquationSystem *sys);
void extract_equations_from_constraints(const ConstraintGraph *graph, EquationSystem *sys);
void solve_equations_pass(EquationSystem *sys, GroebnerResult *result, int *solved_count, int *multiple_solutions,
                          bool *no_solution, bool do_substitute);
void cleanup_groebner_result(GroebnerResult *result);
SolverStatus groebner_basis_compute(EquationSystem *system);
static void dirty_set_init(DirtyVariableSet *ds);
static bool dirty_set_contains(DirtyVariableSet *ds, int var_id);
static void dirty_set_add(DirtyVariableSet *ds, int var_id);
static void dirty_set_clear(DirtyVariableSet *ds);
static void dirty_set_free(DirtyVariableSet *ds);
static void filter_equations_for_dirty(EquationSystem *sys, DirtyVariableSet *ds, EquationSystem *filtered);

lv_DECLARE_STREAM_CTX(solver);

/* ================================================================== */
/*  脏变量集合（内联实现，因仅在增量模块中使用）                         */
/* ================================================================== */

static void dirty_set_init(DirtyVariableSet *ds) {
    ds->dirty_ids = NULL;
    ds->dirty_count = 0;
    ds->capacity = 0;
}

static bool dirty_set_contains(DirtyVariableSet *ds, int var_id) {
    for (int i = 0; i < ds->dirty_count; i++) {
        if (ds->dirty_ids[i] == var_id)
            return true;
    }
    return false;
}

static void dirty_set_add(DirtyVariableSet *ds, int var_id) {
    if (dirty_set_contains(ds, var_id))
        return;
    if (ds->dirty_count >= ds->capacity) {
        int new_cap = ds->capacity == 0 ? lv_SOLVER_DYNARRAY_INIT_CAP : ds->capacity * 2;
        if (new_cap > 0 && new_cap > INT_MAX / 2)
            return;
        new_cap = ds->capacity == 0 ? lv_SOLVER_DYNARRAY_INIT_CAP : ds->capacity * 2;
        ds->capacity = new_cap;
        int *new_ids = lv_realloc(ds->dirty_ids, (size_t) ds->capacity * sizeof(int));
        if (!new_ids)
            return;
        ds->dirty_ids = new_ids;
    }
    ds->dirty_ids[ds->dirty_count++] = var_id;
}

static void dirty_set_clear(DirtyVariableSet *ds) {
    ds->dirty_count = 0;
}

static void dirty_set_free(DirtyVariableSet *ds) {
    lv_free((void **) &ds->dirty_ids);
    ds->dirty_ids = NULL;
    ds->dirty_count = 0;
    ds->capacity = 0;
}

static void filter_equations_for_dirty(EquationSystem *sys, DirtyVariableSet *ds, EquationSystem *filtered) {
    equation_system_init(filtered);
    if (!sys || !ds || ds->dirty_count == 0)
        return;

    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].poly.degree < 0)
            continue;
        if (dirty_set_contains(ds, sys->eqs[i].var_node_id)) {
            if (equation_system_push(filtered, sys->eqs[i].poly, sys->eqs[i].var_node_id, sys->eqs[i].coord_index) != 0)
                return;
        }
    }

    DirtyVariableSet related;
    dirty_set_init(&related);
    for (int i = 0; i < filtered->count; i++) {
        dirty_set_add(&related, filtered->eqs[i].var_node_id);
    }

    for (int i = 0; i < sys->count; i++) {
        if (sys->eqs[i].poly.degree < 0)
            continue;
        if (dirty_set_contains(&related, sys->eqs[i].var_node_id)) {
            bool found = false;
            for (int j = 0; j < filtered->count; j++) {
                if (filtered->eqs[j].var_node_id == sys->eqs[i].var_node_id &&
                    filtered->eqs[j].coord_index == sys->eqs[i].coord_index) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (equation_system_push(filtered, sys->eqs[i].poly, sys->eqs[i].var_node_id,
                                         sys->eqs[i].coord_index) != 0) {
                    dirty_set_free(&related);
                    return;
                }
            }
        }
    }
    dirty_set_free(&related);
}

/* ================================================================== */
/*  内部: 依赖传播 (增量求解支持)                                       */
/* ================================================================== */

static void propagate_dependency(const ConstraintGraph *graph, int var_id, bool *affected) {
    if (!graph || !affected || var_id < 0)
        return;

    int alloc_size = graph->node_count;
    int *queue = lv_calloc((size_t) alloc_size, sizeof(int));
    if (!queue) {
        return;
    }
    int queue_head = 0, queue_tail = 0;

    int start_idx = -1;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id == var_id) {
            start_idx = i;
            break;
        }
    }
    if (start_idx < 0) {
        lv_free((void **) &queue);
        return;
    }

    affected[start_idx] = true;
    queue[queue_tail++] = start_idx;

    while (queue_head < queue_tail) {
        int cur_idx = queue[queue_head++];
        int cur_id = graph->nodes[cur_idx]->id;

        for (int ci = 0; ci < graph->constraint_count; ci++) {
            const Constraint *c = graph->constraints[ci];
            if (!c)
                continue;

            bool participates = false;
            for (int p = 0; p < c->participant_count; p++) {
                if (c->participants[p] == cur_id) {
                    participates = true;
                    break;
                }
            }
            if (!participates)
                continue;

            for (int p = 0; p < c->participant_count; p++) {
                int pid = c->participants[p];
                for (int ni = 0; ni < graph->node_count; ni++) {
                    if (graph->nodes[ni]->id == pid && !affected[ni]) {
                        affected[ni] = true;
                        if (queue_tail < alloc_size) {
                            queue[queue_tail++] = ni;
                        }
                        break;
                    }
                }
            }
        }
    }

    lv_free((void **) &queue);
}

/* ================================================================== */
/*  PUBLIC API: solver_incremental_solve                               */
/* ================================================================== */

GroebnerResult *solver_incremental_solve(ConstraintGraph *graph, const int *dirty_var_ids, int n_dirty_vars) {
    if (!graph)
        return NULL;

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_START;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = n_dirty_vars;
        ev.description = "增量求解开始";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_inc;
        lv_SAFE_SNPRINTF(_snw_inc, detail, sizeof(detail), "{\"phase\":\"incremental\",\"dirty_count\":%d}",
                         n_dirty_vars);
        lv_UNUSED(_snw_inc);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    if (!dirty_var_ids || n_dirty_vars <= 0) {
        /* forward declaration assumed in solver.c: */
        extern SolverStatus solve_algebraic_system(ConstraintGraph *, const int *, int, GroebnerResult **);
        GroebnerResult *result = NULL;
        solve_algebraic_system(graph, NULL, 0, &result);
        return result;
    }

    bool *affected = lv_calloc((size_t) graph->node_count, sizeof(bool));
    for (int i = 0; i < n_dirty_vars; i++) {
        propagate_dependency(graph, dirty_var_ids[i], affected);
    }

    int *affected_ids = NULL;
    int affected_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        if (affected[i]) {
            int *new_ids = lv_realloc(affected_ids, (size_t) (affected_count + 1) * sizeof(int));
            if (new_ids) {
                affected_ids = new_ids;
                affected_ids[affected_count++] = graph->nodes[i]->id;
            }
        }
    }
    lv_free((void **) &affected);

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_PROGRESS;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.progress = 0.20;
        ev.step_number = affected_count;
        ev.description = "依赖传播完成";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_dep;
        lv_SAFE_SNPRINTF(_snw_dep, detail, sizeof(detail), "{\"phase\":\"dependency_propagation\",\"affected\":%d}",
                         affected_count);
        lv_UNUSED(_snw_dep);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    if (affected_count == 0 || !affected_ids) {
        lv_free((void **) &affected_ids);
        GroebnerResult *result = lv_calloc(1, sizeof(GroebnerResult));
        return result;
    }

    EquationSystem full_sys;
    equation_system_init(&full_sys);
    extract_equations_from_constraints(graph, &full_sys);

    DirtyVariableSet ds;
    dirty_set_init(&ds);
    for (int i = 0; i < affected_count; i++) {
        dirty_set_add(&ds, affected_ids[i]);
    }

    DirtyVariableSet expanded;
    dirty_set_init(&expanded);
    for (int i = 0; i < affected_count; i++) {
        dirty_set_add(&expanded, affected_ids[i]);
    }

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c)
            continue;
        bool involves_affected = false;
        for (int p = 0; p < c->participant_count; p++) {
            if (dirty_set_contains(&ds, c->participants[p])) {
                involves_affected = true;
                break;
            }
        }
        if (involves_affected) {
            for (int p = 0; p < c->participant_count; p++) {
                dirty_set_add(&expanded, c->participants[p]);
            }
        }
    }

    EquationSystem filtered_sys;
    filter_equations_for_dirty(&full_sys, &expanded, &filtered_sys);

    dirty_set_free(&ds);
    dirty_set_free(&expanded);
    equation_system_clear(&full_sys);
    lv_free((void **) &affected_ids);

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_PROGRESS;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.progress = 0.40;
        ev.step_number = filtered_sys.count;
        ev.description = "增量求解方程过滤完成";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_filt;
        lv_SAFE_SNPRINTF(_snw_filt, detail, sizeof(detail), "{\"phase\":\"filter\",\"filtered_eq_count\":%d}",
                         filtered_sys.count);
        lv_UNUSED(_snw_filt);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    GroebnerResult *result = lv_calloc(1, sizeof(GroebnerResult));
    if (!result)
        return NULL;
    result->solutions = NULL;
    result->solution_count = 0;
    result->unique = false;
    result->overdetermined = false;

    if (filtered_sys.count == 0) {
        equation_system_clear(&filtered_sys);
        return result;
    }

    bool has_oos = false;
    for (int i = 0; i < filtered_sys.count; i++) {
        if (filtered_sys.eqs[i].poly.degree > 2) {
            has_oos = true;
            break;
        }
    }

    int solved_count = 0;
    int multiple_solutions = 0;
    bool no_solution = false;

    if (!has_oos) {
        solve_equations_pass(&filtered_sys, result, &solved_count, &multiple_solutions, &no_solution, false);
    }

    if (no_solution) {
        cleanup_groebner_result(result);
    }

    if (!no_solution && !has_oos) {
        int remaining = 0;
        for (int i = 0; i < filtered_sys.count; i++) {
            if (filtered_sys.eqs[i].poly.degree >= 0)
                remaining++;
        }

        if (remaining > 0) {
            SolverStatus gb_status = groebner_basis_compute(&filtered_sys);
            if (gb_status != SOLVER_STATUS_OUT_OF_SCOPE) {
                solve_equations_pass(&filtered_sys, result, &solved_count, &multiple_solutions, &no_solution, false);
            }
        }
    }

    if (multiple_solutions > 0) {
        result->unique = false;
    } else if (solved_count > 0) {
        result->unique = true;
    }

    equation_system_clear(&filtered_sys);

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_DONE;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = solved_count;
        ev.description = multiple_solutions > 0 ? "增量求解完成: 多解"
                         : solved_count > 0     ? "增量求解完成: 唯一解"
                                                : "增量求解完成: 部分求解";
        char detail[SOLVER_DETAIL_BUF_SIZE];
        int _snw_inc_done;
        lv_SAFE_SNPRINTF(_snw_inc_done, detail, sizeof(detail),
                         "{\"phase\":\"incremental_done\",\"solved\":%d,\"multiple\":%d,\"unique\":%d}", solved_count,
                         multiple_solutions, result->unique ? 1 : 0);
        lv_UNUSED(_snw_inc_done);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    return result;
}
