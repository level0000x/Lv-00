/**
 * @file solver_incremental.c
 * @brief 增量求解（仅重新求解与脏变量相关的依赖子图）
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"
#include "lv/solver_dirty_set.h"
#include "lv/lv_graph_traversal.h"

/* 前向声明（实现在其他 solver 子模块中） */
void extract_equations_from_constraints(const ConstraintGraph *graph, EquationSystem *sys);
void solve_equations_pass(EquationSystem *sys, GroebnerResult *result, int *solved_count, int *multiple_solutions,
                          bool *no_solution, bool do_substitute);
void cleanup_groebner_result(GroebnerResult *result);

/* ================================================================== */
/*  内部: 依赖传播 (增量求解支持)                                       */
/* ================================================================== */

/* 依赖传播 BFS 的邻居枚举上下文（统一遍历设施 lv_bfs_run） */
typedef struct {
    const ConstraintGraph *graph;
} DependencyPropagateCtx;

/* BFS 邻居回调：产出与 node_id（节点索引）共享任一约束的所有节点索引
 * （含自身；visited 由驱动入队时去重，与原手写实现语义一致） */
static int dependency_propagate_neighbors_cb(void *ctx, int node_id, int batch_index,
                                             int *out_neighbors, void **out_edge_infos,
                                             int max_neighbors) {
    DependencyPropagateCtx *c = (DependencyPropagateCtx *) ctx;
    (void) batch_index;
    (void) out_edge_infos;
    if (!out_neighbors || max_neighbors <= 0)
        return 0;
    const ConstraintGraph *graph = c->graph;
    if (node_id < 0 || node_id >= graph->node_count)
        return 0;

    int cur_id = graph->nodes[node_id]->id;
    int count = 0;
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        const Constraint *con = graph->constraints[ci];
        if (!con)
            continue;
        bool participates = false;
        for (int p = 0; p < con->participant_count; p++) {
            if (con->participants[p] == cur_id) {
                participates = true;
                break;
            }
        }
        if (!participates)
            continue;
        for (int p = 0; p < con->participant_count; p++) {
            int pid = con->participants[p];
            for (int ni = 0; ni < graph->node_count; ni++) {
                if (graph->nodes[ni]->id == pid) {
                    if (count < max_neighbors)
                        out_neighbors[count] = ni;
                    count++;
                    break;
                }
            }
        }
    }
    return count < max_neighbors ? count : max_neighbors;
}

static void propagate_dependency(const ConstraintGraph *graph, int var_id, bool *affected) {
    if (!graph || !affected || var_id < 0)
        return;

    int start_idx = -1;
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]->id == var_id) {
            start_idx = i;
            break;
        }
    }
    if (start_idx < 0)
        return;

    /* 统一遍历设施 lv_bfs_run：seeds = {start_idx}，visited = affected
     * （mark_on_enqueue：入队时标记，避免重复入队；seed 即使已标记也重新入队，
     * 与原手写实现逐次独立传播的语义一致），邻居 = 共享任一约束的节点索引 */
    DependencyPropagateCtx bfs_ctx = { graph };
    lvBfsSpec spec = {
        .node_count = graph->node_count,
        .seeds = &start_idx,
        .seed_count = 1,
        .visited = affected,
        .mark_on_enqueue = true,
        .max_queue = 0,
        .neighbors = dependency_propagate_neighbors_cb,
        .visit = NULL,
        .ctx = &bfs_ctx,
    };
    (void) lv_bfs_run(&spec);
}

/* ================================================================== */
/*  PUBLIC API: solver_incremental_solve                               */
/* ================================================================== */

GroebnerResult *solver_incremental_solve(ConstraintGraph *graph, const int *dirty_var_ids, int n_dirty_vars) {
    if (!graph)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "graph is NULL");

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_SOLVE_START;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.step_number = n_dirty_vars;
        ev.description = "增量求解开始";
        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
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

    lvDArray affected_ids_arr;
    lv_darray_init(&affected_ids_arr, sizeof(int));
    for (int i = 0; i < graph->node_count; i++) {
        if (affected[i]) {
            int id = graph->nodes[i]->id;
            lv_darray_push(&affected_ids_arr, &id);
        }
    }
    int *affected_ids = (int *)affected_ids_arr.data;
    int affected_count = affected_ids_arr.count;
    lv_free((void **) &affected);

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_PROGRESS;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.progress = 0.20;
        ev.step_number = affected_count;
        ev.description = "依赖传播完成";
        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
        int _snw_dep;
        lv_SAFE_SNPRINTF(_snw_dep, detail, sizeof(detail), "{\"phase\":\"dependency_propagation\",\"affected\":%d}",
                         affected_count);
        lv_UNUSED(_snw_dep);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    if (affected_count == 0 || !affected_ids) {
        lv_darray_free(&affected_ids_arr);
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
    lv_darray_free(&affected_ids_arr);

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_PROGRESS;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.progress = 0.40;
        ev.step_number = filtered_sys.eqs.count;
        ev.description = "增量求解方程过滤完成";
        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
        int _snw_filt;
        lv_SAFE_SNPRINTF(_snw_filt, detail, sizeof(detail), "{\"phase\":\"filter\",\"filtered_eq_count\":%d}",
                         filtered_sys.eqs.count);
        lv_UNUSED(_snw_filt);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    GroebnerResult *result = lv_calloc(1, sizeof(GroebnerResult));
    if (!result)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "result allocation failed");
    result->solutions = NULL;
    result->solution_count = 0;
    result->unique = false;
    result->overdetermined = false;

    if (filtered_sys.eqs.count == 0) {
        equation_system_clear(&filtered_sys);
        return result;
    }

    bool has_oos = false;
    for (int i = 0; i < filtered_sys.eqs.count; i++) {
        PolyEquation *eq = (PolyEquation *)lv_darray_get(&filtered_sys.eqs, i);
        if (eq && eq->poly.degree > 2) {
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
        for (int i = 0; i < filtered_sys.eqs.count; i++) {
            PolyEquation *eq = (PolyEquation *)lv_darray_get(&filtered_sys.eqs, i);
            if (eq && eq->poly.degree >= 0)
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
        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
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
