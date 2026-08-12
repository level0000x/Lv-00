/**
 * @file solver_geom_templates.c
 * @brief 几何推理消元模板
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 *          包含相似三角形、勾股定理、平行线截割等几何模板。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"
#include "lv/geo_utils.h"

/* 前向声明 */

int coord_to_double(const SymbolicCoord *c, double *out);
void double_to_mpz_scaled(double val, mpz_t result, int64_t scale);
bool point_coord(const GeomNode *pt, int idx, double *out);
int count_point_variables(const ConstraintGraph *graph, int **out_ids);

/* ================================================================== */
/*  内部: 线段收集骨架（相似三角形/勾股/平行截割三模板共用）           */
/* ================================================================== */

typedef struct {
    int id;
    int p1, p2;
    double dx, dy;
} SegInfo;

static void collect_segment_incidence(const ConstraintGraph *graph, SegInfo *segs, int *count, bool with_direction) {
    int seg_count = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *n = graph->nodes[i];
        if (n && n->type == GEOM_LINE_SEGMENT && n->coord_count >= 4) {
            segs[seg_count].id = n->id;
            segs[seg_count].p1 = -1;
            segs[seg_count].p2 = -1;
            if (with_direction) {
                segs[seg_count].dx = 0;
                segs[seg_count].dy = 0;
            }
            seg_count++;
        }
    }
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (c->type != INCIDENCE || c->participant_count < 2)
            continue;
        int pt_id = c->participants[0];
        int seg_id = c->participants[1];
        for (int s = 0; s < seg_count; s++) {
            if (segs[s].id == seg_id) {
                if (segs[s].p1 < 0)
                    segs[s].p1 = pt_id;
                else if (segs[s].p2 < 0)
                    segs[s].p2 = pt_id;
                break;
            }
        }
    }
    *count = seg_count;
}

/* ================================================================== */
/*  内部: 几何推理消元模板 - 相似三角形                                 */
/* ================================================================== */

int template_similar_triangles(ConstraintGraph *graph, EquationSystem *sys) {
    if (!graph || !sys)
        return 0;
    int added = 0;
    int *point_ids = NULL;
    int pt_count = count_point_variables(graph, &point_ids);

    SegInfo *segs = lv_calloc((size_t) graph->node_count, sizeof(SegInfo));
    if (!segs) {
        lv_free((void **) &point_ids);
        return 0;
    }
    int seg_count = 0;
    collect_segment_incidence(graph, segs, &seg_count, false);

    for (int a = 0; a < pt_count && added < 10; a++) {
        for (int b = a + 1; b < pt_count && added < 10; b++) {
            for (int c = b + 1; c < pt_count && added < 10; c++) {
                int pa = point_ids[a], pb = point_ids[b], pc = point_ids[c];
                bool has_ab = false, has_bc = false, has_ac = false;
                for (int s = 0; s < seg_count; s++) {
                    if ((segs[s].p1 == pa && segs[s].p2 == pb) || (segs[s].p1 == pb && segs[s].p2 == pa))
                        has_ab = true;
                    if ((segs[s].p1 == pb && segs[s].p2 == pc) || (segs[s].p1 == pc && segs[s].p2 == pb))
                        has_bc = true;
                    if ((segs[s].p1 == pa && segs[s].p2 == pc) || (segs[s].p1 == pc && segs[s].p2 == pa))
                        has_ac = true;
                }
                if (!has_ab || !has_bc || !has_ac)
                    continue;

                GeomNode *nodeA = graph_get_node(graph, pa);
                GeomNode *nodeB = graph_get_node(graph, pb);
                GeomNode *nodeC = graph_get_node(graph, pc);
                if (!nodeA || !nodeB || !nodeC)
                    continue;

                double xa, ya, xb, yb, xc, yc;
                bool has_coords = point_coord(nodeA, 0, &xa) && point_coord(nodeA, 1, &ya) &&
                                  point_coord(nodeB, 0, &xb) && point_coord(nodeB, 1, &yb) &&
                                  point_coord(nodeC, 0, &xc) && point_coord(nodeC, 1, &yc);
                if (!has_coords)
                    continue;

                double ab_len = geo_distance_2d(xa, ya, xb, yb);
                double bc_len = geo_distance_2d(xb, yb, xc, yc);
                double ac_len = geo_distance_2d(xa, ya, xc, yc);
                if (ab_len < lv_EPSILON_DOUBLE || bc_len < lv_EPSILON_DOUBLE || ac_len < lv_EPSILON_DOUBLE)
                    continue;

                int64_t scale = lv_SOLVER_SCALE_FACTOR;
                mpz_t ab2_mpz, bc2_mpz, ac2_mpz;
                mpz_init(ab2_mpz);
                mpz_init(bc2_mpz);
                mpz_init(ac2_mpz);
                double_to_mpz_scaled(ab_len * ab_len, ab2_mpz, scale);
                double_to_mpz_scaled(bc_len * bc_len, bc2_mpz, scale);
                double_to_mpz_scaled(ac_len * ac_len, ac2_mpz, scale);
                mpz_clear(ab2_mpz);
                mpz_clear(bc2_mpz);
                mpz_clear(ac2_mpz);
            }
        }
    }

    lv_free((void **) &point_ids);
    lv_free((void **) &segs);
    return added;
}

/* ================================================================== */
/*  内部: 几何推理消元模板 - 勾股定理                                   */
/* ================================================================== */

int template_pythagorean(ConstraintGraph *graph, EquationSystem *sys) {
    if (!graph || !sys)
        return 0;
    int added = 0;
    int *point_ids = NULL;
    int pt_count = count_point_variables(graph, &point_ids);

    SegInfo *segs = lv_calloc((size_t) graph->node_count, sizeof(SegInfo));
    if (!segs) {
        lv_free((void **) &point_ids);
        return 0;
    }
    int seg_count = 0;
    collect_segment_incidence(graph, segs, &seg_count, false);

    for (int a = 0; a < pt_count && added < 10; a++) {
        for (int b = a + 1; b < pt_count && added < 10; b++) {
            for (int c = b + 1; c < pt_count && added < 10; c++) {
                int pa = point_ids[a], pb = point_ids[b], pc = point_ids[c];
                bool has_ab = false, has_bc = false, has_ac = false;
                for (int s = 0; s < seg_count; s++) {
                    if ((segs[s].p1 == pa && segs[s].p2 == pb) || (segs[s].p1 == pb && segs[s].p2 == pa))
                        has_ab = true;
                    if ((segs[s].p1 == pb && segs[s].p2 == pc) || (segs[s].p1 == pc && segs[s].p2 == pb))
                        has_bc = true;
                    if ((segs[s].p1 == pa && segs[s].p2 == pc) || (segs[s].p1 == pc && segs[s].p2 == pa))
                        has_ac = true;
                }
                if (!has_ab || !has_bc || !has_ac)
                    continue;

                GeomNode *nodeA = graph_get_node(graph, pa);
                GeomNode *nodeB = graph_get_node(graph, pb);
                GeomNode *nodeC = graph_get_node(graph, pc);
                if (!nodeA || !nodeB || !nodeC)
                    continue;

                double xa, ya, xb, yb, xc, yc;
                bool has_coords = point_coord(nodeA, 0, &xa) && point_coord(nodeA, 1, &ya) &&
                                  point_coord(nodeB, 0, &xb) && point_coord(nodeB, 1, &yb) &&
                                  point_coord(nodeC, 0, &xc) && point_coord(nodeC, 1, &yc);
                if (!has_coords)
                    continue;

                double cax = xa - xc, cay = ya - yc;
                double cbx = xb - xc, cby = yb - yc;
                double dot = cax * cbx + cay * cby;

                if (fabs(dot) < lv_EPSILON_LOW * (geo_norm_2d(cax, cay) * geo_norm_2d(cbx, cby) + 1.0)) {
                    int64_t scale = lv_SOLVER_SCALE_FACTOR;
                    mpz_t ab2_x_mpz, ab2_y_mpz;
                    mpz_init(ab2_x_mpz);
                    mpz_init(ab2_y_mpz);
                    double_to_mpz_scaled((xa - xb) * (xa - xb), ab2_x_mpz, scale);
                    double_to_mpz_scaled((ya - yb) * (ya - yb), ab2_y_mpz, scale);
                    mpz_t ac2_x_mpz, ac2_y_mpz;
                    mpz_init(ac2_x_mpz);
                    mpz_init(ac2_y_mpz);
                    double_to_mpz_scaled((xa - xc) * (xa - xc), ac2_x_mpz, scale);
                    double_to_mpz_scaled((ya - yc) * (ya - yc), ac2_y_mpz, scale);
                    mpz_t bc2_x_mpz, bc2_y_mpz;
                    mpz_init(bc2_x_mpz);
                    mpz_init(bc2_y_mpz);
                    double_to_mpz_scaled((xb - xc) * (xb - xc), bc2_x_mpz, scale);
                    double_to_mpz_scaled((yb - yc) * (yb - yc), bc2_y_mpz, scale);

                    mpz_t lhs_mpz, rhs_mpz, diff_mpz;
                    mpz_init(lhs_mpz);
                    mpz_init(rhs_mpz);
                    mpz_init(diff_mpz);
                    mpz_add(lhs_mpz, ab2_x_mpz, ab2_y_mpz);
                    mpz_add(rhs_mpz, ac2_x_mpz, ac2_y_mpz);
                    mpz_add(rhs_mpz, rhs_mpz, bc2_x_mpz);
                    mpz_add(rhs_mpz, rhs_mpz, bc2_y_mpz);
                    mpz_sub(diff_mpz, lhs_mpz, rhs_mpz);
                    mpz_abs(diff_mpz, diff_mpz);

                    mpz_t threshold;
                    mpz_init(threshold);
                    mpz_set_si(threshold, scale * 10);
                    if (mpz_cmp(diff_mpz, threshold) < 0) {
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 0;
                        poly.coeffs = lv_malloc(sizeof(mpz_t));
                        if (poly.coeffs) {
                            mpz_init_set(poly.coeffs[0], diff_mpz);
                            mpz_poly_clear(&poly);
                            added++;
                        } else {
                            mpz_poly_clear(&poly);
                        }
                    }

                    mpz_clear(ab2_x_mpz);
                    mpz_clear(ab2_y_mpz);
                    mpz_clear(ac2_x_mpz);
                    mpz_clear(ac2_y_mpz);
                    mpz_clear(bc2_x_mpz);
                    mpz_clear(bc2_y_mpz);
                    mpz_clear(lhs_mpz);
                    mpz_clear(rhs_mpz);
                    mpz_clear(diff_mpz);
                    mpz_clear(threshold);
                }
            }
        }
    }

    lv_free((void **) &point_ids);
    lv_free((void **) &segs);
    return added;
}

/* ================================================================== */
/*  内部: 几何推理消元模板 - 平行线截割 (Parallel cut)                  */
/* ================================================================== */

int template_parallel_cut(const ConstraintGraph *graph, EquationSystem *sys) {
    int added = 0;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *li = graph->nodes[i];
        if (li->type != GEOM_LINE_SEGMENT || li->coord_count < 4)
            continue;

        double x1_i, y1_i, x2_i, y2_i;
        if (!coord_to_double(li->symbolic_coords[0], &x1_i) || !coord_to_double(li->symbolic_coords[1], &y1_i) ||
            !coord_to_double(li->symbolic_coords[2], &x2_i) || !coord_to_double(li->symbolic_coords[3], &y2_i))
            continue;

        double dx_i = x2_i - x1_i, dy_i = y2_i - y1_i;
        double len_i = geo_norm_2d(dx_i, dy_i);
        if (len_i < lv_EPSILON_DOUBLE)
            continue;
        double nx_i = dx_i / len_i, ny_i = dy_i / len_i;

        for (int j = i + 1; j < graph->node_count; j++) {
            GeomNode *lj = graph->nodes[j];
            if (lj->type != GEOM_LINE_SEGMENT || lj->coord_count < 4)
                continue;

            double x1_j, y1_j, x2_j, y2_j;
            if (!coord_to_double(lj->symbolic_coords[0], &x1_j) || !coord_to_double(lj->symbolic_coords[1], &y1_j) ||
                !coord_to_double(lj->symbolic_coords[2], &x2_j) || !coord_to_double(lj->symbolic_coords[3], &y2_j))
                continue;

            double dx_j = x2_j - x1_j, dy_j = y2_j - y1_j;
            double len_j = geo_norm_2d(dx_j, dy_j);
            if (len_j < lv_EPSILON_DOUBLE)
                continue;
            double nx_j = dx_j / len_j, ny_j = dy_j / len_j;

            double cross = nx_i * ny_j - ny_i * nx_j;
            if (fabs(cross) > lv_EPSILON_LOW)
                continue;

            for (int ci = 0; ci < graph->constraint_count; ci++) {
                Constraint *c = graph->constraints[ci];
                if (c->type != INCIDENCE || c->participant_count < 2)
                    continue;

                GeomNode *pt = graph_get_node(graph, c->participants[0]);
                GeomNode *seg = graph_get_node(graph, c->participants[1]);
                if (!pt || pt->type != GEOM_POINT)
                    continue;

                int on_line_i = (seg && seg->id == li->id);
                int on_line_j = (seg && seg->id == lj->id);
                if (!on_line_i && !on_line_j)
                    continue;

                if (on_line_i && pt->coord_count >= 2) {
                    for (int cj = ci + 1; cj < graph->constraint_count; cj++) {
                        Constraint *c2 = graph->constraints[cj];
                        if (c2->type != INCIDENCE || c2->participant_count < 2)
                            continue;

                        GeomNode *pt2 = graph_get_node(graph, c2->participants[0]);
                        GeomNode *seg2 = graph_get_node(graph, c2->participants[1]);
                        if (!pt2 || pt2->type != GEOM_POINT || pt2->coord_count < 2)
                            continue;
                        if (!seg2 || seg2->id != lj->id)
                            continue;

                        double px, py;
                        if (!coord_to_double(pt->symbolic_coords[0], &px) ||
                            !coord_to_double(pt->symbolic_coords[1], &py))
                            continue;
                        double t_i = ((px - x1_i) * dx_i + (py - y1_i) * dy_i) / (len_i * len_i);

                        double px2, py2;
                        if (!coord_to_double(pt2->symbolic_coords[0], &px2) ||
                            !coord_to_double(pt2->symbolic_coords[1], &py2))
                            continue;
                        double t_j = ((px2 - x1_j) * dx_j + (py2 - y1_j) * dy_j) / (len_j * len_j);

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

                        double coeff = t_i * len_j - t_j * len_i;
                        if (fabs(coeff) > lv_EPSILON_NUMERIC_COMPARE) {
                            double_to_mpz_scaled(len_j, poly.coeffs[1], scale);
                            double_to_mpz_scaled(-t_j * len_i, poly.coeffs[0], scale);
                            if (lv_equation_push_checked(sys, poly, pt->id, 0) != 0) goto push_error;
                            mpz_poly_clear(&poly);
                            added++;
                        } else {
                            mpz_poly_clear(&poly);
                        }
                        break;
                    }
                }
            }
        }
    }
    return added;
push_error:
    lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "equation system push failed (OOM)");
}

/* ================================================================== */
/*  内部: 几何推理消元模板 - 平行线截割定理                              */
/* ================================================================== */

static int template_parallel_intercept(ConstraintGraph *graph, EquationSystem *sys) {
    if (!graph || !sys)
        return 0;
    int added = 0;

    SegInfo *segs = lv_calloc((size_t) graph->node_count, sizeof(SegInfo));
    if (!segs)
        return 0;
    int seg_count = 0;
    collect_segment_incidence(graph, segs, &seg_count, true);

    for (int s = 0; s < seg_count; s++) {
        if (segs[s].p1 < 0 || segs[s].p2 < 0)
            continue;
        GeomNode *n1 = graph_get_node(graph, segs[s].p1);
        GeomNode *n2 = graph_get_node(graph, segs[s].p2);
        if (!n1 || !n2)
            continue;
        double x1, y1, x2, y2;
        if (point_coord(n1, 0, &x1) && point_coord(n1, 1, &y1) && point_coord(n2, 0, &x2) && point_coord(n2, 1, &y2)) {
            segs[s].dx = x2 - x1;
            segs[s].dy = y2 - y1;
        }
    }

    for (int i = 0; i < seg_count && added < 10; i++) {
        if (segs[i].p1 < 0 || fabs(segs[i].dx) + fabs(segs[i].dy) < lv_EPSILON_DOUBLE)
            continue;
        for (int j = i + 1; j < seg_count && added < 10; j++) {
            if (segs[j].p1 < 0 || fabs(segs[j].dx) + fabs(segs[j].dy) < lv_EPSILON_DOUBLE)
                continue;
            double cross = segs[i].dx * segs[j].dy - segs[i].dy * segs[j].dx;
            double len_i = geo_norm_2d(segs[i].dx, segs[i].dy);
            double len_j = geo_norm_2d(segs[j].dx, segs[j].dy);
            if (fabs(cross) < lv_EPSILON_LOW * (len_i * len_j + 1.0)) {
                for (int k = 0; k < seg_count && added < 10; k++) {
                    if (k == i || k == j)
                        continue;
                    if (segs[k].p1 < 0)
                        continue;
                    GeomNode *kp1 = graph_get_node(graph, segs[k].p1);
                    GeomNode *kp2 = graph_get_node(graph, segs[k].p2);
                    if (!kp1 || !kp2)
                        continue;
                    bool k1_on_i = false, k2_on_j = false, k1_on_j = false, k2_on_i = false;
                    for (int ci = 0; ci < graph->constraint_count; ci++) {
                        Constraint *c = graph->constraints[ci];
                        if (c->type != INCIDENCE || c->participant_count < 2)
                            continue;
                        int pt = c->participants[0];
                        int seg = c->participants[1];
                        if (pt == segs[k].p1 && seg == segs[i].id)
                            k1_on_i = true;
                        if (pt == segs[k].p2 && seg == segs[j].id)
                            k2_on_j = true;
                        if (pt == segs[k].p1 && seg == segs[j].id)
                            k1_on_j = true;
                        if (pt == segs[k].p2 && seg == segs[i].id)
                            k2_on_i = true;
                    }
                    if ((k1_on_i && k2_on_j) || (k1_on_j && k2_on_i)) {
                        int64_t scale = lv_SOLVER_SCALE_FACTOR;
                        mpz_poly_t poly;
                        mpz_poly_init(&poly);
                        poly.degree = 0;
                        poly.coeffs = lv_malloc(sizeof(mpz_t));
                        if (!poly.coeffs) {
                            mpz_poly_clear(&poly);
                            continue;
                        }
                        mpz_init(poly.coeffs[0]);
                        double_to_mpz_scaled(cross, poly.coeffs[0], scale);
                        mpz_poly_clear(&poly);
                        added++;
                    }
                }
            }
        }
    }

    lv_free((void **) &segs);
    return added;
}

/* ================================================================== */
/*  内部: 主函数 - 应用所有几何推理模板                                 */
/* ================================================================== */

static int apply_geometry_templates(ConstraintGraph *graph, EquationSystem *sys) {
    if (!graph || !sys)
        return 0;
    int total = 0;
    total += template_similar_triangles(graph, sys);
    total += template_pythagorean(graph, sys);
    total += template_parallel_intercept(graph, sys);
    return total;
}
