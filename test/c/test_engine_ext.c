/**
 * @file test_engine_ext.c
 * @brief 引擎扩展契约测试（批次 C-㊷：engine.h 13 个零覆盖 API）
 *
 * 覆盖 13 个 ctest 零覆盖 API：
 *   - 资源族：engine_add_rewrite_rule / engine_load_module /
 *     engine_load_axiom_package / engine_set_rewrite_step_limit /
 *     engine_get_rewrite_step_limit
 *   - 冻结点族：engine_restore_frozen_point / engine_destroy_frozen_point
 *     （engine_create_frozen_point 已覆盖，本批测其消费路径）
 *   - 电路跳闸族：engine_handle_circuit_trip /
 *     engine_handle_circuit_trip_with_action
 *   - 流程族：engine_rewrite_and_solve / engine_unify / engine_pack_function
 *   - 流式族：engine_emit_stream_event
 *
 * 契约要点（与实现核对）：
 *   - engine_add_rewrite_rule 转移规则所有权；engine_destroy 统一释放。
 *   - engine_restore_frozen_point 成功后快照被消耗（引擎重新打点），
 *     调用者不得再 destroy 该快照。
 *   - engine_handle_circuit_trip_with_action 无冻结点回滚 → ROLLBACK
 *     （带告警）；NULL engine → ERROR。
 *   - engine_rewrite_and_solve 空引擎/空图返回 >= 0 步数；NULL → 负值。
 *   - engine_unify 委托 unify_construction_with_proposition。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_unified.h"

int g_pass_count = 0;
int g_fail_count = 0;

/* ============== 测试：重写规则注册 ============== */

static void test_rewrite_rule_api(void) {
    /* NULL 契约 */
    TEST_ASSERT(!engine_add_rewrite_rule(NULL, NULL), "NULL engine+rule");
    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT(!engine_add_rewrite_rule(engine, NULL), "NULL rule");
    TEST_ASSERT(!engine_add_rewrite_rule(NULL, (const RewriteRule *)0x1), "NULL engine");

    /* 正路径：注册规则（所有权转移，engine_destroy 释放） */
    RewriteRule *rule = rewrite_rule_create("simplify", NULL, NULL, 1);
    TEST_ASSERT_NOT_NULL(rule);
    TEST_ASSERT(engine_add_rewrite_rule(engine, rule), "注册规则");
    TEST_ASSERT_EQ(engine->rewrite_rule_count, 1);
    TEST_ASSERT(engine->rewrite_rules[0] == rule, "规则指针入列");

    engine_destroy(engine);
    printf("  test_rewrite_rule_api: PASSED\n");
}

/* ============== 测试：重写步数上限 ============== */

static void test_rewrite_step_limit_api(void) {
    /* NULL 契约 */
    TEST_ASSERT_EQ(engine_get_rewrite_step_limit(NULL), lv_DEFAULT_REWRITE_STEP_LIMIT);
    engine_set_rewrite_step_limit(NULL, 5); /* 不崩溃即通过 */

    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);

    /* 默认值 */
    TEST_ASSERT_EQ(engine_get_rewrite_step_limit(engine), lv_DEFAULT_REWRITE_STEP_LIMIT);

    /* 设置/获取往返 */
    engine_set_rewrite_step_limit(engine, 42);
    TEST_ASSERT_EQ(engine_get_rewrite_step_limit(engine), 42);

    /* 非法值（<=0）→ 回默认 */
    engine_set_rewrite_step_limit(engine, 0);
    TEST_ASSERT_EQ(engine_get_rewrite_step_limit(engine), lv_DEFAULT_REWRITE_STEP_LIMIT);

    engine_destroy(engine);
    printf("  test_rewrite_step_limit_api: PASSED\n");
}

/* ============== 测试：冻结点快照 ============== */

static void test_frozen_point_api(void) {
    /* NULL 契约 */
    TEST_ASSERT_NULL(engine_create_frozen_point(NULL));
    engine_destroy_frozen_point(NULL); /* 不崩溃即通过 */
    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT(!engine_restore_frozen_point(NULL, (void *)0x1), "NULL engine");
    TEST_ASSERT(!engine_restore_frozen_point(engine, NULL), "NULL frozen_point");

    /* 正路径：打点 → 改图 → 恢复 → 图回滚 */
    void *fp = engine_create_frozen_point(engine);
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_EQ(graph_get_node_count(engine_get_main_graph(engine)), 0);

    /* 打点后加节点（模拟有风险操作） */
    add_point(engine_get_main_graph(engine), 0, 1, 0, 1);
    add_point(engine_get_main_graph(engine), 1, 1, 0, 1);
    TEST_ASSERT_EQ(graph_get_node_count(engine_get_main_graph(engine)), 2);

    /* 恢复：快照被消耗，图回滚到 0 节点 */
    TEST_ASSERT(engine_restore_frozen_point(engine, fp), "恢复冻结点");
    TEST_ASSERT_EQ(graph_get_node_count(engine_get_main_graph(engine)), 0);
    /* 恢复后引擎自动重新打点（engine->frozen_point 非 NULL），engine_destroy 统一释放 */

    engine_destroy(engine);
    printf("  test_frozen_point_api: PASSED\n");
}

/* ============== 测试：电路跳闸 ============== */

static void test_circuit_trip_api(void) {
    /* handle：NULL → IGNORE */
    TEST_ASSERT_EQ(engine_handle_circuit_trip(NULL), ENGINE_CIRCUIT_IGNORE);
    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);
    /* 无冻结点、无溢出 → IGNORE */
    TEST_ASSERT_EQ(engine_handle_circuit_trip(engine), ENGINE_CIRCUIT_IGNORE);

    /* with_action：NULL → ERROR；非法动作 → ERROR */
    TEST_ASSERT_EQ(engine_handle_circuit_trip_with_action(NULL, ENGINE_CIRCUIT_ACTION_IGNORE), ENGINE_CIRCUIT_ERROR);
    TEST_ASSERT_EQ(engine_handle_circuit_trip_with_action(engine, (EngineCircuitAction)99), ENGINE_CIRCUIT_ERROR);

    /* IGNORE → IGNORE；DOWNGRADE → DOWNGRADE；ROLLBACK（无冻结点）→ ROLLBACK */
    TEST_ASSERT_EQ(engine_handle_circuit_trip_with_action(engine, ENGINE_CIRCUIT_ACTION_IGNORE),
                   ENGINE_CIRCUIT_IGNORE);
    TEST_ASSERT_EQ(engine_handle_circuit_trip_with_action(engine, ENGINE_CIRCUIT_ACTION_DOWNGRADE),
                   ENGINE_CIRCUIT_DOWNGRADE);
    TEST_ASSERT_EQ(engine_handle_circuit_trip_with_action(engine, ENGINE_CIRCUIT_ACTION_ROLLBACK),
                   ENGINE_CIRCUIT_ROLLBACK);

    engine_destroy(engine);
    printf("  test_circuit_trip_api: PASSED\n");
}

/* ============== 测试：流式事件 ============== */

static void test_stream_event_api(void) {
    engine_emit_stream_event(NULL, STREAM_EVENT_INFO, "ignored", 0, -1, -1); /* 不崩溃即通过 */
    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);
    /* engine 有 stream_ctx → 发射不崩溃 */
    engine_emit_stream_event(engine, STREAM_EVENT_INFO, "test event", 1, 2, 3);
    engine_emit_stream_event(engine, STREAM_EVENT_ENGINE_START, "start", 0, -1, -1);
    engine_destroy(engine);
    printf("  test_stream_event_api: PASSED\n");
}

/* ============== 测试：模块/公理包加载 ============== */

static void test_load_module_axiom_api(void) {
    /* NULL 契约 */
    TEST_ASSERT_EQ(engine_load_module(NULL, NULL), MODULE_LOAD_ERROR_INVALID_PATH);
    TEST_ASSERT_EQ(engine_load_axiom_package(NULL, NULL), AXIOM_LOAD_NULL_POINTER);
    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQ(engine_load_module(engine, NULL), MODULE_LOAD_ERROR_INVALID_PATH);
    TEST_ASSERT_EQ(engine_load_axiom_package(engine, NULL), AXIOM_LOAD_NULL_POINTER);

    /* 不存在的文件 → 非 OK（文件缺失路径） */
    ModuleLoadStatus ms = engine_load_module(engine, "no_such_file.lvmod");
    TEST_ASSERT(ms != MODULE_LOAD_OK, "模块文件缺失加载失败");
    AxiomLoadStatus as = engine_load_axiom_package(engine, "no_such_file.lvax");
    TEST_ASSERT(as != AXIOM_LOAD_OK, "公理包文件缺失加载失败");

    engine_destroy(engine);
    printf("  test_load_module_axiom_api: PASSED\n");
}

/* ============== 测试：引擎合一 ============== */

static void test_engine_unify_api(void) {
    /* NULL 契约 → FAILED */
    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);
    TEST_ASSERT_EQ(engine_unify(NULL, NULL, NULL), UNIFY_STATUS_FAILED);
    TEST_ASSERT_EQ(engine_unify(engine, NULL, NULL), UNIFY_STATUS_FAILED);

    /* 同构 → OK；异构（构造缺约束，命题含约束）→ 非 OK */
    ConstraintGraph *a = graph_create();
    ConstraintGraph *b = graph_create();
    ConstraintGraph *plain = graph_create();
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(plain);
    for (int i = 0; i < 3; i++) {
        add_point(a, 0, 1, i, 1);
        add_point(b, 0, 1, i, 1);
        add_point(plain, 0, 1, i, 1);
    }
    TEST_ASSERT(graph_add_line_segment(a, 0, 1) == ADD_NODE_OK, "a 线段");
    TEST_ASSERT(graph_add_line_segment(b, 0, 1) == ADD_NODE_OK, "b 线段");
    TEST_ASSERT(graph_add_line_segment(plain, 0, 1) == ADD_NODE_OK, "plain 线段");
    int sa = graph_get_last_added_node_id(a);
    int sb = graph_get_last_added_node_id(b);
    TEST_ASSERT(graph_add_incidence(a, 0, sa) >= 0, "a 关联约束");
    TEST_ASSERT(graph_add_incidence(b, 0, sb) >= 0, "b 关联约束");
    /* plain 无 incidence 约束 */

    TEST_ASSERT_EQ(engine_unify(engine, a, b), UNIFY_STATUS_OK);
    /* construction=plain（无约束），proposition=a（含约束）→ 失败 */
    TEST_ASSERT(engine_unify(engine, plain, a) != UNIFY_STATUS_OK, "异构合一失败");

    graph_destroy(plain);
    graph_destroy(a);
    graph_destroy(b);
    engine_destroy(engine);
    printf("  test_engine_unify_api: PASSED\n");
}

/* ============== 测试：函数块打包 ============== */

static void test_pack_function_api(void) {
    /* NULL 契约 */
    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);
    int out_id = -1;
    TEST_ASSERT(!engine_pack_function(NULL, NULL, 0, NULL, 0, NULL, 0, &out_id), "NULL engine");
    TEST_ASSERT(!engine_pack_function(engine, NULL, 1, NULL, 0, NULL, 0, &out_id), "节点数组 NULL");
    int fake[1] = {99};
    TEST_ASSERT(!engine_pack_function(engine, fake, 1, NULL, 0, NULL, 0, &out_id), "节点不存在");

    /* 端口类型不对 → false */
    ConstraintGraph *g = engine_get_main_graph(engine);
    int p0 = add_point(g, 0, 1, 0, 1);
    int p1 = add_point(g, 1, 1, 0, 1);
    int internal[2] = {p0, p1};
    int not_port[1] = {p0}; /* 点不是端口 */
    TEST_ASSERT(!engine_pack_function(engine, internal, 2, not_port, 1, NULL, 0, &out_id), "输入非端口");

    /* 正路径：2 内部点 + input/output 端口 → true + id */
    TEST_ASSERT_EQ(graph_add_port(g, PORT_INPUT, 0, 0), ADD_NODE_OK);
    int pin = graph_get_last_added_node_id(g);
    TEST_ASSERT_EQ(graph_add_port(g, PORT_OUTPUT, 0, 0), ADD_NODE_OK);
    int pout = graph_get_last_added_node_id(g);
    int in_ports[1] = {pin};
    int out_ports[1] = {pout};
    out_id = -1;
    TEST_ASSERT(engine_pack_function(engine, internal, 2, in_ports, 1, out_ports, 1, &out_id), "打包函数块");
    TEST_ASSERT(out_id >= 0, "函数块 id 输出");
    GeomNode *fb = graph_get_node(g, out_id);
    TEST_ASSERT_NOT_NULL(fb);
    TEST_ASSERT_EQ(fb->type, GEOM_FUNCTION_BLOCK);

    engine_destroy(engine);
    printf("  test_pack_function_api: PASSED\n");
}

/* ============== 测试：重写-求解工作流 ============== */

static void test_rewrite_solve_api(void) {
    /* NULL → 负值 */
    TEST_ASSERT(engine_rewrite_and_solve(NULL, 10, 10) < 0, "NULL engine 负值");

    /* 空引擎空图：无规则 → 重写跳过，返回 >= 0 步数（不崩溃） */
    lvEngine *engine = engine_create();
    TEST_ASSERT_NOT_NULL(engine);
    int steps = engine_rewrite_and_solve(engine, 10, 10);
    TEST_ASSERT(steps >= 0, "空图重写-求解返回非负步数");

    engine_destroy(engine);
    printf("  test_rewrite_solve_api: PASSED\n");
}

/* ============== 测试入口 ============== */

TEST_MAIN_BEGIN("Lv-00 Engine Ext Test Suite")
    printf("=== Lv-00 Engine Ext Test Suite (batch C-㊷) ===\n\n");
    lv_init();

    TEST_MAIN_RUN(test_rewrite_rule_api);
    TEST_MAIN_RUN(test_rewrite_step_limit_api);
    TEST_MAIN_RUN(test_frozen_point_api);
    TEST_MAIN_RUN(test_circuit_trip_api);
    TEST_MAIN_RUN(test_stream_event_api);
    TEST_MAIN_RUN(test_load_module_axiom_api);
    TEST_MAIN_RUN(test_engine_unify_api);
    TEST_MAIN_RUN(test_pack_function_api);
    TEST_MAIN_RUN(test_rewrite_solve_api);

    lv_cleanup();
TEST_MAIN_END()
