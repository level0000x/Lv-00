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

#include "solver_common.h"

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

/** @brief solver 全局流式上下文定义（供所有 solver 模块通过 solver_types.h 的 extern 引用）
 * 注：setter solver_set_stream_context 定义在 solver.c（异文件），且本文件的
 * solver_set_stream_context_local 为 static 局部 setter，不适用 LV_STREAM_CTX_DEFINE 宏，保留手写。 */
lv_THREAD_LOCAL StreamContext *solver_stream_ctx = NULL;

static void solver_set_stream_context_local(StreamContext *ctx) {
    solver_stream_ctx = ctx;
}

static void solver_snapshot_rollback(ConstraintGraph *graph, SolverSnapshot *snap) {
    solver_snapshot_restore(graph, snap);
    solver_snapshot_free(snap);
}

/**
 * @brief 释放 solver_handle_multiple_solutions 输出的分支坐标数组
 *
 * 分支数组为扁平存储：valid_count 个分支 × 每分支 quadratic_count 个坐标。
 * solver_multibranch.c 内每分支坐标数（判别式 > 0 的二次方程数）会被截断到
 * 12（2^12 上限），此处用相同筛选条件在方程系统上重新统计，保证释放计数一致。
 */
static void solver_free_multiple_branches(const EquationSystem *sys, SymbolicCoord **branches, int valid_count) {
    if (!branches || valid_count <= 0)
        return;
    /* 每分支坐标数（判别式 > 0 的二次方程数）与 solver_multibranch.c 的分支
     * 收集共用 solver_count_positive_disc_quadratics 判定（solver_types.h），
     * 保证 scale 常量与判定语义单一来源，释放计数不会漂移。 */
    int per_branch = solver_count_positive_disc_quadratics(sys, lv_SOLVER_SCALE_FACTOR);
    if (per_branch > 12)
        per_branch = 12;
    for (int i = 0; i < valid_count * per_branch; i++) {
        symbolic_coord_destroy(branches[i]);
    }
    lv_free((void **) &branches);
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
            solver_snapshot_rollback(graph, &snapshot);
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
        solver_snapshot_rollback(graph, &snapshot);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    /* Step 4: Order variables by dependency */
    {
        int *all_var_ids = lv_calloc((size_t) eqs_count, sizeof(int));
        if (!all_var_ids) {
            equation_system_clear(&sys);
            *out_result = result;
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解错误: 内存分配失败", 0);
            solver_snapshot_rollback(graph, &snapshot);
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
                    solver_snapshot_rollback(graph, &snapshot);
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
        solver_snapshot_rollback(graph, &snapshot);
        return SOLVER_STATUS_NO_SOLUTION;
    }

    /* Step 7: Gröbner basis elimination */
    {
        int remaining_before_gb = 0;
        for (int i = 0; i < eqs_count; i++) {
            if (eqs_arr[i].poly.degree >= 0)
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
                char detail[lv_SOLVER_DETAIL_BUF_SIZE];
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
            for (int i = 0; i < eqs_count; i++) {
                if (eqs_arr[i].poly.degree >= 0 && eqs_arr[i].poly.degree > 4) {
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
                solver_snapshot_rollback(graph, &snapshot);
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
                solver_snapshot_rollback(graph, &snapshot);
                return SOLVER_STATUS_OUT_OF_SCOPE;
            }

            if (gb_status == SOLVER_STATUS_OK) {
                solve_equations_pass(&sys, result, &solved_count, &multiple_solutions, &no_solution, true);

                if (no_solution || check_contradiction_after_substitution(&sys)) {
                    cleanup_groebner_result(result);
                    equation_system_clear(&sys);
                    *out_result = result;
                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: Groebner基求解后无解", 0);
                    solver_snapshot_rollback(graph, &snapshot);
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
    for (int i = 0; i < eqs_count; i++) {
        if (eqs_arr[i].poly.degree >= 0)
            remaining++;
    }

    /* 注意：equation_system_clear(&sys) 已推迟到各返回分支内执行，
     * 因为多解分支处理（solver_handle_multiple_solutions）需要方程系统仍有效。 */

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_PROGRESS;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.progress = 1.0;
        ev.step_number = solved_count;
        ev.total_steps = solved_count + remaining;
        ev.description = "代数求解总结";
        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
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
        equation_system_clear(&sys);
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 未能求解任何变量", 0);
        solver_snapshot_rollback(graph, &snapshot);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    if (result->overdetermined) {
        equation_system_clear(&sys);
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 超定系统", 0);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_OVERCONSTRAINED;
    }

    if (multiple_solutions > 0) {
        /* 多解分支处理（任务2接线）：调用 solver_handle_multiple_solutions
         * 枚举二次方程多解分支（笛卡尔积，上限 2^12），过滤与其余方程矛盾的
         * 组合。分支坐标为 double 近似值，仅用于过滤与流式输出，正式精确解
         * 保留在 result->solutions 中（solve_equations_pass 产生的精确根），
         * 不覆盖。 */
        SymbolicCoord **branches = NULL;
        int branch_count = 0;
        SolverStatus br_status = solver_handle_multiple_solutions(result, &sys, &branches, &branch_count);

        if (br_status == SOLVER_STATUS_NO_SOLUTION) {
            /* 所有多解分支均与其余方程矛盾 → 无解 */
            cleanup_groebner_result(result);
            equation_system_clear(&sys);
            *out_result = result;
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "求解完成: 多解分支均矛盾，无解", 0);
            solver_snapshot_free(&snapshot);
            return SOLVER_STATUS_NO_SOLUTION;
        }

        if (br_status == SOLVER_STATUS_UNIQUE && branch_count > 0) {
            /* 多解分支过滤后仅剩唯一有效分支 → 修正为唯一解 */
            result->unique = true;
            solver_free_multiple_branches(&sys, branches, branch_count);
            equation_system_clear(&sys);
            *out_result = result;
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 多解分支过滤后唯一解", 0);
            solver_snapshot_free(&snapshot);
            return SOLVER_STATUS_UNIQUE;
        }

        /* br_status == OK（多个有效分支）或 UNIQUE 但无二次分支（如三次多解）
         * 或 TIMEOUT（资源受限）→ 保持多解状态 */
        solver_free_multiple_branches(&sys, branches, branch_count);
        equation_system_clear(&sys);
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 多解", 0);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_MULTIPLE;
    }

    if (solved_count > 0 && remaining == 0) {
        equation_system_clear(&sys);
        result->unique = true;
        *out_result = result;
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 唯一解", 0);
        solver_snapshot_free(&snapshot);
        return SOLVER_STATUS_UNIQUE;
    }

    equation_system_clear(&sys);
    *out_result = result;
    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "求解完成: 部分求解/欠定", 0);
    solver_snapshot_free(&snapshot);
    return SOLVER_STATUS_OK;
}
