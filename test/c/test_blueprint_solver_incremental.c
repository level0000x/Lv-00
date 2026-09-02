/**
 * @file test_blueprint_solver_incremental.c
 * @brief 蓝图增量求解契约测试（TEN_LAYER_OPTIMIZED_PLAN §15.4，批次 G5c）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"
#include "lv/blueprint_solver_incremental.h"

int g_pass_count = 0;
int g_fail_count = 0;

static void test_lifecycle(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    lvIncrementalSolver *s = lv_solver_incremental_create(g);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT(s->graph == g, "graph 借用");
    TEST_ASSERT(!s->is_valid, "初始缓存无效");
    lv_solver_incremental_destroy(s);
    lv_solver_incremental_destroy(NULL); /* NULL 安全 */
    TEST_ASSERT_NULL(lv_solver_incremental_create(NULL));
    graph_destroy(g);
}

static void test_solve_empty_graph(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    lvIncrementalSolver *s = lv_solver_incremental_create(g);
    TEST_ASSERT_NOT_NULL(s);
    lvSolveResult result = lv_SOLVE_FAILED;
    TEST_ASSERT(lv_solve_incremental(s, &result), "空图求解成功");
    TEST_ASSERT(s->is_valid, "求解后缓存有效");
    TEST_ASSERT(!lv_solve_incremental(NULL, &result), "NULL 拒绝");
    lv_solver_incremental_destroy(s);
    graph_destroy(g);
}

static void test_solve_with_points(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    /* 两个点 + 一条线段 */
    add_point(g, 0, 1, 0, 1); /* p0 (0,0) */
    add_point(g, 4, 1, 0, 1); /* p1 (4,0) */
    TEST_ASSERT_EQ(graph_add_line_segment(g, 0, 1), ADD_NODE_OK);

    lvIncrementalSolver *s = lv_solver_incremental_create(g);
    TEST_ASSERT_NOT_NULL(s);
    lvSolveResult result = lv_SOLVE_FAILED;
    TEST_ASSERT(lv_solve_incremental(s, &result), "有点图求解成功");
    TEST_ASSERT(s->is_valid, "缓存有效");
    TEST_ASSERT(s->solver_sys != NULL, "内部求解系统已建");

    /* 缓存命中路径：二次调用直接返回 */
    lvSolveResult result2 = lv_SOLVE_FAILED;
    TEST_ASSERT(lv_solve_incremental(s, &result2), "缓存命中求解");
    TEST_ASSERT(result2 == result, "缓存结果一致");

    lv_solver_incremental_destroy(s);
    graph_destroy(g);
}

static void test_invalidate_and_mark_changed(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    add_point(g, 0, 1, 0, 1);
    lvIncrementalSolver *s = lv_solver_incremental_create(g);
    TEST_ASSERT_NOT_NULL(s);

    lvSolveResult result;
    TEST_ASSERT(lv_solve_incremental(s, &result), "首次求解");
    TEST_ASSERT(s->is_valid, "有效");

    /* mark_changed：去重 + 置失效 */
    TEST_ASSERT(lv_incremental_solver_mark_changed(s, 0), "标记变更");
    TEST_ASSERT_EQ(s->changed_count, 1);
    TEST_ASSERT(lv_incremental_solver_mark_changed(s, 0), "重复标记（去重）");
    TEST_ASSERT_EQ(s->changed_count, 1);
    TEST_ASSERT(!s->is_valid, "变更后缓存失效");
    TEST_ASSERT(lv_incremental_solver_mark_changed(s, 1), "再标记");
    TEST_ASSERT_EQ(s->changed_count, 2);

    /* 变更后重解 */
    lvSolveResult result2;
    TEST_ASSERT(lv_solve_incremental(s, &result2), "变更后重解");
    TEST_ASSERT(s->is_valid, "重解后有效");
    TEST_ASSERT_EQ(s->changed_count, 0);

    /* invalidate */
    lv_incremental_solver_invalidate(s);
    TEST_ASSERT(!s->is_valid, "invalidate 后失效");
    lv_incremental_solver_invalidate(NULL); /* NULL 安全 */
    TEST_ASSERT(!lv_incremental_solver_mark_changed(NULL, 1), "NULL 拒绝");

    lv_solver_incremental_destroy(s);
    graph_destroy(g);
}

static void test_solve_parallel(void) {
    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    add_point(g, 0, 1, 0, 1);
    add_point(g, 4, 1, 0, 1);
    add_point(g, 1, 1, 1, 1);
    add_point(g, 5, 1, 1, 1);
    graph_add_line_segment(g, 0, 1);
    graph_add_line_segment(g, 2, 3);

    lvSubgraphTask *tasks = NULL;
    int task_count = 0;
    TEST_ASSERT_EQ(lv_graph_decompose(g, &tasks, &task_count), 0);
    TEST_ASSERT(task_count >= 1, "有子图");

    lvSolveResult result = lv_SOLVE_FAILED;
    TEST_ASSERT(lv_solve_parallel(tasks, task_count, 2, &result), "并行求解成功");
    TEST_ASSERT(!lv_solve_parallel(NULL, 1, 2, &result), "NULL tasks 拒绝");
    TEST_ASSERT(!lv_solve_parallel(tasks, 0, 2, &result), "0 任务拒绝");

    for (int i = 0; i < task_count; i++)
        lv_free((void **) &tasks[i].node_ids);
    lv_free((void **) &tasks);
    graph_destroy(g);
}

TEST_MAIN_BEGIN("Lv-00 Solver Incremental (G5c) Test Suite")
    printf("=== Lv-00 Solver Incremental (G5c) Test Suite ===\n\n");
    lv_init();
    TEST_MAIN_RUN(test_lifecycle);
    TEST_MAIN_RUN(test_solve_empty_graph);
    TEST_MAIN_RUN(test_solve_with_points);
    TEST_MAIN_RUN(test_invalidate_and_mark_changed);
    TEST_MAIN_RUN(test_solve_parallel);
    lv_cleanup();
TEST_MAIN_END()
