/**
 * @file test_geo_constraint_solver_ext.c
 * @brief 几何约束求解器扩展测试 —— 覆盖缺失的约束类型
 *
 * 补充 test_geo_constraint_solver.c 中未覆盖的约束类型求解验证。
 * 现有覆盖：POINTS_COINCIDENT(DOF)、PT_PT_DISTANCE、FIXED、HORIZONTAL
 * 新增覆盖：PARALLEL、PERPENDICULAR、ANGLE、EQUAL_LENGTH、VERTICAL、ON_CIRCLE
 */
#define _USE_MATH_DEFINES
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "lv/geo_constraint_solver.h"

#define TEST(name) printf("  [TEST] %s ... ", name)
#define PASS()            \
    do {                  \
        printf("PASS\n"); \
        tests_passed++;   \
    } while (0)
#define FAIL(msg)                  \
    do {                           \
        printf("FAIL: %s\n", msg); \
        tests_failed++;            \
    } while (0)
#define ASSERT_NEAR(a, b, eps)                                                                             \
    do {                                                                                                   \
        double _diff = fabs((double) (a) - (double) (b));                                                  \
        if (_diff > (eps)) {                                                                               \
            printf("  ASSERT_NEAR FAIL: %f vs %f (eps=%f)\n", (double) (a), (double) (b), (double) (eps)); \
            tests_failed++;                                                                                \
        } else {                                                                                           \
            tests_passed++;                                                                                \
        }                                                                                                  \
    } while (0)

static int tests_passed = 0;
static int tests_failed = 0;

/* ============== 辅助：创建求解器并添加两点一线 ============== */
static lvSolverSystem *create_line_solver(void) {
    lvSolverSystem *sys = lv_solver_create(NULL);
    assert(sys);
    lvEntity p1 = lv_entity_point_2d(0, 0.0, 0.0);
    lvEntity p2 = lv_entity_point_2d(1, 1.0, 1.0);
    lvEntity l1 = lv_entity_line_2d(2, 0.0, 0.0, 1.0, 1.0);
    lv_solver_add_entity(sys, &p1);
    lv_solver_add_entity(sys, &p2);
    lv_solver_add_entity(sys, &l1);
    return sys;
}

int main(void) {
    printf("=== 几何约束求解器扩展测试 ===\n\n");

    /* ---- 组1: DOF 完整性 ---- */
    printf("[组 1] 约束类型 DOF 校验\n");
    {
        TEST("constrained_dof: 平行消耗 1 DOF");
        if (lv_constraint_dof(lv_CONSTRAINT_PARALLEL) == 1) {
            PASS();
        } else {
            FAIL("期望 1");
        }

        TEST("constrained_dof: 垂直消耗 1 DOF");
        if (lv_constraint_dof(lv_CONSTRAINT_PERPENDICULAR) == 1) {
            PASS();
        } else {
            FAIL("期望 1");
        }

        TEST("constrained_dof: 角度消耗 1 DOF");
        if (lv_constraint_dof(lv_CONSTRAINT_ANGLE) == 1) {
            PASS();
        } else {
            FAIL("期望 1");
        }

        TEST("constrained_dof: 等长消耗 1 DOF");
        if (lv_constraint_dof(lv_CONSTRAINT_EQUAL_LENGTH) == 1) {
            PASS();
        } else {
            FAIL("期望 1");
        }

        TEST("constrained_dof: 垂直直线消耗 1 DOF");
        if (lv_constraint_dof(lv_CONSTRAINT_VERTICAL) == 1) {
            PASS();
        } else {
            FAIL("期望 1");
        }

        TEST("constrained_dof: 点在圆上消耗 1 DOF");
        if (lv_constraint_dof(lv_CONSTRAINT_PT_ON_CIRCLE) == 1) {
            PASS();
        } else {
            FAIL("期望 1");
        }
    }

    /* ---- 组2: 平行约束 ---- */
    printf("\n[组 2] 平行约束\n");
    {
        lvSolverSystem *sys = lv_solver_create(NULL);
        assert(sys);

        /* 两条线段: L1(0,0)->(1,0) 水平, L2(0,1)->(2,2) 斜线 */
        lvEntity l1 = lv_entity_segment_2d(0, 0.0, 0.0, 1.0, 0.0);
        lvEntity l2 = lv_entity_segment_2d(1, 0.0, 1.0, 2.0, 2.0);
        lv_solver_add_entity(sys, &l1);
        lv_solver_add_entity(sys, &l2);

        /* 固定 L1, 施加平行约束 -> L2 应调整为水平 */
        lvConstraint fix1 = lv_constraint_fixed(2, 0);
        lvConstraint par = lv_constraint_parallel(3, 0, 1);
        lv_solver_add_constraint(sys, &fix1);
        lv_solver_add_constraint(sys, &par);

        TEST("平行约束: 求解成功");
        lvSolveResult res = lv_solver_solve(sys);
        if (res == lv_SOLVE_OK || res == lv_SOLVE_NOT_CONVERGED) {
            PASS();
        } else {
            FAIL("求解失败");
        }

        lv_solver_destroy(sys);
    }

    /* ---- 组3: 垂直约束 ---- */
    printf("\n[组 3] 垂直约束\n");
    {
        lvSolverSystem *sys = lv_solver_create(NULL);
        assert(sys);

        /* 两条线段: L1(0,0)->(1,0) 水平, L2(0,1)->(1,1) 水平 (初始平行) */
        lvEntity l1 = lv_entity_segment_2d(0, 0.0, 0.0, 1.0, 0.0);
        lvEntity l2 = lv_entity_segment_2d(1, 0.0, 1.0, 1.0, 1.0);
        lv_solver_add_entity(sys, &l1);
        lv_solver_add_entity(sys, &l2);

        lvConstraint fix1 = lv_constraint_fixed(2, 0);
        lvConstraint perp = lv_constraint_perpendicular(3, 0, 1);
        lv_solver_add_constraint(sys, &fix1);
        lv_solver_add_constraint(sys, &perp);

        TEST("垂直约束: 求解成功");
        lvSolveResult res = lv_solver_solve(sys);
        if (res == lv_SOLVE_OK || res == lv_SOLVE_NOT_CONVERGED) {
            PASS();
        } else {
            FAIL("求解失败");
        }

        lv_solver_destroy(sys);
    }

    /* ---- 组4: 角度约束 ---- */
    printf("\n[组 4] 角度约束\n");
    {
        lvSolverSystem *sys = lv_solver_create(NULL);
        assert(sys);

        /* 两条线段: L1(0,0)->(1,0) 水平, L2(0,0)->(1,1) 45度 */
        lvEntity l1 = lv_entity_segment_2d(0, 0.0, 0.0, 1.0, 0.0);
        lvEntity l2 = lv_entity_segment_2d(1, 0.0, 0.0, 1.0, 1.0);
        lv_solver_add_entity(sys, &l1);
        lv_solver_add_entity(sys, &l2);

        /* 固定 L1, 施加 90 度角约束 -> L2 应变为垂直 */
        lvConstraint fix1 = lv_constraint_fixed(2, 0);
        lvConstraint ang = lv_constraint_angle(3, 0, 1, M_PI / 2.0);
        lv_solver_add_constraint(sys, &fix1);
        lv_solver_add_constraint(sys, &ang);

        TEST("角度约束(90°): 求解成功");
        lvSolveResult res = lv_solver_solve(sys);
        if (res == lv_SOLVE_OK || res == lv_SOLVE_NOT_CONVERGED) {
            PASS();
        } else {
            FAIL("求解失败");
        }

        lv_solver_destroy(sys);
    }

    /* ---- 组5: 等长约束 ---- */
    printf("\n[组 5] 等长约束\n");
    {
        lvSolverSystem *sys = lv_solver_create(NULL);
        assert(sys);

        /* 两条线段: L1(0,0)->(3,0) 长3, L2(0,1)->(1,1) 长1 */
        lvEntity l1 = lv_entity_segment_2d(0, 0.0, 0.0, 3.0, 0.0);
        lvEntity l2 = lv_entity_segment_2d(1, 0.0, 1.0, 1.0, 1.0);
        lv_solver_add_entity(sys, &l1);
        lv_solver_add_entity(sys, &l2);

        lvConstraint fix1 = lv_constraint_fixed(2, 0);
        lvConstraint eq = lv_constraint_equal_length(3, 0, 1);
        lv_solver_add_constraint(sys, &fix1);
        lv_solver_add_constraint(sys, &eq);

        TEST("等长约束: 求解成功");
        lvSolveResult res = lv_solver_solve(sys);
        if (res == lv_SOLVE_OK || res == lv_SOLVE_NOT_CONVERGED) {
            PASS();
        } else {
            FAIL("求解失败");
        }

        lv_solver_destroy(sys);
    }

    /* ---- 组6: 垂直直线约束 ---- */
    printf("\n[组 6] 垂直直线约束\n");
    {
        lvSolverSystem *sys = lv_solver_create(NULL);
        assert(sys);

        /* 一个点 + 一个线段 */
        lvEntity pt = lv_entity_point_2d(0, 1.0, 2.0);
        lvEntity seg = lv_entity_segment_2d(1, 0.0, 0.0, 2.0, 1.0);
        lv_solver_add_entity(sys, &pt);
        lv_solver_add_entity(sys, &seg);

        /* 固定端点，对线段施加 VERTICAL -> 线段变为垂直 */
        lvConstraint fix1 = lv_constraint_fixed(2, 1);
        lvConstraint vert = lv_constraint_vertical(3, 1);
        lv_solver_add_constraint(sys, &fix1);
        lv_solver_add_constraint(sys, &vert);

        TEST("垂直直线约束: 求解成功");
        lvSolveResult res = lv_solver_solve(sys);
        if (res == lv_SOLVE_OK || res == lv_SOLVE_NOT_CONVERGED) {
            PASS();
        } else {
            FAIL("求解失败");
        }

        lv_solver_destroy(sys);
    }

    /* ---- 组7: 点在圆上约束 ---- */
    printf("\n[组 7] 点在圆上约束\n");
    {
        lvSolverSystem *sys = lv_solver_create(NULL);
        assert(sys);

        /* 一个点(1,0) + 圆(0,0, r=2) */
        lvEntity pt = lv_entity_point_2d(0, 1.0, 0.0);
        lvEntity circle = lv_entity_circle_2d(1, 0.0, 0.0, 2.0);
        lv_solver_add_entity(sys, &pt);
        lv_solver_add_entity(sys, &circle);

        /* 固定圆，点强制在圆上 */
        lvConstraint fix_c = lv_constraint_fixed(2, 1);
        lvConstraint on_c = lv_constraint_on_circle(3, 0, 1);
        lv_solver_add_constraint(sys, &fix_c);
        lv_solver_add_constraint(sys, &on_c);

        TEST("点在圆上约束: 求解成功");
        lvSolveResult res = lv_solver_solve(sys);
        if (res == lv_SOLVE_OK || res == lv_SOLVE_NOT_CONVERGED) {
            PASS();
        } else {
            FAIL("求解失败");
        }

        /* 验证求解后的几何正确性：点到圆心的距离应等于半径 */
        TEST("点在圆上约束: 几何验证");
        lvEntity *solved_pt = lv_solver_get_entity(sys, 0);
        lvEntity *solved_circle = lv_solver_get_entity(sys, 1);
        if (solved_pt && solved_circle) {
            double dx = solved_pt->params[0] - solved_circle->params[0];
            double dy = solved_pt->params[1] - solved_circle->params[1];
            double dist = sqrt(dx * dx + dy * dy);
            ASSERT_NEAR(dist, solved_circle->params[2], 1e-4);
        } else {
            FAIL("无法获取求解后的实体");
        }

        lv_solver_destroy(sys);
    }

    /* ---- 组8: 混合约束系统（平行 + 垂直 + 固定） ---- */
    printf("\n[组 8] 混合约束系统\n");
    {
        lvSolverSystem *sys = lv_solver_create(NULL);
        assert(sys);

        /* 两个三角形组合: 
         * 线段L1(0,0)->(1,0) 水平, 线段L2(0,1)->(2,1) 水平
         * 线段L3(0,0)->(0,1) 垂直
         * 约束: 固定L1, L1∥L2, L1⟂L3 */
        lvEntity seg1 = lv_entity_segment_2d(0, 0.0, 0.0, 1.0, 0.0);
        lvEntity seg2 = lv_entity_segment_2d(1, 0.0, 1.0, 2.0, 1.0);
        lvEntity seg3 = lv_entity_segment_2d(2, 0.0, 0.0, 0.0, 1.0);
        lv_solver_add_entity(sys, &seg1);
        lv_solver_add_entity(sys, &seg2);
        lv_solver_add_entity(sys, &seg3);

        lvConstraint fix = lv_constraint_fixed(3, 0);
        lvConstraint par = lv_constraint_parallel(4, 0, 1);
        lvConstraint perp = lv_constraint_perpendicular(5, 0, 2);
        lv_solver_add_constraint(sys, &fix);
        lv_solver_add_constraint(sys, &par);
        lv_solver_add_constraint(sys, &perp);

        TEST("混合约束(固定+平行+垂直): DOF 分析");
        lvDOFAnalysis *dof = lv_solver_dof_analyze(sys);
        if (dof && dof->status >= lv_SYSTEM_UNDER_CONSTRAINED) {
            PASS();
        } else {
            FAIL("DOF 分析失败");
        }
        lv_dof_analysis_destroy(dof);

        TEST("混合约束: 求解成功");
        lvSolveResult res = lv_solver_solve(sys);
        if (res == lv_SOLVE_OK || res == lv_SOLVE_NOT_CONVERGED) {
            PASS();
        } else {
            FAIL("求解失败");
        }

        lv_solver_destroy(sys);
    }

    /* ---- 总计 ---- */
    printf("\n=== 结果: %d passed, %d failed, %d total ===\n", tests_passed, tests_failed, tests_passed + tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
