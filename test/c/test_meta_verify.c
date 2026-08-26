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
#include "lv/lv_parse_utils.h"

#include "test_helpers.h"

/* 测试计数器（test_helpers.h 要求定义） */
int g_pass_count = 0;
int g_fail_count = 0;

/* 前向声明：待测函数（layer4_reasoning/proof/meta_verify.c，lv_graph_ 前缀） */
extern int lv_graph_meta_verify_completeness(const ConstraintGraph *graph);
extern int lv_graph_meta_verify_soundness(const ConstraintGraph *graph);
extern int lv_graph_meta_verify_differential(const ConstraintGraph *graph_a, const ConstraintGraph *graph_b);

/**
 * @brief 创建包含一个已指定坐标的 POINT 节点的约束图
 * @return 约束图指针，失败返回 NULL
 */
static ConstraintGraph *create_fully_specified_graph(void) {
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;

    SymbolicCoord *cx = mk_rat(1, 1);
    SymbolicCoord *cy = mk_rat(2, 1);
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
    /* graph_add_point 深拷贝坐标，调用方保留所有权：成功/失败均释放 */
    symbolic_coord_destroy(cx);
    symbolic_coord_destroy(cy);
    if (res != ADD_NODE_OK) {
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
    TEST_ASSERT_EQ(lv_graph_meta_verify_completeness(NULL), -1);
    TEST_ASSERT_EQ(lv_graph_meta_verify_soundness(NULL), -1);
    TEST_ASSERT_EQ(lv_graph_meta_verify_differential(NULL, NULL), -1);
    TEST_ASSERT_EQ(lv_graph_meta_verify_differential(NULL, graph_create()), -1);

    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQ(lv_graph_meta_verify_differential(g, NULL), -1);
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
    TEST_ASSERT_EQ(lv_graph_meta_verify_completeness(full), 1);
    graph_destroy(full);

    /* 未解析点的图 → 不完备 */
    ConstraintGraph *unresolved = create_unresolved_graph();
    TEST_ASSERT_NOT_NULL(unresolved);
    TEST_ASSERT_EQ(lv_graph_meta_verify_completeness(unresolved), 0);
    graph_destroy(unresolved);

    /* 空图应视为完备（没有未解析的点） */
    ConstraintGraph *empty = graph_create();
    TEST_ASSERT_NOT_NULL(empty);
    TEST_ASSERT_EQ(lv_graph_meta_verify_completeness(empty), 1);
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
    TEST_ASSERT_EQ(lv_graph_meta_verify_soundness(empty), 1);
    graph_destroy(empty);

    /* 一个点，无约束 → 可靠 */
    ConstraintGraph *graph = create_fully_specified_graph();
    TEST_ASSERT_NOT_NULL(graph);
    TEST_ASSERT_EQ(lv_graph_meta_verify_soundness(graph), 1);
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
    TEST_ASSERT_EQ(lv_graph_meta_verify_differential(g1, g1), 0);
    graph_destroy(g1);

    /* 不同节点数的图 → 有差异 */
    ConstraintGraph *ga = create_fully_specified_graph();
    ConstraintGraph *gb = create_unresolved_graph();
    TEST_ASSERT_NOT_NULL(ga);
    TEST_ASSERT_NOT_NULL(gb);
    int diff = lv_graph_meta_verify_differential(ga, gb);
    TEST_ASSERT(diff > 0, "Completeness-nodes graphs should differ");
    graph_destroy(ga);
    graph_destroy(gb);

    /* 空图与空图 → 无差异 */
    ConstraintGraph *empty1 = graph_create();
    ConstraintGraph *empty2 = graph_create();
    TEST_ASSERT_NOT_NULL(empty1);
    TEST_ASSERT_NOT_NULL(empty2);
    TEST_ASSERT_EQ(lv_graph_meta_verify_differential(empty1, empty2), 0);
    graph_destroy(empty1);
    graph_destroy(empty2);
}

/**
 * @brief 测试判据 I 设施 lv_parse_int_before（关键词后置数字提取）
 *
 * 覆盖：正常提取 / 无前驱数字 / pos 处为数字 / NULL 参数 / 起点边界 /
 * 溢出失败不写 out / 非数字隔断时仅取紧邻数字串。
 */
static void test_parse_int_before(void) {
    /* 数字串与关键词紧邻时回退提取（设施契约；真实消息含空格时不触发） */
    const char *text = "完成12尝试，输出123字节，识别7个几何对象";
    int value = 0;
    TEST_ASSERT_EQ(lv_parse_int_before(text, strstr(text, "尝试"), &value), 0);
    TEST_ASSERT_EQ(value, 12);
    value = 0;
    TEST_ASSERT_EQ(lv_parse_int_before(text, strstr(text, "字节"), &value), 0);
    TEST_ASSERT_EQ(value, 123);
    value = 0;
    TEST_ASSERT_EQ(lv_parse_int_before(text, strstr(text, "个几何对象"), &value), 0);
    TEST_ASSERT_EQ(value, 7);

    /* 起点边界：数字串起点位于 base 处仍可解析 */
    const char *at_base = "42标记";
    value = 0;
    TEST_ASSERT_EQ(lv_parse_int_before(at_base, strstr(at_base, "标记"), &value), 0);
    TEST_ASSERT_EQ(value, 42);

    /* 无前驱数字 → -1，不写 out */
    const char *no_digit = "abc标记";
    value = -7;
    TEST_ASSERT_EQ(lv_parse_int_before(no_digit, strstr(no_digit, "标记"), &value), -1);
    TEST_ASSERT_EQ(value, -7);

    /* pos 指向数字本身 → -1 */
    TEST_ASSERT_EQ(lv_parse_int_before("123", "123", &value), -1);

    /* 非数字隔断：仅取紧邻标记的数字串 */
    const char *gapped = "7次12标记";
    value = 0;
    TEST_ASSERT_EQ(lv_parse_int_before(gapped, strstr(gapped, "标记"), &value), 0);
    TEST_ASSERT_EQ(value, 12);

    /* 溢出失败：不写 out */
    const char *overflow = "999999999999999999999标记";
    value = 5;
    TEST_ASSERT_EQ(lv_parse_int_before(overflow, strstr(overflow, "标记"), &value), -1);
    TEST_ASSERT_EQ(value, 5);

    /* NULL / 越界参数 → -1 */
    TEST_ASSERT_EQ(lv_parse_int_before(NULL, "x", &value), -1);
    TEST_ASSERT_EQ(lv_parse_int_before("x", NULL, &value), -1);
    TEST_ASSERT_EQ(lv_parse_int_before("x", "x", NULL), -1);
    TEST_ASSERT_EQ(lv_parse_int_before("123", "123", NULL), -1);
}

TEST_MAIN_BEGIN("Meta Verify")

    TEST_MAIN_RUN(test_null_input);
    TEST_MAIN_RUN(test_completeness);
    TEST_MAIN_RUN(test_soundness);
    TEST_MAIN_RUN(test_differential);
    TEST_MAIN_RUN(test_parse_int_before);


TEST_MAIN_END()
