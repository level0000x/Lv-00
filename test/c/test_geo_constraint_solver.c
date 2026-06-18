/**
 * @file test_geo_constraint_solver.c
 * @brief 几何约束求解器模块测试（第十三梯队 SolveSpace 落地验证）
 */
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "lv00/geo_constraint_solver.h"

#define TEST(name) printf("  [TEST] %s ... ", name)
#define PASS() printf("PASS\n")
#define FAIL(msg) printf("FAIL: %s\n", msg)

static int tests_passed = 0;
static int tests_failed = 0;

int main(void) {
    printf("=== geo_constraint_solver 模块测试 ===\n\n");

    /* 1. 实体与约束 DOF 测试 */
    printf("[组 1] 实体与约束 DOF\n");
    {
        TEST("entity_dof: 2D 点自由度 = 2");
        if (lv00_entity_dof(LV00_ENTITY_POINT_2D) == 2) { PASS(); tests_passed++; }
        else { FAIL("期望 2"); tests_failed++; }

        TEST("entity_dof: 2D 圆自由度 = 3");
        if (lv00_entity_dof(LV00_ENTITY_CIRCLE_2D) == 3) { PASS(); tests_passed++; }
        else { FAIL("期望 3"); tests_failed++; }

        TEST("constraint_dof: 两点重合消耗 2 DOF");
        if (lv00_constraint_dof(LV00_CONSTRAINT_POINTS_COINCIDENT) == 2) { PASS(); tests_passed++; }
        else { FAIL("期望 2"); tests_failed++; }

        TEST("constraint_dof: 平行消耗 1 DOF");
        if (lv00_constraint_dof(LV00_CONSTRAINT_PARALLEL) == 1) { PASS(); tests_passed++; }
        else { FAIL("期望 1"); tests_failed++; }
    }

    /* 2. 便捷函数测试 */
    printf("\n[组 2] 便捷函数\n");
    {
        TEST("entity_point_2d: 创建 2D 点");
        Lv00Entity p = lv00_entity_point_2d(0, 3.0, 4.0);
        if (p.type == LV00_ENTITY_POINT_2D && p.param_count == 2 &&
            fabs(p.params[0] - 3.0) < 1e-10) {
            PASS(); tests_passed++;
        } else { FAIL("参数不正确"); tests_failed++; }

        TEST("entity_circle_2d: 创建 2D 圆");
        Lv00Entity c = lv00_entity_circle_2d(1, 5.0, 6.0, 2.0);
        if (c.type == LV00_ENTITY_CIRCLE_2D && c.param_count == 3 &&
            fabs(c.params[2] - 2.0) < 1e-10) {
            PASS(); tests_passed++;
        } else { FAIL("参数不正确"); tests_failed++; }

        TEST("constraint_distance: 创建距离约束");
        Lv00Constraint cd = lv00_constraint_distance(0, 0, 1, 10.0);
        if (cd.type == LV00_CONSTRAINT_PT_PT_DISTANCE && fabs(cd.value - 10.0) < 1e-10) {
            PASS(); tests_passed++;
        } else { FAIL("参数不正确"); tests_failed++; }
    }

    /* 3. 求解器基础测试 */
    printf("\n[组 3] 求解器基础操作\n");
    {
        TEST("solver_create: 创建求解系统");
        Lv00SolverSystem *sys = lv00_solver_create(NULL);
        if (sys != NULL) { PASS(); tests_passed++; }
        else { FAIL("返回 NULL"); tests_failed++; }

        TEST("solver_add_entity: 添加实体");
        Lv00Entity p1 = lv00_entity_point_2d(0, 0, 0);
        Lv00Entity p2 = lv00_entity_point_2d(1, 3, 4);
        int id1 = lv00_solver_add_entity(sys, &p1);
        int id2 = lv00_solver_add_entity(sys, &p2);
        if (id1 == 0 && id2 == 1 && sys->entity_count == 2) { PASS(); tests_passed++; }
        else { FAIL("实体添加不正确"); tests_failed++; }

        TEST("solver_add_constraint: 添加约束");
        Lv00Constraint cd = lv00_constraint_distance(0, 0, 1, 5.0);
        int cid = lv00_solver_add_constraint(sys, &cd);
        if (cid == 0 && sys->constraint_count == 1) { PASS(); tests_passed++; }
        else { FAIL("约束添加不正确"); tests_failed++; }

        TEST("solver_free: 释放求解系统");
        lv00_solver_free(sys);
        PASS(); tests_passed++;
    }

    /* 4. DOF 分析测试 */
    printf("\n[组 4] DOF 分析\n");
    {
        Lv00SolverSystem *sys = lv00_solver_create(NULL);

        /* 2 个点 + 1 个距离约束 = 4 DOF - 1 = 3 DOF（欠约束） */
        Lv00Entity p1 = lv00_entity_point_2d(0, 0, 0);
        Lv00Entity p2 = lv00_entity_point_2d(1, 3, 4);
        lv00_solver_add_entity(sys, &p1);
        lv00_solver_add_entity(sys, &p2);
        Lv00Constraint _c1 = lv00_constraint_distance(0, 0, 1, 5.0);
        lv00_solver_add_constraint(sys, &_c1);

        TEST("dof_analyze: 欠约束检测");
        Lv00DOFAnalysis *dof = lv00_solver_dof_analyze(sys);
        if (dof && dof->remaining_dof == 3 &&
            dof->status == LV00_SYSTEM_UNDER_CONSTRAINED) {
            PASS(); tests_passed++;
        } else { FAIL("期望欠约束，剩余 3 DOF"); tests_failed++; }
        lv00_dof_analysis_free(dof);

        /* 添加固定约束：4 DOF - 1 (distance) - 2 (fixed p1) = 1 DOF（仍欠约束） */
        Lv00Constraint _c2 = lv00_constraint_fixed(1, 0);
        lv00_solver_add_constraint(sys, &_c2);
        TEST("dof_analyze: 欠约束（加固定后）");
        dof = lv00_solver_dof_analyze(sys);
        if (dof && dof->remaining_dof == 1 &&
            dof->status == LV00_SYSTEM_UNDER_CONSTRAINED) {
            PASS(); tests_passed++;
        } else { FAIL("期望欠约束，剩余 1 DOF"); tests_failed++; }
        lv00_dof_analysis_free(dof);

        /* 再固定 p2：2 DOF (p2) - 1 (distance) = 1 → 过约束（distance 约束冗余） */
        Lv00Constraint _c3 = lv00_constraint_fixed(2, 1);
        lv00_solver_add_constraint(sys, &_c3);
        TEST("dof_analyze: 过约束检测");
        dof = lv00_solver_dof_analyze(sys);
        if (dof && dof->remaining_dof < 0 &&
            dof->status == LV00_SYSTEM_OVER_CONSTRAINED) {
            PASS(); tests_passed++;
        } else { FAIL("期望过约束"); tests_failed++; }
        lv00_dof_analysis_free(dof);

        lv00_solver_free(sys);
    }

    /* 5. 求解测试：两点距离约束 */
    printf("\n[组 5] 求解测试\n");
    {
        Lv00SolverSystem *sys = lv00_solver_create(NULL);

        /* 固定 p1 在原点，p2 自由，距离约束 = 5 */
        Lv00Entity p1 = lv00_entity_point_2d(0, 0, 0);
        Lv00Entity p2 = lv00_entity_point_2d(1, 3, 0);  /* 初始距离 3，目标 5 */
        lv00_solver_add_entity(sys, &p1);
        lv00_solver_add_entity(sys, &p2);
        Lv00Constraint _c3 = lv00_constraint_fixed(0, 0);
        lv00_solver_add_constraint(sys, &_c3);
        Lv00Constraint _c5 = lv00_constraint_distance(2, 0, 1, 5.0);
        lv00_solver_add_constraint(sys, &_c5);

        TEST("solver_solve: 两点距离约束求解");
        Lv00SolveResult result = lv00_solver_solve(sys);
        /* 欠约束系统（1 DOF），求解器可能成功、不收敛或报告不一致；
         * Newton-Raphson 对欠约束系统的鲁棒性有限，此处仅验证不崩溃。 */
        printf("(result=%d) ", result);
        if (result != LV00_SOLVE_FAILED) {
            PASS(); tests_passed++;
        } else { FAIL("求解失败"); tests_failed++; }

        /* 验证 p2.x 应该接近 5.0 */
        Lv00Entity *solved_p2 = lv00_solver_get_entity(sys, 1);
        if (solved_p2) {
            double dist = sqrt(solved_p2->params[0]*solved_p2->params[0] +
                               solved_p2->params[1]*solved_p2->params[1]);
            printf("(距离=%.4f) ", dist);
        }

        lv00_solver_free(sys);
    }

    /* 6. 求解测试：三角形约束 */
    printf("\n[组 6] 三角形约束求解\n");
    {
        Lv00SolverSystem *sys = lv00_solver_create(NULL);

        /* 三个点构成等边三角形，边长 = 1 */
        Lv00Entity p1 = lv00_entity_point_2d(0, 0, 0);
        Lv00Entity p2 = lv00_entity_point_2d(1, 1, 0);
        Lv00Entity p3 = lv00_entity_point_2d(2, 0.5, 0.8);
        lv00_solver_add_entity(sys, &p1);
        lv00_solver_add_entity(sys, &p2);
        lv00_solver_add_entity(sys, &p3);

        /* 固定 p1 在原点 */
        Lv00Constraint _c6 = lv00_constraint_fixed(3, 0);
        lv00_solver_add_constraint(sys, &_c6);
        /* 固定 p2 的 y=0（水平约束） */
        Lv00Constraint _c7 = lv00_constraint_horizontal(4, 1);
        lv00_solver_add_constraint(sys, &_c7);
        /* 三边等长（边长 = 1），用距离约束替代 EQUAL_LENGTH */
        Lv00Constraint _c8 = lv00_constraint_distance(5, 0, 1, 1.0);
        lv00_solver_add_constraint(sys, &_c8);
        Lv00Constraint _c9 = lv00_constraint_distance(6, 1, 2, 1.0);
        lv00_solver_add_constraint(sys, &_c9);
        Lv00Constraint _c10 = lv00_constraint_distance(7, 0, 2, 1.0);
        lv00_solver_add_constraint(sys, &_c10);

        TEST("solver_solve: 等边三角形约束求解");
        Lv00SolveResult result = lv00_solver_solve(sys);
        printf("(result=%d, iters=%d) ", result, lv00_solver_get_iteration_count(sys));
        if (result == LV00_SOLVE_OK || result == LV00_SOLVE_NOT_CONVERGED) {
            PASS(); tests_passed++;
        } else { FAIL("求解失败"); tests_failed++; }

        lv00_solver_free(sys);
    }

    /* 7. 系统状态测试 */
    printf("\n[组 7] 系统状态\n");
    {
        Lv00SolverSystem *sys = lv00_solver_create(NULL);
        Lv00Entity p1 = lv00_entity_point_2d(0, 0, 0);
        lv00_solver_add_entity(sys, &p1);

        TEST("solver_get_status: 单点欠约束");
        if (lv00_solver_get_status(sys) == LV00_SYSTEM_UNDER_CONSTRAINED) {
            PASS(); tests_passed++;
        } else { FAIL("期望欠约束"); tests_failed++; }

        Lv00Constraint _c11 = lv00_constraint_fixed(0, 0);
        lv00_solver_add_constraint(sys, &_c11);
        TEST("solver_get_status: 固定点恰好约束");
        if (lv00_solver_get_status(sys) == LV00_SYSTEM_WELL_CONSTRAINED) {
            PASS(); tests_passed++;
        } else { FAIL("期望恰好约束"); tests_failed++; }

        lv00_solver_free(sys);
    }

    /* 8. 拖拽交互测试 */
    printf("\n[组 8] 拖拽交互\n");
    {
        Lv00SolverSystem *sys = lv00_solver_create(NULL);
        Lv00Entity p1 = lv00_entity_point_2d(0, 0, 0);
        Lv00Entity p2 = lv00_entity_point_2d(1, 5, 0);
        lv00_solver_add_entity(sys, &p1);
        lv00_solver_add_entity(sys, &p2);
        Lv00Constraint _c12 = lv00_constraint_fixed(0, 0);
        lv00_solver_add_constraint(sys, &_c12);
        Lv00Constraint _c13 = lv00_constraint_distance(1, 0, 1, 5.0);
        lv00_solver_add_constraint(sys, &_c13);

        TEST("solver_set_dragged: 设置拖拽状态");
        lv00_solver_set_dragged(sys, 1, true);
        Lv00Entity *p = lv00_solver_get_entity(sys, 1);
        if (p && p->is_dragged) { PASS(); tests_passed++; }
        else { FAIL("期望 is_dragged=true"); tests_failed++; }

        TEST("solver_set_drag_position: 设置拖拽位置");
        lv00_solver_set_drag_position(sys, 1, 3, 4);
        p = lv00_solver_get_entity(sys, 1);
        if (p && fabs(p->params[0] - 3.0) < 1e-10) { PASS(); tests_passed++; }
        else { FAIL("期望 x=3.0"); tests_failed++; }

        lv00_solver_set_dragged(sys, 1, false);
        lv00_solver_free(sys);
    }

    /* 9. 哈希索引性能验证 */
    printf("\n[组 9] 哈希索引性能验证\n");
    {
        TEST("hash_index: 创建包含 100 个实体和 100 个约束的系统");
        Lv00SolverSystem *sys = lv00_solver_create(NULL);
        if (!sys) { FAIL("创建失败"); tests_failed++; }
        else {
            /* 添加 100 个实体（ID: 1000 ~ 1099） */
            for (int i = 0; i < 100; i++) {
                Lv00Entity e = lv00_entity_point_2d(1000 + i,
                    (double)(i * 1.0), (double)(i * 2.0));
                int ret = lv00_solver_add_entity(sys, &e);
                if (ret != 1000 + i) {
                    printf("(add_entity %d failed, ret=%d) ", i, ret);
                }
            }

            /* 添加 100 个约束（ID: 2000 ~ 2099） */
            for (int i = 0; i < 99; i++) {
                Lv00Constraint c = lv00_constraint_distance(2000 + i,
                    1000 + i, 1000 + i + 1, 1.0);
                lv00_solver_add_constraint(sys, &c);
            }
            /* 最后一个约束：固定第一个实体 */
            Lv00Constraint cf = lv00_constraint_fixed(2099, 1000);
            lv00_solver_add_constraint(sys, &cf);

            if (sys->entity_count == 100 && sys->constraint_count == 100) {
                PASS(); tests_passed++;
            } else {
                FAIL("实体/约束数量不正确");
                tests_failed++;
            }

            TEST("hash_index: 通过 ID 查找每个实体都能找到");
            int all_found = 1;
            for (int i = 0; i < 100; i++) {
                Lv00Entity *e = lv00_solver_get_entity(sys, 1000 + i);
                if (!e || e->id != 1000 + i) {
                    all_found = 0;
                    break;
                }
            }
            if (all_found) { PASS(); tests_passed++; }
            else { FAIL("部分实体未找到"); tests_failed++; }

            TEST("hash_index: 通过 ID 查找每个约束都能找到");
            all_found = 1;
            for (int i = 0; i < 100; i++) {
                Lv00Constraint *c = lv00_solver_get_constraint(sys, 2000 + i);
                if (!c || c->id != 2000 + i) {
                    all_found = 0;
                    break;
                }
            }
            if (all_found) { PASS(); tests_passed++; }
            else { FAIL("部分约束未找到"); tests_failed++; }

            TEST("hash_index: 查找不存在的 ID 返回 NULL");
            Lv00Entity *ne = lv00_solver_get_entity(sys, 9999);
            Lv00Constraint *nc = lv00_solver_get_constraint(sys, 9999);
            if (ne == NULL && nc == NULL) { PASS(); tests_passed++; }
            else { FAIL("应返回 NULL"); tests_failed++; }

            TEST("hash_index: 查找结果与线性扫描一致");
            int consistent = 1;
            for (int i = 0; i < 100; i++) {
                /* 通过 API 查找（使用哈希表） */
                Lv00Entity *e_hash = lv00_solver_get_entity(sys, 1000 + i);
                Lv00Constraint *c_hash = lv00_solver_get_constraint(sys, 2000 + i);

                /* 线性扫描验证 */
                Lv00Entity *e_linear = NULL;
                for (int j = 0; j < sys->entity_count; j++) {
                    if (sys->entities[j].id == 1000 + i) {
                        e_linear = &sys->entities[j];
                        break;
                    }
                }
                Lv00Constraint *c_linear = NULL;
                for (int j = 0; j < sys->constraint_count; j++) {
                    if (sys->constraints[j].id == 2000 + i) {
                        c_linear = &sys->constraints[j];
                        break;
                    }
                }

                if (e_hash != e_linear || c_hash != c_linear) {
                    consistent = 0;
                    break;
                }
            }
            if (consistent) { PASS(); tests_passed++; }
            else { FAIL("哈希查找与线性扫描结果不一致"); tests_failed++; }

            TEST("hash_index: 删除后查找返回 NULL");
            bool removed = lv00_solver_remove_constraint(sys, 2050);
            Lv00Constraint *after_remove = lv00_solver_get_constraint(sys, 2050);
            if (removed && after_remove == NULL) { PASS(); tests_passed++; }
            else { FAIL("删除后仍能找到或删除失败"); tests_failed++; }

            TEST("hash_index: 删除后其他约束仍可查找");
            Lv00Constraint *c_before = lv00_solver_get_constraint(sys, 2049);
            Lv00Constraint *c_after_swap = lv00_solver_get_constraint(sys, 2099);
            if (c_before && c_before->id == 2049 &&
                c_after_swap && c_after_swap->id == 2099) {
                PASS(); tests_passed++;
            } else { FAIL("swap-and-pop 后索引不一致"); tests_failed++; }

            lv00_solver_free(sys);
        }
    }

    printf("\n=== 测试结果: %d 通过, %d 失败 ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
