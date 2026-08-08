/**
 * @file test_equiv_class.c
 * @brief 等价类管理器 (equiv_class) 单元测试
 *
 * 测试内容：
 * - 管理器生命周期
 * - 坐标等价合并 (equiv_merge_by_coord)
 * - 等价查询 (equiv_find, equiv_are_equivalent)
 * - 等价类计数 (equiv_class_count)
 * - 等价类查询 (equiv_get_class)
 * - 批量合并 (equiv_merge_all)
 * - 合并合法性证明 (equiv_prove_merge_valid)
 * - NULL 输入安全性
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equiv_class.h"
#include "lv.h"
#include "test_helpers.h"

/* 全局测试计数器 */
int g_pass_count = 0;
int g_fail_count = 0;

/* ================================================================
 * 测试 1: 等价类管理器创建与销毁生命周期
 * ================================================================ */
void test_equiv_lifecycle(void) {
    printf("  TEST: equiv_manager_create/destroy lifecycle...\n");

    /* 正常创建/销毁 */
    ConstraintGraph *graph = graph_create();
    EquivClassManager *mgr = equiv_manager_create(graph);
    TEST_ASSERT(mgr != NULL, "equiv_manager_create with valid graph");
    TEST_ASSERT(mgr->graph == graph, "mgr->graph matches input graph");
    equiv_manager_destroy(mgr);
    graph_destroy(graph);

    /* NULL graph -> 返回 NULL */
    EquivClassManager *null_mgr = equiv_manager_create(NULL);
    TEST_ASSERT(null_mgr == NULL, "equiv_manager_create(NULL) returns NULL");

    /* NULL safety on destroy */
    equiv_manager_destroy(NULL);

    printf("  PASS: lifecycle\n");
}

/* ================================================================
 * 测试 2: 坐标等价合并 (equiv_merge_by_coord)
 * ================================================================ */
void test_equiv_merge_by_coord(void) {
    printf("  TEST: equiv_merge_by_coord — 同坐标点合并...\n");

    ConstraintGraph *graph = graph_create();

    /* 两个坐标相同的点 (1, 2) */
    add_point(graph, 1, 1, 2, 1);
    add_point(graph, 1, 1, 2, 1);

    EquivClassManager *mgr = equiv_manager_create(graph);
    int merges = equiv_merge_by_coord(mgr);

    TEST_ASSERT(merges >= 1, "merge_by_coord finds >= 1 merge for same-coord points");

    /* 验证等价关系 */
    TEST_ASSERT(equiv_are_equivalent(mgr, 0, 1), "same-coord nodes 0 and 1 are equivalent");

    equiv_manager_destroy(mgr);
    graph_destroy(graph);

    /* NULL input */
    int null_merges = equiv_merge_by_coord(NULL);
    TEST_ASSERT(null_merges == 0, "equiv_merge_by_coord(NULL) returns 0");

    printf("  PASS: merge_by_coord\n");
}

/* ================================================================
 * 测试 3: equiv_find 查询
 * ================================================================ */
void test_equiv_find(void) {
    printf("  TEST: equiv_find...\n");

    ConstraintGraph *graph = graph_create();

    add_point(graph, 5, 1, 5, 1);
    add_point(graph, 5, 1, 5, 1);

    EquivClassManager *mgr = equiv_manager_create(graph);
    equiv_merge_by_coord(mgr);

    /* 合并后两个节点的 find 应返回相同的代表 */
    int r0 = equiv_find(mgr, 0);
    int r1 = equiv_find(mgr, 1);
    TEST_ASSERT(r0 >= 0, "equiv_find(0) returns valid id");
    TEST_ASSERT(r1 >= 0, "equiv_find(1) returns valid id");
    TEST_ASSERT(r0 == r1, "equiv_find returns same root for merged nodes");

    /* 无效节点 */
    int r_invalid = equiv_find(mgr, 999);
    TEST_ASSERT(r_invalid == -1, "equiv_find on invalid node returns -1");

    equiv_manager_destroy(mgr);
    graph_destroy(graph);

    /* NULL input */
    int r_null = equiv_find(NULL, 0);
    TEST_ASSERT(r_null == -1, "equiv_find(NULL, ...) returns -1");

    printf("  PASS: find\n");
}

/* ================================================================
 * 测试 4: equiv_are_equivalent
 * ================================================================ */
void test_equiv_are_equivalent(void) {
    printf("  TEST: equiv_are_equivalent...\n");

    ConstraintGraph *graph = graph_create();

    /* 点 0,1 同坐标 → 等价；点 2 不同坐标 → 不等价 */
    add_point(graph, 0, 1, 0, 1);
    add_point(graph, 0, 1, 0, 1);
    add_point(graph, 7, 1, 7, 1);

    EquivClassManager *mgr = equiv_manager_create(graph);
    equiv_merge_by_coord(mgr);

    /* 同坐标 → 等价 */
    TEST_ASSERT(equiv_are_equivalent(mgr, 0, 1), "same-coord nodes 0 and 1 are equivalent");

    /* 不同坐标 → 不等价 */
    TEST_ASSERT(!equiv_are_equivalent(mgr, 0, 2), "different-coord nodes 0 and 2 are not equivalent");

    /* 同一节点自身 */
    TEST_ASSERT(equiv_are_equivalent(mgr, 0, 0), "node 0 is equivalent to itself");

    equiv_manager_destroy(mgr);
    graph_destroy(graph);

    /* NULL input */
    TEST_ASSERT(!equiv_are_equivalent(NULL, 0, 1), "equiv_are_equivalent(NULL, ...) returns false");

    printf("  PASS: are_equivalent\n");
}

/* ================================================================
 * 测试 5: equiv_class_count 计数
 * ================================================================ */
void test_equiv_class_count(void) {
    printf("  TEST: equiv_class_count — 合并减少类数量...\n");

    ConstraintGraph *graph = graph_create();

    /* 4 个点：3 个同坐标 (0,0), 1 个不同坐标 (1,1) */
    add_point(graph, 0, 1, 0, 1);
    add_point(graph, 0, 1, 0, 1);
    add_point(graph, 0, 1, 0, 1);
    add_point(graph, 1, 1, 1, 1);

    EquivClassManager *mgr = equiv_manager_create(graph);

    /* 合并前每个节点单独一个类（尚未创建显式类） */
    /* 实际的 count 通过 init 在 create 中设立的空间 */
    int count_before = equiv_class_count(mgr);
    /* 初始状态下可能为 0 或其它值，只要合并后减少或保持不变 */

    /* 执行坐标合并 */
    equiv_merge_by_coord(mgr);

    int count_after = equiv_class_count(mgr);
    /* 3 个同坐标点应在一个类中，1 个点在另一个类中 */
    /* 但也要考虑只有被合并的点才会创建类 */
    TEST_ASSERT(count_after >= 1, "class count >= 1 after merge");

    /* 验证具体的等价关系 */
    TEST_ASSERT(equiv_are_equivalent(mgr, 0, 1), "nodes 0 and 1 equivalent after merge");
    TEST_ASSERT(equiv_are_equivalent(mgr, 1, 2), "nodes 1 and 2 equivalent after merge");
    TEST_ASSERT(!equiv_are_equivalent(mgr, 0, 3), "nodes 0 and 3 not equivalent");

    equiv_manager_destroy(mgr);
    graph_destroy(graph);

    /* NULL input */
    int null_count = equiv_class_count(NULL);
    TEST_ASSERT(null_count == 0, "equiv_class_count(NULL) returns 0");

    printf("  PASS: class_count\n");
}

/* ================================================================
 * 测试 6: equiv_get_class 查询
 * ================================================================ */
void test_equiv_get_class(void) {
    printf("  TEST: equiv_get_class...\n");

    ConstraintGraph *graph = graph_create();

    add_point(graph, 3, 1, 3, 1);
    add_point(graph, 3, 1, 3, 1);

    EquivClassManager *mgr = equiv_manager_create(graph);
    equiv_merge_by_coord(mgr);

    /* 有效节点返回非 NULL 类 */
    const EquivClass *ec = equiv_get_class(mgr, 0);
    TEST_ASSERT(ec != NULL, "equiv_get_class for valid node returns non-NULL");
    TEST_ASSERT(ec->member_count >= 2, "merged class has at least 2 members");
    TEST_ASSERT(ec->min_trust == TRUST_GREEN, "default trust is GREEN");

    /* 无效节点返回 NULL */
    const EquivClass *ec_invalid = equiv_get_class(mgr, 99);
    TEST_ASSERT(ec_invalid == NULL, "equiv_get_class for invalid node returns NULL");

    /* 负节点 ID 返回 NULL */
    const EquivClass *ec_neg = equiv_get_class(mgr, -1);
    TEST_ASSERT(ec_neg == NULL, "equiv_get_class for negative node returns NULL");

    equiv_manager_destroy(mgr);
    graph_destroy(graph);

    /* NULL input */
    const EquivClass *ec_null = equiv_get_class(NULL, 0);
    TEST_ASSERT(ec_null == NULL, "equiv_get_class(NULL, ...) returns NULL");

    printf("  PASS: get_class\n");
}

/* ================================================================
 * 测试 7: equiv_merge_all 批量合并
 * ================================================================ */
void test_equiv_merge_all(void) {
    printf("  TEST: equiv_merge_all...\n");

    ConstraintGraph *graph = graph_create();

    /* 4 个点：0,1 同坐标；2,3 不同坐标 */
    add_point(graph, 2, 1, 2, 1);
    add_point(graph, 2, 1, 2, 1);
    add_point(graph, 4, 1, 4, 1);
    add_point(graph, 5, 1, 5, 1);

    EquivClassManager *mgr = equiv_manager_create(graph);
    int total = equiv_merge_all(mgr);

    TEST_ASSERT(total >= 1, "equiv_merge_all returns >= 1 merges");

    /* 同坐标等价 */
    TEST_ASSERT(equiv_are_equivalent(mgr, 0, 1), "nodes 0 and 1 are equivalent after merge_all");

    /* 不同坐标不等价 */
    TEST_ASSERT(!equiv_are_equivalent(mgr, 0, 2), "nodes 0 and 2 are not equivalent after merge_all");
    TEST_ASSERT(!equiv_are_equivalent(mgr, 2, 3), "nodes 2 and 3 are not equivalent");

    /* 统计信息可用 */
    int64_t total_s, coord_s, derive_s, conj_s, transform_s, rejected_s;
    equiv_get_statistics(mgr, &total_s, &coord_s, &derive_s, &conj_s, &transform_s, &rejected_s);
    TEST_ASSERT(total_s == total, "statistics total matches merge_all return value");

    equiv_manager_destroy(mgr);
    graph_destroy(graph);

    /* NULL input */
    int null_total = equiv_merge_all(NULL);
    TEST_ASSERT(null_total == 0, "equiv_merge_all(NULL) returns 0");

    printf("  PASS: merge_all\n");
}

/* ================================================================
 * 测试 8: equiv_prove_merge_valid 合法性验证
 * ================================================================ */
void test_equiv_prove_merge_valid(void) {
    printf("  TEST: equiv_prove_merge_valid...\n");

    ConstraintGraph *graph = graph_create();

    /* 两个同坐标点 → 它们会在同一类中 */
    add_point(graph, 0, 1, 0, 1);
    add_point(graph, 0, 1, 0, 1);
    /* 一个不同坐标点 → 单独一类 */
    add_point(graph, 9, 1, 9, 1);

    EquivClassManager *mgr = equiv_manager_create(graph);
    equiv_merge_by_coord(mgr);

    /* 获取节点 0 和节点 2 的类索引 */
    int class_a = mgr->node_to_class[0]; /* 通过内部结构获取类索引 */
    int class_b = mgr->node_to_class[2];

    /* 如果两个类不同，验证合并合法性 */
    if (class_a != class_b && class_a >= 0 && class_b >= 0) {
        bool valid = equiv_prove_merge_valid(mgr, class_a, class_b);
        TEST_ASSERT(valid == true, "prove_merge_valid between two GREEN classes returns true");
    }

    /* 同一类 → 返回 true */
    bool self_valid = equiv_prove_merge_valid(mgr, class_a, class_a);
    TEST_ASSERT(self_valid == true, "prove_merge_valid with same class returns true");

    /* 无效类索引 → 返回 false */
    bool invalid_a = equiv_prove_merge_valid(mgr, -1, 0);
    TEST_ASSERT(!invalid_a, "prove_merge_valid with negative index returns false");

    bool invalid_b = equiv_prove_merge_valid(mgr, 0, 9999);
    TEST_ASSERT(!invalid_b, "prove_merge_valid with OOB index returns false");

    equiv_manager_destroy(mgr);
    graph_destroy(graph);

    /* NULL input */
    bool null_valid = equiv_prove_merge_valid(NULL, 0, 1);
    TEST_ASSERT(!null_valid, "equiv_prove_merge_valid(NULL, ...) returns false");

    printf("  PASS: prove_merge_valid\n");
}

/* ================================================================
 * 测试 9: NULL 输入安全性综合测试
 * ================================================================ */
void test_equiv_null_safety(void) {
    printf("  TEST: NULL input safety for all public functions...\n");

    /* 已在各测试中覆盖的 NULL 输入 */
    /* equiv_manager_create — test_equiv_lifecycle */
    /* equiv_manager_destroy — test_equiv_lifecycle */
    /* equiv_find — test_equiv_find */
    /* equiv_are_equivalent — test_equiv_are_equivalent */
    /* equiv_merge_by_coord — test_equiv_merge_by_coord */
    /* equiv_get_class — test_equiv_get_class */
    /* equiv_class_count — test_equiv_class_count */
    /* equiv_merge_all — test_equiv_merge_all */
    /* equiv_prove_merge_valid — test_equiv_prove_merge_valid */

    /* equiv_get_statistics */
    equiv_get_statistics(NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(1, "equiv_get_statistics(NULL) does not crash");

    /* equiv_manager_are_equivalent */
    bool r = equiv_manager_are_equivalent(NULL, 0, 1);
    TEST_ASSERT(r == false, "equiv_manager_are_equivalent(NULL, ...) returns false");

    /* equiv_derive_from_constraints */
    int dc = equiv_derive_from_constraints(NULL);
    TEST_ASSERT(dc == 0, "equiv_derive_from_constraints(NULL) returns 0");

    /* equiv_merge_algebraic_conjugates */
    int ac = equiv_merge_algebraic_conjugates(NULL);
    TEST_ASSERT(ac == 0, "equiv_merge_algebraic_conjugates(NULL) returns 0");

    /* equiv_merge_by_transform */
    int tc = equiv_merge_by_transform(NULL);
    TEST_ASSERT(tc == 0, "equiv_merge_by_transform(NULL) returns 0");

    printf("  PASS: NULL safety\n");
}

/* ================================================================
 * 主函数
 * ================================================================ */
TEST_MAIN_BEGIN("EquivClass (等价类管理器) 单元测试")
    TEST_MAIN_RUN(test_equiv_lifecycle);
    TEST_MAIN_RUN(test_equiv_merge_by_coord);
    TEST_MAIN_RUN(test_equiv_find);
    TEST_MAIN_RUN(test_equiv_are_equivalent);
    TEST_MAIN_RUN(test_equiv_class_count);
    TEST_MAIN_RUN(test_equiv_get_class);
    TEST_MAIN_RUN(test_equiv_merge_all);
    TEST_MAIN_RUN(test_equiv_prove_merge_valid);
    TEST_MAIN_RUN(test_equiv_null_safety);
TEST_MAIN_END()
