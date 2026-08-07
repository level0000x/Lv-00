/**
 * @file solver_eliminate.c
 * @brief 几何推理消元与超出范围分析
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"

/* 前向声明 */

void extract_equations_from_constraints(const ConstraintGraph *graph, EquationSystem *sys);
bool solve_linear(const mpz_poly_t *poly, double *x_out);
void substitute_solved(EquationSystem *sys, int var_node_id, int coord_index, double value);
bool try_factor_polynomial(const mpz_poly_t *poly, mpz_poly_t *factor1, mpz_poly_t *factor2);
bool point_coord(const GeomNode *pt, int idx, double *out);
SymbolicCoord *symbolic_coord_create_rational(int64_t num, uint64_t den);
void symbolic_coord_destroy(SymbolicCoord *coord);
char *mpz_poly_get_str(const mpz_poly_t *p);
bool debug_is_debug_mode(void);

/* 几何模板函数（在 solver_geom_templates.c 中） */
int template_similar_triangles(ConstraintGraph *graph, EquationSystem *sys);
int template_pythagorean(ConstraintGraph *graph, EquationSystem *sys);
int template_parallel_cut(const ConstraintGraph *graph, EquationSystem *sys);

/* ================================================================== */
/*  PUBLIC API: eliminate_geometry                                     */
/* ================================================================== */

SolverStatus eliminate_geometry(ConstraintGraph *graph, int target_var_id, const int *eliminate_ids, int elim_count) {
    lv_UNUSED(target_var_id);
    if (!graph || !eliminate_ids || elim_count <= 0)
        return SOLVER_STATUS_OK;

    EquationSystem sys;
    equation_system_init(&sys);
    extract_equations_from_constraints(graph, &sys);
    PolyEquation *const eqs_arr = (PolyEquation *)sys.eqs.data;
    const int eqs_count = sys.eqs.count;

    bool *is_linear = lv_calloc((size_t) eqs_count, sizeof(bool));
    for (int i = 0; i < eqs_count; i++) {
        is_linear[i] = (eqs_arr[i].poly.degree <= 1);
    }

    bool any_eliminated = false;
    bool out_of_scope_found = false;

    for (int e = 0; e < elim_count; e++) {
        int eid = eliminate_ids[e];
        bool found_linear = false;

        for (int i = 0; i < eqs_count; i++) {
            if (eqs_arr[i].var_node_id != eid)
                continue;
            if (!is_linear[i]) {
                if (eqs_arr[i].poly.degree > 2)
                    out_of_scope_found = true;
                continue;
            }

            double val;
            if (solve_linear(&eqs_arr[i].poly, &val)) {
                substitute_solved(&sys, eid, eqs_arr[i].coord_index, val);
                GeomNode *node = graph_get_node(graph, eid);
                if (node && node->coord_count > eqs_arr[i].coord_index) {
                    if (fabs(val) > 9.2e12) {
                        LOG_WARN("solver", "数值过大 (%.2f > 9.2e12)，使用近似有理数", val);
                        /* 降级：使用近似值创建坐标 */
                        SymbolicCoord *approx = symbolic_coord_create_rational(
                            (int64_t) (val * lv_SOLVER_SCALE_FACTOR), lv_SOLVER_SCALE_FACTOR);
                        if (approx) {
                            symbolic_coord_destroy(node->symbolic_coords[eqs_arr[i].coord_index]);
                            node->symbolic_coords[eqs_arr[i].coord_index] = approx;
                        }
                    } else {
                        SymbolicCoord *new_coord = symbolic_coord_create_rational(
                            (int64_t) (val * lv_SOLVER_SCALE_FACTOR), lv_SOLVER_SCALE_FACTOR);
                        if (new_coord) {
                            symbolic_coord_destroy(node->symbolic_coords[eqs_arr[i].coord_index]);
                            node->symbolic_coords[eqs_arr[i].coord_index] = new_coord;
                        }
                    }
                }
                found_linear = true;
                any_eliminated = true;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED, "变量解得 (几何消元)", eid);
            }
        }

        if (!found_linear) {
            for (int ci = 0; ci < graph->constraint_count; ci++) {
                Constraint *c = graph->constraints[ci];
                if (c->type != BETWEENNESS)
                    continue;
                if (c->participant_count < 3)
                    continue;
                if (c->participants[1] == eid) {
                    GeomNode *p1 = graph_get_node(graph, c->participants[0]);
                    GeomNode *p3 = graph_get_node(graph, c->participants[2]);
                    if (p1 && p3 && p1->coord_count >= 2 && p3->coord_count >= 2) {
                        double x1, y1, x3, y3;
                        if (point_coord(p1, 0, &x1) && point_coord(p1, 1, &y1) && point_coord(p3, 0, &x3) &&
                            point_coord(p3, 1, &y3)) {
                            GeomNode *target = graph_get_node(graph, eid);
                            if (target) {
                                lv_free((void **) &target->numeric_assumption_declaration);
                                char buf[lv_SOLVER_DETAIL_BUF_SIZE];
                                int _snw;
                                lv_SAFE_SNPRINTF(_snw, buf, sizeof(buf), "betweenness:p1=(%.6f,%.6f),p3=(%.6f,%.6f)",
                                                 x1, y1, x3, y3);
                                lv_UNUSED(_snw);
                                /* 手写 malloc+strlcpy 收敛为 lv_strdup */
                                target->numeric_assumption_declaration = lv_strdup(buf);
                            }
                        }
                    }
                }
            }
        }
    }

    lv_free((void **) &is_linear);
    equation_system_clear(&sys);

    {
        EquationSystem tmpl_sys;
        equation_system_init(&tmpl_sys);
        int tmpl_added = template_similar_triangles(graph, &tmpl_sys);
        tmpl_added += template_pythagorean(graph, &tmpl_sys);
        tmpl_added += template_parallel_cut(graph, &tmpl_sys);

        if (tmpl_added > 0) {
            PolyEquation *const tmpl_eqs = (PolyEquation *)tmpl_sys.eqs.data;
            const int tmpl_eqs_count = tmpl_sys.eqs.count;
            for (int e = 0; e < elim_count; e++) {
                int eid = eliminate_ids[e];
                for (int i = 0; i < tmpl_eqs_count; i++) {
                    if (tmpl_eqs[i].var_node_id != eid)
                        continue;
                    if (tmpl_eqs[i].poly.degree != 1)
                        continue;

                    double val;
                    if (solve_linear(&tmpl_eqs[i].poly, &val)) {
                        GeomNode *node = graph_get_node(graph, eid);
                        if (node && node->coord_count > tmpl_eqs[i].coord_index) {
                            if (fabs(val) > 9.2e12) {
                                LOG_WARN("solver", "数值过大 (%.2f > 9.2e12)，使用近似有理数", val);
                                /* 降级：使用近似值创建坐标 */
                                SymbolicCoord *approx = symbolic_coord_create_rational(
                                    (int64_t) (val * lv_SOLVER_SCALE_FACTOR), lv_SOLVER_SCALE_FACTOR);
                                if (approx) {
                                    symbolic_coord_destroy(node->symbolic_coords[tmpl_eqs[i].coord_index]);
                                    node->symbolic_coords[tmpl_eqs[i].coord_index] = approx;
                                    any_eliminated = true;
                                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED,
                                                       "变量解得 (几何模板)", eid);
                                }
                            } else {
                                SymbolicCoord *new_coord = symbolic_coord_create_rational(
                                    (int64_t) (val * lv_SOLVER_SCALE_FACTOR), lv_SOLVER_SCALE_FACTOR);
                                if (new_coord) {
                                    symbolic_coord_destroy(node->symbolic_coords[tmpl_eqs[i].coord_index]);
                                    node->symbolic_coords[tmpl_eqs[i].coord_index] = new_coord;
                                    any_eliminated = true;
                                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_VARIABLE_RESOLVED,
                                                       "变量解得 (几何模板)", eid);
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
        equation_system_clear(&tmpl_sys);
    }

    if (out_of_scope_found)
        return SOLVER_STATUS_OUT_OF_SCOPE;
    if (any_eliminated)
        return SOLVER_STATUS_OK;
    return SOLVER_STATUS_OK;
}

/* ================================================================== */
/*  PUBLIC API: analyze_out_of_scope                                   */
/* ================================================================== */

SolverStatus analyze_out_of_scope(const ConstraintGraph *graph, int var_id, char **suggestion) {
    if (!graph || !suggestion)
        return SOLVER_STATUS_OUT_OF_SCOPE;

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_INFO;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.var_id = var_id;
        ev.description = "开始超出代数范围分析";
        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
        int _snw_diag;
        lv_SAFE_SNPRINTF(_snw_diag, detail, sizeof(detail), "{\"phase\":\"analyze_out_of_scope\",\"var_id\":%d}",
                         var_id);
        lv_UNUSED(_snw_diag);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

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
        ev.var_id = var_id;
        ev.description = "方程系统已提取，开始诊断";
        stream_emit(solver_stream_ctx, &ev);
    }

    mpz_poly_t *target_poly = NULL;
    for (int i = 0; i < eqs_count; i++) {
        if (eqs_arr[i].var_node_id == var_id && eqs_arr[i].poly.degree > 3) {
            target_poly = &eqs_arr[i].poly;
            break;
        }
    }

    if (!target_poly) {
        *suggestion = lv_strdup_safe(
            "No single high-degree equation found for this variable. "
            "The system may be out of scope due to coupled nonlinear equations. "
            "Consider decomposing the construction into simpler sub-problems with auxiliary construction lines.");

        if (solver_stream_ctx) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_WARNING;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.var_id = var_id;
            ev.description = "超出范围分析：无单一高次方程，系统整体耦合非线性";
            ev.detail_json = "{\"diagnosis\":\"coupled_nonlinear\",\"resolvable\":false}";
            stream_emit(solver_stream_ctx, &ev);
        }

        equation_system_clear(&sys);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_INFO;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.var_id = var_id;
        ev.description = "发现高次方程，尝试因式分解";
        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
        int _snw_found;
        lv_SAFE_SNPRINTF(_snw_found, detail, sizeof(detail), "{\"degree\":%d,\"var_id\":%d}", target_poly->degree,
                         var_id);
        lv_UNUSED(_snw_found);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    mpz_poly_t factor1, factor2;
    bool factored = try_factor_polynomial(target_poly, &factor1, &factor2);

    if (factored) {
        char *f1_str = mpz_poly_get_str(&factor1);
        char *f2_str = mpz_poly_get_str(&factor2);

        *suggestion = lv_asprintf(
            "Polynomial factors into (%s) * (%s). "
            "Split into multiple quadratic steps with auxiliary lines: "
            "solve each factor separately and combine solutions. "
            "Each factor of degree <= 2 is within constructible scope.",
            f1_str, f2_str);

        if (solver_stream_ctx) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_INFO;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.var_id = var_id;
            ev.description = "因式分解成功，可通过拆分求解";
            char detail[lv_SOLVER_DETAIL_BUF_SIZE];
            /* 多项式字符串做 JSON 转义，防特殊字符破坏 detail_json */
            char esc1[lv_SOLVER_DETAIL_BUF_SIZE / 2], esc2[lv_SOLVER_DETAIL_BUF_SIZE / 2];
            lv_str_json_escape(f1_str ? f1_str : "", f1_str ? strlen(f1_str) : 0, esc1, sizeof(esc1));
            lv_str_json_escape(f2_str ? f2_str : "", f2_str ? strlen(f2_str) : 0, esc2, sizeof(esc2));
            int _snw_fact;
            lv_SAFE_SNPRINTF(_snw_fact, detail, sizeof(detail),
                             "{\"diagnosis\":\"factorable\",\"resolvable\":true,\"factor1\":\"%s\",\"factor2\":\"%s\"}",
                             esc1, esc2);
            lv_UNUSED(_snw_fact);
            ev.detail_json = detail;
            stream_emit(solver_stream_ctx, &ev);
        }

        lv_free((void **) &f1_str);
        lv_free((void **) &f2_str);
        mpz_poly_clear(&factor1);
        mpz_poly_clear(&factor2);
        equation_system_clear(&sys);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }

    if (target_poly->degree == 4) {
        bool biquadratic = true;
        for (int i = 1; i <= 3; i += 2) {
            if (mpz_cmp_si(target_poly->coeffs[i], 0) != 0) {
                biquadratic = false;
                break;
            }
        }
        if (biquadratic) {
            *suggestion = lv_strdup_safe(
                "Biquadratic equation detected (only even powers). "
                "Substitute u = x^2 to reduce to quadratic, solve for u, "
                "then take square roots. This is within constructible scope via two quadratic steps.");

            if (solver_stream_ctx) {
                StreamEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = STREAM_EVENT_INFO;
                ev.timestamp_ms = stream_timestamp_ms();
                ev.var_id = var_id;
                ev.description = "双二次方程，可通过变量替换 u=x² 归约为二次方程求解";
                ev.detail_json =
                    "{\"diagnosis\":\"biquadratic\",\"resolvable\":true,\"method\":\"substitute_u_equals_x_squared\"}";
                stream_emit(solver_stream_ctx, &ev);
            }

            equation_system_clear(&sys);
            return SOLVER_STATUS_OUT_OF_SCOPE;
        }
    }

    char *poly_str = mpz_poly_get_str(target_poly);
    size_t needed = 256 + strlen(poly_str);
    if (needed > INT_MAX || needed == 0) {
        lv_free((void **) &poly_str);
        return SOLVER_STATUS_OUT_OF_SCOPE;
    }
    *suggestion = lv_malloc(needed);
    int _snw2;
    lv_SAFE_SNPRINTF(_snw2, *suggestion, needed,
                     "Irreducible polynomial of degree %d with coefficients [%s]. "
                     "Exceeds quadratic coverage. Consider: (1) adding auxiliary construction lines "
                     "to decompose into quadratic sub-problems, (2) checking if the problem reduces "
                     "to a known unconstructible problem (angle trisection, cube duplication, circle squaring), "
                     "or (3) using numerical approximation with Neusis construction.",
                     target_poly->degree, poly_str);
    lv_UNUSED(_snw2);

    if (solver_stream_ctx) {
        StreamEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = STREAM_EVENT_ERROR;
        ev.timestamp_ms = stream_timestamp_ms();
        ev.var_id = var_id;
        ev.description = "不可约高次多项式，超出二次可构造范围";
        char detail[lv_SOLVER_DETAIL_BUF_SIZE];
        int _snw_irr;
        lv_SAFE_SNPRINTF(
            _snw_irr, detail, sizeof(detail),
            "{\"diagnosis\":\"irreducible_high_degree\",\"degree\":%d,\"polynomial\":\"%s\",\"resolvable\":false}",
            target_poly->degree, poly_str);
        lv_UNUSED(_snw_irr);
        ev.detail_json = detail;
        stream_emit(solver_stream_ctx, &ev);
    }

    lv_free((void **) &poly_str);
    equation_system_clear(&sys);
    return SOLVER_STATUS_OUT_OF_SCOPE;
}
