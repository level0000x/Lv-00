/**
 * @file solver_equation_extract.c
 * @brief 约束方程提取（由 solver_coord_extract.c 拆分子模块，修复 SOLVER_SPLIT_PLAN #7）
 *
 * @details 从约束图提取几何约束为多项式方程组：节点查找、线方程、
 *          各约束类型的方程提取（incidence/intersection/betweenness/
 *          containment/angle/connection）与整体提取入口。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"
#include "lv/coeff_pool.h" /* 共享的多项式系数内存池（实现与池拥有权见 lv/coeff_pool.h） */
#include "lv/lv_parse_utils.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_numeric.h"
#include "lv/geo_utils.h"
#include "lv/lv_lifecycle.h" /* lv_DEFER + lv_mpz_clear_deferred */

/* ------------------------------------------------------------------ */
/*  内部：按 ID 查找几何节点                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief 在约束图中按 ID 查找几何节点
 *
 * 封装 graph_get_node 调用，提供统一的内部查找接口。
 *
 * @param graph 约束图指针
 * @param id    目标节点的 ID
 * @return 找到的 GeomNode 指针，未找到返回 NULL
 */
static GeomNode *find_node(const ConstraintGraph *graph, int id) {
    return graph_get_node(graph, id);
}

/**
 * @brief 提取点的数值坐标值
 *
 * 从点的 symbolc_coords 数组的指定索引获取 double 数值。
 *
 * @param pt  目标点节点（必须为 GEOM_POINT 类型且坐标数量足够）
 * @param idx 坐标索引（0 = x, 1 = y）
 * @param out 输出：坐标的 double 近似值
 * @return true 表示成功获取，false 表示节点类型不符或坐标不足
 */
bool point_coord(const GeomNode *pt, int idx, double *out) {
    if (!pt || pt->type != GEOM_POINT || pt->coord_count <= idx)
        return false;
    return coord_to_double(pt->symbolic_coords[idx], out);
}

/**
 * @brief 一次提取点的 x/y 双分量数值坐标
 *
 * 等价于 `point_coord(pt, 0, x) && point_coord(pt, 1, y)`，收敛
 * 求解器各处以成对 point_coord 调用读取点二维坐标的样板。
 *
 * @param pt 目标点节点
 * @param x  输出：x 坐标 double 近似值
 * @param y  输出：y 坐标 double 近似值
 * @return true 表示 x/y 均成功获取，false 表示任一分量失败
 */
bool point_coord_xy(const GeomNode *pt, double *x, double *y) {
    return point_coord(pt, 0, x) && point_coord(pt, 1, y);
}

/* =======================================================================
 * 内部函数：从两点构建直线方程 ax + by + c = 0
 * ======================================================================= */

typedef struct {
    double a, b, c; /* ax + by + c = 0 */
} LineEquation;

bool line_from_two_points(GeomNode *p1, GeomNode *p2, LineEquation *out) {
    double x1, y1, x2, y2;
    if (!point_coord_xy(p1, &x1, &y1))
        return false;
    if (!point_coord_xy(p2, &x2, &y2))
        return false;
    /* 方向向量 (dx, dy) */
    double dx = x2 - x1;
    double dy = y2 - y1;

    /* 检测退化情况：两点重合
     * 使用 epsilon 比较而非精确相等（==），原因：
     * 浮点运算存在舍入误差，即使两点在数学上重合，
     * 经过坐标变换或中间计算后，dx 和 dy 可能不为精确的 0.0。
     * 使用 fabs(dx) < 1e-15 可以正确识别数值上近似为零的情况，
     * 避免将几乎重合的误判为有效直线（导致法向量接近零、方程退化）。 */
    if (fabs(dx) < 1e-15 && fabs(dy) < 1e-15) {
        LOG_WARN("solver", "line_from_two_points: 两点重合，无法确定直线");
        return false;
    }

    /* 法向量: (dy, -dx) => dy*(x-x1) - dx*(y-y1) = 0 */
    out->a = dy;
    out->b = -dx;
    out->c = -(out->a * x1 + out->b * y1);
    return true;
}

/* ------------------------------------------------------------------ */
/*  内部：从约束中提取代数方程                                         */
/* ------------------------------------------------------------------ */

/*
 * For INCIDENCE(point, line_segment):
 *   The line segment has two endpoint points.  The incidence constraint
 *   means the point lies on the line, i.e. cross product of direction
 *   vector and (point - endpoint) is zero.  This gives one linear equation.
 *
 * For INTERSECTION(line1, line2, result_point):
 *   The result point lies on both lines => two linear equations.
 *
 * For BETWEENNESS(p1, p2, p3):
 *   p2 lies on segment p1-p3.  This gives a collinearity equation
 *   plus a ratio constraint 0 <= t <= 1 where p2 = p1 + t*(p3-p1).
 *
 * For distance constraints (not directly a ConstraintType, but encoded
 * via CONNECTION or special numeric_assumption_declaration):
 *   (x2-x1)^2 + (y2-y1)^2 = d^2, which is quadratic.
 *
 * 精度限制说明：
 *   本函数在将几何约束转换为多项式方程时，使用 double 近似值来表示
 *   坐标和参数（如线段端点、距离值等），然后通过 double_to_mpz_scaled()
 *   将 double 转换为缩放后的 mpz_t 整数系数。这意味着：
 *   1. 对于 RATIONAL 类型的坐标，coord_to_double() 可以通过 mpq_get_d()
 *      获得精确的 double 表示（前提是值在 double 精度范围内）。
 *   2. 对于 QUADRATIC/ALGEBRAIC 类型的坐标，coord_to_double() 需要
 *      先序列化为字符串再解析，存在额外的精度损失。
 *   3. 缩放因子 scale=1000000 提供了约 6 位十进制精度，对于大多数
 *      几何计算足够，但不适用于需要高精度的场景。
 *   4. 如果需要完全精确的方程提取，应重构为直接使用 mpq_t/mpz_t
 *      而非经过 double 中间表示。
 */

/* ── ConstraintExtract VTable ── */
typedef int (*ConstraintExtractFunc)(const ConstraintGraph *graph, EquationSystem *sys, const Constraint *c);

static int extract_incidence(const ConstraintGraph *graph, EquationSystem *sys, const Constraint *c) {
    GeomNode *pt = find_node(graph, c->participants[0]);
    GeomNode *line = find_node(graph, c->participants[1]);
    if (!pt || !line)
        return 0;
    if (line->type == GEOM_LINE_SEGMENT && line->coord_count >= 2) {
        if (line->coord_count >= 4) {
            int64_t scale = lv_SOLVER_SCALE_FACTOR;
            mpz_t lx1_s, ly1_s, lx2_s, ly2_s;
            mpz_init(lx1_s);
            mpz_init(ly1_s);
            mpz_init(lx2_s);
            mpz_init(ly2_s);
            bool exact = coord_to_mpz_scaled_exact(line->symbolic_coords[0], lx1_s, scale) &&
                         coord_to_mpz_scaled_exact(line->symbolic_coords[1], ly1_s, scale) &&
                         coord_to_mpz_scaled_exact(line->symbolic_coords[2], lx2_s, scale) &&
                         coord_to_mpz_scaled_exact(line->symbolic_coords[3], ly2_s, scale);

            if (exact) {
                mpz_t dx_s, dy_s;
                mpz_init(dx_s);
                mpz_init(dy_s);
                mpz_sub(dx_s, lx2_s, lx1_s);
                mpz_sub(dy_s, ly2_s, ly1_s);

                mpz_poly_t poly;
                if (solver_poly_pool_init(&poly, 1, 2) != 0) {
                    mpz_clear(dx_s);
                    mpz_clear(dy_s);
                    mpz_clear(lx1_s);
                    mpz_clear(ly1_s);
                    mpz_clear(lx2_s);
                    mpz_clear(ly2_s);
                    return 0;
                }
                mpz_neg(poly.coeffs[1], dy_s);
                {
                    mpz_t term1, term2;
                    mpz_init(term1);
                    mpz_init(term2);
                    mpz_mul(term1, dy_s, lx1_s);
                    mpz_mul(term2, dx_s, ly1_s);
                    mpz_sub(term1, term1, term2);
                    mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                    mpz_clear(term1);
                    mpz_clear(term2);
                }
                if (solver_poly_pool_push(sys, &poly, pt->id, 0) != 0) {
                    mpz_clear(dx_s);
                    mpz_clear(dy_s);
                    mpz_clear(lx1_s);
                    mpz_clear(ly1_s);
                    mpz_clear(lx2_s);
                    mpz_clear(ly2_s);
                    lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                }

                if (solver_poly_pool_init(&poly, 1, 2) != 0) {
                    mpz_clear(dx_s);
                    mpz_clear(dy_s);
                    mpz_clear(lx1_s);
                    mpz_clear(ly1_s);
                    mpz_clear(lx2_s);
                    mpz_clear(ly2_s);
                    return 0;
                }
                mpz_set(poly.coeffs[1], dx_s);
                {
                    mpz_t term1, term2;
                    mpz_init(term1);
                    mpz_init(term2);
                    mpz_mul(term1, dx_s, ly1_s);
                    mpz_mul(term2, dy_s, lx1_s);
                    mpz_add(term1, term1, term2);
                    mpz_neg(term1, term1);
                    mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                    mpz_clear(term1);
                    mpz_clear(term2);
                }
                if (solver_poly_pool_push(sys, &poly, pt->id, 1) != 0) {
                    mpz_clear(dx_s);
                    mpz_clear(dy_s);
                    mpz_clear(lx1_s);
                    mpz_clear(ly1_s);
                    mpz_clear(lx2_s);
                    mpz_clear(ly2_s);
                    lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                }
                mpz_clear(dx_s);
                mpz_clear(dy_s);
            } else {
                double lx1, ly1, lx2, ly2;
                if (coord_to_double(line->symbolic_coords[0], &lx1) &&
                    coord_to_double(line->symbolic_coords[1], &ly1) &&
                    coord_to_double(line->symbolic_coords[2], &lx2) &&
                    coord_to_double(line->symbolic_coords[3], &ly2)) {
                    double dx = lx2 - lx1;
                    double dy = ly2 - ly1;
                    mpz_poly_t poly;
                    if (solver_poly_pool_init(&poly, 1, 2) != 0) {
                        return 0;
                    }
                    double_to_mpz_scaled(-dy, poly.coeffs[1], scale);
                    double_to_mpz_scaled(dy * lx1 - dx * ly1, poly.coeffs[0], scale);
                    if (solver_poly_pool_push(sys, &poly, pt->id, 0) != 0) {
                        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                    }

                    if (solver_poly_pool_init(&poly, 1, 2) != 0) {
                        return 0;
                    }
                    double_to_mpz_scaled(dx, poly.coeffs[1], scale);
                    double_to_mpz_scaled(-dx * ly1 - dy * lx1, poly.coeffs[0], scale);
                    if (solver_poly_pool_push(sys, &poly, pt->id, 1) != 0) {
                        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                    }
                }
            }
            mpz_clear(lx1_s);
            mpz_clear(ly1_s);
            mpz_clear(lx2_s);
            mpz_clear(ly2_s);
        }
    }
    return 0;
}

static int extract_intersection(const ConstraintGraph *graph, EquationSystem *sys, const Constraint *c) {
    if (c->participant_count < 3)
        return 0;
    GeomNode *line1 = find_node(graph, c->participants[0]);
    GeomNode *line2 = find_node(graph, c->participants[1]);
    GeomNode *rpt = find_node(graph, c->participants[2]);
    if (!line1 || !line2 || !rpt)
        return 0;

    int64_t scale = lv_SOLVER_SCALE_FACTOR;

    if (line1->type == GEOM_LINE_SEGMENT && line1->coord_count >= 4 && line2->type == GEOM_LINE_SEGMENT &&
        line2->coord_count >= 4) {
        mpz_t l1x1_s, l1y1_s, l1x2_s, l1y2_s;
        mpz_t l2x1_s, l2y1_s, l2x2_s, l2y2_s;
        mpz_init(l1x1_s);
        mpz_init(l1y1_s);
        mpz_init(l1x2_s);
        mpz_init(l1y2_s);
        mpz_init(l2x1_s);
        mpz_init(l2y1_s);
        mpz_init(l2x2_s);
        mpz_init(l2y2_s);

        bool exact1 = coord_to_mpz_scaled_exact(line1->symbolic_coords[0], l1x1_s, scale) &&
                      coord_to_mpz_scaled_exact(line1->symbolic_coords[1], l1y1_s, scale) &&
                      coord_to_mpz_scaled_exact(line1->symbolic_coords[2], l1x2_s, scale) &&
                      coord_to_mpz_scaled_exact(line1->symbolic_coords[3], l1y2_s, scale);
        bool exact2 = coord_to_mpz_scaled_exact(line2->symbolic_coords[0], l2x1_s, scale) &&
                      coord_to_mpz_scaled_exact(line2->symbolic_coords[1], l2y1_s, scale) &&
                      coord_to_mpz_scaled_exact(line2->symbolic_coords[2], l2x2_s, scale) &&
                      coord_to_mpz_scaled_exact(line2->symbolic_coords[3], l2y2_s, scale);

        if (exact1 && exact2) {
            mpz_t a1_s, b1_s, c1_s, a2_s, b2_s, c2_s;
            mpz_t dx1_s, dy1_s, dx2_s, dy2_s;
            mpz_init(a1_s);
            mpz_init(b1_s);
            mpz_init(c1_s);
            mpz_init(a2_s);
            mpz_init(b2_s);
            mpz_init(c2_s);
            mpz_init(dx1_s);
            mpz_init(dy1_s);
            mpz_init(dx2_s);
            mpz_init(dy2_s);

            mpz_sub(dx1_s, l1x2_s, l1x1_s);
            mpz_sub(dy1_s, l1y2_s, l1y1_s);
            mpz_set(a1_s, dy1_s);
            mpz_neg(b1_s, dx1_s);
            {
                mpz_t t1, t2;
                mpz_init(t1);
                mpz_init(t2);
                mpz_mul(t1, a1_s, l1x1_s);
                mpz_mul(t2, b1_s, l1y1_s);
                mpz_add(t1, t1, t2);
                mpz_neg(t1, t1);
                mpz_fdiv_q_ui(c1_s, t1, (unsigned long) scale);
                mpz_clear(t1);
                mpz_clear(t2);
            }

            mpz_sub(dx2_s, l2x2_s, l2x1_s);
            mpz_sub(dy2_s, l2y2_s, l2y1_s);
            mpz_set(a2_s, dy2_s);
            mpz_neg(b2_s, dx2_s);
            {
                mpz_t t1, t2;
                mpz_init(t1);
                mpz_init(t2);
                mpz_mul(t1, a2_s, l2x1_s);
                mpz_mul(t2, b2_s, l2y1_s);
                mpz_add(t1, t1, t2);
                mpz_neg(t1, t1);
                mpz_fdiv_q_ui(c2_s, t1, (unsigned long) scale);
                mpz_clear(t1);
                mpz_clear(t2);
            }

            mpz_t D_s, x_num_s, y_num_s;
            mpz_init(D_s);
            mpz_init(x_num_s);
            mpz_init(y_num_s);
            {
                mpz_t t1, t2;
                mpz_init(t1);
                mpz_init(t2);
                mpz_mul(t1, a1_s, b2_s);
                mpz_mul(t2, a2_s, b1_s);
                mpz_sub(D_s, t1, t2);
                mpz_clear(t1);
                mpz_clear(t2);
            }

            if (mpz_sgn(D_s) != 0) {
                {
                    mpz_t t1, t2;
                    mpz_init(t1);
                    mpz_init(t2);
                    mpz_mul(t1, b1_s, c2_s);
                    mpz_mul(t2, b2_s, c1_s);
                    mpz_sub(x_num_s, t1, t2);
                    mpz_clear(t1);
                    mpz_clear(t2);
                }
                {
                    mpz_t t1, t2;
                    mpz_init(t1);
                    mpz_init(t2);
                    mpz_mul(t1, a2_s, c1_s);
                    mpz_mul(t2, a1_s, c2_s);
                    mpz_sub(y_num_s, t1, t2);
                    mpz_clear(t1);
                    mpz_clear(t2);
                }

                mpz_poly_t poly;
                if (solver_poly_pool_init(&poly, 1, 2) == 0) {
                    mpz_set(poly.coeffs[1], D_s);
                    mpz_neg(poly.coeffs[0], x_num_s);
                    if (solver_poly_pool_push(sys, &poly, rpt->id, 0) != 0) {
                        mpz_clear(D_s);
                        mpz_clear(x_num_s);
                        mpz_clear(y_num_s);
                        mpz_clear(a1_s);
                        mpz_clear(b1_s);
                        mpz_clear(c1_s);
                        mpz_clear(a2_s);
                        mpz_clear(b2_s);
                        mpz_clear(c2_s);
                        mpz_clear(dx1_s);
                        mpz_clear(dy1_s);
                        mpz_clear(dx2_s);
                        mpz_clear(dy2_s);
                        mpz_clear(l1x1_s);
                        mpz_clear(l1y1_s);
                        mpz_clear(l1x2_s);
                        mpz_clear(l1y2_s);
                        mpz_clear(l2x1_s);
                        mpz_clear(l2y1_s);
                        mpz_clear(l2x2_s);
                        mpz_clear(l2y2_s);
                        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                    }
                }

                if (solver_poly_pool_init(&poly, 1, 2) == 0) {
                    mpz_set(poly.coeffs[1], D_s);
                    mpz_neg(poly.coeffs[0], y_num_s);
                    if (solver_poly_pool_push(sys, &poly, rpt->id, 1) != 0) {
                        mpz_clear(D_s);
                        mpz_clear(x_num_s);
                        mpz_clear(y_num_s);
                        mpz_clear(a1_s);
                        mpz_clear(b1_s);
                        mpz_clear(c1_s);
                        mpz_clear(a2_s);
                        mpz_clear(b2_s);
                        mpz_clear(c2_s);
                        mpz_clear(dx1_s);
                        mpz_clear(dy1_s);
                        mpz_clear(dx2_s);
                        mpz_clear(dy2_s);
                        mpz_clear(l1x1_s);
                        mpz_clear(l1y1_s);
                        mpz_clear(l1x2_s);
                        mpz_clear(l1y2_s);
                        mpz_clear(l2x1_s);
                        mpz_clear(l2y1_s);
                        mpz_clear(l2x2_s);
                        mpz_clear(l2y2_s);
                        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                    }
                }
            }

            mpz_clear(D_s);
            mpz_clear(x_num_s);
            mpz_clear(y_num_s);
            mpz_clear(a1_s);
            mpz_clear(b1_s);
            mpz_clear(c1_s);
            mpz_clear(a2_s);
            mpz_clear(b2_s);
            mpz_clear(c2_s);
            mpz_clear(dx1_s);
            mpz_clear(dy1_s);
            mpz_clear(dx2_s);
            mpz_clear(dy2_s);
            mpz_clear(l1x1_s);
            mpz_clear(l1y1_s);
            mpz_clear(l1x2_s);
            mpz_clear(l1y2_s);
            mpz_clear(l2x1_s);
            mpz_clear(l2y1_s);
            mpz_clear(l2x2_s);
            mpz_clear(l2y2_s);
            return 0;
        }

        mpz_clear(l1x1_s);
        mpz_clear(l1y1_s);
        mpz_clear(l1x2_s);
        mpz_clear(l1y2_s);
        mpz_clear(l2x1_s);
        mpz_clear(l2y1_s);
        mpz_clear(l2x2_s);
        mpz_clear(l2y2_s);
    }

    {
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
            double D = le1.a * le2.b - le2.a * le1.b;

            if (fabs(D) < lv_EPSILON_NUMERIC_COMPARE) {
                return 0;
            }

            double x_numerator = le1.b * le2.c - le2.b * le1.c;
            double y_numerator = le2.a * le1.c - le1.a * le2.c;

            mpz_poly_t poly;
            if (solver_poly_pool_init(&poly, 1, lv_SOLVER_LINEAR_COEFF_COUNT) != 0) {
                return 0;
            }
            double_to_mpz_scaled(D, poly.coeffs[1], scale);
            double_to_mpz_scaled(-x_numerator, poly.coeffs[0], scale);
            if (solver_poly_pool_push(sys, &poly, rpt->id, 0) != 0) {
                lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
            }

            if (solver_poly_pool_init(&poly, 1, 2) != 0) {
                return 0;
            }
            double_to_mpz_scaled(D, poly.coeffs[1], scale);
            double_to_mpz_scaled(-y_numerator, poly.coeffs[0], scale);
            if (solver_poly_pool_push(sys, &poly, rpt->id, 1) != 0) {
                lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
            }
        }
    }
    return 0;
}

static int extract_betweenness(const ConstraintGraph *graph, EquationSystem *sys, const Constraint *c) {
    if (c->participant_count < 3)
        return 0;
    GeomNode *p1 = find_node(graph, c->participants[0]);
    GeomNode *p2 = find_node(graph, c->participants[1]);
    GeomNode *p3 = find_node(graph, c->participants[2]);
    if (!p1 || !p2 || !p3)
        return 0;
    if (p1->type != GEOM_POINT || p3->type != GEOM_POINT)
        return 0;

    int64_t scale = lv_SOLVER_SCALE_FACTOR;
    mpz_t x1_s, y1_s, x3_s, y3_s;
    mpz_init(x1_s);
    mpz_init(y1_s);
    mpz_init(x3_s);
    mpz_init(y3_s);

    if (p1->symbolic_coords && p3->symbolic_coords && p1->coord_count >= 2 && p3->coord_count >= 2 &&
        coord_to_mpz_scaled_exact(p1->symbolic_coords[0], x1_s, scale) &&
        coord_to_mpz_scaled_exact(p1->symbolic_coords[1], y1_s, scale) &&
        coord_to_mpz_scaled_exact(p3->symbolic_coords[0], x3_s, scale) &&
        coord_to_mpz_scaled_exact(p3->symbolic_coords[1], y3_s, scale)) {
        mpz_t dx_s, dy_s;
        mpz_init(dx_s);
        mpz_init(dy_s);
        mpz_sub(dx_s, x3_s, x1_s);
        mpz_sub(dy_s, y3_s, y1_s);

        {
            mpz_poly_t poly;
            if (solver_poly_pool_init(&poly, 1, 2) == 0) {
                mpz_set(poly.coeffs[1], dy_s);
                mpz_t term1, term2;
                mpz_init(term1);
                mpz_init(term2);
                mpz_mul(term1, y1_s, dx_s);
                mpz_mul(term2, x1_s, dy_s);
                mpz_sub(term1, term1, term2);
                mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                mpz_clear(term1);
                mpz_clear(term2);
                if (solver_poly_pool_push(sys, &poly, p2->id, 0) != 0) {
                    mpz_clear(dx_s);
                    mpz_clear(dy_s);
                    mpz_clear(x1_s);
                    mpz_clear(y1_s);
                    mpz_clear(x3_s);
                    mpz_clear(y3_s);
                    lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                }
            }
        }

        {
            mpz_poly_t poly;
            if (solver_poly_pool_init(&poly, 1, 2) == 0) {
                mpz_neg(poly.coeffs[1], dx_s);
                mpz_t term1, term2;
                mpz_init(term1);
                mpz_init(term2);
                mpz_mul(term1, dy_s, x1_s);
                mpz_mul(term2, dx_s, y1_s);
                mpz_sub(term1, term1, term2);
                mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                mpz_clear(term1);
                mpz_clear(term2);
                if (solver_poly_pool_push(sys, &poly, p2->id, 1) != 0) {
                    mpz_clear(dx_s);
                    mpz_clear(dy_s);
                    mpz_clear(x1_s);
                    mpz_clear(y1_s);
                    mpz_clear(x3_s);
                    mpz_clear(y3_s);
                    lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                }
            }
        }

        mpz_clear(dx_s);
        mpz_clear(dy_s);
    }
    mpz_clear(x1_s);
    mpz_clear(y1_s);
    mpz_clear(x3_s);
    mpz_clear(y3_s);
    return 0;
}

static int extract_containment(const ConstraintGraph *graph, EquationSystem *sys, const Constraint *c) {
    if (c->participant_count < 2)
        return 0;
    GeomNode *inner = find_node(graph, c->participants[0]);
    GeomNode *outer = find_node(graph, c->participants[1]);
    if (!inner || !outer)
        return 0;

    if (inner->type != GEOM_POINT || outer->type != GEOM_REGION)
        return 0;
    if (outer->data.region.segment_count <= 0 || !outer->data.region.boundary_segments)
        return 0;

    {
        int64_t scale = lv_SOLVER_SCALE_FACTOR;
        int seg_count = outer->data.region.segment_count;

        for (int si = 0; si < seg_count; si++) {
            GeomNode *seg = outer->data.region.boundary_segments[si];
            if (!seg || seg->type != GEOM_LINE_SEGMENT)
                continue;
            if (seg->coord_count < 4 || !seg->symbolic_coords)
                continue;

            mpz_t sx1_s, sy1_s, sx2_s, sy2_s;
            mpz_init(sx1_s);
            mpz_init(sy1_s);
            mpz_init(sx2_s);
            mpz_init(sy2_s);

            if (coord_to_mpz_scaled_exact(seg->symbolic_coords[0], sx1_s, scale) &&
                coord_to_mpz_scaled_exact(seg->symbolic_coords[1], sy1_s, scale) &&
                coord_to_mpz_scaled_exact(seg->symbolic_coords[2], sx2_s, scale) &&
                coord_to_mpz_scaled_exact(seg->symbolic_coords[3], sy2_s, scale)) {
                mpz_t dx_s, dy_s;
                mpz_init(dx_s);
                mpz_init(dy_s);
                mpz_sub(dx_s, sx2_s, sx1_s);
                mpz_sub(dy_s, sy2_s, sy1_s);

                {
                    mpz_poly_t poly;
                    if (solver_poly_pool_init(&poly, 1, 2) == 0) {
                        mpz_set(poly.coeffs[1], dy_s);
                        mpz_t term1, term2;
                        mpz_init(term1);
                        mpz_init(term2);
                        mpz_mul(term1, dx_s, sy1_s);
                        mpz_mul(term2, dy_s, sx1_s);
                        mpz_sub(term1, term1, term2);
                        mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                        mpz_clear(term1);
                        mpz_clear(term2);
                        if (solver_poly_pool_push(sys, &poly, inner->id, 0) != 0) {
                            mpz_clear(dx_s);
                            mpz_clear(dy_s);
                            mpz_clear(sx1_s);
                            mpz_clear(sy1_s);
                            mpz_clear(sx2_s);
                            mpz_clear(sy2_s);
                            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                        }
                    }
                }

                {
                    mpz_poly_t poly;
                    if (solver_poly_pool_init(&poly, 1, 2) == 0) {
                        mpz_neg(poly.coeffs[1], dx_s);
                        mpz_t term1, term2;
                        mpz_init(term1);
                        mpz_init(term2);
                        mpz_mul(term1, dy_s, sx1_s);
                        mpz_mul(term2, dx_s, sy1_s);
                        mpz_sub(term1, term1, term2);
                        mpz_fdiv_q_ui(poly.coeffs[0], term1, (unsigned long) scale);
                        mpz_clear(term1);
                        mpz_clear(term2);
                        if (solver_poly_pool_push(sys, &poly, inner->id, 1) != 0) {
                            mpz_clear(dx_s);
                            mpz_clear(dy_s);
                            mpz_clear(sx1_s);
                            mpz_clear(sy1_s);
                            mpz_clear(sx2_s);
                            mpz_clear(sy2_s);
                            lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
                        }
                    }
                }

                mpz_clear(dx_s);
                mpz_clear(dy_s);
            }

            mpz_clear(sx1_s);
            mpz_clear(sy1_s);
            mpz_clear(sx2_s);
            mpz_clear(sy2_s);
        }
    }
    return 0;
}

static int extract_angle(const ConstraintGraph *graph, EquationSystem *sys, const Constraint *c) {
    (void)graph;
    (void)sys;
    (void)c;
    return 0;
}

static int extract_connection(const ConstraintGraph *graph, EquationSystem *sys, const Constraint *c) {
    if (c->participant_count < 2)
        return 0;
    GeomNode *nodeA = find_node(graph, c->participants[0]);
    GeomNode *nodeB = find_node(graph, c->participants[1]);
    if (!nodeA || !nodeB)
        return 0;

    double dist_val = -1.0;
    GeomNode *dist_node = NULL;
    const char *prefix = "distance=";
    size_t prefix_len = strlen(prefix);
    for (int ni = 0; ni < 2; ni++) {
        GeomNode *n = (ni == 0) ? nodeA : nodeB;
        if (!n || !n->numeric_assumption_declaration)
            continue;
        const char *decl = n->numeric_assumption_declaration;
        if (lv_str_startswith(decl, prefix)) {
            if (lv_parse_double(decl + prefix_len, &dist_val) != 0)
                dist_val = 0.0;
            dist_node = n;
            break;
        }
    }

    if (dist_val < 0)
        return 0;

    if (nodeA->coord_count < 2 || nodeB->coord_count < 2)
        return 0;
    if (!nodeA->symbolic_coords || !nodeB->symbolic_coords)
        return 0;

    double ax, ay, bx, by;
    if (!coord_to_double(nodeA->symbolic_coords[0], &ax) ||
        !coord_to_double(nodeA->symbolic_coords[1], &ay) ||
        !coord_to_double(nodeB->symbolic_coords[0], &bx) ||
        !coord_to_double(nodeB->symbolic_coords[1], &by))
        return 0;

    double dist_sq = dist_val * dist_val;
    int64_t scale = lv_SOLVER_SCALE_FACTOR;

    mpz_poly_t poly;
    if (solver_poly_pool_init(&poly, 2, lv_SOLVER_QUADRATIC_COEFF_COUNT) != 0)
        return 0;
    mpz_set_si(poly.coeffs[2], scale);
    double_to_mpz_scaled(-2.0 * ax, poly.coeffs[1], scale);
    double_to_mpz_scaled(geo_norm_sq_2d(ax, ay) - dist_sq, poly.coeffs[0], scale);
    if (solver_poly_pool_push(sys, &poly, nodeB->id, 0) != 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");

    if (solver_poly_pool_init(&poly, 2, 3) != 0)
        return 0;
    mpz_set_si(poly.coeffs[2], scale);
    double_to_mpz_scaled(-2.0 * ay, poly.coeffs[1], scale);
    double_to_mpz_scaled(geo_norm_sq_2d(ax, ay) - dist_sq, poly.coeffs[0], scale);
    if (solver_poly_pool_push(sys, &poly, nodeB->id, 1) != 0)
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "push failed (OOM)");
    return 0;
}

static const ConstraintExtractFunc constraint_extract_ops[] = {
    [INCIDENCE] = extract_incidence,
    [INTERSECTION] = extract_intersection,
    [BETWEENNESS] = extract_betweenness,
    [CONTAINMENT] = extract_containment,
    [ANGLE] = extract_angle,
    [CONNECTION] = extract_connection,
};

/**
 * @brief 从约束图中提取代数方程
 *
 * @details 遍历约束图中的所有约束，根据约束类型生成对应的多项式方程：
 *          INCIDENCE（关联）、INTERSECTION（交点）、BETWEENNESS（介于）、
 *          CONTAINMENT（包含）、CONNECTION（连接）。
 *          也处理线段节点上的 numeric_assumption_declaration 距离约束。
 *          构造临时点用于 line_from_two_points，内存管理采用栈分配，
 *          不使用动态分配以避免泄漏风险。
 *
 * @param graph 约束图指针
 * @param sys   输出：存储提取方程的系统
 */
void extract_equations_from_constraints(const ConstraintGraph *graph, EquationSystem *sys) {
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c || c->participant_count < 2)
            continue;

        int type = c->type;
        if (lv_index_in_range(type, (int)(sizeof(constraint_extract_ops) / sizeof(constraint_extract_ops[0]))) && constraint_extract_ops[type]) {
            if (constraint_extract_ops[type](graph, sys, c) != 0)
                goto push_error;
        } else {
            lv_LOG_WARNING("Unknown constraint type %d in extract_equations_from_constraints", c->type);
        }
    }

    /* 第二遍扫描：从设置了数值假设声明的节点中提取距离约束
       （编码平方距离 = d^2）。 */
    for (int ni = 0; ni < graph->node_count; ni++) {
        GeomNode *node = graph->nodes[ni];
        if (!node || !node->numeric_assumption_declaration)
            continue;
        if (node->type != GEOM_LINE_SEGMENT)
            continue;

        /* 检查声明是否编码了距离约束。
           格式："distance=<value>" 或仅为数值。 */
        const char *decl = node->numeric_assumption_declaration;
        double dist_sq = -1.0;

        /* 尝试解析为 "distance=<value>" 格式 */
        const char *prefix = "distance=";
        size_t prefix_len = strlen(prefix); /* 缓存前缀长度，避免重复计算 */
        if (lv_str_startswith(decl, prefix)) {
            if (lv_parse_double(decl + prefix_len, &dist_sq) != 0)
                dist_sq = 0.0;
            dist_sq = dist_sq * dist_sq; /* 存储平方值 */
        } else {
            /* 尝试解析为纯数字（视为距离的平方） */
            double val;
            if (lv_parse_double(decl, &val) == 0 && val >= 0) {
                dist_sq = val;
            }
        }

        if (dist_sq < 0)
            continue;

        /* 线段在 symbolic_coords 中存储了端点坐标。
           为第二个端点建立距离方程
          （第一个端点通常已固定）。 */
        if (node->coord_count >= 4) {
            double x1, y1;
            if (coord_to_double(node->symbolic_coords[0], &x1) && coord_to_double(node->symbolic_coords[1], &y1)) {
                /* (x - x1)^2 + (y - y1)^2 = dist_sq
                   => x^2 - 2*x1*x + x1^2 + y^2 - 2*y1*y + y1^2 - dist_sq = 0
                   这是一个关于 x 和 y 的二次方程。将其存储为两个独立的一元方程
                  （耦合），求解器将按方程组处理。 */

                /* 对于第二个端点的 x 坐标：需要第二个端点的节点 ID。
                   由于线段直接存储坐标，我们创建以线段 ID 标记的方程。 */
                int64_t scale = lv_SOLVER_SCALE_FACTOR;

                /* x^2 - 2*x1*x + (x1^2 + y1^2 - dist_sq - y^2 + 2*y1*y) = 0
                   作为 x 的单变量方程: x^2 - 2*x1*x + const = 0
                   其中 const = x1^2 + y1^2 - dist_sq（忽略含 y 的耦合项）。
                   注意：距离方程展开后常数项为 x1^2 + y1^2 - dist_sq，
                   原代码错误地使用了 x1^2 + dist_sq（符号错误）。 */
                mpz_poly_t poly;
                if (solver_poly_pool_init(&poly, 2, lv_SOLVER_QUADRATIC_COEFF_COUNT) != 0)
                    continue;
                mpz_set_si(poly.coeffs[2], scale);                                        /* x^2 系数 */
                double_to_mpz_scaled(-2.0 * x1, poly.coeffs[1], scale);                   /* x 系数 */
                double_to_mpz_scaled(x1 * x1 + y1 * y1 - dist_sq, poly.coeffs[0], scale); /* 常数项（已修正符号） */
                if (solver_poly_pool_push(sys, &poly, node->id, 0) != 0)
                    goto push_error;

                /* 同理对 y 建立方程：y^2 - 2*y1*y + (x1^2 + y1^2 - dist_sq) = 0 */
                if (solver_poly_pool_init(&poly, 2, 3) != 0)
                    continue;
                mpz_set_si(poly.coeffs[2], scale);
                double_to_mpz_scaled(-2.0 * y1, poly.coeffs[1], scale);
                double_to_mpz_scaled(x1 * x1 + y1 * y1 - dist_sq, poly.coeffs[0], scale); /* 常数项（已修正符号） */
                if (solver_poly_pool_push(sys, &poly, node->id, 1) != 0)
                    goto push_error;
            }
        }
    }
push_error:
    return;
}

/* ------------------------------------------------------------------ */
/*  公共 API：solver_extract_equations_full                            */
/* ------------------------------------------------------------------ */
/* 原实现在 solver_equation_extract.c（增强提取器，与上方 6 对约束提取
 * 函数构成双写）。数学收敛后统一为 solver_coord_extract.c 的
 * extract_equations_from_constraints 主路径，本函数保留为薄包装以维持
 * 公共 API 契约（NULL 校验 + 返回方程数 eqs.count）。 */
int solver_extract_equations_full(const ConstraintGraph *graph, EquationSystem *out_system) {
    if (!graph || !out_system)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "solver_extract_equations_full: graph or out_system is NULL");
    extract_equations_from_constraints(graph, out_system);
    return out_system->eqs.count;
}

/* ------------------------------------------------------------------ */
/*  内部：统计每个变量的有效方程数量                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    int node_id;
    int eq_count;
    int max_degree;
} VarInfo;

/* 统计方程系统中每个变量节点对应的方程数量和最高次数。
   返回 VarInfo 数组，通过 out_var_count 输出变量数量。
   如果方程系统为空（var_count == 0），直接返回 NULL。 */
static VarInfo *build_var_info(const EquationSystem *sys, int node_count, int *out_var_count) {
    /* 收集所有不重复的变量节点 id */
    int *var_ids = lv_malloc((size_t) sys->eqs.count * sizeof(int));
    if (!var_ids)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "build_var_info: lv_malloc for var_ids failed (count=%d)", sys->eqs.count);
    int var_count = 0;
    for (int i = 0; i < sys->eqs.count; i++) {
        PolyEquation *eq = (PolyEquation *)lv_darray_get(&sys->eqs, i);
        if (!eq) continue;
        lv_int_append_unique(var_ids, &var_count, eq->var_node_id);
    }

    /* 提前返回：如果没有变量，避免 lv_calloc(0, ...) 的未定义行为 */
    if (var_count == 0) {
        lv_free((void **) &var_ids);
        *out_var_count = 0;
        lv_RETURN_ERROR_NULL(lv_ERROR_NOT_FOUND, "build_var_info: no variables found in equation system (eqs.count=%d)", sys->eqs.count);
    }

    VarInfo *info = lv_calloc((size_t) var_count, sizeof(VarInfo));
    for (int i = 0; i < var_count; i++) {
        info[i].node_id = var_ids[i];
        info[i].eq_count = 0;
        info[i].max_degree = 0;
    }
    for (int i = 0; i < sys->eqs.count; i++) {
        PolyEquation *eq = (PolyEquation *)lv_darray_get(&sys->eqs, i);
        if (!eq) continue;
        int vid = eq->var_node_id;
        for (int j = 0; j < var_count; j++) {
            if (info[j].node_id == vid) {
                info[j].eq_count++;
                int deg = eq->poly.degree;
                if (deg > info[j].max_degree)
                    info[j].max_degree = deg;
                break;
            }
        }
    }
    lv_free((void **) &var_ids);
    *out_var_count = var_count;
    return info;
}

/* ------------------------------------------------------------------ */
/*  内部：求解一元一次方程 a*x + b = 0                                    */
/* ------------------------------------------------------------------ */

/* 一元一次求解统一走 solver_linear.c 的 solve_linear，此处不再重复定义。 */
