/**
 * @file geometry_ops.c
 * @brief 蓝图几何运算实现（TEN_LAYER_OPTIMIZED_PLAN §12.3/12.5/12.6/12.7 落地）
 *
 * 2D 几何运算：坐标分量对输入/输出（见 geometry_ops.h 头注释的适配说明）。
 * 符号精度路径用 SymbolicCoord 算术（add/sub/mul/div/negate）；
 * 需开方/三角的心点（外心/垂心/内心/九点圆/旁心）与圆交点走 double 数值
 * 计算后 symbolic_coord_from_double_rounded 构造（精度 1e6）。
 */

#include "lv/geometry_ops.h"

#include <math.h>

#include "lv/lv_utils.h"

/* ============================================================
 * 内部工具
 * ============================================================ */

/** @brief double 分量 → 有理坐标 */
static SymbolicCoord *dbl_coord(double v) {
    return symbolic_coord_from_double_rounded(v, 1000000);
}

/** @brief 双坐标对释放 */
static void pair_destroy(SymbolicCoord *a, SymbolicCoord *b) {
    symbolic_coord_pair_destroy(a, b);
}

/** @brief 输出中点（符号精度）共用 */
static bool mid_symbolic(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                         const SymbolicCoord *by, SymbolicCoord **ox, SymbolicCoord **oy) {
    if (ox == NULL || oy == NULL || ax == NULL || ay == NULL || bx == NULL || by == NULL)
        return false;
    SymbolicCoord *sx = symbolic_coord_add(ax, bx);
    SymbolicCoord *sy = symbolic_coord_add(ay, by);
    SymbolicCoord *two = symbolic_coord_create_rational(2, 1);
    if (sx == NULL || sy == NULL || two == NULL) {
        pair_destroy(sx, sy);
        symbolic_coord_destroy(two);
        return false;
    }
    *ox = symbolic_coord_divide(sx, two);
    *oy = symbolic_coord_divide(sy, two);
    pair_destroy(sx, sy);
    symbolic_coord_destroy(two);
    return *ox != NULL && *oy != NULL;
}

/** @brief 两分量是否（容差内）相等（判零向量用 double） */
static bool nearly_zero(double v, double tol) {
    return fabs(v) <= tol;
}

/* ============================================================
 * Point（§12.3）
 * ============================================================ */

bool lv_point_distance_sq(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                          const SymbolicCoord *by, SymbolicCoord **out_result) {
    if (out_result == NULL)
        return false;
    *out_result = NULL;
    if (ax == NULL || ay == NULL || bx == NULL || by == NULL)
        return false;
    SymbolicCoord *dx = symbolic_coord_subtract(bx, ax);
    SymbolicCoord *dy = symbolic_coord_subtract(by, ay);
    if (dx == NULL || dy == NULL) {
        pair_destroy(dx, dy);
        return false;
    }
    SymbolicCoord *dx2 = symbolic_coord_multiply(dx, dx);
    SymbolicCoord *dy2 = symbolic_coord_multiply(dy, dy);
    pair_destroy(dx, dy);
    if (dx2 == NULL || dy2 == NULL) {
        pair_destroy(dx2, dy2);
        return false;
    }
    SymbolicCoord *sum = symbolic_coord_add(dx2, dy2);
    pair_destroy(dx2, dy2);
    if (sum == NULL)
        return false;
    *out_result = sum;
    return true;
}

bool lv_point_midpoint(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx, const SymbolicCoord *by,
                       SymbolicCoord **out_x, SymbolicCoord **out_y) {
    return mid_symbolic(ax, ay, bx, by, out_x, out_y);
}

bool lv_point_is_collinear(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                           const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy, bool *out_result,
                           double tolerance) {
    if (out_result == NULL || ax == NULL || ay == NULL || bx == NULL || by == NULL || cx == NULL || cy == NULL)
        return false;
    *out_result = false;
    /* 叉积 (b-a)×(c-a) = (bx-ax)(cy-ay) - (by-ay)(cx-ax) */
    SymbolicCoord *u1 = symbolic_coord_subtract(bx, ax);
    SymbolicCoord *u2 = symbolic_coord_subtract(by, ay);
    SymbolicCoord *v1 = symbolic_coord_subtract(cx, ax);
    SymbolicCoord *v2 = symbolic_coord_subtract(cy, ay);
    if (u1 == NULL || u2 == NULL || v1 == NULL || v2 == NULL) {
        pair_destroy(u1, u2);
        pair_destroy(v1, v2);
        return false;
    }
    SymbolicCoord *c1 = symbolic_coord_multiply(u1, v2);
    SymbolicCoord *c2 = symbolic_coord_multiply(u2, v1);
    pair_destroy(u1, u2);
    pair_destroy(v1, v2);
    if (c1 == NULL || c2 == NULL) {
        pair_destroy(c1, c2);
        return false;
    }
    SymbolicCoord *cross = symbolic_coord_subtract(c1, c2);
    pair_destroy(c1, c2);
    if (cross == NULL)
        return false;
    if (symbolic_coord_is_zero(cross)) {
        *out_result = true;
    } else {
        /* 非精确类型（或数值噪音）：double 叉积 + 容差 */
        double cross_d = symbolic_coord_to_double(cross);
        *out_result = nearly_zero(cross_d, tolerance);
    }
    symbolic_coord_destroy(cross);
    return true;
}

/* ============================================================
 * Segment（§12.7）
 * ============================================================ */

bool lv_segment_length_sq(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                          const SymbolicCoord *by, SymbolicCoord **out) {
    return lv_point_distance_sq(ax, ay, bx, by, out);
}

bool lv_segment_midpoint(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                         const SymbolicCoord *by, SymbolicCoord **out_x, SymbolicCoord **out_y) {
    return mid_symbolic(ax, ay, bx, by, out_x, out_y);
}

bool lv_segment_direction(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                          const SymbolicCoord *by, SymbolicCoord **out_dx, SymbolicCoord **out_dy) {
    if (out_dx == NULL || out_dy == NULL || ax == NULL || ay == NULL || bx == NULL || by == NULL)
        return false;
    *out_dx = symbolic_coord_subtract(bx, ax);
    *out_dy = symbolic_coord_subtract(by, ay);
    return *out_dx != NULL && *out_dy != NULL;
}

bool lv_segment_is_parallel(const SymbolicCoord *a1x, const SymbolicCoord *a1y, const SymbolicCoord *a2x,
                            const SymbolicCoord *a2y, const SymbolicCoord *b1x, const SymbolicCoord *b1y,
                            const SymbolicCoord *b2x, const SymbolicCoord *b2y, bool *out, double tolerance) {
    if (out == NULL || a1x == NULL || a1y == NULL || a2x == NULL || a2y == NULL || b1x == NULL || b1y == NULL ||
        b2x == NULL || b2y == NULL)
        return false;
    /* 方向叉积 (a2-a1)×(b2-b1) 判零 */
    SymbolicCoord *adx = symbolic_coord_subtract(a2x, a1x);
    SymbolicCoord *ady = symbolic_coord_subtract(a2y, a1y);
    SymbolicCoord *bdx = symbolic_coord_subtract(b2x, b1x);
    SymbolicCoord *bdy = symbolic_coord_subtract(b2y, b1y);
    if (adx == NULL || ady == NULL || bdx == NULL || bdy == NULL) {
        pair_destroy(adx, ady);
        pair_destroy(bdx, bdy);
        return false;
    }
    SymbolicCoord *c1 = symbolic_coord_multiply(adx, bdy);
    SymbolicCoord *c2 = symbolic_coord_multiply(ady, bdx);
    pair_destroy(adx, ady);
    pair_destroy(bdx, bdy);
    if (c1 == NULL || c2 == NULL) {
        pair_destroy(c1, c2);
        return false;
    }
    SymbolicCoord *cross = symbolic_coord_subtract(c1, c2);
    pair_destroy(c1, c2);
    if (cross == NULL)
        return false;
    if (symbolic_coord_is_zero(cross))
        *out = true;
    else
        *out = nearly_zero(symbolic_coord_to_double(cross), tolerance);
    symbolic_coord_destroy(cross);
    return true;
}

bool lv_segment_is_perpendicular(const SymbolicCoord *a1x, const SymbolicCoord *a1y, const SymbolicCoord *a2x,
                                 const SymbolicCoord *a2y, const SymbolicCoord *b1x, const SymbolicCoord *b1y,
                                 const SymbolicCoord *b2x, const SymbolicCoord *b2y, bool *out, double tolerance) {
    if (out == NULL || a1x == NULL || a1y == NULL || a2x == NULL || a2y == NULL || b1x == NULL || b1y == NULL ||
        b2x == NULL || b2y == NULL)
        return false;
    /* 方向点积 (a2-a1)·(b2-b1) 判零 */
    SymbolicCoord *adx = symbolic_coord_subtract(a2x, a1x);
    SymbolicCoord *ady = symbolic_coord_subtract(a2y, a1y);
    SymbolicCoord *bdx = symbolic_coord_subtract(b2x, b1x);
    SymbolicCoord *bdy = symbolic_coord_subtract(b2y, b1y);
    if (adx == NULL || ady == NULL || bdx == NULL || bdy == NULL) {
        pair_destroy(adx, ady);
        pair_destroy(bdx, bdy);
        return false;
    }
    SymbolicCoord *p1 = symbolic_coord_multiply(adx, bdx);
    SymbolicCoord *p2 = symbolic_coord_multiply(ady, bdy);
    pair_destroy(adx, ady);
    pair_destroy(bdx, bdy);
    if (p1 == NULL || p2 == NULL) {
        pair_destroy(p1, p2);
        return false;
    }
    SymbolicCoord *dot = symbolic_coord_add(p1, p2);
    pair_destroy(p1, p2);
    if (dot == NULL)
        return false;
    if (symbolic_coord_is_zero(dot))
        *out = true;
    else
        *out = nearly_zero(symbolic_coord_to_double(dot), tolerance);
    symbolic_coord_destroy(dot);
    return true;
}

bool lv_segment_intersection(const SymbolicCoord *a1x, const SymbolicCoord *a1y, const SymbolicCoord *a2x,
                             const SymbolicCoord *a2y, const SymbolicCoord *b1x, const SymbolicCoord *b1y,
                             const SymbolicCoord *b2x, const SymbolicCoord *b2y, SymbolicCoord **out_x,
                             SymbolicCoord **out_y) {
    if (out_x == NULL || out_y == NULL || a1x == NULL || a1y == NULL || a2x == NULL || a2y == NULL || b1x == NULL ||
        b1y == NULL || b2x == NULL || b2y == NULL)
        return false;
    *out_x = NULL;
    *out_y = NULL;
    /* 数值：线段参数化 A + t·d1 = B + s·d2 */
    double a1xd = symbolic_coord_to_double(a1x), a1yd = symbolic_coord_to_double(a1y);
    double a2xd = symbolic_coord_to_double(a2x), a2yd = symbolic_coord_to_double(a2y);
    double b1xd = symbolic_coord_to_double(b1x), b1yd = symbolic_coord_to_double(b1y);
    double b2xd = symbolic_coord_to_double(b2x), b2yd = symbolic_coord_to_double(b2y);
    double d1x = a2xd - a1xd, d1y = a2yd - a1yd;
    double d2x = b2xd - b1xd, d2y = b2yd - b1yd;
    double denom = d1x * d2y - d1y * d2x;
    if (fabs(denom) < 1e-12)
        return false; /* 平行/共线 */
    double t = ((b1xd - a1xd) * d2y - (b1yd - a1yd) * d2x) / denom;
    if (t < -1e-9 || t > 1.0 + 1e-9)
        return false; /* 交点在线段外 */
    *out_x = dbl_coord(a1xd + t * d1x);
    *out_y = dbl_coord(a1yd + t * d1y);
    return *out_x != NULL && *out_y != NULL;
}

bool lv_segment_contains_point(const SymbolicCoord *a1x, const SymbolicCoord *a1y, const SymbolicCoord *a2x,
                               const SymbolicCoord *a2y, const SymbolicCoord *px, const SymbolicCoord *py, bool *out,
                               double tolerance) {
    bool collinear = false;
    if (!lv_point_is_collinear(a1x, a1y, a2x, a2y, px, py, &collinear, tolerance))
        return false;
    if (out == NULL)
        return false;
    *out = false;
    if (!collinear)
        return true;
    /* 包围盒检查 */
    double a1xd = symbolic_coord_to_double(a1x), a1yd = symbolic_coord_to_double(a1y);
    double a2xd = symbolic_coord_to_double(a2x), a2yd = symbolic_coord_to_double(a2y);
    double pxd = symbolic_coord_to_double(px), pyd = symbolic_coord_to_double(py);
    double minx = fmin(a1xd, a2xd) - tolerance, maxx = fmax(a1xd, a2xd) + tolerance;
    double miny = fmin(a1yd, a2yd) - tolerance, maxy = fmax(a1yd, a2yd) + tolerance;
    *out = pxd >= minx && pxd <= maxx && pyd >= miny && pyd <= maxy;
    return true;
}

bool lv_segment_distance_to_point(const SymbolicCoord *a1x, const SymbolicCoord *a1y, const SymbolicCoord *a2x,
                                  const SymbolicCoord *a2y, const SymbolicCoord *px, const SymbolicCoord *py,
                                  SymbolicCoord **out_distance) {
    if (out_distance == NULL || a1x == NULL || a1y == NULL || a2x == NULL || a2y == NULL || px == NULL || py == NULL)
        return false;
    *out_distance = NULL;
    /* 点到线段距离：投影参数钳位 */
    double a1xd = symbolic_coord_to_double(a1x), a1yd = symbolic_coord_to_double(a1y);
    double a2xd = symbolic_coord_to_double(a2x), a2yd = symbolic_coord_to_double(a2y);
    double pxd = symbolic_coord_to_double(px), pyd = symbolic_coord_to_double(py);
    double dx = a2xd - a1xd, dy = a2yd - a1yd;
    double len_sq = dx * dx + dy * dy;
    double t = 0.0;
    if (len_sq > 0.0)
        t = ((pxd - a1xd) * dx + (pyd - a1yd) * dy) / len_sq;
    t = fmax(0.0, fmin(1.0, t));
    double cx = a1xd + t * dx, cy = a1yd + t * dy;
    double dist = sqrt((pxd - cx) * (pxd - cx) + (pyd - cy) * (pyd - cy));
    *out_distance = dbl_coord(dist);
    return *out_distance != NULL;
}

/* ============================================================
 * Triangle（§12.6）
 * ============================================================ */

bool lv_triangle_area(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx, const SymbolicCoord *by,
                      const SymbolicCoord *cx, const SymbolicCoord *cy, SymbolicCoord **out_area) {
    if (out_area == NULL || ax == NULL || ay == NULL || bx == NULL || by == NULL || cx == NULL || cy == NULL)
        return false;
    *out_area = NULL;
    /* 面积 = |叉积|/2；符号路径：叉积判号后乘 1/2 或 -1/2 */
    SymbolicCoord *u1 = symbolic_coord_subtract(bx, ax);
    SymbolicCoord *u2 = symbolic_coord_subtract(by, ay);
    SymbolicCoord *v1 = symbolic_coord_subtract(cx, ax);
    SymbolicCoord *v2 = symbolic_coord_subtract(cy, ay);
    if (u1 == NULL || u2 == NULL || v1 == NULL || v2 == NULL) {
        pair_destroy(u1, u2);
        pair_destroy(v1, v2);
        return false;
    }
    SymbolicCoord *c1 = symbolic_coord_multiply(u1, v2);
    SymbolicCoord *c2 = symbolic_coord_multiply(u2, v1);
    pair_destroy(u1, u2);
    pair_destroy(v1, v2);
    if (c1 == NULL || c2 == NULL) {
        pair_destroy(c1, c2);
        return false;
    }
    SymbolicCoord *cross = symbolic_coord_subtract(c1, c2); /* 2S（有符号） */
    pair_destroy(c1, c2);
    if (cross == NULL)
        return false;
    SymbolicCoord *half2 = NULL;
    if (symbolic_coord_is_negative(cross)) {
        SymbolicCoord *neg = symbolic_coord_negate(cross);
        symbolic_coord_destroy(cross);
        if (neg == NULL)
            return false;
        half2 = symbolic_coord_create_rational(1, 2);
        if (half2 == NULL) {
            symbolic_coord_destroy(neg);
            return false;
        }
        *out_area = symbolic_coord_multiply(neg, half2);
        symbolic_coord_destroy(neg);
    } else {
        half2 = symbolic_coord_create_rational(1, 2);
        if (half2 == NULL) {
            symbolic_coord_destroy(cross);
            return false;
        }
        *out_area = symbolic_coord_multiply(cross, half2);
    }
    symbolic_coord_destroy(cross);
    symbolic_coord_destroy(half2);
    return *out_area != NULL;
}

bool lv_triangle_centroid(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                          const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                          SymbolicCoord **out_x, SymbolicCoord **out_y) {
    if (out_x == NULL || out_y == NULL || ax == NULL || ay == NULL || bx == NULL || by == NULL || cx == NULL ||
        cy == NULL)
        return false;
    *out_x = NULL;
    *out_y = NULL;
    SymbolicCoord *sx = symbolic_coord_add(ax, bx);
    SymbolicCoord *sy = symbolic_coord_add(ay, by);
    if (sx == NULL || sy == NULL) {
        pair_destroy(sx, sy);
        return false;
    }
    SymbolicCoord *tx = symbolic_coord_add(sx, cx);
    SymbolicCoord *ty = symbolic_coord_add(sy, cy);
    pair_destroy(sx, sy);
    if (tx == NULL || ty == NULL) {
        pair_destroy(tx, ty);
        return false;
    }
    SymbolicCoord *three = symbolic_coord_create_rational(3, 1);
    if (three == NULL) {
        pair_destroy(tx, ty);
        return false;
    }
    *out_x = symbolic_coord_divide(tx, three);
    *out_y = symbolic_coord_divide(ty, three);
    pair_destroy(tx, ty);
    symbolic_coord_destroy(three);
    return *out_x != NULL && *out_y != NULL;
}

bool lv_triangle_circumcenter(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                              const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                              SymbolicCoord **out_x, SymbolicCoord **out_y) {
    if (out_x == NULL || out_y == NULL || ax == NULL || ay == NULL || bx == NULL || by == NULL || cx == NULL ||
        cy == NULL)
        return false;
    *out_x = NULL;
    *out_y = NULL;
    double axd = symbolic_coord_to_double(ax), ayd = symbolic_coord_to_double(ay);
    double bxd = symbolic_coord_to_double(bx), byd = symbolic_coord_to_double(by);
    double cxd = symbolic_coord_to_double(cx), cyd = symbolic_coord_to_double(cy);
    double d = 2.0 * (axd * (byd - cyd) + bxd * (cyd - ayd) + cxd * (ayd - byd));
    if (fabs(d) < 1e-12)
        return false; /* 共线退化 */
    double ux = ((axd * axd + ayd * ayd) * (byd - cyd) + (bxd * bxd + byd * byd) * (cyd - ayd) +
                 (cxd * cxd + cyd * cyd) * (ayd - byd)) /
                d;
    double uy = ((axd * axd + ayd * ayd) * (cxd - bxd) + (bxd * bxd + byd * byd) * (axd - cxd) +
                 (cxd * cxd + cyd * cyd) * (bxd - axd)) /
                d;
    *out_x = dbl_coord(ux);
    *out_y = dbl_coord(uy);
    return *out_x != NULL && *out_y != NULL;
}

bool lv_triangle_orthocenter(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                             const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                             SymbolicCoord **out_x, SymbolicCoord **out_y) {
    if (out_x == NULL || out_y == NULL || ax == NULL || ay == NULL || bx == NULL || by == NULL || cx == NULL ||
        cy == NULL)
        return false;
    *out_x = NULL;
    *out_y = NULL;
    double axd = symbolic_coord_to_double(ax), ayd = symbolic_coord_to_double(ay);
    double bxd = symbolic_coord_to_double(bx), byd = symbolic_coord_to_double(by);
    double cxd = symbolic_coord_to_double(cx), cyd = symbolic_coord_to_double(cy);
    /* 垂心 = A + B + C - 2·O（O 为外心；欧拉线性质，需外心） */
    double ccx, ccy;
    {
        double d = 2.0 * (axd * (byd - cyd) + bxd * (cyd - ayd) + cxd * (ayd - byd));
        if (fabs(d) < 1e-12)
            return false;
        ccx = ((axd * axd + ayd * ayd) * (byd - cyd) + (bxd * bxd + byd * byd) * (cyd - ayd) +
               (cxd * cxd + cyd * cyd) * (ayd - byd)) /
              d;
        ccy = ((axd * axd + ayd * ayd) * (cxd - bxd) + (bxd * bxd + byd * byd) * (axd - cxd) +
               (cxd * cxd + cyd * cyd) * (bxd - axd)) /
              d;
    }
    double hx = axd + bxd + cxd - 2.0 * ccx;
    double hy = ayd + byd + cyd - 2.0 * ccy;
    *out_x = dbl_coord(hx);
    *out_y = dbl_coord(hy);
    return *out_x != NULL && *out_y != NULL;
}

bool lv_triangle_incenter(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                          const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                          SymbolicCoord **out_x, SymbolicCoord **out_y) {
    if (out_x == NULL || out_y == NULL || ax == NULL || ay == NULL || bx == NULL || by == NULL || cx == NULL ||
        cy == NULL)
        return false;
    *out_x = NULL;
    *out_y = NULL;
    double axd = symbolic_coord_to_double(ax), ayd = symbolic_coord_to_double(ay);
    double bxd = symbolic_coord_to_double(bx), byd = symbolic_coord_to_double(by);
    double cxd = symbolic_coord_to_double(cx), cyd = symbolic_coord_to_double(cy);
    double la = sqrt((bxd - cxd) * (bxd - cxd) + (byd - cyd) * (byd - cyd));
    double lb = sqrt((axd - cxd) * (axd - cxd) + (ayd - cyd) * (ayd - cyd));
    double lc = sqrt((axd - bxd) * (axd - bxd) + (ayd - byd) * (ayd - byd));
    double per = la + lb + lc;
    if (per <= 0.0)
        return false;
    double ix = (la * axd + lb * bxd + lc * cxd) / per;
    double iy = (la * ayd + lb * byd + lc * cyd) / per;
    *out_x = dbl_coord(ix);
    *out_y = dbl_coord(iy);
    return *out_x != NULL && *out_y != NULL;
}

bool lv_triangle_nine_point_center(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                                   const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                                   SymbolicCoord **out_x, SymbolicCoord **out_y) {
    /* 九点圆心 = 外心与垂心的中点 */
    SymbolicCoord *ox = NULL, *oy = NULL, *hx = NULL, *hy = NULL;
    if (!lv_triangle_circumcenter(ax, ay, bx, by, cx, cy, &ox, &oy))
        return false;
    bool ok = lv_triangle_orthocenter(ax, ay, bx, by, cx, cy, &hx, &hy);
    if (!ok) {
        pair_destroy(ox, oy);
        return false;
    }
    ok = mid_symbolic(ox, oy, hx, hy, out_x, out_y);
    pair_destroy(ox, oy);
    pair_destroy(hx, hy);
    return ok;
}

bool lv_triangle_excenter(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                          const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                          SymbolicCoord **out_x, SymbolicCoord **out_y) {
    if (out_x == NULL || out_y == NULL || ax == NULL || ay == NULL || bx == NULL || by == NULL || cx == NULL ||
        cy == NULL)
        return false;
    *out_x = NULL;
    *out_y = NULL;
    double axd = symbolic_coord_to_double(ax), ayd = symbolic_coord_to_double(ay);
    double bxd = symbolic_coord_to_double(bx), byd = symbolic_coord_to_double(by);
    double cxd = symbolic_coord_to_double(cx), cyd = symbolic_coord_to_double(cy);
    double la = sqrt((bxd - cxd) * (bxd - cxd) + (byd - cyd) * (byd - cyd));
    double lb = sqrt((axd - cxd) * (axd - cxd) + (ayd - cyd) * (ayd - cyd));
    double lc = sqrt((axd - bxd) * (axd - bxd) + (ayd - byd) * (ayd - byd));
    double denom = -la + lb + lc; /* A 角旁心（对 A 的外旁心） */
    if (fabs(denom) < 1e-12)
        return false;
    double ex = (-la * axd + lb * bxd + lc * cxd) / denom;
    double ey = (-la * ayd + lb * byd + lc * cyd) / denom;
    *out_x = dbl_coord(ex);
    *out_y = dbl_coord(ey);
    return *out_x != NULL && *out_y != NULL;
}

bool lv_triangle_inradius(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                          const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                          SymbolicCoord **out_radius) {
    if (out_radius == NULL || ax == NULL || ay == NULL || bx == NULL || by == NULL || cx == NULL || cy == NULL)
        return false;
    *out_radius = NULL;
    double axd = symbolic_coord_to_double(ax), ayd = symbolic_coord_to_double(ay);
    double bxd = symbolic_coord_to_double(bx), byd = symbolic_coord_to_double(by);
    double cxd = symbolic_coord_to_double(cx), cyd = symbolic_coord_to_double(cy);
    double la = sqrt((bxd - cxd) * (bxd - cxd) + (byd - cyd) * (byd - cyd));
    double lb = sqrt((axd - cxd) * (axd - cxd) + (ayd - cyd) * (ayd - cyd));
    double lc = sqrt((axd - bxd) * (axd - bxd) + (ayd - byd) * (ayd - byd));
    double s = (la + lb + lc) / 2.0;
    double area = fabs((bxd - axd) * (cyd - ayd) - (byd - ayd) * (cxd - axd)) / 2.0;
    if (s <= 0.0)
        return false;
    *out_radius = dbl_coord(area / s);
    return *out_radius != NULL;
}

bool lv_triangle_circumradius(const SymbolicCoord *ax, const SymbolicCoord *ay, const SymbolicCoord *bx,
                              const SymbolicCoord *by, const SymbolicCoord *cx, const SymbolicCoord *cy,
                              SymbolicCoord **out_radius) {
    if (out_radius == NULL || ax == NULL || ay == NULL || bx == NULL || by == NULL || cx == NULL || cy == NULL)
        return false;
    *out_radius = NULL;
    double axd = symbolic_coord_to_double(ax), ayd = symbolic_coord_to_double(ay);
    double bxd = symbolic_coord_to_double(bx), byd = symbolic_coord_to_double(by);
    double cxd = symbolic_coord_to_double(cx), cyd = symbolic_coord_to_double(cy);
    double la = sqrt((bxd - cxd) * (bxd - cxd) + (byd - cyd) * (byd - cyd));
    double lb = sqrt((axd - cxd) * (axd - cxd) + (ayd - cyd) * (ayd - cyd));
    double lc = sqrt((axd - bxd) * (axd - bxd) + (ayd - byd) * (ayd - byd));
    double area = fabs((bxd - axd) * (cyd - ayd) - (byd - ayd) * (cxd - axd)) / 2.0;
    if (area <= 1e-12)
        return false;
    *out_radius = dbl_coord(la * lb * lc / (4.0 * area));
    return *out_radius != NULL;
}

/* ============================================================
 * Intersect（§12.5）
 * ============================================================ */

bool lv_intersect_lines(const SymbolicCoord *p1x, const SymbolicCoord *p1y, const SymbolicCoord *d1x,
                        const SymbolicCoord *d1y, const SymbolicCoord *p2x, const SymbolicCoord *p2y,
                        const SymbolicCoord *d2x, const SymbolicCoord *d2y, SymbolicCoord **out_x,
                        SymbolicCoord **out_y, bool *out_parallel) {
    if (out_x == NULL || out_y == NULL || out_parallel == NULL || p1x == NULL || p1y == NULL || d1x == NULL ||
        d1y == NULL || p2x == NULL || p2y == NULL || d2x == NULL || d2y == NULL)
        return false;
    *out_x = NULL;
    *out_y = NULL;
    *out_parallel = false;
    double p1xd = symbolic_coord_to_double(p1x), p1yd = symbolic_coord_to_double(p1y);
    double d1xd = symbolic_coord_to_double(d1x), d1yd = symbolic_coord_to_double(d1y);
    double p2xd = symbolic_coord_to_double(p2x), p2yd = symbolic_coord_to_double(p2y);
    double d2xd = symbolic_coord_to_double(d2x), d2yd = symbolic_coord_to_double(d2y);
    double denom = d1xd * d2yd - d1yd * d2xd;
    if (fabs(denom) < 1e-12) {
        *out_parallel = true;
        return false;
    }
    double dx = p2xd - p1xd, dy = p2yd - p1yd;
    double t = (dx * d2yd - dy * d2xd) / denom;
    *out_x = dbl_coord(p1xd + t * d1xd);
    *out_y = dbl_coord(p1yd + t * d1yd);
    return *out_x != NULL && *out_y != NULL;
}

bool lv_intersect_circles(const SymbolicCoord *c1x, const SymbolicCoord *c1y, const SymbolicCoord *r1_sq,
                          const SymbolicCoord *c2x, const SymbolicCoord *c2y, const SymbolicCoord *r2_sq,
                          SymbolicCoord **out_p1x, SymbolicCoord **out_p1y, SymbolicCoord **out_p2x,
                          SymbolicCoord **out_p2y, int *out_count) {
    if (out_p1x == NULL || out_p1y == NULL || out_p2x == NULL || out_p2y == NULL || out_count == NULL || c1x == NULL ||
        c1y == NULL || r1_sq == NULL || c2x == NULL || c2y == NULL || r2_sq == NULL)
        return false;
    *out_p1x = NULL;
    *out_p1y = NULL;
    *out_p2x = NULL;
    *out_p2y = NULL;
    *out_count = 0;
    double c1xd = symbolic_coord_to_double(c1x), c1yd = symbolic_coord_to_double(c1y);
    double c2xd = symbolic_coord_to_double(c2x), c2yd = symbolic_coord_to_double(c2y);
    double r1 = sqrt(fabs(symbolic_coord_to_double(r1_sq)));
    double r2 = sqrt(fabs(symbolic_coord_to_double(r2_sq)));
    double dx = c2xd - c1xd, dy = c2yd - c1yd;
    double d = sqrt(dx * dx + dy * dy);
    if (d < 1e-12)
        return true; /* 同心：无交点（out_count=0） */
    double a = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
    double h2 = r1 * r1 - a * a;
    double h = (h2 > 0.0) ? sqrt(h2) : 0.0;
    double mx = c1xd + a * dx / d, my = c1yd + a * dy / d;
    double ux = -dy / d, uy = dx / d;
    if (h2 < -1e-9)
        return true; /* 相离：无交点 */
    if (h2 < 1e-9) {
        /* 相切：1 交点 */
        *out_p1x = dbl_coord(mx);
        *out_p1y = dbl_coord(my);
        *out_count = 1;
        return *out_p1x != NULL && *out_p1y != NULL;
    }
    *out_p1x = dbl_coord(mx + h * ux);
    *out_p1y = dbl_coord(my + h * uy);
    *out_p2x = dbl_coord(mx - h * ux);
    *out_p2y = dbl_coord(my - h * uy);
    *out_count = 2;
    return *out_p1x != NULL && *out_p1y != NULL && *out_p2x != NULL && *out_p2y != NULL;
}

bool lv_intersect_line_circle(const SymbolicCoord *line_px, const SymbolicCoord *line_py, const SymbolicCoord *line_dx,
                              const SymbolicCoord *line_dy, const SymbolicCoord *circle_cx,
                              const SymbolicCoord *circle_cy, const SymbolicCoord *circle_r2, SymbolicCoord **out_p1x,
                              SymbolicCoord **out_p1y, SymbolicCoord **out_p2x, SymbolicCoord **out_p2y,
                              int *out_count) {
    if (out_p1x == NULL || out_p1y == NULL || out_p2x == NULL || out_p2y == NULL || out_count == NULL ||
        line_px == NULL || line_py == NULL || line_dx == NULL || line_dy == NULL || circle_cx == NULL ||
        circle_cy == NULL || circle_r2 == NULL)
        return false;
    *out_p1x = NULL;
    *out_p1y = NULL;
    *out_p2x = NULL;
    *out_p2y = NULL;
    *out_count = 0;
    double px = symbolic_coord_to_double(line_px), py = symbolic_coord_to_double(line_py);
    double vx = symbolic_coord_to_double(line_dx), vy = symbolic_coord_to_double(line_dy);
    double cx = symbolic_coord_to_double(circle_cx), cy = symbolic_coord_to_double(circle_cy);
    double r = sqrt(fabs(symbolic_coord_to_double(circle_r2)));
    double fx = px - cx, fy = py - cy;
    double a = vx * vx + vy * vy;
    if (a < 1e-12)
        return true; /* 直线无方向 */
    double b = 2.0 * (fx * vx + fy * vy);
    double c = fx * fx + fy * fy - r * r;
    double disc = b * b - 4.0 * a * c;
    if (disc < -1e-9)
        return true; /* 相离 */
    if (disc < 1e-9) {
        double t = -b / (2.0 * a);
        *out_p1x = dbl_coord(px + t * vx);
        *out_p1y = dbl_coord(py + t * vy);
        *out_count = 1;
        return *out_p1x != NULL && *out_p1y != NULL;
    }
    double sq = sqrt(disc);
    double t1 = (-b + sq) / (2.0 * a);
    double t2 = (-b - sq) / (2.0 * a);
    *out_p1x = dbl_coord(px + t1 * vx);
    *out_p1y = dbl_coord(py + t1 * vy);
    *out_p2x = dbl_coord(px + t2 * vx);
    *out_p2y = dbl_coord(py + t2 * vy);
    *out_count = 2;
    return *out_p1x != NULL && *out_p1y != NULL && *out_p2x != NULL && *out_p2y != NULL;
}
