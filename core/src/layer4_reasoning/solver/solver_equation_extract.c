/**
 * @file solver_equation_extract.c
 * @brief 从约束图提取代数方程的增强版本
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"

/* ── 直线方程结构体 ── */
typedef struct {
    double a, b, c;
} LineEquation;

/* 前向声明 */

int coord_to_double(const SymbolicCoord *c, double *out);
bool coord_to_mpz_scaled(const SymbolicCoord *c, mpz_t result, int64_t scale);
void double_to_mpz_scaled(double val, mpz_t result, int64_t scale);
bool point_coord(const GeomNode *pt, int idx, double *out);
bool line_from_two_points(GeomNode *p1, GeomNode *p2, LineEquation *out);

/* ================================================================== */
/*  PUBLIC API: solver_extract_equations_full                          */
/* ================================================================== */

int solver_extract_equations_full(const ConstraintGraph *graph, EquationSystem *out_system) {
    if (!graph || !out_system)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "solver_extract_equations_full: graph or out_system is NULL");

    int count = 0;

    for (int ci = 0; ci < graph->constraint_count; ci++) {
        const Constraint *c = graph->constraints[ci];
        if (!c || c->participant_count < 2)
            continue;

        switch (c->type) {
            case INCIDENCE: {
                GeomNode *pt = graph_get_node(graph, c->participants[0]);
                GeomNode *line = graph_get_node(graph, c->participants[1]);
                if (!pt || !line)
                    break;
                if (line->type == GEOM_LINE_SEGMENT) {
                    double lx1, ly1, lx2, ly2;
                    bool got_coords = false;

                    if (line->coord_count >= 4) {
                        got_coords = coord_to_double(line->symbolic_coords[0], &lx1) &&
                                     coord_to_double(line->symbolic_coords[1], &ly1) &&
                                     coord_to_double(line->symbolic_coords[2], &lx2) &&
                                     coord_to_double(line->symbolic_coords[3], &ly2);
                    } else if (line->coord_count >= 2) {
                        GeomNode *ep1 = NULL, *ep2 = NULL;
                        for (int cj = 0; cj < graph->constraint_count; cj++) {
                            const Constraint *c2 = graph->constraints[cj];
                            if (c2->type != INCIDENCE || c2->participant_count < 2)
                                continue;
                            if (c2->participants[1] == line->id) {
                                GeomNode *candidate = graph_get_node(graph, c2->participants[0]);
                                if (candidate && candidate->type == GEOM_POINT) {
                                    if (!ep1)
                                        ep1 = candidate;
                                    else if (!ep2)
                                        ep2 = candidate;
                                }
                            }
                        }
                        if (ep1 && ep2) {
                            got_coords = point_coord(ep1, 0, &lx1) && point_coord(ep1, 1, &ly1) &&
                                         point_coord(ep2, 0, &lx2) && point_coord(ep2, 1, &ly2);
                        }
                    }

                    if (got_coords) {
                        double dx = lx2 - lx1;
                        double dy = ly2 - ly1;

                        int64_t scale = lv_SOLVER_SCALE_FACTOR;
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                            break;
                        }
                        mpz_init(poly.coeffs[1]);
                        mpz_init(poly.coeffs[0]);
                        double_to_mpz_scaled(-dy, poly.coeffs[1], scale);
                        double_to_mpz_scaled(dy * lx1 - dx * ly1, poly.coeffs[0], scale);
                        EQUATION_PUSH_OR_GOTO(out_system, poly, pt->id, 0, push_error);
                        mpz_poly_clear(&poly);
                        count++;
                        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                           "提取方程: 关联约束 (x)", count);

                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                            break;
                        }
                        mpz_init(poly.coeffs[1]);
                        mpz_init(poly.coeffs[0]);
                        double_to_mpz_scaled(dx, poly.coeffs[1], scale);
                        double_to_mpz_scaled(-dx * ly1 - dy * lx1, poly.coeffs[0], scale);
                        EQUATION_PUSH_OR_GOTO(out_system, poly, pt->id, 1, push_error);
                        mpz_poly_clear(&poly);
                        count++;
                        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                           "提取方程: 关联约束 (y)", count);
                    }
                }
                break;
            }

            case INTERSECTION: {
                if (c->participant_count < 3)
                    break;
                GeomNode *line1 = graph_get_node(graph, c->participants[0]);
                GeomNode *line2 = graph_get_node(graph, c->participants[1]);
                GeomNode *rpt = graph_get_node(graph, c->participants[2]);
                if (!line1 || !line2 || !rpt)
                    break;

                LineEquation le1, le2;
                bool got1 = false, got2 = false;

                if (line1->type == GEOM_LINE_SEGMENT && line1->coord_count >= 4) {
                    GeomNode ep1_storage, ep2_storage;
                    memset(&ep1_storage, 0, sizeof(GeomNode));
                    memset(&ep2_storage, 0, sizeof(GeomNode));
                    GeomNode *ep1 = &ep1_storage;
                    GeomNode *ep2 = &ep2_storage;
                    ep1->type = GEOM_POINT;
                    ep1->coord_count = 2;
                    ep1->symbolic_coords = &line1->symbolic_coords[0];
                    ep2->type = GEOM_POINT;
                    ep2->coord_count = 2;
                    ep2->symbolic_coords = &line1->symbolic_coords[2];
                    got1 = line_from_two_points(ep1, ep2, &le1);
                }
                if (line2->type == GEOM_LINE_SEGMENT && line2->coord_count >= 4) {
                    GeomNode ep1_storage, ep2_storage;
                    memset(&ep1_storage, 0, sizeof(GeomNode));
                    memset(&ep2_storage, 0, sizeof(GeomNode));
                    GeomNode *ep1 = &ep1_storage;
                    GeomNode *ep2 = &ep2_storage;
                    ep1->type = GEOM_POINT;
                    ep1->coord_count = 2;
                    ep1->symbolic_coords = &line2->symbolic_coords[0];
                    ep2->type = GEOM_POINT;
                    ep2->coord_count = 2;
                    ep2->symbolic_coords = &line2->symbolic_coords[2];
                    got2 = line_from_two_points(ep1, ep2, &le2);
                }

                if (got1 && got2) {
                    int64_t scale = lv_SOLVER_SCALE_FACTOR;
                    mpz_poly_t poly;
                    mpz_poly_init(&poly);
                    poly.degree = 1;
                    poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                    if (!poly.coeffs) {
                        mpz_poly_clear(&poly);
                        continue;
                    }
                    mpz_init(poly.coeffs[1]);
                    mpz_init(poly.coeffs[0]);
                    double_to_mpz_scaled(le1.a, poly.coeffs[1], scale);
                    double_to_mpz_scaled(le1.c, poly.coeffs[0], scale);
                    EQUATION_PUSH_OR_GOTO(out_system, poly, rpt->id, 0, push_error);
                    mpz_poly_clear(&poly);
                    count++;
                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                       "提取方程: 交点约束 (x)", count);

                    mpz_poly_init(&poly);
                    poly.degree = 1;
                    poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                    if (!poly.coeffs) {
                        mpz_poly_clear(&poly);
                        continue;
                    }
                    mpz_init(poly.coeffs[1]);
                    mpz_init(poly.coeffs[0]);
                    double_to_mpz_scaled(le2.a, poly.coeffs[1], scale);
                    double_to_mpz_scaled(le2.c, poly.coeffs[0], scale);
                    EQUATION_PUSH_OR_GOTO(out_system, poly, rpt->id, 1, push_error);
                    mpz_poly_clear(&poly);
                    count++;
                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                       "提取方程: 交点约束 (y)", count);
                }
                break;
            }

            case CONTAINMENT: {
                if (c->participant_count < 2)
                    break;
                GeomNode *inner = graph_get_node(graph, c->participants[0]);
                GeomNode *outer = graph_get_node(graph, c->participants[1]);
                if (!inner || !outer)
                    break;

                if (inner->type != GEOM_POINT || outer->type != GEOM_REGION)
                    break;
                if (outer->data.region.segment_count <= 0 || !outer->data.region.boundary_segments)
                    break;

                {
                    int64_t scale = lv_SOLVER_SCALE_FACTOR;
                    int seg_count = outer->data.region.segment_count;

                    for (int si = 0; si < seg_count; si++) {
                        GeomNode *seg = outer->data.region.boundary_segments[si];
                        if (!seg || seg->type != GEOM_LINE_SEGMENT)
                            continue;
                        if (seg->coord_count < 4 || !seg->symbolic_coords)
                            continue;

                        bool seg_is_rational = (seg->symbolic_coords[0] && seg->symbolic_coords[0]->type == RATIONAL &&
                                                seg->symbolic_coords[1] && seg->symbolic_coords[1]->type == RATIONAL &&
                                                seg->symbolic_coords[2] && seg->symbolic_coords[2]->type == RATIONAL &&
                                                seg->symbolic_coords[3] && seg->symbolic_coords[3]->type == RATIONAL);

                        if (seg_is_rational) {
                            mpz_t sx1_s, sy1_s, sx2_s, sy2_s;
                            mpz_init(sx1_s);
                            mpz_init(sy1_s);
                            mpz_init(sx2_s);
                            mpz_init(sy2_s);

                            if (coord_to_mpz_scaled(seg->symbolic_coords[0], sx1_s, scale) &&
                                coord_to_mpz_scaled(seg->symbolic_coords[1], sy1_s, scale) &&
                                coord_to_mpz_scaled(seg->symbolic_coords[2], sx2_s, scale) &&
                                coord_to_mpz_scaled(seg->symbolic_coords[3], sy2_s, scale)) {
                                mpz_t dx_s, dy_s;
                                mpz_init(dx_s);
                                mpz_init(dy_s);
                                mpz_sub(dx_s, sx2_s, sx1_s);
                                mpz_sub(dy_s, sy2_s, sy1_s);

                                mpz_t term1, term2;
                                mpz_init(term1);
                                mpz_init(term2);

                                {
                                    mpz_poly_t poly;
                                    mpz_poly_init(&poly);
                                    poly.degree = 1;
                                    poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                                    if (!poly.coeffs) {
                                        mpz_poly_clear(&poly);
                                    } else {
                                        mpz_init(poly.coeffs[1]);
                                        mpz_init(poly.coeffs[0]);
                                        mpz_set(poly.coeffs[1], dy_s);
                                        mpz_mul(term1, dy_s, sx1_s);
                                        mpz_mul(term2, dx_s, sy1_s);
                                        mpz_sub(poly.coeffs[0], term2, term1);
                                        EQUATION_PUSH_OR_GOTO(out_system, poly, inner->id, 0, push_error);
                                        mpz_poly_clear(&poly);
                                        count++;
                                        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                                           "提取方程: 包容约束 [精确] (x)", count);
                                    }
                                }

                                {
                                    mpz_poly_t poly;
                                    mpz_poly_init(&poly);
                                    poly.degree = 1;
                                    poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                                    if (!poly.coeffs) {
                                        mpz_poly_clear(&poly);
                                    } else {
                                        mpz_init(poly.coeffs[1]);
                                        mpz_init(poly.coeffs[0]);
                                        mpz_neg(poly.coeffs[1], dx_s);
                                        mpz_mul(term1, dy_s, sx1_s);
                                        mpz_mul(term2, dx_s, sy1_s);
                                        mpz_sub(poly.coeffs[0], term1, term2);
                                        EQUATION_PUSH_OR_GOTO(out_system, poly, inner->id, 1, push_error);
                                        mpz_poly_clear(&poly);
                                        count++;
                                        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                                           "提取方程: 包容约束 [精确] (y)", count);
                                    }
                                }
                                mpz_clear(term1);
                                mpz_clear(term2);
                                mpz_clear(dx_s);
                                mpz_clear(dy_s);
                            }
                            mpz_clear(sx1_s);
                            mpz_clear(sy1_s);
                            mpz_clear(sx2_s);
                            mpz_clear(sy2_s);
                        } else {
                            double sx1, sy1, sx2, sy2;
                            if (!coord_to_double(seg->symbolic_coords[0], &sx1) ||
                                !coord_to_double(seg->symbolic_coords[1], &sy1) ||
                                !coord_to_double(seg->symbolic_coords[2], &sx2) ||
                                !coord_to_double(seg->symbolic_coords[3], &sy2))
                                continue;

                            double dx = sx2 - sx1;
                            double dy = sy2 - sy1;

                            mpz_poly_t poly;
                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                            if (!poly.coeffs) {
                                mpz_poly_clear(&poly);
                                continue;
                            }
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            double_to_mpz_scaled(dy, poly.coeffs[1], scale);
                            double_to_mpz_scaled(-dy * sx1 + dx * sy1, poly.coeffs[0], scale);
                            EQUATION_PUSH_OR_GOTO(out_system, poly, inner->id, 0, push_error);
                            mpz_poly_clear(&poly);
                            count++;
                            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                               "提取方程: 包容约束 [近似] (x)", count);

                            mpz_poly_init(&poly);
                            poly.degree = 1;
                            poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                            if (!poly.coeffs) {
                                mpz_poly_clear(&poly);
                                continue;
                            }
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            double_to_mpz_scaled(-dx, poly.coeffs[1], scale);
                            double_to_mpz_scaled(dy * sx1 - dx * sy1, poly.coeffs[0], scale);
                            EQUATION_PUSH_OR_GOTO(out_system, poly, inner->id, 1, push_error);
                            mpz_poly_clear(&poly);
                            count++;
                            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                               "提取方程: 包容约束 [近似] (y)", count);
                        }
                    }
                }
                break;
            }

            case BETWEENNESS: {
                if (c->participant_count < 3)
                    break;
                GeomNode *p1 = graph_get_node(graph, c->participants[0]);
                GeomNode *p2 = graph_get_node(graph, c->participants[1]);
                GeomNode *p3 = graph_get_node(graph, c->participants[2]);
                if (!p1 || !p2 || !p3)
                    break;

                if (!p1->symbolic_coords || !p3->symbolic_coords)
                    break;

                bool exact_mode = false;
                mpz_t dx13_s, dy13_s, x1_s, y1_s;
                if (p1->symbolic_coords[0] && p1->symbolic_coords[0]->type == RATIONAL && p1->symbolic_coords[1] &&
                    p1->symbolic_coords[1]->type == RATIONAL && p3->symbolic_coords[0] &&
                    p3->symbolic_coords[0]->type == RATIONAL && p3->symbolic_coords[1] &&
                    p3->symbolic_coords[1]->type == RATIONAL) {
                    int64_t scale = lv_SOLVER_SCALE_FACTOR;
                    mpz_init(dx13_s);
                    mpz_init(dy13_s);
                    mpz_init(x1_s);
                    mpz_init(y1_s);

                    if (coord_to_mpz_scaled(p1->symbolic_coords[0], x1_s, scale) &&
                        coord_to_mpz_scaled(p1->symbolic_coords[1], y1_s, scale)) {
                        mpz_t x3_s, y3_s;
                        mpz_init(x3_s);
                        mpz_init(y3_s);

                        if (coord_to_mpz_scaled(p3->symbolic_coords[0], x3_s, scale) &&
                            coord_to_mpz_scaled(p3->symbolic_coords[1], y3_s, scale)) {
                            mpz_sub(dx13_s, x3_s, x1_s);
                            mpz_sub(dy13_s, y3_s, y1_s);

                            mpz_t term1, term2;
                            mpz_init(term1);
                            mpz_init(term2);

                            {
                                mpz_poly_t poly;
                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                                if (!poly.coeffs) {
                                    mpz_poly_clear(&poly);
                                } else {
                                    mpz_init(poly.coeffs[1]);
                                    mpz_init(poly.coeffs[0]);
                                    mpz_set(poly.coeffs[1], dy13_s);
                                    mpz_mul(term1, dx13_s, y1_s);
                                    mpz_mul(term2, dy13_s, x1_s);
                                    mpz_sub(poly.coeffs[0], term1, term2);
                                    EQUATION_PUSH_OR_GOTO(out_system, poly, p2->id, 0, push_error);
                                    mpz_poly_clear(&poly);
                                    count++;
                                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                                       "提取方程: 介于约束 [精确] (x)", count);
                                }
                            }

                            {
                                mpz_poly_t poly;
                                mpz_poly_init(&poly);
                                poly.degree = 1;
                                poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                                if (!poly.coeffs) {
                                    mpz_poly_clear(&poly);
                                } else {
                                    mpz_init(poly.coeffs[1]);
                                    mpz_init(poly.coeffs[0]);
                                    mpz_neg(poly.coeffs[1], dx13_s);
                                    mpz_mul(term1, dy13_s, x1_s);
                                    mpz_mul(term2, dx13_s, y1_s);
                                    mpz_sub(poly.coeffs[0], term1, term2);
                                    EQUATION_PUSH_OR_GOTO(out_system, poly, p2->id, 1, push_error);
                                    mpz_poly_clear(&poly);
                                    count++;
                                    stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                                       "提取方程: 介于约束 [精确] (y)", count);
                                }
                            }

                            mpz_clear(term1);
                            mpz_clear(term2);
                            exact_mode = true;
                        }
                        mpz_clear(x3_s);
                        mpz_clear(y3_s);
                    }
                    if (!exact_mode) {
                        mpz_clear(dx13_s);
                        mpz_clear(dy13_s);
                        mpz_clear(x1_s);
                        mpz_clear(y1_s);
                    }
                }

                if (!exact_mode) {
                    double x1, y1, x2, y2, x3, y3;
                    bool ok = point_coord(p1, 0, &x1) && point_coord(p1, 1, &y1) && point_coord(p3, 0, &x3) &&
                              point_coord(p3, 1, &y3);
                    if (ok) {
                        double dx13 = x3 - x1;
                        double dy13 = y3 - y1;
                        int64_t scale = lv_SOLVER_SCALE_FACTOR;

                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                        } else {
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            double_to_mpz_scaled(dy13, poly.coeffs[1], scale);
                            double_to_mpz_scaled(dx13 * y1 - dy13 * x1, poly.coeffs[0], scale);
                            EQUATION_PUSH_OR_GOTO(out_system, poly, p2->id, 0, push_error);
                            mpz_poly_clear(&poly);
                            count++;
                            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                               "提取方程: 介于约束 [近似] (x)", count);
                        }

                        mpz_poly_init(&poly);
                        poly.degree = 1;
                        poly.coeffs = lv_malloc(2 * sizeof(mpz_t));
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                        } else {
                            mpz_init(poly.coeffs[1]);
                            mpz_init(poly.coeffs[0]);
                            double_to_mpz_scaled(-dx13, poly.coeffs[1], scale);
                            double_to_mpz_scaled(-dx13 * y1 + dy13 * x1, poly.coeffs[0], scale);
                            EQUATION_PUSH_OR_GOTO(out_system, poly, p2->id, 1, push_error);
                            mpz_poly_clear(&poly);
                            count++;
                            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED,
                                               "提取方程: 介于约束 [近似] (y)", count);
                        }
                    }
                }
                break;
            }

            case ANGLE: {
                if (c->participant_count < 2)
                    break;
                /* angle(line1, line2): angle between two segments */
                count++;
                break;
            }

            case CONNECTION: {
                if (c->participant_count < 2)
                    break;
                GeomNode *nodeA = graph_get_node(graph, c->participants[0]);
                GeomNode *nodeB = graph_get_node(graph, c->participants[1]);
                if (!nodeA || !nodeB)
                    break;

                double dist_val = -1.0;
                GeomNode *dist_node = NULL;
                const char *prefix = "distance=";
                size_t prefix_len = strlen(prefix);
                for (int ni = 0; ni < 2; ni++) {
                    GeomNode *n = (ni == 0) ? nodeA : nodeB;
                    if (!n || !n->numeric_assumption_declaration)
                        continue;
                    const char *decl = n->numeric_assumption_declaration;
                    if (strncmp(decl, prefix, prefix_len) == 0) {
                        dist_val = strtod(decl + prefix_len, NULL);
                        dist_node = n;
                        break;
                    }
                }

                if (dist_val < 0)
                    break;

                if (nodeA->coord_count < 2 || nodeB->coord_count < 2)
                    break;
                if (!nodeA->symbolic_coords || !nodeB->symbolic_coords)
                    break;

                double ax, ay, bx, by;
                if (!coord_to_double(nodeA->symbolic_coords[0], &ax) ||
                    !coord_to_double(nodeA->symbolic_coords[1], &ay) ||
                    !coord_to_double(nodeB->symbolic_coords[0], &bx) ||
                    !coord_to_double(nodeB->symbolic_coords[1], &by))
                    break;

                double dist_sq = dist_val * dist_val;
                int64_t scale = lv_SOLVER_SCALE_FACTOR;

                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 2;
                poly.coeffs = lv_malloc(3 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    break;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * ax, poly.coeffs[1], scale);
                double_to_mpz_scaled(ax * ax + ay * ay - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(out_system, poly, nodeB->id, 0, push_error);
                mpz_poly_clear(&poly);
                count++;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED, "提取方程: 连接约束 (x)",
                                   count);

                mpz_poly_init(&poly);
                poly.degree = 2;
                poly.coeffs = lv_malloc(3 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    break;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * ay, poly.coeffs[1], scale);
                double_to_mpz_scaled(ax * ax + ay * ay - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(out_system, poly, nodeB->id, 1, push_error);
                mpz_poly_clear(&poly);
                count++;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED, "提取方程: 连接约束 (y)",
                                   count);
                break;
            }
            default:
                lv_LOG_WARNING("Unknown constraint type %d in solver_extract_equations", c->type);
                break;
        }
    }

    /* Second pass: extract distance constraints from nodes */
    for (int ni = 0; ni < graph->node_count; ni++) {
        GeomNode *node = graph->nodes[ni];
        if (!node || !node->numeric_assumption_declaration)
            continue;
        if (node->type != GEOM_LINE_SEGMENT)
            continue;

        const char *decl = node->numeric_assumption_declaration;
        double dist_sq = -1.0;

        const char *prefix = "distance=";
        size_t prefix_len = strlen(prefix);
        if (strncmp(decl, prefix, prefix_len) == 0) {
            dist_sq = strtod(decl + prefix_len, NULL);
            dist_sq = dist_sq * dist_sq;
        } else {
            char *end = NULL;
            double val = strtod(decl, &end);
            if (end != decl && val >= 0) {
                dist_sq = val;
            }
        }

        if (dist_sq < 0)
            continue;

        if (node->coord_count >= 4) {
            double x1, y1;
            if (coord_to_double(node->symbolic_coords[0], &x1) && coord_to_double(node->symbolic_coords[1], &y1)) {
                int64_t scale = lv_SOLVER_SCALE_FACTOR;

                mpz_poly_t poly;
                mpz_poly_init(&poly);
                poly.degree = 2;
                poly.coeffs = lv_malloc(3 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    continue;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * x1, poly.coeffs[1], scale);
                double_to_mpz_scaled(x1 * x1 + y1 * y1 - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(out_system, poly, node->id, 0, push_error);
                mpz_poly_clear(&poly);
                count++;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED, "提取方程: 距离约束 (x)",
                                   count);

                mpz_poly_init(&poly);
                poly.degree = 2;
                poly.coeffs = lv_malloc(3 * sizeof(mpz_t));
                if (!poly.coeffs) {
                    mpz_poly_clear(&poly);
                    continue;
                }
                mpz_init(poly.coeffs[2]);
                mpz_init(poly.coeffs[1]);
                mpz_init(poly.coeffs[0]);
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * y1, poly.coeffs[1], scale);
                double_to_mpz_scaled(x1 * x1 + y1 * y1 - dist_sq, poly.coeffs[0], scale);
                EQUATION_PUSH_OR_GOTO(out_system, poly, node->id, 1, push_error);
                mpz_poly_clear(&poly);
                count++;
                stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_EQUATION_EXTRACTED, "提取方程: 距离约束 (y)",
                                   count);
            }
        }
    }

    return count;
push_error:
    lv_RETURN_ERROR(lv_ERROR_INTERNAL, "solver_extract_equations_full: push failed");
}
