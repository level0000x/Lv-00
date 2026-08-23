/**
 * @file test_parametric_curves_ext.c
 * @brief 参数曲线曲面契约测试（批次 C-㊺续21：parametric_curves.h 13 个零覆盖 API）
 *
 * 覆盖零覆盖 API：
 *   曲线族：curve_create / destroy / evaluate / tangent / arc_length /
 *     get_domain / is_closed
 *   曲面族：surface_create / destroy / evaluate / normal / area /
 *     get_domain
 *
 * 契约要点（与实现核对）：
 *   - create：eval_func NULL 或域无效（t_min >= t_max）→ NULL。
 *   - evaluate/tangent：域外（含 epsilon 容差）→ false 且不静默外推；
 *     微小越界钳制到边界。
 *   - arc_length：deriv_func NULL → -1.0；n_steps<=0 用默认 64；超限钳制。
 *   - 圆曲线（参数化）：弧长 ≈ 2πr；直线切线常数。
 *   - 平面曲面（z=0 平面）：法向量 z 方向；面积 = 矩形面积。
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/parametric_curves.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试辅助：圆曲线 ============== */

typedef struct {
    double radius;
    double cx, cy;
} CircleData;

static void circle_eval(double t, void *ud, lvPoint2D *out) {
    CircleData *c = (CircleData *) ud;
    out->x = c->cx + c->radius * cos(t);
    out->y = c->cy + c->radius * sin(t);
}

static void circle_deriv(double t, void *ud, double *dx, double *dy) {
    CircleData *c = (CircleData *) ud;
    *dx = -c->radius * sin(t);
    *dy = c->radius * cos(t);
}

typedef struct {
    double a, b, c, d; /* x=a*t+b, y=c*t+d */
} LineData;

static void line_eval(double t, void *ud, lvPoint2D *out) {
    LineData *l = (LineData *) ud;
    out->x = l->a * t + l->b;
    out->y = l->c * t + l->d;
}

static void line_deriv(double t, void *ud, double *dx, double *dy) {
    (void) t;
    LineData *l = (LineData *) ud;
    *dx = l->a;
    *dy = l->c;
}

/* ============== 测试：曲线 ============== */

static void test_curve_api(void) {
    CircleData cdata = {1.0, 0.0, 0.0};

    /* create：单位圆 [0, 2π] 闭合 */
    struct lvParametricCurve *c = lv_curve_create(0.0, 2.0 * M_PI, circle_eval, circle_deriv, &cdata, true);
    TEST_ASSERT_NOT_NULL(c);

    /* is_closed / get_domain */
    TEST_ASSERT(lv_curve_is_closed(c), "闭合曲线");
    double tmin = -1, tmax = -1;
    TEST_ASSERT(lv_curve_get_domain(c, &tmin, &tmax), "获取域");
    TEST_ASSERT_DOUBLE(tmin, 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(tmax, 2.0 * M_PI, 1e-12);

    /* evaluate：t=0 → (1,0)，t=π/2 → (0,1) */
    lvPoint2D p;
    TEST_ASSERT(lv_curve_evaluate(c, 0.0, &p), "求值 t=0");
    TEST_ASSERT_DOUBLE(p.x, 1.0, 1e-9);
    TEST_ASSERT_DOUBLE(p.y, 0.0, 1e-9);
    TEST_ASSERT(lv_curve_evaluate(c, M_PI / 2.0, &p), "求值 t=π/2");
    TEST_ASSERT_DOUBLE(p.x, 0.0, 1e-9);
    TEST_ASSERT_DOUBLE(p.y, 1.0, 1e-9);

    /* evaluate 域外 → false（不静默外推） */
    TEST_ASSERT(!lv_curve_evaluate(c, -0.5, &p), "域外下界拒绝");
    TEST_ASSERT(!lv_curve_evaluate(c, 2.0 * M_PI + 0.5, &p), "域外上界拒绝");

    /* tangent：t=0 → (0, 1)（dx=-r sin 0=0, dy=r cos 0=1） */
    double dx = 0, dy = 0;
    TEST_ASSERT(lv_curve_tangent(c, 0.0, &dx, &dy), "切线 t=0");
    TEST_ASSERT_DOUBLE(dx, 0.0, 1e-9);
    TEST_ASSERT_DOUBLE(dy, 1.0, 1e-9);

    /* arc_length：单位圆周长 ≈ 2π */
    double len = lv_curve_arc_length(c, 1024);
    TEST_ASSERT(len > 0, "弧长为正");
    TEST_ASSERT_DOUBLE(len, 2.0 * M_PI, 1e-3);

    /* 非闭合直线：长度 = 线段长 */
    LineData ldata = {3.0, 0.0, 4.0, 0.0}; /* x=3t, y=4t, t∈[0,1] → 长 5 */
    struct lvParametricCurve *line = lv_curve_create(0.0, 1.0, line_eval, line_deriv, &ldata, false);
    TEST_ASSERT_NOT_NULL(line);
    TEST_ASSERT(!lv_curve_is_closed(line), "直线非闭合");
    len = lv_curve_arc_length(line, 0); /* 默认步数 */
    TEST_ASSERT_DOUBLE(len, 5.0, 1e-9);
    lv_curve_destroy(line);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_curve_create(0, 1, NULL, line_deriv, NULL, false));   /* eval NULL */
    TEST_ASSERT_NULL(lv_curve_create(1, 1, circle_eval, circle_deriv, NULL, false)); /* 域无效 */
    TEST_ASSERT_NULL(lv_curve_create(2, 1, circle_eval, circle_deriv, NULL, false)); /* t_min>t_max */
    TEST_ASSERT(!lv_curve_evaluate(NULL, 0, &p), "eval NULL curve");
    TEST_ASSERT(!lv_curve_evaluate(c, 0, NULL), "eval NULL out");
    TEST_ASSERT(!lv_curve_tangent(NULL, 0, &dx, &dy), "tangent NULL curve");
    TEST_ASSERT(!lv_curve_tangent(c, 0, NULL, &dy), "tangent NULL dx");
    TEST_ASSERT(!lv_curve_get_domain(NULL, &tmin, &tmax), "domain NULL curve");
    TEST_ASSERT(!lv_curve_get_domain(c, NULL, &tmax), "domain NULL out");
    TEST_ASSERT(!lv_curve_is_closed(NULL), "is_closed NULL");
    /* 无导数函数：tangent/arc_length 失败 */
    struct lvParametricCurve *no_deriv = lv_curve_create(0, 1, circle_eval, NULL, &cdata, false);
    TEST_ASSERT_NOT_NULL(no_deriv);
    TEST_ASSERT(!lv_curve_tangent(no_deriv, 0.5, &dx, &dy), "无导数 tangent 失败");
    TEST_ASSERT_DOUBLE(lv_curve_arc_length(no_deriv, 10), -1.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_curve_arc_length(NULL, 10), -1.0, 1e-12);
    lv_curve_destroy(no_deriv);

    lv_curve_destroy(c);
    lv_curve_destroy(NULL);
    printf("  test_curve_api: PASSED\n");
}

/* ============== 测试辅助：平面曲面 ============== */

typedef struct {
    double z; /* 平面高度 */
} PlaneData;

static void plane_eval(double u, double v, void *ud, lvPoint3D *out) {
    PlaneData *pd = (PlaneData *) ud;
    out->x = u;
    out->y = v;
    out->z = pd->z;
}

static void plane_deriv(double u, double v, void *ud, lvPoint3D *du, lvPoint3D *dv) {
    (void) u;
    (void) v;
    (void) ud;
    du->x = 1.0;
    du->y = 0.0;
    du->z = 0.0;
    dv->x = 0.0;
    dv->y = 1.0;
    dv->z = 0.0;
}

/* ============== 测试：曲面 ============== */

static void test_surface_api(void) {
    PlaneData pdata = {5.0};

    /* create：平面 u∈[0,2], v∈[0,3] */
    struct lvParametricSurface *s = lv_surface_create(0.0, 2.0, 0.0, 3.0, plane_eval, plane_deriv, &pdata);
    TEST_ASSERT_NOT_NULL(s);

    /* get_domain */
    lvParametricDomain2D dom;
    TEST_ASSERT(lv_surface_get_domain(s, &dom), "获取域");
    TEST_ASSERT_DOUBLE(dom.u_min, 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(dom.u_max, 2.0, 1e-12);
    TEST_ASSERT_DOUBLE(dom.v_min, 0.0, 1e-12);
    TEST_ASSERT_DOUBLE(dom.v_max, 3.0, 1e-12);

    /* evaluate：(1,1) → (1,1,5) */
    lvPoint3D pt;
    TEST_ASSERT(lv_surface_evaluate(s, 1.0, 1.0, &pt), "求值");
    TEST_ASSERT_DOUBLE(pt.x, 1.0, 1e-9);
    TEST_ASSERT_DOUBLE(pt.y, 1.0, 1e-9);
    TEST_ASSERT_DOUBLE(pt.z, 5.0, 1e-9);

    /* normal：du × dv = (0,0,1) */
    double nx = 0, ny = 0, nz = 0;
    TEST_ASSERT(lv_surface_normal(s, 1.0, 1.0, &nx, &ny, &nz), "法向量");
    TEST_ASSERT_DOUBLE(nx, 0.0, 1e-9);
    TEST_ASSERT_DOUBLE(ny, 0.0, 1e-9);
    TEST_ASSERT_DOUBLE(nz, 1.0, 1e-9);

    /* area：2×3 矩形 = 6 */
    double area = lv_surface_area(s, 32, 32);
    TEST_ASSERT(area > 0, "面积为正");
    TEST_ASSERT_DOUBLE(area, 6.0, 1e-6);

    /* NULL 契约 */
    TEST_ASSERT_NULL(lv_surface_create(0, 1, 0, 1, NULL, plane_deriv, NULL));     /* eval NULL */
    TEST_ASSERT_NULL(lv_surface_create(1, 1, 0, 1, plane_eval, plane_deriv, NULL)); /* u 域无效 */
    TEST_ASSERT_NULL(lv_surface_create(0, 1, 1, 1, plane_eval, plane_deriv, NULL)); /* v 域无效 */
    TEST_ASSERT(!lv_surface_evaluate(NULL, 0, 0, &pt), "eval NULL surf");
    TEST_ASSERT(!lv_surface_evaluate(s, 0, 0, NULL), "eval NULL out");
    TEST_ASSERT(!lv_surface_normal(NULL, 0, 0, &nx, &ny, &nz), "normal NULL surf");
    TEST_ASSERT(!lv_surface_normal(s, 0, 0, NULL, &ny, &nz), "normal NULL out");
    TEST_ASSERT(!lv_surface_get_domain(NULL, &dom), "domain NULL surf");
    TEST_ASSERT(!lv_surface_get_domain(s, NULL), "domain NULL out");
    /* 无偏导数函数：normal/area 失败 */
    struct lvParametricSurface *no_deriv = lv_surface_create(0, 1, 0, 1, plane_eval, NULL, &pdata);
    TEST_ASSERT_NOT_NULL(no_deriv);
    TEST_ASSERT(!lv_surface_normal(no_deriv, 0.5, 0.5, &nx, &ny, &nz), "无导数 normal 失败");
    TEST_ASSERT_DOUBLE(lv_surface_area(no_deriv, 8, 8), -1.0, 1e-12);
    TEST_ASSERT_DOUBLE(lv_surface_area(NULL, 8, 8), -1.0, 1e-12);
    lv_surface_destroy(no_deriv);

    lv_surface_destroy(s);
    lv_surface_destroy(NULL);
    printf("  test_surface_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Parametric Curves Ext Test Suite")
    printf("=== Lv-00 Parametric Curves Ext Test Suite (batch C-㊺续21) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_curve_api);
    TEST_MAIN_RUN(test_surface_api);

    lv_cleanup();
TEST_MAIN_END()
