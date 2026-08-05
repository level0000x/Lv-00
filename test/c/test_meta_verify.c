/**
 * @file test_meta_verify.c
 * @brief 元验证逻辑单元测试：完备性、可靠性、差分验证
 *
 * 测试 meta_verify_completeness / meta_verify_soundness / meta_verify_differential
 * 三个函数的正确性，覆盖正常路径和边界条件。
 */

#include <stdio.h>
#include <stdlib.h>

#include "lv/conflict_detector.h"
#include "lv/constraint_graph.h"
#include "lv/lv.h"

#include "test_helpers.h"

/* 测试计数器（test_helpers.h 要求定义） */
int g_pass_count = 0;
int g_fail_count = 0;

/* 前向声明：待测函数 */
extern int meta_verify_completeness(const ConstraintGraph *graph);
extern int meta_verify_soundness(const ConstraintGraph *graph);
extern int meta_verify_differential(const ConstraintGraph *graph_a, const ConstraintGraph *graph_b);

/**
 * @brief 创建包含一个已指定坐标的 POINT 节点的约束图
 * @return 约束图指针，失败返回 NULL
 */
static ConstraintGraph *create_fully_specified_graph(void) {
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;

    SymbolicCoord *cx = symbolic_coord_create_rational(1, 1);
    SymbolicCoord *cy = symbolic_coord_create_rational(2, 1);
    if (!cx || !cy) {
        if (cx)
            symbolic_coord_destroy(cx);
        if (cy)
            symbolic_coord_destroy(cy);
        graph_destroy(g);
        return NULL;
    }

    SymbolicCoord *coords[] = {cx, cy};
    AddNodeResult res = graph_add_point(g, coords, 2);
    if (res != ADD_NODE_OK) {
        symbolic_coord_destroy(cx);
        symbolic_coord_destroy(cy);
        graph_destroy(g);
        return NULL;
    }
    return g;
}

/**
 * @brief 创建包含未指定坐标的 POINT 节点的约束图（不完备）
 * @return 约束图指针，失败返回 NULL
 */
static ConstraintGraph *create_unresolved_graph(void) {
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;

    /* 添加点时不传递坐标（coord_count = 0），模拟未解析节点 */
    AddNodeResult res = graph_add_point(g, NULL, 0);
    if (res != ADD_NODE_OK) {
        graph_destroy(g);
        return NULL;
    }
    return g;
}

/**
 * @brief 测试 NULL 输入处理
 * 所有三个函数在收到 NULL 时应返回 -1
 */
static void test_null_input(void) {
    TEST_ASSERT_EQ(meta_verify_completeness(NULL), -1);
    TEST_ASSERT_EQ(meta_verify_soundness(NULL), -1);
    TEST_ASSERT_EQ(meta_verify_differential(NULL, NULL), -1);
    TEST_ASSERT_EQ(meta_verify_differential(NULL, graph_create()), -1);

    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(meta_verify_differential(g, NULL), -1);
    graph_destroy(g);
}

/**
 * @brief 测试完备性检查
 * - 完整指定的图返回 1
 * - 含有未解析点的图返回 0
 */
static void test_completeness(void) {
    /* 完全指定的图 → 完备 */
    ConstraintGraph *full = create_fully_specified_graph();
    TEST_ASSERT_NOT_NULL(full);
    TEST_ASSERT_EQ(meta_verify_completeness(full), 1);
    graph_destroy(full);

    /* 未解析点的图 → 不完备 */
    ConstraintGraph *unresolved = create_unresolved_graph();
    TEST_ASSERT_NOT_NULL(unresolved);
    TEST_ASSERT_EQ(meta_verify_completeness(unresolved), 0);
    graph_destroy(unresolved);

    /* 空图应视为完备（没有未解析的点） */
    ConstraintGraph *empty = graph_create();
    TEST_ASSERT_NOT_NULL(empty);
    TEST_ASSERT_EQ(meta_verify_completeness(empty), 1);
    graph_destroy(empty);
}

/**
 * @brief 测试可靠性检查
 * - 一致的图返回 1
 * - 空图返回 1
 */
static void test_soundness(void) {
    /* 一致的空图 → 可靠 */
    ConstraintGraph *empty = graph_create();
    TEST_ASSERT_NOT_NULL(empty);
    TEST_ASSERT_EQ(meta_verify_soundness(empty), 1);
    graph_destroy(empty);

    /* 一个点，无约束 → 可靠 */
    ConstraintGraph *graph = create_fully_specified_graph();
    TEST_ASSERT_NOT_NULL(graph);
    TEST_ASSERT_EQ(meta_verify_soundness(graph), 1);
    graph_destroy(graph);
}

/**
 * @brief 测试差分验证
 * - 相同图返回 0
 * - 不同图返回 > 0
 * - 与自身比较返回 0
 */
static void test_differential(void) {
    /* 与自身比较 → 无差异 */
    ConstraintGraph *g1 = create_fully_specified_graph();
    TEST_ASSERT_NOT_NULL(g1);
    TEST_ASSERT_EQ(meta_verify_differential(g1, g1), 0);
    graph_destroy(g1);

    /* 不同节点数的图 → 有差异 */
    ConstraintGraph *ga = create_fully_specified_graph();
    ConstraintGraph *gb = create_unresolved_graph();
    TEST_ASSERT_NOT_NULL(ga);
    TEST_ASSERT_NOT_NULL(gb);
    int diff = meta_verify_differential(ga, gb);
    TEST_ASSERT(diff > 0, "Completeness-nodes graphs should differ");
    graph_destroy(ga);
    graph_destroy(gb);

    /* 空图与空图 → 无差异 */
    ConstraintGraph *empty1 = graph_create();
    ConstraintGraph *empty2 = graph_create();
    TEST_ASSERT_NOT_NULL(empty1);
    TEST_ASSERT_NOT_NULL(empty2);
    TEST_ASSERT_EQ(meta_verify_differential(empty1, empty2), 0);
    graph_destroy(empty1);
    graph_destroy(empty2);
}

TEST_MAIN_BEGIN("Meta Verify")

    TEST_MAIN_RUN(test_null_input);
    TEST_MAIN_RUN(test_completeness);
    TEST_MAIN_RUN(test_soundness);
    TEST_MAIN_RUN(test_differential);


TEST_MAIN_END()
