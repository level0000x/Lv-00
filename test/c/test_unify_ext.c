/**
 * @file test_unify_ext.c
 * @brief 合一扩展契约测试（批次 C-㊶：unify.h 15 个零覆盖 API）
 *
 * 覆盖 15 个 ctest 零覆盖 API：
 *   - 等价存储族：unify_equivalence_storage_init / lv_unify_equivalence_
 *     storage_cleanup / unify_clear_equivalences / unify_equivalence_count /
 *     unify_declare_proposition_equivalence / unify_find_equivalent_proposition
 *   - 合一族：unify_construction_with_proposition_coord / _hash_filtered /
 *     _detailed（含 unify_failure_info_destroy）
 *   - 精细化匹配族：unify_coords_equal / unify_match_coords /
 *     unify_match_constraints
 *   - 实例化：unify_instantiate_proposition
 *   - 流式上下文：unify_set_stream_context
 *
 * 批次暴露并修复的缺陷：
 *   1. M5：unify_coords_equal / unify_equivalence_storage_init 头文件声明
 *      但全库无实现（零引用故链接未暴露）——按头注释契约补齐实现。
 *   2. M2：基础/coord/hash_filtered 三个合一 API 对 NULL 入参崩溃
 *      （graph_normalize(NULL) 解引用）——与 detailed 版本对齐返回
 *      UNIFY_STATUS_FAILED。
 *
 * 契约要点：
 *   - 等价存储为 TLS lvTlsVector：declare 双向、重复声明更新变换规则。
 *   - unify_coords_equal 不关心节点类型，仅比较 symbolic_coords 内容。
 *   - unify_match_coords 返回 0 相等 / 非 0 不等 / 单 NULL -1 / 双 NULL 0。
 *   - unify_instantiate_proposition 对 concrete_type 引用语义（不拷贝）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* 构造同构图：2 点 + 线段 + INCIDENCE 约束（两次调用产生同构副本） */
static ConstraintGraph *make_unify_graph(void) {
    ConstraintGraph *g = graph_create();
    if (!g)
        return NULL;
    int p0 = add_point(g, 0, 1, 0, 1);
    int p1 = add_point(g, 1, 1, 0, 1);
    if (graph_add_line_segment(g, p0, p1) != ADD_NODE_OK) {
        graph_destroy(g);
        return NULL;
    }
    int seg = graph_get_last_added_node_id(g);
    if (graph_add_incidence(g, p0, seg) < 0) {
        graph_destroy(g);
        return NULL;
    }
    return g;
}

/* ============== 测试：等价声明存储 ============== */

static void test_equivalence_storage_api(void) {
    /* init 可重复调用（重置存储状态） */
    unify_equivalence_storage_init();
    unify_equivalence_storage_init();
    TEST_ASSERT_EQ(unify_equivalence_count(), 0);

    /* declare：正常 / 重复更新 */
    TEST_ASSERT(unify_declare_proposition_equivalence(1, 2, NULL), "声明 1≡2");
    TEST_ASSERT_EQ(unify_equivalence_count(), 1);
    TEST_ASSERT(unify_declare_proposition_equivalence(2, 3, NULL), "声明 2≡3");
    TEST_ASSERT_EQ(unify_equivalence_count(), 2);
    /* 重复声明（含反向）→ 更新而非新增 */
    TEST_ASSERT(unify_declare_proposition_equivalence(2, 1, NULL), "反向重复声明更新");
    TEST_ASSERT_EQ(unify_equivalence_count(), 2);

    /* find：双向查找 / NULL 契约 */
    int equiv[8];
    memset(equiv, -1, sizeof(equiv));
    TEST_ASSERT_EQ(unify_find_equivalent_proposition(1, equiv, 8), 1);
    TEST_ASSERT_EQ(equiv[0], 2);
    memset(equiv, -1, sizeof(equiv));
    TEST_ASSERT_EQ(unify_find_equivalent_proposition(2, equiv, 8), 2); /* 1 和 3 */
    TEST_ASSERT_EQ(unify_find_equivalent_proposition(3, equiv, 8), 1);
    TEST_ASSERT_EQ(equiv[0], 2);
    TEST_ASSERT_EQ(unify_find_equivalent_proposition(1, NULL, 8), 0);
    TEST_ASSERT_EQ(unify_find_equivalent_proposition(1, equiv, 0), 0);
    TEST_ASSERT_EQ(unify_find_equivalent_proposition(99, equiv, 8), 0);

    /* transformation 图所有权：declare 后 clear 由存储释放 */
    ConstraintGraph *tx = graph_create();
    TEST_ASSERT_NOT_NULL(tx);
    TEST_ASSERT(unify_declare_proposition_equivalence(10, 11, tx), "带变换规则声明");

    /* clear：清空全部（含销毁 transformation 图） */
    unify_clear_equivalences();
    TEST_ASSERT_EQ(unify_equivalence_count(), 0);
    TEST_ASSERT_EQ(unify_find_equivalent_proposition(1, equiv, 8), 0);

    /* cleanup：释放存储后仍可重新声明 */
    lv_unify_equivalence_storage_cleanup();
    TEST_ASSERT(unify_declare_proposition_equivalence(5, 6, NULL), "cleanup 后重新声明");
    TEST_ASSERT_EQ(unify_equivalence_count(), 1);
    unify_clear_equivalences();

    printf("  test_equivalence_storage_api: PASSED\n");
}

/* ============== 测试：unify_coords_equal ============== */

static void test_coords_equal_api(void) {
    /* NULL 契约 */
    TEST_ASSERT_EQ(unify_coords_equal(NULL, NULL), 0);
    TEST_ASSERT_EQ(unify_coords_equal(NULL, (GeomNode *)0x1), 0);

    ConstraintGraph *g = graph_create();
    TEST_ASSERT_NOT_NULL(g);
    int a = add_point(g, 0, 1, 0, 1);
    int b = add_point(g, 0, 1, 0, 1); /* 同坐标 (0,0) → 等 */
    int c = add_point(g, 1, 1, 0, 1); /* 不同坐标 (1,0) → 不等 */
    GeomNode *na = graph_get_node(g, a);
    GeomNode *nb = graph_get_node(g, b);
    GeomNode *nc = graph_get_node(g, c);
    TEST_ASSERT_NOT_NULL(na);
    TEST_ASSERT_NOT_NULL(nb);
    TEST_ASSERT_NOT_NULL(nc);

    /* 相同坐标 → 1；不同坐标 → 0 */
    TEST_ASSERT_EQ(unify_coords_equal(na, nb), 1);
    TEST_ASSERT_EQ(unify_coords_equal(na, nc), 0);
    TEST_ASSERT_EQ(unify_coords_equal(na, na), 1);

    /* coord_count 不同 → 0（线段节点坐标更多） */
    TEST_ASSERT_EQ(graph_add_line_segment(g, a, b), ADD_NODE_OK);
    int seg = graph_get_last_added_node_id(g);
    GeomNode *ns = graph_get_node(g, seg);
    TEST_ASSERT_NOT_NULL(ns);
    TEST_ASSERT_EQ(unify_coords_equal(na, ns), 0);

    /* coord_count > 0 但 symbolic_coords NULL → 0 */
    int saved_count = nc->coord_count;
    nc->coord_count = 2;
    SymbolicCoord **saved_coords = nc->symbolic_coords;
    nc->symbolic_coords = NULL;
    TEST_ASSERT_EQ(unify_coords_equal(nc, nb), 0);
    nc->coord_count = saved_count;
    nc->symbolic_coords = saved_coords;

    graph_destroy(g);
    printf("  test_coords_equal_api: PASSED\n");
}

/* ============== 测试：unify_match_coords ============== */

static void test_match_coords_api(void) {
    /* NULL 契约：双 NULL → 0（相等）；单 NULL → -1 */
    TEST_ASSERT_EQ(unify_match_coords(NULL, NULL), 0);
    SymbolicCoord *s = symbolic_coord_create_rational(1, 2);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQ(unify_match_coords(s, NULL), -1);
    TEST_ASSERT_EQ(unify_match_coords(NULL, s), -1);

    /* 相同 → 0；不同 → 非 0 */
    SymbolicCoord *s2 = symbolic_coord_create_rational(1, 2);
    SymbolicCoord *s3 = symbolic_coord_create_rational(3, 4);
    TEST_ASSERT_NOT_NULL(s2);
    TEST_ASSERT_NOT_NULL(s3);
    TEST_ASSERT_EQ(unify_match_coords(s, s2), 0);
    TEST_ASSERT(unify_match_coords(s, s3) != 0, "不同坐标不相等");

    symbolic_coord_destroy(s);
    symbolic_coord_destroy(s2);
    symbolic_coord_destroy(s3);
    printf("  test_match_coords_api: PASSED\n");
}

/* ============== 测试：unify_match_constraints ============== */

static void test_match_constraints_api(void) {
    /* NULL 契约 → -1 */
    TEST_ASSERT_EQ(unify_match_constraints(NULL, NULL, NULL), -1);

    ConstraintGraph *a = make_unify_graph();
    ConstraintGraph *b = make_unify_graph();
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    /* 同构 → 1 对约束匹配 */
    int bindings[8];
    TEST_ASSERT_EQ(unify_match_constraints(a, b, bindings), 1);
    TEST_ASSERT(bindings[0] >= 0 && bindings[1] >= 0, "绑定输出约束 id 对");
    /* bindings NULL 仅计数 */
    TEST_ASSERT_EQ(unify_match_constraints(a, b, NULL), 1);

    /* 命题无约束 → 0 */
    ConstraintGraph *empty = graph_create();
    TEST_ASSERT_NOT_NULL(empty);
    TEST_ASSERT_EQ(unify_match_constraints(a, empty, NULL), 0);

    /* 构造无对应约束 → -1 */
    TEST_ASSERT_EQ(unify_match_constraints(empty, a, NULL), -1);

    graph_destroy(empty);
    graph_destroy(a);
    graph_destroy(b);
    printf("  test_match_constraints_api: PASSED\n");
}

/* ============== 测试：合一三族（coord / hash_filtered） ============== */

static void test_unify_status_api(void) {
    /* NULL 契约（C-㊶ 修复后）→ FAILED */
    TEST_ASSERT_EQ(unify_construction_with_proposition_coord(NULL, NULL), UNIFY_STATUS_FAILED);
    TEST_ASSERT_EQ(unify_construction_with_proposition_hash_filtered(NULL, NULL), UNIFY_STATUS_FAILED);

    ConstraintGraph *a = make_unify_graph();
    ConstraintGraph *b = make_unify_graph();
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);

    /* 同构 → OK（coord 与 hash_filtered） */
    TEST_ASSERT_EQ(unify_construction_with_proposition_coord(a, b), UNIFY_STATUS_OK);
    TEST_ASSERT_EQ(unify_construction_with_proposition_hash_filtered(a, b), UNIFY_STATUS_OK);

    /* 异构（命题含约束但构造无约束）→ 失败（coord 返回 COORD_MISMATCH，
     * hash_filtered 与基础版一致返回 CONSTRAINT_MISMATCH） */
    ConstraintGraph *plain = graph_create();
    TEST_ASSERT_NOT_NULL(plain);
    add_point(plain, 0, 1, 0, 1);
    add_point(plain, 1, 1, 0, 1);
    TEST_ASSERT_EQ(unify_construction_with_proposition_coord(plain, b), UNIFY_STATUS_COORD_MISMATCH);
    TEST_ASSERT_EQ(unify_construction_with_proposition_hash_filtered(plain, b), UNIFY_STATUS_CONSTRAINT_MISMATCH);

    graph_destroy(plain);
    graph_destroy(a);
    graph_destroy(b);
    printf("  test_unify_status_api: PASSED\n");
}

/* ============== 测试：详细合一 + 失败信息 ============== */

static void test_detailed_unify_api(void) {
    /* destroy：NULL 安全 */
    unify_failure_info_destroy(NULL);

    /* NULL 入参 → FAILED + failure 填充 */
    UnifyFailureInfo failure;
    memset(&failure, 0, sizeof(failure));
    TEST_ASSERT_EQ(unify_construction_with_proposition_detailed(NULL, NULL, &failure), UNIFY_STATUS_FAILED);
    TEST_ASSERT_EQ(failure.status, UNIFY_STATUS_FAILED);
    TEST_ASSERT_NOT_NULL(failure.description);
    unify_failure_info_destroy(&failure);

    /* out_failure NULL 也可用 */
    TEST_ASSERT_EQ(unify_construction_with_proposition_detailed(NULL, NULL, NULL), UNIFY_STATUS_FAILED);

    /* 同构 → OK，failure 不填充（status 保持 OK） */
    ConstraintGraph *a = make_unify_graph();
    ConstraintGraph *b = make_unify_graph();
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    memset(&failure, 0, sizeof(failure));
    TEST_ASSERT_EQ(unify_construction_with_proposition_detailed(a, b, &failure), UNIFY_STATUS_OK);
    TEST_ASSERT_EQ(failure.status, UNIFY_STATUS_OK);

    /* 异构 → 失败状态 + destroy 安全 */
    ConstraintGraph *plain = graph_create();
    TEST_ASSERT_NOT_NULL(plain);
    add_point(plain, 0, 1, 0, 1);
    add_point(plain, 1, 1, 0, 1);
    memset(&failure, 0, sizeof(failure));
    UnifyStatus st = unify_construction_with_proposition_detailed(plain, b, &failure);
    TEST_ASSERT(st != UNIFY_STATUS_OK, "异构合一失败");
    unify_failure_info_destroy(&failure);

    graph_destroy(plain);
    graph_destroy(a);
    graph_destroy(b);
    printf("  test_detailed_unify_api: PASSED\n");
}

/* ============== 测试：命题实例化 ============== */

static void test_instantiate_api(void) {
    /* NULL 契约 → false */
    ConstraintGraph *prop = graph_create();
    TEST_ASSERT_NOT_NULL(prop);
    TEST_ASSERT(!unify_instantiate_proposition(NULL, 0, NULL, NULL), "NULL proposition");
    TEST_ASSERT(!unify_instantiate_proposition(prop, 0, NULL, NULL), "NULL concrete_type");
    TEST_ASSERT(!unify_instantiate_proposition(prop, 0, (TypeRegion *)0x1, NULL), "NULL out");

    /* 正路径：端口节点实例化为具体类型（引用语义） */
    TypeSystem *ts = type_system_create();
    TEST_ASSERT_NOT_NULL(ts);
    TypeRegion *point_type = type_create_point(ts);
    TEST_ASSERT_NOT_NULL(point_type);
    TEST_ASSERT_EQ(graph_add_port(prop, PORT_INPUT, 0, 0), ADD_NODE_OK);
    int port_id = graph_get_last_added_node_id(prop);

    ConstraintGraph *inst = NULL;
    TEST_ASSERT(unify_instantiate_proposition(prop, port_id, point_type, &inst), "实例化成功");
    TEST_ASSERT_NOT_NULL(inst);
    GeomNode *inst_port = graph_get_node(inst, port_id);
    TEST_ASSERT_NOT_NULL(inst_port);
    TEST_ASSERT_EQ(inst_port->type, GEOM_PORT);
    TEST_ASSERT_NOT_NULL(inst_port->data.port);
    TEST_ASSERT(inst_port->data.port->type_region == point_type, "端口类型区域替换为具体类型");

    /* 不存在的节点 id → 深拷贝成功但无替换 */
    ConstraintGraph *inst2 = NULL;
    TEST_ASSERT(unify_instantiate_proposition(prop, 999, point_type, &inst2), "未知节点 id 实例化");
    TEST_ASSERT_NOT_NULL(inst2);

    graph_destroy(inst2);
    graph_destroy(inst);
    graph_destroy(prop);
    type_system_destroy(ts);
    printf("  test_instantiate_api: PASSED\n");
}

/* ============== 测试：流式上下文 ============== */

static void test_stream_ctx_api(void) {
    unify_set_stream_context(NULL); /* NULL 安全 */
    unify_set_stream_context(NULL);
    printf("  test_stream_ctx_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Unify Ext Test Suite")
    printf("=== Lv-00 Unify Ext Test Suite (batch C-㊶) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_equivalence_storage_api);
    TEST_MAIN_RUN(test_coords_equal_api);
    TEST_MAIN_RUN(test_match_coords_api);
    TEST_MAIN_RUN(test_match_constraints_api);
    TEST_MAIN_RUN(test_unify_status_api);
    TEST_MAIN_RUN(test_detailed_unify_api);
    TEST_MAIN_RUN(test_instantiate_api);
    TEST_MAIN_RUN(test_stream_ctx_api);

    lv_cleanup();
TEST_MAIN_END()
